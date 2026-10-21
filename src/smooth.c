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
#include "smooth.h"

#include "backgrnd.h"
#include "levelrec.h"
#include "keyboard.h"
#include "logging.h"
#include "mouse.h"
#include "network.h"
#include "palette.h"
#include "nortsong.h"
#include "sprite.h"
#include "varz.h"
#include "vga256d.h"
#include "video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool smooth_motion = false;
bool smooth_recording = false;

// A draw of the frame.  dst and src are surfaces: 0 and 1 of targets[].
typedef struct
{
	Uint8 op;
	Uint8 dst, src;
	Uint8 tag;
	Uint16 id, part, occurrence;
	int x, y;
	intptr_t a, b, c, d;
} Entry;

// A busy frame draws about a thousand things (each 2x2 sprite counts four).
#define MAX_ENTRIES 16384

typedef struct
{
	Entry entries[MAX_ENTRIES];
	unsigned int count;
	bool incomplete;  // something was drawn that the list cannot redraw
} List;

static List lists[2];
static List *cur = &lists[0];   // the frame being drawn
static List *prev = &lists[1];  // the frame before it
static bool prevUsable = false;

// prev's tagged entries by identity.
#define HASH_SIZE 32768
static Sint32 prevIndex[HASH_SIZE];

// The playfield as it was when the last frame was shown: the list of the frame
// being drawn starts from it.
static Uint8 *snapshot[2] = { NULL, NULL };
static size_t snapshotSize = 0;
static bool haveSnapshot = false;

// Where the frame is redrawn, and the screen it goes on.
static SDL_Surface *scratch[2] = { NULL, NULL };
static SDL_Surface *compose = NULL;

// The surfaces that are recorded: in a level game_screen and VGAScreen2, in
// the weapon simulator of the ship menu the screen.
static SDL_Surface *targets[2] = { NULL, NULL };

// The weapon simulator: whether the frame being shown redraws, and the screen
// as the moving part of it was drawn.
static bool simFrame = false;
static bool simRedraws = false;
static Uint32 simLast = 0;

static SmoothTag curTag = SMOOTH_NONE;
static Uint16 curId = 0, curPart = 0, curOccurrence = 0;

// TYRIAN_SMOOTH_STATS=1: the log says how many pictures are shown.
static bool statsEnabled = false;
static unsigned long picturesShown = 0, framesDue = 0;
static Uint32 statsSince = 0;

// When the next picture is due on the display (in the units of nortsong.c),
// and whether showing one waits for the display (VSync).
static Uint32 nextPicture = 0;
static bool presentWaits = false;

static bool testEnabled = false;
static Uint32 testAlpha = 0;
static unsigned long testSame = 0, testDifferent = 0, testSkipped = 0;

// A thing that moved more than this between two frames jumped (it is a new
// thing in a slot that was used by another, or it wrapped around): it is drawn
// where it is, without sliding.
#define MAX_SLIDE 40

void smooth_tag(SmoothTag tag, unsigned int id, unsigned int part)
{
	curTag = tag;
	curId = id;
	curPart = part;
	curOccurrence = 0;
}

void smooth_untag(void)
{
	curTag = SMOOTH_NONE;
}

static int surfaceIndex(const SDL_Surface *surface)
{
	if (surface == NULL)
		return -1;
	if (surface == targets[0])
		return 0;
	if (surface == targets[1])
		return 1;
	return -1;
}

void smooth_record(SmoothOp op, SDL_Surface *dst, SDL_Surface *src, int x, int y,
                   intptr_t a, intptr_t b, intptr_t c, intptr_t d)
{
	const int dstIndex = surfaceIndex(dst);
	if (dstIndex < 0)
		return;  // the HUD and the menus are shown as they are

	int srcIndex = 0;
	if (src != NULL)
	{
		srcIndex = surfaceIndex(src);
		if (srcIndex < 0)
		{
			cur->incomplete = true;
			return;
		}
	}

	if (cur->count == MAX_ENTRIES)
	{
		cur->incomplete = true;
		return;
	}

	Entry *e = &cur->entries[cur->count++];
	e->op = op;
	e->dst = dstIndex;
	e->src = srcIndex;
	e->tag = curTag;
	e->id = curId;
	e->part = curPart;
	e->occurrence = curTag != SMOOTH_NONE ? curOccurrence++ : 0;
	e->x = x;
	e->y = y;
	e->a = a;
	e->b = b;
	e->c = c;
	e->d = d;
}

