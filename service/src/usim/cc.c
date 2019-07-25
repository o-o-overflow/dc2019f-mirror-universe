// cc --- crude version of CC

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <memory.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>

#include "ucode.h"
#include "disass.h"
#include "cc.h"

#include "lcadrd.h"
#include "lcadmc.h"

extern int yyparse(void);
extern int yylex(void);
extern int yyerror(const char *);
extern FILE *yyin;

static int fd;
static char *file = NULL;

static int verbose;
static bool debug = false;
static bool batch = false;

static uint32_t PC;

static char *serial_devicename = "/dev/ttyUSB1";

size_t
cc_send(const void *b, size_t len)
{
	int ret;

	ret = -1;

	// Send slowly so as not to confuse hardware.
	for (size_t i = 0; i < len; i++) {
		ret = write(fd, b + i, 1);
		if (ret != 1)
			perror("write");
		tcflush(fd, TCOFLUSH);
		usleep(50);
		usleep(2000);
	}

	usleep(10000);

	return ret;
}

static uint16_t
reg_get(int base, int reg)
{
	unsigned char buffer[64];
	unsigned char nibs[4];
	int ret;
	int mask;
	int loops;
	int off;
	uint16_t v;

again:
	buffer[0] = base | (reg & 0x1f);
	if (debug)
		printf("send %02x\n", buffer[0]);
	cc_send(buffer, 1);
	usleep(1000 * 100);

	memset(buffer, 0, 64);
	memset(nibs, 0, 4);
	loops = 0;
	off = 0;
	while (1) {
		ret = read(fd, buffer + off, 64 - off);
		if (ret > 0)
			off += ret;
		if (off == 4)
			break;
		if (ret < 0 && errno == 11) {
			usleep(100);

			loops++;
			if (loops > 5)
				goto again;
			continue;
		}
	}

	if (debug) {
		printf("response %d\n", ret);
		printf("%02x %02x %02x %02x\n", buffer[0], buffer[1], buffer[2], buffer[3]);
	}

	if (off < 4) {
		printf("spy register read failed (ret=%d, off=%d, errno=%d)\n", ret, off, errno);
		return -1;
	}

	// Response should be 0x3x, 0x4x, 0x5x, 0x6x, but hardware can
	// sometimes repeat characters.
	mask = 0;
	for (int i = 0; i < ret; i++) {
		int nib;

		nib = (buffer[i] & 0xf0) >> 4;
		switch (nib) {
		case 3:
			mask |= 8;
			nibs[0] = buffer[i];
			break;
		case 4:
			mask |= 4;
			nibs[1] = buffer[i];
			break;
		case 5:
			mask |= 2;
			nibs[2] = buffer[i];
			break;
		case 6:
			mask |= 1;
			nibs[3] = buffer[i];
			break;
		}
	}

	if (mask == 0xf) {
		if (debug)
			printf("response ok\n");
		v = ((nibs[0] & 0x0f) << 12) | ((nibs[1] & 0x0f) << 8) | ((nibs[2] & 0x0f) << 4) | ((nibs[3] & 0x0f) << 0);
		if (debug)
			printf("reg %o = 0x%04x (0%o)\n", reg, v, v);
		return v;
	}

	return 0;
}

static int
reg_set(int base, int reg, int v)
{
	unsigned char buffer[64];
	int ret;

	if (debug)
		printf("cc_set(r=%d, v=%o)\n", reg, v);

	buffer[0] = 0x30 | ((v >> 12) & 0xf);
	buffer[1] = 0x40 | ((v >> 8) & 0xf);
	buffer[2] = 0x50 | ((v >> 4) & 0xf);
	buffer[3] = 0x60 | ((v >> 0) & 0xf);
	buffer[4] = base | (reg & 0x1f);
	if (debug) {
		printf("writing, fd=%d\n", fd);
		printf("%02x %02x %02x %02x %02x\n", buffer[0], buffer[1], buffer[2], buffer[3], buffer[4]);
	}

	ret = cc_send(buffer, 5);
	if (debug)
		printf("ret %d\n", ret);
	if (ret != 5)
		printf("cc_set: write error%d\n", ret);

	return 0;
}

