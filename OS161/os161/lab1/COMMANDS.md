# Command Reference

This file is a quick reference for the Lab 1 shell.

## Built-in commands
- `pwd` - print the current working directory
- `cd <dir>` - change directory
- `echo <text>` - print text to the terminal
- `exit` - close the shell

## File and directory commands
- `ls [dir]` - list files in the current directory or in `dir`
- `mkdir <name>` - create a directory
- `touch <file>` - create an empty file
- `rm <file>` - remove a file
- `cp <source> <dest>` - copy a file
- `mv <source> <dest>` - move or rename a file
- `cat <file...>` - display file contents

## Execution behavior
- Commands are parsed using spaces and tabs only.
- Multiple commands can be run on one line using `;`.
- External commands support pipes with `|` and redirection with `<`, `>`, and `>>`.
- Background execution is supported for external commands using trailing `&`.
- Quoted strings are not supported.
- Operators should be space-separated.
- `cd`, `help`, and `exit` are built-ins and do not support pipes/redirection/background.
- Commands that are not built in are passed to the system through `fork()` and `execvp()`.

## Example session
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