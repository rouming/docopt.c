#ifndef DOCOPT_H
#define DOCOPT_H

#include <errno.h>
#include "list.h"
#include "hash.h"

enum {
	T_STR,
	T_FLAG,
	T_REQGRP,
	T_OPTGRP,
};

enum {
	F_SEP = 1<<16,
	F_ARR = 1<<17,
	F_VAL = 1<<18,

	F_MASK = (F_SEP | F_ARR | F_VAL)
};

struct str {
	const char *ptr;
	size_t len;
};

struct arg {
	unsigned type;
	unsigned flags;
	struct cmd *cmd;
	char *name;
	struct arg *prev;          /* prev argument in the stack */
	struct list_head args;     /* list of children args in group */
	struct list_head argsent;  /* entry in args of a parent group */
	struct list_head entry;    /* entry in rawargs or reqgrps or optgrps */
	struct list_head hlistent; /* entry in hashed_args->list */
};

struct hashed_args {
	struct hash_entry hentry;
	struct list_head list;
	char *name;
	unsigned type;
	unsigned flags;
};

struct cmd {
	unsigned optgrpsnum;
	unsigned reqgrpsnum;
	struct arg *stack;         /* stack of groups */
	struct list_head args;     /* list of args for this command */
	struct list_head rawargs;  /* list of all non-group args  */
	struct list_head reqgrps;  /* list of required groups */
	struct list_head optgrps;  /* list of optional groups */
	struct list_head cmdsent;  /* entry in ctx->cmds */
};

struct ctx {
	char basename[32];
	FILE *in;
	FILE *yyaccout;
	FILE *lexout;
	FILE *hdrout;
	bool interactive;
	bool havearrays;
	unsigned cmdsnum;
	struct list_head cmds;
	struct hash_table uniqargs; /* hashed non-group unique args: hashed_args */
};

void lex_beginUSAGE(void);
void yyerror(struct ctx *ctx, const char *errstr);
void yyset_in(FILE * fp);
int yylex_destroy(void);

void ctx_oneol(struct ctx *ctx);
void ctx_onparsed(struct ctx *ctx);
void ctx_onerror(struct ctx *ctx);

void ctx_dump(struct ctx *ctx);
void ctx_freecmds(struct ctx *ctx);
void ctx_free(struct ctx *ctx);
void ctx_init(struct ctx *ctx);
int ctx_newcmd(struct ctx *ctx);
int ctx_newarg(struct ctx *ctx, unsigned type, unsigned flags,
	       const char *name, size_t len);
void ctx_poparg(struct ctx *ctx);
struct cmd *ctx_lastcmd(struct ctx *ctx);

#define CTX_ONERROR(ctx) ({				\
	if (ctx->interactive) {				\
		ctx_freecmds(ctx);			\
		yyerrok;				\
		printf("> ");				\
	} else						\
		/* For non-interactive mode we never	\
		   proceed further. Just return from	\
		   yyparse() */				\
		return -1;				\
})

#define CTX_NEWCMD(ctx) ({			\
	int rc = ctx_newcmd(ctx);		\
	if (rc)					\
		return rc;			\
})

#define CMD_PUSH(ctx, typeflags, ptr, len) ({			\
	unsigned type  = (typeflags) & ~F_MASK;			\
	unsigned flags = (typeflags) & F_MASK;			\
	int rc = ctx_newarg(ctx, type, flags, ptr, len);	\
	if (rc)							\
		return rc;					\
})

#define CMD_POP(ctx) ({						\
	ctx_poparg(ctx);					\
})

#define ARG_SET(ctx, flag) ({					\
	struct cmd *cmd = ctx_lastcmd(ctx);			\
	struct arg *arg;					\
								\
	if (cmd->stack == NULL)					\
		arg = list_last_entry(&cmd->args,		\
				      struct arg, argsent);	\
	else							\
		arg = list_last_entry(&cmd->stack->args,	\
				      struct arg, argsent);	\
	arg->flags |= flag;					\
})

#endif /* DOCOPT_H */
