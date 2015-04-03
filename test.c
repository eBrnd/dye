#include <stdio.h>
#include <unistd.h>

int main(int argc, char** argv) {
  fputs("I'm stdout\n", stdout);
  fputs("I'm stderr\n", stderr);

  sleep(3);

  return 32;
}

