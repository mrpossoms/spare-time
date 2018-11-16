CC=gcc
CFLAGS=-g
LINK=-lncurses -lpthread

tunnel: tunnel.c
	$(CC) $(CFLAGS) $^ -o $@ $(LINK)
