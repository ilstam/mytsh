#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "parser.tab.h"


#define PROJ_NAME "mytsh"

#define MAX_INPUT 5000
#define MAX_PATH 5000
#define MAX_TOKENS 100

/* used for pipes */
#define READ_END 0
#define WRITE_END 1

#define ATTR_UNUSED __attribute__((unused))


bool empty_line;
command final_cmd;

typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string (char *str);
extern void yy_delete_buffer (YY_BUFFER_STATE buffer);


void exec_cmd(command *cmd);

void builtin_cd(char **tokens, int ntokens);
void builtin_pwd(char **tokens, int ntokens);
void builtin_exit(char **tokens, int ntokens);
void builtin_kill(char **tokens, int ntokens);
void builtin_alias(char **tokens, int ntokens);
void builtin_unalias(char **tokens, int ntokens);


typedef void (*builtin_func)(char **, int);

struct builtin_pair {
    char *name;
    builtin_func fp;
};

struct builtin_pair builtins[] = {
    {"cd", builtin_cd},
    {"pwd", builtin_pwd},
    {"exit", builtin_exit},
    {"kill", builtin_kill},
    {"alias", builtin_alias},
    {"unalias", builtin_unalias},
};


/*
 * Tokenize a c-string, into variable size tokens.
 * Return the number of tokens which s was broken to on success, else 0.
 *
 * @param s - mutable string
 * @param tokens - to be filled with NULL-terminated string tokens
 * @param ntoks - maximum number of desired tokens
 * @param delism - string consisting of the desired delimiter characters
 */
int s_tokenize(char *s, char *tokens[], int ntoks, const char *delims)
{
    if (s  == NULL || tokens == NULL || delims == NULL || !*s || !*delims || ntoks < 1) {
        return 0;
    }

    tokens[0] = strtok(s, delims);
    if (tokens[0] == NULL) {
        return 0;
    }

    int i;
    for (i = 1; i < ntoks && (tokens[i] = strtok(NULL, delims)) != NULL; i++)
        ; /* void */

    return i;
}

void print_prompt(void)
{
    char hostname[MAX_PATH+1] = {'\0'};
    char *username = getlogin();

    gethostname(hostname, MAX_PATH);
    if (!username) {
        username = "unknown";
    }

    printf("[%s@%s]$ ", username, hostname);
}

void builtin_cd(char **tokens, int ntokens)
{
    int r = ntokens == 1 ? chdir(getenv("HOME")) : chdir(tokens[1]);
    if (r < 0) {
        perror("cd");
    }
}

void builtin_pwd(char **tokens ATTR_UNUSED, int ntokens)
{
    if (ntokens > 1) {
        printf("pwd: too many arguments\n");
        return;
    }
    char buf[MAX_PATH];
    getcwd(buf, MAX_PATH);
    puts(buf);
}

void builtin_exit(char **tokens ATTR_UNUSED, int ntokens ATTR_UNUSED)
{
    exit(EXIT_SUCCESS);
}

void builtin_kill(char **tokens, int ntokens)
{
    if (ntokens != 3) {
        printf("kill: wrong number of arguments\n");
        printf("usage: kill <signo> <pid> \n");
        return;
    }

    char *endptr;

    int signo = strtol(tokens[1], &endptr, 10);
    if (*endptr != '\0' || signo <= 0) {
        printf("sincos: signo must be a positive integer\n");
        printf("usage: kill <signo> <pid> \n");
        return;
    }

    int pid = strtol(tokens[2], &endptr, 10);
    if (*endptr != '\0' || pid <= 0) {
        printf("sincos: pid must be a positive integer\n");
        printf("usage: kill <signo> <pid> \n");
        return;
    }

    if (kill(pid, signo) < 0) {
        perror("kill");
    }
}

void builtin_alias(char **tokens ATTR_UNUSED, int ntokens ATTR_UNUSED)
{
    puts("alias built-in not implemented yet");
}

void builtin_unalias(char **tokens ATTR_UNUSED, int ntokens ATTR_UNUSED)
{
    puts("unalias built-in not implemented yet");
}

/*
 * Recursively free all memory allocated for the cmd structure.
 */
void free_cmd(command *cmd)
{
    if (cmd->type == CMD_PIPE) {
        free(cmd->pipe.right);
        free_cmd(cmd->pipe.left);
        free(cmd->pipe.left);
    }
}

void exec_cmd_simple(cmd_simple *cmd)
{
    char *tokens[MAX_TOKENS];
    int ntokens = s_tokenize(cmd->name, tokens, 50, " ");
    tokens[ntokens] = NULL;

    size_t num_builtins = sizeof(builtins) / sizeof(struct builtin_pair);
    for (size_t i = 0; i < num_builtins; i++) {
        if (!strcmp(tokens[0], builtins[i].name)) {
            (*builtins[i].fp)(tokens, ntokens);
            return;
        }
    }

    pid_t pid = fork();

    if (pid < 0) {
        perror(PROJ_NAME);
        return;
    }

    if (pid == 0) {
        /* child process */
        if (!cmd->in_redir[0] == '\0') {
            close(STDIN_FILENO);
            if (open(cmd->in_redir, O_RDONLY) < 0) {
                perror(tokens[0]);
                exit(EXIT_FAILURE);
            }
        }

        if (!cmd->out_redir[0] == '\0') {
            close(STDOUT_FILENO);
            if (open(cmd->out_redir, O_WRONLY | O_CREAT, 0666) < 0) {
                perror(tokens[0]);
                exit(EXIT_FAILURE);
            }
        }

        execvp(tokens[0], tokens);
        if (errno == ENOENT) {
            printf(PROJ_NAME ": command not found: %s\n", tokens[0]);
        } else {
            perror(PROJ_NAME);
        }
        exit(EXIT_FAILURE);
    }

    /* parent process */
    if (!cmd->bg) {
        waitpid(pid, NULL, 0);
    }
}

void exec_cmd_pipe(cmd_pipe *cmd)
{
    int p[2];
    pid_t pid1, pid2;

    if (pipe(p) < 0) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    if ((pid1 = fork()) == 0) {
        dup2(p[WRITE_END], STDOUT_FILENO);
        close(p[READ_END]);
        close(p[WRITE_END]);
        exec_cmd(cmd->left);
        exit(EXIT_SUCCESS);
    }

    if ((pid2 = fork()) == 0) {
        dup2(p[READ_END], STDIN_FILENO);
        close(p[READ_END]);
        close(p[WRITE_END]);
        exec_cmd(cmd->right);
        exit(EXIT_SUCCESS);
    }

    if (pid1 < 0 || pid2 < 0) {
        perror(PROJ_NAME);
        return;
    }

    close(p[READ_END]);
    close(p[WRITE_END]);

    while (wait(NULL) > 0) {
        /* wait for all children */
    }
}

void exec_cmd(command *cmd)
{
    if (cmd->type == CMD_SIMPLE) {
        exec_cmd_simple(&cmd->simple);
    } else if (cmd->type == CMD_PIPE) {
        exec_cmd_pipe(&cmd->pipe);
    }
}

int main(void)
{
    int retval;
    char input[MAX_INPUT+1];
    YY_BUFFER_STATE buffer;

    while (true) {
        print_prompt();
        fgets(input, MAX_INPUT, stdin);
        buffer = yy_scan_string(input);

        if ((retval = yyparse()) || empty_line) {
            continue;
        }

        yy_delete_buffer(buffer);
        exec_cmd(&final_cmd);
        free_cmd(&final_cmd);
    }

    return EXIT_SUCCESS;
}