uint16_t
cc_get(int reg)
{
	return reg_get(0x80, reg);
}

int
cc_set(int reg, int v)
{
	return reg_set(0xa0, reg, v);
}

static uint16_t
mmc_get(int reg)
{
	return reg_get(0xc0, reg);
}

static int
mmc_set(int reg, int v)
{
	return reg_set(0xd0, reg, v);
}

static uint32_t
cc_read_obus_(void)
{
	return spy_read32(025, 024);
}

static uint16_t
cc_read_scratch(void)
{
	return cc_get(SPY_SCRATCH);
}

static uint32_t
bitmask(int wid)
{
	uint32_t m;

	m = 0;

	for (int i = 0; i < wid; i++) {
		m <<= 1;
		m |= 1;
	}
	return m;
}

uint64_t
ir_pair(int field, uint32_t val)
{
	uint64_t ir;
	uint32_t mask;
	int shift;
	int width;

	shift = field >> 6;
	width = field & 077;
	mask = bitmask(width);

	val &= mask;
	ir = ((uint64_t) val) << shift;

	if (debug)
		printf("ir_pair field=%o, shift=%d, width=%d, mask=%x, val=%x\n", field, shift, width, mask, val);

	return ir;
}

static void
cc_write_md_1s(void)
{
	cc_write_md(0xffffffff);
}

static void
cc_write_md_0s(void)
{
	cc_write_md(0x00000000);
}

static void
cc_report_basic_regs(void)
{
	uint32_t A;
	uint32_t M;
	uint32_t PC;
	uint32_t MD;
	uint32_t VMA;

	A = cc_read_a_bus();
	printf("A = %011o (%08x)\n", A, A);

	M = cc_read_m_bus();
	printf("M = %011o (%08x)\n", M, M);

	PC = cc_read_pc();
	printf("PC = %011o (%08x)\n", PC, PC);

	MD = cc_read_md();
	printf("MD = %011o (%08x)\n", MD, MD);

	VMA = cc_read_vma();
	printf("VMA= %011o (%08x)\n", VMA, VMA);

	{
		uint32_t disk;
		uint32_t bd;
		uint32_t mmc;

		disk = cc_get(SPY_DISK);
		bd = cc_get(SPY_BD);
		mmc = bd >> 6;
		bd &= 0x3f;

		printf("disk-state %d (0x%04x)\n", disk, disk);
		printf("bd-state %d (0x%04x)\n", bd, bd);
		printf("mmc-state %d (0x%04x)\n", mmc, mmc);
	}
}

static void
cc_report_pc(uint32_t *ppc)
{
	uint32_t PC;

	PC = cc_read_pc();
	printf("PC = %011o (%08x)\n", PC, PC);
	*ppc = PC;
}

static void
cc_report_pc_and_ir(uint32_t *ppc)
{
	uint32_t PC;
	uint64_t ir;

	PC = cc_read_pc();
	ir = cc_read_ir();
	printf("PC=%011o (%08x) ir=%" PRIu64 " ", PC, PC, ir);
	printf("%s\n", uinst_desc(ir, NULL));
	*ppc = PC;
}

static void
cc_report_pc_md_ir(uint32_t *ppc)
{
	uint32_t PC;
	uint32_t MD;
	uint64_t ir;

	PC = cc_read_pc();
	MD = cc_read_md();
	ir = cc_read_ir();
	printf("PC=%011o MD=%011o (%08x) ir=%" PRIu64 " ", PC, MD, MD, ir);
	printf("%s\n", uinst_desc(ir, NULL));
	*ppc = PC;
}

