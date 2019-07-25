#ifndef USIM_MEM_H
#define USIM_MEM_H

struct page_s {
	uint32_t w[256];
};

extern struct page_s *phy_pages[16 * 1024];
extern int phys_ram_pages;

extern int l1_map[2048];
extern int l2_map[1024];
extern uint32_t last_virt;
extern uint32_t last_l1;
extern uint32_t last_l2;

extern void invalidate_vtop_cache(void);

extern uint32_t map_vtop(uint32_t virt, int *pl1_map, int *poffset);

extern int write_phy_mem(int paddr, uint32_t v);
extern int add_new_page_no(int pn);
extern int read_phy_mem(int paddr, uint32_t *pv);

extern int restore_state(char *fn);
extern int save_state(char *fn);

#endif
