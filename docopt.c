#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <linux/limits.h>

#include "docopt.tab.h"
#include "docopt.h"

void yyerror(struct ctx *ctx, const char *errstr)
{
	fprintf(stderr, "Error: %s\n", errstr);
}

static bool argtype_isgroup(unsigned type)
{
	return type == T_REQGRP || type == T_OPTGRP;
}

static bool arg_isgroup(struct arg *arg)
{
	return argtype_isgroup(arg->type);
}

__attribute__((unused))
static const char *arg_strtype(struct arg *arg)
{
	bool s = arg->flags & F_SEP;
	bool a = arg->flags & F_ARR;

	switch (arg->type) {
	case T_FLAG:
		return s ? "FLAG|" : "FLAG";
	case T_STR:
		return  s && a  ? "STR[]|" :
			s && !a ? "STR|" :
		       !s && a  ? "STR[]" :
			          "STR";
	case T_REQGRP:
		return s ? "REQGRP|" : "REQGRP";
	case T_OPTGRP:
		return s ? "OPTGRP|" : "OPTGRP";
	default:
		assert(0);
		return NULL;
	}
}

static void arg_init(struct arg *arg, struct cmd *cmd, unsigned type,
		     unsigned flags, char *name, size_t len)
{
	arg->cmd = cmd;
	arg->type = type;
	arg->flags = flags;
	arg->name = name;
	arg->prev = NULL;
	INIT_LIST_HEAD(&arg->args);
}

static struct arg *arg_alloc(struct cmd *cmd, unsigned type, unsigned flags,
			     const char *name, size_t len)
{
	struct arg *arg;
	char *dupname;

	arg = malloc(sizeof(*arg));
	if (arg == NULL)
		return NULL;
	dupname = strndup(name, len);
	if (dupname == NULL) {
		free(arg);
		return NULL;
	}
	arg_init(arg, cmd, type, flags, dupname, len);

	return arg;
}

static void arg_free(struct arg *arg)
{
	free(arg->name);
	free(arg);
}

static void cmd_init(struct cmd *cmd)
{
	cmd->stack = NULL;
	cmd->optgrpsnum = 0;
	cmd->reqgrpsnum = 0;
	INIT_LIST_HEAD(&cmd->args);
	INIT_LIST_HEAD(&cmd->rawargs);
	INIT_LIST_HEAD(&cmd->reqgrps);
	INIT_LIST_HEAD(&cmd->optgrps);
}

static struct cmd *cmd_alloc(void)
{
	struct cmd *cmd;

	cmd = malloc(sizeof(*cmd));
	if (cmd == NULL)
		return NULL;
	cmd_init(cmd);

	return cmd;
}

static void cmd_free(struct cmd *cmd)
{
	free(cmd);
}

int ctx_newcmd(struct ctx *ctx)
{
	struct cmd *cmd;

	cmd = cmd_alloc();
	if (cmd == NULL)
		return -ENOMEM;
	ctx->cmdsnum += 1;
	list_add_tail(&cmd->cmdsent, &ctx->cmds);

	return 0;
}

static void cmd_push(struct cmd *cmd, struct arg *arg)
{
	struct arg *top;

	top = cmd->stack;
	arg->prev = top;
	cmd->stack = arg;
}

static void cmd_pop(struct cmd *cmd)
{
	struct arg *top;

	top = cmd->stack;
	assert(top);
	cmd->stack = top->prev;
	top->prev = NULL;
}

void ctx_poparg(struct ctx *ctx)
{
	struct cmd *cmd;

	cmd = ctx_lastcmd(ctx);
	assert(cmd);
	cmd_pop(cmd);
}

static struct hashed_args *hargs_alloc(const struct arg *arg)
{
	struct hashed_args *hargs;

	hargs = malloc(sizeof(*hargs));
	if (hargs == NULL)
		return NULL;

	hargs->name = strdup(arg->name);
	if (hargs->name == NULL) {
		free(hargs);

		return NULL;
	}

	hargs->type = arg->type;
	hargs->flags = arg->flags;

	INIT_LIST_HEAD(&hargs->list);
	hash_entry_init(&hargs->hentry, hargs->name, strlen(hargs->name));

	return hargs;
}

static void hargs_free(struct hashed_args *hargs)
{
	free(hargs->name);
	free(hargs);
}

