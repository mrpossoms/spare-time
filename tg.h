#ifndef TERM_GAME_H
#define TERM_GAME_H

#include <unistd.h>
#include <termios.h>
#include <term.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>

extern int TG_TIMEOUT;


int tg_game_settings(struct termios* old_settings)
{
	struct termios newt;

	// disable echo from stdin
	tcgetattr(STDIN_FILENO, old_settings);
	newt = *old_settings;
	newt.c_lflag &= ~ECHO;
	newt.c_lflag &= ~ICANON;
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	// hide cursor
	fputs("\033[?25l", stderr);

	return 0;
}


int tg_restore_settings(struct termios* old_settings)
{
	tcsetattr(STDIN_FILENO, TCSANOW, old_settings);
	fputs("\033[?25h", stderr);

	return 0;
}


int tg_term_width()
{
	char buf[2048];
	if (tgetent(buf, getenv("TERM")) >= 0)
	{
		return tgetnum("co");
	}

	return -1;
}


int tg_term_height()
{
	char buf[2048];
	if (tgetent(buf, getenv("TERM")) >= 0)
	{
		return tgetnum("li");
	}

	return -1;
}


int tg_key_get(char* key)
{
	fd_set fds;
	struct timeval tv = { 0, TG_TIMEOUT };

	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);

	__fpurge(stdin);

	switch(select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv))
	{
		case 0:  // timeout
		case -1: // error
			return 0;
	}

	usleep(tv.tv_usec); // wait the remaining timeout

	return read(STDIN_FILENO, key, sizeof(char)) == sizeof(char);
}


typedef struct {
	int row;
	int col;
	const char* fmt;
	struct {
		uint8_t centered : 1;
	} mode;

	char   _buf[1024];
	size_t _len;
} tg_str_t;


int tg_str(int row, int col, tg_str_t* ctx, ...)
{
	va_list ap;

	va_start(ap, ctx);
	vsnprintf(ctx->_buf, sizeof(ctx->_buf), ctx->fmt, ap);
	va_end(ap);

	ctx->_len = strlen(ctx->_buf);

	if (row != ctx->row) { return -1; }

	if (col >= ctx->col && col < ctx->col + ctx->_len)
	{
		int i = col - ctx->col;
		return ctx->_buf[i];
	}

	return -1;
}


void tg_rasterize(int rows, int cols, char* (*sampler)(int row, int col))
{
	static char move_up[16] = {};
	sprintf(move_up, "\033[%dA", rows);


	// line
	//fprintf(stderr, "\033[91m");
	for (int r = 0; r < rows; ++r)
	for (int c = 0; c < cols; ++c)
	{
		char* glyph = sampler(r, c);
		fprintf(stderr, "%s", glyph);
	} fputc('\n', stderr);

	//fprintf(stderr, "\033[39m");

	//if (!is_dead())
	fprintf(stderr, "%s", move_up);
}

#endif
