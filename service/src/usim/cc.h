#ifndef USIM_NCC_H
#define USIM_NCC_H

#include <stdint.h>

extern int cc_set(int reg, int v);
extern uint16_t cc_get(int reg);
extern size_t cc_send(const void *b, size_t len);
extern uint64_t ir_pair(int field, uint32_t val);

extern void cmd_prompt(void);

extern void cmd_start(int q);
extern void cmd_stop(void);
extern void cmd_reset(void);
extern void cmd_step_once(void);
extern void cmd_step_until(uint32_t n);
extern void cmd_step_until_adr(uint32_t adr);
extern void cmd_read_m_mem(int adr);
extern void cmd_read_a_mem(int adr);

extern void oldcmd(int cmd);

#endif
