// disk.c -- emulate a Trident disk
//
// Each disk block contains one Lisp Machine page worth of data,
// i.e. 256. words or 1024. bytes.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <err.h>

#include "usim.h"
#include "ucfg.h"
#include "utrace.h"
#include "ucode.h"
#include "mem.h"
#include "misc.h"

#include "syms.h"

#define LABEL_LABL 011420440514ULL
#define LABEL_BLANK 020020020020ULL

#define DISKS_MAX 8

struct {
	int fd;
	uint8_t *mm;

	int cyls;
	int heads;
	int blocks_per_track;
} disks[DISKS_MAX];

static int disk_status = 1;
static int disk_cmd;
static int disk_clp;
static int disk_ma;
static int disk_ecc;
static int disk_da;

static int cur_unit;
static int cur_cyl;
static int cur_head;
static int cur_block;

static int disk_interrupt_delay;

static int
disk_read(int unit, int block_no, uint32_t *buffer)
{
	off_t offset;

	unit = 0;		///---!!!

	offset = block_no * BLOCKSZ;

	DEBUG(TRACE_DISK, "disk: file image block %d(10), offset %ld(10)\n", block_no, (long) offset);

	memcpy(buffer, disks[unit].mm + offset, BLOCKSZ);

	return 0;
}

static int
disk_write(int unit, int block_no, uint32_t *buffer)
{
	off_t offset;

	unit = 0;		///---!!!

	offset = block_no * BLOCKSZ;

	DEBUG(TRACE_DISK, "disk: file image block %d, offset %ld\n", block_no, (long) offset);

	memcpy(disks[unit].mm + offset, buffer, BLOCKSZ);

	return 0;
}

static void
disk_read_block(uint32_t vma, int unit, int cyl, int head, int block)
{
	int block_no;
	uint32_t buffer[256];

	block_no = (cyl * disks[unit].blocks_per_track * disks[unit].heads) + (head * disks[unit].blocks_per_track) + block;
	if (disk_read(unit, block_no, buffer) < 0) {
		ERR(TRACE_DISK, "disk_read_block: error reading block_no %d\n", block_no);
		return;
	}

	for (int i = 0; i < 256; i++) {
		write_phy_mem(vma + i, buffer[i]);
	}
}

static void
disk_write_block(uint32_t vma, int unit, int cyl, int head, int block)
{
	int block_no;
	uint32_t buffer[256];

	block_no = (cyl * disks[unit].blocks_per_track * disks[unit].heads) + (head * disks[unit].blocks_per_track) + block;

	for (int i = 0; i < 256; i++) {
		read_phy_mem(vma + i, &buffer[i]);
	}

	disk_write(unit, block_no, buffer);
}

static void
disk_throw_interrupt(void)
{
	DEBUG(TRACE_DISK, "disk: throw interrupt\n");
	disk_status |= 1 << 3;
	assert_xbus_interrupt();
}

static void
disk_future_interrupt(void)
{
	disk_interrupt_delay = 100;
	disk_interrupt_delay = 2500;
}

static void
disk_show_cur_addr(void)
{
	DEBUG(TRACE_DISK, "disk: unit %d, CHB %o/%o/%o\n", cur_unit, cur_cyl, cur_head, cur_block);
}

static void
disk_decode_addr(void)
{
	cur_unit = (disk_da >> 28) & 07;
	cur_cyl = (disk_da >> 16) & 07777;
	cur_head = (disk_da >> 8) & 0377;
	cur_block = disk_da & 0377;
}

static void
disk_undecode_addr(void)
{
	disk_da =
		((cur_unit & 07) << 28) |
		((cur_cyl & 07777) << 16) |
		((cur_head & 0377) << 8) |
		((cur_block & 0377));
}

static void
disk_incr_block(void)
{
	int unit;

	unit = 0;		///---!!!

	cur_block++;
	if (cur_block >= disks[unit].blocks_per_track) {
		cur_block = 0;
		cur_head++;
		if (cur_head >= disks[unit].heads) {
			cur_head = 0;
			cur_cyl++;
		}
	}
}

static void
disk_ccw(void (*disk_fn)(uint32_t vma, int unit, int cyl, int head, int block))
{
	uint32_t ccw;
	uint32_t vma;

	disk_decode_addr();

	// Process CCW's.
	for (int i = 0; i < 65535; i++) {
		int f;

		f = read_phy_mem(disk_clp, &ccw);
		if (f) {
			// Huh. what to do now?
			ERR(TRACE_DISK, "disk: mem[clp=%o] yielded fault (no page)\n", disk_clp);
			return;
		}

		DEBUG(TRACE_DISK, "disk: mem[clp=%o] -> ccw %08o\n", disk_clp, ccw);

		vma = ccw & ~0377;
		disk_ma = vma;

		disk_show_cur_addr();

		(*disk_fn)(vma, cur_unit, cur_cyl, cur_head, cur_block);

		if ((ccw & 1) == 0) {
			DEBUG(TRACE_DISK, "disk: last ccw\n");
			break;
		}

		disk_incr_block();

		disk_clp++;
	}

	disk_undecode_addr();

	if (disk_cmd & 04000) {
		disk_future_interrupt();
	}
}

