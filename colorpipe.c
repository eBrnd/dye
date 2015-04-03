#include <unistd.h>

int main(int argc, char** argv) {
  char flt[] = "> <";
  char c;
  ssize_t n;
  while (0 < (n = read(0, &c, 1))) {
    flt[1] = c;
    write(1, flt, sizeof(flt));
  }
}
