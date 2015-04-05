#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
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

ssize_t do_write(int fd, const char* buf, size_t len) {
  ssize_t total = 0;

  while (len) {
    ssize_t written = write(fd, buf, len);
    if (written == -1) {
      if (errno == EINTR)
        continue;

      perror("write");
      return -1;
    }

    buf += written;
    len -= written;
    total += written;
  }

  return total;
}

bool dye_pipe(int in_fd, const char* color_string) {
  char buffer[128];
  ssize_t nbyte;

  for (;;) {
    while ((nbyte = read(in_fd, buffer, sizeof(buffer))) == -1 && errno == EINTR);

    if (nbyte <= 0) // Less than 0: Error; Equals 0: Pipe's empty.
      break;

    if (do_write(STDOUT_FILENO, color_string, strlen(color_string)) < 0)
      return false;
    if (do_write(STDOUT_FILENO, buffer, nbyte) < 0)
      return false;
  }

  return true;
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

  int outsock_fds[2];
  int errsock_fds[2];
  if (socketpair(AF_LOCAL, SOCK_SEQPACKET, 0, outsock_fds)
      || socketpair(AF_LOCAL, SOCK_SEQPACKET, 0, errsock_fds)) {
    perror("socketpair");
    return 128;
  }

  if (fcntl(outsock_fds[0], F_SETFD, FD_CLOEXEC) == -1
      || fcntl(outsock_fds[1], F_SETFD, FD_CLOEXEC) == -1
      || fcntl(errsock_fds[0], F_SETFD, FD_CLOEXEC) == -1
      || fcntl(errsock_fds[1], F_SETFD, FD_CLOEXEC) == -1
      || fcntl(outsock_fds[0], F_SETFL, O_NONBLOCK) == -1
      || fcntl(errsock_fds[0], F_SETFL, O_NONBLOCK) == -1) {
    perror("fcntl");
    return 128;
  }

  int sockoptval = 1;
  if (setsockopt(outsock_fds[0], SOL_SOCKET, SO_TIMESTAMP, &sockoptval, sizeof(sockoptval)) < 0
      || setsockopt(errsock_fds[0], SOL_SOCKET, SO_TIMESTAMP, &sockoptval, sizeof(sockoptval)) < 0)
  {
    perror("setsockopt");
    return 128;
  }

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
    // Move writing end of socketpairs to stdout and stderr.
    while (dup2(outsock_fds[1], STDOUT_FILENO) < 0 || dup2(errsock_fds[1], STDERR_FILENO) < 0) {
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

  // Close writing end of socketpairs so they're only open in the child now.
  close(outsock_fds[1]); // (Ignore error code of close here because the only error that can happen
  close(errsock_fds[1]); //  in this case is EINTR, and linux says "don't retry", so we just go on.)

  struct pollfd pollfds[] = { { outsock_fds[0], POLLIN, 0 }, { errsock_fds[0], POLLIN, 0 } };

  for (;;) {
    int pollres = poll(pollfds, 2, -1);

    if (pollres > 0) {
      if (pollfds[0].revents & POLLIN && !dye_pipe(outsock_fds[0], out_color))
        goto error;

      if (pollfds[1].revents & POLLIN && !dye_pipe(errsock_fds[0], err_color))
        goto error;

      if (pollfds[0].revents & POLLHUP && pollfds[1].revents & POLLHUP)
        break;

      continue;
    } else if (pollres < 0) {
      if (errno == EINTR)
        continue;

      perror("poll"); // Fall through to error;
    }

  error:
    kill(pid, SIGTERM);
    break;
  }

  // Reset color before exiting.
  char reset_colorcode[] = "\033[39m";
  do_write(STDOUT_FILENO, reset_colorcode, strlen(reset_colorcode)); // Ignore errors here.

  int status;
  if (wait(&status) < 0) {
    perror("wait");
    return 128;
  }

  return WEXITSTATUS(status);
}
