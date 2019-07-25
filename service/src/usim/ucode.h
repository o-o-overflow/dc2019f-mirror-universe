#ifndef USIM_UCODE_H
#define USIM_UCODE_H

#include <stdbool.h>
#include <stdint.h>

#define NOP_MASK 03777777777767777LL

typedef uint64_t ucw_t;

extern ucw_t prom_ucode[512];
extern bool run_ucode_flag;

extern int read_prom(char *promfn);

extern void run(void);
extern void dump_state(void);

extern void write_a_mem(int loc, uint32_t v);
extern uint32_t read_a_mem(int loc);

extern void assert_unibus_interrupt(int vector);
extern void deassert_unibus_interrupt(void);

extern void assert_xbus_interrupt(void);
extern void deassert_xbus_interrupt(void);

#endif
