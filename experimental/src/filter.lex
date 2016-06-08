%{

#include <string>

#define SAVE_TOKEN yylval.string = new std::strign(yytext, yyleng)
#define TOKEN(Tok) (yylval.token = Tok)

%}

%%

[ \t\n]               ;
[a-zA-Z][a-zA-Z0-9_]* SAVE_TOKEN; return T_IDENTIFIER;
[0-9]+                SAVE_TOKEN; return T_INTEGER;
":"                   return TOKEN(T_SEMICOLON);
.                     fprintf(stderr, "Unknown token!\n"); yyterminate();

%%
