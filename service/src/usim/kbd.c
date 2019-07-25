// kbd.c --- keyboard handling

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <err.h>

#include "usim.h"
#include "ucfg.h"
#include "utrace.h"
#include "ucode.h"
#include "iob.h"
#include "kbd.h"

int kbd_type = 0;

uint32_t kbd_key_scan;

#define KEY_QUEUE_LEN 10

static int key_queue[KEY_QUEUE_LEN];
static int key_queue_optr = 0;
static int key_queue_iptr = 0;
static int key_queue_free = KEY_QUEUE_LEN;

static void
queue_key_event(int ev)
{
	int v;

	v = (1 << 16) | ev;

	if (key_queue_free > 0) {
		DEBUG(TRACE_IOB, "queue_key_event() - queuing 0%o, q len before %d\n", v, KEY_QUEUE_LEN - key_queue_free);
		key_queue_free--;
		key_queue[key_queue_optr] = v;
		key_queue_optr = (key_queue_optr + 1) % KEY_QUEUE_LEN;
	} else {
		WARNING(TRACE_IOB, "IOB key queue full!");
		if (!(iob_csr & (1 << 5)) && (iob_csr & (1 << 2))) {
			iob_csr |= 1 << 5;
			WARNING(TRACE_IOB, "queue_key_event generating interrupt");
			assert_unibus_interrupt(0260);
		}
	}
}

void
kbd_dequeue_key_event(void)
{
	if (iob_csr & (1 << 5))	// Already something to be read.
		return;

	if (key_queue_free < KEY_QUEUE_LEN) {
		int v = key_queue[key_queue_iptr];
		DEBUG(TRACE_IOB, "dequeue_key_event() - dequeuing 0%o, q len before %d\n", v, KEY_QUEUE_LEN - key_queue_free);
		key_queue_iptr = (key_queue_iptr + 1) % KEY_QUEUE_LEN;
		key_queue_free++;
		kbd_key_scan = (1 << 16) | v;
		if (iob_csr & (1 << 2)) { // Keyboard interrupt enabled?
			iob_csr |= 1 << 5;
			WARNING(TRACE_IOB, "dequeue_key_event generating interrupt (q len after %d)", KEY_QUEUE_LEN - key_queue_free);
			assert_unibus_interrupt(0260);
		}
	}
}

void
kbd_key_event(int code, int keydown)
{
	int v;

	DEBUG(TRACE_IOB, "key_event(code=%x, keydown=%x)\n", code, keydown);

	v = ((!keydown) << 8) | code;

	if (iob_csr & (1 << 5))
		queue_key_event(v); // Already something there, queue this.
	else {
		kbd_key_scan = (1 << 16) | v;
		DEBUG(TRACE_IOB, "key_event() - 0%o\n", kbd_key_scan);
		if (iob_csr & (1 << 2)) {
			iob_csr |= 1 << 5;
			assert_unibus_interrupt(0260);
		}
	}
}

void
kbd_warm_boot_key(void)
{
	// Send a Return to get the machine booted.
	kbd_key_event(062, 0);
}

void
kbd_init(void)
{
	if (kbd_type == 0)
		knight_init();
	else
		cadet_init();
}
