CC=gcc
CFLAGS= -W -Wall -g
LIBS= -lreadline
SOURCES= vtcc.c cc_core.c

all:
	$(CC) $(CFLAGS) $(SOURCES) -o vtcc $(LIBS)