void smooth_forget_sprites(void)
{
	if (smooth_recording)
		cur->incomplete = true;
}

static Uint32 hashOf(const Entry *e)
{
	Uint32 h = ((Uint32)e->tag << 24) ^ ((Uint32)e->id << 12) ^ e->part ^ ((Uint32)e->occurrence << 20);
	h ^= h >> 15;
	h *= 0x2c1b3c6dU;
	h ^= h >> 12;
	return h & (HASH_SIZE - 1);
}

static bool sameThing(const Entry *a, const Entry *b)
{
	return a->tag == b->tag && a->id == b->id && a->part == b->part && a->occurrence == b->occurrence;
}

static void indexPrev(void)
{
	for (size_t i = 0; i < HASH_SIZE; ++i)
		prevIndex[i] = -1;

	for (unsigned int i = 0; i < prev->count; ++i)
	{
		const Entry *e = &prev->entries[i];
		if (e->tag == SMOOTH_NONE)
			continue;

		Uint32 h = hashOf(e);
		while (prevIndex[h] >= 0)
			h = (h + 1) & (HASH_SIZE - 1);
		prevIndex[h] = i;
	}
}

static const Entry *findPrev(const Entry *e)
{
	for (Uint32 h = hashOf(e); prevIndex[h] >= 0; h = (h + 1) & (HASH_SIZE - 1))
	{
		const Entry *p = &prev->entries[prevIndex[h]];
		if (sameThing(p, e))
			return p;
	}
	return NULL;
}

// The point at `alpha` (0..65536) of the way from `from` to `to`.
static int between(int from, int to, Uint32 alpha)
{
	return from + (int)(((Sint64)(to - from) * alpha + 32768) >> 16);
}

static void drawEntry(const Entry *e, int x, int y, intptr_t b, intptr_t c)
{
	SDL_Surface *dst = scratch[e->dst];
	SDL_Surface *src = scratch[e->src];

	switch ((SmoothOp)e->op)
	{
	case SMOOTH_OP_SPRITE:
		blit_sprite(dst, x, y, e->a, b);
		break;
	case SMOOTH_OP_SPRITE_BLEND:
		blit_sprite_blend(dst, x, y, e->a, b);
		break;
	case SMOOTH_OP_SPRITE_HV_UNSAFE:
		blit_sprite_hv_unsafe(dst, x, y, e->a, b, c, e->d);
		break;
	case SMOOTH_OP_SPRITE_HV:
		blit_sprite_hv(dst, x, y, e->a, b, c, e->d);
		break;
	case SMOOTH_OP_SPRITE_HV_BLEND:
		blit_sprite_hv_blend(dst, x, y, e->a, b, c, e->d);
		break;
	case SMOOTH_OP_SPRITE_DARK:
		blit_sprite_dark(dst, x, y, e->a, b, c);
		break;
	case SMOOTH_OP_SPRITE2:
	case SMOOTH_OP_SPRITE2_CLIP:
	case SMOOTH_OP_SPRITE2_BLEND:
	case SMOOTH_OP_SPRITE2_DARKEN:
	case SMOOTH_OP_SPRITE2_FILTER:
	case SMOOTH_OP_SPRITE2_FILTER_CLIP:
	{
		const Sprite2_array sheet = { .size = (size_t)e->a, .data = (Uint8 *)b };
		switch ((SmoothOp)e->op)
		{
		case SMOOTH_OP_SPRITE2:
			blit_sprite2(dst, x, y, sheet, c);
			break;
		case SMOOTH_OP_SPRITE2_CLIP:
			blit_sprite2_clip(dst, x, y, sheet, c);
			break;
		case SMOOTH_OP_SPRITE2_BLEND:
			blit_sprite2_blend(dst, x, y, sheet, c);
			break;
		case SMOOTH_OP_SPRITE2_DARKEN:
			blit_sprite2_darken(dst, x, y, sheet, c);
			break;
		case SMOOTH_OP_SPRITE2_FILTER:
			blit_sprite2_filter(dst, x, y, sheet, c, e->d);
			break;
		default:
			blit_sprite2_filter_clip(dst, x, y, sheet, c, e->d);
			break;
		}
		break;
	}
	case SMOOTH_OP_PIX:
		JE_pix(dst, x, y, e->a);
		break;
	case SMOOTH_OP_RECTANGLE:
		JE_rectangle(dst, x, y, e->a, b, c);
		break;
	case SMOOTH_OP_FILL:
		fill_rectangle_xy(dst, x, y, e->a, b, c);
		break;
	case SMOOTH_OP_BAR_SHADE:
		JE_barShade(dst, x, y, e->a, b);
		break;
	case SMOOTH_OP_BAR_BRIGHT:
		JE_barBright(dst, x, y, e->a, b);
		break;
	case SMOOTH_OP_CLEAR:
		JE_clr256(dst);
		break;
	case SMOOTH_OP_BACKGROUND:
		draw_background_layer(dst, e->a, (Uint8 **)b, c, x, y, e->d);
		break;
	case SMOOTH_OP_DARKEN_BACKGROUND:
		darken_background(dst, e->a);
		break;
	case SMOOTH_OP_FILTER_HUE:
		filterScreenHue(dst, e->a);
		break;
	case SMOOTH_OP_FILTER_VALUE:
		filterScreenValue(dst, e->a);
		break;
	case SMOOTH_OP_LAVA:
		lava_filter(dst, src);
		break;
	case SMOOTH_OP_WATER:
		water_filter(dst, src);
		break;
	case SMOOTH_OP_ICED_BLUR:
		iced_blur_filter(dst, src);
		break;
	case SMOOTH_OP_BLUR:
		blur_filter(dst, src);
		break;
	case SMOOTH_OP_STAR:
		draw_starfield_star(dst, x, y, e->a);
		break;
	case SMOOTH_OP_SUPERPIXEL:
		draw_superpixel(dst, x, y, e->a, b);
		break;
	case SMOOTH_OP_COUNT:
		break;
	}
}

