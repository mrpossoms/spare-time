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
	uint8_t is_dead;
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
	.vel = { 0, 0.05 },
};

particle_system_t thruster_psys = {
	.start_life = 10,
	.repulsion = 0.0f,
	.density_glyphs = " .,:;x%&##"
};

particle_system_t crash_psys = {
	.repulsion = 0.5f,
	.start_life = 10000,
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


void spawn_crash(craft_t* c)
{
	for (int i = CRAFT_W; i--;)
	for (int j = CRAFT_H; j--;)
	{
		char part = c->parts[j][i];

		if (part == ' ' || part == '\0') { continue; }
		else
		{
			particle_t p = {
				.pos = { c->pos.x + i - c->origin.x, c->pos.y + j - c->origin.y },
				.vel = { c->vel.x + randf() * 0.01f, c->vel.y + randf() * 0.01f },
				.glyph = part,
				.life = 100000,
			};
			spawn_particle(&crash_psys, &p);
		}
	}
}


void update_particle_sys(particle_system_t* sys)
{
	particle_t* parts = sys->particles;
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

void player_thruster(float dx, float dy)
{
	if (craft.fuel == 0 || craft.is_dead) { return; }
	craft.fuel--;
	craft.vel.x += dx;
	craft.vel.y += dy;
	spawn_thruster_jet(craft.pos.x, craft.pos.y, -dx * 50, -dy * 50);
}


int do_craft_intersect(craft_t const* c0, craft_t const* c1)
{
	if (c0->is_dead || c1->is_dead) { return 0; }

	for (int c0_r = CRAFT_H; c0_r--;)
	for (int c0_c = CRAFT_W; c0_c--;)
	{
		char c0_part = c0->parts[c0_r][c0_c];
		int c0_part_x = c0->pos.x - c0->origin.x + c0_c;
		int c0_part_y = c0->pos.y - c0->origin.y + c0_r;

		if (c0_part == ' ' || c0_part == '\0') { continue; }

		for (int c1_r = CRAFT_H; c1_r--;)
		for (int c1_c = CRAFT_W; c1_c--;)
		{
			char c1_part = c1->parts[c1_r][c1_c];
			if (c1_part == ' ' || c1_part == '\0') { continue; }
			else
			{
				int c1_part_x = c1->pos.x - c1->origin.x + c1_c;
				int c1_part_y = c1->pos.y - c1->origin.y + c1_r;

				if (c1_part_x == c0_part_x && c1_part_y == c0_part_y)
				{ return 1; }
			}
		}
	}

	return 0;
}


char sample_craft(craft_t const* c, int row, int col)
{
	if (c->is_dead) { return '\0'; }

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
		case 'w':
			player_thruster(0, imp);
                        break;
                case 'k':
		case 's':
			player_thruster(0, -imp);
                        break;
                case 'j':
		case 'a':
			player_thruster(imp, 0);
                        break;
                case 'l':
		case 'd':
			player_thruster(-imp, 0);
                        break;
		case 'b':
			spawn_crash(&craft);
			craft.is_dead = 1;
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

	c = sample_particle_sys(&crash_psys, row, col);
	if (c != '\0') { return &c; }

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
	update_craft(&station);
	update_particle_sys(&thruster_psys);
	update_particle_sys(&crash_psys);

	if (do_craft_intersect(&craft, &station))
	{
		craft.is_dead = station.is_dead = 1;
		spawn_crash(&craft);
		spawn_crash(&station);
	}
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
