# dye
Colorize output of a program's stdout and stderr.

## Problem:
You have a program that generates a lot of output, mostly on stdout, but also some on stderr. It's
hard to find the error messages in the wall of text.

## Solution:
Run the program through dye. By default it prints the output from stderr in read and stdout in the
normal foreground color.

For more info just run `dye -h`.

## Known issues:
For now, if the program you're running needs any options that start with a `-`, dye will also try
to parse them, eating your program's option. You can stop it from doing that by adding a `--` before
the command to run.

Example: `dye -o green -- ls -l`
