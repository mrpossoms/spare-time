#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <curses.h>
#include <term.h>
#include <termios.h>
#include <time.h>
#include <math.h>

#include "tg.h"

#define CRAFT_W 8
#define CRAFT_H 8

int TG_TIMEOUT = 100;

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
		"#-[]-#",
	},
	.pos = { 40, 2 },
};

void compute_origin(craft_t* c)
{
	int part_count = 0;

	c->origin.x = c->origin.y = 0;

	for (int i = CRAFT_W; --i;)
	for (int j = CRAFT_H; --j;)
	{
		if (c->parts[j][j] == '\0') { continue; }

		c->origin.x += i;
		c->origin.y += j;
		++part_count;
	}

	c->origin.x /= part_count;
	c->origin.y /= part_count;
}

char sample_craft(craft_t const* c, int row, int col)
{
	int c_col = (col - (int)c->pos.x) - c->origin.x;
	int c_row = (row - (int)c->pos.y) - c->origin.y; 

	if (c_col < 0 || c_row < 0 || c_col >= CRAFT_W || c_row >= CRAFT_H) return '\0';

	return c->parts[c_row][c_col];
}


void update_craft(craft_t* c)
{
	c->pos.x += c->vel.x;
	c->pos.y += c->vel.y;
}


void sig_winch_hndlr(int sig)
{
	term.max_cols = tg_term_width();
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
	c = sample_craft(&craft, row, col);
	if (c != '\0') { return &c; }

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

	signal(SIGWINCH, sig_winch_hndlr);
	signal(SIGINT, sig_int_hndlr);
	sig_winch_hndlr(0);

	tg_game_settings(&oldt);


	while (playing())
	{
		input_hndlr();
		update();
		tg_rasterize(term.max_rows, term.max_cols, sampler);
	}

	tg_restore_settings(&oldt);

	return 1;
}
