%{
#include "parser.tab.h"
#include <string.h>
%}

%option noinput
%option nounput

CHAR            [0-9a-zA-Z_/\-\*\.\=]

%%

"\n"            { return 0; }
"&"             { return BACKGROUND; }
"|"             { return PIPE;}
">"             { return OUTPUT_REDIR; }
"<"             { return INPUT_REDIR; }
{CHAR}+         { yylval.string = strdup(yytext); return STRING; }
.               ;

%%
