

CC = gcc -std=gnu99
CC += -Wall -Wextra

CFLAGS = -g3 -O3 -I/usr/X11R6/include

all: TAGS usim readmcr diskmaker lod lmfs cc

usim.o: CFLAGS += -DVERSION=\"$(VERSION)\"
usim: usim.o ucode.o mem.o iob.o mouse.o kbd.o knight.o cadet.o tv.o x11.o chaos.o disk.o ini.o ucfg.o trace.o disass.o syms.o misc.o
	$(CC) $(CFLAGS) -o $@ $^ -lpthread -lX11 -L/usr/X11R6/lib

readmcr: readmcr.o disass.o misc.o syms.o
	$(CC) $(CFLAGS) -o $@ $^

diskmaker: diskmaker.o misc.o
	$(CC) $(CFLAGS) -o $@ $^

lmfs: lmfs.o misc.o
	$(CC) $(CFLAGS) -o $@ $^

lod: lod.o disass.o misc.o syms.o
	$(CC) $(CFLAGS) -o $@ $^

cc: YFLAGS = -d
cc: cc.o ccy.o ccl.o disass.o lcadrd.o misc.o syms.o
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f *.o
	rm -rf *.tab.c *.tab.h
	rm -f *~
	rm -f xx
	rm -f usim lod readmcr diskmaker lmfs cc

.PHONY: TAGS
TAGS:
	find . -type f -iname "*.[ch]" | etags -

.PHONY: format-source
format-source:
	find -name "*.[ch]" -exec ./format-source {} \;

dist:
	rm -rf usim-$(VERSION)
	svn export . usim-$(VERSION)
	tar zcf usim-$(VERSION).tar.gz usim-$(VERSION)

