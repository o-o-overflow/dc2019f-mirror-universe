#ifndef USIM_LCADRD_H
#define USIM_LCADRD_H

#include <stdint.h>

#include "ucode.h"

enum {				// Reading
	SPY_IR_LOW      = 000,
	SPY_IR_MED      = 001,
	SPY_IR_HIGH     = 002,
	SPY_SCRATCH     = 003, ///---!!!
	SPY_OPC         = 004,
	SPY_PC          = 005,
	SPY_OB_LOW      = 006,
	SPY_OB_HIGH     = 007,
	SPY_FLAG_1      = 010,
	SPY_FLAG_2      = 011,
	SPY_M_LOW       = 012,
	SPY_M_HIGH      = 013,
	SPY_A_LOW       = 014,
	SPY_A_HIGH      = 015,
	SPY_STAT_LOW    = 016,
	SPY_STAT_HIGH   = 017,

	SPY_MD_LOW      = 020, ///---!!!
	SPY_MD_HIGH     = 021, ///---!!!
	SPY_VMA_LOW     = 022, ///---!!!
	SPY_VMA_HIGH    = 023, ///---!!!
	SPY_DISK        = 026, ///---!!!
	SPY_BD          = 027, ///---!!!
};

enum {				// WRITING
	//SPY_IR_LOW    = 0,
	//SPY_IR_MED    = 1,
	//SPY_IR_HIGH   = 2,
	SPY_CLK         = 3,
	SPY_OPC_CONTROL = 4,
	SPY_MODE        = 5,
};

#define w32(high, low) ((high << 16) | low)
#define w64(high, med, low) ((high << 32) | (med << 16) | low)

extern uint16_t spy_read (int regn);
#define spy_read16(regn) spy_read(regn)
extern uint32_t spy_read32(int high, int low);
extern uint64_t spy_read64(int high, int med, int low);

extern void spy_write (int regn, int val);
#define spy_write16(regn, val) spy_write(regn, val)
extern void spy_write32(int high, int low, uint32_t val);
extern void spy_write64(int high, int med, int low, uint64_t val);

extern uint32_t cc_read_obus(void);
extern uint32_t cc_read_a_bus(void);
extern uint32_t cc_read_m_bus(void);
extern uint64_t cc_read_ir(void);
extern uint16_t cc_read_pc(void);
extern uint32_t cc_read_status(void);
extern void cc_write_diag_ir(ucw_t ir);
extern void cc_write_ir(ucw_t ir);
extern void cc_write_md(uint32_t num);
extern uint32_t cc_read_md(void);
extern void cc_write_vma(uint32_t val);
extern uint32_t cc_read_vma(void);

extern void cc_debug_clock(void);
extern void cc_noop_debug_clock(void);
extern void cc_clock(void);
extern void cc_noop_clock(void);
extern void cc_single_step(void);

extern void cc_execute_r(ucw_t ir);
extern void cc_execute_w(ucw_t ir);

extern uint32_t cc_read_m_mem(uint32_t adr);
extern void cc_write_m_mem(uint32_t loc, uint32_t val);
extern uint32_t cc_read_a_mem(uint32_t adr);
extern void cc_write_a_mem(uint32_t loc, uint32_t val);

extern void cc_stop_mach(void);
extern void cc_start_mach(void);

#endif