static void
disk_start_read(void)
{
	disk_ccw(&disk_read_block);
}

static void
disk_start_read_compare(void)
{
	DEBUG(TRACE_DISK, "disk_start_read_compare!\n");
	disk_decode_addr();
	disk_show_cur_addr();
}

static void
disk_start_write(void)
{
	disk_ccw(&disk_write_block);
}

static int
disk_start(void)
{
	DEBUG(TRACE_DISK, "disk: start, cmd (%o) ", disk_cmd);

	switch (disk_cmd & 01777) {
	case 0:
		DEBUG(TRACE_DISK, "read\n");
		disk_start_read();
		break;
	case 010:
		DEBUG(TRACE_DISK, "read compare\n");
		disk_start_read_compare();
		break;
	case 011:
		DEBUG(TRACE_DISK, "write\n");
		disk_start_write();
		break;
	case 01005:
		DEBUG(TRACE_DISK, "recalibrate\n");
		break;
	case 0405:
		DEBUG(TRACE_DISK, "fault clear\n");
		break;
	default:
		DEBUG(TRACE_DISK, "unknown\n");
		return -1;
	}

	return 0;
}

void
disk_xbus_read(int offset, uint32_t *pv)
{
	DEBUG(TRACE_MISC, "disk register read, offset %o\n", offset);

	switch (offset) {
	case 0370:
		DEBUG(TRACE_MISC, "disk: read status\n");
		*pv = disk_status;
		break;
	case 0371:
		DEBUG(TRACE_MISC, "disk: read ma\n");
		*pv = disk_ma;
		break;
	case 0372:
		DEBUG(TRACE_MISC, "disk: read da\n");
		*pv = disk_da;
		break;
	case 0373:
		DEBUG(TRACE_MISC, "disk: read ecc\n");
		*pv = disk_ecc;
		break;
	case 0374:
		DEBUG(TRACE_MISC, "disk: status read\n");
		*pv = disk_status;
		break;
	case 0375:
		*pv = disk_clp;
		break;
	case 0376:
		*pv = disk_da;
		break;
	case 0377:
		*pv = 0;
		break;
	default:
		DEBUG(TRACE_DISK, "disk: unknown reg read %o\n", offset);
		break;
	}
}

void
disk_xbus_write(int offset, uint32_t v)
{
	DEBUG(TRACE_MISC, "disk register write, offset %o <- %o\n", offset, v);

	switch (offset) {
	case 0370:
		DEBUG(TRACE_DISK, "disk: load status %o\n", v);
		break;
	case 0374:
		disk_cmd = v;
		if ((disk_cmd & 06000) == 0)
			deassert_xbus_interrupt();
		DEBUG(TRACE_DISK, "disk: load cmd %o\n", v);
		break;
	case 0375:
		DEBUG(TRACE_DISK, "disk: load clp %o (phys page %o)\n", v, v << 8);
		disk_clp = v;
		break;
	case 0376:
		disk_da = v;
		DEBUG(TRACE_MISC, "disk: load da %o\n", v);
		break;
	case 0377:
		disk_start();
		break;
	default:
		DEBUG(TRACE_DISK, "disk: unknown reg write %o\n", offset);
		break;
	}
}

void
disk_poll(void)
{
	if (disk_interrupt_delay) {
		if (--disk_interrupt_delay == 0) {
			disk_throw_interrupt();
		}
	}
}

int
disk_init(int unit, char *filename)
{
	uint32_t label[256];
	int ret;

	label[0] = 0;

	if (unit >= DISKS_MAX)
		errx(1, "disk: only 8 disk devices are supported");

	INFO(TRACE_DISK, "disk: opening %s\n", filename);

	disks[unit].fd = open(filename, O_RDWR | O_BINARY);
	if (disks[unit].fd < 0) {
		disks[unit].fd = 0;
		perror(filename);
		exit(1);
	}

	struct stat st;
	fstat(disks[unit].fd, &st);
	INFO(TRACE_DISK, "disk: size: %zd bytes\n", st.st_size);
	disks[unit].mm = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, disks[unit].fd, 0);

	ret = disk_read(unit, 0, label);
	if (ret < 0 || label[0] != LABEL_LABL) {
		WARNING(TRACE_DISK, "disk: invalid pack label (%o) - disk image ignored\n", label[0]);
		close(disks[unit].fd);
		disks[unit].fd = 0;
		return -1;
	}

	disks[unit].cyls = label[2];
	disks[unit].heads = label[3];
	disks[unit].blocks_per_track = label[4];

	INFO(TRACE_DISK, "disk: image CHB %o/%o/%o\n", disks[unit].cyls, disks[unit].heads, disks[unit].blocks_per_track);

	return 0;
}
