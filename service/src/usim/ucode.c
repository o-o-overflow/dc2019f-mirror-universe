// ucode.c --- CADR microcode simulator
//
// 'The time has come,' the Walrus said,
//   'To talk of many things:
// Of shoes -- and ships -- and sealing wax --
//   Of cabbages -- and kings --
// And why the sea is boiling hot --
//   And whether pigs have wings.'
//       -- Lewis Carroll, The Walrus and Carpenter
//
// (and then, they ate all the clams :-)

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

#include "usim.h"
#include "utrace.h"
#include "ucfg.h"
#include "ucode.h"
#include "mem.h"
#include "iob.h"
#include "tv.h"
#include "chaos.h"
#include "disk.h"

#include "disass.h"
#include "syms.h"
#include "misc.h"

bool run_ucode_flag = true;

ucw_t prom_ucode[512];

static ucw_t ucode[16 * 1024];
static uint32_t dispatch_memory[2048];

static size_t cycles;

static int u_pc;

static int page_fault_flag;
static int interrupt_pending_flag;
static int interrupt_status_reg;

static int sequence_break_flag;
static int interrupt_enable_flag;
static int bus_reset_flag;

static uint32_t md;
static uint32_t vma;
static uint32_t q;
static uint32_t opc;

static uint32_t new_md;
static int new_md_delay;

static int write_fault_bit;
static int access_fault_bit;

static int alu_carry;
static uint32_t alu_out;

static uint32_t oa_reg_lo;
static uint32_t oa_reg_hi;
static int oa_reg_lo_set;
static int oa_reg_hi_set;

static int interrupt_control;
static uint32_t dispatch_constant;

ucw_t prom_ucode[512];
bool prom_enabled_flag = true;

static void record_lc_history(void);

int
read_prom(char *file)
{
	int fd;
	uint32_t code;
	uint32_t start;
	uint32_t size;

	fd = open(file, O_RDONLY | O_BINARY);
	if (fd < 0) {
		perror(file);
		exit(1);
	}

	code = read32(fd);
	start = read32(fd);
	size = read32(fd);
	INFO(TRACE_MISC, "prom (%s): code: %d, start: %d, size: %d\n", file, code, start, size);

	int loc = start;
	for (uint32_t i = 0; i < size; i++) {
		uint16_t w1;
		uint16_t w2;
		uint16_t w3;
		uint16_t w4;

		w1 = read16(fd);
		w2 = read16(fd);
		w3 = read16(fd);
		w4 = read16(fd);
		prom_ucode[loc] =
			((uint64_t) w1 << 48) |
			((uint64_t) w2 << 32) |
			((uint64_t) w3 << 16) |
			((uint64_t) w4 << 0);

		loc++;
	}

	return 0;
}

static void
set_interrupt_status_reg(int new)
{
	interrupt_status_reg = new;
	interrupt_pending_flag = (interrupt_status_reg & 0140000) ? 1 : 0;
}

void
assert_unibus_interrupt(int vector)
{
	// Unibus interrupts enabled?
	if (interrupt_status_reg & 02000) {
		DEBUG(TRACE_INT, "assert: unibus interrupt (enabled)\n");
		set_interrupt_status_reg((interrupt_status_reg & ~01774) | 0100000 | (vector & 01774));
	} else {
		DEBUG(TRACE_INT, "assert: unibus interrupt (disabled)\n");
	}
}

void
deassert_unibus_interrupt(void)
{
	if (interrupt_status_reg & 0100000) {
		DEBUG(TRACE_INT, "deassert: unibus interrupt\n");
		set_interrupt_status_reg(interrupt_status_reg & ~(01774 | 0100000));
	}
}

void
assert_xbus_interrupt(void)
{
	DEBUG(TRACE_INT, "assert: xbus interrupt (%o)\n", interrupt_status_reg);
	set_interrupt_status_reg(interrupt_status_reg | 040000);
}

void
deassert_xbus_interrupt(void)
{
	if (interrupt_status_reg & 040000) {
		DEBUG(TRACE_INT, "deassert: xbus interrupt\n");
		set_interrupt_status_reg(interrupt_status_reg & ~040000);
	}
}

// ---!!! read_mem, write_mem: Document each address.

static void
unibus_read(int offset, uint32_t *pv)
{
	switch (offset) {
	case 040:
		DEBUG(TRACE_IOB, "unibus: read interrupt status\n");
		*pv = 0;
		break;
	case 044:
		DEBUG(TRACE_IOB, "unibus: read error status\n");
		*pv = 0;
		break;
	default:
		*pv = 0;
		break;
	}
}

// Read virtual memory, returns -1 on fault and 0 if OK.
static int
read_mem(int vaddr, uint32_t *pv)
{
	uint32_t map;
	int pn;
	int offset;
	struct page_s *page;

	access_fault_bit = 0;
	write_fault_bit = 0;
	page_fault_flag = 0;

	// 14 bit page number.
	map = map_vtop(vaddr, (int *) 0, &offset);
	pn = map & 037777;

	if ((map & (1 << 23)) == 0) {
		// No access permission.
		access_fault_bit = 1;
		page_fault_flag = 1;
		opc = pn;
		*pv = 0;
		DEBUG(TRACE_MISC, "read_mem(vaddr=%o) access fault\n", vaddr);
		return -1;
	}

	page = phy_pages[pn];
	if (pn < 020000 && page) {
		*pv = page->w[offset];
		return 0;
	}

	// Simulate fixed number of RAM pages (< 2MW?).
	if (pn >= phys_ram_pages && pn <= 035777) {
		*pv = 0xffffffff;
		return 0;
	}

	switch (pn) {
	case 036000:
		// Inhibit color probe.
		if ((vaddr & 077700000) == 077200000) {
			*pv = 0x0;
			return 0;
		}
		offset = vaddr & 077777;
		tv_read(offset, pv);
		return 0;
	case 037764:		// Extra xbus devices.
		offset <<= 1;
		iob_unibus_read(offset, (int *) pv);
		return 0;
	case 037766:		// Unibus.
		unibus_read(offset, pv);
		return 0;
	case 036777:		      // Disk & TV controller on XBUS.
		if (offset >= 0370) { // Disk.
			disk_xbus_read(offset, pv);
			return 0;
		}
		if (offset == 0360) { // TV.
			tv_xbus_read(offset, pv);
			return 0;
		}
		DEBUG(TRACE_MISC, "xbus read %o %o\n", offset, vaddr);
		*pv = 0;
		return 0;
	}

	// Page fault.
	page = phy_pages[pn];
	if (page == 0) {
		page_fault_flag = 1;
		opc = pn;
		DEBUG(TRACE_MISC, "read_mem(vaddr=%o) page fault\n", vaddr);
		*pv = 0;
		return -1;
	}

	*pv = page->w[offset];
	return 0;
}

