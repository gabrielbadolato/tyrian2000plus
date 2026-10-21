/*
 * Tyrian 2000+: Tyrian 2000 with modern controls, based on OpenTyrian2000
 * Copyright (C) 2026 Tyrian 2000+ contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

/*
 * Touch screen controls.  A stick on the left and the buttons on the right,
 * in the bars beside the game (landscape) or below it (portrait; the game
 * moves to the top).  Where there are no bars, they are drawn over the game,
 * see-through.  The stick floats: it centers where the finger lands, anywhere
 * in its zone.
 *
 * Everything here works with SDL's finger events and its renderer, so it
 * runs the same in the browser and in a future Android build.  SDL's own
 * touch-to-mouse is turned off: a finger on a control must not also click.
 * Fingers on the game act as the mouse instead (pushed as SDL mouse events),
 * so the menus work by tapping and, in a level, dragging moves the ship.
 */
#include "touch.h"

#include "font.h"
#include "fonthand.h"
#include "joystick.h"
#include "keyboard.h"
#include "lang.h"
#include "opentyr.h"
#include "sprite.h"
#include "video.h"

#include <math.h>
#include <string.h>

#define FINGERS_MAX 10
#define STICK_DEAD_ZONE 0.15f
#define MENU_REPEAT_FIRST_MS 350
#define MENU_REPEAT_MS 150

typedef enum
{
	ROLE_NONE = 0,
	ROLE_STICK,
	ROLE_BUTTON,  // + button index
	ROLE_SCREEN = ROLE_BUTTON + TOUCH_BUTTONS,  // the game itself: the mouse
} Role;

typedef struct
{
	bool used;
	SDL_FingerID id;
	int role;
	float x, y;  // window pixels
} Finger;

static Finger fingers[FINGERS_MAX];

static bool shown = false;
static bool redraw = false;

static bool stickHeld = false;
static float stickCenterX, stickCenterY, stickR;  // where the finger took it
static float stickX, stickY;  // -1..1, after the dead zone

static bool held[TOUCH_BUTTONS];
static bool gamePressed[TOUCH_BUTTONS];  // for the level, since it last looked
static bool menuPressed[TOUCH_BUTTONS];  // for the menus, since they last looked

static int menuDirection = -1;  // 0 up, 1 right, 2 down, 3 left (as the controllers)
static Uint32 menuRepeatAt;

static bool mouseFingerDown = false;

void touch_init(void)
{
	// a finger on a control must not click too; fingers on the game are
	// made into mouse events here
	SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
}

bool touch_shown(void)
{
	return shown;
}

void touch_hide(void)
{
	if (!shown)
		return;

	shown = false;
	redraw = true;
	memset(fingers, 0, sizeof(fingers));
	memset(held, 0, sizeof(held));
	stickHeld = false;
	stickX = stickY = 0;
	mouseFingerDown = false;
}

bool touch_take_redraw(void)
{
	const bool r = redraw;
	redraw = false;
	return r;
}

static float minf(float a, float b) { return a < b ? a : b; }
static float maxf(float a, float b) { return a > b ? a : b; }

void touch_adjust_game_rect(int win_w, int win_h, SDL_Rect *game)
{
	// portrait: the game at the top, the controls below it
	if (shown && win_h > win_w && game->y > 0)
		game->y = 0;
}

void touch_layout(int win_w, int win_h, const SDL_Rect *game, TouchLayout *out)
{
	const float w = win_w, h = win_h;
	const float u = minf(w, h);

	if (h > w)  // portrait: everything below the game
	{
		const float top = game->y + game->h;
		const float room = maxf(h - top, u * 0.45f);
		const float y0 = h - room;
		const float m = u * 0.05f;

		const float r = minf(u * 0.19f, room * 0.30f);
		out->stick = (TouchCircle){ m + r * 1.1f, y0 + room * 0.55f, r };
		out->stickZone = (SDL_Rect){ 0, (int)(y0 + room * 0.18f), (int)(w * 0.5f), (int)(h - (y0 + room * 0.18f)) };

		const float rf = minf(u * 0.13f, room * 0.21f), rb = rf * 0.75f;
		const float fx = w - m - rf, fy = out->stick.y + r * 0.35f;
		out->button[TOUCH_FIRE] = (TouchCircle){ fx, fy, rf };
		out->button[TOUCH_SIDEKICKS] = (TouchCircle){ fx - rf - rb * 1.35f, fy - rb * 0.2f, rb };
		out->button[TOUCH_MODE] = (TouchCircle){ fx - rf * 0.15f, fy - rf - rb * 1.3f, rb };
		const float rm = rb * 0.75f;
		out->button[TOUCH_MENU] = (TouchCircle){ m + rm, y0 + m * 0.5f + rm, rm };
	}
	else  // landscape: the stick at the left, the buttons at the right
	{
		const float m = u * 0.06f;
		const float r = u * 0.17f;
		out->stick = (TouchCircle){ m + r, h - m - r, r };

		const float zoneW = maxf(game->x, out->stick.x + r * 1.5f);
		out->stickZone = (SDL_Rect){ 0, (int)(h * 0.3f), (int)zoneW, (int)(h - h * 0.3f) };

		const float rf = u * 0.12f, rb = rf * 0.75f;
		const float fx = w - m - rf, fy = h - m - rf;
		out->button[TOUCH_FIRE] = (TouchCircle){ fx, fy, rf };
		out->button[TOUCH_SIDEKICKS] = (TouchCircle){ fx - rf - rb * 1.35f, fy - rb * 0.1f, rb };
		out->button[TOUCH_MODE] = (TouchCircle){ fx - rf * 0.1f, fy - rf - rb * 1.4f, rb };
		const float rm = rb * 0.75f;
		out->button[TOUCH_MENU] = (TouchCircle){ m * 0.6f + rm, m * 0.6f + rm, rm };
	}
}

