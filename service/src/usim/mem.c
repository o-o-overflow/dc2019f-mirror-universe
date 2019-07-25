// mem.c --- CADR memory routines

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "usim.h"
#include "utrace.h"
#include "ucode.h"
#include "mem.h"

int phys_ram_pages = 8192;	// 2 MW.

struct page_s *phy_pages[16 * 1024];

int l1_map[2048];
int l2_map[1024];

uint32_t last_virt = 0xffffff00;
uint32_t last_l1;
uint32_t last_l2;

void
invalidate_vtop_cache(void)
{
	last_virt = 0xffffff00;
}

// Map virtual address to physical address, possibly returning l1
// mapping and possibly returning offset into page.
uint32_t
map_vtop(uint32_t virt, int *pl1_map, int *poffset)
{
	int l1_index;
	int l2_index;
	int l1;
	uint32_t l2;

	virt &= 077777777;	// 24 bit address.

	// Frame buffer.
	if ((virt & 077700000) == 077000000) {
		if (virt >= 077051757 && virt <= 077051763) {
			DEBUG(TRACE_MISC, "disk run light\n");
		}
		if (poffset)
			*poffset = virt & 0377;
		return (1 << 22) | (1 << 23) | 036000;
	}

	// Color.
	if ((virt & 077700000) == 077200000) {
		if (poffset)
			*poffset = virt & 0377;
		return (1 << 22) | (1 << 23) | 036000;
	}

	// This should be moved below - I'm not sure it has to happen
	// anymore.
	if ((virt & 077777400) == 077377400) {
		if (poffset)
			*poffset = virt & 0377;
		return (1 << 22) | (1 << 23) | 036777;
	}

	// 11 bit L1 index.
	l1_index = (virt >> 13) & 03777;
	l1 = l1_map[l1_index] & 037;
	if (pl1_map)
		*pl1_map = l1;

	// 10 bit L2 index.
	l2_index = (l1 << 5) | ((virt >> 8) & 037);
	l2 = l2_map[l2_index];

	if (poffset)
		*poffset = virt & 0377;

	last_virt = virt & 0xffffff00;
	last_l1 = l1;
	last_l2 = l2;

	return l2;
}

static char *page_block;
static int page_block_count;

static struct page_s *
new_page(void)
{
	struct page_s *page;

	if (page_block == 0) {
		page_block = malloc(sizeof(struct page_s) * 1024);
		page_block_count = 1024;
	}

	page = (struct page_s *) page_block;

	page_block += sizeof(struct page_s);
	page_block_count--;

	if (page_block_count == 0)
		page_block = 0;

	return page;
}

// Add a new physical memory page, generally in response to L2 mapping
// (but can also be due to physical access to RAM).
int
add_new_page_no(int pn)
{
	struct page_s *page;

	page = phy_pages[pn];
	if (page == 0) {
		page = new_page();
		if (page) {
			phy_pages[pn] = page;
			return 0;
		}
	}
	return -1;
}

// Read physical memory, with no virtual-to-physical mapping (used by
// disk controller).
int
read_phy_mem(int paddr, uint32_t *pv)
{
	int pn;
	int offset;
	struct page_s *page;

	pn = paddr >> 8;
	offset = paddr & 0377;

	page = phy_pages[pn];
	if (page == 0) {
		// Page does not exist.
		if (pn < phys_ram_pages) {
			DEBUG(TRACE_MISC, "[read_phy_mem] adding phy ram page %o (address %o)\n", pn, paddr);
			add_new_page_no(pn);
			page = phy_pages[pn];
		} else {
			DEBUG(TRACE_MISC, "[read_phy_mem] address %o does not exist\n", paddr);
			return -1;
		}
	}

	*pv = page->w[offset];

	return 0;
}

int
write_phy_mem(int paddr, uint32_t v)
{
	int pn;
	int offset;
	struct page_s *page;

	pn = paddr >> 8;
	offset = paddr & 0377;

	page = phy_pages[pn];
	if (page == 0) {
		// Page does not exist - add it (probably result of
		// disk write).
		if (pn < phys_ram_pages) {
			DEBUG(TRACE_MISC, "[write_phy_mem] adding phy ram page %o (address %o)\n", pn, paddr);
			add_new_page_no(pn);
			page = phy_pages[pn];
		} else {
			DEBUG(TRACE_MISC, "[write_phy_mem] address %o does not exist\n", paddr);
			return -1;
		}
	}

	page->w[offset] = v;

	return 0;
}

#define PAGES_TO_SAVE 8192

static bool restored = false;

int
restore_state(char *fn)
{
	int fd;

	if (restored == true)
		return 0;
	restored = true;

	fd = open(fn, O_RDONLY);
	if (fd < 0)
		return -1;

	for (int i = 0; i < PAGES_TO_SAVE; i++) {
		ssize_t ret;

		add_new_page_no(i);

		ret = read(fd, (char *) phy_pages[i], sizeof(struct page_s));
		if (ret < 0) {
			close(fd);
			return -1;
		}
	}

	close(fd);

	return 0;
}

int
save_state(char *fn)
{
	int fd;

	fd = open(fn, O_RDWR | O_CREAT, 0666);
	if (fd < 0)
		return -1;

	for (int i = 0; i < PAGES_TO_SAVE; i++) {
		ssize_t ret;

		ret = write(fd, (char *) phy_pages[i], sizeof(struct page_s));
		if (ret < 0) {
			close(fd);
			return -1;
		}
	}

	close(fd);

	return 0;
}