static void
unibus_write(int offset, uint32_t v)
{
	switch (offset) {
	case 012:
		DEBUG(TRACE_IOB, "unibus: write mode register %o\n", v);
		if ((v & 044) == 044) {
			DEBUG(TRACE_IOB, "unibus: disabling prom enable flag\n");
			prom_enabled_flag = false;

			if (warm_boot_flag) {
				restore_state(ucfg.usim_state_filename);
			}
		}

		if (v & 2) {
			DEBUG(TRACE_IOB, "unibus: normal speed\n");
		}
		break;
	case 040:
		DEBUG(TRACE_IOB, "unibus: write interrupt status %o\n", v);
		set_interrupt_status_reg((interrupt_status_reg & ~0036001) | (v & 0036001));
		break;
	case 042:
		DEBUG(TRACE_IOB, "unibus: write interrupt stim %o\n", v);
		set_interrupt_status_reg((interrupt_status_reg & ~0101774) | (v & 0101774));
		break;
	case 044:
		DEBUG(TRACE_IOB, "unibus: clear bus error %o\n", v);
		break;
	default:
		if (offset >= 0140 && offset <= 0176) {
			DEBUG(TRACE_IOB, "unibus: mapping reg %o\n", offset);
			break;
		}
		DEBUG(TRACE_IOB, "unibus: write? v %o, offset %o\n", v, offset);
		break;
	}
}

// Write virtual memory.
static int
write_mem(int vaddr, uint32_t v)
{
	uint32_t map;
	int pn;
	int offset;
	struct page_s *page;

	write_fault_bit = 0;
	access_fault_bit = 0;
	page_fault_flag = 0;

	// 14 bit page number.
	map = map_vtop(vaddr, (int *) 0, &offset);
	pn = map & 037777;

	if ((map & (1 << 23)) == 0) {
		// No access permission.
		access_fault_bit = 1;
		page_fault_flag = 1;
		opc = pn;
		DEBUG(TRACE_MISC, "write_mem(vaddr=%o) access fault\n", vaddr);
		return -1;
	}

	if ((map & (1 << 22)) == 0) {
		// No write permission.
		write_fault_bit = 1;
		page_fault_flag = 1;
		opc = pn;
		DEBUG(TRACE_MISC, "write_mem(vaddr=%o) write fault\n", vaddr);
		return -1;
	}

	page = phy_pages[pn];
	if (pn < 020000 && page) {
		page->w[offset] = v;
		return 0;
	}

	switch (pn) {
	case 036000:
		// Inhibit color probe.
		if ((vaddr & 077700000) == 077200000) {
			return 0;
		}
		offset = vaddr & 077777;
		tv_write(offset, v);
		return 0;
	case 037760:
		DEBUG(TRACE_MISC, "tv: reg write %o, offset %o, v %o\n", vaddr, offset, v);
		return 0;
	case 037764:		// Extra xbus devices.
		offset <<= 1;
		DEBUG(TRACE_IOB, "unibus: iob v %o, offset %o\n", vaddr, offset);
		iob_unibus_write(offset, v);
		return 0;
	case 037766:		// Unibus.
		offset <<= 1;
		unibus_write(offset, v);
		return 0;
	case 036777:		// Disk & TV controller on XBUS.
		if (offset >= 0370) {
			disk_xbus_write(offset, v);
			return 0;
		}
		if (offset == 0360) {
			tv_xbus_write(offset, v);
			return 0;
		}
		return 0;
	}

	// Catch questionable accesses.
	if (pn >= 036000) {
		DEBUG(TRACE_MISC, "??: reg write vaddr %o, pn %o, offset %o, v %o; u_pc %o\n", vaddr, pn, offset, v, u_pc);
	}

	page = phy_pages[pn];
	if (page == 0) {
		// Page fault.
		page_fault_flag = 1;
		opc = pn;
		return -1;
	}

	page->w[offset] = v;
	return 0;
}

static uint32_t a_memory[1024];
static uint32_t m_memory[32];

void
write_a_mem(int loc, uint32_t v)
{
	a_memory[loc] = v;
}

uint32_t
read_a_mem(int loc)
{
	return a_memory[loc];
}

static uint32_t
read_m_mem(int loc)
{
	if (loc > 32)
		WARNING(TRACE_MISC, "read m-memory address > 32! (%o)\n", loc);

	return m_memory[loc];
}

static void
write_m_mem(int loc, uint32_t v)
{
	m_memory[loc] = v;
	a_memory[loc] = v;
}

static uint32_t pdl_memory[1024];
static int pdl_ptr;
static int pdl_index;

#define USE_PDL_PTR 1
#define USE_PDL_INDEX 2

static uint32_t
read_pdl_mem(int which)
{
	switch (which) {
	case USE_PDL_PTR:
		if (pdl_ptr > 1024)
			WARNING(TRACE_MISC, "pdl ptr %o!\n", pdl_ptr);
		return pdl_memory[pdl_ptr];
	case USE_PDL_INDEX:
		if (pdl_index > 1024)
			WARNING(TRACE_MISC, "pdl ptr %o!\n", pdl_index);
		return pdl_memory[pdl_index];
	}
	return -1;		///---!! Not reachable.
}

static void
write_pdl_mem(int which, uint32_t v)
{
	if (pdl_index >= 1024) {
		WARNING(TRACE_MISC, "pdl ptr %o!\n", pdl_index);
		return;
	}

	switch (which) {
	case USE_PDL_PTR:
		pdl_memory[pdl_ptr] = v;
		break;
	case USE_PDL_INDEX:
		pdl_memory[pdl_index] = v;
		break;
	}
}

static int spc_stack[32];
static int spc_stack_ptr;

static void
push_spc(int pc)
{
	spc_stack_ptr = (spc_stack_ptr + 1) & 037;
	spc_stack[spc_stack_ptr] = pc;
}

static int
pop_spc(void)
{
	uint32_t v;

	v = spc_stack[spc_stack_ptr];
	spc_stack_ptr = (spc_stack_ptr - 1) & 037;
	return v;
}

static int lc;
static int lc_byte_mode_flag;

