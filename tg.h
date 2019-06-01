#ifndef TERM_GAME_H
#define TERM_GAME_H

#include <unistd.h>
#include <termios.h>
#include <term.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#ifdef __linux__
#include <stdio_ext.h>
#endif
#include <stdlib.h>

extern int TG_TIMEOUT;

typedef struct {
	struct { float x, y; } pos;
	struct { float x, y; } vel;
	int life;
	char glyph;
} tg_particle_t;

typedef struct {
	tg_particle_t particles[128];
	int start_life;
	float repulsion;
	char density_glyphs[10];

	size_t _glyph_count;
	int _living_count;
} tg_particle_system_t;

float tg_randf() { return (random() % 2048) / 1024.f - 1.f; }

void tg_update_particle_sys(tg_particle_system_t* sys)
{
	tg_particle_t* parts = sys->particles;
	if (sys->repulsion > 0)
	for (int i = sys->_living_count; i--;)
	for (int j = sys->_living_count; j--;)
	{
		if (i == j) continue;
		float d_x = parts[i].pos.x - parts[j].pos.x;
		float d_y = parts[i].pos.y - parts[j].pos.y;

		float dist = sqrtf(d_x * d_x + d_y * d_y);

		if (dist < 0.5f)
		{
		float rep_x = (parts[j].vel.x - parts[i].vel.x) * sys->repulsion;//sys->repulsion / (0.01 + d_x * d_x);
		float rep_y = (parts[j].vel.y - parts[i].vel.y) * sys->repulsion;//sys->repulsion / (0.01 + d_y * d_y);

		parts[i].vel.x += rep_x;
		parts[i].vel.y += rep_y;
		}
	}

	for (int i = sys->_living_count; i--;)
	{
		if (parts[i].life <= 0)
		{
			parts[i] = parts[sys->_living_count - 1];
			sys->_living_count--;
		}
		else
		{
			parts[i].pos.x += parts[i].vel.x;
			parts[i].pos.y += parts[i].vel.y;
			parts[i].life--;
		}
	}
}


char tg_sample_particle_sys(tg_particle_system_t const* sys, int row, int col)
{
	char c = 0;
	int density = 0;

	for (int i = sys->_living_count; i--;)
	{
		if ((int)sys->particles[i].pos.x == col &&
		    (int)sys->particles[i].pos.y == row)
		{
			density++;
			if (sys->particles[i].glyph > 0) { c = sys->particles[i].glyph; }
		}
	}

	if (density && c == 0)
	{
		density = density >= sizeof(sys->density_glyphs) ? sizeof(sys->density_glyphs) - 1 : density;
		c = sys->density_glyphs[density];
	}

	return c;
}


void tg_spawn_particle(tg_particle_system_t* sys, tg_particle_t* p)
{
	if (sys->_living_count >= sizeof(sys->particles) / sizeof(tg_particle_t)) { return; }

	sys->particles[sys->_living_count] = *p;
	sys->_living_count++;
}


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

#ifdef __linux__
	__fpurge(stdin);
#elif defined(__APPLE__)
	fpurge(stdin);
#endif

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
	
	int adj_col = ctx->col;
	if (ctx->mode.centered) { adj_col -= ctx->_len >> 1; }
	if (col >= adj_col && col < adj_col + ctx->_len)
	{
		int i = col - adj_col;
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
