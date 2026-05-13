#define _POSIX_C_SOURCE 200809L

#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#define SHELL_MAX_INPUT 1024
#define MAX_ARGS 128
#define COPY_BUFFER_SIZE 4096

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static void print_banner(void);
static void print_help_screen(void);
static int execute_external(int argc, char *argv[], int run_in_background);
static int execute_command(int argc, char *argv[], int run_in_background);

typedef struct ParsedCommand {
    char *args[MAX_ARGS];
    int argc;
    char *in_file;
    char *out_file;
    int append_out;
} ParsedCommand;

static int is_operator_token(const char *tok) {
    return tok != NULL &&
           (strcmp(tok, "|") == 0 || strcmp(tok, "<") == 0 || strcmp(tok, ">") == 0 || strcmp(tok, ">>") == 0 || strcmp(tok, "&") == 0);
}

static int contains_pipe_or_redirection(int argc, char *argv[]) {
    int i;

    for (i = 0; i < argc; ++i) {
        if (strcmp(argv[i], "|") == 0 || strcmp(argv[i], "<") == 0 || strcmp(argv[i], ">") == 0 || strcmp(argv[i], ">>") == 0) {
            return 1;
        }
    }

    return 0;
}

static int parse_external_command(int argc, char *argv[], ParsedCommand *commands, int *command_count) {
    int i;
    int cmd_idx = 0;

    if (argc <= 0) {
        fprintf(stderr, "syntax error: empty command\n");
        return -1;
    }

    commands[0].argc = 0;
    commands[0].in_file = NULL;
    commands[0].out_file = NULL;
    commands[0].append_out = 0;

    for (i = 0; i < argc; ++i) {
        char *tok = argv[i];

        if (strcmp(tok, "|") == 0) {
            if (commands[cmd_idx].argc == 0) {
                fprintf(stderr, "syntax error near unexpected token `|'\n");
                return -1;
            }

            commands[cmd_idx].args[commands[cmd_idx].argc] = NULL;
            ++cmd_idx;

            if (cmd_idx >= MAX_ARGS - 1) {
                fprintf(stderr, "too many piped commands\n");
                return -1;
            }

            commands[cmd_idx].argc = 0;
            commands[cmd_idx].in_file = NULL;
            commands[cmd_idx].out_file = NULL;
            commands[cmd_idx].append_out = 0;
            continue;
        }

        if (strcmp(tok, "<") == 0 || strcmp(tok, ">") == 0 || strcmp(tok, ">>") == 0) {
            int is_output = (strcmp(tok, "<") != 0);

            if (i + 1 >= argc || is_operator_token(argv[i + 1])) {
                fprintf(stderr, "syntax error near unexpected token `%s'\n", tok);
                return -1;
            }

            if (is_output) {
                commands[cmd_idx].out_file = argv[++i];
                commands[cmd_idx].append_out = (strcmp(tok, ">>") == 0);
            } else {
                commands[cmd_idx].in_file = argv[++i];
            }
            continue;
        }

        if (strcmp(tok, "&") == 0) {
            fprintf(stderr, "syntax error near unexpected token `&'\n");
            return -1;
        }

        if (commands[cmd_idx].argc >= MAX_ARGS - 1) {
            fprintf(stderr, "too many arguments\n");
            return -1;
        }
        commands[cmd_idx].args[commands[cmd_idx].argc++] = tok;
    }

    if (commands[cmd_idx].argc == 0) {
        fprintf(stderr, "syntax error: command expected after pipe\n");
        return -1;
    }

    commands[cmd_idx].args[commands[cmd_idx].argc] = NULL;
    *command_count = cmd_idx + 1;
    return 0;
}

static int apply_redirections(const ParsedCommand *cmd) {
    if (cmd->in_file != NULL) {
        int fd_in = open(cmd->in_file, O_RDONLY);

        if (fd_in < 0) {
            perror("input redirection");
            return -1;
        }
        if (dup2(fd_in, STDIN_FILENO) < 0) {
            perror("dup2");
            close(fd_in);
            return -1;
        }
        close(fd_in);
    }

    if (cmd->out_file != NULL) {
        int flags = O_CREAT | O_WRONLY | (cmd->append_out ? O_APPEND : O_TRUNC);
        int fd_out = open(cmd->out_file, flags, 0644);

        if (fd_out < 0) {
            perror("output redirection");
            return -1;
        }
        if (dup2(fd_out, STDOUT_FILENO) < 0) {
            perror("dup2");
            close(fd_out);
            return -1;
        }
        close(fd_out);
    }

    return 0;
}

