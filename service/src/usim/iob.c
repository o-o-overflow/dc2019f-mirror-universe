// iob.c --- CADR I/O board
//
// The IOB board consits of the following devices:
//
// - General-Purpose I/O
// - Clocks
// - Command/Status register (CSR)
// - Keyboard
// - Mouse
// - Chaosnet
//
// See SYS:DOC;IOB TEXT for details.

// ---!!! Order this into proper sections: IOB, Mouse, TV, ...

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <sys/time.h>

#include "usim.h"
#include "utrace.h"
#include "ucode.h"
#include "kbd.h"
#include "mouse.h"
#include "chaos.h"

uint32_t iob_csr;
static uint32_t cv;

static uint32_t
get_us_clock(void)
{
	uint32_t v;
	static struct timeval tv;
	struct timeval tv2;
	uint32_t ds;
	uint32_t du;

	if (tv.tv_sec == 0) {
		gettimeofday(&tv, 0);
		v = 0;
	} else {
		gettimeofday(&tv2, 0);
		if (tv2.tv_usec < tv.tv_usec) {
			tv2.tv_sec--;
			tv2.tv_usec += 1000 * 1000;
		}
		ds = tv2.tv_sec - tv.tv_sec;
		du = tv2.tv_usec - tv.tv_usec;
		v = (ds * 1000 * 1000) + du;
	}

	return v;
}

static uint32_t
get_us_clock_low(void)
{
	cv = get_us_clock();
	return cv & 0xffff;
}

static uint32_t
get_us_clock_high(void)
{
	return (uint32_t) (cv >> 16);
}

static uint32_t
get_60hz_clock(void)
{
	return 0;
}

void
iob_unibus_read(int offset, int *pv)
{
	*pv = 0;		// For now default to zero.

	switch (offset) {
	case 0100:
		*pv = kbd_key_scan & 0177777;
		DEBUG(TRACE_IOB, "unibus: kbd low %011o\n", *pv);
		iob_csr &= ~(1 << 5);	// Clear CSR<5>.
		break;
	case 0102:
		*pv = (kbd_key_scan >> 16) & 0177777;
		DEBUG(TRACE_IOB, "unibus: kbd high %011o\n", *pv);
		iob_csr &= ~(1 << 5);	// Clear CSR<5>.
		break;
	case 0104:
		*pv = (mouse_tail << 12) | (mouse_middle << 13) | (mouse_head << 14) | (mouse_y & 07777);
		DEBUG(TRACE_IOB, "unibus: mouse y %011o\n", *pv);

		mouse_tail = 0;
		mouse_middle = 0;
		mouse_head = 0;

		iob_csr &= ~(1 << 4);	// Clear CSR<4>.
		break;
	case 0106:
		*pv = (mouse_rawx << 12) | (mouse_rawy << 14) | (mouse_x & 07777);
		DEBUG(TRACE_IOB, "unibus: mouse x %011o\n", *pv);
		break;
	case 0110:
		DEBUG(TRACE_IOB, "unibus: beep\n");
		fprintf(stderr, "\a");	// Beep!
		break;
	case 0112:
		*pv = iob_csr;
		DEBUG(TRACE_IOB, "unibus: kbd csr %011o\n", *pv);
		break;
	case 0120:
		*pv = get_us_clock_low();
		DEBUG(TRACE_IOB, "unibus: usec clock low\n");
		break;
	case 0122:
		*pv = get_us_clock_high();
		DEBUG(TRACE_IOB, "unibus: usec clock high\n");
		break;
	case 0124:
		*pv = get_60hz_clock();
		DEBUG(TRACE_IOB, "unibus: 60hz clock\n");
		break;
	case 0140:
		*pv = chaos_get_csr();
		break;
	case 0142:
		*pv = chaos_get_addr();
		DEBUG(TRACE_IOB, "unibus: chaos read my-number\n");
		break;
	case 0144:
		*pv = chaos_get_rcv_buffer();
		DEBUG(TRACE_IOB, "unibus: chaos read rcv buffer %06o\n", *pv);
		break;
	case 0146:
		*pv = chaos_get_bit_count();
		DEBUG(TRACE_IOB, "unibus: chaos read bit-count 0%o\n", *pv);
		break;
	case 0152:
		*pv = chaos_get_addr();
		DEBUG(TRACE_IOB, "unibus: chaos read xmt => %o\n", *pv);
		chaos_xmit_pkt();
		break;
	case 160:
	case 162:
	case 164:
	case 166:
		DEBUG(TRACE_IOB, "unibus: uart read ---!!! %o\n", *pv);
		break;
	default:
		if (offset > 0140 && offset <= 0153)
			DEBUG(TRACE_IOB, "unibus: chaos read other %o\n", offset);
		chaos_xmit_pkt();
		break;
	}
}

void
iob_unibus_write(int offset, int v)
{
	switch (offset) {
	case 0100:
		DEBUG(TRACE_IOB, "unibus: kbd low\n");
		break;
	case 0102:
		DEBUG(TRACE_IOB, "unibus: kbd high\n");
		break;
	case 0104:
		DEBUG(TRACE_IOB, "unibus: mouse y\n");
		break;
	case 0106:
		DEBUG(TRACE_IOB, "unibus: mouse x\n");
		break;
	case 0110:
		DEBUG(TRACE_IOB, "unibus: beep\n");
		break;
	case 0112:
		DEBUG(TRACE_IOB, "unibus: kbd csr\n");
		iob_csr = (iob_csr & ~017) | (v & 017);
		break;
	case 0120:
		DEBUG(TRACE_IOB, "unibus: usec clock\n");
		break;
	case 0122:
		DEBUG(TRACE_IOB, "unibus: usec clock\n");
		break;
	case 0124:
		DEBUG(TRACE_IOB, "unibus: start 60hz clock\n");
		break;
	case 0140:
		DEBUG(TRACE_IOB, "unibus: chaos write %011o\n", v);
		chaos_set_csr(v);
		break;
	case 0142:
		DEBUG(TRACE_IOB, "unibus: chaos write-buffer write %011o\n", v);
		chaos_put_xmit_buffer(v);
		break;
	case 160:
	case 162:
	case 164:
	case 166:
		DEBUG(TRACE_IOB, "unibus: uart write ---!!! %o\n", v);
		break;
	default:
		if (offset > 0140 && offset <= 0152)
			DEBUG(TRACE_IOB, "unibus: chaos write other\n");
		break;
	}
}

void
iob_poll(void)
{
}

void
iob_init(void)
{
	kbd_init();
	mouse_init();
}