static int ctx_hasharg(struct ctx *ctx, struct arg *arg)
{
	struct arg *other;
	struct hash_entry *hent;
	struct hashed_args *found;
	unsigned int hint;
	char buf[128];
	size_t len;

	len = strlen(arg->name);
	hent = hash_lookup(&ctx->uniqargs, arg->name, len, &hint);
	if (hent == NULL) {
		found = hargs_alloc(arg);
		if (found == NULL)
			return -ENOMEM;

		list_add_tail(&arg->hlistent, &found->list);
		hash_insert(&ctx->uniqargs, &found->hentry, &hint);

		return 0;
	}
	found = container_of(hent, typeof(*found), hentry);
	list_for_each_entry(other, &found->list, hlistent) {
		if (other->cmd == arg->cmd) {
			snprintf(buf, sizeof(buf),
				 "found arguments with similar names: '%s'",
				 arg->name);
			yyerror(ctx, buf);

			return -EINVAL;
		}
		if (other->type != arg->type) {
			snprintf(buf, sizeof(buf),
				 "found arguments with different types: '%s'",
				 arg->name);
			yyerror(ctx, buf);

			return -EINVAL;
		}
	}
	list_add_tail(&arg->hlistent, &found->list);
	found->flags |= arg->flags & F_ARR;

	return 0;
}

int ctx_newarg(struct ctx *ctx, unsigned type, unsigned flags,
	       const char *name, size_t len)
{
	struct cmd *cmd;
	struct arg *arg;
	char buf[32];
	int rc;

	cmd = ctx_lastcmd(ctx);
	assert(cmd);

	if (name == NULL) {
		const char *grp = (type == T_OPTGRP ? "opt" : "req");
		unsigned num = (type == T_OPTGRP ? cmd->optgrpsnum :
				cmd->reqgrpsnum);
		assert(argtype_isgroup(type));
		len = snprintf(buf, sizeof(buf), "cmd%d-%sgrp%d",
			       ctx->cmdsnum, grp, num + 1);
		name = buf;
	}

	ctx->havearrays |= flags & F_ARR;

	arg = arg_alloc(cmd, type, flags, name, len);
	if (arg == NULL)
		return -ENOMEM;

	if (type == T_REQGRP) {
		cmd->reqgrpsnum += 1;
		list_add_tail(&arg->entry, &cmd->reqgrps);
	} else if (type == T_OPTGRP) {
		cmd->optgrpsnum += 1;
		list_add_tail(&arg->entry, &cmd->optgrps);
	} else {
		list_add_tail(&arg->entry, &cmd->rawargs);
		rc = ctx_hasharg(ctx, arg);
		if (rc)
			return rc;
	}

	if (cmd->stack == NULL)
		list_add_tail(&arg->argsent, &cmd->args);
	else
		list_add_tail(&arg->argsent, &cmd->stack->args);

	if (arg_isgroup(arg))
		cmd_push(cmd, arg);

	return 0;
}

struct cmd *ctx_lastcmd(struct ctx *ctx)
{
	assert(!list_empty(&ctx->cmds));
	return list_last_entry(&ctx->cmds, struct cmd, cmdsent);
}

void ctx_init(struct ctx *ctx)
{
	strcpy(ctx->basename, "HEADER_IS_HERE");
	ctx->yyaccout = stdout;
	ctx->lexout = stdout;
	ctx->hdrout = stdout;
	ctx->interactive = false;
	ctx->havearrays = false;
	ctx->cmdsnum = 0;
	INIT_LIST_HEAD(&ctx->cmds);
	hash_init(&ctx->uniqargs);
}

void ctx_freecmds(struct ctx *ctx)
{
	struct hashed_args *hargs, *tmphargs;
	struct cmd *cmd, *tmpcmd;
	struct arg *arg, *tmparg;

	list_for_each_entry_safe(cmd, tmpcmd, &ctx->cmds, cmdsent) {
		list_for_each_entry_safe(arg, tmparg, &cmd->rawargs, entry)
			arg_free(arg);
		list_for_each_entry_safe(arg, tmparg, &cmd->reqgrps, entry)
			arg_free(arg);
		list_for_each_entry_safe(arg, tmparg, &cmd->optgrps, entry)
			arg_free(arg);
		list_del(&cmd->cmdsent);
		cmd_free(cmd);
	}
	hash_for_each_entry_safe(hargs, tmphargs, &ctx->uniqargs, hentry) {
		hash_remove(&hargs->hentry);
		hargs_free(hargs);
	}
	ctx->cmdsnum = 0;
	ctx->havearrays = false;
}