// Advance the LC register, following the rules; will read next VMA if
// needed.
static void
advance_lc(int *ppc)
{
	int old_lc;

	old_lc = lc & 0377777777; // LC is 26 bits.

	if (lc_byte_mode_flag) {
		lc++;		// Byte mode.
	} else {
		lc += 2;	// 16-bit mode.
	}

	// NEED-FETCH?
	if (lc & (1UL << 31UL)) {
		lc &= ~(1UL << 31UL);
		vma = old_lc >> 2;
		read_mem(old_lc >> 2, &new_md);
		new_md_delay = 2;
		DEBUG(TRACE_MISC, "advance_lc() read vma %011o -> %011o\n", old_lc >> 2, new_md);
	} else {
		// Force skipping 2 instruction (PF + SET-MD).
		if (ppc)
			*ppc |= 2;
		DEBUG(TRACE_MISC, "advance_lc() no read; md = %011o\n", md);
	}

	{
		char lc0b;
		char lc1;
		char last_byte_in_word;

		// This is ugly, but follows the hardware logic (I
		// need to distill it to intent but it seems correct).
		lc0b = (lc_byte_mode_flag ? 1 : 0) & // Byte mode.
			((lc & 1) ? 1 : 0);	     // LC0.
		lc1 = (lc & 2) ? 1 : 0;
		last_byte_in_word = (~lc0b & ~lc1) & 1;
		DEBUG(TRACE_MISC, "lc0b %d, lc1 %d, last_byte_in_word %d\n", lc0b, lc1, last_byte_in_word);
		if (last_byte_in_word)
			// Set NEED-FETCH.
			lc |= (1UL << 31UL);
	}

	record_lc_history();
}

// Write value to decoded destination.
static void
write_dest(int dest, uint32_t out_bus)
{
	if (dest & 04000) {
		write_a_mem(dest & 03777, out_bus);
		return;
	}

	switch (dest >> 5) {
	case 1:			// LC (location counter) 26 bits.
		DEBUG(TRACE_MISC, "writing LC <- %o\n", out_bus);
		lc = (lc & ~0377777777) | (out_bus & 0377777777);

		if (lc_byte_mode_flag) {
			// ---!!! Not sure about byte mode...
		} else {
			// In half word mode, low order bit is
			// ignored.
			lc &= ~1;
		}

		// Set NEED-FETCH.
		lc |= (1UL << 31UL);

		record_lc_history();
		break;
	case 2:			// Interrrupt Control <29-26>.
		DEBUG(TRACE_MISC, "writing IC <- %o\n", out_bus);
		interrupt_control = out_bus;

		lc_byte_mode_flag = interrupt_control & (1 << 29);
		bus_reset_flag = interrupt_control & (1 << 28);
		interrupt_enable_flag = interrupt_control & (1 << 27);
		sequence_break_flag = interrupt_control & (1 << 26);

		if (sequence_break_flag) {
			DEBUG(TRACE_INT, "ic: sequence break request\n");
		}

		if (interrupt_enable_flag) {
			DEBUG(TRACE_INT, "ic: interrupt enable\n");
		}

		if (bus_reset_flag) {
			DEBUG(TRACE_INT, "ic: bus reset\n");
		}

		if (lc_byte_mode_flag) {
			DEBUG(TRACE_INT, "ic: lc byte mode\n");
		}

		lc = (lc & ~(017 << 26)) | // Preserve flags.
			(interrupt_control & (017 << 26));
		break;
	case 010:		// PDL (addressed by pointer)
		DEBUG(TRACE_MISC, "writing pdl[%o] <- %o\n", pdl_ptr, out_bus);
		write_pdl_mem(USE_PDL_PTR, out_bus);
		break;
	case 011:		// PDL (addressed by pointer, push)
		pdl_ptr = (pdl_ptr + 1) & 01777;
		DEBUG(TRACE_MISC, "writing pdl[%o] <- %o, push\n", pdl_ptr, out_bus);
		write_pdl_mem(USE_PDL_PTR, out_bus);
		break;
	case 012:		// PDL (address by index).
		DEBUG(TRACE_MISC, "writing pdl[%o] <- %o\n", pdl_index, out_bus);
		write_pdl_mem(USE_PDL_INDEX, out_bus);
		break;
	case 013:		// PDL index.
		DEBUG(TRACE_MISC, "pdl-index <- %o\n", out_bus);
		pdl_index = out_bus & 01777;
		break;
	case 014:		// PDL pointer.
		DEBUG(TRACE_MISC, "pdl-ptr <- %o\n", out_bus);
		pdl_ptr = out_bus & 01777;
		break;
	case 015:		// SPC data, push.
		push_spc(out_bus);
		break;
	case 016:		// Next instruction modifier (lo).
		oa_reg_lo = out_bus & 0377777777;
		oa_reg_lo_set = 1;
		DEBUG(TRACE_MISC, "setting oa_reg lo %o\n", oa_reg_lo);
		break;
	case 017:		// Next instruction modifier (hi).
		oa_reg_hi = out_bus;
		oa_reg_hi_set = 1;
		DEBUG(TRACE_MISC, "setting oa_reg hi %o\n", oa_reg_hi);
		break;
	case 020:		// VMA register (memory address).
		vma = out_bus;
		break;
	case 021:	      // VMA register, start main memory read.
		vma = out_bus;
		read_mem(vma, &new_md);
		new_md_delay = 2;
		break;
	case 022:	     // VMA register, start main memory write.
		vma = out_bus;
		write_mem(vma, md);
		break;
	case 023:		// VMA register, write map.
		vma = out_bus;
		DEBUG(TRACE_VM, "vma-write-map md=%o, vma=%o (addr %o)\n", md, vma, md >> 13);
	write_map:
		if ((vma >> 26) & 1) {
			int l1_index;
			int l1_data;

			l1_index = (md >> 13) & 03777;
			l1_data = (vma >> 27) & 037;

			l1_map[l1_index] = l1_data;
			invalidate_vtop_cache();
			DEBUG(TRACE_VM, "l1_map[%o] <- %o\n", l1_index, l1_data);
		}

		if ((vma >> 25) & 1) {
			int l1_index;
			int l1_data;
			int l2_index;
			uint32_t l2_data;

			l1_index = (md >> 13) & 03777;
			l1_data = l1_map[l1_index];

			l2_index = (l1_data << 5) | ((md >> 8) & 037);
			l2_data = vma;

			l2_map[l2_index] = l2_data;
			invalidate_vtop_cache();
			DEBUG(TRACE_VM, "l2_map[%o] <- %o\n", l2_index, l2_data);
			add_new_page_no(l2_data & 037777);
		}
		break;
	case 030:		// MD register (memory data).
		md = out_bus;
		DEBUG(TRACE_MISC, "md<-%o\n", md);
		break;
	case 031:
		md = out_bus;
		read_mem(vma, &new_md);
		new_md_delay = 2;
		break;
	case 032:
		md = out_bus;
		write_mem(vma, md);
		break;
	case 033:		// MD register, write map (like 23).
		md = out_bus;
		DEBUG(TRACE_MISC, "memory-data-write-map md=%o, vma=%o (addr %o)\n", md, vma, md >> 13);
		goto write_map;
		break;
	}
	write_m_mem(dest & 037, out_bus);
}