static char *trim_whitespace(char *s) {
    char *end;

    if (s == NULL) {
        return NULL;
    }

    while (*s != '\0' && isspace((unsigned char)*s)) {
        ++s;
    }

    if (*s == '\0') {
        return s;
    }

    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        --end;
    }

    return s;
}

static void trim_newline(char *s) {
    size_t n;

    if (s == NULL) {
        return;
    }

    n = strlen(s);
    if (n > 0 && s[n - 1] == '\n') {
        s[n - 1] = '\0';
    }
}

static int parse_input(char *line, char *argv[]) {
    int argc = 0;
    char *token;

    token = strtok(line, " \t");
    while (token != NULL && argc < MAX_ARGS - 1) {
        argv[argc++] = token;
        token = strtok(NULL, " \t");
    }

    argv[argc] = NULL;
    return argc;
}

static int cmd_pwd(void) {
    char cwd[PATH_MAX];

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("pwd");
        return -1;
    }

    printf("%s\n", cwd);
    return 0;
}

static void format_mode(mode_t mode, char perms[11]) {
    perms[0] = S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' : '-';
    perms[1] = (mode & S_IRUSR) ? 'r' : '-';
    perms[2] = (mode & S_IWUSR) ? 'w' : '-';
    perms[3] = (mode & S_IXUSR) ? 'x' : '-';
    perms[4] = (mode & S_IRGRP) ? 'r' : '-';
    perms[5] = (mode & S_IWGRP) ? 'w' : '-';
    perms[6] = (mode & S_IXGRP) ? 'x' : '-';
    perms[7] = (mode & S_IROTH) ? 'r' : '-';
    perms[8] = (mode & S_IWOTH) ? 'w' : '-';
    perms[9] = (mode & S_IXOTH) ? 'x' : '-';
    perms[10] = '\0';
}

static void print_long_entry(const char *dir_path, const char *name) {
    char full_path[PATH_MAX];
    char perms[11];
    char time_buf[32];
    struct stat st;
    struct passwd *pw;
    struct group *gr;
    struct tm *tm_info;

    if (strcmp(dir_path, "/") == 0) {
        (void)snprintf(full_path, sizeof(full_path), "/%s", name);
    } else {
        (void)snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, name);
    }

    if (lstat(full_path, &st) != 0) {
        perror("ls");
        return;
    }

    format_mode(st.st_mode, perms);
    pw = getpwuid(st.st_uid);
    gr = getgrgid(st.st_gid);
    tm_info = localtime(&st.st_mtime);

    if (tm_info != NULL) {
        (void)strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", tm_info);
    } else {
        (void)snprintf(time_buf, sizeof(time_buf), "--- -- --:--");
    }

    printf("%s %2lu %-8s %-8s %8lld %s %s\n",
           perms,
           (unsigned long)st.st_nlink,
           (pw != NULL) ? pw->pw_name : "unknown",
           (gr != NULL) ? gr->gr_name : "unknown",
           (long long)st.st_size,
           time_buf,
           name);
}

static int cmd_ls(int argc, char *argv[]) {
    const char *path = ".";
    int long_format = 0;
    int i;
    struct dirent *entry;

    for (i = 1; i < argc; ++i) {
        if (argv[i][0] == '-' && argv[i][1] != '\0') {
            int j;

            for (j = 1; argv[i][j] != '\0'; ++j) {
                if (argv[i][j] == 'l') {
                    long_format = 1;
                } else {
                    fprintf(stderr, "ls: unsupported option -- %c\n", argv[i][j]);
                    return -1;
                }
            }
            continue;
        }

        if (argv[i][0] == '-') {
            continue;
        }
        path = argv[i];
        break;
    }

    DIR *dir = opendir(path);

    if (dir == NULL) {
        perror("ls");
        return -1;
    }

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        if (long_format) {
            print_long_entry(path, entry->d_name);
        } else {
            printf("%s\n", entry->d_name);
        }
    }

    closedir(dir);
    return 0;
}

static int cmd_cd(int argc, char *argv[]) {
    const char *target;

    if (argc < 2) {
        target = getenv("HOME");
        if (target == NULL) {
            fprintf(stderr, "cd: HOME is not set\n");
            return -1;
        }
    } else {
        target = argv[1];
    }

    if (chdir(target) != 0) {
        perror("cd");
        return -1;
    }

    return 0;
}