void ctx_free(struct ctx *ctx)
{
	ctx_freecmds(ctx);
	if (ctx->in)
		fclose(ctx->in);
	if (ctx->yyaccout && ctx->yyaccout != stdout)
		fclose(ctx->yyaccout);
	if (ctx->lexout && ctx->lexout != stdout)
		fclose(ctx->lexout);
	if (ctx->hdrout && ctx->hdrout != stdout)
		fclose(ctx->hdrout);
}

static int print_strtoupper(FILE *out, const char *str)
{
	int i, cnt;

	for (cnt = 0, i = 0; i < strlen(str); i++) {
		if (isalnum(str[i])) {
			cnt++;
			fprintf(out, "%c", toupper(str[i]));
		}
	}

	return cnt;
}

static int print_strtolower(FILE *out, const char *str)
{
	int i, cnt;

	for (cnt = 0, i = 0; i < strlen(str); i++) {
		if (isalnum(str[i])) {
			cnt++;
			fprintf(out, "%c", tolower(str[i]));
		}
	}

	return cnt;
}

static void hdr_dumpusage(struct ctx *ctx)
{
	FILE *out = ctx->hdrout;
	char *line = NULL, *nl;
	size_t len = 0;
	ssize_t read;

	if (ctx->interactive) {
		fprintf(out, "/* TODO: extract interactive input from lex /*\n");
		fprintf(out, "static const char * const cli_usage = ");
		fprintf(out, "\"Usage: CMD\";\n");
	} else {
		(void)fseek(ctx->in, 0, SEEK_SET);

		fprintf(out, "static const char * const cli_usage =");
		while ((read = getline(&line, &len, ctx->in)) != -1) {
			nl = strchr(line, '\n');
			if (nl)
				*nl = '\0';
			fprintf(out, "\n\t\"%s\\n\"", line);
		}
		fprintf(out, ";\n");
		free(line);
	}
}

static void hdr_dump(struct ctx *ctx)
{
	const char *header[] = {
		"/*",
		" * This is common header for command line interface parser",
		" * generated by docopt.c utility.",
		" */",
		"",
	};
	const char *body[] = {
		"",
		"int cli_parse(int argc, char **argv, struct cli *cli);",
		"void cli_free(struct cli *cli);",
		"",
	};
	FILE *out = ctx->hdrout;
	struct hashed_args *hargs;
	int i;

	for (i = 0; i < ARRAY_SIZE(header); i++)
		fprintf(out, "%s\n", header[i]);

	fprintf(out, "#ifndef __");
	print_strtoupper(out, ctx->basename);
	fprintf(out, "_H__\n");
	fprintf(out, "#define __");
	print_strtoupper(out, ctx->basename);
	fprintf(out, "_H__\n");
	fprintf(out, "\n");
	fprintf(out, "struct cli {\n");

	/*
	 * Print C structure of CLI members
	 */
	hash_for_each_entry(hargs, &ctx->uniqargs, hentry) {
		if (hargs->type != T_STR)
			continue;
		if (hargs->flags & F_ARR) {
			fprintf(out, "	char **");
			print_strtolower(out, hargs->name);
			fprintf(out, "_arr;\n");
			fprintf(out, "	unsigned ");
			print_strtolower(out, hargs->name);
			fprintf(out, "_num;\n");
		} else {
			fprintf(out, "	char *");
			print_strtolower(out, hargs->name);
			fprintf(out, ";\n");
		}
	}
	hash_for_each_entry(hargs, &ctx->uniqargs, hentry) {
		if (hargs->type == T_FLAG) {
			fprintf(out, "	unsigned ");
			print_strtolower(out, hargs->name);
			fprintf(out, ";\n");
		}
	}
	fprintf(out, "};\n\n");

	hdr_dumpusage(ctx);

	for (i = 0; i < ARRAY_SIZE(body); i++)
		fprintf(out, "%s\n", body[i]);

	fprintf(out, "#endif /* __");
	print_strtoupper(out, ctx->basename);
	fprintf(out, "_H__ */\n");
}

