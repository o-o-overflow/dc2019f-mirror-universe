// lod --- dump a load band (LOD) file

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "usim.h"
#include "disass.h"
#include "misc.h"

static int show_comm;
static int show_scratch;
static int show_initial_fef;
static int show_fef;
static int show_initial_sg;
static int show_memory;
static int width = 24;

static int lodfd;

static int bnum = -1;
static unsigned int bbuf[256] = { 0 };

static struct {
	char *name;
	uint32_t a;
	uint32_t v;
} cv[] = {
	{"A-V-RESIDENT-SYMBOL-AREA", 0, 0},
	{"A-V-SYSTEM-COMMUNICATION-AREA", 0, 0},
	{"A-V-SCRATCH-PAD-INIT-AREA", 0, 0},
	{"A-V-MICRO-CODE-SYMBOL-AREA", 0, 0},
	{"A-V-REGION-ORIGIN", 0, 0},
	{"A-V-REGION-LENGTH", 0, 0},
	{"A-V-REGION-BITS", 0, 0},
	{"A-V-REGION-FREE-POINTER", 0, 0},
	{"A-V-PAGE-TABLE-AREA", 0, 0},
	{"A-V-PHYSICAL-PAGE-DATA", 0, 0},
	{"A-V-ADDRESS-SPACE-MAP", 0, 0},
	{"A-V-REGION-GC-POINTER", 0, 0},
	{"A-V-REGION-LIST-THREAD", 0, 0},
	{"A-V-AREA-NAME", 0, 0},
	{"A-V-AREA-REGION-LIST", 0, 0},
	{"A-V-AREA-REGION-BITS", 0, 0},
	{"A-V-AREA-REGION-SIZE", 0, 0},
	{"A-V-AREA-MAXIMUM-SIZE", 0, 0},
	{"A-V-SUPPORT-ENTRY-VECTOR", 0, 0},
	{"A-V-CONSTANTS-AREA", 0, 0},
	{"A-V-EXTRA-PDL-AREA", 0, 0},
	{"A-V-MICRO-CODE-ENTRY-AREA", 0, 0},
	{"A-V-MICRO-CODE-ENTRY-NAME-AREA", 0, 0},
	{"A-V-MICRO-CODE-ENTRY-ARGS-INFO-AREA", 0, 0},
	{"A-V-MICRO-CODE-ENTRY-MAX-PDL-USAGE", 0, 0},
	{"A-V-MICRO-CODE-PAGING-AREA", 0, 0},
	{"A-V-PAGE-GC-BITS", 0, 0},
	{"A-V-MICRO-CODE-ENTRY-ARGLIST-AREA", 0, 0},
	{"A-V-MICRO-CODE-SYMBOL-NAME-AREA", 0, 0},
	{"A-V-LINEAR-PDL-AREA", 0, 0},
	{"A-V-LINEAR-BIND-PDL-AREA", 0, 0},
	{"A-V-INIT-LIST-AREA", 0, 0},
	{"A-V-FIRST-UNFIXED-AREA", 0, 0},
	{(char *) 0, 0, 0}
};

static struct {
	char *name;
	uint32_t a;
	uint32_t v;
} sv[] = {
	{"A-INITIAL-FEF", 0, 0},
	{"A-QTRSTKG", 0, 0},
	{"A-QCSTKG", 0, 0},
	{"A-QISTKG", 0, 0},
	{(char *) 0, 0, 0}
};

uint32_t
read_virt(int addr)
{
	int b;
	off_t offset;

	addr &= 077777777;
	b = addr / 256;

	offset = b * BLOCKSZ;

	if (b != bnum) {
		off_t ret;

		bnum = b;
		ret = lseek(lodfd, offset, SEEK_SET);
		if (ret != offset) {
			perror("seek");
		}

		ret = read(lodfd, bbuf, BLOCKSZ);
		if (ret != BLOCKSZ) {
			perror("read");
		}
	}

	return bbuf[addr % 256];
}

static uint32_t
show(int a, int cr)
{
	uint32_t v;

	v = read_virt(a);
	printf("%011o %011o (0x%08x)", a, v, v);
	if (cr)
		printf("\n");

	return v;
}

static uint32_t
showlabel(char *l, int a, int cr)
{
	unsigned int v;

	printf("%s: ", l);
	v = show(a, cr);

	return v;
}

static void
showstr(int a)
{
	int t;
	int j;
	char s[256];

	t = read_virt(a) & 0xff;
	j = 0;
	for (int i = 0; i < t; i += 4) {
		uint32_t n;

		n = read_virt(a + 1 + (i / 4));
		s[j++] = n >> 0;
		s[j++] = n >> 8;
		s[j++] = n >> 16;
		s[j++] = n >> 24;
	}
	s[t] = 0;
	printf("'%s' ", s);
}