// A background layer at `alpha` of the way from its last frame to this one.
// Its position is the tile row (b, a pointer into the map, `stride` tiles per
// row), the tile column (c) and the offset in pixels inside the tile (x, y).
static void drawBackgroundBetween(const Entry *e, const Entry *p, Uint32 alpha)
{
	Uint8 **row = (Uint8 **)e->b;
	intptr_t column = e->c;
	int x = e->x, y = e->y;

	const intptr_t stride = e->d;
	const ptrdiff_t rows = (Uint8 **)p->b - row;  // the map scrolls by moving back
	if (p->d == stride && p->a == e->a && rows % stride == 0)
	{
		// how far the tiles moved down and right since the last frame
		const int dy = (e->y - p->y) + 28 * (int)(rows / stride);
		const int dx = (e->x - 24 * (int)e->c) - (p->x - 24 * (int)p->c);

		if (abs(dy) <= 2 * 28 && abs(dx) <= 2 * 24)
		{
			y -= dy - between(0, dy, alpha);
			x -= dx - between(0, dx, alpha);

			// back into a tile, between the two frames' tiles
			while (y < 0)
			{
				y += 28;
				row += stride;
			}
			while (y >= 28)
			{
				y -= 28;
				row -= stride;
			}
			while (x < 0)
			{
				x += 24;
				column += 1;
			}
			while (x >= 24)
			{
				x -= 24;
				column -= 1;
			}
		}
	}

	draw_background_layer(scratch[e->dst], e->a, row, column, x, y, stride);
}

// Redraws the frame being drawn (cur) from the snapshot, with everything that
// moved at `alpha` of the way from the last frame (65536: where it is now).
static void redraw(Uint32 alpha)
{
	memcpy(scratch[0]->pixels, snapshot[0], snapshotSize);
	memcpy(scratch[1]->pixels, snapshot[1], snapshotSize);

	const bool wasRecording = smooth_recording;
	smooth_recording = false;

	for (unsigned int i = 0; i < cur->count; ++i)
	{
		const Entry *e = &cur->entries[i];
		const Entry *p = (alpha < 65536 && e->tag != SMOOTH_NONE) ? findPrev(e) : NULL;

		if (p == NULL || p->op != e->op)
		{
			drawEntry(e, e->x, e->y, e->b, e->c);
		}
		else if (e->op == SMOOTH_OP_BACKGROUND)
		{
			drawBackgroundBetween(e, p, alpha);
		}
		else if (abs(e->x - p->x) > MAX_SLIDE || abs(e->y - p->y) > MAX_SLIDE)
		{
			drawEntry(e, e->x, e->y, e->b, e->c);
		}
		else
		{
			drawEntry(e, between(p->x, e->x, alpha), between(p->y, e->y, alpha), e->b, e->c);
		}
	}

	smooth_recording = wasRecording;
}

