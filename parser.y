%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int yylex(void);
void yyerror(char *s);
%}

%code requires {
    #include <stdbool.h>

    typedef enum {
        CMD_SIMPLE,
        CMD_PIPE,
    } cmd_type;

    typedef struct {
        char name[5000];
        char in_redir[5000];
        char out_redir[5000];
        bool bg;
    } cmd_simple;

    typedef struct {
        struct command *left;
        struct command *right;
    } cmd_pipe;

    typedef struct command {
        cmd_type type;
        union {
            cmd_simple simple;
            cmd_pipe pipe;
        };
    } command;

    extern command final_cmd;
}

%union {
    bool bool_type;
    char *string;
    char string_buffer[5000];
    struct {char *in; char *out;} redir_pair;
    cmd_simple cmd_simple;
    command command;
}

%type <command> pipeline
%type <cmd_simple> simple
%type <string_buffer> command
%type <redir_pair> redir
%type <string> input_redir
%type <string> output_redir
%type <bool_type> back_ground

%start cmd_line
%token EXIT PIPE INPUT_REDIR OUTPUT_REDIR STRING NL BACKGROUND

%%
cmd_line :
         | EXIT {}
         | pipeline back_ground {
                                  final_cmd = $1;
                                  if (final_cmd.type == CMD_SIMPLE) {
                                      final_cmd.simple.bg = $2;
                                  } else {
                                      final_cmd.pipe.right->simple.bg = $2;
                                  }
                                }
         ;

back_ground : BACKGROUND  { $$ = true; }
            | /* empty */ { $$ = false; }
            ;

simple : command redir {
                         strcpy($$.name, $1);
                         strcpy($$.in_redir, $2.in);
                         strcpy($$.out_redir, $2.out);
                        }
       ;

command : command STRING { strcat($$, " "); strcat($$, yylval.string); }
        | STRING         { strcpy($$, yylval.string); }
        ;

redir : input_redir output_redir { $$.in = $1; $$.out = $2; }
      ;

input_redir : INPUT_REDIR STRING { $$ = yylval.string; }
            | /* empty */        { $$ = ""; }
            ;
output_redir : OUTPUT_REDIR STRING { $$ = yylval.string; }
             | /* empty */         { $$ = ""; }
             ;

pipeline : simple               { $$.type = CMD_SIMPLE; $$.simple = $1; }
         | pipeline PIPE simple {
                                  $$.type = CMD_PIPE;

                                  command *left = malloc(sizeof(command));
                                  *left = $1;
                                  $$.pipe.left = left;

                                  command *right = malloc(sizeof(command));
                                  right->simple = $3;
                                  $$.pipe.right = right;
                                }
         ;
%%

void yyerror(char *s)
{
    fprintf(stderr, "parsing error: %s\n", s);
}
