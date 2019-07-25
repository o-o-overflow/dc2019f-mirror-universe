// disass.c --- macrocode/microcode disassembler routines

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>

#include "usim.h"
#include "ucode.h"
#include "disass.h"
#include "syms.h"
#include "misc.h"

#define SYS78 1

#if SYS78
#include "defmic78.h"
#else
#include "defmic99.h"
#endif

// This micro-assembler disassembler is based on SYS: CC; CADLD LISP.

#define UBUFSZ 128
static char uinstbuf[UBUFSZ];
static void *uinstbufp;

#define PRIN1(args...) do { uinstbufp += sprintf(uinstbufp, args); } while (0)
#define PRIN1SP(args...) do { PRIN1(args); PRIN1(" "); } while (0)

static symtab_t uinstsymtab;

static void
byte_field_out(uint64_t u, int val, bool always_reflect_mrot, bool length_is_minus_one)
{
	int tem;

	PRIN1("(Byte-field ");

	if (length_is_minus_one == true)
		PRIN1SP("%lo", load_byte(val, 005, 005) + 1);
	else
		PRIN1SP("%lo", load_byte(val, 005, 005));

	tem = load_byte(val, 000, 005);
	if (tem == 0) {
	} else {
		if (always_reflect_mrot || load_byte(u, 014, 002) == 1)
			tem = 32 - tem;
	}
	PRIN1("%o", tem);

	PRIN1(") ");
}

static void
c_or_d_adr_out(symtype_t type, int val)
{
	char *lbl;

	lbl = sym_find_by_type_val(&uinstsymtab, type, val, NULL);
	if (lbl)
		PRIN1SP("%s", lbl);
	else
		PRIN1SP("%o", val);
}

static void
a_or_m_adr_out(symtype_t type, int val)
{
	char *lbl;

	if (val == 0)
		return;

	lbl = sym_find_by_type_val(&uinstsymtab, type, val, NULL);
	if (lbl)
		PRIN1("%s", lbl);
	else
		PRIN1("%o@%c", val, type == AMEM ? 'A' : 'M');

	PRIN1(" ");
}

static void
type_field(symtype_t type, uint64_t u, int pp, int ss)
{
	int val;

	val = load_byte(u, pp, ss);

	if (type == IMEM || type == DMEM)
		c_or_d_adr_out(type, val);
	else if (type == AMEM || type == MMEM)
		a_or_m_adr_out(type, val);
	else
		PRIN1SP("%o", val);
}

static void
m_source_desc(uint64_t u)
{
	int m;
	int fsource;

	m = load_byte(u, 037, 001);
	switch (m) {
	case 0:
		type_field(MMEM, u, 032, 006);
		break;
	case 1:
		fsource = load_byte(u, 032, 005);
		switch (fsource) {
		case 0: PRIN1SP("READ-I-ARG"); break;
		case 1: PRIN1SP("MICRO-STACK-PNTR-AND-DATA"); break;
		case 2: PRIN1SP("PDL-BUFFER-POINTER"); break;
		case 3: PRIN1SP("PDL-BUFFER-INDEX"); break;
		case 4: PRIN1SP("FSOURCE-%o", fsource); break;
		case 5: PRIN1SP("C-PDL-BUFFER-INDEX"); break;
		case 6: PRIN1SP("C-OPC-BUFFER"); break;
		case 7: PRIN1SP("Q-R"); break;
		case 8: PRIN1SP("VMA"); break;
		case 9: PRIN1SP("MEMORY-MAP-DATA"); break;
		case 10: PRIN1SP("MD"); break;
		case 11: PRIN1SP("LOCATION-COUNTER"); break;
		case 12: PRIN1SP("MICRO-STACK-PNTR-AND-DATA-POP"); break;
		case 13 ... 19: PRIN1SP("FSOURCE-%o", fsource); break;
		case 20: PRIN1SP("C-PDL-BUFFER-POINTER-POP"); break;
		case 21: PRIN1SP("C-PDL-BUFFER-POINTER"); break;
		case 22: PRIN1SP("FSOURCE-%o", fsource); break;
		case 23 ... 31: PRIN1SP("FSOURCE-%o", fsource); break;
		}
	}
}

static void
q_dest_desc(uint64_t u)
{
	int alu;
	int dest;

	alu = load_byte(u, 053, 002);
	dest = load_byte(u, 000, 002);
	if (alu == 0 && dest == 3)
		PRIN1(" (Q-R) ");
}

static void
a_dest_desc(uint64_t u)
{
	type_field(AMEM, u, 016, 012);
}

