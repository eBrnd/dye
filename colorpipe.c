#include <unistd.h>
#include <string.h>

char color_normal[] = "\033[0m";
char color_green[] = "\033[32m";

int main(int argc, char** argv) {
  char color_string[sizeof(color_green) + 1 + sizeof(color_normal)];
  size_t output_pos = sizeof(color_green);
  strcpy(color_string, color_green);
  strcpy(color_string + output_pos + 1, color_normal);

  char c;
  ssize_t n;
  while (0 < (n = read(0, &c, 1))) {
    color_string[output_pos] = c;
    write(1, color_string, sizeof(color_string));
  }
}
