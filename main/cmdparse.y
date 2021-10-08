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
#include "driver/uart.h"

#include "platform.h"

int yylex();
static void yyerror(const char *);
ssize_t sockout(const void *, size_t);
void telnet_echo(bool);
int nvssetbr(uint32_t);
int nvssave(void);

#define SOCKOUTCC(constchar)	sockout(constchar, sizeof(constchar))

static void gpio_press(uint32_t gpio, bool longpress)
{
	gpio_set_level(gpio, 1);
	vTaskDelay((longpress ? 5000 : 500) / portTICK_RATE_MS);
	gpio_set_level(gpio, 0);
}

/**
 * Reverse-fill a buffer with ascii representation of uint and CR/LF.
 * @param buf target buffer
 * @param len target buffer size
 * @param n number to convert
 * @return offset for start of number in buffer
 * @warning sizeof(buf) must be >= 2
 */
static unsigned int uint2buf(char *buf, size_t len, unsigned int n)
{
	buf[--len] = '\n';
	buf[--len] = '\r';

	for (; len && n; n /= 10)
		buf[--len] = '0' + (n % 10);

	return len;
}


#define PROMPT	"> "

static const char prompt[] = PROMPT;
static const char wrongpass[] = "wrong password!\r\n";
static const char cmdinv[] = "what?\r\n";
static const char cmdok[] = "ok\r\n";
static const char cmderr[] = "error!\r\n";
static const char cmdhelp[] = "[<obj>] [<adj>] <verb>\r\n"
			"\t<obj>: baudrate ledhdd ledpower power reset\r\n"
			"\t<adj>: long <baudrateval>\r\n"
			"\t<verb>: console get help press quit save set\r\n";

%}

%union {
	int gpio;
	uint32_t uval;
}

%verbose

%token TOK_O_POWER TOK_O_RESET
%token TOK_I_LEDPOWER TOK_I_LEDHDD
%token TOK_V_PRESS TOK_V_GET TOK_V_SET TOK_V_HELP TOK_V_CONSOLE TOK_V_QUIT TOK_V_SAVE
%token TOK_A_LONG
%token TOK_PASS TOK_BAUDRATE
%token <uval> TOK_UVAL

%type <gpio> i_obj o_obj;

%%
start: pass cmds ;

pass:
	TOK_PASS '\r'		{ telnet_echo(1); SOCKOUTCC("\r\n" PROMPT); }
	| error '\r'		{ SOCKOUTCC(wrongpass); YYACCEPT; }
;

cmds:	/* nohting */
	| cmds '\004'		{ /* Ctrl-D */ YYACCEPT; }
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
	| TOK_BAUDRATE TOK_UVAL TOK_V_SET
		{
			uart_set_baudrate(SERIAL_PORT, $2) ? SOCKOUTCC(cmderr) : SOCKOUTCC(cmdok);
		}
	| TOK_BAUDRATE TOK_V_GET
		{
			uint32_t br; char buf[12];	// 10 digits (uint32_max) + '\r\n'
			uart_get_baudrate(SERIAL_PORT, &br);
			br = uint2buf(buf, sizeof(buf), br);
			sockout(buf+br, sizeof(buf)-br);
		}
	| TOK_BAUDRATE TOK_V_SAVE
		{
			uint32_t br;
			uart_get_baudrate(SERIAL_PORT, &br);
			(!nvssetbr(br) && !nvssave()) ? SOCKOUTCC(cmdok) : SOCKOUTCC(cmderr);
		}
	| TOK_V_CONSOLE		{ YYABORT; }
	| TOK_V_QUIT		{ SOCKOUTCC("bye\r\n"); YYACCEPT; }
;

%%

static void yyerror(const char * s)
{
}
