/*
//  cmdparse.l
//
//
//  (C) 2021 Thibaut VARENE
//  License: GPLv2 - http://www.gnu.org/licenses/gpl-2.0.html
*/

/* noyywrap disables automatic rewinding for the next file to parse. Since we
 always parse only a single string, there's no need to do any wraps. And
 using yywrap requires linking with -lfl, which provides the default yywrap
 implementation that always returns 1 anyway. */
%option noyywrap

/* nounput simplifies the lexer, by removing support for putting a character
 back into the input stream. We never use such capability anyway. */
%option nounput

/* we never directly read input */
%option noinput

/* we don't need the default rule */
%option nodefault

/* nounistd suppresses inclusion of the non-ANSI header file unistd.h.
 This option is meant to target environments in which unistd.h does not exist.
 Be aware that certain options may cause flex to generate code that relies on
 functions normally found in unistd.h, (e.g. isatty(), read().)
 If you wish to use these functions, you will have to inform your compiler where
 to find them. See option-always-interactive. See option-read. */
%option nounistd

/* batch means that we'll never use the generated lexer interactively. */
%option batch

/* Enables debug mode. To see the debug messages, one needs to also set
 yy_flex_debug to 1, then the debug messages will be printed on stderr. */
%option nodebug

/* https://datatracker.ietf.org/doc/html/rfc854 */
TELNETWILL	\xff\xfb
TELNETWONT	\xff\xfc
TELNETDO	\xff\xfd
TELNETDONT	\xff\xfe

TELNETOPTNEG	\xff\xfa.+\xff\xf0
TELNETNOOPT	\xff[\xf1-\xf9]

%{
	#include "cmdparse.tab.h"
	ssize_t sockin(void *, size_t);
	void yytelnet(char *);
	
	#define YY_INPUT(buf, result, max_size)	\
	{ \
		ssize_t len = sockin(buf, max_size); \
		result = (len > 0) ? len : YY_NULL; \
	}
%}

%%
	/* password */
admin		{ return TOK_PASS; }

	/* outputs */
power		{ return TOK_O_POWER; }
reset		{ return TOK_O_RESET; }

	/* inputs */
ledpower	{ return TOK_I_LEDPOWER; }
ledhdd		{ return TOK_I_LEDHDD; }

	/* verbs */
press		{ return TOK_V_PRESS; }
get		{ return TOK_V_GET; }
help		{ return TOK_V_HELP; }
exit		|
quit		{ return TOK_V_QUIT; }

	/* adjectives */
long		{ return TOK_A_LONG; }

[ \t\n]+	/* ignore whitespace */;


	/* telnet control - heaven forbid, handle this from lexer */
{TELNETWILL}.	|
{TELNETWONT}.	|
{TELNETDO}.	|
{TELNETDONT}.	{ yytelnet(yytext); }
{TELNETOPTNEG}+	|
{TELNETNOOPT}+		/* ignore telnet commands */;

.		{ return *yytext; }

<<EOF>>		{ yyterminate(); }
%%
