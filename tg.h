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

/**
 * @brief      Returns a random float.
 *
 * @return     Random float in the range [-1, 1]
 */
float tg_randf() { return (random() % 2048) / 1024.f - 1.f; }

/**
 * @brief      Computes next particle system state (animates) from the previous
 *             state.
 *
 * @param      sys   The particle system.
 */
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

/**
 * @brief      Passes the row and column of the character being rendered. If
 *             particles exist in that location, a non 0 character will be
 *             returned.
 *
 * @param      sys   The particle system.
 * @param[in]  row   The row of the character the terminal is rendering.
 * @param[in]  col   The col of the character the terminal is rendering.
 *
 * @return     character to be displayed if there are particles present, 0
 *             otherwise.
 */
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

/**
 * @brief      Spawns given particle in the particle system.
 *
 * @param      sys   The particle system.
 * @param      p     Pointer to particle that will be spawned in the system.
 */
void tg_spawn_particle(tg_particle_system_t* sys, tg_particle_t* p)
{
	if (sys->_living_count >= sizeof(sys->particles) / sizeof(tg_particle_t)) { return; }

	sys->particles[sys->_living_count] = *p;
	sys->_living_count++;
}

/**
 * @brief      Removes all living particles from the system.
 *
 * @param      sys   The particle system.
 */
void tg_clear_particles(tg_particle_system_t* sys)
{
	sys->_living_count = 0;
}

/**
 * @brief      Sets the terminal up to render a game like yours!
 *
 * @param      old_settings  The old terminal settings. Keep these to restore
 *                           the previous settings for your terminal.
 *
 * @return     0 on success
 */
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

/**
 * @brief      Restores a previously saved terminal settings object.
 *
 * @param      old_settings  The old terminal settings.
 *
 * @return     0 on success
 */
int tg_restore_settings(struct termios* old_settings)
{
	tcsetattr(STDIN_FILENO, TCSANOW, old_settings);
	fputs("\033[?25h", stderr);

	return 0;
}


/**
 * @brief      Get the current width in columns of the terminal
 *
 * @return     width in columns, -1 if the value cannot be retrieved
 */
int tg_term_width()
{
	char buf[2048];
	if (tgetent(buf, getenv("TERM")) >= 0)
	{
		return tgetnum("co");
	}

	return -1;
}

/**
 * @brief      Get the current height in rows of the terminal
 *
 * @return     height in rows, -1 if the value cannot be retrieved
 */
int tg_term_height()
{
	char buf[2048];
	if (tgetent(buf, getenv("TERM")) >= 0)
	{
		return tgetnum("li");
	}

	return -1;
}

/**
 * @brief      Returns a key pressed withing TG_TIMEOUT microseconds
 *
 * @param      key   The key character pressed
 *
 * @return     1 if a key is pressed, 0 otherwise.
 */
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

/**
 * Context for drawing a string in the game.
 */
typedef struct {
	int row;         // row where the origin of the string should appear.
	int col;         // col where the origin of the string should appear.
	const char* fmt; // printf style format string
	struct {
		// centered indicates that the string will appear centered around the origin.
		uint8_t centered : 1;
	} mode;          // defines the mode of the format string's origin

	char   _buf[1024];
	size_t _len;
} tg_str_t;

/**
 * @brief      Samples a string in the game.
 *
 * @param[in]  row        The row
 * @param[in]  col        The col
 * @param      ctx        The string context
 * @param[in]  <unnamed>  variadic input to context's format string
 *
 * @return     { description_of_the_return_value }
 */
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

/**
 * @brief      Erases the last 'rows' number of lines from the terminal
 * and moves the cursor back up that same number of rows. This is intended
 * to get ready for drawing a new frame.
 *
 * @param[in]  rows  The rows to clear
 */
void tg_clear(int rows)
{
	static char move_up[16] = {};
	sprintf(move_up, "\033[%dA", rows);
	fprintf(stderr, "%s", move_up);
}

/**
 * @brief      tg_rasterize simply iterates over each row and column from 0 to
 * 'rows' and 0 to 'cols' for each row-col pair. With each pair, the `sampler`
 * function pointer is called. This function pointer is user provided. And allows
 * the programmer's game to dictate what should be displayed in each row-col pair
 * by returning a pointer to the appropriate character.
 *
 * @param[in]  rows     The number of rows that will be sampled
 * @param[in]  cols     The number of cols that will be sampled
 * @param      sampler  The sampler function pointer, whose purpose is described
 *                      above
 */
void tg_rasterize(int rows, int cols, char* (*sampler)(int row, int col))
{
	for (int r = 0; r < rows; ++r)
	for (int c = 0; c < cols; ++c)
	{
		char* glyph = sampler(r, c);
		fprintf(stderr, "%s", glyph);
	} fputc('\n', stderr);
}

#endif