static void lex_dumpheader(struct ctx *ctx)
{
	const char *header1[] = {
		"/*",
		" * This is lex scanner for command line interface parser",
		" * generated by docopt.c utility.",
		" */",
		"",
		"%{",
	};
	const char *header2[] = {
		"",
		"extern int yycurarg;",
		"extern int yyargc;",
		"extern char **yyargv;",
		"",
		"%}",
		"",
		"%option nounput",
		"%option noinput",
		"%option nodefault",
		"",
		"%%",
		"",
		" /* single character ops */",
		"\"=\" { return yytext[0]; }",
		""
	};
	const char *header3[] = {
                "[^ \\t\\n=]+  { yylval.str = yytext; return WORD; }",
                "[ \\t]       { /* ignore whitespace */ }",
                "\\n          { yyterminate(); }",
		"",
		"<<EOF>> {",
		"	YY_BUFFER_STATE buf;",
		"",
		"	/* Just take another string from an argument array */",
		"",
		"	if (++yycurarg == yyargc)",
		"		yyterminate();",
		"",
		"	yy_delete_buffer(YY_CURRENT_BUFFER);",
		"	buf = yy_scan_string(yyargv[yycurarg]);",
		"	if (buf == NULL)",
		"		yyterminate();",
		"	yy_switch_to_buffer(buf);",
		"}",
		"%%",
		"",
		"int yywrap(void)",
		"{",
		"	/*",
		"	 * With '%option noyywrap' flex can generate code which",
		"	 * gcc does not like and complains with '\"yywrap\" redefined'.",
		"	 */",
		"	return 1;",
		"}",
	};
	FILE *out = ctx->lexout;
	struct hashed_args *hargs;
	int i;

	for (i = 0; i < ARRAY_SIZE(header1); i++)
		fprintf(out, "%s\n", header1[i]);
	fprintf(out, "#include \"%s.tab.h\"\n", ctx->basename);
	for (i = 0; i < ARRAY_SIZE(header2); i++)
		fprintf(out, "%s\n", header2[i]);

	/*
	 * Print patterns of terminal symbols (tokens)
	 */
	hash_for_each_entry(hargs, &ctx->uniqargs, hentry) {
		if (hargs->type == T_FLAG || hargs->flags & F_VAL) {
			fprintf(out, "\"%s\" { return ", hargs->name);
			print_strtoupper(out, hargs->name);
			fprintf(out, "; }\n");
		}
	}
	fprintf(out, "\n");

	for (i = 0; i < ARRAY_SIZE(header3); i++)
		fprintf(out, "%s\n", header3[i]);
}

static void lex_dump(struct ctx *ctx)
{
	lex_dumpheader(ctx);
}

static bool ctx_isarr(struct ctx *ctx, const struct arg *arg)
{
	struct hash_entry *hent;
	struct hashed_args *found;

	hent = hash_lookup(&ctx->uniqargs, arg->name,
			   strlen(arg->name), NULL);
	if (hent == NULL)
		return false;

	found = container_of(hent, typeof(*found), hentry);

	return found->flags & F_ARR;
}

static unsigned yacc_dumparg(struct ctx *ctx, struct arg *arg, unsigned refs)
{
	FILE *out = ctx->yyaccout;

	if (arg_isgroup(arg))
		fprintf(out, "%s", arg->name);
	else if (arg->type == T_STR) {
		bool arr;

		/*
		 * Arrays (repeating strings) are for the whole context,
		 * not only for this command.  Also arguments with values
		 * along with repeating strings require separate rules
		 * (see yacc_dumprules()).
		 */
		arr = ctx_isarr(ctx, arg);
		if ((arg->flags & F_ARR) && (arg->flags & F_VAL)) {
			/* Intermediate rule with '-arr' suffix to
			   cover repeating strings with values */
			print_strtolower(out, arg->name);
			fprintf(out, "-arr");
		} else if (arr || arg->flags & F_VAL) {
			print_strtolower(out, arg->name);
		} else {
			refs += 1;
			fprintf(out, "WORD[ref%u]", refs);
			if (arr) {
				fprintf(out, " { CLI_STRDUP_ARR(cli, ");
				print_strtolower(out, arg->name);
				fprintf(out, ", $<str>ref%u); }", refs);
			} else {
				fprintf(out, " { CLI_STRDUP(cli, ");
				print_strtolower(out, arg->name);
				fprintf(out, ", $<str>ref%u); }", refs);
			}
		}
	} else {
		print_strtoupper(out, arg->name);
		fprintf(out, " { cli->");
		print_strtolower(out, arg->name);
		fprintf(out, " = 1; }");
	}

	return refs;
}

