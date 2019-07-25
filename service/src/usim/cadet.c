// cadet.c --- Space Cadet (aka new) keyboard translation

#include <stdint.h>
#include <err.h>

#include "lmch.h"
#include "cadet.h"

// See SYS:LMWIN;COLD LISP for details.
//
// The second dimension is 200 long and indexed by keycode.
// The first dimension is the shifts:
//  0 unshifted
//  1 shift
//  2 top
//  3 greek
//  4 shift greek
// Elements in the table are 16-bit unsigned numbers.
// Bit 15 on and bit 14 on means undefined code, ignore and beep.
// Bit 15 on and bit 14 off means low bits are shift for bit in KBD-SHIFTS
//    (40 octal for right-hand key of a pair)
// Bit 15 off is ordinary code.
uint32_t cadet_table[0200][5] = {
	{},				       // 0 not used
	{LMCH_roman_ii},		       // 1 Roman II
	{LMCH_roman_iv},		       // 2 Roman IV
	{100011},			       // 3 Mode lock
	{100006},			       // 5 Left super
	{LMCH_4, LMCH_dollar, LMCH_dollar},    // 11 Four
	{LMCH_r, LMCH_R, LMCH_down_horseshoe}, // 12 R
	{LMCH_f, LMCH_F},		       // 13 F
	{LMCH_v, LMCH_V},		       // 14 V
	{100008},			       // 15 Alt Lock
	{LMCH_hand_right},		       // 17 Hand Right
	{100004},			       // 20 Left control
	{LMCH_colon, LMCH_plus_minus, LMCH_plus_minus}, // 21 plus-minus
	{LMCH_tab},					// 22 tab
	{LMCH_rubout},					// 23 rubout
	{100000},				// 24 Left Shift
	{100040},				// 25 Right Shift
	{100044},				// 26 Right control
	{LMCH_hold_output},			// 30 hold output
	{LMCH_8, LMCH_asterisk, LMCH_asterisk}, // 31 Eight
	{LMCH_i, LMCH_I, LMCH_infinity},	// 32 I
	{LMCH_k, LMCH_K, LMCH_right_arrow},     // 33 K
	{LMCH_comma, LMCH_less, LMCH_less},     // 34 comma
	{100041},				// 35 Right Greek
	{LMCH_line},				// 36 Line
	{LMCH_backslash, LMCH_bar, LMCH_bar},	// 37 Backslash
	{LMCH_esc},				// 40 terminal
	{LMCH_network},				// 42 network
	{100001},				// 44 Left Greek
	{100005},				// 45 Left Meta
	{LMCH_status},				// 46 status
	{LMCH_resume},				// 47 resume
	{LMCH_form},				// 50 clear screen
	{LMCH_6, LMCH_asciicircum, LMCH_asciicircum}, // 51 Six
	{LMCH_y, LMCH_Y, LMCH_right_horseshoe},	      // 52 Y
	{LMCH_h, LMCH_H, LMCH_down_arrow},	      // 53 H
	{LMCH_n, LMCH_N, LMCH_less_or_equal},	      // 54 N
	{LMCH_2, LMCH_at, LMCH_at},		      // 61 Two
	{LMCH_w, LMCH_W, LMCH_or_sign},		      // 62 W
	{LMCH_s, LMCH_S},			      // 63 S
	{LMCH_x, LMCH_X},			      // 64 X
	{100046},				  // 65 Right Super
	{LMCH_abort},				  // 67 Abort
	{LMCH_9, LMCH_parenleft, LMCH_parenleft}, // 71 Nine
	{LMCH_o, LMCH_O, LMCH_existential_quantifier},	   // 72 O
	{LMCH_l, LMCH_L, LMCH_partial_delta, LMCH_lambda}, // 73 L/lambda
	{LMCH_period, LMCH_greater, LMCH_greater}, // 74 period
	{LMCH_grave, LMCH_asciitilde, LMCH_asciitilde, LMCH_not_sign}, // 77 back quote
	{LMCH_back_next},		       // 100 macro
	{LMCH_roman_i},			       // 101 Roman I
	{LMCH_roman_iii},		       // 102 Roman III
	{100002},			       // 104 Left Top
	{LMCH_hand_up},			       // 106 Up Thumb
	{LMCH_call},			       // 107 Call
	{LMCH_clear},			       // 110 Clear Input
	{LMCH_5, LMCH_percent, LMCH_percent},  // 111 Five
	{LMCH_t, LMCH_T, LMCH_left_horseshoe}, // 112 T
	{LMCH_g, LMCH_G, LMCH_up_arrow, LMCH_gamma},   // 113 G/gamma
	{LMCH_b, LMCH_B, LMCH_equivalence, LMCH_beta}, // 114 B
	{},					       // 115 Repeat
	{LMCH_help},				       // 116 Help
	{LMCH_hand_left, LMCH_hand_left, LMCH_hand_left, LMCH_circle_x, LMCH_circle_x}, // 117 Hand Left
	{LMCH_quote},			      // 120 Quote
	{LMCH_1, LMCH_exclam, LMCH_exclam},   // 121 One
	{LMCH_q, LMCH_Q, LMCH_and_sign},      // 122 Q
	{LMCH_a, LMCH_A, 140000, LMCH_alpha}, // 123 A
	{LMCH_z, LMCH_Z},		      // 124 Z
	{100003},			      // 125 Caps Lock
	{LMCH_equal, LMCH_plus, LMCH_plus},   // 126 Equals
	{LMCH_minus, LMCH_underscore, LMCH_underscore}, // 131 Minus
	{LMCH_parenleft, LMCH_bracketleft, LMCH_bracketleft}, // 132 Open parenthesis
	{LMCH_apostrophe, LMCH_quotedbl, LMCH_quotedbl, LMCH_center_dot}, // 133 Apostrophe/center-dot
	{LMCH_sp},		// 134 Space
	{LMCH_cr},		// 136 Return
	{LMCH_parenright, LMCH_bracketright, LMCH_bracketright}, // 137 Close parenthesis
	{LMCH_system},				     // 141 system
	{LMCH_altmode},				     // 143 Alt Mode
	{100007},				     // 145 Left Hyper
	{LMCH_braceright, 140000, 140000},	     // 146 }
	{LMCH_7, LMCH_ampersand, LMCH_ampersand},    // 151 Seven
	{LMCH_u, LMCH_U, LMCH_universal_quantifier}, // 152 U
	{LMCH_j, LMCH_J, LMCH_left_arrow},	     // 153 J
	{LMCH_m, LMCH_M, LMCH_greater_or_equal},     // 154 M
	{100042},				     // 155 Right Top
	{LMCH_end},				     // 156 End
	{LMCH_delete},				     // 157 Delete
	{LMCH_bs},				     // 160 Overstrike
	{LMCH_3, LMCH_numbersign, LMCH_numbersign},  // 161 Three
	{LMCH_e, LMCH_E, LMCH_up_horseshoe, LMCH_epsilon}, // 162 E
	{LMCH_d, LMCH_D, 140000, LMCH_delta},	    // 163 D/delta
	{LMCH_c, LMCH_C, LMCH_not_equal},	    // 164 C
	{100045},				    // 165 Right Meta
	{LMCH_braceleft, 140000, 140000},	    // 166 {
	{LMCH_break},				    // 167 Break
	{LMCH_stop_output},			    // 170 Stop Output
	{LMCH_0, LMCH_parenright, LMCH_parenright}, // 171 Zero
	{LMCH_p, LMCH_P, LMCH_partial_delta, LMCH_pi}, // 172 P
	{LMCH_semicolon, LMCH_colon, LMCH_colon}, //  173 Semicolon
	{LMCH_slash, LMCH_question, LMCH_question, LMCH_integral}, // 174 Question/Integral
	{100047},		// 175 Right Hyper
	{LMCH_hand_down, LMCH_hand_down, LMCH_hand_down, /* #/^?^M, #/^?^M */}, // 176 Down Thumb
};

void
cadet_init(void)
{
	errx(1, "space cadet keyboard not implemented yet");
}
