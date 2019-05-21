#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <curses.h>
#include <term.h>
#include <termios.h>
#include <time.h>
#include <math.h>

#include "tg.h"

#define CRAFT_W 32
#define CRAFT_H 32

int TG_TIMEOUT = 100;

uint8_t rand_tbl[512];

struct {
	int max_rows, max_cols;
} term = { 18, 0 };

typedef struct {
	struct { int x, y; } origin;
	char parts[CRAFT_H][CRAFT_W];

	struct { float x, y; } pos;
	struct { float x, y; } vel;
} craft_t;

struct termios oldt;

craft_t craft = {
	.parts = {
		"    /:\\    ",
		"###-[^]-###",
	},
	.pos = { 0, 0 },
};


void compute_origin(craft_t* c)
{
	int part_count = 0;

	c->origin.x = c->origin.y = 0;

	for (int i = 0; i < CRAFT_W; ++i)
	for (int j = 0; j < CRAFT_H; ++j)
	{
		char part = c->parts[j][i];
		if (part == ' ') { continue; }
		if (part == '\0') { break; }

		c->origin.x += i;
		c->origin.y += j;
		++part_count;
	}

	c->origin.x /= part_count;
	c->origin.y /= part_count;
}


char sample_craft(craft_t const* c, int row, int col)
{
	int c_col = (col - (int)c->pos.x) + c->origin.x;
	int c_row = (row - (int)c->pos.y) + c->origin.y;

	if (c_col < 0 || c_row < 0 || c_col >= CRAFT_W || c_row >= CRAFT_H) return '\0';

	char part = c->parts[c_row][c_col];

	if (part == ' ') { return '\0'; }

	return part;
}


void update_craft(craft_t* c)
{
	c->pos.x += c->vel.x;
	c->pos.y += c->vel.y;
}


void sig_winch_hndlr(int sig)
{
	term.max_cols = tg_term_width();
	term.max_rows = tg_term_height() - 1;

	if (term.max_cols < 0)
	{
		term.max_cols = 80;
		term.max_rows = 40;
	}
}


void sig_int_hndlr(int sig)
{
	tg_restore_settings(&oldt);
	exit(1);
}


void input_hndlr()
{
	char c;
	if (tg_key_get(&c) == 0)
	{ // no key pressed
		return;
	}

	float imp = 0.01f;
	switch(c)
	{ // handle key accordingly
                case 'i':
                        craft.vel.y -= imp;
                        break;
                case 'k':
                        craft.vel.y += imp;
                        break;
                case 'j':
                        craft.vel.x -= imp;
                        break;
                case 'l':
                        craft.vel.x += imp;
                        break;
		default:
			// TODO
			;
	}
}


static inline char* sampler(int row, int col)
{
	// return character for a given row and column in the terminal
	static char c;

	{ // draw velocity string
		tg_str_t vel_str = {
			1, 1,
			"V: %0.2f, %0.2f",
		};

		c = tg_str(row, col, &vel_str, craft.vel.x, craft.vel.y);
		if (c > -1) return &c;
	}

	c = sample_craft(&craft, row, col);
	if (c != '\0') { return &c; }

	// render stars
	if (rand_tbl[((row * term.max_cols) + col) % 512] < 4) { return "*"; }

	return " ";
}


int playing()
{
	// return 0 when the game loop should terminate
	return 1;
}


void update()
{
	// do game logic, update game state
	update_craft(&craft);
}


int main(int argc, char* argv[])
{
	srandom(time(NULL));
	signal(SIGWINCH, sig_winch_hndlr);
	signal(SIGINT, sig_int_hndlr);
	sig_winch_hndlr(0);

	for (int i = sizeof(rand_tbl); i--;) { rand_tbl[i] = random() % 256; }

	tg_game_settings(&oldt);
	compute_origin(&craft);

	while (playing())
	{
		input_hndlr();
		update();
		tg_rasterize(term.max_rows, term.max_cols, sampler);
	}

	tg_restore_settings(&oldt);

	return 1;
}