static int cmd_mkdir(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "mkdir: missing operand\n");
        return -1;
    }

    if (mkdir(argv[1], 0755) != 0) {
        perror("mkdir");
        return -1;
    }

    return 0;
}

static int cmd_touch(int argc, char *argv[]) {
    int fd;

    if (argc < 2) {
        fprintf(stderr, "touch: missing file operand\n");
        return -1;
    }

    fd = open(argv[1], O_CREAT | O_WRONLY, 0644);
    if (fd < 0) {
        perror("touch");
        return -1;
    }

    close(fd);
    return 0;
}

static int cmd_rm(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "rm: missing file operand\n");
        return -1;
    }

    if (unlink(argv[1]) != 0) {
        perror("rm");
        return -1;
    }

    return 0;
}

static int copy_file(const char *src, const char *dst) {
    char buffer[COPY_BUFFER_SIZE];
    ssize_t bytes_read;
    int in_fd;
    int out_fd;

    in_fd = open(src, O_RDONLY);
    if (in_fd < 0) {
        perror("cp");
        return -1;
    }

    out_fd = open(dst, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (out_fd < 0) {
        perror("cp");
        close(in_fd);
        return -1;
    }

    while ((bytes_read = read(in_fd, buffer, sizeof(buffer))) > 0) {
        ssize_t total_written = 0;

        while (total_written < bytes_read) {
            ssize_t bytes_written = write(out_fd, buffer + total_written, bytes_read - total_written);
            if (bytes_written < 0) {
                perror("cp");
                close(in_fd);
                close(out_fd);
                return -1;
            }
            total_written += bytes_written;
        }
    }

    if (bytes_read < 0) {
        perror("cp");
        close(in_fd);
        close(out_fd);
        return -1;
    }

    close(in_fd);
    close(out_fd);
    return 0;
}

static int cmd_cp(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "cp: missing file operand\n");
        return -1;
    }

    return copy_file(argv[1], argv[2]);
}

static int cmd_mv(int argc, char *argv[]) {
    struct stat st;
    const char *src = argv[1];
    const char *dst = argv[2];
    const char *base_name;
    char resolved_dst[PATH_MAX];

    if (argc < 3) {
        fprintf(stderr, "mv: missing file operand\n");
        return -1;
    }

    if (stat(dst, &st) == 0 && S_ISDIR(st.st_mode)) {
        const char *last_slash = strrchr(src, '/');
        base_name = (last_slash == NULL) ? src : last_slash + 1;

        if (snprintf(resolved_dst, sizeof(resolved_dst), "%s/%s", dst, base_name) >= (int)sizeof(resolved_dst)) {
            fprintf(stderr, "mv: destination path too long\n");
            return -1;
        }
        dst = resolved_dst;
    }

    if (rename(src, dst) == 0) {
        return 0;
    }

    if (errno == EXDEV) {
        if (copy_file(src, dst) == 0 && unlink(src) == 0) {
            return 0;
        }
    }

    perror("mv");
    return -1;
}

static int cmd_cat(int argc, char *argv[]) {
    int i;

    if (argc < 2) {
        fprintf(stderr, "cat: missing file operand\n");
        return -1;
    }

    for (i = 1; i < argc; ++i) {
        char buffer[COPY_BUFFER_SIZE];
        ssize_t bytes_read;
        int fd = open(argv[i], O_RDONLY);

        if (fd < 0) {
            perror("cat");
            return -1;
        }

        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
            ssize_t total_written = 0;

            while (total_written < bytes_read) {
                ssize_t bytes_written = write(STDOUT_FILENO, buffer + total_written, bytes_read - total_written);
                if (bytes_written < 0) {
                    perror("cat");
                    close(fd);
                    return -1;
                }
                total_written += bytes_written;
            }
        }

        if (bytes_read < 0) {
            perror("cat");
            close(fd);
            return -1;
        }

        close(fd);
    }

    return 0;
}

static int cmd_echo(int argc, char *argv[]) {
    int i;

    for (i = 1; i < argc; ++i) {
        printf("%s", argv[i]);
        if (i != argc - 1) {
            printf(" ");
        }
    }
    printf("\n");
    return 0;
}