static void yacc_dumptokens(struct ctx *ctx)
{
	FILE *out = ctx->yyaccout;
	struct hashed_args *hargs;

	fprintf(out, "%%token <str> WORD");

	hash_for_each_entry(hargs, &ctx->uniqargs, hentry) {
		if (hargs->type == T_FLAG || hargs->flags & F_VAL) {
			fprintf(out, " ");
			print_strtoupper(out, hargs->name);
		}
	}

	fprintf(out, "\n");
}

static void yacc_dumpheader(struct ctx *ctx)
{
	static const char *header1[] = {
		"/*",
		" * This is bison grammar for command line interface parser",
		" * generated by docopt.c.",
		" */",
		"",
		"%{",
		"#include <stdio.h>",
		"#include <string.h>",
		"#include <errno.h>",
		"",
		"static int error;",
		"",
		"int yyargc;",
		"int yycurarg;",
		"char **yyargv;",
		"",
		"struct cli;",
		"",
		"int yylex(struct cli *cli);",
		"void yyerror(struct cli *cli, const char *err);",
		"int yylex_destroy(void);",
		"",
		"typedef struct yy_buffer_state* YY_BUFFER_STATE;",
		"void yy_switch_to_buffer(YY_BUFFER_STATE buf);",
		"YY_BUFFER_STATE yy_scan_string(const char *yy_str);",
		"",
		"#define CLI_STRDUP(ptr, member, str) ({		\\",
		"	(ptr)->member = strdup(str);		\\",
		"	if (!(ptr)->member)			\\",
		"		return -ENOMEM;			\\",
		"});",
		"",
		"#define CLI_STRDUP_ARR(ptr, member, str) ({			\\",
		"	char **newarr;						\\",
		"	size_t oldsz, num;					\\",
		"								\\",
		"	num = (ptr)->member ## _num;				\\",
		"	oldsz = sizeof(*newarr) * num;				\\",
		"	newarr = malloc(sizeof(*newarr) + oldsz);		\\",
		"	if (!newarr)						\\",
		"		return -ENOMEM;					\\",
		"	if (oldsz)						\\",
		"		memcpy(newarr, (ptr)->member ## _arr, oldsz);	\\",
		"	free((ptr)->member ## _arr);				\\",
		"	(ptr)->member ## _arr = newarr;				\\",
		"	(ptr)->member ## _arr[num] = strdup(str);		\\",
		"	if (!(ptr)->member ## _arr[num])			\\",
		"		return -ENOMEM;					\\",
		"	(ptr)->member ## _num += 1;				\\",
		"});",
		"",
		"%}",
		"%code requires {",
	};
	static const char *header2[] = {
		"}",
		"%parse-param { struct cli *cli }",
		"%lex-param { struct cli *cli }",
		"%union {",
		"	const char *str;",
		"}",
		"%define parse.error verbose",
		"",
		"%start commands",
		"",
	};
	FILE *out = ctx->yyaccout;
	int i;

	for (i = 0; i < ARRAY_SIZE(header1); i++)
		fprintf(out, "%s\n", header1[i]);
	fprintf(out, "#include \"%s.h\"\n", ctx->basename);
	for (i = 0; i < ARRAY_SIZE(header2); i++)
		fprintf(out, "%s\n", header2[i]);

	yacc_dumptokens(ctx);
}

