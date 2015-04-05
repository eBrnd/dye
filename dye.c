#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void print_usage(const char* name, bool moreinformation) {
  printf("Usage: %s [OPTION]... COMMAND\n", name);
  if (moreinformation)
    printf("Try '%s -h' for more inforamtion\n", name);
}

void print_help(const char* name) {
  print_usage(name, false);
  printf("Colorize output of a command's stdout and stderr.\n"
         "Example: %s make\n\n"
         "Options:\n"
         "  -o <color>    Color for stdout (default: none)\n"
         "  -e <color>    Color for stderr (default: red)\n"
         "  -h            Print this help.\n\n"
         "  Valid colors are: \033[31mred, \033[32mgreen, \033[33mbrown, \033[34mblue, "
         "\033[35mmagenta, \033[36mcyan, \033[37mwhite\033[39m\n",
         name);
}

int choose_color(char* colorname, char const** colorcode) {
  if (!strcmp(colorname, "red"))     { *colorcode = "\033[31m"; return 0; }
  if (!strcmp(colorname, "green"))   { *colorcode = "\033[32m"; return 0; }
  if (!strcmp(colorname, "brown"))   { *colorcode = "\033[33m"; return 0; }
  if (!strcmp(colorname, "blue"))    { *colorcode = "\033[34m"; return 0; }
  if (!strcmp(colorname, "magenta")) { *colorcode = "\033[35m"; return 0; }
  if (!strcmp(colorname, "cyan"))    { *colorcode = "\033[36m"; return 0; }
  if (!strcmp(colorname, "white"))   { *colorcode = "\033[37m"; return 0; }

  return -1;
}

void dye_pipe(int in_fd, const char* color_string) {
  char buffer[128];
  size_t nbyte;

  for (;;) {
    while ((nbyte = read(in_fd, buffer, sizeof(buffer))) == -1 && errno == EINTR);

    if (nbyte <= 0) // Less than 0: Error; Equals 0: Pipe's empty.
      break;

    write(STDOUT_FILENO, color_string, strlen(color_string));
    write(STDOUT_FILENO, buffer, nbyte);
  }

}

int main(int argc, char** argv) {
  if (argc < 2) {
    print_usage(argv[0], true);
    return 128;
  }

  char const* out_color = "\033[39m";
  char const* err_color = "\033[31m";

  char c;
  while ((c = getopt(argc, argv, "o:e:h")) != -1) {
    switch (c) {
    case 'o':
      if (choose_color(optarg, &out_color)) {
        fprintf(stderr, "Invalid color for stdout.\n");
        print_usage(argv[0], true);
        return 128;
      }
      break;
    case 'e':
      if (choose_color(optarg, &err_color)) {
        fprintf(stderr, "Invalid color for stderr.\n");
        print_usage(argv[0], true);
        return 128;
      }
      break;
    case 'h':
      print_help(argv[0]);
      return 0;
      break;
    case '?':
      print_usage(argv[0], true);
      return 128;
      break;
    }
  }

  int outpipe_fds[2];
  int errpipe_fds[2];
  if (pipe(outpipe_fds) || pipe(errpipe_fds)) {
    perror("pipe");
    return 128;
  }

  fcntl(outpipe_fds[0], F_SETFD, FD_CLOEXEC);
  fcntl(outpipe_fds[1], F_SETFD, FD_CLOEXEC);
  fcntl(errpipe_fds[0], F_SETFD, FD_CLOEXEC);
  fcntl(errpipe_fds[1], F_SETFD, FD_CLOEXEC);

  char** exec_args = calloc(sizeof(argv[0]), argc - optind + 1); // One extra for null termination.
  if (!exec_args) {
    perror("calloc");
    return 128;
  }
  for (int i = optind; i < argc; i++)
    exec_args[i-optind] = argv[i];

  int pid = fork();
  if (pid == -1) {
    perror("fork");
    free(exec_args);
    return 128;
  }

  if (pid == 0) { // Child.
    // Move writing end of pipes to stdout and stderr.
    while (dup2(outpipe_fds[1], STDOUT_FILENO) < 0 || dup2(errpipe_fds[1], STDERR_FILENO) < 0) {
      if (errno != EINTR) {
        free(exec_args);
        perror("dup2");
        return 128;
      }
    }

    execvp(exec_args[0], exec_args);

    // We reach this point only if execvp fails.
    perror("exec");
    free(exec_args);
    return 128;
  }

  // Parent.
  free(exec_args);

  // Close writing end of pipes so they're only open in the child now.
  close(outpipe_fds[1]); // (Ignore error code of close here because the only error that can happen
  close(errpipe_fds[1]); //  in this case is EINTR, and linux says "don't retry", so we just go on.)

  struct pollfd pollfds[] = { { outpipe_fds[0], POLLIN, 0 }, { errpipe_fds[0], POLLIN, 0 } };

  // Line buffer.
  const size_t bufsiz = 80;
  char buffer[bufsiz];
  size_t nbyte;

  for (;;) {
    if (poll(pollfds, 2, -1)) {
      if (pollfds[0].revents & POLLIN)
        dye_pipe(outpipe_fds[0], out_color);

      if (pollfds[1].revents & POLLIN)
        dye_pipe(errpipe_fds[0], err_color);

      if (pollfds[0].revents & POLLHUP && pollfds[1].revents & POLLHUP)
        break;
    }
  }

  char reset_colorcode[] = "\033[39m";
  write(STDOUT_FILENO, reset_colorcode, strlen(reset_colorcode)); // Reset color before exiting.

  int status;
  if (wait(&status) < 0) {
    perror("wait");
    return 128;
  }

  return WEXITSTATUS(status);
}
