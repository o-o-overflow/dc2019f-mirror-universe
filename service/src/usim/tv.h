#ifndef USIM_TV_H
#define USIM_TV_H

extern uint32_t tv_bitmap[768 * 1024];
extern uint32_t tv_width;
extern uint32_t tv_height;

extern void tv_init(void);
extern void tv_poll(void);
extern void tv_write(uint32_t offset, uint32_t bits);
extern void tv_read(uint32_t offset, uint32_t *pv);

extern void tv_xbus_read(uint32_t offset, uint32_t *pv);
extern void tv_xbus_write(uint32_t offset, uint32_t v);

#endif
