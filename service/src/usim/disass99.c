// disass99.c -- macrocode disassembler for System 99
//
// See SYS: SYS2; DISASS LISP for details.

#define DISASSBUFSZ 128
static char dissbuf[DISASSBUFSZ];
static void *dissbufp;

#define PRIN1(args...) do { dissbufp += sprintf(dissbufp, args); } while (0)
#define PRIN1SP(args...) do { PRIN1(args); PRIN1(" "); } while (0)

static int
constants_area(int addr)
{
	PRIN1("%o", addr);
}

static void
disassemble_address(unsigned int fef, unsigned int reg, unsigned int disp, bool second_word, unsigned int pc)
{
	PRIN1(" ");

	if ((reg >= 4) && !second_word)
		disp &= 077;

	if (reg < 4) {
		PRIN1("FEF|%d", disp);
	} else if (reg == 4) {
		PRIN1("'");
		constants_area(disp);
	} else if (reg == 5) {
		PRIN1("LOCAL|%d", disp);
	} else if (reg == 6) {
		PRIN1("ARG|%d", disp);
	} else if (second_word == false && disp == 077) {
		PRIN1("PDL-POP");
	} else if (disp < 040) {
		PRIN1("SELF|%d", disp);
	} else if (disp < 070) {
		PRIN1("SELF-MAP|%d", disp - 040);
	} else {
		PRIN1("PDL|%d (undefined)", disp);
	}
}

static int
disassemble_instruction_length(uint32_t wd)
{
	int op;
	int disp;

	op = ldb(01104, wd);
	disp = ldb(00011, wd);

	if ((op == 014) && (disp == 0777)) {
		printf("ILEN == 2\n");
		return 2;
	}
	if ((op < 014) && (disp == 0776)) {
		printf("ILEN == 2\n");
		return 2;
	}
	return 1;
}

char *
disassemble_instruction(uint32_t fef, uint32_t pc, uint32_t wd, uint32_t second_word)
{
	int op;
	int subop;
	int dest;
	int reg;
	int disp;
	int ilen;

	defmics_init();

	dissbuf[0] = '\0';
	dissbufp = dissbuf;

	ilen = disassemble_instruction_length(wd);

	PRIN1("%011o %06o ", pc, wd);

	op = ldb(01104, wd);
	subop = ldb(01503, wd);
	dest = ldb(01602, wd);
	disp = ldb(00011, wd);
	reg = ldb(00603, wd);

	if (ilen == 2) {
		pc++;
		// If a two-word insn has a source address, it must be an extended address,
		// so set up REG and DISP to be right for that.
		if (op != 014) {
			reg = ldb(00603, second_word);
			disp = dpb(ldb(01104, second_word), 00604, ldb(00006, second_word));
		}
	}

	if (op < 011)
		op = ldb(01105, wd);

	if (wd == 0)
		PRIN1("0");
	else if (op < 011) {	// DEST/ADDR
		PRIN1("%s %s", call_names[op], dest_names[dest]);
		disassemble_address(fef, reg, disp, second_word, -1);
	} else if (op == 011) { // ND1
		PRIN1("%s", nd1_names[subop]);
		disassemble_address(fef, reg, disp, second_word, -1);
	} else if (op == 012) { // ND2
		PRIN1("%s", nd2_names[subop]);
		disassemble_address(fef, reg, disp, second_word, -1);
	} else if (op == 013) { // ND3
		PRIN1("%s", nd3_names[subop]);
		disassemble_address(fef, reg, disp, second_word, -1);
	} else if (op == 014) { // BRANCH
		PRIN1("%s", branch_names[subop]);

		if (disp > 0400)
			disp |= ~0400;

		if (disp != -1) {
			PRIN1(" %o", pc + disp + 1);
		} else {
			disp = second_word;
			if (disp > 0100000)
				disp |= ~0100000;
			PRIN1(" LONG %d", pc + disp + 1);
			return dissbuf;
		}
	} else if (op == 015) { // MISC
		bool print_dest;

		print_dest = true;

		PRIN1SP("(MISC)");

		if ((1 & subop) != 0)
			disp = disp + 01000;

		if (disp < 0220) {
			int i;

			i = ldb(00403, disp);
			PRIN1SP("%s (%d)",
				arefi2_names[i],
				ldb(00004, disp) + ldb(00402, disp) == 2 ? 1 : 0);
		} else if (disp < 0240) {
			PRIN1("PDL-POP %d time(s) ", disp - 0220);

			if (dest == 0)
				return dissbuf;
		} else if (disp == 0460) {
			PRIN1("%s", ifloor_names[dest]);
			PRIN1SP(" one value to stack");
			print_dest = false;
		} else if (disp == 0510) {
			PRIN1("%s", ifloor_names[dest]);
			PRIN1(" two values to stack");
			print_dest = false;
		} else {
			if (disp < 1024 && defmics_vector[disp])
				PRIN1SP("%s", defmics[defmics_vector[disp]].name);
			else
				PRIN1SP("#%o", disp);
		}

		if (print_dest == true)
			PRIN1("%s", dest_names[dest]);
	} else if (op == 016) { // ND4
		if (subop == 00) {
			PRIN1("STACK-CLOSURE-DISCONNECT  local slot %d", disp);
		} else if (subop == 01) {
			PRIN1SP("STACK-CLOSURE-UNSHARE LOCAL|%d", disp);
		} else if (subop == 02) {
			PRIN1("MAKE-STACK-CLOSURE DISP  local slot %d", disp);
		} else if (subop == 03) {
			PRIN1("PUSH-NUMBER %d", disp);
		} else if (subop == 04) {
			PRIN1("STACK-CLOSURE-DISCONNECT-FIRST  local slot %d", disp);
		} else if (subop == 05) {
			PRIN1SP("PUSH-CDR-IF-CAR-EQUAL");
			disassemble_address(fef, reg, disp, second_word, pc);
		} else if (subop == 06) {
			PRIN1SP("PUSH-CDR-STORE-CAR-IF-CONS");
			disassemble_address(fef, reg, disp, second_word, pc);
		} else {
			PRIN1("UNDEF-ND4-%d %d", subop, disp);
		}
	} else if (op == 020) { // AREFI
		PRIN1SP("%s (%d)",
			arefi_names[reg],
			ldb(00006, disp) + (reg == 2 || reg == 6) ? 1 : 0);
		PRIN1("%s", dest_names[dest]);
	} else
		PRIN1("UNDEF-%o", op);

	return dissbuf;
}
