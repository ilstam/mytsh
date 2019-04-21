#include <stdio.h>
#include <stdlib.h>
#include "parser.tab.h"

#define MAX_INPUT 5000

command final_cmd;

typedef struct yy_buffer_state *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string (char *str);
extern void yy_delete_buffer (YY_BUFFER_STATE buffer);


void free_cmd(command *cmd)
{
    if (cmd->type == CMD_SIMPLE) {
    } else {
        free(cmd->pipe.right);
        free_cmd(cmd->pipe.left);
        free(cmd->pipe.left);
    }
}

void exec_cmd(command *cmd)
{
    if (cmd->type == CMD_SIMPLE) {
        printf("__exec__\n");
        printf("name: %s\n", cmd->simple.name);
        printf("args: %s\n", cmd->simple.args);
        printf("in: %s\n", cmd->simple.in_redir);
        printf("out: %s\n", cmd->simple.out_redir);
        printf("bg: %d\n", cmd->simple.bg);
        puts("");
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
        printf("myshell$ ");

        fgets(input, MAX_INPUT, stdin);

        buffer = yy_scan_string(input);

        if ((retval = yyparse())) {
            continue;
        }

        yy_delete_buffer(buffer);
        exec_cmd(&final_cmd);
        free_cmd(&final_cmd);
    }

    return 0;
}