static void yacc_dumpfooter(struct ctx *ctx)
{
	const char *footer1[] = {
		"",
		"void yyerror(struct cli *cli, const char *errstr)",
		"{",
		"	if (yycurarg >= yyargc)",
		"		fprintf(stderr, \"\\nError: required parameter is missing\\n\\n\");",
		"	else",
		"		fprintf(stderr, \"\\nError: %d parameter '%s' is incorrect\\n\\n\",",
		"			yycurarg, yyargv[yycurarg]);",
		"	error = -1;",
		"}",
		"",
		"void cli_free(struct cli *cli)",
		"{",
	};
	const char *footer2[] = {
		"}",
		"",
		"int cli_parse(int argc, char **argv, struct cli *cli)",
		"{",
		"	static char *empty_argv[] = {\"\"};",
		"	YY_BUFFER_STATE buf;",
		"",
		"	memset(cli, 0, sizeof(*cli));",
		"",
		"	if (argc < 1)",
		"		return -1;",
		"	else if (argc == 1) {",
		"		yycurarg = 0;",
		"		yyargc = 1;",
		"		yyargv = empty_argv;",
		"	} else {",
		"		yycurarg = 1;",
		"		yyargc = argc;",
		"		yyargv = argv;",
		"	}",
		"",
		"	buf = yy_scan_string(yyargv[yycurarg]);",
		"	if (buf == NULL)",
		"		return -1;",
		"	yy_switch_to_buffer(buf);",
		"	yyparse(cli);",
		"	yylex_destroy();",
		"",
		"	if (error)",
		"		cli_free(cli);",
		"",
		"	return error;",
		"}",
		"",
		"#ifdef MAIN_EXAMPLE",
		"int main(int argc, char **argv)",
		"{",
		"	struct cli cli;",
	};
	const char *footer3[] = {
		"",
		"	rc = cli_parse(argc, argv, &cli);",
		"	if (rc) {",
		"		fprintf(stderr, \"%s\\n\", cli_usage);",
		"		return -1;",
		"	}",
	};
	const char *footer4[] = {
		"	cli_free(&cli);",
		"",
		"	return 0;",
		"}",
		"#endif",
	};
	FILE *out = ctx->yyaccout;
	struct hashed_args *hargs;
	int i;

	for (i = 0; i < ARRAY_SIZE(footer1); i++)
		fprintf(out, "%s\n", footer1[i]);

	if (ctx->havearrays)
		fprintf(out, "	unsigned i;\n\n");

	/*
	 * Print free for char* members of CLI structure
	 */
	hash_for_each_entry(hargs, &ctx->uniqargs, hentry) {
		if (hargs->type != T_STR)
			continue;
		if (hargs->flags & F_ARR) {
			fprintf(out, "	for (i = 0; i < cli->");
			print_strtolower(out, hargs->name);
			fprintf(out, "_num; i++)\n");
			fprintf(out, "		free(cli->");
			print_strtolower(out, hargs->name);
			fprintf(out, "_arr[i]);\n");
			fprintf(out, "	free(cli->");
			print_strtolower(out, hargs->name);
			fprintf(out, "_arr);\n");
		} else {
			fprintf(out, "	free(cli->");
			print_strtolower(out, hargs->name);
			fprintf(out, ");\n");
		}
	}
	for (i = 0; i < ARRAY_SIZE(footer2); i++)
		fprintf(out, "%s\n", footer2[i]);

	if (ctx->havearrays)
		fprintf(out, "	int rc, i;\n");
	else
		fprintf(out, "	int rc;\n");

	for (i = 0; i < ARRAY_SIZE(footer3); i++)
		fprintf(out, "%s\n", footer3[i]);

	/*
	 * Print all members as an example
	 */
	hash_for_each_entry(hargs, &ctx->uniqargs, hentry) {
		if (hargs->type == T_STR) {
			if (hargs->flags & F_ARR) {
				fprintf(out, "	for (i = 0; i < cli.");
				print_strtolower(out, hargs->name);
				fprintf(out, "_num; i++)\n");
				fprintf(out, "		printf(\"'");
				print_strtolower(out, hargs->name);
				fprintf(out, "_arr[%%d]' = '%%s'\\n\", ");
				fprintf(out, "i, cli.");
				print_strtolower(out, hargs->name);
				fprintf(out, "_arr[i]);\n");
			} else {
				fprintf(out, "	printf(\"'");
				print_strtolower(out, hargs->name);
				fprintf(out, "' = '%%s'\\n\", ");
				fprintf(out, "cli.");
				print_strtolower(out, hargs->name);
				fprintf(out, ");\n");
			}
		} else {
			fprintf(out, "	printf(\"'");
			print_strtolower(out, hargs->name);
			fprintf(out, "' = '%%d'\\n\", ");
			fprintf(out, "cli.");
			print_strtolower(out, hargs->name);
			fprintf(out, ");\n");
		}
	}

	for (i = 0; i < ARRAY_SIZE(footer4); i++)
		fprintf(out, "%s\n", footer4[i]);
}