static void
cc_report_status(void)
{
	uint32_t s;
	uint32_t f1;
	uint32_t f2;

	s = cc_read_status();
	f1 = s >> 16;
	f2 = s & 0xffff;

	printf("flags1: %04x (", f1);
	if (f1 & (1 << 15))
		printf("waiting ");
	if (f1 & (1 << 12))
		printf("promdisable ");
	if (f1 & (1 << 11))
		printf("stathalt ");
	if (f1 & (1 << 10))
		printf("err ");
	if (f1 & (1 << 9))
		printf("ssdone ");
	if (f1 & (1 << 8))
		printf("srun ");
	printf(") ");
	printf("flags2: %04x (", f2);
	if (f2 & (1 << 2))
		printf("jcond ");
	if (f2 & (1 << 3))
		printf("vmaok ");
	if (f2 & (1 << 4))
		printf("nop ");
	printf(") ");
}

static void
cc_pipe(void)
{
	uint64_t isn;

	for (int i = 0; i < 8; i++) {
		printf("addr %o:\n", i);
		isn =
			ir_pair(CONS_IR_M_SRC, i) |
			ir_pair(CONS_IR_ALUF, CONS_ALU_SETM) |
			ir_pair(CONS_IR_OB, CONS_OB_ALU);
		printf("%" PRIu64 " ", isn);
		printf("%s\n", uinst_desc(isn, NULL));

		cc_write_diag_ir(isn);
		cc_noop_debug_clock();
		printf(" obus1 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());

		cc_debug_clock();
		printf(" obus2 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());

		cc_debug_clock();
		printf(" obus3 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());

		cc_clock();
		printf(" obus4 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());

		cc_clock();
		printf(" obus5 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());
	}
}

static void
cc_pipe2(void)
{
	uint64_t isn;
	uint32_t v2;

	for (uint32_t i = 0; i < 8; i++) {
		printf("val %o:\n", i);
		cc_write_md(i);
		v2 = cc_read_md();
		if (v2 != i) {
			printf("cc_pipe2; md readback error (got=%o wanted=%o)\n", v2, i);
		}
		isn =
			ir_pair(CONS_IR_M_SRC, CONS_M_SRC_MD) |
			ir_pair(CONS_IR_ALUF, CONS_ALU_SETM) |
			ir_pair(CONS_IR_OB, CONS_OB_ALU) |
			ir_pair(CONS_IR_M_MEM_DEST, i);
		printf("%" PRIu64 " ", isn);
		printf("%s\n", uinst_desc(isn, NULL));
		cc_write_diag_ir(isn);

		cc_noop_debug_clock();
		printf(" obus1 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());

		cc_debug_clock();
		printf(" obus2 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());

		cc_debug_clock();
		printf(" obus3 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());

		cc_debug_clock();
		printf(" obus4 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());

		cc_debug_clock();
		printf(" obus5 %o %o %o\n", cc_read_obus(), cc_read_obus_(), cc_read_m_bus());
	}
}

static uint64_t setup_map_inst[] = {
	04000000000110003UL, // (alu) SETZ a=0 m=0 m[0] C=0 alu-> Q-R -><none>,m[2]
	00000000000150173UL, // (alu) SETO a=0 m=0 m[0] C=0 alu-> Q-R -><none>,m[3]
	00600101602370010UL, // (byte) a=2 m=m[3] dpb pos=10, width=1 ->a_mem[47]

	04600101446230166UL, // (byte) a=2 m=m[3] dpb pos=26, width=4 ->VMA,write-map ,m[4]
	04600201400270400UL, // (byte) a=4 m=m[3] dpb pos=0, width=11 -><none>,m[5]
	00002340060010050UL, // (alu) SETA a=47 m=0 m[0] C=0 alu-> ->MD ,m[0]
	00600241446030152UL, // (byte) a=5 m=m[3] dpb pos=12, width=4 ->VMA,write-map ,m[0]
	00002365060010310UL, // (alu) M+A [ADD] a=47 m=52 MD C=0 alu-> ->MD ,m[0]
	00600201400270041UL, // (byte) a=4 m=m[3] dpb pos=1, width=2 -><none>,m[5]
	04600241446030444UL, // (byte) a=5 m=m[3] dpb pos=4, width=12 ->VMA,write-map ,m[0]
	00002365060010310UL, // (alu) M+A [ADD] a=47 m=52 MD C=0 alu-> ->MD ,m[0]
	04600201446030000UL, // (byte) a=4 m=m[3] dpb pos=0, width=1 ->VMA,write-map ,m[0]
	0
};

static void
cc_setup_map(void)
{
	for (int i = 0; 1; i++) {
		if (setup_map_inst[i] == 0)
			break;
		printf("%d ", i);
		fflush(stdout);
		cc_execute_r(setup_map_inst[i]);
	}
}

static void
cc_report_ide_regs(void)
{
	printf("setting up map...\n");
	cc_setup_map();

	printf("read ide...\n");
	cc_write_a_mem(2, 01333);
	cc_write_a_mem(3, 0773);
	cc_write_a_mem(4, 0774);
	cc_write_a_mem(5, 0777);

	for (int i = 0; i < 8; i++) {
		uint32_t v;

		cc_write_a_mem(1, i | 020);
		cc_execute_r(0000040060010050UL);	// alu seta a=1 ->md
		cc_execute_r(0000140044010050UL);	// alu seta a=3 alu-> ->vma+write

		cc_execute_w(0000100060010050UL);	// alu seta a=2 ->md
		cc_execute_w(0000200044010050UL);	// alu seta a=4 alu-> ->vma+write

		cc_execute_w(0000240044010050UL);	// alu seta a=5 alu-> ->vma+write

		cc_execute_w(0000140042010050UL);	// alu seta a=3 alu-> ->vma+read */
		v = cc_read_md();
		printf("ide[%d] = 0x%08x 0x%02x\n", i, v, v & 0xff);
	}

	printf("a[1]=%0o\n", cc_read_a_mem(1));
	printf("a[2]=%0o\n", cc_read_a_mem(2));
	printf("a[3]=%0o\n", cc_read_a_mem(3));
	printf("a[4]=%0o\n", cc_read_a_mem(4));
	printf("a[5]=%0o\n", cc_read_a_mem(5));
	printf("a[6]=%0o\n", cc_read_a_mem(6));
}

static int
test_scratch(uint16_t v)
{
	uint16_t s1;
	uint16_t s2;

	s1 = cc_read_scratch();
	cc_set(SPY_SCRATCH, v);
	s2 = cc_read_scratch();

	printf("write 0%o; scratch %o -> %o (0x%x) ", v, s1, s2, s2);
	if (s2 == v) {
		printf("ok\n");
	} else {
		printf("BAD\n");
		s2 = cc_read_scratch();
		printf(" reread; scratch %o -> %o (0x%x) ", s1, s2, s2);
		if (s2 == v) {
			printf("ok\n");
		} else {
			printf("BAD\n");
			s1 = cc_read_scratch();
			cc_set(SPY_SCRATCH, v);
			s2 = cc_read_scratch();

			printf(" rewrite 0%o; scratch %o -> %o (0x%x) ", v, s1, s2, s2);
			if (s2 == v) {
				printf("ok\n");
			} else {
				printf("BAD\n");
				return -1;
			}
		}
	}

	return 0;
}

static int vv = 0;

static void
cc_test_scratch(void)
{
	test_scratch(01234);
	test_scratch(04321);
	test_scratch(0);
	test_scratch(07777);
	test_scratch(0123456);
	test_scratch(0x2222);
	test_scratch(++vv);
}

static int
test_ir(uint64_t isn)
{
	uint64_t iv;

	printf("test ir %" PRIu64 " ", isn);

	cc_write_ir(isn);
	iv = cc_read_ir();
	if (iv == isn) {
		printf("ok\n");
	} else {
		printf("bad (want 0x%" PRIx64 " got 0x%" PRIx64 ")\n", isn, iv);
		printf(" reread; ");
		iv = cc_read_ir();
		if (iv == isn) {
			printf("ok\n");
		} else {
			printf("bad\n");
			printf(" rewrite; ");
			cc_write_ir(isn);
			iv = cc_read_ir();
			if (iv == isn) {
				printf("ok\n");
			} else {
				printf("bad\n");
				return -1;
			}
		}
	}

	return 0;
}

static void
cc_test_ir(void)
{
	test_ir(0);
	test_ir(1);
	test_ir(0x000022220000UL);
	test_ir(0x011133332222UL);
	test_ir(2);
	test_ir(0x011155552222UL);
}

void
oldcmd(int cmd)
{
	uint32_t v;

	switch (cmd) {
	case 'p':
		cc_pipe();
		break;
	case 'q':
		cc_pipe2();
		break;
	case 'c':
		cc_single_step();
		cc_report_status();
		cc_report_pc_and_ir(&PC);
		break;
	case 'S':
		cc_test_scratch();
		break;
	case 'r':
		cc_report_basic_regs();
		break;
	case 'I':
		cc_report_ide_regs();
		break;
	case 'R':
		for (int r = 0; r < 027; r++) {
			uint16_t v;
			v = cc_get(r);
			printf("spy reg %o = %06o (0x%x)\n", r, v, v);
		}
		break;
	case 'n':
		cc_set(SPY_CLK, 2);
		usleep(1000 * 200);
		cc_set(SPY_CLK, 0);
		usleep(1000 * 200);
		break;
	case 'i':
		cc_test_ir();
		break;
	case 'v':
		cc_write_vma(0123456);
		printf("vma=%011o\n", cc_read_vma());
		break;
	case 'd':
		cc_write_md_1s();
		printf("write md ones MD=%011o\n", cc_read_md());

		cc_write_md_0s();
		printf("write md zeros MD=%011o\n", cc_read_md());

		cc_write_md(01234567);
		printf("write md 01234567 MD=%011o\n", cc_read_md());

		cc_write_md(07654321);
		printf("write md 07654321 MD=%011o\n", cc_read_md());

		cc_write_vma(0);
		printf("write vma 0 VMA=%011o\n", cc_read_vma());

		cc_write_vma(01234567);
		printf("write vma 01234567 VMA=%011o\n", cc_read_vma());
		break;
	case 'm':
		cc_write_a_mem(2, 0);
		cc_execute_r(04000100042310050ULL); // (alu) SETA a=2 m=0 m[0] C=0 alu-> ->VMA,start-read ,m[6]
		v = cc_read_md();
		printf("@0 MD=%011o (0x%x)\n", v, v);
		break;
	case 'G':
		for (int i = 0; i < 4; i++) {
			cc_write_a_mem(2, i);
			verbose = 1;
			cc_execute_r(04000100042310050ULL); // (alu) SETA a=2 m=0 m[0] C=0 alu-> ->VMA,start-read ,m[6]
			verbose = 0;
			v = cc_read_md();
			printf("@%o MD=%011o (0x%x)\n", i, v, v);
		}
		for (int i = 0776; i < 01000; i++) {
			cc_write_a_mem(2, i);
			cc_execute_r(04000100042310050ULL); // (alu) SETA a=2 m=0 m[0] C=0 alu-> ->VMA,start-read ,m[6]
			v = cc_read_md();
			printf("@%o MD=%011o (0x%x)\n", i, v, v);
		}
		break;
	case 'a':
		cc_execute_w(04600101442330007ULL); // (byte) a=2 m=m[3] dpb pos=7, width=1 ->VMA,start-read ,m[6]
		printf("@200 MD=%011o\n", cc_read_md());

		cc_execute_w(00002003042310310ULL); // (alu) M+A [ADD] a=40 m=6 m[6] C=0 alu-> ->VMA,start-read ,m[6]
		printf("@201 MD=%011o\n", cc_read_md());

		cc_execute_r(00002003000310310ULL); // (alu) M+A [ADD] a=40 m=6 m[6] C=0 alu-> -><none>,m[6]
		cc_execute_w(00000003042010030ULL); // (alu) SETM a=0 m=6 m[6] C=0 alu-> ->VMA,start-read ,m[0]
		printf("@202 MD=%011o\n", cc_read_md());

		printf("VMA= %011o\n", cc_read_vma());
		break;
	case 't':
		cc_write_a_mem(1, 01234567);
		cc_write_a_mem(2, 07654321);
		printf("A[0] = %011o\n", cc_read_a_mem(0));
		printf("A[1] = %011o\n", cc_read_a_mem(1));
		printf("A[2] = %011o\n", cc_read_a_mem(2));
		printf("A[3] = %011o\n", cc_read_a_mem(3));
		break;
	case '1':
		printf("%04x\n", mmc_get(0));
		printf("%04x\n", mmc_get(1));
		break;
	case '2':
		mmc_set(0, 0x0 | (1 << 2));
		break;
	case '3':
		mmc_set(0, 0x1 | (1 << 2));
		printf("%04x\n", mmc_get(0));
		printf("%04x\n", mmc_get(0));
		printf("%04x\n", mmc_get(0));
		printf("%04x\n", mmc_get(0));
		printf("%04x\n", mmc_get(0));
		break;
	case '4':
		mmc_set(0, 1 << 3);
		printf("%04x\n", mmc_get(0));
		printf("%04x\n", mmc_get(0));
		printf("%04x\n", mmc_get(0));
		break;
	}
}

void
cmd_start(int q)
{
	if (q != 105) {		///---!!! Value should be octal!
		printf("FOOBAR??\n");
		return;
	}

	printf("; start\n");
	cc_start_mach();
}

void
cmd_stop(void)
{
	printf("; stop\n");
	cc_stop_mach();
	cc_report_status();
	printf("\n");
	cc_report_basic_regs();
}

void
cmd_reset(void)
{
	printf("; reset\n");
	cc_set(SPY_MODE, 0000);
	cc_set(SPY_MODE, 0301);
	cc_set(SPY_MODE, 0001);
}

void
cmd_step_once(void)
{
	printf("; step\n");
	cc_clock();
	cc_report_status();
	cc_report_pc(&PC);
}

void
cmd_step_until(uint32_t n)
{
	printf("; step n (%d) times, n < 40000...\n", n);
	for (uint32_t i = 0; i < n; i++) {
		cc_clock();
		cc_report_status();
		cc_report_pc(&PC);
	}
}

void
cmd_step_until_adr(uint32_t adr)
{
	printf("; step until about to execute micro instr at adr (%d)\n", adr);
	printf("run until PC=%o\n", adr);
	while (1) {
		cc_clock();
		cc_report_pc_md_ir(&PC);
		if (PC == adr)
			break;
	}
}

void
cmd_read_m_mem(int adr)
{
	printf("; M memory (%d)\n", adr);
	for (int r = 0; r < 010; r++) {
		uint32_t v;

		v = cc_read_m_mem(r);
		printf("M[%o] = %011o (0x%x)\n", r, v, v);
	}
}

void
cmd_read_a_mem(int adr)
{
	printf("; A memory (%d)\n", adr);
	for (int r = 0; r < 010; r++) {
		uint32_t v;

		v = cc_read_a_mem(r);
		printf("A[%o] = %011o (0x%x)\n", r, v, v);
	}
}

void
cmd_prompt(void)
{
	if (batch == false)
		printf("(cc) ");
}

static void
usage(void)
{
	fprintf(stderr, "usage: cc [OPTION]... [DEVICE]\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  -d             print extra debug output\n");
	fprintf(stderr, "  -F FILE        input file\n");
	fprintf(stderr, "  -h             help message\n");
}

int
main(int argc, char **argv)
{
	int c;
	FILE *f = NULL;

	yyin = stdin;

	while ((c = getopt(argc, argv, "hF:")) != -1) {
		switch (c) {
		case 'd':
			debug = true;
			break;
		case 'F':
			file = strdup(optarg);
			break;
		case 'h':
			usage();
			exit(0);
		default:
			continue;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc > 0)
		serial_devicename = strdup(argv[0]);
	printf("opening %s\n", serial_devicename);
	fd = open(serial_devicename, O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		perror(serial_devicename);
		exit(1);
	}

	if (file != NULL) {
		f = fopen(file, "r");
		if (!f) {
			perror("read");
			exit(1);
		}

		batch = true;
		yyin = f;
	}

	cmd_prompt();
	do {
		yyparse();
	} while (!feof(yyin));

	close(fd);
	if (!f)
		fclose(f);

	exit(0);
}
