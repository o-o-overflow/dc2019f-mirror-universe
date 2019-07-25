#ifndef USIM_IOB_H
#define USIM_IOB_H

extern uint32_t iob_csr;

extern int iob_init(void);
extern void iob_poll(void);

extern void iob_unibus_read(int offset, int *pv);
extern void iob_unibus_write(int offset, int v);

#endif