static bool ensureBuffers(void)
{
	const size_t size = (size_t)game_screen->pitch * game_screen->h;
	if (snapshot[0] != NULL && snapshotSize == size)
		return true;

	free(snapshot[0]);
	free(snapshot[1]);
	snapshot[0] = malloc(size);
	snapshot[1] = malloc(size);
	snapshotSize = size;

	for (int i = 0; i < 2; ++i)
	{
		if (scratch[i] == NULL)
			scratch[i] = SDL_CreateRGBSurface(0, game_screen->w, game_screen->h, 8, 0, 0, 0, 0);
	}
	if (compose == NULL)
		compose = SDL_CreateRGBSurface(0, VGAScreenSeg->w, VGAScreenSeg->h, 8, 0, 0, 0, 0);

	return snapshot[0] != NULL && snapshot[1] != NULL &&
	       scratch[0] != NULL && scratch[1] != NULL && compose != NULL &&
	       (size_t)scratch[0]->pitch * scratch[0]->h == size;
}

static bool wanted(void)
{
	return (smooth_motion || testEnabled) && !isNetworkGame && !levelrec_fast();
}

void smooth_level_begin(void)
{
	statsEnabled = getenv("TYRIAN_SMOOTH_STATS") != NULL;

	cur->count = 0;
	cur->incomplete = false;
	prev->count = 0;
	prevUsable = false;
	haveSnapshot = false;
	simFrame = false;
	targets[0] = game_screen;
	targets[1] = VGAScreen2;
	smooth_untag();
	smooth_recording = wanted() && ensureBuffers();
}

void smooth_level_end(void)
{
	smooth_recording = false;
	prevUsable = false;
	haveSnapshot = false;
	smooth_untag();
}

// Whether redrawing the list gives the frame that was drawn.
static bool frameRedraws(void)
{
	if (!haveSnapshot || cur->incomplete)
		return false;

	redraw(65536);
	return memcmp(scratch[0]->pixels, game_screen->pixels, snapshotSize) == 0 &&
	       memcmp(scratch[1]->pixels, VGAScreen2->pixels, snapshotSize) == 0;
}

// The time between two pictures on the display, in the units of nortsong.c
// (1/1024 ms).
static Sint32 refreshPeriod(void)
{
	int hz = 0;
	SDL_DisplayMode mode;
	const int display = SDL_GetWindowDisplayIndex(main_window);
	if (display >= 0 && SDL_GetCurrentDisplayMode(display, &mode) == 0)
		hz = mode.refresh_rate;
	const char *forced = getenv("TYRIAN_SMOOTH_HZ");  // for the tests
	if (forced != NULL)
		hz = atoi(forced);
	if (hz < 24 || hz > 1000)
		hz = 60;
	return (1000 << 10) / hz;
}

static void showBetween(Uint32 alpha, int view)
{
	redraw(alpha);

	memcpy(compose->pixels, VGAScreenSeg->pixels, (size_t)compose->pitch * compose->h);
	if (view == 1)
		showFlipped(compose, scratch[0]);
	else if (view == 2)
		showHeadlight(compose, scratch[0], player[0].x, player[0].y);
	else
		showNormal(compose, scratch[0]);
	video_present(compose);
}

// Until the frame that lasts `period` is due, a picture per refresh of the
// display, each drawn and shown by show(alpha): never faster, even without
// VSync, and never one that would show after the frame is due, so the game
// keeps its own timing.  With VSync, showing a picture waits for the display,
// which says when the next one is due.
static bool showUntilDue(Sint32 period, Sint32 refresh, void (*show)(Uint32 alpha, const void *data), const void *data)
{
	bool shown = false;

	for (;;)
	{
		const Uint32 now = SDL_GetTicks() << 10;
		const Uint32 due = now + frame_time_left();

		// lost track (a pause, a slow frame, a new level)?
		if ((Sint32)(now - nextPicture) > 4 * refresh || (Sint32)(nextPicture - now) > 4 * refresh)
			nextPicture = now;
		while ((Sint32)(nextPicture - now) < 0)
			nextPicture += refresh;

		if (period <= 0 || (Sint32)(due - nextPicture) < 0)
			break;  // it would show after the frame is due

		if (!presentWaits)
		{
			const Sint32 early = nextPicture - now;
			if (early >= 1024)
				SDL_Delay((Uint32)early >> 10);
		}

		// how far the game is from the last frame to this one when it shows
		const Sint32 since = period - (Sint32)(due - nextPicture);
		const Uint32 alpha = since <= 0 ? 0 : since >= period ? 65536 : (Uint32)(((Sint64)since << 16) / period);

		const Uint64 before = SDL_GetPerformanceCounter();
		show(alpha, data);
		const Uint64 took = SDL_GetPerformanceCounter() - before;
		shown = true;
		++picturesShown;

		// a picture that took more than half a millisecond waited for the display
		presentWaits = vsync_enabled && took * 2000 > SDL_GetPerformanceFrequency();
		if (presentWaits)
			nextPicture = (SDL_GetTicks() << 10) + refresh;
		else
			nextPicture += refresh;
	}

	return shown;
}

