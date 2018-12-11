CC=gcc
CFLAGS=-g
LINK=-lncurses -lpthread -lm

tunnel: tunnel.c tg.h
	$(CC) $(CFLAGS) $< -o $@ $(LINK)
