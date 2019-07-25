// ccy.y --- grammar for CC

%{
#include <stdio.h>

#include "cc.h"

extern int yylex(void);
extern int yyerror(const char *);
%}

%token LNL LSP
%token LNUMBER
%token LCTRL_N LCTRL_R LCTRL_S
%token LAT_M LAT_A
%token LFOOBAR
%token LSL_R  LSL_I  LSL_B  LSL_D

%start input

%%                              // Rules section.

input:
  /* empty */
| input line
;

line:
  LNL		{ cmd_prompt(); }
| exp LNL	{ cmd_prompt(); }
| oexp LNL	{ cmd_prompt(); }
| error LNL	{ cmd_prompt(); yyerrok; }
;

exp:
	  LCTRL_R	{ cmd_reset(); }
| LNUMBER LSP LFOOBAR	{ cmd_start($1); }
|         LCTRL_S	{ cmd_stop(); }
|         LCTRL_N	{ cmd_step_once(); }
| LNUMBER LCTRL_N	{ if ($1 < 40000) cmd_step_until($1); else cmd_step_until_adr($1); }
| LNUMBER LAT_M		{ cmd_read_m_mem($1); }
| LNUMBER LAT_A		{ cmd_read_a_mem($1); }
;

oexp:
  'p'	{ oldcmd('p'); }
| 'q'	{ oldcmd('q'); }
| 'c'	{ oldcmd('c'); }
| 'S'	{ oldcmd('S'); }
| 'r'	{ oldcmd('r'); }
| 'I'	{ oldcmd('I'); }
| 'R'	{ oldcmd('R'); }
| 'n'	{ oldcmd('n'); }
| 'i'	{ oldcmd('i'); }
| 'v'	{ oldcmd('v'); }
| 'd'	{ oldcmd('d'); }
| 'm'	{ oldcmd('m'); }
| 'G'	{ oldcmd('G'); }
| 'a'	{ oldcmd('a'); }
| 't'	{ oldcmd('t'); }
| LSL_R	{ oldcmd('1'); }
| LSL_I	{ oldcmd('2'); }
| LSL_B	{ oldcmd('3'); }
| LSL_D	{ oldcmd('4'); }
%%

int
yyerror(const char *s)
{
	fprintf(stderr, "%s\n", s);
	return 1;
}

int
yywrap(void)
{
	return 1;
}
