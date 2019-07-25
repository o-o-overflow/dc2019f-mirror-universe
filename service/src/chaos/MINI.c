/*
 * chaosnet MINI protocol server
 *
 * Reverse engineered from MIT Lispm code
 *
 * 8/2010 rjs@fdy2.demon.co.uk
 */

#include <sys/param.h>

#include <stdio.h>
#include <fcntl.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/ioctl.h>

#define _XOPEN_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>

#include "chaos.h"

struct chstatus chst;		/* Status buffer for connections */

#define LOG_INFO	0
#define LOG_ERR		1
#define LOG_NOTICE      2

int log_verbose = 0;
int log_stderr_tofile = 1;

void
log(int level, char *fmt, ...)
{
	char string[512];
	va_list ap;

	if (log_stderr_tofile == 0) return;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	fflush(stderr);
}

/*
 * This server gets called from the general RFC server, who gives us control
 * with standard input and standard output set to the control connection
 * and with the connection open (already accepted)
 */
main(argc, argv)
int argc;
char **argv;
{
	int length, fd, binary, ret, ret1;
	unsigned char *cp;
	unsigned char tbuf[20];
	struct chpacket p, pout;
	struct stat sbuf;
	struct tm *ptm;
	static char ebuf[BUFSIZ];

	if (log_stderr_tofile) {
		/* stderr */
		char logfilename[256];
		(void)close(2);
#if 0
		unlink("MINI.log");
		strcpy(logfilename, "MINI.log");
#else
		sprintf(logfilename, "MINI.%d.log", getpid());
#endif
		(void)open(logfilename, O_WRONLY | O_CREAT, 0666);
		(void)lseek(2, 0L, 2);
		setbuf(stderr, ebuf);

		fprintf(stderr,"MINI!\n"); fflush(stderr);
	} else {
		(void)close(2);
		(void)open("/dev/null", O_WRONLY);
	}

	chsetmode(0, CHRECORD);
	chstatus(0, &chst);

	log(LOG_INFO, "MINI: %s\n", argv[1]);
	binary = 0;

	for (;;) {

		length = read(0, (char *)&p, sizeof(p));
		if (length <= 0) {
			log(LOG_INFO, "MINI: Ctl connection broken(%d,%d)\n",
				length, errno);
			exit(0);
		}

		switch (p.cp_op) {
		case 0200:
		case 0201:
			((char *)&p)[length] = '\0';
			log(LOG_INFO, "MINI: op %o %s\n", p.cp_op, p.cp_data);
			if ((fd = open(p.cp_data, O_RDONLY)) < 0) {
				pout.cp_op = 0203;
				log(LOG_ERR, "MINI: open failed %s\n",
				    strerror(errno));
			} else {
				pout.cp_op = 0202;
				fstat(fd, &sbuf);
				ptm = localtime(&sbuf.st_mtime);
				strftime(tbuf, sizeof(tbuf), "%D %T", ptm);
				length = sprintf(pout.cp_data, "%s%c%s",
						 p.cp_data, 0215, tbuf);
				while (write(1, (char *)&pout, length + 1) < 0)
					usleep(10000);
				binary = p.cp_op & 1;
			}

			do {
				pout.cp_op = (binary) ? DWDOP : DATOP;
				length = read(fd, pout.cp_data, CHMAXDATA);
				/*log(LOG_INFO, "MINI: read %d\n", length);*/
				if (length == 0) break;
				if (binary == 0)
				  to_lispm(pout.cp_data, length);

				while (write(1, (char *)&pout, length + 1) < 0)
					usleep(10000);
			} while (length > 0);

			log(LOG_INFO, "MINI: before eof\n");
			pout.cp_op = EOFOP;
			while (write(1, (char *)&pout, 1) < 0)
				usleep(10000);
			close(fd);
			break;
		default:
			log(LOG_INFO, "MINI: op %o\n", p.cp_op);
			break;
		}
	}
}


/*
 * Character set conversion routines.
 */
to_lispm(data, length)
unsigned char *data;
int length;
{
	register int c, i;

	for (i = 0; i < length; i++) {
		c = data[i] & 0377;
		switch (c) {
		case 0210:	/* Map SAIL symbols back */
		case 0211:
		case 0212:
		case 0213:
		case 0214:
		case 0215:
		case 0377:	/* Give back infinity */
			c &= 0177;
			break;
		case '\n':	/* Map canonical newline to lispm */
			c = CHNL;
			break;
		case 015:	/* Reverse linefeed map kludge */
			c = 0212;
		case 010:	/* Map the format effectors back */
		case 011:
		case 013:
		case 014:
		case 0177:
			c |= 0200;
		}
		data[i] = c;
	}
}
