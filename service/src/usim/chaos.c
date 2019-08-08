// chaos.c --- chaosnet interface

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/uio.h>

#include "usim.h"
#include "utrace.h"
#include "ucode.h"
#include "chaos.h"

#define CHAOS_CSR_TIMER_INTERRUPT_ENABLE (1 << 0)
#define CHAOS_CSR_LOOP_BACK (1 << 1)
#define CHAOS_CSR_RECEIVE_ALL (1 << 2)
#define CHAOS_CSR_RECEIVER_CLEAR (1 << 3)
#define CHAOS_CSR_RECEIVE_ENABLE (1 << 4)
#define CHAOS_CSR_TRANSMIT_ENABLE (1 << 5)
#define CHAOS_CSR_INTERRUPT_ENABLES (3 << 4)
#define CHAOS_CSR_TRANSMIT_ABORT (1 << 6)
#define CHAOS_CSR_TRANSMIT_DONE (1 << 7)
#define CHAOS_CSR_TRANSMITTER_CLEAR (1 << 8)
#define CHAOS_CSR_LOST_COUNT (017 << 9)
#define CHAOS_CSR_RESET (1 << 13)
#define CHAOS_CSR_CRC_ERROR (1 << 14)
#define CHAOS_CSR_RECEIVE_DONE (1 << 15)

static int chaos_csr;

static int chaos_addr = 0401;
static int chaos_bit_count;
static int chaos_lost_count = 0;

#define CHAOS_BUF_SIZE_BYTES 8192

static unsigned short chaos_xmit_buffer[CHAOS_BUF_SIZE_BYTES / 2];
static int chaos_xmit_buffer_size;
static int chaos_xmit_buffer_ptr;

static unsigned short chaos_rcv_buffer[CHAOS_BUF_SIZE_BYTES / 2];
static unsigned short chaos_rcv_buffer_toss[CHAOS_BUF_SIZE_BYTES / 2];
static int chaos_rcv_buffer_ptr;
static int chaos_rcv_buffer_size;
static int chaos_rcv_buffer_empty;

static int chaos_fd;
static bool chaos_need_reconnect;
static int reconnect_delay;

static void chaos_force_reconect(void);
static int chaos_send_to_chaosd(char *buffer, int size);

// RFC1071: Compute Internet Checksum for COUNT bytes beginning at
// location ADDR.
static unsigned short
ch_checksum(const unsigned char *addr, int count)
{
	long sum = 0;

	while (count > 1) {
		sum += *(addr) << 8 | *(addr + 1);
		addr += 2;
		count -= 2;
	}

	// Add left-over byte, if any.
	if (count > 0)
		sum += *(unsigned char *) addr;

	// Fold 32-bit sum to 16 bits.
	while (sum >> 16)
		sum = (sum & 0xffff) + (sum >> 16);

	return (~sum) & 0xffff;
}

static void
chaos_rx_pkt(void)
{
	chaos_rcv_buffer_ptr = 0;
	chaos_bit_count = (chaos_rcv_buffer_size * 2 * 8) - 1;
	if (chaos_rcv_buffer_size > 0) {
		DEBUG(TRACE_CHAOS, "chaos: set RDN, generate interrupt\n");
		chaos_csr |= CHAOS_CSR_RECEIVE_DONE;
		if (chaos_csr & CHAOS_CSR_RECEIVE_ENABLE)
			assert_unibus_interrupt(0270);
	} else {
		DEBUG(TRACE_CHAOS, "chaos_rx_pkt: called, but no data in buffer\n");
	}
}

static void
char_xmit_done_intr(void)
{
	chaos_csr |= CHAOS_CSR_TRANSMIT_DONE;
	if (chaos_csr & CHAOS_CSR_TRANSMIT_ENABLE)
		assert_unibus_interrupt(0270);
}

void
chaos_xmit_pkt(void)
{
	DEBUG(TRACE_CHAOS, "chaos_xmit_pkt() %d bytes, data len %d\n", chaos_xmit_buffer_ptr * 2, (chaos_xmit_buffer_ptr > 0 ? chaos_xmit_buffer[1] & 0x3f : -1));

	chaos_xmit_buffer_size = chaos_xmit_buffer_ptr;

	// Dest is already in the buffer.

	chaos_xmit_buffer[chaos_xmit_buffer_size++] = (unsigned short) chaos_addr; // Source.
	chaos_xmit_buffer[chaos_xmit_buffer_size] = ch_checksum((unsigned char *) chaos_xmit_buffer, chaos_xmit_buffer_size * 2); // Checksum.
	chaos_xmit_buffer_size++;

	chaos_send_to_chaosd((char *) chaos_xmit_buffer, chaos_xmit_buffer_size * 2);

	chaos_xmit_buffer_ptr = 0;
	char_xmit_done_intr();
}

