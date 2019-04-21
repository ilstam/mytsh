%{
#include "parser.tab.h"
#include <string.h>
%}

CHAR            [0-9a-zA-Z_/\-\*\.]

%%

exit            { return EXIT; }
"\n"            { return 0; }
"&"             { return BACKGROUND; }
"|"             { return PIPE;}
">"             { return OUTPUT_REDIR; }
"<"             { return INPUT_REDIR; }
{CHAR}+         { yylval.string = strdup(yytext); return STRING; }
.               ;

%%
