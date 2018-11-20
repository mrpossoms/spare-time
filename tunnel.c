#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <curses.h>
#include <term.h>
#include <termios.h>
#include <time.h>
#include <math.h>

#include "term_game.h"

struct {
	int max_rows, max_cols;
} term = { 18, 0 };

typedef struct {
	int top, bottom;	
} opening_t;

struct {
	struct {
		int x, y;
		int dx, dy;
	} player;
	
	struct {
		opening_t gaps[512];	
		int gap_size;
		int x;
	} world;

	time_t start_time;
	time_t last_shrank;
	int paused;
} game = {
	.player = { 1, 3 },
};

struct termios oldt;

void sig_winch_hndlr(int sig)
{
	term.max_cols = tg_term_width();
	if (term.max_cols < 0)
	{
		term.max_cols = 80;
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
	{
		game.player.dx = game.player.dy = 0;
		return;
	}

	switch(c)
	{
		case 'i':
			game.player.dy = -1;
			break;
		case 'k':
			game.player.dy = 1;
			break;
		case 'j':
			game.player.dx = -1;
			break;
		case 'l':
			game.player.dx = 1;
			break;
		default:
			game.player.dy = 0;
	}
}


static inline char* sampler(int row, int col)
{
	if (row == game.player.y)
	if (col == game.player.x)
		return "\033[0;32m>\033[0m";

	opening_t* gap = game.world.gaps + ((col + game.world.x) % term.max_cols);
	
	if (row < gap->top || row > gap->bottom)
		return "X";

	return " ";
}


static inline int is_dead()
{
	int x = game.player.x + game.world.x;
	opening_t* gap = game.world.gaps + (x % term.max_cols);
	return game.player.y < gap->top || game.player.y > gap->bottom; 
}


void rasterize()
{
	int rows = term.max_rows;
	static char move_up[16] = {};
	sprintf(move_up, "\033[%dA", rows);
	

	// line
	//fprintf(stderr, "\033[91m");
	for (int r = 0; r < rows; ++r)
	for (int c = 0; c < term.max_cols; ++c)
	{
		char* glyph = sampler(r, c);
		fprintf(stderr, "%s", glyph);
	} fputc('\n', stderr);

	//fprintf(stderr, "\033[39m");

	if (!is_dead()) fprintf(stderr, "%s", move_up);
}


void next_gap(opening_t* next, opening_t* last)
{
	int delta = (random() % 3) - 1;

	int top = last->top + delta;
	int gap = (fabsf(sin(game.world.x / 10.f)) + 1.f) * game.world.gap_size;
	int max_top = term.max_rows - gap;
	if (top > max_top) top = max_top;
	if (top < 0) top = 0; 

	next->top = top;
	next->bottom = top + gap;
}


int time_played()
{
	return time(NULL) - game.start_time;
}

#define CLAMP(x, min, max) (x > max ? max : (x < min ? min : x))
void update()
{
	int dy = game.player.dy;	
	int dx = game.player.dx;
	
	game.player.x += dx;
	game.player.y += dy;
	
	game.player.x = CLAMP(game.player.x, 0, term.max_cols);
	game.player.y = CLAMP(game.player.y, 0, term.max_rows);

	game.world.x++;

	if (time_played() % 10 == 0 && game.last_shrank != time_played())
	{
		game.last_shrank = time_played();
		game.world.gap_size--;
	}

	opening_t* last = game.world.gaps + ((game.world.x + term.max_cols - 2) % term.max_cols);
	opening_t* next = game.world.gaps + ((game.world.x - 1) % term.max_cols);

	next_gap(next, last);

}


int main(int argc, char* argv[])
{

	signal(SIGWINCH, sig_winch_hndlr);
	signal(SIGINT, sig_int_hndlr);
	sig_winch_hndlr(0);

	printf("Controls:\n\ti & k - move up and down\n\tj & l - move left and right\nStarting in ");
	
	//if(0)
	for(int i = 3; i--;)
	{
		printf("%d ", i + 1);
		fflush(stdout);
		sleep(1);
	} putchar('\n'); 

	tg_game_settings(&oldt);

	game.world.gap_size = 7;
	int top = 0; 

	game.world.gaps[0].top = 0;
	game.world.gaps[0].bottom = term.max_rows - 1;
	opening_t* last = game.world.gaps;

	for (int i = 1; i < term.max_cols; ++i)
	{
		if (game.world.gap_size > term.max_rows) { game.world.gap_size--; }

		next_gap(game.world.gaps + i, last);
		last = game.world.gaps + i;
	}

	game.start_time = time(NULL);

	while (!is_dead())
	{
		input_hndlr();
		update();
		rasterize();
	}

	tg_restore_settings(&oldt);
	printf("\nSCORE: %d\n", game.world.x);

	return 1;
}
