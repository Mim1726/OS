#!/usr/bin/env sh
set -eu

# Compile the custom shell for Assignment SH_01.
gcc -Wall -Wextra -Werror -std=c11 shell.c -o shell

# Run the shell.
./shell
