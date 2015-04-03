#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>

int main(int argc, char** argv) {
  int outpipe_fds[2];
  int errpipe_fds[2];
  if (pipe(outpipe_fds) || pipe(errpipe_fds))
    return -1;

  int pid = fork();

  if (pid == -1)
    return -1;

  if (pid == 0) // child
  {
    dup2(outpipe_fds[1], 1); // Move writing end of pipes to stdout and stderr.
    dup2(errpipe_fds[1], 2);
    return execl(argv[1], "");
  }

  // main thread
  close(outpipe_fds[1]); // Close writing end of pipes so they're only open in the child now.
  close(errpipe_fds[1]);

  char c;
  while (0 < read(errpipe_fds[0], &c, 1)) {
    printf(">%c<", c);
  }

  int status;
  wait(&status);

  return WEXITSTATUS(status);
}