// For 32-bit integers, (A + B) & (1 << 32) will always be
// zero.  Without resorting to 64-bit arithmetic, you can find the
// carry by B > ~A.  How does it work? ~A (the complement of A) is the
// largest possible number you can add to A without a carry: A + ~A =
// (1 << 32) - 1.  If B is any larger, then a carry will be generated
// from the top bit.
#define add32(a, b, ci, out, co)					\
	out = ((uint32_t) a) + (b) + ((ci) ? 1 : 0);			\
	co = (ci) ? (((b) >= ~(a)) ? 0:1) : (((b) > ~(a)) ? 0:1) ;
#define sub32(a, b, ci, out, co)			\
	out = (a) - (b) - ((ci) ? 0 : 1);		\
	co = (unsigned)(out) < (unsigned)(a) ? 1 : 0;

static uint32_t
rotate_left(uint32_t value, int bitstorotate)
{
	uint32_t tmp;
	int mask;

	// Determine which bits will be impacted by the rotate.
	if (bitstorotate == 0)
		mask = 0;
	else
		mask = (int) 0x80000000 >> bitstorotate;
	// Save off the affected bits.
	tmp = (uint64_t) (value & mask) >> (32 - bitstorotate);
	// Perform the actual rotate, and add the rotated bits back in
	// (in the proper location).
	return (value << bitstorotate) | tmp;
}

#define MAX_PC_HISTORY 64

static struct {
	unsigned int pc;
} pc_history[MAX_PC_HISTORY];

static int pc_history_head = 0;
static int pc_history_tail= 0;

static void
record_pc_history(int pc)
{
	pc_history[pc_history_head].pc = pc;

	pc_history_head = (pc_history_head + 1) % MAX_PC_HISTORY;
}

#define MAX_LC_HISTORY 200

struct {
	unsigned short instr;
	unsigned int lc;
} lc_history[MAX_LC_HISTORY];

int lc_history_head = 0;
int lc_history_tail = 0;

static void
record_lc_history(void)
{
	unsigned int instr;

	{
		int oafb;
		int owfb;
		int opff;

		oafb = access_fault_bit;
		owfb = write_fault_bit;
		opff = page_fault_flag;

		read_mem(lc >> 2, &instr);

		access_fault_bit = oafb;
		write_fault_bit = owfb;
		page_fault_flag = opff;
	}

	lc_history[lc_history_head].instr = (lc & 2) ? (instr >> 16) & 0xffff : (instr & 0xffff);
	lc_history[lc_history_head].lc = lc;

	if (0) {
		unsigned short instr;
		int loc;

		instr = lc_history[lc_history_head].instr;
		loc = lc_history[lc_history_head].lc & 0377777777;
		printf("\t%s\n", disassemble_instruction(0, loc, instr, instr));
	}

	lc_history_head = (lc_history_head + 1) % MAX_LC_HISTORY;
}

static void
printlbl(symtype_t type, int loc)
{
	char *l;
	int offset;

	l = sym_find_by_type_val(prom_enabled_flag ? &sym_prom : &sym_mcr, type, loc, &offset);
	if (l == NULL) {
		printf("%03o", loc);
	} else {
		if (offset == 0)
			printf("(%s)", l);
		else
			printf("(%s %o)", l, offset);
	}
}

static void
show_pc_history(void)
{
	int head;

	printf("Micro PC History (OPC's), oldest first:	\n");
	head = pc_history_head;
	for (int i = 0; i < MAX_PC_HISTORY; i++) {
		unsigned int pc;

		pc = pc_history[head].pc;
		head = (head + 1) % MAX_PC_HISTORY;

		if (pc == 0)
			break;

		printf("  %05o\t", pc);
		printlbl(IMEM, pc);
		if (prom_enabled_flag)
			printf("\t...in the PROM.");
		printf("\n");
	}
}

static void
show_spc_stack(void)
{
	if (spc_stack_ptr == 0)
		return;

	printf("Backtrace of microcode subroutine stack:\n");
	for (int i = spc_stack_ptr; i >= 0; i--) {
		int pc;

		pc = spc_stack[i] & 037777;

		printf("%2o %011o ", i, spc_stack[i]);
		printlbl(IMEM, pc);
		printf("\n");
	}
}

static void
show_lc_history(void)
{
	int head;

	printf("Complete backtrace follows:\n");
	head = lc_history_head;
	for (int i = 0; i < MAX_LC_HISTORY; i++) {
		unsigned short instr;
		int loc;

		instr = lc_history[head].instr;
		loc = lc_history[head].lc & 0377777777;
		head = (head + 1) % MAX_LC_HISTORY;

		// Skip printing out obviously empty entries.
		if (loc == 0 && instr == 0)
			continue;

		printf("\t%s\n", disassemble_instruction(0, loc, instr, instr));
	}

	printf("\n");
}

static void
show_mmem(void)
{
	printf("M-MEM:\n");
	for (int i = 0; i < 32; i += 4) {
		printf("\tM[%02o] %011o %011o %011o %011o\n",
		       i, m_memory[i + 0], m_memory[i + 1], m_memory[i + 2], m_memory[i + 3]);
	}
	printf("\n");
}


static void
show_amem(void)
{
	printf("A-MEM:\n");
	for (int i = 0; i < 1024; i += 4) {
		int skipped;

		printf("\tA[%04o] %011o %011o %011o %011o\n",
		       i, a_memory[i + 0], a_memory[i + 1], a_memory[i + 2], a_memory[i + 3]);

		skipped = 0;
		while (a_memory[i + 0] == a_memory[i + 0 + 4] &&
		       a_memory[i + 1] == a_memory[i + 1 + 4] &&
		       a_memory[i + 2] == a_memory[i + 2 + 4] &&
		       a_memory[i + 3] == a_memory[i + 3 + 4] &&
		       i < 1024) {
			if (skipped == 0)
				printf("\t...\n");
			skipped++;
			i += 4;
		}

	}
	printf("\n");
}

static void
show_ammem_sym(void)
{
	printf("A/M-MEMORY BY SYMBOL:\n");
	for (int i = 0; i < 1024; i++) {
		char *l;

		l = sym_find_by_type_val(prom_enabled_flag ? &sym_prom : &sym_mcr, AMEM, i, NULL);
		if (l != NULL) {
			printf("\t%04o %-40s %011o", i, l, a_memory[i]);
			if (i < 32) {
				l = sym_find_by_type_val(prom_enabled_flag ? &sym_prom : &sym_mcr, MMEM, i, NULL);
				if (l != NULL) {
					printf("  %-40s %011o", l, m_memory[i]);
				}
			}
			printf("\n");
		}

	}
	printf("\n");
}

static void
show_spc(void)
{
	printf("SPC STACK:\n");
	printf("\tSPC POINTER: %o\n", spc_stack_ptr);
	for (int i = 0; i < 32; i += 4) {
		printf("\tSPC[%02o] %011o %011o %011o %011o\n",
		       i,
		       spc_stack[i + 0], spc_stack[i + 1], spc_stack[i + 2], spc_stack[i + 3]);
	}
	printf("\n");
}

