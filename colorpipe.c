#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

const char default_color_arg[] = "green";

const char* choose_color(const char* arg) {
  if (!strcmp(arg, "red")) return "\033[31m";
  if (!strcmp(arg, "green")) return "\033[32m";
  if (!strcmp(arg, "brown")) return "\033[33m";
  if (!strcmp(arg, "blue")) return "\033[34m";
  if (!strcmp(arg, "magenta")) return "\033[35m";
  if (!strcmp(arg, "cyan")) return "\033[36m";
  if (!strcmp(arg, "white")) return "\033[37m";

  if (!strcmp(arg, "help") || !strcmp(arg, "-h") || !strcmp(arg, "--help")) {
    printf("colorpipe - colorize output\n\n"
           "  Usage: colorpipe <color>\n\n"
           "  Valid colors are: \033[31mred, \033[32mgreen, \033[33mbrown, \033[34mblue, \033[35mmagenta, \033[36mcyan, \033[37mwhite\033[39m\n");
    exit(0);
  }

  fprintf(stderr, "\033[36mcolorpipe \033[31merror: \033[39mInvalid color code.\n");
  exit(22);
}

const char color_normal[] = "\033[39m";
const size_t colstr_len = sizeof(color_normal); // Assumes that all the color strings above are the same size as color_normal.

int main(int argc, char** argv) {
  char color_string[2*colstr_len + 1];
  size_t output_pos = colstr_len;

  if (argc > 1)
    strcpy(color_string, choose_color(argv[1]));
  else
    strcpy(color_string, choose_color(default_color_arg));

  strcpy(color_string + output_pos + 1, color_normal);

  char c;
  ssize_t n;

  while (0 < (n = read(0, &c, 1))) {
    color_string[output_pos] = c;
    write(1, color_string, sizeof(color_string));
  }
}
