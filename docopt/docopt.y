%{
#include <stdio.h>

int yylex(void);

//int yydebug = 1;

%}

%code requires {
	#include "docopt.h"
}
%union {
	struct str str;
	struct arg *arg;
}

%locations
%define parse.error verbose

%parse-param { struct ctx *ctx }

%token <str> ARG OPTARG POSARG POSARG_DDD WORD
%token EOL

%start input

%%

input: input line
     | line

line: EOL                                    { ctx_oneol(ctx); }
    | ARG { CTX_NEWCMD(ctx); } list-args EOL { ctx_onparsed(ctx); }
    | error EOL                              { CTX_ONERROR(ctx); }

list-args: list-args '|' { ARG_SET(ctx, F_SEP); } arg
		 | list-args arg
         | arg

arg: { CMD_PUSH(ctx, T_REQGRP, NULL, 0); } '(' list-args ')' { CMD_POP(ctx); }
   | { CMD_PUSH(ctx, T_OPTGRP, NULL, 0); } '[' list-args ']' { CMD_POP(ctx); }
   | POSARG                 { CMD_PUSH(ctx, T_STR, $1.ptr, $1.len); }
   | POSARG_DDD             { CMD_PUSH(ctx, T_STR | F_ARR, $1.ptr, $1.len); }
   | OPTARG                 { CMD_PUSH(ctx, T_FLAG, $1.ptr, $1.len); }
   | OPTARG '=' POSARG      { CMD_PUSH(ctx, T_STR | F_VAL, $1.ptr, $1.len); }
   | OPTARG '=' POSARG_DDD  { CMD_PUSH(ctx, T_STR | F_VAL | F_ARR, $1.ptr, $1.len); }
   | ARG                    { CMD_PUSH(ctx, T_FLAG, $1.ptr, $1.len); }

%%