static void current_layout(TouchLayout *layout, int *win_w, int *win_h, SDL_Rect *game)
{
	SDL_GetWindowSize(main_window, win_w, win_h);
	video_get_output_rect(game);
	touch_layout(*win_w, *win_h, game, layout);
}

static int button_at(const TouchLayout *layout, float x, float y)
{
	int best = -1;
	float bestD = 0;
	for (int b = 0; b < TOUCH_BUTTONS; ++b)
	{
		const TouchCircle *c = &layout->button[b];
		const float d = hypotf(x - c->x, y - c->y) / c->r;
		if (d < 1.35f && (best < 0 || d < bestD))
		{
			best = b;
			bestD = d;
		}
	}
	return best;
}

static bool in_rect(const SDL_Rect *r, float x, float y)
{
	return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h;
}

static void update_stick(float x, float y)
{
	float dx = (x - stickCenterX) / stickR, dy = (y - stickCenterY) / stickR;
	const float d = hypotf(dx, dy);
	if (d > 1)
	{
		dx /= d;
		dy /= d;
	}
	// the dead zone, then the rest stretched back to 0..1
	const float m = minf(d, 1);
	const float k = m <= STICK_DEAD_ZONE ? 0 : (m - STICK_DEAD_ZONE) / (1 - STICK_DEAD_ZONE) / m;
	stickX = dx * k;
	stickY = dy * k;
}

static void push_mouse(Uint32 type, int x, int y, int xrel, int yrel)
{
	SDL_Event e;
	memset(&e, 0, sizeof(e));
	e.type = type;
	if (type == SDL_MOUSEMOTION)
	{
		e.motion.windowID = SDL_GetWindowID(main_window);
		e.motion.which = SDL_TOUCH_MOUSEID;
		e.motion.state = mouseFingerDown ? SDL_BUTTON_LMASK : 0;
		e.motion.x = x;
		e.motion.y = y;
		e.motion.xrel = xrel;
		e.motion.yrel = yrel;
	}
	else
	{
		e.button.windowID = SDL_GetWindowID(main_window);
		e.button.which = SDL_TOUCH_MOUSEID;
		e.button.button = SDL_BUTTON_LEFT;
		e.button.state = type == SDL_MOUSEBUTTONDOWN ? SDL_PRESSED : SDL_RELEASED;
		e.button.clicks = 1;
		e.button.x = x;
		e.button.y = y;
	}
	SDL_PushEvent(&e);
}

static Finger *find_finger(SDL_FingerID id)
{
	for (int i = 0; i < FINGERS_MAX; ++i)
		if (fingers[i].used && fingers[i].id == id)
			return &fingers[i];
	return NULL;
}

