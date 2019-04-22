#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "parser.tab.h"


#define PROJ_NAME "mysh"

#define MAX_INPUT 5000
#define MAX_PATH 5000
#define MAX_TOKENS 100


bool empty_line;
command final_cmd;

typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string (char *str);
extern void yy_delete_buffer (YY_BUFFER_STATE buffer);


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

void free_cmd(command *cmd)
{
    if (cmd->type == CMD_SIMPLE) {
    } else {
        free(cmd->pipe.right);
        free_cmd(cmd->pipe.left);
        free(cmd->pipe.left);
    }
}

/*
 * Checks if the command to be executed matches any shell built-in.
 * If so, it executes the built-in and returns true, otherwise returns false.
 */
bool handle_built_in(char **tokens, int ntokens)
{
    if (!strcmp(tokens[0], "cd")) {

        int r = ntokens == 1 ? chdir(getenv("HOME")) : chdir(tokens[1]);
        if (r < 0) {
            perror("cd");
        }

    } else if (!strcmp(tokens[0], "pwd")) {

        if (ntokens > 1) {
            printf("pwd: too many arguments\n");
            return true;
        }
        char buf[MAX_PATH];
        getcwd(buf, MAX_PATH);
        puts(buf);

    } else if (!strcmp(tokens[0], "exit")) {

        exit(EXIT_SUCCESS);

    } else if (!strcmp(tokens[0], "kill")) {
        puts("kill built-in not implemented yet");
    } else if (!strcmp(tokens[0], "alias")) {
        puts("alias built-in not implemented yet");
    } else if (!strcmp(tokens[0], "unalias")) {
        puts("unalias built-in not implemented yet");
    } else {
        return false;
    }

    return true;
}

void exec_cmd(command *cmd)
{
    if (cmd->type == CMD_SIMPLE) {
        char *tokens[MAX_TOKENS];
        int ntokens = s_tokenize(cmd->simple.name, tokens, 50, " ");
        tokens[ntokens] = NULL;

        if (handle_built_in(tokens, ntokens)) {
            return;
        }

        pid_t pid = fork();

        if (pid < 0) {
            /* error forking */
            perror(PROJ_NAME);
        } else if (pid == 0) {
            /* child process */
            execvp(tokens[0], tokens);
            if (errno == ENOENT) {
                printf(PROJ_NAME ": command not found: %s\n", tokens[0]);
            } else {
                perror(PROJ_NAME);
            }
            exit(EXIT_FAILURE);
        } else {
            /* parent process */
            int status;
            do {
                waitpid(pid, &status, WUNTRACED);
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }

        /*printf("name: %s\n", cmd->simple.name);*/
        /*printf("in: %s\n", cmd->simple.in_redir);*/
        /*printf("out: %s\n", cmd->simple.out_redir);*/
        /*printf("bg: %d\n", cmd->simple.bg);*/
        /*puts("");*/
    } else {
        exec_cmd(cmd->pipe.left);
        printf("|\n");
        exec_cmd(cmd->pipe.right);
    }
}

int main(void)
{
    int retval;
    char input[MAX_INPUT+1];
    YY_BUFFER_STATE buffer;

    while (true) {
        printf(PROJ_NAME "$ ");

        fgets(input, MAX_INPUT, stdin);

        buffer = yy_scan_string(input);

        if ((retval = yyparse()) || empty_line) {
            continue;
        }

        yy_delete_buffer(buffer);
        exec_cmd(&final_cmd);
        free_cmd(&final_cmd);
    }

    return 0;
}
