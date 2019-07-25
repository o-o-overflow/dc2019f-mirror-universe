#ifndef USIM_KNIGHT_H
#define USIM_KNIGHT_H

extern unsigned short knight_translate_table[3][256];

extern void knight_init(void);

#define KNIGHT_VANILLA		0	// VANILLA
#define KNIGHT_SHIFT		0300	// SHIFT BITS
#define KNIGHT_TOP		01400	// TOP BITS
#define KNIGHT_CONTROL		06000	// CONTROL BITS
#define KNIGHT_META		030000	// META BITS
#define KNIGHT_SHIFT_LOCK	040000	// SHIFT LOCK

#define KNIGHT_break		00
#define KNIGHT_escape		01
#define KNIGHT_1		02
#define KNIGHT_2		03
#define KNIGHT_3		04
#define KNIGHT_4		05
#define KNIGHT_5		06
#define KNIGHT_6		07
#define KNIGHT_7		010
#define KNIGHT_8		011
#define KNIGHT_9		012
#define KNIGHT_0		013
#define KNIGHT_minus		014
#define KNIGHT_at		015
#define KNIGHT_asciicircum	016
#define KNIGHT_bs		017
#define KNIGHT_call		020

#define KNIGHT_clear		021
#define KNIGHT_tab		022
#define KNIGHT_altmode		023
#define KNIGHT_q		024
#define KNIGHT_w		025
#define KNIGHT_e		026
#define KNIGHT_r		027
#define KNIGHT_t		030
#define KNIGHT_y		031
#define KNIGHT_u		032
#define KNIGHT_i		033
#define KNIGHT_o		034
#define KNIGHT_p		035
#define KNIGHT_bracketleft	036
#define KNIGHT_bracketright	037
#define KNIGHT_backslash	040
#define KNIGHT_slash		041
#define KNIGHT_plus_minus	042
#define KNIGHT_circle_plus	043

#define KNIGHT_form		044
#define KNIGHT_vt		045
#define KNIGHT_rubout		046
#define KNIGHT_a		047
#define KNIGHT_s		050
#define KNIGHT_d		051
#define KNIGHT_f		052
#define KNIGHT_g		053
#define KNIGHT_h		054
#define KNIGHT_j		055
#define KNIGHT_k		056
#define KNIGHT_l		057
#define KNIGHT_semicolon	060
#define KNIGHT_colon		061
#define KNIGHT_cr		062
#define KNIGHT_line		063
#define KNIGHT_back_next	064

#define KNIGHT_z		065
#define KNIGHT_x		066
#define KNIGHT_c		067
#define KNIGHT_v		070
#define KNIGHT_b		071
#define KNIGHT_n		072
#define KNIGHT_m		073
#define KNIGHT_comma		074
#define KNIGHT_period		075
#define KNIGHT_slash1		076

#define KNIGHT_sp		077

// Shift

#define KNIGHT_exclam		(KNIGHT_1 | KNIGHT_SHIFT)
#define KNIGHT_quotedbl		(KNIGHT_2 | KNIGHT_SHIFT)
#define KNIGHT_numbersign	(KNIGHT_3 | KNIGHT_SHIFT)
#define KNIGHT_dollar		(KNIGHT_4 | KNIGHT_SHIFT)
#define KNIGHT_percent		(KNIGHT_5 | KNIGHT_SHIFT)
#define KNIGHT_ampersand	(KNIGHT_6 | KNIGHT_SHIFT)
#define KNIGHT_apostrophe	(KNIGHT_7 | KNIGHT_SHIFT)
#define KNIGHT_parenleft	(KNIGHT_8 | KNIGHT_SHIFT)
#define KNIGHT_parenright	(KNIGHT_9 | KNIGHT_SHIFT)
#define KNIGHT_underscore	(KNIGHT_0 | KNIGHT_SHIFT)
#define KNIGHT_equal		(KNIGHT_minus | KNIGHT_SHIFT)
#define KNIGHT_grave		(KNIGHT_at | KNIGHT_SHIFT)
#define KNIGHT_asciitilde	(KNIGHT_asciicircum | KNIGHT_SHIFT)

#define KNIGHT_Q		(KNIGHT_q | KNIGHT_SHIFT)
#define KNIGHT_W		(KNIGHT_w | KNIGHT_SHIFT)
#define KNIGHT_E		(KNIGHT_e | KNIGHT_SHIFT)
#define KNIGHT_R		(KNIGHT_r | KNIGHT_SHIFT)
#define KNIGHT_T		(KNIGHT_t | KNIGHT_SHIFT)
#define KNIGHT_Y		(KNIGHT_y | KNIGHT_SHIFT)
#define KNIGHT_U		(KNIGHT_u | KNIGHT_SHIFT)
#define KNIGHT_I		(KNIGHT_i | KNIGHT_SHIFT)
#define KNIGHT_O		(KNIGHT_o | KNIGHT_SHIFT)
#define KNIGHT_P		(KNIGHT_p | KNIGHT_SHIFT)
#define KNIGHT_braceleft	(KNIGHT_bracketleft | KNIGHT_SHIFT)
#define KNIGHT_braceright	(KNIGHT_bracketright | KNIGHT_SHIFT)
#define KNIGHT_bar		(KNIGHT_backslash | KNIGHT_SHIFT)
#define KNIGHT_infinity		(KNIGHT_slash | KNIGHT_SHIFT)
#define KNIGHT_delta		(KNIGHT_plus_minus | KNIGHT_SHIFT)

