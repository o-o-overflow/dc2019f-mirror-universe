/* ccl.l --- lexer for CC
 */

%{
#include <stdio.h>

#include "y.tab.h"

extern int yylval;
%}
%%
" "	return LSP;
\n	return LNL;
[0-9]+	{
		yylval = atoi(yytext);
		return LNUMBER;
	}
"^N"	return LCTRL_N;
"^R"	return LCTRL_R;
"^S"	return LCTRL_S;
"@M"	return LAT_M;
"@A"	return LAT_A;
"FOOBAR" return LFOOBAR;
"/r"	return LSL_R;
"/i"	return LSL_I;
"/b"	return LSL_B;
"/d"	return LSL_D;
.	return yytext[0];