static void yacc_dumprules(struct ctx *ctx)
{
	FILE *out = ctx->yyaccout;
	struct hashed_args *hargs;

	/*
	 * Arrays (repating strings) and arguments with values
	 * require separate yacc rules.
	 */
	hash_for_each_entry(hargs, &ctx->uniqargs, hentry) {
		int len;

		if (hargs->flags & F_VAL) {
			len = print_strtolower(out, hargs->name);
			fprintf(out, ": ");
			print_strtoupper(out, hargs->name);
			if (hargs->flags & F_ARR)
				fprintf(out, " WORD { CLI_STRDUP_ARR(cli, ");
			else
				fprintf(out, " WORD { CLI_STRDUP(cli, ");
			print_strtolower(out, hargs->name);
			fprintf(out, ", $2); }\n");
			fprintf(out, "%*s%s", len, "", "| ");
			print_strtoupper(out, hargs->name);
			if (hargs->flags & F_ARR)
				fprintf(out, " '=' WORD { CLI_STRDUP_ARR(cli, ");
			else
				fprintf(out, " '=' WORD { CLI_STRDUP(cli, ");
			print_strtolower(out, hargs->name);
			fprintf(out, ", $3); }\n\n");
		} else if (hargs->flags & F_ARR) {
			len = print_strtolower(out, hargs->name);
			fprintf(out, ": WORD { CLI_STRDUP_ARR(cli, ");
			print_strtolower(out, hargs->name);
			fprintf(out, ", $1); }\n");
			fprintf(out, "%*s%s", len, "", "| ");
			print_strtolower(out, hargs->name);
			fprintf(out, " WORD { CLI_STRDUP_ARR(cli, ");
			print_strtolower(out, hargs->name);
			fprintf(out, ", $2); }\n\n");
		}

		/* Additional rule to combining repeating
		   arguments with values */
		if ((hargs->flags & F_VAL) && (hargs->flags & F_ARR)) {
			len = print_strtolower(out, hargs->name);
			fprintf(out, "-arr: ");
			print_strtolower(out, hargs->name);
			fprintf(out, "\n");
			fprintf(out, "%*s%s", len + 4, "", "| ");
			print_strtolower(out, hargs->name);
			fprintf(out, "-arr ");
			print_strtolower(out, hargs->name);
			fprintf(out, "\n\n");
		}
	}
}

static void yacc_dump(struct ctx *ctx)
{
	FILE *out = ctx->yyaccout;
	unsigned icmd, igrp, iarg, refs;
	struct arg *arg, *grp;
	struct cmd *cmd;
	bool sep;

	yacc_dumpheader(ctx);

	fprintf(out, "%%%%\n\n");

	icmd = 0;
	list_for_each_entry(cmd, &ctx->cmds, cmdsent) {
		if (icmd++ == 0)
			fprintf(out, "commands: cmd%d\n", icmd);
		else
			fprintf(out, "        | cmd%d\n", icmd);
	}
	fprintf(out, "\n");

	yacc_dumprules(ctx);

	icmd = 0;
	list_for_each_entry(cmd, &ctx->cmds, cmdsent) {
		int cmdlen = 0;

		refs = 0;
		iarg = 0;
		sep = false;
		list_for_each_entry(arg, &cmd->args, argsent) {
			if (iarg++ == 0)
				cmdlen = fprintf(out, "cmd%d: ", icmd+1);
			else if (sep) {
				sep = false;
				fprintf(out, "%*s%s", cmdlen - 2, "", "| ");
			}

			refs = yacc_dumparg(ctx, arg, refs);

			if (arg->flags & F_SEP ||
			    list_is_last(&arg->argsent, &cmd->args)) {
				sep = true;
				fprintf(out, "\n");
			} else
				fprintf(out, " ");
		}
		fprintf(out, "\n");

		igrp = 0;
		list_for_each_entry(grp, &cmd->optgrps, entry) {
			bool asreqgrp;

			refs = 0;
			iarg = 0;
			sep = false;
			asreqgrp = false;

			list_for_each_entry(arg, &grp->args, argsent) {
				if (arg->flags & F_SEP) {
					asreqgrp = true;
					break;
				}
			}
			list_for_each_entry(arg, &grp->args, argsent) {
				if (iarg++ == 0)
					fprintf(out, "%s:\n%*s | ", grp->name,
						(int)strlen(grp->name) - 1,
						"");
				else if (sep) {
					sep = false;
					fprintf(out, "%*s | %s ",
						(int)strlen(grp->name) - 1,
						"", grp->name);
				}

				refs = yacc_dumparg(ctx, arg, refs);

				if (!asreqgrp || arg->flags & F_SEP ||
				    list_is_last(&arg->argsent, &grp->args)) {
					sep = true;
					fprintf(out, "\n");
				} else
					fprintf(out, " ");
			}
			fprintf(out, "\n");
			igrp += 1;
		}

		igrp = 0;
		list_for_each_entry(grp, &cmd->reqgrps, entry) {
			refs = 0;
			iarg = 0;
			sep = false;
			list_for_each_entry(arg, &grp->args, argsent) {
				if (iarg++ == 0)
					fprintf(out, "%s: ", grp->name);
				else if (sep) {
					sep = false;
					fprintf(out, "%*s ",
						(int)strlen(grp->name) + 1,
						"|");
				}

				refs = yacc_dumparg(ctx, arg, refs);

				if (arg->flags & F_SEP ||
				    list_is_last(&arg->argsent, &grp->args)) {
					sep = true;
					fprintf(out, "\n");
				} else
					fprintf(out, " ");
			}
			fprintf(out, "\n");
			igrp += 1;
		}
		icmd += 1;
	}

	fprintf(out, "%%%%\n");

	yacc_dumpfooter(ctx);
}

