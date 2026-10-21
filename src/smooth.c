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

bool smooth_motion = true;
bool smooth_motion_chosen = false;
bool smooth_fine = true;
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

// What a lead is for: background layers 1-3 and the enemies that move with them.
enum { LEAD_NONE, LEAD_LAYER1, LEAD_LAYER2, LEAD_LAYER3, LEAD_GROUND, LEAD_SKY, LEAD_TOP, LEADS };

typedef struct
{
	Entry entries[MAX_ENTRIES];
	unsigned int count;
	bool incomplete;  // something was drawn that the list cannot redraw
	// how far things are drawn from where they are, in 1/65536 pixels (LEAD_*)
	Sint32 leadX[LEADS], leadY[LEADS];
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

static Sint32 refreshPeriod(void);

// How long the frame being drawn lasts (frame_period() * frameCountMax when it
// was scheduled: at the "Slow" game speed, frameCountMax changes every frame).
static Sint32 levelFramePeriod = 0;

static bool testEnabled = false;
static Uint32 testAlpha = 0;
static unsigned long testFrames = 0, testFineDifferent = 0;  // finer pictures checked, and wrong
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

// Finer pictures: the picture in between is drawn `fineScale` times bigger
// than the game's screen, so a thing can stand a fraction of a pixel of the
// game away from where it was (the farthest background moves one pixel per
// frame: without this it would not move in between at all).  Each pixel of
// the game becomes fineScale x fineScale pixels; the pixels at the same place
// (phase) in each of them make a picture the size of the game's screen, and
// in it a thing a fraction of a pixel to the right shows either where it is or
// one pixel to the right.  So the big picture is the list redrawn once per
// phase with the game's own drawing, and the phases woven together.
static int fineScale = 1, phaseX = 0, phaseY = 0;

static int floorDiv(int a, int b)
{
	return a >= 0 ? a / b : -((-a + b - 1) / b);
}

// Where to draw, in the phase being drawn, a thing `alpha` of the way from
// `from` to `to`.
static int phasedLead(int from, int to, Sint32 leadFrom, Sint32 leadTo, Uint32 alpha, int phase)
{
	// in 1/65536 of a fine pixel
	const Sint64 fineFrom = ((Sint64)from * 65536 + leadFrom) * fineScale;
	const Sint64 fineTo = ((Sint64)to * 65536 + leadTo) * fineScale;
	const Sint64 at = fineFrom + (((fineTo - fineFrom) * alpha) >> 16);
	const int fine = (int)((at + 32768) >> 16);
	return floorDiv(fine - phase + fineScale - 1, fineScale);
}

// A layer that steps a pixel every few frames (map1YDelayMax, map2YDelayMax)
// would stand still and then jump; drawn `lead` pixels (/65536) from where it
// is, it moves the same amount each frame.  Its ground enemies step with it.
// The layers also slide sideways by a fraction of the ship's movement
// (parallax), rounded down to whole pixels: the fraction is a lead too.
static unsigned int leadIndex(const Entry *e)
{
	if (e->tag == SMOOTH_BACKGROUND && e->id >= 1 && e->id <= 3)
		return LEAD_LAYER1 + e->id - 1;
	if (e->tag == SMOOTH_ENEMY && e->id < 100)
		return e->id < 25 ? LEAD_SKY : (e->id >= 50 && e->id < 75) ? LEAD_TOP : LEAD_GROUND;  // JE_drawEnemy's slots
	return LEAD_NONE;
}

static Sint32 leadXOf(const List *list, const Entry *e)
{
	return list->leadX[leadIndex(e)];
}

static Sint32 leadOf(const List *list, const Entry *e)
{
	return list->leadY[leadIndex(e)];
}

// the sideways leads of layers 1-3, as of the ship's last movement and the one before
static Sint32 parallax[4], parallaxBefore[4];

void smooth_parallax(float exact, int ofs1, int ofs2, int ofs3, bool layer3AsLayer1)
{
	memcpy(parallaxBefore, parallax, sizeof parallax);
	const float exacts[4] = { 0, exact / 3, exact * 2 / 3, exact };
	const int ofs[4] = { 0, ofs1, ofs2, ofs3 };
	for (int i = 1; i <= 3; ++i)
	{
		const float lead = exacts[i] - ofs[i];
		parallax[i] = lead > -2 && lead < 2 ? (Sint32)(lead * 65536) : 0;  // not at the edges, where it wraps
	}
	if (layer3AsLayer1)
		parallax[3] = parallax[1];
}

void smooth_enemies_layer(int enemyOffset, int layer, bool before)
{
	if (!smooth_recording || layer < 1 || layer > 3)
		return;
	const unsigned int index = enemyOffset == 25 ? LEAD_SKY : enemyOffset == 75 ? LEAD_TOP : LEAD_GROUND;
	cur->leadX[index] = (before ? parallaxBefore : parallax)[layer];
}

void smooth_layer_lead(unsigned int layer, unsigned int xLayer, int delay, int delayMax, int move)
{
	if (!smooth_recording || layer < 1 || layer > 3 || xLayer < 1 || xLayer > 3)
		return;
	cur->leadX[LEAD_LAYER1 + layer - 1] = parallax[xLayer];
	if (delayMax <= 1)
	{
		cur->leadY[LEAD_LAYER1 + layer - 1] = 0;
		if (layer == 1)
			cur->leadY[LEAD_GROUND] = 0;
		return;
	}
	// `steps` frames since the layer last moved, centered on where it is drawn
	const int steps = delayMax - delay;
	const int pixels = move >= 2 ? move : 1;
	cur->leadY[LEAD_LAYER1 + layer - 1] = (Sint32)((Sint64)(2 * steps - (delayMax - 1)) * pixels * 65536 / (2 * delayMax));
	// the ground enemies step a frame before the layer (when delay is 1)
	if (layer == 1)
		cur->leadY[LEAD_GROUND] = (Sint32)((Sint64)(2 * ((steps + 1) % delayMax) - (delayMax - 1)) * pixels * 65536 / (2 * delayMax));
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

// How far a background layer's tiles moved down and right since the last
// frame, if it is the same layer a little further on.
static bool backgroundMoved(const Entry *e, const Entry *p, int *dx, int *dy)
{
	const intptr_t stride = e->d;
	const ptrdiff_t rows = (Uint8 **)p->b - (Uint8 **)e->b;  // the map scrolls by moving back
	if (p->d != stride || p->a != e->a || rows % stride != 0)
		return false;
	*dy = (e->y - p->y) + 28 * (int)(rows / stride);
	*dx = (e->x - 24 * (int)e->c) - (p->x - 24 * (int)p->c);
	return abs(*dy) <= 2 * 28 && abs(*dx) <= 2 * 24;
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

	int dx, dy;
	if (backgroundMoved(e, p, &dx, &dy))
	{
		y = phasedLead(y - dy, y, leadOf(prev, p), leadOf(cur, e), alpha, phaseY);
		x = phasedLead(x - dx, x, leadXOf(prev, p), leadXOf(cur, e), alpha, phaseX);

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

	draw_background_layer(scratch[e->dst], e->a, row, column, x, y, stride);
}

// Whether p (last frame) and e (this one) can be drawn in between: the same
// kind of drawing, or two sprites drawn differently (an enemy flashes when it
// is hit: blit_sprite2 one frame, blit_sprite2_filter the next).
static bool sameKind(const Entry *p, const Entry *e)
{
	if (p->op == e->op)
		return true;
	const bool sprite = e->op <= SMOOTH_OP_SPRITE2_FILTER_CLIP, spriteBefore = p->op <= SMOOTH_OP_SPRITE2_FILTER_CLIP;
	return sprite && spriteBefore;
}

// Redraws the frame being drawn (cur) from the snapshot, with everything that
// moved at `alpha` of the way from the last frame (65536: where it is now), in
// the phase (x, y) of a picture `scale` times bigger (1: the game's size).
static void redrawPhase(Uint32 alpha, int scale, int x, int y)
{
	fineScale = scale;
	phaseX = x;
	phaseY = y;

	memcpy(scratch[0]->pixels, snapshot[0], snapshotSize);
	memcpy(scratch[1]->pixels, snapshot[1], snapshotSize);

	const bool wasRecording = smooth_recording;
	smooth_recording = false;

	for (unsigned int i = 0; i < cur->count; ++i)
	{
		const Entry *e = &cur->entries[i];
		const Entry *p = (alpha < 65536 && e->tag != SMOOTH_NONE) ? findPrev(e) : NULL;

		if (p == NULL || !sameKind(p, e))
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
			drawEntry(e, phasedLead(p->x, e->x, leadXOf(prev, p), leadXOf(cur, e), alpha, phaseX), phasedLead(p->y, e->y, leadOf(prev, p), leadOf(cur, e), alpha, phaseY), e->b, e->c);
		}
	}

	smooth_recording = wasRecording;
}

static void redraw(Uint32 alpha)
{
	redrawPhase(alpha, 1, 0, 0);
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

	const bool ok = snapshot[0] != NULL && snapshot[1] != NULL &&
	                scratch[0] != NULL && scratch[1] != NULL && compose != NULL &&
	                (size_t)scratch[0]->pitch * scratch[0]->h == size;
	if (!ok)
	{
		// try again next time, instead of remembering half of the buffers
		free(snapshot[0]);
		free(snapshot[1]);
		snapshot[0] = snapshot[1] = NULL;
		snapshotSize = 0;
	}
	return ok;
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
	memset(cur->leadX, 0, sizeof(cur->leadX));
	memset(cur->leadY, 0, sizeof(cur->leadY));
	prev->count = 0;
	prevUsable = false;
	haveSnapshot = false;
	simFrame = false;
	targets[0] = game_screen;
	targets[1] = VGAScreen2;
	smooth_untag();
	smooth_recording = wanted() && ensureBuffers();

	// once, so the log says what the display was taken for (a Steam Deck
	// OLED reported as 60 Hz would show fewer pictures than it could)
	static bool refreshLogged = false;
	if (smooth_recording && !refreshLogged)
	{
		refreshLogged = true;
		logInfo("Smooth motion: on, display at %d Hz.", (int)((1000 << 10) / refreshPeriod()));
	}
}

static void judderLevelEnd(void);

void smooth_level_end(void)
{
	judderLevelEnd();
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

// The screen (the HUD, and the playfield redrawn in scratch[0]) in compose.
static void composeScreen(int view)
{
	memcpy(compose->pixels, VGAScreenSeg->pixels, (size_t)compose->pitch * compose->h);
	if (view == 1)
		showFlipped(compose, scratch[0]);
	else if (view == 2)
		showHeadlight(compose, scratch[0], player[0].x, player[0].y);
	else
		showNormal(compose, scratch[0]);
}

// The finer picture (fineScale x the screen), and the scale it was made at.
static Uint8 *fine = NULL;
static size_t fineSize = 0;
static int fineLimit = SMOOTH_FINE_MAX;  // lowered when the pictures take too long
static Uint64 renderTook = 0;  // how long the last picture took to draw (not to show)
static Uint64 renderSum = 0, totalSum = 0;
static Sint32 slowPicture = 0;  // how long a picture took when it took longer than a refresh (ms << 10)
static unsigned int renderCount = 0;
static Uint64 statsRender = 0, statsShow = 0;  // TYRIAN_SMOOTH_STATS: time drawing and showing
static int statsScale = 1;

// How much finer the pictures in between are drawn (1: as the game).
static int pictureScale(void)
{
	if (!smooth_fine)
		return 1;
	const int scale = video_fine_scale();
	return scale < fineLimit ? scale : fineLimit;
}

// The picture `alpha` of the way from the last frame, `scale` times finer, in
// fine (its pixels as the game's).
static bool drawFine(Uint32 alpha, int view, int scale)
{
	const int w = compose->w * scale, h = compose->h * scale;
	if (fineSize < (size_t)w * h)
	{
		free(fine);
		fine = malloc((size_t)w * h);
		fineSize = fine != NULL ? (size_t)w * h : 0;
		if (fine == NULL)
			return false;
	}

	for (int y = 0; y < scale; ++y)
	{
		for (int x = 0; x < scale; ++x)
		{
			redrawPhase(alpha, scale, x, y);
			composeScreen(view);

			// woven into the finer picture
			const Uint8 *src = compose->pixels;
			Uint8 *dst = fine + (size_t)y * w + x;
			for (int row = 0; row < compose->h; ++row)
			{
				for (int col = 0; col < compose->w; ++col)
					dst[col * scale] = src[col];
				src += compose->pitch;
				dst += (size_t)w * scale;
			}
		}
	}
	return true;
}

static void showBetween(Uint32 alpha, int view)
{
	const Uint64 before = SDL_GetPerformanceCounter();
	const int scale = pictureScale();

	if (scale > 1 && drawFine(alpha, view, scale))
	{
		renderTook = SDL_GetPerformanceCounter() - before;
		video_present_fine(fine, compose->w * scale, scale);
	}
	else
	{
		redraw(alpha);
		composeScreen(view);
		renderTook = SDL_GetPerformanceCounter() - before;
		video_present(compose);
	}
	const Uint64 total = SDL_GetPerformanceCounter() - before;
	statsRender += renderTook;
	statsShow += total - renderTook;
	statsScale = scale;

	// Waiting for the display takes up to a refresh; a picture that takes
	// longer is slow to show (a computer without a graphics card), so the
	// next ones start early enough to be shown before the frame is due.
	const Uint64 budget = (Uint64)refreshPeriod() * SDL_GetPerformanceFrequency() / (1000 << 10);
	slowPicture = total * 4 > budget * 5 ? (Sint32)(total * (1000 << 10) / SDL_GetPerformanceFrequency()) : 0;

	if (scale > 1)
	{
		// too slow for the display: one step coarser (16 phases to 9, to 4)
		renderSum += renderTook;
		totalSum += total;
		if (++renderCount == 16)
		{
			const Uint64 average = renderSum / renderCount, averageTotal = totalSum / renderCount;
			if (average * 2 > budget || averageTotal * 4 > budget * 5)
			{
				fineLimit = scale - 1;
				logInfo("Smooth motion: pictures %dx finer took %.1f ms to draw and %.1f ms in all, of %.1f; now %dx.",
				        scale, average * 1000.0 / SDL_GetPerformanceFrequency(),
				        averageTotal * 1000.0 / SDL_GetPerformanceFrequency(),
				        budget * 1000.0 / SDL_GetPerformanceFrequency(), fineLimit);
			}
			renderSum = totalSum = 0;
			renderCount = 0;
		}
	}
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

		if (period <= 0 || (Sint32)(due - nextPicture - slowPicture) < 0)
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
		renderTook = 0;
		show(alpha, data);
		const Uint64 took = SDL_GetPerformanceCounter() - before - renderTook;
		shown = true;
		++picturesShown;

		// a picture that took more than half a millisecond waited for the display
		presentWaits = vsync_enabled && took * 2000 > SDL_GetPerformanceFrequency();
		if (presentWaits)
		{
			// a refresh after the display showed it, and never sooner than
			// one after the last (so this ends even if the clock stood still)
			const Uint32 after = (SDL_GetTicks() << 10) + refresh;
			nextPicture += refresh;
			if ((Sint32)(after - nextPicture) > 0)
				nextPicture = after;
		}
		else
			nextPicture += refresh;
	}

	if (!shown)
		slowPicture /= 2;  // try again: the slow one may have been a one-off

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
		const double ms = picturesShown > 0 ? 1000.0 / SDL_GetPerformanceFrequency() / picturesShown : 0;
		logInfo("Smooth motion: %lu pictures and %lu frames in %u ms (display: %d Hz); %dx, %.2f ms drawing and %.2f ms showing each",
		        picturesShown, framesDue, now - statsSince, (int)((1000 << 10) / refresh),
		        statsScale, statsRender * ms, statsShow * ms);
		statsRender = statsShow = 0;
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

		// SMOOTH_DUMP_FINE=<scale>: the finer pictures too
		const char *fineDump = getenv("SMOOTH_DUMP_FINE");
		const int scale = fineDump != NULL ? atoi(fineDump) : 0;
		if (scale > 1 && scale <= SMOOTH_FINE_MAX && drawFine(quarter * 16384, view, scale))
		{
			SDL_Surface *big = SDL_CreateRGBSurfaceFrom(fine, compose->w * scale, compose->h * scale, 8, compose->w * scale, 0, 0, 0, 0);
			if (big != NULL)
			{
				SDL_SetPaletteColors(big->format->palette, palette, 0, 256);
				snprintf(path, sizeof path, "%s/frame%05ld-%u-fine.bmp", dir, frame, quarter);
				SDL_SaveBMP(big, path);
				SDL_FreeSurface(big);
			}
		}
	}
}

// The tests also look for things whose speed zigzags: a thing that moves 1
// pixel on one frame and 0 or 2 on the next, again and again (such as a layer
// that steps every few frames), still judders in the pictures in between.
// Each thing seen in consecutive frames is followed; at the end of the level,
// those that zigzagged in a quarter of a second or more of frames are logged.
#define TRACKS 8192
typedef struct
{
	Uint64 key;  // 0: free
	unsigned long lastFrame;
	Sint32 lastDx, lastDy, lastAx, lastAy;
	unsigned int frames, stopGo, moving;
	Uint16 enemyType;
} Track;
static Track tracks[TRACKS];
static unsigned long judderTotal[16];
static Uint32 judderThings[24];  // logged in this level
static unsigned int judderLogged = 0;

static const char *const tagNames[] = {
	"none", "background", "enemy", "player", "sidekick", "player shot", "enemy shot",
	"explosion", "rep explosion", "star", "superpixel",
};

static void judderReport(Track *t)
{
	// at least half the frames moving, and zigzagging speed in a quarter of them
	if (t->frames >= 35 && t->moving * 2 >= t->frames && t->stopGo * 4 >= t->frames)
	{
		const unsigned int tag = (unsigned int)(t->key >> 48);
		++judderTotal[tag & 15];
		const Uint32 thing = (Uint32)(t->key >> 32);  // the tag and id, whatever the part
		bool logged = false;
		for (unsigned int i = 0; i < judderLogged && i < COUNTOF(judderThings); ++i)
			logged = logged || judderThings[i] == thing;
		const bool fresh = !logged && judderLogged < COUNTOF(judderThings);
		if (fresh)
			judderThings[judderLogged++] = thing;
		if (fresh && tag != SMOOTH_PLAYER)  // the tests' pilot moves the ship in fits and starts
			logInfo("Smooth motion test: %s: %s %u (part %u, type %u) speed zigzags in %u of %u frames",
			        levelName, tag < COUNTOF(tagNames) ? tagNames[tag] : "?", (unsigned int)(t->key >> 32) & 0xffff,
			        (unsigned int)t->key & 0xffff, t->enemyType, t->stopGo, t->frames);
	}
	t->frames = t->stopGo = t->moving = 0;
}

static void judderFrame(void)
{
	for (unsigned int i = 0; i < cur->count; ++i)
	{
		const Entry *e = &cur->entries[i];
		if (e->tag == SMOOTH_NONE || e->occurrence != 0)
			continue;
		const Entry *p = findPrev(e);
		if (p == NULL || !sameKind(p, e))
			continue;

		int dx, dy;
		if (e->op == SMOOTH_OP_BACKGROUND)
		{
			if (!backgroundMoved(e, p, &dx, &dy))
				continue;
		}
		else
		{
			dx = e->x - p->x;
			dy = e->y - p->y;
			if (abs(dx) > MAX_SLIDE || abs(dy) > MAX_SLIDE)
				continue;
		}
		// in 1/65536 pixels, as the pictures in between draw it
		const Sint32 fx = dx * 65536 + leadXOf(cur, e) - leadXOf(prev, p), fy = dy * 65536 + leadOf(cur, e) - leadOf(prev, p);

		const Uint64 key = (Uint64)e->tag << 48 | (Uint64)e->id << 32 | e->part;
		Uint32 h = (Uint32)((key * 0x9E3779B97F4A7C15ull) >> 51) & (TRACKS - 1);
		while (tracks[h].key != 0 && tracks[h].key != key)
			h = (h + 1) & (TRACKS - 1);
		Track *t = &tracks[h];
		if (t->key == 0)
			t->key = key;
		else if (t->lastFrame + 1 != testFrames)
			judderReport(t);  // a new thing in the slot

		if (t->frames > 0)
		{
			// speeding up and slowing down by turns (1, 2, 1, 2 or 1, 0, 1, 0 pixels)
			const Sint32 ax = fx - t->lastDx, ay = fy - t->lastDy;
			if (t->frames > 1 && (((Sint64)ax * t->lastAx < 0 && abs(ax) >= 32768 && abs(t->lastAx) >= 32768) ||
			                      ((Sint64)ay * t->lastAy < 0 && abs(ay) >= 32768 && abs(t->lastAy) >= 32768)))
				++t->stopGo;
			t->lastAx = ax;
			t->lastAy = ay;
		}
		t->moving += fx != 0 || fy != 0;
		++t->frames;
		t->lastDx = fx;
		t->lastDy = fy;
		t->lastFrame = testFrames;
		t->enemyType = e->tag == SMOOTH_ENEMY && e->id < COUNTOF(enemy) ? enemy[e->id].enemytype : 0;
	}
}

static void judderLevelEnd(void)
{
	if (!testEnabled)
		return;
	for (unsigned int i = 0; i < TRACKS; ++i)
	{
		if (tracks[i].key != 0)
			judderReport(&tracks[i]);
		tracks[i].key = 0;
	}
	judderLogged = 0;
}

void smooth_test_judder(unsigned long counts[16])
{
	memcpy(counts, judderTotal, sizeof judderTotal);
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
			drawFine(testAlpha, view, 3);  // and the finer ones

			judderFrame();

			// all the way to this frame, every phase of the finer picture is the frame
			if (testFrames++ % 35 == 0 && drawFine(65536, view, 2))
			{
				redraw(65536);
				composeScreen(view);
				for (int y = 0; y < compose->h * 2; ++y)
				{
					const Uint8 *row = (const Uint8 *)compose->pixels + (size_t)(y / 2) * compose->pitch;
					for (int x = 0; x < compose->w * 2; ++x)
					{
						if (fine[(size_t)y * compose->w * 2 + x] != row[x / 2])
						{
							++testFineDifferent;
							y = compose->h * 2;
							break;
						}
					}
				}
			}
		}
		return false;
	}

	if (!same || !prevUsable)
		return false;

	const Sint32 refresh = refreshPeriod();
	const Sint32 period = levelFramePeriod > 0 ? levelFramePeriod : frame_period() * frameCountMax;
	const bool shown = showUntilDue(period, refresh, showLevelPicture, &view);

	countFrame(refresh);

	return shown;
}

void smooth_frame_shown(void)
{
	levelFramePeriod = frame_period() * frameCountMax;

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
	memset(cur->leadX, 0, sizeof(cur->leadX));
	memset(cur->leadY, 0, sizeof(cur->leadY));
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

Uint32 smooth_refresh_ms(void)
{
	return (Uint32)refreshPeriod() >> 10;
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
	memset(cur->leadX, 0, sizeof(cur->leadX));
	memset(cur->leadY, 0, sizeof(cur->leadY));
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
	testFrames = testFineDifferent = 0;
}

void smooth_test_counts(unsigned long *same, unsigned long *different, unsigned long *skipped)
{
	*same = testSame;
	*different = testDifferent + testFineDifferent;  // a finer picture that is wrong counts too
	*skipped = testSkipped;
}