static void print_banner(void) {
    const char *user = getenv("USER");
    char host[256];
    const char *host_name = NULL;

    if (gethostname(host, sizeof(host)) == 0) {
        host_name = host;
    }

    if (user == NULL || user[0] == '\0') {
        user = "user";
    }
    if (host_name == NULL || host_name[0] == '\0') {
        host_name = "unknown-host";
    }

    printf("\n");
    printf("    ╭────────────────────────────────────────────────────────────────╮\n");
    printf("      Welcome to My Command Shell - Lab Assignment 1\n");
    printf("    ├────────────────────────────────────────────────────────────────┤\n");
    printf("      ▸  %s@%s: interactive shell ready\n", user, host_name);
    printf("      ▸  Type help to show all available commands\n");
    printf("      ▸  Type exit to quit the shell\n");
    printf("    ╰────────────────────────────────────────────────────────────────╯\n");
    printf("\n");
}

static void print_help_screen(void) {
    printf("\n");
    printf("    ╭────────────────────────────────────────────────────────────────╮\n");
    printf("      📚 SHELL COMMAND REFERENCE 📚\n");
    printf("    ├────────────────────────────────────────────────────────────────┤\n");
    printf("\n");
    printf("    🔷 SHELL CONTROL COMMANDS:\n");
    printf("    ├─  help   →  Display this help message\n");
    printf("    └─  exit   →  Exit the shell\n");
    printf("\n");
    printf("    🔷 DIRECTORY & FILE NAVIGATION:\n");
    printf("    ├─  pwd    →  Print current working directory\n");
    printf("    ├─  ls     →  List directory contents\n");
    printf("    └─  cd     →  Change directory\n");
    printf("\n");
    printf("    🔷 FILE & DIRECTORY CREATION:\n");
    printf("    ├─  mkdir  →  Create directory\n");
    printf("    └─  touch  →  Create empty file\n");
    printf("\n");
    printf("    🔷 FILE MANAGEMENT:\n");
    printf("    ├─  rm     →  Remove file or empty directory\n");
    printf("    ├─  cp     →  Copy file\n");
    printf("    └─  mv     →  Move/Rename file\n");
    printf("\n");
    printf("    🔷 FILE VIEWING & OUTPUT:\n");
    printf("    ├─  cat    →  Display file contents\n");
    printf("    └─  echo   →  Print text to terminal\n");
    printf("\n");
    printf("    🔷 EXTERNAL PROGRAMS:\n");
    printf("    └─  Any unlisted command →  Executed via fork() + execvp()\n");
    printf("\n");
    printf("    🔷 BONUS FEATURES:\n");
    printf("    ├─  |       →  Pipe output of one command to another\n");
    printf("    ├─  > / >> →  Redirect/append stdout to a file\n");
    printf("    ├─  <       →  Redirect stdin from a file\n");
    printf("    └─  &       →  Run command in background\n");
    printf("\n");
    printf("    ╰────────────────────────────────────────────────────────────────╯\n");
    printf("\n");
}

static int execute_external(int argc, char *argv[], int run_in_background) {
    ParsedCommand commands[MAX_ARGS];
    int command_count;
    int i;
    int status;
    pid_t pids[MAX_ARGS];

    if (parse_external_command(argc, argv, commands, &command_count) != 0) {
        return -1;
    }

    if (command_count == 1) {
        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            return -1;
        }

        if (pid == 0) {
            if (apply_redirections(&commands[0]) != 0) {
                _exit(EXIT_FAILURE);
            }

            execvp(commands[0].args[0], commands[0].args);

            if (errno == ENOENT) {
                fprintf(stderr, "%s: command not found\n", commands[0].args[0]);
            } else {
                perror("exec");
            }
            _exit(EXIT_FAILURE);
        }

        if (run_in_background) {
            printf("[background pid %ld]\n", (long)pid);
            return 0;
        }

        if (waitpid(pid, &status, 0) < 0) {
            perror("waitpid");
            return -1;
        }

        return 0;
    }

    {
        int pipes[MAX_ARGS][2];

        if (run_in_background) {
            fprintf(stderr, "background execution for pipelines is not supported\n");
            return -1;
        }

        for (i = 0; i < command_count - 1; ++i) {
            if (pipe(pipes[i]) < 0) {
                perror("pipe");
                return -1;
            }
        }

        for (i = 0; i < command_count; ++i) {
            pid_t pid = fork();
            int j;

            if (pid < 0) {
                perror("fork");
                return -1;
            }

            if (pid == 0) {
                if (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) < 0) {
                    perror("dup2");
                    _exit(EXIT_FAILURE);
                }
                if (i < command_count - 1 && dup2(pipes[i][1], STDOUT_FILENO) < 0) {
                    perror("dup2");
                    _exit(EXIT_FAILURE);
                }

                for (j = 0; j < command_count - 1; ++j) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }

                if (apply_redirections(&commands[i]) != 0) {
                    _exit(EXIT_FAILURE);
                }

                execvp(commands[i].args[0], commands[i].args);

                if (errno == ENOENT) {
                    fprintf(stderr, "%s: command not found\n", commands[i].args[0]);
                } else {
                    perror("exec");
                }
                _exit(EXIT_FAILURE);
            }

            pids[i] = pid;
        }

        for (i = 0; i < command_count - 1; ++i) {
            close(pipes[i][0]);
            close(pipes[i][1]);
        }
    }

    for (i = 0; i < command_count; ++i) {
        if (waitpid(pids[i], &status, 0) < 0) {
            perror("waitpid");
            return -1;
        }
    }

    return 0;
}

