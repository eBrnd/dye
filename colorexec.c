#include <ctype.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

void print_usage() {
  printf("Usage: colorexec [OPTION]... COMMAND\n"
         "Try 'colorexec -h' for more inforamtion\n");
}

void print_help() {
  print_usage();
  printf("Colorize output of a command's stdout and stderr.\n"
         "Example: colorexec make\n\n"
         "Options:\n"
         "  -o <color>    Color for stdout (default: none)\n"
         "  -e <color>    Color for stderr (default: red)\n"
         "  -h            Print this help.");
}

int main(int argc, char** argv) {
  if (argc < 2)
    return -1;

  char c;
  while ((c = getopt(argc, argv, "o:e:h")) != -1) {
    switch (c) {
    case 'o':
      break;
    case 'e':
      break;
    case 'h':
      print_help();
      break;
    case '?':
      print_usage();
      break;
    }
  }

  int outpipe_fds[2];
  int errpipe_fds[2];
  if (pipe(outpipe_fds) || pipe(errpipe_fds))
    return -1;

  char** exec_args = calloc(sizeof(argv[0]), argc); // One extra for null termination.
  if (!exec_args)
    return -1;
  for (int i = 0; i < argc-1; i++)
    exec_args[i] = argv[i+1];

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
        while (0 < (nbyte = read(outpipe_fds[0], buffer, bufsiz)))
          write(STDOUT_FILENO, buffer, nbyte);
      }

      if (pollfds[1].revents & POLLIN) {
        while (0 < (nbyte = read(errpipe_fds[0], buffer, bufsiz)))
        {
          write(STDOUT_FILENO, ">", 1);
          write(STDOUT_FILENO, buffer, nbyte);
          write(STDOUT_FILENO, "<", 1);
        }
      }

      if (pollfds[0].revents & POLLHUP || pollfds[1].revents & POLLHUP)
        running = false;
    }
  }

  int status;
  if (0 > wait(&status))
    return -1;

  return WEXITSTATUS(status);
}