static void
m_dest_desc(uint64_t u)
{
	int fdest;

	type_field(MMEM, u, 016, 005);

	fdest = load_byte(u, 023, 005);
	switch (fdest) {
	case 0: break;
	case 1: PRIN1SP("LOCATION-COUNTER"); break;
	case 2: PRIN1SP("INTERRUPT-CONTROL"); break;
	case 3 ... 7: PRIN1SP("FDEST-%o", fdest); break;
	case 8: PRIN1SP("C-PDL-BUFFER-POINTER"); break;
	case 9: PRIN1SP("C-PDL-BUFFER-POINTER-PUSH"); break;
	case 10: PRIN1SP("C-PDL-BUFFER-INDEX"); break;
	case 11: PRIN1SP("PDL-BUFFER-INDEX"); break;
	case 12: PRIN1SP("PDL-BUFFER-POINTER"); break;
	case 13: PRIN1SP("MICRO-STACK-DATA-PUSH"); break;
	case 14: PRIN1SP("OA-REG-LOW"); break;
	case 15: PRIN1SP("OA-REG-HI"); break;
	case 16: PRIN1SP("VMA"); break;
	case 17: PRIN1SP("VMA-START-READ"); break;
	case 18: PRIN1SP("VMA-START-WRITE"); break;
	case 19: PRIN1SP("VMA-WRITE-MAP"); break;
	case 20 ... 23: PRIN1SP("FDEST-%o", fdest); break;
	case 24: PRIN1SP("MD"); break;
	case 25: PRIN1SP("FDEST-%o", fdest); break;
	case 26: PRIN1SP("MD-START-WRITE"); break;
	case 27: PRIN1SP("MD-WRITE-MAP"); break;
	case 28 ... 31: PRIN1SP("FDEST-%o", fdest); break;
	}
}

static void
dest_desc_1(uint64_t u)
{
	int dest;
	int alu;

	PRIN1(" (");

	dest = load_byte(u, 031, 001);
	switch (dest) {
	case 0: m_dest_desc(u); break;
	case 1: a_dest_desc(u); break;
	}

	alu = load_byte(u, 053, 002);
	dest = load_byte(u, 000, 002);
	if (alu == 0 && dest == 3)
		PRIN1("Q-R");

	PRIN1(") ");
}

static void
dest_desc(uint64_t u)
{
	int dest;

	dest = load_byte(u, 016, 013);
	if (dest == 0)
		q_dest_desc(u);
	else
		dest_desc_1(u);
}

static void
sub_carry_desc(uint64_t u)
{
	int carry;

	carry = load_byte(u, 002, 001);
	if (carry == 0)
		PRIN1SP("ALU-CARRY-IN-ZERO");
}

static void
normal_carry_desc(uint64_t u)
{
	int carry;

	carry = load_byte(u, 002, 001);
	if (carry == 1)
		PRIN1SP("ALU-CARRY-IN-ONE");
}

static void
alu_desc(uint64_t u)
{
	int alu_function;
	int output_selector;
	int mf;
	int q;
	int alu;
	int ilong;

	dest_desc(u);

	alu_function = load_byte(u, 003, 006);
	switch(alu_function) {
	case 0: PRIN1SP("SETZ"); break;
	case 1: PRIN1SP("AND"); break;
	case 2: PRIN1SP("ANDCA"); break;
	case 3: PRIN1SP("SETM"); break;
	case 4: PRIN1SP("ANDCM"); break;
	case 5: /* SETA */ break;
	case 6: PRIN1SP("XOR"); break;
	case 7: PRIN1SP("IOR"); break;
	case 8: PRIN1SP("ANDCB"); break;
	case 9: PRIN1SP("EQV"); break;
	case 10: PRIN1SP("SETCA"); break;
	case 11: PRIN1SP("ORCA"); break;
	case 12: PRIN1SP("SETCM"); break;
	case 13: PRIN1SP("ORCM"); break;
	case 14: PRIN1SP("ORCB"); break;
	case 15: PRIN1SP("SETO"); break;
	case 16 ... 21: PRIN1SP("ALU-FUNCTION-%o", alu_function); break;
	case 22: PRIN1SP("SUB"); break;
	case 23 ... 24: PRIN1SP("ALU-FUNCTION-%o", alu_function); break;
	case 25: PRIN1SP("ADD"); break;
	case 26 ... 27: PRIN1SP("ALU-FUNCTION-%o", alu_function); break;
	case 28: PRIN1SP("INCM"); break;
	case 29 ... 30: PRIN1SP("ALU-FUNCTION-%o", alu_function); break;
	case 31: PRIN1SP("LSHM"); break;
	case 32: PRIN1SP("MUL"); break;
	case 33: PRIN1SP("DIV"); break;
	case 34 ... 36: PRIN1SP("ALU-FUNCTION-%o", alu_function); break;
	case 37: PRIN1SP("DIVRC"); break;
	case 38 ... 40: PRIN1SP("ALU-FUNCTION-%o", alu_function); break;
	case 41: PRIN1SP("DIVFS"); break;
	case 42 ... 63: PRIN1SP("ALU-FUNCTION-%o", alu_function); break;
	}

	alu = load_byte(u, 003, 006);
	if (alu == 026)
		sub_carry_desc(u);
	else
		normal_carry_desc(u);

	output_selector = load_byte(u, 014, 002);
	switch (output_selector) {
	case 0: PRIN1SP("OUTPUT-SELECTOR-%o", output_selector); break;
	case 1: break;
	case 2: PRIN1SP("OUTPUT-SELECTOR-RIGHTSHIFT-1"); break;
	case 3: PRIN1SP("OUTPUT-SELECTOR-LEFTSHIFT-1"); break;
	}

	q = load_byte(u, 000, 002);
	switch (q) {
	case 0: break;
	case 1: PRIN1SP("SHIFT-Q-LEFT"); break;
	case 2: PRIN1SP("SHIFT-Q-RIGHT"); break;
	case 3: break;
	}

	m_source_desc(u);

	type_field(AMEM, u, 040, 012);

	mf = load_byte(u, 012, 002);
	switch (mf) {
	case 0: break;
	case 1 ... 3: PRIN1SP("MF-%o", mf);
	}

	ilong = load_byte(u, 055, 001);
	switch (ilong) {
	case 0: break;
	case 1: PRIN1SP("ILONG");
	}
}