static void finger_down(SDL_FingerID id, float x, float y)
{
	Finger *f = find_finger(id);
	for (int i = 0; f == NULL && i < FINGERS_MAX; ++i)
		if (!fingers[i].used)
			f = &fingers[i];
	if (f == NULL)
		return;

	*f = (Finger){ true, id, ROLE_NONE, x, y };

	TouchLayout layout;
	int w, h;
	SDL_Rect game;
	current_layout(&layout, &w, &h, &game);

	const int b = button_at(&layout, x, y);
	if (b >= 0)
	{
		f->role = ROLE_BUTTON + b;
		held[b] = true;
		gamePressed[b] = true;
		menuPressed[b] = true;
	}
	else if (!stickHeld && in_rect(&layout.stickZone, x, y))
	{
		f->role = ROLE_STICK;
		stickHeld = true;
		stickR = layout.stick.r;
		// centered under the finger, but the base stays on the screen
		stickCenterX = SDL_clamp(x, stickR * 0.6f, w - stickR * 0.6f);
		stickCenterY = SDL_clamp(y, stickR * 0.6f, h - stickR * 0.6f);
		update_stick(x, y);
	}
	else if (!mouseFingerDown && in_rect(&game, x, y))
	{
		f->role = ROLE_SCREEN;
		mouseFingerDown = true;
		push_mouse(SDL_MOUSEMOTION, x, y, 0, 0);
		push_mouse(SDL_MOUSEBUTTONDOWN, x, y, 0, 0);
	}
	redraw = true;
}

static void finger_motion(SDL_FingerID id, float x, float y)
{
	Finger *f = find_finger(id);
	if (f == NULL)
		return;

	const float oldX = f->x, oldY = f->y;
	f->x = x;
	f->y = y;

	if (f->role == ROLE_STICK)
	{
		update_stick(x, y);
		redraw = true;
	}
	else if (f->role == ROLE_SCREEN)
	{
		const int xrel = (int)lroundf(x) - (int)lroundf(oldX), yrel = (int)lroundf(y) - (int)lroundf(oldY);
		if (xrel != 0 || yrel != 0)
			push_mouse(SDL_MOUSEMOTION, x, y, xrel, yrel);
	}
	else if (f->role >= ROLE_BUTTON && f->role < ROLE_SCREEN)
	{
		// sliding from one button to another presses the new one
		TouchLayout layout;
		int w, h;
		SDL_Rect game;
		current_layout(&layout, &w, &h, &game);
		const int b = button_at(&layout, x, y);
		if (b >= 0 && b != f->role - ROLE_BUTTON)
		{
			held[f->role - ROLE_BUTTON] = false;
			f->role = ROLE_BUTTON + b;
			held[b] = gamePressed[b] = menuPressed[b] = true;
			redraw = true;
		}
	}
}

static void finger_up(SDL_FingerID id, float x, float y)
{
	Finger *f = find_finger(id);
	if (f == NULL)
		return;

	if (f->role == ROLE_STICK)
	{
		stickHeld = false;
		stickX = stickY = 0;
	}
	else if (f->role == ROLE_SCREEN)
	{
		push_mouse(SDL_MOUSEBUTTONUP, x, y, 0, 0);
		mouseFingerDown = false;
	}
	else if (f->role >= ROLE_BUTTON && f->role < ROLE_SCREEN)
	{
		const int b = f->role - ROLE_BUTTON;
		held[b] = false;
		// another finger may still hold the same button
		for (int i = 0; i < FINGERS_MAX; ++i)
			if (&fingers[i] != f && fingers[i].used && fingers[i].role == f->role)
				held[b] = true;
	}

	f->used = false;
	redraw = true;
}

void touch_handle_event(const SDL_Event *ev)
{
	if (ev->tfinger.touchId == SDL_MOUSE_TOUCHID)
		return;  // the mouse, made into touches by SDL (not asked for)

	int w, h;
	SDL_GetWindowSize(main_window, &w, &h);
	const float x = ev->tfinger.x * w, y = ev->tfinger.y * h;

	switch (ev->type)
	{
	case SDL_FINGERDOWN:
		if (!shown)
		{
			shown = true;
			redraw = true;
			memset(gamePressed, 0, sizeof(gamePressed));
			memset(menuPressed, 0, sizeof(menuPressed));
		}
		finger_down(ev->tfinger.fingerId, x, y);
		break;
	case SDL_FINGERMOTION:
		finger_motion(ev->tfinger.fingerId, x, y);
		break;
	case SDL_FINGERUP:
		finger_up(ev->tfinger.fingerId, x, y);
		break;
	}
}