#define KNIGHT_A		(KNIGHT_a | KNIGHT_SHIFT)
#define KNIGHT_S		(KNIGHT_s | KNIGHT_SHIFT)
#define KNIGHT_D		(KNIGHT_d | KNIGHT_SHIFT)
#define KNIGHT_F		(KNIGHT_f | KNIGHT_SHIFT)
#define KNIGHT_G		(KNIGHT_g | KNIGHT_SHIFT)
#define KNIGHT_H		(KNIGHT_h | KNIGHT_SHIFT)
#define KNIGHT_J		(KNIGHT_j | KNIGHT_SHIFT)
#define KNIGHT_K		(KNIGHT_k | KNIGHT_SHIFT)
#define KNIGHT_L		(KNIGHT_l | KNIGHT_SHIFT)
#define KNIGHT_plus		(KNIGHT_semicolon | KNIGHT_SHIFT)
#define KNIGHT_asterisk		(KNIGHT_colon | KNIGHT_SHIFT)

#define KNIGHT_Z		(KNIGHT_z | KNIGHT_SHIFT)
#define KNIGHT_X		(KNIGHT_x | KNIGHT_SHIFT)
#define KNIGHT_C		(KNIGHT_c | KNIGHT_SHIFT)
#define KNIGHT_V		(KNIGHT_v | KNIGHT_SHIFT)
#define KNIGHT_B		(KNIGHT_b | KNIGHT_SHIFT)
#define KNIGHT_N		(KNIGHT_n | KNIGHT_SHIFT)
#define KNIGHT_M		(KNIGHT_m | KNIGHT_SHIFT)
#define KNIGHT_less		(KNIGHT_comma | KNIGHT_SHIFT)
#define KNIGHT_greater		(KNIGHT_period | KNIGHT_SHIFT)
#define KNIGHT_question		(KNIGHT_slash1 | KNIGHT_SHIFT)

// Top

#define KNIGHT_network		(KNIGHT_break | KNIGHT_TOP)
#define KNIGHT_system		(KNIGHT_escape | KNIGHT_TOP)
#define KNIGHT_abort		(KNIGHT_call | KNIGHT_TOP)
#define KNIGHT_and_sign		(KNIGHT_q | KNIGHT_TOP)
#define KNIGHT_or_sign		(KNIGHT_w | KNIGHT_TOP)
#define KNIGHT_up_horseshoe	(KNIGHT_e | KNIGHT_TOP)
#define KNIGHT_down_horseshoe	(KNIGHT_r | KNIGHT_TOP)
#define KNIGHT_left_horseshoe	(KNIGHT_t | KNIGHT_TOP)
#define KNIGHT_right_horseshoe	(KNIGHT_y | KNIGHT_TOP)
#define KNIGHT_not_sign		(KNIGHT_u | KNIGHT_TOP)
#define KNIGHT_circle_x		(KNIGHT_i | KNIGHT_TOP)
#define KNIGHT_down_arrow	(KNIGHT_o | KNIGHT_TOP)
#define KNIGHT_up_arrow		(KNIGHT_p | KNIGHT_TOP)
#define KNIGHT_less_or_equal	(KNIGHT_a | KNIGHT_TOP)
#define KNIGHT_greater_or_equal	(KNIGHT_s | KNIGHT_TOP)
#define KNIGHT_equivalence	(KNIGHT_d | KNIGHT_TOP)
#define KNIGHT_partial_delta	(KNIGHT_f | KNIGHT_TOP)
#define KNIGHT_not_equal	(KNIGHT_g | KNIGHT_TOP)
#define KNIGHT_help		(KNIGHT_h | KNIGHT_TOP)
#define KNIGHT_left_arrow	(KNIGHT_j | KNIGHT_TOP)
#define KNIGHT_right_arrow	(KNIGHT_k | KNIGHT_TOP)
#define KNIGHT_double_arrow	(KNIGHT_l | KNIGHT_TOP)
#define KNIGHT_end		(KNIGHT_cr | KNIGHT_TOP)
#define KNIGHT_alpha		(KNIGHT_z | KNIGHT_TOP)
#define KNIGHT_beta		(KNIGHT_x | KNIGHT_TOP)
#define KNIGHT_epsilon		(KNIGHT_c | KNIGHT_TOP)
#define KNIGHT_lambda		(KNIGHT_v | KNIGHT_TOP)
#define KNIGHT_pi		(KNIGHT_b | KNIGHT_TOP)
#define KNIGHT_universal_quantifier (KNIGHT_n | KNIGHT_TOP)
#define KNIGHT_existential_quantifier (KNIGHT_m | KNIGHT_TOP)

#endif
