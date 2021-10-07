//
//  cmdparse.y
//
//
//  (C) 2021 Thibaut VARENE
//  License: GPLv2 - http://www.gnu.org/licenses/gpl-2.0.html
//

%{
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"	// this should pull hal/gpio_types.h and give us gpio_num_t, except it doesn't -> use int

#include "platform.h"

int yylex();
static void yyerror(const char *);
ssize_t sockout(const void *, size_t);
void telnet_echo(bool);

#define SOCKOUTCC(constchar)	sockout(constchar, sizeof(constchar))

void gpio_press(uint32_t gpio, bool longpress)
{
	gpio_set_level(gpio, 1);
	vTaskDelay((longpress ? 5000 : 500) / portTICK_RATE_MS);
	gpio_set_level(gpio, 0);
}

#define PROMPT	"> "

static const char prompt[] = PROMPT;
static const char wrongpass[] = "wrong password!\r\n";
static const char cmdinv[] = "what?\r\n";
static const char cmdok[] = "ok\r\n";
static const char cmdhelp[] = "[<obj>] [<adj>] <verb>\r\n"
			"\t<obj>: ledhdd ledpower power reset\r\n"
			"\t<adj>: long\r\n"
			"\t<verb>: get help press quit\r\n";

%}

%union {
	int gpio;
}

%verbose

%token TOK_O_POWER TOK_O_RESET
%token TOK_I_LEDPOWER TOK_I_LEDHDD
%token TOK_V_PRESS TOK_V_GET TOK_V_HELP TOK_V_QUIT
%token TOK_A_LONG
%token TOK_PASS

%type <gpio> i_obj o_obj;

%%
start: pass cmds ;

pass:
	TOK_PASS '\r'		{ telnet_echo(1); SOCKOUTCC("\r\n" PROMPT); }
	| error '\r'		{ SOCKOUTCC(wrongpass); YYABORT; }
;

cmds:	/* nohting */
	| cmds cmd ';'
	| cmds cmd '\r'		{ SOCKOUTCC(prompt); }
	| error '\r'		{ SOCKOUTCC(cmdinv); SOCKOUTCC(prompt); yyerrok; }	/* stop processing until newline if we have an error */
;

cmd:
	/* nothing */ | i_cmd | o_cmd | s_cmd
;

i_cmd:
	i_obj TOK_V_GET
		{
			char buf[3];
			buf[0] = '0' + !gpio_get_level($1); /* inverted logic */
			buf[1] = '\r'; buf[2] = '\n'; SOCKOUTCC(buf);
		}
;

i_obj:
	TOK_I_LEDPOWER		{ $$=GPIO_INPUT_LEDPOWER; }
	| TOK_I_LEDHDD		{ $$=GPIO_INPUT_LEDHDD; }
;

o_cmd:
	o_obj TOK_V_PRESS	{ gpio_press($1, false); SOCKOUTCC(cmdok); }
	| o_obj TOK_A_LONG TOK_V_PRESS	{ gpio_press($1, true); SOCKOUTCC(cmdok); }
;

o_obj:
	TOK_O_POWER		{ $$=GPIO_OUTPUT_POWER; }
	| TOK_O_RESET		{ $$=GPIO_OUTPUT_RESET; }
;

s_cmd:
	TOK_V_HELP		{ SOCKOUTCC(cmdhelp); }
	| TOK_V_QUIT		{ SOCKOUTCC("bye\r\n"); YYABORT; }
;

%%

static void yyerror(const char * s)
{
}
