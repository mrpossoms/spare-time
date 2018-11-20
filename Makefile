CC=gcc
CFLAGS=-g
LINK=-lncurses -lpthread -lm

tunnel: tunnel.c
	$(CC) $(CFLAGS) $^ -o $@ $(LINK)