// TYRIAN_SMOOTH_STATS: a frame was due.
static void countFrame(Sint32 refresh)
{
	statsEnabled = statsEnabled || getenv("TYRIAN_SMOOTH_STATS") != NULL;
	if (!statsEnabled)
		return;

	++framesDue;
	const Uint32 now = SDL_GetTicks();
	if (now - statsSince >= 5000)
	{
		logInfo("Smooth motion: %lu pictures and %lu frames in %u ms (display: %d Hz)",
		        picturesShown, framesDue, now - statsSince, (int)((1000 << 10) / refresh));
		picturesShown = framesDue = 0;
		statsSince = now;
	}
}

static void showLevelPicture(Uint32 alpha, const void *data)
{
	showBetween(alpha, *(const int *)data);
}

// SMOOTH_DUMP=<dir>:<first>:<count>: the tests save the pictures between frame
// <first> and the <count> after it, at a quarter of the way each, to look at.
static void dumpPictures(int view)
{
	static const char *dir = NULL;
	static long first = -1, count = 0, frame = 0;

	if (first < 0)
	{
		const char *spec = getenv("SMOOTH_DUMP");
		static char buf[512];
		if (spec == NULL || strlen(spec) >= sizeof buf)
		{
			first = 0;
			return;
		}
		strcpy(buf, spec);
		char *colon = strchr(buf, ':');
		if (colon == NULL)
			return;
		*colon = '\0';
		dir = buf;
		first = strtol(colon + 1, &colon, 10);
		count = *colon == ':' ? strtol(colon + 1, NULL, 10) : 1;
	}

	++frame;
	if (dir == NULL || frame < first || frame >= first + count)
		return;

	SDL_Color palette[256];
	for (int i = 0; i < 256; ++i)
		palette[i] = (SDL_Color){ colors[i].r, colors[i].g, colors[i].b, 255 };
	SDL_SetPaletteColors(compose->format->palette, palette, 0, 256);

	for (Uint32 quarter = 0; quarter < 4; ++quarter)
	{
		redraw(quarter * 16384);
		memcpy(compose->pixels, VGAScreenSeg->pixels, (size_t)compose->pitch * compose->h);
		if (view == 1)
			showFlipped(compose, scratch[0]);
		else if (view == 2)
			showHeadlight(compose, scratch[0], player[0].x, player[0].y);
		else
			showNormal(compose, scratch[0]);

		char path[600];
		snprintf(path, sizeof path, "%s/frame%05ld-%u.bmp", dir, frame, quarter);
		SDL_SaveBMP(compose, path);
	}
}

bool smooth_frame_due(int view)
{
	if (!smooth_recording)
		return false;

	const bool same = frameRedraws();

	if (testEnabled)
	{
		if (!haveSnapshot || cur->incomplete)
			++testSkipped;
		else if (same)
			++testSame;
		else
			++testDifferent;

		if (same && prevUsable)
		{
			redraw(testAlpha);  // the pictures in between must draw too
			dumpPictures(view);
		}
		return false;
	}

	if (!same || !prevUsable)
		return false;

	const Sint32 refresh = refreshPeriod();
	const bool shown = showUntilDue(frame_period() * frameCountMax, refresh, showLevelPicture, &view);

	countFrame(refresh);

	return shown;
}

void smooth_frame_shown(void)
{
	if (!smooth_recording)
	{
		// smooth motion was turned on in the middle of a level
		if (wanted())
			smooth_level_begin();
		return;
	}
	if (!wanted())
	{
		smooth_level_end();
		return;
	}

	memcpy(snapshot[0], game_screen->pixels, snapshotSize);
	memcpy(snapshot[1], VGAScreen2->pixels, snapshotSize);
	haveSnapshot = true;

	List *const t = prev;
	prev = cur;
	cur = t;
	cur->count = 0;
	cur->incomplete = false;
	prevUsable = prev->count > 0;
	if (prevUsable)
		indexPrev();

	smooth_untag();
}

