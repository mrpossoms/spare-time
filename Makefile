CC=gcc
CFLAGS=-g -O3
LINK=-lncurses -lpthread -lm

deltav: deltav.c tg.h
	$(CC) $(CFLAGS) $< -o $@ $(LINK)

tunnel: tunnel.c tg.h
	$(CC) $(CFLAGS) $< -o $@ $(LINK)
