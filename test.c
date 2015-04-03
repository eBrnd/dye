#include <stdio.h>

int main(int argc, char** argv) {
  fputs("I'm stdout\n", stdout);
  fputs("I'm stderr\n", stderr);

  return 0;
}