// Outside the levels.
static bool menuWanted(void)
{
	return smooth_motion && !isNetworkGame && !levelrec_fast();
}

bool smooth_show_until_due(int frames, void (*show)(Uint32 alpha, const void *data), const void *data)
{
	if (!menuWanted())
		return false;

	const Sint32 refresh = refreshPeriod();
	const bool shown = showUntilDue(frames * frame_period(), refresh, show, data);
	countFrame(refresh);
	return shown;
}

Uint32 smooth_picture_wait(void)
{
	if (!menuWanted())
		return UINT32_MAX;

	const Sint32 refresh = refreshPeriod();
	const Uint32 now = SDL_GetTicks() << 10;
	if ((Sint32)(now - nextPicture) > 4 * refresh || (Sint32)(nextPicture - now) > 4 * refresh)
		nextPicture = now;

	const Sint32 early = nextPicture - now;
	return early <= 0 ? 0 : ((Uint32)early + 1023) >> 10;
}

void smooth_picture_shown(void)
{
	const Sint32 refresh = refreshPeriod();
	const Uint32 now = SDL_GetTicks() << 10;
	nextPicture += refresh;
	while ((Sint32)(nextPicture - now) < 0)
		nextPicture += refresh;
	++picturesShown;
}

// The weapon simulator draws inside the frame of the upgrade interface, at the
// left of the ship menu (fill_rectangle_xy in JE_weaponViewFrame).
#define SIM_X1 8
#define SIM_Y1 8
#define SIM_X2 143
#define SIM_Y2 182

void smooth_sim_begin(void)
{
	// a frame of the simulator shortly after the last one goes on from it
	const Uint32 now = SDL_GetTicks();
	const bool goingOn = simFrame && now - simLast < 250;
	simLast = now;

	simFrame = menuWanted() && ensureBuffers() &&
	           (size_t)VGAScreen->pitch * VGAScreen->h == snapshotSize &&
	           (size_t)compose->pitch * compose->h == snapshotSize;
	if (!simFrame)
		return;

	List *const t = prev;
	prev = cur;
	cur = t;
	cur->count = 0;
	cur->incomplete = false;
	prevUsable = goingOn && simRedraws && prev->count > 0;
	if (prevUsable)
		indexPrev();

	// the list of this frame starts from the screen as it is
	targets[0] = VGAScreen;
	targets[1] = NULL;
	memcpy(snapshot[0], VGAScreen->pixels, snapshotSize);
	haveSnapshot = true;
	simRedraws = false;

	smooth_untag();
	smooth_recording = true;
}

void smooth_sim_drawn(void)
{
	if (!simFrame)
		return;

	smooth_recording = false;
	smooth_untag();

	memcpy(snapshot[1], VGAScreen->pixels, snapshotSize);
	if (cur->incomplete)
		return;

	redraw(65536);
	simRedraws = memcmp(scratch[0]->pixels, snapshot[1], snapshotSize) == 0;
}

static void showSimPicture(Uint32 alpha, const void *data)
{
	(void)data;

	redraw(alpha);

	// what was drawn over the simulator after it (the interface, the ship)
	// stays as it is; the mouse pointer goes where the mouse is now
	const Uint8 *const drawn = snapshot[1];
	const Uint8 *const between = scratch[0]->pixels;
	Uint8 *const out = compose->pixels;
	memcpy(out, VGAScreen->pixels, snapshotSize);
	mouse_pointer_remove(compose);
	for (int y = SIM_Y1; y <= SIM_Y2; ++y)
	{
		for (int x = SIM_X1; x <= SIM_X2; ++x)
		{
			const size_t i = (size_t)y * compose->pitch + x;
			if (out[i] == drawn[i])
				out[i] = between[i];
		}
	}
	handleSdlEvents();
	mouse_pointer_draw_now(compose);

	video_present(compose);
}

bool smooth_sim_show(int frames)
{
	if (!simFrame || !simRedraws || !prevUsable || !menuWanted())
		return false;

	const Sint32 refresh = refreshPeriod();
	const bool shown = showUntilDue(frames * frame_period(), refresh, showSimPicture, NULL);
	countFrame(refresh);
	return shown;
}

void smooth_test_enable(bool enable, Uint32 alpha)
{
	testEnabled = enable;
	testAlpha = alpha;
	testSame = testDifferent = testSkipped = 0;
}

void smooth_test_counts(unsigned long *same, unsigned long *different, unsigned long *skipped)
{
	*same = testSame;
	*different = testDifferent;
	*skipped = testSkipped;
}
