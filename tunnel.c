#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <curses.h>
#include <term.h>
#include <termios.h>


struct {
	int max_rows, max_cols;
} term;

typedef struct {
	int top, bottom;	
} opening_t;

struct {
	struct {
		int x, y;
		int dy;
	} player;
	
	struct {
		opening_t gaps[512];	
		int gap_size;
		int x;
	} world;

	int paused;
} game = {
	.player = { 1, 3 },
};

struct termios oldt;

void sig_winch_hndlr(int sig)
{
	char termbuf[2048];
	if (tgetent(termbuf, getenv("TERM")) >= 0) {
		term.max_cols = tgetnum("co") /* -2 */;
	} else {
		term.max_cols = 80;
	}
}


void sig_int_hndlr(int sig)
{
	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	exit(1);
}


void input_hndlr()
{
	fd_set fds;
	struct timeval tv = { 0, 100000 };

	FD_ZERO(&fds);
	FD_SET(STDIN_FILENO, &fds);

	switch(select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv))
	{
		case 0:  // timeout
		case -1: // error
			game.player.dy = 0;
			return;
	}

	char c;// = getchar();
	read(STDIN_FILENO, &c, sizeof(c));
	switch(c)
	{
		case 'i':
			game.player.dy = -1;
			break;
		case 'k':
			game.player.dy = 1;
			break;
		default:
			game.player.dy = 0;
	}
}


static inline char* sampler(int row, int col)
{
	if (row == game.player.y)
	if (col == game.player.x)
		return "\033[0;32m>\033[91m";

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
	int rows = 9;
	static char move_up[16] = {};
	sprintf(move_up, "\033[%dA", rows);
	

	// line
	fprintf(stderr, "\033[91m");
	for (int r = 0; r < rows; ++r)
	for (int c = 0; c < term.max_cols; ++c)
	{
		char* glyph = sampler(r, c);
		fprintf(stderr, "%s", glyph);
	} fputc('\n', stderr);
	fprintf(stderr, "\033[39m");

	if (!is_dead()) fprintf(stderr, "%s", move_up);
}


void next_gap(opening_t* next, opening_t* last)
{
	int delta = (random() % 3) - 1;

	int top = last->top + delta;

	if (top > 8) top = 8;
	if (top < 0) top = 0; 

	next->top = top;
	next->bottom = top + game.world.gap_size;
}


void update()
{
	int dy = game.player.dy;	
	if ( game.player.y > 0 && dy < 0)
	{
		game.player.y += dy;
	}

	if ( game.player.y < 8 && dy > 0)
	{
		game.player.y += dy;
	}

	game.world.x++;

	opening_t* last = game.world.gaps + ((game.world.x + term.max_cols - 2) % term.max_cols);
	opening_t* next = game.world.gaps + ((game.world.x - 1) % term.max_cols);

	next_gap(next, last);

}


int main(int argc, char* argv[])
{

	signal(SIGWINCH, sig_winch_hndlr);
	signal(SIGINT, sig_int_hndlr);
	sig_winch_hndlr(0);

	printf("Controls:\n\ti - move up\n\tj - move down\n");
	sleep(3);

	tcgetattr(STDIN_FILENO, &oldt);
	struct termios newt = oldt;
	newt.c_lflag &= ~ECHO;
	newt.c_lflag &= ~ICANON;
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	fputs("\033[?25l", stderr);

	game.world.gap_size = 9;
	int top = 0; //(random() % 9) - game.world.gap_size;
	
	game.world.gaps[0].top = 0;
	game.world.gaps[0].bottom = 8;
	opening_t* last = game.world.gaps;

	for (int i = 1; i < term.max_cols; ++i)
	{
		if (game.world.gap_size > 3) { game.world.gap_size--; }

		next_gap(game.world.gaps + i, last);
		last = game.world.gaps + i;
	}

	while (!is_dead())
	{
		input_hndlr();
		update();
		rasterize();
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	fputs("\033[?25h", stderr);
	printf("\nSCORE: %d\n", game.world.x);

	return 1;
}
