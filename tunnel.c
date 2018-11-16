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
	int x;

	struct {
		int top, bottom;	
	} edges;
} opening_t;

struct {
	struct {
		int x, y;
		int dy;
	} player;
	
	struct {
		
	} world;

	int paused;
} game = {
	.player = { 1, 3 },
};


void sig_winch_hndlr(int sig)
{
	char termbuf[2048];
	if (tgetent(termbuf, getenv("TERM")) >= 0) {
		term.max_cols = tgetnum("co") /* -2 */;
	} else {
		term.max_cols = 80;
	}
}


void sig_int_hndlr(int sig)   { endwin(); exit(0); }


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


char sampler(int row, int col)
{
	if (row == game.player.y)
	if (col == game.player.x)
		return '>';

	return ' ';
}


void rasterize()
{
	int rows = 9;
	static char move_up[16] = {};
	sprintf(move_up, "\033[%dA", rows);
	

	// line
	for (int r = 0; r < rows; ++r)
	for (int c = 0; c < term.max_cols; ++c)
	{
		char glyph = sampler(r, c);
		fputc(glyph, stderr);
	} fputc('\n', stderr);

	fprintf(stderr, "%s", move_up);
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
}


int main(int argc, char* argv[])
{

	signal(SIGWINCH, sig_winch_hndlr);
	sig_winch_hndlr(0);

	struct termios oldt;
	tcgetattr(STDIN_FILENO, &oldt);
	struct termios newt = oldt;
	newt.c_lflag &= ~ECHO;
	newt.c_lflag &= ~ICANON;
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);

	fputs("\033[?25l", stderr);

	int i = 0;
	while (!game.paused)
	{
		// TODO: write a game
		input_hndlr();
		update();
		rasterize();
	}

	return 0;
}