void touch_push_as_keyboard(void)
{
	if (!shown)
		return;

	if (menuPressed[TOUCH_FIRE] || menuPressed[TOUCH_MODE])
		push_key(SDL_SCANCODE_RETURN);
	if (menuPressed[TOUCH_SIDEKICKS] || menuPressed[TOUCH_MENU])
		push_key(SDL_SCANCODE_ESCAPE);
	memset(menuPressed, 0, sizeof(menuPressed));
	memset(gamePressed, 0, sizeof(gamePressed));

	// the stick as the arrow keys, repeating while held
	int direction = -1;
	if (fabsf(stickX) >= 0.5f || fabsf(stickY) >= 0.5f)
	{
		if (fabsf(stickX) > fabsf(stickY))
			direction = stickX > 0 ? 1 : 3;
		else
			direction = stickY > 0 ? 2 : 0;
	}

	const Uint32 now = SDL_GetTicks();
	if (direction != menuDirection)
	{
		menuDirection = direction;
		menuRepeatAt = now + MENU_REPEAT_FIRST_MS;
	}
	else if (direction < 0 || (Sint32)(now - menuRepeatAt) < 0)
	{
		return;
	}
	else
	{
		menuRepeatAt = now + MENU_REPEAT_MS;
	}

	static const SDL_Scancode keys[4] = { SDL_SCANCODE_UP, SDL_SCANCODE_RIGHT, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT };
	if (direction >= 0)
		push_key(keys[direction]);
}

bool touch_game_input(TouchGameInput *out)
{
	memset(out, 0, sizeof(*out));
	if (!shown)
		return false;

	out->x = stickX;
	out->y = stickY;
	out->fire = held[TOUCH_FIRE];
	out->sidekicks = held[TOUCH_SIDEKICKS];
	out->mode_pressed = gamePressed[TOUCH_MODE];
	out->menu_pressed = gamePressed[TOUCH_MENU];

	// what was pressed during the level is not for the menu that comes next
	memset(gamePressed, 0, sizeof(gamePressed));
	memset(menuPressed, 0, sizeof(menuPressed));
	menuDirection = -1;
	return true;
}

// ---- drawing ----

enum { DISC_SIZE = 128 };

static SDL_Texture *discTexture, *ringTexture;
static SDL_Texture *labelTexture[TOUCH_BUTTONS];
static char labelText[TOUCH_BUTTONS][32];
static int labelW[TOUCH_BUTTONS], labelH[TOUCH_BUTTONS];

void touch_release_textures(void)
{
	if (discTexture != NULL)
		SDL_DestroyTexture(discTexture);
	if (ringTexture != NULL)
		SDL_DestroyTexture(ringTexture);
	discTexture = ringTexture = NULL;

	for (int b = 0; b < TOUCH_BUTTONS; ++b)
	{
		if (labelTexture[b] != NULL)
			SDL_DestroyTexture(labelTexture[b]);
		labelTexture[b] = NULL;
		labelText[b][0] = '\0';
	}
}

// a white disc (or ring) with soft edges, tinted when drawn
static SDL_Texture *make_circle(SDL_Renderer *renderer, bool ring)
{
	SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, DISC_SIZE, DISC_SIZE, 32, SDL_PIXELFORMAT_ARGB8888);
	if (s == NULL)
		return NULL;

	const float c = (DISC_SIZE - 1) / 2.0f, outer = DISC_SIZE / 2.0f - 1, inner = outer - 7;
	for (int y = 0; y < DISC_SIZE; ++y)
	{
		Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + y * s->pitch);
		for (int x = 0; x < DISC_SIZE; ++x)
		{
			const float d = hypotf(x - c, y - c);
			float a = SDL_clamp(outer - d + 0.5f, 0.0f, 1.0f);
			if (ring)
				a *= SDL_clamp(d - inner + 0.5f, 0.0f, 1.0f);
			row[x] = ((Uint32)(a * 255) << 24) | 0xFFFFFF;
		}
	}

	SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, s);
	SDL_FreeSurface(s);
	if (t != NULL)
	{
		SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
		SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
	}
	return t;
}

