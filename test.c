#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
  fputs("I'm stdout\nYeah, stdout.\n", stdout);
  fputs("I'm stderr\nYeah, strerr.\n", stderr);

  sleep(3);

  return 32;
}

