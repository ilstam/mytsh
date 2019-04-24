#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>

#include "parser.tab.h"


#define PROJ_NAME "mytsh"

#define MAX_INPUT 5000
#define MAX_PATH 5000
#define MAX_HOSTNAME 30
#define MAX_PROMPT 100
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

void builtin_cd(const char **tokens, int ntokens);
void builtin_pwd(const char **tokens, int ntokens);
void builtin_exit(const char **tokens, int ntokens);
void builtin_kill(const char **tokens, int ntokens);
void builtin_alias(const char **tokens, int ntokens);
void builtin_unalias(const char **tokens, int ntokens);


typedef void (*builtin_func)(const char **, int);

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

typedef struct alias_node {
    char *alias;
    char *real;
    struct alias_node *next;
} alias_node;

alias_node *alias_list;


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
    for (i = 1; i < ntoks && (tokens[i] = strtok(NULL, delims)) != NULL; i++) {
        ; /* void */
    }

    return i;
}

/*
 * Returns true if all chars of s are whitespace, else false.
 */
bool s_is_empty(const char *s)
{
    for (; *s; s++) {
        if (!isspace(*s)) {
            return false;
        }
    }

    return true;
}

/*
 * Fills @param prompt with the shell prompt and returns a pointer to it.
 */
char *get_prompt(char *prompt)
{
    char hostname[MAX_HOSTNAME+1] = {'\0'};
    char *username = getlogin();

    gethostname(hostname, MAX_HOSTNAME);
    if (!username) {
        username = "unknown";
    }

    snprintf(prompt, MAX_PROMPT, "[%s@%s]$ ", username, hostname);
    return prompt;
}

void builtin_cd(const char **tokens, int ntokens)
{
    int r = ntokens == 1 ? chdir(getenv("HOME")) : chdir(tokens[1]);
    if (r < 0) {
        perror("cd");
    }
}

void builtin_pwd(const char **tokens ATTR_UNUSED, int ntokens)
{
    if (ntokens > 1) {
        printf("pwd: too many arguments\n");
        return;
    }
    char buf[MAX_PATH];
    getcwd(buf, MAX_PATH);
    puts(buf);
}

void builtin_exit(const char **tokens ATTR_UNUSED, int ntokens ATTR_UNUSED)
{
    exit(EXIT_SUCCESS);
}

void builtin_kill(const char **tokens, int ntokens)
{
    if (ntokens != 3) {
        printf("kill: wrong number of arguments\n");
        goto usage;
    }

    char *endptr;

    int signo = strtol(tokens[1], &endptr, 10);
    if (*endptr != '\0' || signo <= 0) {
        printf("sincos: signo must be a positive integer\n");
        goto usage;
    }

    int pid = strtol(tokens[2], &endptr, 10);
    if (*endptr != '\0' || pid <= 0) {
        printf("sincos: pid must be a positive integer\n");
        goto usage;
    }

    if (kill(pid, signo) < 0) {
        perror("kill");
    }

usage:
    printf("usage: kill <signo> <pid> \n");
}

void builtin_alias(const char **tokens, int ntokens)
{
    if (ntokens == 1) {
        /* just print all aliases and quit */

        for (alias_node *n = alias_list; n; n = n->next) {
            printf("%s=%s\n", n->alias, n->real);
        }

        return;
    }

    /* Put spaces around the first occurance of the '=' char
     * encountered and tokenize the string again. */

    char command_line[MAX_INPUT];
    int coppied = 0;
    bool eq_matched = false;

    for (int i = 0; i < ntokens; i++) {
        const char *s = tokens[i];
        while (*s) {
            if (!eq_matched && *s == '=') {
                command_line[coppied++] = ' ';
                command_line[coppied++] = '=';
                command_line[coppied++] = ' ';
                eq_matched = true;
            } else {
                command_line[coppied++] = *s;
            }
            s++;
        }
        command_line[coppied++] = ' ';
    }
    command_line[coppied] = '\0';

    char *new_tokens[MAX_TOKENS];
    ntokens = s_tokenize(command_line, new_tokens, MAX_TOKENS, " ");

    /* check whether this is an alias assignment */

    if (ntokens >= 3 && !strcmp(new_tokens[2], "=")) {

        if (ntokens == 3 || !strcmp(new_tokens[1], "=") || !strcmp(new_tokens[3], "=")) {
            puts("alias: wrong syntax");
            puts("usage: alias foo=bar");
            return;
        }

        char tmp_buf[MAX_INPUT] = {'\0'};

        /* search if there's already an alias with the same name and update it */

        for (alias_node *n = alias_list; n; n = n->next) {
            if (!strcmp(n->alias, new_tokens[1])) {
                free(n->real);

                for (int i = 3; i < ntokens; i++) {
                    strcat(tmp_buf, new_tokens[i]);
                    strcat(tmp_buf, " ");
                }
                n->real = malloc(strlen(tmp_buf));
                strcpy(n->real, tmp_buf);

                return;
            }
        }

        /* add a new alias node in the list */

        alias_node *new = malloc(sizeof(alias_node));
        new->alias = malloc(strlen(new_tokens[1]));
        strcpy(new->alias, new_tokens[1]);

        for (int i = 3; i < ntokens; i++) {
            strcat(tmp_buf, new_tokens[i]);
            strcat(tmp_buf, " ");
        }
        new->real = malloc(strlen(tmp_buf));
        strcpy(new->real, tmp_buf);

        new->next = alias_list;
        alias_list = new;

        return;
    }

    /* if any of the tokens matches any alias print them */

    for (int i = 1; i < ntokens; i++) {
        for (alias_node *n = alias_list; n; n = n->next) {
            if (!strcmp(n->alias, new_tokens[i])) {
                printf("%s=%s\n", n->alias, n->real);
            }
        }
    }
}

