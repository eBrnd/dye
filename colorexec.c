#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char** argv) {
  int pid = fork();

  if (pid == -1)
    return -1;

  if (pid == 0) // child
    return execl(argv[1], "");

  // main thread
  int status;
  waitpid(pid, &status, 0);

  return WEXITSTATUS(status);
}
