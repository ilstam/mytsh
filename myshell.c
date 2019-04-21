#include <stdio.h>
#include <stdlib.h>
#include "parser.tab.h"

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
    YY_BUFFER_STATE buffer;

    buffer = yy_scan_string("ls1 foo bar < in.txt > out.txt ");
    yyparse();
    yy_delete_buffer(buffer);
    exec_cmd(&final_cmd);
    free_cmd(&final_cmd);

    buffer = yy_scan_string("lsallo foo2 bar2 < in2.txt > out2.txt &");
    yyparse();
    yy_delete_buffer(buffer);
    exec_cmd(&final_cmd);
    free_cmd(&final_cmd);

    buffer = yy_scan_string("ls1 | ls2");
    yyparse();
    yy_delete_buffer(buffer);
    exec_cmd(&final_cmd);
    free_cmd(&final_cmd);

    buffer = yy_scan_string("ls3 | ls4");
    yyparse();
    yy_delete_buffer(buffer);
    exec_cmd(&final_cmd);
    free_cmd(&final_cmd);

    buffer = yy_scan_string("test1 papa | test2 this is < in3.txt | test3 &");
    yyparse();
    yy_delete_buffer(buffer);
    exec_cmd(&final_cmd);
    free_cmd(&final_cmd);

    return 0;
}