static void
show_pdl(void)
{
	printf("PDL MEMORY:\n");
	printf("\tPDL POINTER: %o, PDL INDEX: %o\n", pdl_ptr, pdl_index);
	for (int i = 0; i < 1024; i += 4) {
		int skipped;

		printf("\tPDL[%04o] %011o %011o %011o %011o\n",
		       i, pdl_memory[i + 0], pdl_memory[i + 1], pdl_memory[i + 2], pdl_memory[i + 3]);

		skipped = 0;
		while (pdl_memory[i + 0] == pdl_memory[i + 0 + 4] &&
		       pdl_memory[i + 1] == pdl_memory[i + 1 + 4] &&
		       pdl_memory[i + 2] == pdl_memory[i + 2 + 4] &&
		       pdl_memory[i + 3] == pdl_memory[i + 3 + 4] &&
		       i < 1024) {
			if (skipped == 0)
				printf("\t...\n");
			skipped++;
			i += 4;
		}
	}
	printf("\n");
}

static void
show_l1_map(void)
{
	printf("L1 MAP:\n");
	for (int i = 0; i < 2048; i += 4) {
		int skipped;

		printf("\tL1[%04o] %011o %011o %011o %011o\n",
		       i, l1_map[i + 0], l1_map[i + 1], l1_map[i + 2], l1_map[i + 3]);

		skipped = 0;
		while (l1_map[i + 0] == l1_map[i + 0 + 4] &&
		       l1_map[i + 1] == l1_map[i + 1 + 4] &&
		       l1_map[i + 2] == l1_map[i + 2 + 4] &&
		       l1_map[i + 3] == l1_map[i + 3 + 4] &&
		       i < 2048) {
			if (skipped == 0)
				printf("\t...\n");
			skipped++;
			i += 4;
		}
	}
	printf("\n");
}

static void
show_l2_map(void)
{
	printf("L2 MAP:\n");
	for (int i = 0; i < 1024; i += 4) {
		int skipped;

		printf("\tL2[%04o] %011o %011o %011o %011o\n",
		       i, l2_map[i + 0], l2_map[i + 1], l2_map[i + 2], l2_map[i + 3]);

		skipped = 0;
		while (l2_map[i + 0] == l2_map[i + 0 + 4] &&
		       l2_map[i + 1] == l2_map[i + 1 + 4] &&
		       l2_map[i + 2] == l2_map[i + 2 + 4] &&
		       l2_map[i + 3] == l2_map[i + 3 + 4] &&
		       i < 1024) {
			if (skipped == 0)
				printf("\t...\n");
			skipped++;
			i += 4;
		}
	}
	printf("\n");
}

void
dump_state(void)
{
	unsigned int pc;

	pc = pc_history[pc_history_tail].pc;

	printf("***********************************************\n");
	printf("PC=%05o\t", pc);
	printlbl(IMEM, pc);
	printf("\n");
	printf("IR=%s\n", uinst_desc(ucode[pc], prom_enabled_flag ? &sym_prom : &sym_mcr));
	show_pc_history();
	show_spc_stack();
	show_lc_history();

	show_mmem();
	show_amem();
	show_ammem_sym();

	show_spc();

	show_pdl();

	show_l1_map();
	show_l2_map();

	save_state(ucfg.usim_state_filename);
}

