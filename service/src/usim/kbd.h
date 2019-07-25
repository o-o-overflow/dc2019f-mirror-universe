#ifndef USIM_KBD_H
#define USIM_KBD_H

#include <stdint.h>

#include "knight.h"
#include "cadet.h"

extern int kbd_type;

extern uint32_t kbd_key_scan;

extern void kbd_init(void);
extern void kbd_warm_boot_key(void);
extern void kbd_key_event(int code, int keydown);
extern void kbd_dequeue_key_event(void);

#endif