// the label in the game's own font, as white pixels on transparent
static void make_label(SDL_Renderer *renderer, int b, const char *text)
{
	if (labelTexture[b] != NULL && strcmp(labelText[b], text) == 0)
		return;

	if (labelTexture[b] != NULL)
		SDL_DestroyTexture(labelTexture[b]);
	labelTexture[b] = NULL;
	SDL_strlcpy(labelText[b], text, sizeof(labelText[b]));

	if (!sprite_exists(FONT_NORMAL, fontMap['A']))
		return;  // the font is not loaded yet

	SDL_Surface *src = SDL_CreateRGBSurface(0, 320, 24, 8, 0, 0, 0, 0);
	if (src == NULL)
		return;
	SDL_FillRect(src, NULL, 0);
	drawFontHv(src, 2, 2, text, FONT_NORMAL, 15, 4);

	int x0 = src->w, y0 = src->h, x1 = -1, y1 = -1;
	for (int y = 0; y < src->h; ++y)
		for (int x = 0; x < src->w; ++x)
			if (((Uint8 *)src->pixels)[y * src->pitch + x] != 0)
			{
				x0 = SDL_min(x0, x); x1 = SDL_max(x1, x);
				y0 = SDL_min(y0, y); y1 = SDL_max(y1, y);
			}

	if (x1 >= 0)
	{
		const int w = x1 - x0 + 1, h = y1 - y0 + 1;
		SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
		if (s != NULL)
		{
			for (int y = 0; y < h; ++y)
			{
				Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + y * s->pitch);
				for (int x = 0; x < w; ++x)
				{
					const Uint8 p = ((Uint8 *)src->pixels)[(y0 + y) * src->pitch + x0 + x];
					const Uint32 v = p == 0 ? 0 : 150 + (p & 0x0F) * 7;
					row[x] = p == 0 ? 0 : 0xFF000000 | (v << 16) | (v << 8) | v;
				}
			}
			labelTexture[b] = SDL_CreateTextureFromSurface(renderer, s);
			if (labelTexture[b] != NULL)
			{
				SDL_SetTextureBlendMode(labelTexture[b], SDL_BLENDMODE_BLEND);
				SDL_SetTextureScaleMode(labelTexture[b], SDL_ScaleModeNearest);
			}
			labelW[b] = w;
			labelH[b] = h;
			SDL_FreeSurface(s);
		}
	}
	SDL_FreeSurface(src);
}

static void draw_circle(SDL_Renderer *renderer, SDL_Texture *t, float x, float y, float r, Uint8 alpha)
{
	if (t == NULL)
		return;
	const SDL_Rect dst = { (int)lroundf(x - r), (int)lroundf(y - r), (int)lroundf(2 * r), (int)lroundf(2 * r) };
	SDL_SetTextureAlphaMod(t, alpha);
	SDL_RenderCopy(renderer, t, NULL, &dst);
}

void touch_draw(SDL_Renderer *renderer, int win_w, int win_h, const SDL_Rect *game)
{
	if (!shown)
		return;

	if (discTexture == NULL)
		discTexture = make_circle(renderer, false);
	if (ringTexture == NULL)
		ringTexture = make_circle(renderer, true);

	TouchLayout layout;
	touch_layout(win_w, win_h, game, &layout);

	// amber, like the game's menus
	const Uint8 cr = 240, cg = 181, cb = 60;
	SDL_SetTextureColorMod(discTexture, cr, cg, cb);
	SDL_SetTextureColorMod(ringTexture, cr, cg, cb);

	// the stick: where the finger took it, or its place
	const float sx = stickHeld ? stickCenterX : layout.stick.x;
	const float sy = stickHeld ? stickCenterY : layout.stick.y;
	const float r = layout.stick.r;
	draw_circle(renderer, discTexture, sx, sy, r, 40);
	draw_circle(renderer, ringTexture, sx, sy, r, 110);
	float kx = stickX, ky = stickY;
	if (stickHeld)
	{
		// the knob follows the finger itself, inside the base
		for (int i = 0; i < FINGERS_MAX; ++i)
			if (fingers[i].used && fingers[i].role == ROLE_STICK)
			{
				kx = (fingers[i].x - sx) / r;
				ky = (fingers[i].y - sy) / r;
				const float d = hypotf(kx, ky);
				if (d > 1) { kx /= d; ky /= d; }
			}
	}
	draw_circle(renderer, discTexture, sx + kx * r, sy + ky * r, r * 0.45f, stickHeld ? 170 : 110);

	static const char *const labels[TOUCH_BUTTONS] = { "FIRE", "SIDEKICKS", "MODE", "MENU" };
	for (int b = 0; b < TOUCH_BUTTONS; ++b)
	{
		const TouchCircle *c = &layout.button[b];
		draw_circle(renderer, discTexture, c->x, c->y, c->r, held[b] ? 150 : 45);
		draw_circle(renderer, ringTexture, c->x, c->y, c->r, held[b] ? 220 : 120);

		make_label(renderer, b, tr(labels[b]));
		if (labelTexture[b] != NULL)
		{
			// the largest whole scale that fits
			int scale = 1;
			while ((scale + 1) * labelW[b] <= c->r * 1.6f && (scale + 1) * labelH[b] <= c->r * 0.8f)
				++scale;
			const SDL_Rect dst = { (int)lroundf(c->x - scale * labelW[b] / 2.0f), (int)lroundf(c->y - scale * labelH[b] / 2.0f),
			                       scale * labelW[b], scale * labelH[b] };
			SDL_SetTextureAlphaMod(labelTexture[b], held[b] ? 255 : 210);
			SDL_RenderCopy(renderer, labelTexture[b], NULL, &dst);
		}
	}
}
