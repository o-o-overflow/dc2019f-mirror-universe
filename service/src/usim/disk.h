#ifndef USIM_DISK_H
#define USIM_DISK_H

extern int disk_init(int unit, char *filename);
extern void disk_poll(void);

extern void disk_xbus_read(int offset, uint32_t *pv);
extern void disk_xbus_write(int offset, uint32_t v);

#endif
