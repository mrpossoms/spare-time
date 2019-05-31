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

int TG_TIMEOUT = 10;

uint8_t rand_tbl[512];

struct {
	int max_rows, max_cols;
} term = { 18, 0 };

typedef struct {
	struct { int x, y; } origin;
	char parts[CRAFT_H][CRAFT_W];

	struct { float x, y; } pos;
	struct { float x, y; } vel;
	uint8_t fuel;
} craft_t;

typedef struct {
	struct { float x, y; } pos;
	struct { float x, y; } vel;
	int life;
	char glyph;
} particle_t;

typedef struct {
	particle_t particles[128];
	int start_life;
	float repulsion;
	char density_glyphs[10];

	size_t _glyph_count;
	int _living_count;
} particle_system_t;

struct termios oldt;

craft_t craft = {
	.parts = {
		"    /:\\    ",
		"###-[^]-###",
	},
	.pos = { 0, 0 },
	.fuel = 100,
};

craft_t station = {
	.parts = {
		"## ##   _   ## ##",
		"## ##  |.|  ## ##",
		"##-##==[:]==##-##",
		"## ##  |.|  ## ##",
		"## ##   V   ## ##",
	},
	.pos = { 40, 20 },
};

particle_system_t thruster_psys = {
	.start_life = 10,
	.repulsion = 0.01f,
	.density_glyphs = " .,:;x%&##"
};


float randf() { return (random() % 2048) / 1024.f - 1.f; }

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


char sample_particle_sys(particle_system_t const* sys, int row, int col)
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


void spawn_particle(particle_system_t* sys, particle_t* p)
{
	if (sys->_living_count >= sizeof(sys->particles) / sizeof(particle_t)) { return; }

	sys->particles[sys->_living_count] = *p;
	sys->_living_count++;
}


void update_particle_sys(particle_system_t* sys)
{
	particle_t* parts = sys->particles;
	if (sys->repulsion > 0)
	for (int i = sys->_living_count; i--;)
	for (int j = sys->_living_count; j--;)
	{
		if (i == j) continue;
		float d_x = parts[j].pos.x - parts[i].pos.x;
		float d_y = parts[j].pos.y - parts[i].pos.y;

		float dist = 0.001f + sqrtf(d_x * d_x + d_y * d_y);

		float rep_x = sys->repulsion * d_x / dist;
		float rep_y = sys->repulsion * d_y / dist;

		parts[i].vel.x += rep_x;
		parts[i].vel.y += rep_y;
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


void spawn_thruster_jet(float x, float y, float dx, float dy)
{
	for (int i = 10; i--;)
	{
		particle_t part = {
			.pos = { x + randf() * 0.5f, y + randf() * 0.5f },
			.vel = { dx, dy },
			.life = thruster_psys.start_life + (random() % 10),
		};
		spawn_particle(&thruster_psys, &part);
	}
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

	if (craft.fuel == 0) { return; }

	float imp = 0.01f;
	switch(c)
	{ // handle key accordingly
                case 'i':
		case 'w':
			craft.fuel--;
                        craft.vel.y -= imp;
			spawn_thruster_jet(craft.pos.x, craft.pos.y, 0, imp * 100);
                        break;
                case 'k':
		case 's':
			craft.fuel--;
                        craft.vel.y += imp;
			spawn_thruster_jet(craft.pos.x, craft.pos.y, 0, -imp * 100);
                        break;
                case 'j':
		case 'a':
			craft.fuel--;
                        craft.vel.x -= imp;
			spawn_thruster_jet(craft.pos.x, craft.pos.y, imp * 100, 0);
                        break;
                case 'l':
		case 'd':
			craft.fuel--;
                        craft.vel.x += imp;
			spawn_thruster_jet(craft.pos.x, craft.pos.y, -imp * 100, 0);
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

	{ // draw fuel gauge
		char str[32] = "Fuel: [          ]";
		for (int i = craft.fuel / 10; i--;) { str[7 + i] = '#'; }

		tg_str_t fuel_str = {
			2, 1,
			str,	
		};

		c = tg_str(row, col, &fuel_str);
		if (c > -1) return &c;
	}

	c = sample_particle_sys(&thruster_psys, row, col);
	if (c != '\0') { return &c; }

	c = sample_craft(&craft, row, col);
	if (c != '\0') { return &c; }
	
	c = sample_craft(&station, row, col);
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

void start()
{
	station.pos.x = term.max_cols / 2;
	station.pos.y = 5;

	craft.pos.x = (random() % (term.max_cols / 2)) + term.max_cols / 4;
	craft.pos.y = term.max_rows - 5;
	craft.vel.x = ((random() % 20) - 10) / 100.f;
	craft.vel.y = -((random() % 20)) / 100.f;
}

void update()
{
	// do game logic, update game state
	update_craft(&craft);
	update_particle_sys(&thruster_psys);
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
	compute_origin(&station);

	start();

	while (playing())
	{
		input_hndlr();
		update();
		tg_rasterize(term.max_rows, term.max_cols, sampler);
	}

	tg_restore_settings(&oldt);

	return 1;
}