static void
type_jump_condition(int number)
{
	switch ((number & 01400) >> 010) {
	case 0: PRIN1("JUMP"); break;
	case 1: PRIN1("CALL"); break;
	case 2: PRIN1("POPJ"); break;
	case 3: default: PRIN1("CALL-POPJ-??"); break;
	}

	if ((040 & number) == 0) {
		PRIN1("-IF-BIT-");
		if ((0100 & number) == 0)
			PRIN1("Set");
		else
			PRIN1("Clear");
		if ((0200 & number) == 0)
			PRIN1("-XCT-NEXT");
		PRIN1(" (Byte-field 1 ");
		PRIN1("%o", 32 - (037 & number));
		PRIN1(")");
	} else  {
		char *tem;
		int cond;

		if ((0100 & number) == 0)
			cond = 07 & number;
		else
			cond = (07 & number) + 010;
		switch (cond) {
		case 0: tem = "T"; break;
		case 1: tem = "-LESS-THAN"; break;
		case 2: tem = "-LESS-OR-EQUAL"; break;
		case 3: tem = "-EQUAL"; break;
		case 4: tem = "-IF-PAGE-FAULT"; break;
		case 5: tem = "-IF-PAGE-FAULT-OR-INTERRUPT"; break;
		case 6: tem = "-IF-SEQUENCE-BREAK"; break;
		case 7: tem = "NIL"; break;
		case 8: tem = "T"; break;
		case 9: tem = "-GREATER-OR-EQUAL"; break;
		case 10: tem = "-GREATER-THAN"; break;
		case 11: tem = "-NOT-EQUAL"; break;
		case 12: tem = "-IF-NO-PAGE-FAULT"; break;
		case 13: tem = "-IF-NO-PAGE-FAULT-OR-INTERRUPT"; break;
		case 14: tem = "-IF-NO-SEQUENCE-BREAK"; break;
		case 15: tem = "-NEVER"; break;
		}

		if (strcmp(tem, "T") == 0) {
			if ((0200 & number) == 0)
				PRIN1("-XCT-NEXT");
			PRIN1(" JUMP-CONDITION ");
			PRIN1("%o",  07 & number);
			if ((0100 & number) == 0)
				PRIN1("(Inverted)");
		} else {
			if (strcmp(tem, "NIL") != 0)
				PRIN1("%s", tem);
			if ((0200 & number) == 0)
				PRIN1("-XCT-NEXT");
		}
	}
	PRIN1(" ");
}

static void
jmp_desc(uint64_t u)
{
	int mf;
	int ilong;

	type_jump_condition(load_byte(u, 000, 012));

	m_source_desc(u);

	type_field(AMEM, u, 040, 012);

	type_field(IMEM, u, 014, 016);

	mf = load_byte(u, 012, 002);
	switch (mf) {
	case 0: break;
	case 1 ... 3: PRIN1SP("MF-%o", mf);
	}

	ilong = load_byte(u, 055, 001);
	switch (ilong) {
	case 0: break;
	case 1: PRIN1SP("ILONG");
	}
}

static void
dsp_const_desc(uint64_t u)
{
	PRIN1(" (");

	type_field(NUMBER, u, 040, 012);

	PRIN1(") ");
}