void
run(void)
{
	ucw_t p1;
	int p0_pc;
	int p1_pc;
	char no_exec_next;

	u_pc = 0;

	p1 = 0;
	p0_pc = 0;
	p1_pc = 0;
	no_exec_next = 0;

	write_phy_mem(0, 0);

	while (run_ucode_flag) {
		char op_code;
		char invert_sense;
		char take_jump;
		int a_src;
		int m_src;
		int new_pc;
		int dest;
		int aluop;
		int r_bit;
		int p_bit;
		int n_bit;
		int m_src_value;
		int a_src_value;
		int widthm1;
		int pos;
		int mr_sr_bits;
		uint32_t left_mask;
		uint32_t right_mask;
		uint32_t mask;
		uint32_t old_q;
		int left_mask_index;
		int right_mask_index;
		int disp_const;
		int disp_addr;
		int map;
		int len;
		int rot;
		int carry_in;
		int do_add;
		int do_sub;
		uint32_t out_bus;
		int64_t lv;
		ucw_t u;
		ucw_t w;
#define p0 u
		char n_plus1;
		char enable_ish;
		char popj;

		m_src_value = 0;

		if (cycles == 0) {
			p0 = p1 = 0;
			p1_pc = 0;
			no_exec_next = 0;
		}

	next:
		iob_poll();
		disk_poll();
		if ((cycles & 0x0ffff) == 0) {
			tv_poll();
			chaos_poll();
		}

		// Enforce max. cycles.
		cycles++;
		if (cycles == 0)
			// Handle overflow.
			cycles = 1;

		// Fetch next instruction from PROM or RAM.
#define FETCH() (prom_enabled_flag ? prom_ucode[u_pc] : ucode[u_pc])

		// CPU pipeline.
		p0 = p1;
		p0_pc = p1_pc;
		p1 = FETCH();
		p1_pc = u_pc;
		u_pc++;

		if (new_md_delay) {
			new_md_delay--;
			if (new_md_delay == 0)
				md = new_md;
		}

		// Stall pipe for one cycle.
		if (no_exec_next) {
			DEBUG(TRACE_MISC, "no_exec_next; u_pc %o\n", u_pc);
			no_exec_next = 0;

			p0 = p1;
			p0_pc = p1_pc;

			p1 = FETCH();
			p1_pc = u_pc;
			u_pc++;
		}

		// Next instruction modify.
		if (oa_reg_lo_set) {
			DEBUG(TRACE_MISC, "merging oa lo %o\n", oa_reg_lo);
			oa_reg_lo_set = 0;
			u |= oa_reg_lo;
		}

		if (oa_reg_hi_set) {
			DEBUG(TRACE_MISC, "merging oa hi %o\n", oa_reg_hi);
			oa_reg_hi_set = 0;
			u |= (ucw_t) oa_reg_hi << 26;
		}

		if (0) {
			char *uinst;

			uinst = uinst_desc(u, prom_enabled_flag ? &sym_prom : &sym_mcr);
			printf("%s", uinst);
			printlbl(IMEM, p0_pc);
			printf("\n");
		}

		record_pc_history(p0_pc);

		// NOP short cut.
		if ((u & NOP_MASK) == 0) {
			goto next;
		}

		popj = (u >> 42) & 1;
		a_src = (u >> 32) & 01777;
		m_src = (u >> 26) & 077;

		a_src_value = read_a_mem(a_src); // Get A source value.

		// Calculate M source value.
		if (m_src & 040) {
			uint32_t l2_data;
			uint32_t l1_data;

			l1_data = 0;
			l2_data = 0;

			switch (m_src & 037) {
			case 0:
				m_src_value = dispatch_constant;
				break;
			case 1:
				m_src_value = (spc_stack_ptr << 24) | (spc_stack[spc_stack_ptr] & 01777777);
				break;
			case 2:
				m_src_value = pdl_ptr & 01777;
				break;
			case 3:
				m_src_value = pdl_index & 01777;
				break;
			case 5:
				DEBUG(TRACE_MISC, "reading pdl[%o] -> %o\n", pdl_index, read_pdl_mem(USE_PDL_INDEX));
				m_src_value = read_pdl_mem(USE_PDL_INDEX);
				break;
			case 6:
				m_src_value = opc;
				break;
			case 7:
				m_src_value = q;
				break;
			case 010:
				m_src_value = vma;
				break;
			case 011:
				l2_data = map_vtop(md, (int *) &l1_data, (int *) 0);
				m_src_value = ((uint32_t) write_fault_bit << 31) | ((uint32_t) access_fault_bit << 30) | ((l1_data & 037) << 24) | (l2_data & 077777777);
				break;
			case 012:
				m_src_value = md;
				break;
			case 013:
				if (lc_byte_mode_flag)
					m_src_value = lc;
				else
					m_src_value = lc & ~1;
				break;
			case 014:
				m_src_value = (spc_stack_ptr << 24) | (spc_stack[spc_stack_ptr] & 01777777);
				DEBUG(TRACE_MISC, "reading spc[%o] + ptr -> %o\n", spc_stack_ptr, m_src_value);
				spc_stack_ptr = (spc_stack_ptr - 1) & 037;
				break;
			case 024:
				DEBUG(TRACE_MISC, "reading pdl[%o] -> %o, pop\n", pdl_ptr, read_pdl_mem(USE_PDL_PTR));
				m_src_value = read_pdl_mem(USE_PDL_PTR);
				pdl_ptr = (pdl_ptr - 1) & 01777;
				break;
			case 025:
				DEBUG(TRACE_MISC, "reading pdl[%o] -> %o\n", pdl_ptr, read_pdl_mem(USE_PDL_PTR));
				m_src_value = read_pdl_mem(USE_PDL_PTR);
				break;
			}
		} else {
			m_src_value = read_m_mem(m_src);
		}

		// Decode isntruction.
		switch (op_code = (u >> 43) & 03) {
		case 0:		// ALU
			dest = (u >> 14) & 07777;
			out_bus = (u >> 12) & 3;
			carry_in = (u >> 2) & 1;

			aluop = (u >> 3) & 077;

			alu_carry = 0;

			switch (aluop) {
				// Arithmetic.
			case 020:
				alu_out = carry_in ? 0 : -1;
				alu_carry = 0;
				break;
			case 021:
				lv = (int64_t) (m_src_value & a_src_value) - (carry_in ? 0 : 1);
				alu_out = (uint32_t) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 022:
				lv = (int64_t) (m_src_value & ~a_src_value) - (carry_in ? 0 : 1);
				alu_out = (uint32_t) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 023:
				lv = (int64_t) m_src_value - (carry_in ? 0 : 1);
				alu_out = (uint32_t) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 024:
				lv = (int64_t) (m_src_value | ~a_src_value) + (carry_in ? 1 : 0);
				alu_out = (uint32_t) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 025:
				lv = (int64_t) (m_src_value | ~a_src_value) + (m_src_value & a_src_value) + (carry_in ? 1 : 0);
				alu_out = (uint32_t) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 026: // [M-A-1] [SUB]
				sub32(m_src_value, a_src_value, carry_in, alu_out, alu_carry);
				break;
			case 027:
				lv = (int64_t) (m_src_value | ~a_src_value) + m_src_value + (carry_in ? 1 : 0);
				alu_out = (uint32_t) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 030:
				lv = (int64_t) (m_src_value | a_src_value) + (carry_in ? 1 : 0);
				alu_out = (uint32_t) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 031: // [ADD] [M+A+1]
				add32(m_src_value, a_src_value, carry_in, alu_out, alu_carry);
				break;
			case 032:
				lv = (int64_t) (m_src_value | a_src_value) + (m_src_value & ~a_src_value) + (carry_in ? 1 : 0);
				alu_out = (uint32_t) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 033:
				lv = (int64_t) (m_src_value | a_src_value) + m_src_value + (carry_in ? 1 : 0);
				alu_out = (uint32_t) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 034: // [M+1]
				alu_out = m_src_value + (carry_in ? 1 : 0);
				alu_carry = 0;
				if (m_src_value == (int) 0xffffffff && carry_in)
					alu_carry = 1;
				break;
			case 035:
				lv = (int64_t) m_src_value + (m_src_value & a_src_value) + (carry_in ? 1 : 0);
				alu_out = (uint32_t) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 036:
				lv = (int64_t) m_src_value + (m_src_value | ~a_src_value) + (carry_in ? 1 : 0);
				alu_out = (uint32_t) lv;
				alu_carry = (lv >> 32) ? 1 : 0;
				break;
			case 037: // [M+M] [M+M+1]
				add32(m_src_value, m_src_value, carry_in, alu_out, alu_carry);
				break;

				// Boolean.
			case 000: // [SETZ]
				alu_out = 0;
				break;
			case 001: // [AND]
				alu_out = m_src_value & a_src_value;
				break;
			case 002: // [ANDCA]
				alu_out = m_src_value & ~a_src_value;
				break;
			case 003: // [SETM]
				alu_out = m_src_value;
				break;
			case 004: // [ANDCM]
				alu_out = ~m_src_value & a_src_value;
				break;
			case 005: // [SETA]
				alu_out = a_src_value;
				break;
			case 006: // [XOR]
				alu_out = m_src_value ^ a_src_value;
				break;
			case 007: // [IOR]
				alu_out = m_src_value | a_src_value;
				break;
			case 010: // [ANDCB]
				alu_out = ~a_src_value & ~m_src_value;
				break;
			case 011: // [EQV]
				alu_out = a_src_value == m_src_value;
				break;
			case 012: // [SETCA]
				alu_out = ~a_src_value;
				break;
			case 013: // [ORCA]
				alu_out = m_src_value | ~a_src_value;
				break;
			case 014: // [SETCM]
				alu_out = ~m_src_value;
				break;
			case 015: // [ORCM]
				alu_out = ~m_src_value | a_src_value;
				break;
			case 016: // [ORCB]
				alu_out = ~m_src_value | ~a_src_value;
				break;
			case 017: // [SETO]
				alu_out = ~0;
				break;

				// Conditioanl ALU operation.
			case 040: // Multiply step
				do_add = q & 1;
				if (do_add) {
					add32(a_src_value, m_src_value, carry_in, alu_out, alu_carry);
				} else {
					alu_out = m_src_value;
					alu_carry = alu_out & 0x80000000 ? 1 : 0;
				}
				break;
			case 041: // Divide step
				do_sub = q & 1;
				DEBUG(TRACE_MISC, "do_sub %d\n", do_sub);
				if (do_sub) {
					sub32(m_src_value, a_src_value, !carry_in, alu_out, alu_carry);
				} else {
					add32(m_src_value, a_src_value, carry_in, alu_out, alu_carry);
				}
				break;
			case 045: // Remainder correction
				do_sub = q & 1;
				DEBUG(TRACE_MISC, "do_sub %d\n", do_sub);
				if (do_sub) {
					alu_carry = 0;
				} else {
					add32(alu_out, a_src_value, carry_in, alu_out, alu_carry);
				}
				break;
			case 051: // Initial divide step
				DEBUG(TRACE_MISC, "divide-first-step\n");
				DEBUG(TRACE_MISC, "divide: %o / %o \n", q, a_src_value);
				sub32(m_src_value, a_src_value, !carry_in, alu_out, alu_carry);
				DEBUG(TRACE_MISC, "alu_out %08x %o %d\n", alu_out, alu_out, alu_out);
				break;
			}

			// Q control.
			old_q = q;
			switch (u & 3) {
			case 1:
				DEBUG(TRACE_MISC, "q<<\n");
				q <<= 1;
				// Inverse of ALU sign.
				if ((alu_out & 0x80000000) == 0)
					q |= 1;
				break;
			case 2:
				DEBUG(TRACE_MISC, "q>>\n");
				q >>= 1;
				if (alu_out & 1)
					q |= 0x80000000;
				break;
			case 3:
				DEBUG(TRACE_MISC, "q<-alu\n");
				q = alu_out;
				break;
			}

			// Output bus control.
			switch (out_bus) {
			case 0:
				WARNING(TRACE_MISC, "out_bus == 0!\n");
				out_bus = rotate_left(m_src_value, u & 037);
				break;
			case 1:
				out_bus = alu_out;
				break;
			case 2:
				// "ALU output shifted right one, with
				// the correct sign shifted in,
				// regardless of overflow."
				out_bus = (alu_out >> 1) | (alu_carry ? 0x80000000 : 0);
				break;
			case 3:
				out_bus = (alu_out << 1) | ((old_q & 0x80000000) ? 1 : 0);
				break;
			}

			write_dest(dest, out_bus);
			DEBUG(TRACE_MISC, "alu_out 0x%08x, alu_carry %d, q 0x%08x\n", alu_out, alu_carry, q);
			break;
		case 1:		// JUMP
			new_pc = (u >> 12) & 037777;
			DEBUG(TRACE_MISC, "a=%o (%o), m=%o (%o)\n", a_src, a_src_value, m_src, m_src_value);
			r_bit = (u >> 9) & 1;
			p_bit = (u >> 8) & 1;
			n_bit = (u >> 7) & 1;
			invert_sense = (u >> 6) & 1;
			take_jump = 0;

			if (((u >> 10) & 3) == 1) {
				DEBUG(TRACE_MISC, "halted\n");
				run_ucode_flag = 0;
				break;
			}

		process_jump:
			// Jump condition.
			if (u & (1 << 5)) {
				switch (u & 017) {
				case 0:
					if (op_code != 2)
						WARNING(TRACE_MISC, "jump-condition == 0! u_pc=%o\n", p0_pc);
					break;
				case 1:
					take_jump = m_src_value < a_src_value;
					break;
				case 2:
					take_jump = m_src_value <= a_src_value;
					break;
				case 3:
					take_jump = m_src_value == a_src_value;
					break;
				case 4:
					take_jump = page_fault_flag;
					break;
				case 5:
					DEBUG(TRACE_MISC, "jump i|pf\n");
					take_jump = page_fault_flag | (interrupt_enable_flag ? interrupt_pending_flag : 0);
					break;
				case 6:
					DEBUG(TRACE_MISC, "jump i|pf|sb\n");
					take_jump = page_fault_flag | (interrupt_enable_flag ? interrupt_pending_flag : 0) | sequence_break_flag;
					break;
				case 7:
					take_jump = 1;
					break;
				}
			} else {
				rot = u & 037;
				DEBUG(TRACE_MISC, "jump-if-bit; rot %o, before %o ", rot, m_src_value);
				m_src_value = rotate_left(m_src_value, rot);
				DEBUG(TRACE_MISC, "after %o\n", m_src_value);
				take_jump = m_src_value & 1;
			}

			if (((u >> 10) & 3) == 3) {
				WARNING(TRACE_MISC, "jump w/misc-3!\n");
			}

			if (invert_sense)
				take_jump = !take_jump;

			if (p_bit && take_jump) {
				if (!n_bit)
					push_spc(u_pc);
				else
					push_spc(u_pc - 1);
			}
			// P & R & jump-inst -> write ucode.
			if (p_bit && r_bit && op_code == 1) {
				w = ((ucw_t) (a_src_value & 0177777) << 32) | (uint32_t) m_src_value;
				DEBUG(TRACE_MISC, "u-code write; %Lo @ %o\n", w, new_pc);
				ucode[new_pc] = w;
			}
			if (r_bit && take_jump) {
				new_pc = pop_spc();
				if ((new_pc >> 14) & 1) {
					advance_lc(&new_pc);
				}
				new_pc &= 037777;
			}
			if (take_jump) {
				if (n_bit)
					no_exec_next = 1;
				u_pc = new_pc;
				// inhibit possible POPJ.
				popj = 0;
			}
			break;
		case 2:		// DISPATCH.
			disp_const = (u >> 32) & 01777;
			n_plus1 = (u >> 25) & 1;
			enable_ish = (u >> 24) & 1;
			disp_addr = (u >> 12) & 03777;
			map = (u >> 8) & 3;
			len = (u >> 5) & 07;
			pos = u & 037;

			// Misc. function 3.
			if (((u >> 10) & 3) == 3) {
				if (lc_byte_mode_flag) {
					// Byte mode.
					char ir4;
					char ir3;
					char lc1;
					char lc0;

					ir4 = (u >> 4) & 1;
					ir3 = (u >> 3) & 1;
					lc1 = (lc >> 1) & 1;
					lc0 = (lc >> 0) & 1;
					pos = u & 007;
					pos |= ((ir4 ^ (lc1 ^ lc0)) << 4) | ((ir3 ^ lc0) << 3);
					DEBUG(TRACE_MISC, "byte-mode, pos %o\n", pos);
				} else {
					// 16 bit mode.
					char ir4;
					char lc1;

					ir4 = (u >> 4) & 1;
					lc1 = (lc >> 1) & 1;

					pos = u & 017;

					pos |= ((ir4 ^ lc1) ? 0 : 1) << 4;
					DEBUG(TRACE_MISC, "16b-mode, pos %o\n", pos);
				}
			}
			// Misc. function 2.
			if (((u >> 10) & 3) == 2) {
				DEBUG(TRACE_MISC, "dispatch_memory[%o] <- %o\n", disp_addr, a_src_value);
				dispatch_memory[disp_addr] = a_src_value;
				goto dispatch_done;
			}

			DEBUG(TRACE_MISC, "m-src %o, ", m_src_value);
			// Rotate M-SOURCE.
			m_src_value = rotate_left(m_src_value, pos);
			// Generate mask.
			left_mask_index = (len - 1) & 037;

			mask = ~0;
			mask >>= 31 - left_mask_index;

			if (len == 0)
				mask = 0;

			// Put LDB into DISPATCH-ADDR.
			disp_addr |= m_src_value & mask;

			DEBUG(TRACE_MISC, "rotated %o, mask %o, result %o\n", m_src_value, mask, m_src_value & mask);

			// Tweak DISPATCH-ADDR with L2 map bits.
			if (map) {
				int l2_map_bits;
				int bit18;
				int bit19;

				l2_map_bits = map_vtop(md, (int *) 0, (int *) 0);
				bit19 = ((l2_map_bits >> 19) & 1) ? 1 : 0;
				bit18 = ((l2_map_bits >> 18) & 1) ? 1 : 0;
				DEBUG(TRACE_MISC, "md %o, l2_map_bits %o, b19 %o, b18 %o\n", md, l2_map_bits, bit19, bit18);
				switch (map) {
				case 1:
					disp_addr |= bit18;
					break;
				case 2:
					disp_addr |= bit19;
					break;
				case 3:
					disp_addr |= bit18 | bit19;
					break;
				}
			}
			disp_addr &= 03777;

			DEBUG(TRACE_MISC, "dispatch[%o] -> %o ", disp_addr, dispatch_memory[disp_addr]);

			disp_addr = dispatch_memory[disp_addr];
			dispatch_constant = disp_const;

			new_pc = disp_addr & 037777; // 14 bits.

			n_bit = (disp_addr >> 14) & 1;
			p_bit = (disp_addr >> 15) & 1;
			r_bit = (disp_addr >> 16) & 1;

			DEBUG(TRACE_MISC, "%s%s%s\n", n_bit ? "N " : "", p_bit ? "P " : "", r_bit ? "R " : "");

			if (n_plus1 && n_bit) {
				u_pc--;
			}

			invert_sense = 0;
			take_jump = 1;
			u = 1 << 5;

			// Enable instruction sequence hardware.
			if (enable_ish) {
				advance_lc((int *) 0);
			}
			// Fall through on dispatch.
			if (p_bit && r_bit) {
				if (n_bit)
					no_exec_next = 1;
				goto dispatch_done;
			}
			goto process_jump;
		dispatch_done:
			break;
		case 3:		// BYTE.
			dest = (u >> 14) & 07777;
			mr_sr_bits = (u >> 12) & 3;
			DEBUG(TRACE_MISC, "a=%o (%o), m=%o (%o), dest=%o\n", a_src, a_src_value, m_src, m_src_value, dest);
			widthm1 = (u >> 5) & 037;
			pos = u & 037;

			// Misc. function 3.
			if (((u >> 10) & 3) == 3) {
				if (lc_byte_mode_flag) {
					// Byte mode.
					char ir4;
					char ir3;
					char lc1;
					char lc0;

					ir4 = (u >> 4) & 1;
					ir3 = (u >> 3) & 1;
					lc1 = (lc >> 1) & 1;
					lc0 = (lc >> 0) & 1;

					pos = u & 007;
					pos |= ((ir4 ^ (lc1 ^ lc0)) << 4) | ((ir3 ^ lc0) << 3);
					DEBUG(TRACE_MISC, "byte-mode, pos %o\n", pos);
				} else {
					// 16-bit mode.
					char ir4;
					char lc1;

					ir4 = (u >> 4) & 1;
					lc1 = (lc >> 1) & 1;

					pos = u & 017;
					pos |= ((ir4 ^ lc1) ? 0 : 1) << 4;
					DEBUG(TRACE_MISC, "16b-mode, pos %o\n", pos);
				}
			}

			if (mr_sr_bits & 2)
				right_mask_index = pos;
			else
				right_mask_index = 0;

			left_mask_index = (right_mask_index + widthm1) & 037;

			left_mask = ~0;
			right_mask = ~0;

			left_mask >>= 31 - left_mask_index;
			right_mask <<= right_mask_index;

			mask = left_mask & right_mask;

			DEBUG(TRACE_MISC, "widthm1 %o, pos %o, mr_sr_bits %o\n", widthm1, pos, mr_sr_bits);
			DEBUG(TRACE_MISC, "left_mask_index %o, right_mask_index %o\n", left_mask_index, right_mask_index);
			DEBUG(TRACE_MISC, "left_mask %o, right_mask %o, mask %o\n", left_mask, right_mask, mask);

			out_bus = 0;

			switch (mr_sr_bits) {
			case 0:
				WARNING(TRACE_MISC, "mr_sr_bits == 0!\n");
				break;
			case 1:	// LDB.
				DEBUG(TRACE_MISC, "ldb; m %o\n", m_src_value);
				m_src_value = rotate_left(m_src_value, pos);
				out_bus = (m_src_value & mask) | (a_src_value & ~mask);
				DEBUG(TRACE_MISC, "ldb; m-rot %o, mask %o, result %o\n", m_src_value, mask, out_bus);
				break;
			case 2:	// Selective deposit.
				out_bus = (m_src_value & mask) | (a_src_value & ~mask);
				DEBUG(TRACE_MISC, "sel-dep; a %o, m %o, mask %o -> %o\n", a_src_value, m_src_value, mask, out_bus);
				break;
			case 3:	// DPB.
				DEBUG(TRACE_MISC, "dpb; m %o, pos %o\n", m_src_value, pos);
				// Mask is already rotated.
				m_src_value = rotate_left(m_src_value, pos);
				out_bus = (m_src_value & mask) | (a_src_value & ~mask);
				DEBUG(TRACE_MISC, "dpb; mask %o, result %o\n", mask, out_bus);
				break;
			}

			write_dest(dest, out_bus);
			break;
		}

		if (popj) {
			DEBUG(TRACE_MISC, "popj; ");
			u_pc = pop_spc();
			if ((u_pc >> 14) & 1) {
				advance_lc(&u_pc);
			}
			u_pc &= 037777;
		}
	}
}