void delete_all_aliases(void)
{
    alias_node *tmp = NULL;
    for (; alias_list; alias_list = tmp) {
        tmp = alias_list->next;
        free(alias_list->alias);
        free(alias_list->real);
        free(alias_list);
    }
}

void builtin_unalias(const char **tokens, int ntokens)
{
    if (ntokens == 1) {
        printf("unalias: not enough arguments\n");
        return;
    }

    if (ntokens == 2 && !strcmp(tokens[1], "-a")) {
        delete_all_aliases();
        return;
    }

    for (int i = 1; i < ntokens; i++) {
        bool found = false;

        alias_node *n = alias_list, *prev = NULL;
        for (; n; prev = n, n = n->next) {
            if (!strcmp(n->alias, tokens[i])) {
                free(n->alias);
                free(n->real);

                if (!prev) {
                    alias_list = n->next;
                } else {
                    prev->next = n->next;
                }
                free(n);

                printf("unalias: successfully deleted alias: %s\n", tokens[i]);
                found = true;
                break;
            }
        }

        if (!found) {
            printf("unalias: not such alias found: %s\n", tokens[i]);
        }
    }
}

/*
 * Checks if the first token matches a built-in command.
 *
 * If so, it dispatches the call to the appropriate built-in handler and
 * returns true. Otherwise it returns false.
 */
bool call_builtin(const char **tokens, int ntokens)
{
    size_t num_builtins = sizeof(builtins) / sizeof(struct builtin_pair);
    for (size_t i = 0; i < num_builtins; i++) {
        if (!strcmp(tokens[0], builtins[i].name)) {
            (*builtins[i].fp)(tokens, ntokens);
            return true;
        }
    }
    return false;
}

/*
 * Checks if the first token matches any alias.
 *
 * In case it does, it re-builds tokens using the resulting command after
 * doing the alias replacement and updates the value of ntokens.
 *
 * When there is no match, tokens and ntokens remain unchanged.
 */
void replace_alias(char **tokens, int *ntokens)
{
    alias_node *n = alias_list;
    for (; n; n = n->next) {
        if (!strcmp(n->alias, tokens[0])) {
            break;
        }
    }

    if (n) {
        char final_cmd[MAX_INPUT];
        strcpy(final_cmd, n->real);

        for (int i = 1; i < *ntokens; i++) {
            strcat(final_cmd, " ");
            strcat(final_cmd, tokens[i]);
        }

        *ntokens = s_tokenize(final_cmd, tokens, MAX_TOKENS, " ");
    }

    tokens[*ntokens] = NULL;
}

void exec_cmd_simple(cmd_simple *cmd)
{
    char *tokens[MAX_TOKENS];
    int ntokens = s_tokenize(cmd->name, tokens, MAX_TOKENS, " ");

    replace_alias(tokens, &ntokens);
    if (call_builtin((const char **) tokens, ntokens)) {
        return;
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

int main(void)
{
    char prompt[MAX_PROMPT+1];

    while (true) {
        char *input = readline(get_prompt(prompt));
        if (!s_is_empty(input)) {
            add_history(input);
        }

        YY_BUFFER_STATE buffer = yy_scan_string(input);

        if (!yyparse() && !empty_line) {
            exec_cmd(&final_cmd);
        }

        yy_delete_buffer(buffer);
        free_cmd(&final_cmd);
        free(input);
    }

    return EXIT_SUCCESS;
}