static void
dsp_desc(uint64_t u)
{
	int map;
	int mf;
	int ilong;
	int push_own_address_p;
	int ifetch_p;
	int disp_const;

	PRIN1SP("DISPATCH");

	disp_const = load_byte(u, 040, 012);
	if (disp_const == 0)
		;
	else
		dsp_const_desc(u);

	byte_field_out(u, load_byte(u, 000, 010), true, false);

	m_source_desc(u);

	type_field(DMEM, u, 014, 013);

	push_own_address_p = load_byte(u, 031, 001);
	if (push_own_address_p == 1)
		PRIN1SP("PUSH-OWN-ADDRESS");

	ifetch_p = load_byte(u, 030, 001);
	if (ifetch_p == 1)
		PRIN1SP("IFETCH");

	map = load_byte(u, 010, 002);
	switch (map) {
	case 0: break;
	case 1: PRIN1SP("MAP-14"); break;
	case 2: PRIN1SP("MAP-15"); break;
	case 3: PRIN1SP("MAP-BOTH-14-AND-15"); break;
	}

	mf = load_byte(u, 012, 002);
	switch (mf) {
	case 0: break;
	case 1 ... 3: PRIN1SP("MF-%o", mf);
	}

	ilong = load_byte(u, 055, 001);
	switch (ilong) {
	case 0: break;
	case 1: PRIN1SP("ILONG");
	}
}

static void
byt_desc(uint64_t u)
{
	int byte_operation;
	int mf;
	int ilong;

	dest_desc(u);

	byte_operation = load_byte(u, 014, 002);
	switch (byte_operation) {
	case 0: PRIN1SP("BYTE-OPERATION-%o", byte_operation); break;
	case 1: PRIN1SP("LDB"); break;
	case 2: PRIN1SP("SELECTIVE-DEPOSIT"); break;
	case 3: PRIN1SP("DPB"); break;
	}

	byte_field_out(u, load_byte(u, 000, 012), false, true);

	m_source_desc(u);

	type_field(AMEM, u, 040, 012);

	mf = load_byte(u, 012, 002);
	switch (mf) {
	case 0: break;
	case 1 ... 3: PRIN1SP("MF-%o", mf);
	}

	ilong = load_byte(u, 055, 001);
	switch (ilong) {
	case 0: break;
	case 1: PRIN1SP("ILONG");
	}
}

/* Modify S so that there are no spaces after opening parenthesis, and
   before closing parenthesis.  */
static void
uinst_strip(char *s)
{
	int i;
	int j;

	i = j = 0;
	while (s[i] != '\0') {
		bool skip;

		skip = false;
		if (s[i] == '(' || s[i] == ' ')
			skip = true;
		s[j++] = s[i++];

		if (skip == true) {
			if (s[i] == ' ')
				i++;
			else if (s[i] == ')')
				j--;
		}
	}

	s[j] = '\0';
}

char *
uinst_desc(uint64_t u, symtab_t *symtab)
{
	int popj_after_next_p;
	int opclass;
	int stat_bit;
	int bit_47;

	uinstbuf[0] = '\0';
	uinstbufp = uinstbuf;

	uinstsymtab = *symtab;

	PRIN1("(");

	popj_after_next_p = load_byte(u, 052, 001);
	switch (popj_after_next_p) {
	case 0: break;
	case 1: PRIN1SP("POPJ-AFTER-NEXT"); break;
	}

	opclass = load_byte(u, 053, 002);
	switch (opclass) {
	case 0: alu_desc(u); break;
	case 1: jmp_desc(u); break;
	case 2: dsp_desc(u); break;
	case 3: byt_desc(u); break;
	}

	stat_bit = load_byte(u, 056, 001);
	switch (stat_bit) {
	case 0: break;
	case 1: PRIN1SP("STAT-BIT"); break;
	}

	bit_47 = load_byte(u, 057, 001);
	switch (bit_47) {
	case 0: break;
	case 1: PRIN1SP("BIT-47"); break;
	}

	PRIN1(")");

	uinst_strip(uinstbuf);

	return uinstbuf;
}

#undef PRIN1
#undef PRIN1SP

static int defmics_vector[1024];
static int defmics_size = sizeof defmics / sizeof defmics[0];
static bool defmics_vector_setup = false;

static void
defmics_init(void)
{
	if (defmics_vector_setup == true)
		return;

	for (int i = 0; i < defmics_size; i++) {
		int index;

		if (defmics[i].name == NULL)
			break;

		index = defmics[i].value;
		defmics_vector[index] = i;
	}
	defmics_vector_setup = true;
}

#if SYS78
#include "disass78.c"
#else
#include "disass99.c"
#endif
