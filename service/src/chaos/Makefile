#
#
# $Id$
#

#---------- figure out what we are runnign on ---------

OS_NAME = $(shell uname)

ifeq ($(OS_NAME), Darwin)
OS = OSX
LIBS = 
endif

ifeq ($(OS_NAME), Linux)
OS = LINUX
LIBS =
endif

ifeq ($(OS_NAME), NetBSD)
OS = BSD
LIBS= -lcompat -lcrypt
endif

#----------- code ------------

CHAOSD_OBJ = chaosd.o transport.o node.o log.o signal.o

SERVER_OBJ = server.o chaos.o ncp.o rfc.o testpackets.o log.o signal.o

CFLAGS = -O2 -fno-builtin-log



all: chaosd listen server client time FILE MINI

chaosd: $(CHAOSD_OBJ)
	$(CC) $(CFLAGS) -o chaosd $(CHAOSD_OBJ)

listen: listen.c
	$(CC) $(CFLAGS) -o listen listen.c

server: $(SERVER_OBJ)
	$(CC) $(CFLAGS) -o server $(SERVER_OBJ)

client: client.c
	$(CC) $(CFLAGS) -o client client.c

time: time.c
	$(CC) $(CFLAGS) -o time time.c

FILE: FILE.c FILE.h glob.c chlib.c
	$(CC) $(CFLAGS) -o FILE FILE.c glob.c chlib.c $(LIBS)

MINI: MINI.c chlib.c
	$(CC) $(CFLAGS) -o MINI MINI.c chlib.c

clean:
	rm -f *.o chaosd listen server FILE MINI time client

