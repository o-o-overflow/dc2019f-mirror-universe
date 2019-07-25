// mouse.c --- mouse interface

#include <stdio.h>
#include <err.h>

#include "usim.h"
#include "ucode.h"
#include "iob.h"

#include "syms.h"

int mouse_x;
int mouse_y;
int mouse_head;
int mouse_middle;
int mouse_tail;
int mouse_rawx;
int mouse_rawy;

// Location in A memory of microcode mouse state.
static int mouse_amem_x;
static int mouse_amem_y;

void
mouse_event(int x, int y, int buttons)
{
	iob_csr |= 1 << 4;
	assert_unibus_interrupt(0264);

	// Move mouse closer to where microcode thinks it is.
	int mcx;
	int mcy;
	int dx;
	int dy;

	mcx = read_a_mem(mouse_amem_x);
	mcy = read_a_mem(mouse_amem_y);

	dx = x - mcx;
	dy = y - mcy;
	mouse_x += dx;
	mouse_y += dy;

	if (buttons & 4)
		mouse_head = 1;
	if (buttons & 2)
		mouse_middle = 1;
	if (buttons & 1)
		mouse_tail = 1;
}

void
mouse_init(void)
{
	int val;

	// Defaults if we cannot find them in the symbol table.
	mouse_amem_x = 334;	// A-MOUSE-CURSOR-X
	mouse_amem_y = 335;	// A-MOUSE-CURSOR-Y

	if (sym_find(&sym_mcr, "A-MOUSE-CURSOR-X", &val))
		warnx("can't find A-MOUSE-CURSOR-X in microcode symbols");
	 else
		mouse_amem_x = val;

	if (sym_find(&sym_mcr, "A-MOUSE-CURSOR-Y", &val))
		warnx("can't find A-MOUSE-CURSOR-Y in microcode symbols");
	 else
		mouse_amem_y = val;
}
