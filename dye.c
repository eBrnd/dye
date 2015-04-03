#include <ctype.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void print_usage(bool moreinformation) {
  printf("Usage: dye [OPTION]... COMMAND\n");
  if (moreinformation)
    printf("Try 'dye -h' for more inforamtion\n");
}

void print_help() {
  print_usage(false);
  printf("Colorize output of a command's stdout and stderr.\n"
         "Example: dye make\n\n"
         "Options:\n"
         "  -o <color>    Color for stdout (default: none)\n"
         "  -e <color>    Color for stderr (default: red)\n"
         "  -h            Print this help.\n\n"
         "  Valid colors are: \033[31mred, \033[32mgreen, \033[33mbrown, \033[34mblue, "
         "\033[35mmagenta, \033[36mcyan, \033[37mwhite\033[39m\n");
}

int choose_color(char* colorname, char** colorcode) {
  if (!strcmp(colorname, "red"))     { *colorcode = "\033[31m"; return 0; }
  if (!strcmp(colorname, "green"))   { *colorcode = "\033[32m"; return 0; }
  if (!strcmp(colorname, "brown"))   { *colorcode = "\033[33m"; return 0; }
  if (!strcmp(colorname, "blue"))    { *colorcode = "\033[34m"; return 0; }
  if (!strcmp(colorname, "magenta")) { *colorcode = "\033[35m"; return 0; }
  if (!strcmp(colorname, "cyan"))    { *colorcode = "\033[36m"; return 0; }
  if (!strcmp(colorname, "white"))   { *colorcode = "\033[37m"; return 0; }

  return -1;
}

int main(int argc, char** argv) {
  if (argc < 2)
    return -1;

  char* out_color = "\033[39m";
  char* err_color = "\033[31m";
  const size_t colorcode_len = 5;

  char c;
  while ((c = getopt(argc, argv, "o:e:h")) != -1) {
    switch (c) {
    case 'o':
      if (choose_color(optarg, &out_color)) {
        fprintf(stderr, "Invalid color for stdout.\n");
        print_usage(true);
        return -1;
      }
      break;
    case 'e':
      if (choose_color(optarg, &err_color)) {
        fprintf(stderr, "Invalid color for stderr.\n");
        print_usage(true);
        return -1;
      }
      break;
    case 'h':
      print_help();
      return 0;
      break;
    case '?':
      print_usage(true);
      return -1;
      break;
    }
  }

  int outpipe_fds[2];
  int errpipe_fds[2];
  if (pipe(outpipe_fds) || pipe(errpipe_fds))
    return -1;

  char** exec_args = calloc(sizeof(argv[0]), argc - optind + 1); // One extra for null termination.
  if (!exec_args)
    return -1;
  for (int i = optind; i < argc; i++)
    exec_args[i-optind] = argv[i];

  int pid = fork();
  if (pid == -1) {
    free(exec_args);
    return -1;
  }

  if (pid == 0) { // Child.
    dup2(outpipe_fds[1], STDOUT_FILENO); // Move writing end of pipes to stdout and stderr.
    dup2(errpipe_fds[1], STDERR_FILENO);

    int res = execvp(exec_args[0], exec_args);
    free(exec_args);

    return -1; // Note: we reach this point only if exec fails (better print an error message).
  }

  // Parent.

  // Close writing end of pipes so they're only open in the child now.
  close(outpipe_fds[1]);
  close(errpipe_fds[1]);
  free(exec_args);

  struct pollfd pollfds[2];
  pollfds[0].fd = outpipe_fds[0];
  pollfds[1].fd = errpipe_fds[0];
  pollfds[0].events = pollfds[1].events = POLLIN | POLLHUP;

  // Line buffer.
  const size_t bufsiz = 80;
  char buffer[bufsiz];
  size_t nbyte;

  bool running = true;
  while (running) {
    if (poll(pollfds, 2, -1)) {
      if (pollfds[0].revents & POLLIN) {
        while (0 < (nbyte = read(outpipe_fds[0], buffer, bufsiz))) {
          write(STDOUT_FILENO, out_color, colorcode_len);
          write(STDOUT_FILENO, buffer, nbyte);
        }
      }

      if (pollfds[1].revents & POLLIN) {
        while (0 < (nbyte = read(errpipe_fds[0], buffer, bufsiz))) {
          write(STDOUT_FILENO, err_color, colorcode_len);
          write(STDOUT_FILENO, buffer, nbyte);
        }
      }

      if (pollfds[0].revents & POLLHUP || pollfds[1].revents & POLLHUP)
        running = false;
    }
  }

  int status;
  if (0 > wait(&status))
    return -1;

  write(STDOUT_FILENO, "\033[39m", colorcode_len); // Reset color before exiting.
  return WEXITSTATUS(status);
}