static int
find_and_dump_fef(uint32_t pc, int width)
{
	uint32_t addr;
	uint32_t v;
	uint32_t n;
	uint32_t o;
	int j;
	int tag;
	int icount;
	uint32_t max;
	unsigned short ib[512];

	printf("\n");
	addr = pc >> 2;
	printf("pc %o, addr %o\n", pc, addr);

	// Find FEF.
	tag = -1;
	for (int i = 0; i < 512; i--) {
		n = read_virt(addr);
		tag = (n >> width) & 037;
		if (tag == 7)
			break;
		addr--;
	}

	if (tag != 7) {
		printf("couldn't not find FEF\n");
		return -1;
	}

	n = read_virt(addr);
	o = n & 0777;
	printf("code offset %o\n", o);

	max = read_virt(addr + 1) & 07777;
	icount = (max - o / 2) * 2;

	j = 0;
	for (uint32_t i = 0; i < max; i++) {
		uint32_t loc;
		uint32_t inst;

		loc = addr + i;
		inst = read_virt(loc);
		ib[j++] = inst;
		ib[j++] = inst >> 16;

		if (i < o / 2) {
			show(loc, 1);
		}

		switch (i) {
		case 1:
			break;
		case 2:
			printf(" ");
			v = show(inst, 0);
			tag = (v >> width) & 037;
			switch (tag) {
			case 3:
				printf("\n");
				printf(" ");
				v = show(v, 0);
				tag = (v >> 24) & 037;
				break;
			case 4:
				printf(" ");
				showstr(v);
				printf("\n");
				break;
			}
		}
	}

	printf("\n");

	for (uint32_t i = o; i < o + icount; i++) {
		uint32_t loc;

		loc = addr + i / 2;
		printf("%s\n", disassemble_instruction(addr, loc, ib[i], ib[i]));
	}

	return 0;
}

static void
usage(void)
{
	fprintf(stderr, "usage: lod FILE [OPTION]... FILE\n");
	fprintf(stderr, "\n");
	fprintf(stderr, "  -A             dump everything\n");
	fprintf(stderr, "  -c             dump system communication area\n");
	fprintf(stderr, "  -s             dump scratch-pad area\n");
	fprintf(stderr, "  -f             find and disassemble initial FEF\n");
	fprintf(stderr, "  -g             dump initial stack group\n");
	fprintf(stderr, "  -p PC          find and disassemble FEF for given pc\n");
	fprintf(stderr, "  -a ADDR        find and disassemble FEF for given address\n");
	fprintf(stderr, "  -m ADDR        dump memory\n");
	fprintf(stderr, "  -w             decode 25-bit pointers\n");
	fprintf(stderr, "  -h             help message\n");
}

int
main(int argc, char *argv[])
{
	uint32_t com;
	int c;
	uint32_t pc = 0;
	uint32_t addr = 0;

	while ((c = getopt(argc, argv, "Acsfgp:a:m:wh")) != -1) {
		switch (c) {
		case 'A':
			show_comm++;
			show_scratch++;
			show_initial_fef++;
			show_initial_sg++;
			break;
		case 'c':
			show_comm++;
			break;
		case 's':
			show_scratch++;
			break;
		case 'f':
			show_initial_fef++;
			break;
		case 'g':
			show_initial_sg++;
			break;
		case 'p':
			sscanf(optarg, "%o", &pc);
			show_fef++;
			break;
		case 'a':
			sscanf(optarg, "%o", &addr);
			pc = addr * 4;
			show_fef++;
			break;
		case 'm':
			sscanf(optarg, "%o", &addr);
			show_memory++;
			break;
		case 'w':
			width = 25;
			break;
		case 'h':
			usage();
			exit(0);
		default:
			usage();
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		usage();
		exit(1);
	}

	lodfd = open(argv[0], O_RDONLY);
	if (lodfd < 0) {
		perror(argv[0]);
		exit(1);
	}

	com = showlabel("%SYS-COM-AREA-ORIGIN-PNTR", 0400, 1);
	showlabel("%SYS-COM-BAND-FORMAT", 0410, 1);
	showlabel("%SYS-COM-POINTER-WIDTH", 0432, 1);
	showlabel("%SYS-COM-MAJOR-VERSION", 0427, 1);
	showlabel("%SYS-COM-DESIRED-MICROCODE-VERSION", 0430, 1);

	if (show_comm) {
		printf("\nsystem communication area:\n");
		for (int i = 0; cv[i].name; i++) {
			printf("%s ", cv[i].name);
			cv[i].a = com + i;
			cv[i].v = show(cv[i].a, 0);
			printf("; ");
			show(cv[i].v, 1);
		}
	}

	if (show_scratch) {
		printf("\nscratch-pad:\n");
		for (int i = 0; sv[i].name; i++) {
			sv[i].a = 01000 + i;
			sv[i].v = showlabel(sv[i].name, sv[i].a, 0);
			printf("; ");
			show(sv[i].v, 1);
		}
	}

	if (show_initial_fef) {
		uint32_t v;

		printf("\ninitial fef:\n");

		sv[0].a = 01000 + 0;
		sv[0].v = showlabel(sv[0].name, sv[0].a, 0);
		printf("; ");
		v = show(sv[0].v, 1);
		find_and_dump_fef(v << 2, width);
	}

	if (show_fef) {
		printf("\nfef @ %o:\n", pc);
		find_and_dump_fef(pc, width);
	}

	if (show_initial_sg) {
		uint32_t a;

		printf("\ninitial sg:\n");

		sv[3].a = 01000 + 3;
		sv[3].v = showlabel(sv[3].name, sv[3].a, 1);
		printf("\n");

		a = sv[3].v & 0x00ffffff;

		for (int i = 10; i >= 0; i--) {
			char b[16];

			sprintf(b, "%d", -i);
			show(a - i, 1);
		}
	}

	if (show_memory) {
		printf("memory @ %o:\n", addr);
		for (int i = 0; i < 10; i++) {
			show(addr + i, 1);
		}
	}

	exit(0);
}