int
chaos_get_bit_count(void)
{
	if (chaos_rcv_buffer_size > 0)
		return chaos_bit_count;
	DEBUG(TRACE_CHAOS, "chaos_get_bit_count: returned empty count\n");
	return 07777;
}

int
chaos_get_rcv_buffer(void)
{
	int v = 0;

	if (chaos_rcv_buffer_ptr < chaos_rcv_buffer_size) {
		v = chaos_rcv_buffer[chaos_rcv_buffer_ptr++];
		if (chaos_rcv_buffer_ptr == chaos_rcv_buffer_size) {
			chaos_rcv_buffer_empty = 1;
			DEBUG(TRACE_CHAOS, "chaos_get_rcv_buffer: marked buffer as empty\n");
		}
	} else {
		// Read last word, clear receive done.
		chaos_csr &= ~CHAOS_CSR_RECEIVE_DONE;
		chaos_rcv_buffer_size = 0;
		DEBUG(TRACE_CHAOS, "chaos_get_rcv_buffer: cleared CHAOS_CSR_RECEIVE_DONE\n");
	}

	return v;
}

void
chaos_put_xmit_buffer(int v)
{
	if (chaos_xmit_buffer_ptr < (int) sizeof(chaos_xmit_buffer) / 2)
		chaos_xmit_buffer[chaos_xmit_buffer_ptr++] = (unsigned short) v;
	chaos_csr &= ~CHAOS_CSR_TRANSMIT_DONE;
}

void
chaos_set_addr(int addr)
{
	chaos_addr = addr;
}

int
chaos_get_addr(void)
{
	return chaos_addr;
}

int
chaos_get_csr(void)
{
	static int old_chaos_csr = 0;

	if (chaos_csr != old_chaos_csr) {
		old_chaos_csr = chaos_csr;
		DEBUG(TRACE_CHAOS, "unibus: chaos read csr %o\n", chaos_csr);
	}

	return chaos_csr | ((chaos_lost_count << 9) & 017);
}

void
chaos_set_csr(int v)
{
	int mask;

	v &= 0xffff;
	DEBUG(TRACE_CHAOS, "chaos: set csr bits 0%o, old 0%o\n", v, chaos_csr);

	// Writing these don't stick.
	mask =
		CHAOS_CSR_TRANSMIT_DONE |
		CHAOS_CSR_LOST_COUNT |
		CHAOS_CSR_CRC_ERROR |
		CHAOS_CSR_RECEIVE_DONE | CHAOS_CSR_RECEIVER_CLEAR;

	chaos_csr = (chaos_csr & mask) | (v & ~mask);

	if (chaos_csr & CHAOS_CSR_RESET) {
		DEBUG(TRACE_CHAOS, "reset ");
		chaos_rcv_buffer_size = 0;
		chaos_xmit_buffer_ptr = 0;
		chaos_lost_count = 0;
		chaos_bit_count = 0;
		chaos_rcv_buffer_ptr = 0;
		chaos_csr &= ~(CHAOS_CSR_RESET | CHAOS_CSR_RECEIVE_DONE);
		chaos_csr |= CHAOS_CSR_TRANSMIT_DONE;
		reconnect_delay = 200; // Do it right away.
		chaos_force_reconect();
	}

	if (v & CHAOS_CSR_RECEIVER_CLEAR) {
		chaos_csr &= ~CHAOS_CSR_RECEIVE_DONE;
		chaos_lost_count = 0;
		chaos_bit_count = 0;
		chaos_rcv_buffer_ptr = 0;
		chaos_rcv_buffer_size = 0;
	}

	if (v & (CHAOS_CSR_TRANSMITTER_CLEAR | CHAOS_CSR_TRANSMIT_DONE)) {
		chaos_csr &= ~CHAOS_CSR_TRANSMIT_ABORT;
		chaos_csr |= CHAOS_CSR_TRANSMIT_DONE;
		chaos_xmit_buffer_ptr = 0;
	}

	if (chaos_csr & CHAOS_CSR_RECEIVE_ENABLE) {
		DEBUG(TRACE_CHAOS, "rx-enable ");
		if (chaos_rcv_buffer_empty) {
			chaos_rcv_buffer_ptr = 0;
			chaos_rcv_buffer_size = 0;
		}
		// If buffer is full, generate status and interrupt again.
		if (chaos_rcv_buffer_size > 0) {
			DEBUG(TRACE_CHAOS, "\n rx-enabled and buffer is full\n");
			chaos_rx_pkt();
		}
	}

	if (chaos_csr & CHAOS_CSR_TRANSMIT_ENABLE) {
		DEBUG(TRACE_CHAOS, "tx-enable ");
		chaos_csr |= CHAOS_CSR_TRANSMIT_DONE;
	}

	DEBUG(TRACE_CHAOS, " New csr 0%o\n", chaos_csr);
}

