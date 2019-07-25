// lcadrd.c --- low-level CADR-munging routines for CC
//
// See SYS;LCADR:LCADRD LISP and SYS;LCADR:LCADMC LISP for the
// canonical definition.

#include <stdio.h>
#include <stdint.h>

#include "ucode.h"
#include "cc.h"

#include "lcadmc.h"
#include "lcadrd.h"

// Basic SPY I/O routines.

uint16_t
spy_read (int regn)
{
	return cc_get(regn);
}

uint32_t
spy_read32(int high, int low)
{
	uint16_t h;
	uint16_t l;

	h = spy_read(high);
	l = spy_read(low);

	return w32(h, l);
}

uint64_t
spy_read64(int high,int med,int low)
{
	uint64_t h;
	uint16_t m;
	uint16_t l;

	h = spy_read(high);
	m = spy_read(med);
	l = spy_read(low);

	return w64(h, m, l);
}

void
spy_write (int regn, int val)
{
	cc_set(regn, val);
}

void
spy_write32(int high, int low, uint32_t val)
{
	spy_write(high, (uint16_t) (val >> 16) & 0xffff);
	spy_write(low, (uint16_t) (val >> 0) & 0xffff);
}

void
spy_write64(int high, int med, int low, uint64_t val)
{
	spy_write(high, (uint16_t) ((val >> 32) & 0xffff));
	spy_write(med, (uint16_t) ((val >> 16) & 0xffff));
	spy_write(low, (uint16_t) ((val >> 0) & 0xffff));
}

// Routines which manipulate the machine directly.

uint32_t
cc_read_obus(void)
{
	return spy_read32(SPY_OB_HIGH, SPY_OB_LOW);
}

uint32_t
cc_read_a_bus(void)
{
	return spy_read32(SPY_A_HIGH, SPY_A_LOW);
}

uint32_t
cc_read_m_bus(void)
{
	return spy_read32(SPY_M_HIGH, SPY_M_LOW);
}

uint64_t
cc_read_ir(void)
{
	return spy_read64(SPY_IR_HIGH, SPY_IR_MED, SPY_IR_LOW);
}

uint16_t
cc_read_pc(void)
{
	return spy_read(SPY_PC);
}

uint32_t
cc_read_status(void)
{
	uint16_t v1;
	uint16_t v2;
	uint16_t v3;

	v1 = spy_read(SPY_FLAG_1);
	v2 = spy_read(SPY_FLAG_2);
	v3 = spy_read(SPY_IR_LOW);

	if (v3 & 0100)
		v2 ^= 4;	// Hardware reads JC-TRUE incorrectly.

	return w32(v1, v2);
}

void
cc_write_diag_ir(ucw_t ir)
{
	spy_write64(SPY_IR_HIGH, SPY_IR_MED, SPY_IR_LOW, ir);
	spy_write64(SPY_IR_HIGH, SPY_IR_MED, SPY_IR_LOW, ir);
}

void
cc_write_ir(ucw_t ir)
{
	cc_write_diag_ir(ir);
	cc_noop_debug_clock();
}

void
cc_write_md(uint32_t num)
{
	spy_write32(SPY_MD_HIGH, SPY_MD_LOW, num);

	while (1) {
		uint32_t v;

		v = cc_read_md();
		if (v == num)
			break;

		printf("md readback failed, retry got %x want %x\n", v, num);
		spy_write32(SPY_MD_HIGH, SPY_MD_LOW, num);
	}
}

uint32_t
cc_read_md(void)
{
	return spy_read32(SPY_MD_HIGH, SPY_MD_LOW);
}

void
cc_write_vma(uint32_t val)
{
	spy_write32(SPY_VMA_HIGH, SPY_VMA_LOW, val);

	while (cc_read_vma() != val) {
		printf("vma readback failed, retry\n");
		spy_write32(SPY_VMA_HIGH, SPY_VMA_LOW, val);
	}
}

uint32_t
cc_read_vma(void)
{
	return spy_read32(SPY_VMA_HIGH, SPY_VMA_LOW);
}

void
cc_debug_clock(void)
{
	cc_set(SPY_CLK, 012);
	cc_set(SPY_CLK, 0);
}

void
cc_noop_debug_clock(void)
{
	cc_set(SPY_CLK, 016);
	cc_set(SPY_CLK, 0);
}

void
cc_clock(void)
{
	cc_set(SPY_CLK, 2);
	cc_set(SPY_CLK, 0);
}

void
cc_noop_clock(void)
{
	cc_set(SPY_CLK, 6);
	cc_set(SPY_CLK, 0);
}

void
cc_single_step(void)
{
	cc_set(SPY_CLK, 6);
	cc_set(SPY_CLK, 0);
}

// Routine to execute a symbolic instruction.

// Call these via the CC_EXECUTE function.

void
cc_execute_r(ucw_t ir)
{
again:
	cc_write_diag_ir(ir);
	cc_noop_debug_clock();
	if (cc_read_ir() != ir) {
		printf("ir reread failed; retry\n");
		goto again;
	}

	cc_debug_clock();
}

void
cc_execute_w(ucw_t ir)
{
again:
	cc_write_diag_ir(ir);
	cc_noop_debug_clock();
	if (cc_read_ir() != ir) {
		printf("ir reread failed; retry\n");
		goto again;
	}

	cc_clock();
	cc_noop_clock();
}

// Read and write RAMs.

uint32_t
cc_read_m_mem(uint32_t adr)
{
	cc_execute(0,
		   ir_pair(CONS_IR_M_SRC, adr) |
		   ir_pair(CONS_IR_ALUF, CONS_ALU_SETM) |
		   ir_pair(CONS_IR_OB, CONS_OB_ALU));

	return cc_read_obus();
}

void
cc_write_m_mem(uint32_t loc, uint32_t val)
{
	///---!!!
}

uint32_t
cc_read_a_mem(uint32_t adr)
{
	cc_execute(0,
		   ir_pair(CONS_IR_A_SRC, adr) |
		   ir_pair(CONS_IR_ALUF, CONS_ALU_SETA) |
		   ir_pair(CONS_IR_OB, CONS_OB_ALU));

	return cc_read_obus();
}

void
cc_write_a_mem(uint32_t loc, uint32_t val)
{
	uint32_t v2;

	cc_write_md(val);
	v2 = cc_read_md();
	if (v2 != val) {
		printf("cc_write_a_mem; md readback error (got=%o wanted=%o)\n", v2, val);
	}
	cc_execute(WRITE,
		   ir_pair(CONS_IR_M_SRC, CONS_M_SRC_MD) |
		   ir_pair(CONS_IR_ALUF, CONS_ALU_SETM) |
		   ir_pair(CONS_IR_OB, CONS_OB_ALU) |
		   ir_pair(CONS_IR_A_MEM_DEST, CONS_A_MEM_DEST_INDICATOR + loc));
}

// Reset, start and stop.

void
cc_stop_mach(void)
{
	cc_set(SPY_CLK, 0);
}

void
cc_start_mach(void)
{
	cc_set(SPY_CLK, 0001);
}
