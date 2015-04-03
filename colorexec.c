#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>
#include <stdbool.h>

int main(int argc, char** argv) {
  int outpipe_fds[2];
  int errpipe_fds[2];
  if (pipe(outpipe_fds) || pipe(errpipe_fds))
    return -1;

  int pid = fork();

  if (pid == -1)
    return -1;

  if (pid == 0) // Child.
  {
    dup2(outpipe_fds[1], STDOUT_FILENO); // Move writing end of pipes to stdout and stderr.
    dup2(errpipe_fds[1], STDERR_FILENO);
    return execl(argv[1], "");
  }

  // Main thread.

  // Close writing end of pipes so they're only open in the child now.
  close(outpipe_fds[1]);
  close(errpipe_fds[1]);

  // Set up polling.
  struct pollfd pollfds[2];
  pollfds[0].fd = outpipe_fds[0];
  pollfds[1].fd = errpipe_fds[0];
  pollfds[0].events = pollfds[1].events = POLLIN | POLLHUP;

  // Line buffer.
  const size_t bufsiz = 80;
  char buffer[bufsiz];
  size_t nbyte;

  // Poll loop.
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

  // Wait for child to terminate.
  int status;
  if (0 > wait(&status))
    return -1;

  return WEXITSTATUS(status);
}