#define UNIX_SOCKET_PATH	"/var/tmp/"
#define UNIX_SOCKET_CLIENT_NAME	"chaosd_"
#define UNIX_SOCKET_SERVER_NAME	"chaosd_server"
#define UNIX_SOCKET_PERM	S_IRWXU

static struct sockaddr_un unix_addr;

static void
chaos_force_reconect(void)
{
	DEBUG(TRACE_CHAOS, "chaos: forcing reconnect to chaosd\n");

	close(chaos_fd);
	chaos_fd = 0;
	chaos_need_reconnect = true;
}

static int
chaos_send_to_chaosd(char *buffer, int size)
{
	int wcount, dest_addr;

	// Local loopback.
	if (chaos_csr & CHAOS_CSR_LOOP_BACK) {
		DEBUG(TRACE_CHAOS, "chaos: loopback %d bytes\n", size);
		memcpy(chaos_rcv_buffer, buffer, size);

		chaos_rcv_buffer_size = (size + 1) / 2;
		chaos_rcv_buffer_empty = 0;

		chaos_rx_pkt();

		return 0;
	}

	wcount = (size + 1) / 2;
	dest_addr = ((u_short *) buffer)[wcount - 3];

	DEBUG(TRACE_CHAOS, "chaos tx: dest_addr = %o, chaos_addr=%o, size %d, wcount %d\n", dest_addr, chaos_addr, size, wcount);

	// Recieve packets addressed to us.
	if (dest_addr == chaos_addr) {
		memcpy(chaos_rcv_buffer, buffer, size);

		chaos_rcv_buffer_size = (size + 1) / 2;
		chaos_rcv_buffer_empty = 0;

		chaos_rx_pkt();
	}

	if (!chaos_fd)
		return 0;

	struct iovec iov[2];
	unsigned char lenbytes[4];
	int ret;

	lenbytes[0] = size >> 8;
	lenbytes[1] = size;
	lenbytes[2] = 1;
	lenbytes[3] = 0;

	iov[0].iov_base = lenbytes;
	iov[0].iov_len = 4;

	iov[1].iov_base = buffer;
	iov[1].iov_len = size;

	ret = writev(chaos_fd, iov, 2);
	if (ret < 0) {
		perror("chaos write");
		return -1;
	}

	return 0;
}

static int
chaos_connect_to_server(void)
{
	int len;

	DEBUG(TRACE_CHAOS, "connect_to_server()\n");

	chaos_fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (chaos_fd < 0) {
		perror("socket(AF_UNIX)");
		chaos_fd = 0;
		return -1;
	}

	memset(&unix_addr, 0, sizeof(unix_addr));

	sprintf(unix_addr.sun_path, "%s%s%05u", UNIX_SOCKET_PATH, UNIX_SOCKET_CLIENT_NAME, getpid());

	unix_addr.sun_family = AF_UNIX;
	len = SUN_LEN(&unix_addr);

	unlink(unix_addr.sun_path);

	if (bind(chaos_fd, (struct sockaddr *) &unix_addr, len) < 0) {
		perror("bind(AF_UNIX)");
		return -1;
	}

	if (chmod(unix_addr.sun_path, UNIX_SOCKET_PERM) < 0) {
		perror("chmod(AF_UNIX)");
		return -1;
	}

	memset(&unix_addr, 0, sizeof(unix_addr));
	sprintf(unix_addr.sun_path, "%s%s", UNIX_SOCKET_PATH, UNIX_SOCKET_SERVER_NAME);
	unix_addr.sun_family = AF_UNIX;
	len = SUN_LEN(&unix_addr);

	if (connect(chaos_fd, (struct sockaddr *) &unix_addr, len) < 0) {
		WARNING(TRACE_CHAOS, "chaos: no chaosd server\n");
		return -1;
	}

	socklen_t value = 0;
	socklen_t length = sizeof(value);

	if (getsockopt(chaos_fd, SOL_SOCKET, SO_RCVBUF, &value, &length) == 0) {
		value = value * 4;
		if (setsockopt(chaos_fd, SOL_SOCKET, SO_RCVBUF, &value, sizeof(value)) != 0)
			WARNING(TRACE_CHAOS, "setsockopt(SO_RCVBUF) failed\n");
	}

	return 0;
}

