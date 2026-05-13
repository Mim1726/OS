# Lab Assignment 1 - Custom Shell (Roll 17)

This folder contains a small C-based shell implementation for Lab Assignment 1.

## What is here
- `shell.c` - the shell implementation
- `commands.c` - helper command logic used by the assignment version in this folder
- `shellTalk.c` - extra presentation/output helpers
- `run.sh` / `run_shell.sh` - build-and-run scripts
- `README.md` - project overview
- `COMMANDS.md` - quick command reference

## Overview
The shell supports a command loop, simple argument parsing, built-in commands, and basic file and directory operations without using `system()`.

## Supported commands
- `pwd` - print the current working directory
- `ls` - list directory contents
- `cd` - change directory
- `mkdir` - create a directory
- `touch` - create an empty file
- `rm` - remove a file
- `cp` - copy a file
- `mv` - move or rename a file
- `cat` - display file contents
- `echo` - print text
- `exit` - quit the shell

## Behavior notes
- Input is split on spaces and tabs.
- Multiple commands can be executed on one line using `;`.
- Unknown commands are executed with `fork()` and `execvp()`.
- The shell supports external-command pipes (`|`) and I/O redirection (`<`, `>`, `>>`).
- The shell supports background execution for external commands using trailing `&`.
- Operators should be space-separated (example: `echo hi > out.txt`, `echo one | wc -w`).
- `cd`, `help`, and `exit` are built-ins and do not support pipes/redirection/background.
- Basic error handling is included for missing operands and file errors.

## Build and run
From this folder:

```sh
chmod +x run_shell.sh
./run_shell.sh
```

Or compile manually:

```sh
gcc -Wall -Wextra -Werror -std=c11 shell.c -o shell
./shell
```

## Quick examples
```sh
pwd
ls
mkdir testdir
cd testdir
touch file1.txt
cp file1.txt file2.txt
mv file2.txt moved.txt
cat file1.txt
echo Hello_From_SH_01
rm moved.txt
pwd; ls
echo hi > out.txt
cat < out.txt
echo one | wc -w
/bin/sleep 1 &
exit
```

## Final Thought

This shell is not just a program. It is a small model of how operating systems and command interpreters work.

You are learning how a text command becomes an action, how the shell stays in control, and how the OS provides the low-level tools that make everything happen.

