// knight.c --- Knight (aka old) keyboard translation

#include <string.h>

#include "lmch.h"
#include "knight.h"

// ASCII to Knight keyboard scancode translation table.
unsigned short knight_translate_table[3][256];

// See SYS:LMWIN;COLD LISP for details.
//
// Keyboard translate table is a 3 X 64 array.
// 3 entries for each of 100 keys.  First is vanilla, second shift, third top.
static unsigned char knight_old_table[64][3] = {
	{LMCH_break, LMCH_break, LMCH_network},
	{LMCH_esc, LMCH_esc, LMCH_system},
	{LMCH_1, LMCH_exclam, LMCH_exclam},
	{LMCH_2, LMCH_quotedbl, LMCH_quotedbl},
	{LMCH_3, LMCH_numbersign, LMCH_numbersign},
	{LMCH_4, LMCH_dollar, LMCH_dollar},
	{LMCH_5, LMCH_percent, LMCH_percent},
	{LMCH_6, LMCH_ampersand, LMCH_ampersand},
	{LMCH_7, LMCH_apostrophe, LMCH_apostrophe},
	{LMCH_8, LMCH_parenleft, LMCH_parenleft},
	{LMCH_9, LMCH_parenright, LMCH_parenright},
	{LMCH_0, LMCH_underscore, LMCH_underscore},
	{LMCH_minus, LMCH_equal, LMCH_equal},
	{LMCH_at, LMCH_grave, LMCH_grave},
	{LMCH_asciicircum, LMCH_asciitilde, LMCH_asciitilde},
	{LMCH_bs, LMCH_bs, LMCH_bs},
	{LMCH_call, LMCH_call, LMCH_abort},

	{LMCH_clear, LMCH_clear, LMCH_clear},
	{LMCH_tab, LMCH_tab, LMCH_tab},
	{LMCH_altmode, LMCH_altmode, LMCH_altmode},
	{LMCH_q, LMCH_Q, LMCH_and_sign},
	{LMCH_w, LMCH_W, LMCH_or_sign},
	{LMCH_e, LMCH_E, LMCH_up_horseshoe},
	{LMCH_r, LMCH_R, LMCH_down_horseshoe},
	{LMCH_t, LMCH_T, LMCH_left_horseshoe},
	{LMCH_y, LMCH_Y, LMCH_right_horseshoe},
	{LMCH_u, LMCH_U, LMCH_not_sign},
	{LMCH_i, LMCH_I, LMCH_circle_x},
	{LMCH_o, LMCH_O, LMCH_down_arrow},
	{LMCH_p, LMCH_P, LMCH_up_arrow},
	{LMCH_bracketleft, LMCH_braceleft, LMCH_braceleft},
	{LMCH_bracketright, LMCH_braceright, LMCH_braceright},
	{LMCH_backslash, LMCH_bar, LMCH_bar},
	{LMCH_slash, LMCH_infinity, LMCH_infinity},
	{LMCH_plus_minus, LMCH_delta, LMCH_delta},
	{LMCH_circle_plus, LMCH_circle_plus, LMCH_circle_plus},

	{LMCH_form, LMCH_form, LMCH_form},
	{LMCH_vt, LMCH_vt, LMCH_vt},
	{LMCH_rubout, LMCH_rubout, LMCH_rubout},
	{LMCH_a, LMCH_A, LMCH_less_or_equal},
	{LMCH_s, LMCH_S, LMCH_greater_or_equal},
	{LMCH_d, LMCH_D, LMCH_equivalence},
	{LMCH_f, LMCH_F, LMCH_partial_delta},
	{LMCH_g, LMCH_G, LMCH_not_equal},
	{LMCH_h, LMCH_H, LMCH_help},
	{LMCH_j, LMCH_J, LMCH_left_arrow},
	{LMCH_k, LMCH_K, LMCH_right_arrow},
	{LMCH_l, LMCH_L, LMCH_double_arrow},
	{LMCH_semicolon, LMCH_plus, LMCH_plus},
	{LMCH_colon, LMCH_asterisk, LMCH_asterisk},
	{LMCH_cr, LMCH_cr, LMCH_end},
	{LMCH_line, LMCH_line, LMCH_line},
	{LMCH_back_next, LMCH_back_next, LMCH_back_next},

	// SHIFT-LOCK
	// LEFT-TOP
	// LEFT-SHIFT
	{LMCH_z, LMCH_Z, LMCH_alpha},
	{LMCH_x, LMCH_X, LMCH_beta},
	{LMCH_c, LMCH_C, LMCH_epsilon},
	{LMCH_v, LMCH_V, LMCH_lambda},
	{LMCH_b, LMCH_B, LMCH_pi},
	{LMCH_n, LMCH_N, LMCH_universal_quantifier},
	{LMCH_m, LMCH_M, LMCH_existential_quantifier},
	{LMCH_comma, LMCH_less, LMCH_less},
	{LMCH_period, LMCH_greater, LMCH_greater},
	{LMCH_slash, LMCH_question, LMCH_question},
	// RIGHT-SHIFT
	// RIGHT-TOP

	// LEFT-META
	// LEFT-CTRL
	{LMCH_sp, LMCH_sp, LMCH_sp},
	// RIGHT-CTRL
	// RIGHT-META
};

void
knight_init(void)
{
	memset((char *) knight_translate_table, 0, sizeof(knight_translate_table));

	for (int i = 0; i < 64; i++) {
		unsigned char k;

		k = knight_old_table[i][0];
		knight_translate_table[0][(int) k] = i;
	}

	for (int i = 0; i < 255; i++) {
		knight_translate_table[1][i] = knight_translate_table[0][i] | (3 << 6);
		knight_translate_table[2][i] = knight_translate_table[0][i] | (3 << 8);
	}
}