void ctx_dump(struct ctx *ctx)
{
	hdr_dump(ctx);
	lex_dump(ctx);
	yacc_dump(ctx);
}

static int ctx_validate(struct ctx *ctx)
{
	if (list_empty(&ctx->cmds)) {
		yyerror(ctx, "no valid input");

		return -1;
	}

	return 0;
}

void ctx_oneol(struct ctx *ctx)
{
	if (ctx->interactive)
		printf("> ");
}

void ctx_onparsed(struct ctx *ctx)
{
	int rc;

	if (ctx->interactive) {
		rc = ctx_validate(ctx);
		if (rc == 0)
			ctx_dump(ctx);
		ctx_freecmds(ctx);
		printf("> ");
	}
}

static void path_nosuff(const char *path, char *buf, size_t size)
{
	const char *end;
	size_t min;

	end = strrchr(path, '.') ?: path + strlen(path);
	min = end - path < size - 1 ? end - path : size - 1;
	memcpy(buf, path, min);
	buf[min] = '\0';
}

static int ctx_setupout(struct ctx *ctx, const char *docoptpath)
{
	char filen[PATH_MAX-3];
	char path[PATH_MAX];

	path_nosuff(docoptpath, filen, sizeof(filen));
	strncpy(ctx->basename, basename(filen), sizeof(ctx->basename) - 1);
	ctx->basename[sizeof(ctx->basename) - 1] = '\0';

	snprintf(path, sizeof(path), "%s.y", filen);
	ctx->yyaccout = fopen(path, "wx");
	if (ctx->yyaccout == NULL) {
		perror(path);
		return -1;
	}
	snprintf(path, sizeof(path), "%s.l", filen);
	ctx->lexout = fopen(path, "wx");
	if (ctx->lexout == NULL) {
		perror(path);
		return -1;
	}
	snprintf(path, sizeof(path), "%s.h", filen);
	ctx->hdrout = fopen(path, "wx");
	if (ctx->hdrout == NULL) {
		perror(path);
		return -1;
	}

	return 0;
}

static int ctx_setupin(struct ctx *ctx, const char *docoptpath)
{
	ctx->in = fopen(docoptpath, "r");
	if (ctx->in == NULL) {
		perror(docoptpath);
		return -1;
	}
	yyset_in(ctx->in);

	return 0;
}

int main(int argc, char **argv)
{
	struct ctx ctx;
	int rc;

	if (argc <= 1) {
		fprintf(stderr, "Usage: [-i | <docopt>]\n");
		return -1;
	}

	ctx_init(&ctx);

	if (0 == strcmp(argv[1], "-i")) {
		ctx.interactive = true;
		/* This is a hack (or maybe not) to forcibly switch
		   scanner to USAGE 'start condition' in order not
		   to ask user to enter 'Usage:' */
		lex_beginUSAGE();
		printf("Example: tool --version\n");
		printf("> ");
	} else {
		rc = ctx_setupin(&ctx, argv[1]);
		if (rc)
			return -1;
	}

	rc = yyparse(&ctx);
	if (rc == 0 && !ctx.interactive) {
		rc = ctx_validate(&ctx);
		if (rc)
			goto out;
		rc = ctx_setupout(&ctx, argv[1]);
		if (rc)
			goto out;
		ctx_dump(&ctx);
	}
out:
	ctx_free(&ctx);
	yylex_destroy();

	return rc;
}