int
chaos_poll(void)
{
	int ret;
	struct pollfd pfd[1];
	int nfds, timeout;

	if (chaos_need_reconnect == true) {
		chaos_reconnect();
	}

	if (chaos_fd == 0) {
		return 0;
	}

	timeout = 0;
	nfds = 1;
	pfd[0].fd = chaos_fd;
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;

	ret = poll(pfd, nfds, timeout);
	if (ret == -1) {
		DEBUG(TRACE_CHAOS, "chaos: Polling, nothing there (RDN=%o)\n", chaos_csr & CHAOS_CSR_RECEIVE_DONE);
		chaos_need_reconnect = true;
		return -1;
	} else if (ret == 0) {
			//DEBUG(TRACE_CHAOS, "chaos: timeout\n");
		return -1;
	}

	unsigned char lenbytes[4];
	unsigned int len;

	// Is rx buffer full?
	if (!chaos_rcv_buffer_empty && (chaos_csr & CHAOS_CSR_RECEIVE_DONE)) {
		// Toss packets arriving when buffer is already in
		// use, they will be resent.
		DEBUG(TRACE_CHAOS, "chaos: polling, unread data, drop " "(RDN=%o, lost %d)\n", chaos_csr & CHAOS_CSR_RECEIVE_DONE, chaos_lost_count);
		chaos_lost_count++;
		ret = read(chaos_fd, lenbytes, 4);
		if (ret != 4)
			perror("read");
		len = (lenbytes[0] << 8) | lenbytes[1];
		DEBUG(TRACE_CHAOS, "chaos: tossing packet of %d bytes\n", len);
		if (len > sizeof(chaos_rcv_buffer_toss)) {
			DEBUG(TRACE_CHAOS, "chaos packet won't fit");
			chaos_force_reconect();
			return -1;
		}

		// Toss it...
		ret = read(chaos_fd, (char *) chaos_rcv_buffer_toss, len);
		if (ret != len)
			perror("read");
		return -1;
	}

	// Read header from chaosd.
	ret = read(chaos_fd, lenbytes, 4);
	if (ret <= 0) {
		perror("chaos: header read error");
		chaos_force_reconect();
		return -1;
	}

	len = (lenbytes[0] << 8) | lenbytes[1];

	if (len > sizeof(chaos_rcv_buffer)) {
		DEBUG(TRACE_CHAOS, "chaos: packet too big: " "pkt size %d, buffer size %lu\n", len, sizeof(chaos_rcv_buffer));

		// When we get out of synch break socket conn.
		chaos_force_reconect();
		return -1;
	}

	ret = read(chaos_fd, (char *) chaos_rcv_buffer, len);
	if (ret == -1) {
		perror("chaos: read");
		chaos_force_reconect();
		return -1;
	} else if (ret == 0) {
		DEBUG(TRACE_CHAOS, "chaos: read zero bytes\n");
		return -1;
	}

	DEBUG(TRACE_CHAOS, "chaos: polling; got chaosd packet %d\n", ret);

	int dest_addr;

	chaos_rcv_buffer_size = (ret + 1) / 2;
	chaos_rcv_buffer_empty = 0;
	dest_addr = chaos_rcv_buffer[chaos_rcv_buffer_size - 3];

	DEBUG(TRACE_CHAOS, "chaos rx: to %o, my %o\n", dest_addr, chaos_addr);	

	// If not to us, ignore.
	if (dest_addr != chaos_addr) {
		chaos_rcv_buffer_size = 0;
		chaos_rcv_buffer_empty = 1;
		return 0;
	}

	chaos_rx_pkt();

	return 0;
}

void
chaos_reconnect(void)
{
	static int reconnect_time;

	if (++reconnect_delay < 200)
		return;
	reconnect_delay = 0;

	// Try every 5 seconds.
	if (reconnect_time && time(NULL) < (reconnect_time + 5))
		return;
	reconnect_time = time(NULL);

	NOTICE(TRACE_CHAOS, "chaos: reconnecting to chaosd\n");
	if (chaos_init() == 0) {
		NOTICE(TRACE_CHAOS, "chaos: reconnected\n");
		chaos_need_reconnect = false;
		reconnect_delay = 0;
	}
}

int
chaos_init(void)
{
	if (chaos_connect_to_server()) {
		close(chaos_fd);
		chaos_fd = 0;
		return -1;
	}

	chaos_rcv_buffer_empty = 1;

	INFO(TRACE_CHAOS, "chaos: my address is %o\n", chaos_get_addr());

	return 0;
}
