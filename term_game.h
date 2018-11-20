#ifndef TERM_GAME_H
#define TERM_GAME_H

#include <unistd.h>
#include <termios.h>
#include <term.h>

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

int tg_key_get(char* key)
{
	fd_set fds;
	struct timeval tv = { 0, 100000 };

	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);

	switch(select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv))
	{
		case 0:  // timeout
		case -1: // error
			return 0;
	}

	return read(STDIN_FILENO, key, sizeof(char)) == sizeof(char);
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