static int execute_command(int argc, char *argv[], int run_in_background) {
    int has_meta_operators;

    if (argc == 0) {
        return 0;
    }

    has_meta_operators = contains_pipe_or_redirection(argc, argv);

    if (strcmp(argv[0], "|") == 0 || strcmp(argv[0], "<") == 0 || strcmp(argv[0], ">") == 0 || strcmp(argv[0], ">>") == 0) {
        fprintf(stderr, "syntax error near unexpected token `%s'\n", argv[0]);
        return -1;
    }

    if (strcmp(argv[0], "help") == 0) {
        if (has_meta_operators || run_in_background) {
            fprintf(stderr, "help: redirection, pipes, and background are not supported for built-in commands\n");
            return -1;
        }
        print_help_screen();
        return 0;
    }

    if (strcmp(argv[0], "cd") == 0) {
        if (has_meta_operators || run_in_background) {
            fprintf(stderr, "cd: redirection, pipes, and background are not supported for built-in commands\n");
            return -1;
        }
        return cmd_cd(argc, argv);
    }

    if (strcmp(argv[0], "exit") == 0) {
        if (has_meta_operators || run_in_background) {
            fprintf(stderr, "exit: redirection, pipes, and background are not supported for built-in commands\n");
            return -1;
        }
        return 1;
    }

    if (has_meta_operators || run_in_background) {
        return execute_external(argc, argv, run_in_background);
    }

    if (strcmp(argv[0], "pwd") == 0) {
        return cmd_pwd();
    }
    if (strcmp(argv[0], "ls") == 0) {
        return cmd_ls(argc, argv);
    }
    if (strcmp(argv[0], "mkdir") == 0) {
        return cmd_mkdir(argc, argv);
    }
    if (strcmp(argv[0], "touch") == 0) {
        return cmd_touch(argc, argv);
    }
    if (strcmp(argv[0], "rm") == 0) {
        return cmd_rm(argc, argv);
    }
    if (strcmp(argv[0], "cp") == 0) {
        return cmd_cp(argc, argv);
    }
    if (strcmp(argv[0], "mv") == 0) {
        return cmd_mv(argc, argv);
    }
    if (strcmp(argv[0], "cat") == 0) {
        return cmd_cat(argc, argv);
    }
    if (strcmp(argv[0], "echo") == 0) {
        return cmd_echo(argc, argv);
    }

    // Fallback path: run non-required commands using fork/exec.
    return execute_external(argc, argv, run_in_background);
}

int main(void) {
    char input[SHELL_MAX_INPUT];

    print_banner();

    while (1) {
        char *segment;
        char *saveptr = NULL;
        int should_exit = 0;

        printf("mysh> ");
        fflush(stdout);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("\n");
            break;
        }

        trim_newline(input);

        segment = strtok_r(input, ";", &saveptr);
        while (segment != NULL) {
            char *argv[MAX_ARGS];
            int argc;
            int result;
            int run_in_background = 0;
            char *command = trim_whitespace(segment);

            if (command[0] != '\0') {
                argc = parse_input(command, argv);

                if (argc > 0 && strcmp(argv[argc - 1], "&") == 0) {
                    run_in_background = 1;
                    argv[argc - 1] = NULL;
                    --argc;
                }

                if (argc > 0) {
                    result = execute_command(argc, argv, run_in_background);
                    if (result == 1 && strcmp(argv[0], "exit") == 0) {
                        should_exit = 1;
                        break;
                    }
                } else if (run_in_background) {
                    fprintf(stderr, "syntax error near unexpected token `&'\n");
                }
            }

            segment = strtok_r(NULL, ";", &saveptr);
        }

        if (should_exit) {
            break;
        }
    }

    return 0;
}