%{
#include "docopt.tab.h"

static inline void yylval_setstr(const char *ptr, size_t len)
{
	yylval.str.len = len;
	yylval.str.ptr = ptr;
}

%}

%option nounput
%option noinput
%option nodefault
%option yylineno

%s OPTIONS USAGE

NUM         ([0-9]*)
ARG         ([a-zA-Z_][a-zA-Z0-9\-_.]*)
POSARG      (\<{ARG}\>)
POSARG_DDD  ({POSARG}[.]{3})
WORD        ([a-zA-Z0-9\-_.<>]*)
ANY         ([^ \t\n]*)

%%

 /* single character ops */
"=" |
"|" |
"(" |
")" |
"[" |
"]" { return yytext[0]; }

Options([ ]*): { BEGIN(OPTIONS); }
Usage([ ]*):   { BEGIN(USAGE); }

<INITIAL>{ANY}
<OPTIONS>{ANY} /* TODO: currently ignore options */

<USAGE>{NUM}         { yylval_setstr(yytext, yyleng); return WORD; }
<USAGE>--{ARG}       { yylval_setstr(yytext, yyleng); return OPTARG; }
<USAGE>-{ARG}        { yylval_setstr(yytext, yyleng); return OPTARG; }
<USAGE>{POSARG_DDD}  { yylval_setstr(yytext + 1, yyleng - 5); return POSARG_DDD; }
<USAGE>{POSARG}      { yylval_setstr(yytext + 1, yyleng - 2); return POSARG; }
<USAGE>{ARG}         { yylval_setstr(yytext, yyleng); return ARG; }
<USAGE>{WORD}        { yylval_setstr(yytext, yyleng); return WORD; }

[ \t]+  /* ignore whitespaces */
"\n"    { return EOL; }
.       { return yytext[0]; }
%%

void lex_beginUSAGE(void)
{
	BEGIN(USAGE);
}

int yywrap(void)
{
	return 1;
}

#if 0
main(int argc, char **argv)
{
   yylex();
}
#endif
