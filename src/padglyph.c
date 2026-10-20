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
 * Controller button icons, drawn pixel by pixel here (no game graphics, no
 * brand logos: letters and the four shapes in round buttons, tags for the
 * shoulder buttons) and added to the font sprite tables, so that every text
 * routine draws them like letters, with the text's color and shadow.
 *
 * Pixels hold a shade (1-15, 0 is transparent) like the font sprites do; the
 * text color picks the hue and brightens or darkens the shades.
 */
#include "padglyph.h"

#include "fonthand.h"
#include "opentyr.h"
#include "sprite.h"

#include <stdlib.h>
#include <string.h>

enum { CANVAS_W = 20, CANVAS_H = 16 };

typedef struct
{
	int w, h;
	Uint8 px[CANVAS_H][CANVAS_W];  // shade, 0 = transparent
}
Canvas;

// one icon size: the small font, or the normal font (also used, lower, in the large one)
typedef struct
{
	int h;                          // icon height
	int letterW, letterH;           // 3x5 or 5x7 letters
	Uint8 base, light, dark;        // shades
}
Size;

// "#" is a pixel; bitmaps are rows of equal length

static const char *const letters3x5[][7] =
{
	{ "A", ".#.", "#.#", "###", "#.#", "#.#" },
	{ "B", "##.", "#.#", "##.", "#.#", "##." },
	{ "X", "#.#", "#.#", ".#.", "#.#", "#.#" },
	{ "Y", "#.#", "#.#", ".#.", ".#.", ".#." },
	{ "L", "#..", "#..", "#..", "#..", "###" },
	{ "R", "##.", "#.#", "##.", "#.#", "#.#" },
	{ "S", ".##", "#..", ".#.", "..#", "##." },
	{ "T", "###", ".#.", ".#.", ".#.", ".#." },
	{ "1", ".#.", "##.", ".#.", ".#.", "###" },
	{ "2", "##.", "..#", ".#.", "#..", "###" },
	{ "3", "##.", "..#", ".#.", "..#", "##." },
	{ "Z", "###", "..#", ".#.", "#..", "###" },
	{ NULL },
};

static const char *const letters5x7[][8] =
{
	{ "A", ".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#" },
	{ "B", "####.", "#...#", "#...#", "####.", "#...#", "#...#", "####." },
	{ "X", "#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#" },
	{ "Y", "#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.." },
	{ "L", "#....", "#....", "#....", "#....", "#....", "#....", "#####" },
	{ "R", "####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#" },
	{ "S", ".####", "#....", "#....", ".###.", "....#", "....#", "####." },
	{ "T", "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.." },
	{ "1", "..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###." },
	{ "2", ".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####" },
	{ "3", "####.", "....#", "....#", ".###.", "....#", "....#", "####." },
	{ "Z", "#####", "....#", "...#.", "..#..", ".#...", "#....", "#####" },
	{ NULL },
};

// symbols inside the round buttons
static const char *const symbols[][5] =
{
	{ "#...#", ".#.#.", "..#..", ".#.#.", "#...#" },  // cross
	{ ".###.", "#...#", "#...#", "#...#", ".###." },  // circle
	{ "#####", "#...#", "#...#", "#...#", "#####" },  // square
	{ "..#..", ".#.#.", ".#.#.", "#...#", "#####" },  // triangle
	{ "..#..", ".###.", "#####", ".#.#.", ".#.#." },  // home
};
enum { SYM_CROSS, SYM_CIRCLE, SYM_SQUARE, SYM_TRIANGLE, SYM_HOME };

// symbols on the small center buttons
static const char *const centerSymbols[][5] =
{
	{ "###..", "#.#..", "#####", "..#.#", "..###" },  // view: two windows
	{ "#####", ".....", "#####", ".....", "#####" },  // menu, options: three lines
	{ ".....", "#.#.#", "#.#.#", "#.#.#", "....." },  // create
	{ ".....", ".....", "#####", ".....", "....." },  // minus
	{ "..#..", "..#..", "#####", "..#..", "..#.." },  // plus
	{ "#####", "#...#", "#.#.#", "#...#", "#####" },  // capture
};
enum { CENTER_VIEW, CENTER_LINES, CENTER_CREATE, CENTER_MINUS, CENTER_PLUS, CENTER_CAPTURE };

static const Size sizeSmall =  { 9,  3, 5, 7, 9, 4 };
static const Size sizeNormal = { 11, 5, 7, 13, 15, 9 };

// --- shapes: a mask of the shape, then shaded like a raised button ---

typedef bool Mask[CANVAS_H][CANVAS_W];

static void maskDisc(Mask m, int x0, int d)
{
	const int c = (d - 1) / 2;
	for (int y = 0; y < d; ++y)
		for (int x = 0; x < d; ++x)
			m[y][x0 + x] = 4 * ((x - c) * (x - c) + (y - c) * (y - c)) <= d * d;
}

// a rectangle with its corners cut by triangles: cutTop, cutBottom pixels
static void maskTag(Mask m, int w, int h, int cutTop, int cutBottom)
{
	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < w; ++x)
		{
			const int fromSide = MIN(x, w - 1 - x);
			m[y][x] = !(fromSide + y < cutTop || fromSide + (h - 1 - y) < cutBottom);
		}
	}
}

static bool masked(Mask m, int x, int y)
{
	return x >= 0 && y >= 0 && x < CANVAS_W && y < CANVAS_H && m[y][x];
}

static void fillShaded(Canvas *c, Mask m, const Size *s)
{
	for (int y = 0; y < c->h; ++y)
	{
		for (int x = 0; x < c->w; ++x)
		{
			if (!m[y][x])
				continue;

			Uint8 shade = s->base;
			if (!masked(m, x + 1, y) || !masked(m, x, y + 1))
				shade = s->dark;
			else if (!masked(m, x - 1, y) || !masked(m, x, y - 1))
				shade = s->light;
			c->px[y][x] = shade;
		}
	}
}

// only the edge of the shape
static void fillOutline(Canvas *c, Mask m, Uint8 shade)
{
	for (int y = 0; y < c->h; ++y)
		for (int x = 0; x < c->w; ++x)
			if (m[y][x] && (!masked(m, x - 1, y) || !masked(m, x + 1, y) || !masked(m, x, y - 1) || !masked(m, x, y + 1)))
				c->px[y][x] = shade;
}

// shade 0 cuts the bitmap out of the shape
static void stamp(Canvas *c, const char *const *rows, int nRows, int x0, int y0, Uint8 shade)
{
	for (int y = 0; y < nRows; ++y)
		for (int x = 0; rows[y][x] != '\0'; ++x)
			if (rows[y][x] == '#' && x0 + x < CANVAS_W && y0 + y < CANVAS_H)
				c->px[y0 + y][x0 + x] = shade;
}

static const char *const *letter(const Size *s, char ch)
{
	if (s->letterH == 5)
	{
		for (size_t i = 0; letters3x5[i][0] != NULL; ++i)
			if (letters3x5[i][0][0] == ch)
				return &letters3x5[i][1];
	}
	else
	{
		for (size_t i = 0; letters5x7[i][0] != NULL; ++i)
			if (letters5x7[i][0][0] == ch)
				return &letters5x7[i][1];
	}
	assert(false);
	return NULL;
}

// --- the icons ---

// round face button with a letter or a symbol cut out
static void drawFace(Canvas *c, const Size *s, char ch, int sym)
{
	Mask m = { { false } };
	c->w = s->h;
	maskDisc(m, 0, s->h);
	fillShaded(c, m, s);

	if (ch != '\0')
		stamp(c, letter(s, ch), s->letterH, (s->h - s->letterW) / 2, (s->h - s->letterH) / 2, 0);
	else
		stamp(c, symbols[sym], 5, (s->h - 5) / 2, (s->h - 5) / 2, 0);
}

// shoulder buttons, triggers and stick clicks: a tag with one or two letters cut out
static void drawTag(Canvas *c, const Size *s, const char *text, int cutTop, int cutBottom)
{
	Mask m = { { false } };
	const int margin = 2, letters = text[1] != '\0' ? 2 : 1;
	c->w = letters * (s->letterW + 1) - 1 + 2 * margin;
	maskTag(m, c->w, s->h, cutTop, cutBottom);
	fillShaded(c, m, s);

	const int y = (s->h - s->letterH) / 2;
	for (int i = 0; i < letters; ++i)
		stamp(c, letter(s, text[i]), s->letterH, margin + i * (s->letterW + 1), y, 0);
}

// small center button (view, menu...): a pill with a symbol cut out
static void drawCenter(Canvas *c, const Size *s, int which)
{
	Mask m = { { false } };
	c->w = s->h + 2;
	maskTag(m, c->w, s->h, s->h / 3, s->h / 3);
	fillShaded(c, m, s);
	stamp(c, centerSymbols[which], 5, (c->w - 5) / 2, (s->h - 5) / 2, 0);
}

static void drawDpad(Canvas *c, const Size *s, int direction)
{
	const int h = s->h, arm = h / 3 | 1, a1 = (h - arm) / 2, a2 = a1 + arm - 1;
	c->w = h;

	for (int y = 0; y < h; ++y)
	{
		for (int x = 0; x < h; ++x)
		{
			const bool vertical = x >= a1 && x <= a2, horizontal = y >= a1 && y <= a2;
			if (!vertical && !horizontal)
				continue;

			bool lit;
			switch (direction)
			{
			case 0:  lit = vertical && y < a1; break;
			case 1:  lit = horizontal && x > a2; break;
			case 2:  lit = vertical && y > a2; break;
			default: lit = horizontal && x < a1; break;
			}
			c->px[y][x] = lit ? s->light : s->dark;
		}
	}
}

static void drawArrow(Canvas *c, const Size *s, int direction)
{
	// pointing right: a triangle n wide, 2n - 1 tall
	const int n = s->letterW == 3 ? 3 : 4, len = 2 * n - 1;
	const bool sideways = direction == 1 || direction == 3;
	c->w = sideways ? n : len;

	const int y0 = (s->h - (sideways ? len : n)) / 2;
	for (int i = 0; i < len; ++i)
	{
		for (int j = 0; j < n; ++j)
		{
			if (j > MIN(i, len - 1 - i))
				continue;

			int x, y;
			switch (direction)
			{
			case 0:  x = i; y = n - 1 - j; break;
			case 1:  x = j; y = i; break;
			case 2:  x = i; y = j; break;
			default: x = n - 1 - j; y = i; break;
			}
			c->px[y0 + y][x] = s->base;
		}
	}
}

// a stick seen from above: a ring with its letter
static void drawStick(Canvas *c, const Size *s, char ch)
{
	Mask m = { { false } };
	c->w = s->h;
	maskDisc(m, 0, s->h);
	fillOutline(c, m, s->base);
	stamp(c, letter(s, ch), s->letterH, (s->h - s->letterW) / 2, (s->h - s->letterH) / 2, s->light);
}

static void drawGlyph(Canvas *c, const Size *s, PadGlyph g)
{
	memset(c, 0, sizeof(*c));
	c->h = s->h;

	const int cut = s->h / 4;  // triggers are rounder on top, stick clicks on both ends

	switch (g)
	{
	case PADGLYPH_DPAD_UP:
	case PADGLYPH_DPAD_RIGHT:
	case PADGLYPH_DPAD_DOWN:
	case PADGLYPH_DPAD_LEFT:     drawDpad(c, s, g - PADGLYPH_DPAD_UP); break;
	case PADGLYPH_ARROW_UP:
	case PADGLYPH_ARROW_RIGHT:
	case PADGLYPH_ARROW_DOWN:
	case PADGLYPH_ARROW_LEFT:    drawArrow(c, s, g - PADGLYPH_ARROW_UP); break;
	case PADGLYPH_STICK_LEFT:    drawStick(c, s, 'L'); break;
	case PADGLYPH_STICK_RIGHT:   drawStick(c, s, 'R'); break;

	case PADGLYPH_XBOX_A:        drawFace(c, s, 'A', 0); break;
	case PADGLYPH_XBOX_B:        drawFace(c, s, 'B', 0); break;
	case PADGLYPH_XBOX_X:        drawFace(c, s, 'X', 0); break;
	case PADGLYPH_XBOX_Y:        drawFace(c, s, 'Y', 0); break;
	case PADGLYPH_XBOX_VIEW:     drawCenter(c, s, CENTER_VIEW); break;
	case PADGLYPH_XBOX_GUIDE:    drawFace(c, s, '\0', SYM_HOME); break;
	case PADGLYPH_XBOX_MENU:     drawCenter(c, s, CENTER_LINES); break;
	case PADGLYPH_XBOX_LS:       drawTag(c, s, "LS", cut, cut); break;
	case PADGLYPH_XBOX_RS:       drawTag(c, s, "RS", cut, cut); break;
	case PADGLYPH_XBOX_LB:       drawTag(c, s, "LB", 1, 1); break;
	case PADGLYPH_XBOX_RB:       drawTag(c, s, "RB", 1, 1); break;
	case PADGLYPH_XBOX_LT:       drawTag(c, s, "LT", cut, 1); break;
	case PADGLYPH_XBOX_RT:       drawTag(c, s, "RT", cut, 1); break;

	case PADGLYPH_PS_CROSS:      drawFace(c, s, '\0', SYM_CROSS); break;
	case PADGLYPH_PS_CIRCLE:     drawFace(c, s, '\0', SYM_CIRCLE); break;
	case PADGLYPH_PS_SQUARE:     drawFace(c, s, '\0', SYM_SQUARE); break;
	case PADGLYPH_PS_TRIANGLE:   drawFace(c, s, '\0', SYM_TRIANGLE); break;
	case PADGLYPH_PS_CREATE:     drawCenter(c, s, CENTER_CREATE); break;
	case PADGLYPH_PS_HOME:       drawFace(c, s, '\0', SYM_HOME); break;
	case PADGLYPH_PS_OPTIONS:    drawCenter(c, s, CENTER_LINES); break;
	case PADGLYPH_PS_L3:         drawTag(c, s, "L3", cut, cut); break;
	case PADGLYPH_PS_R3:         drawTag(c, s, "R3", cut, cut); break;
	case PADGLYPH_PS_L1:         drawTag(c, s, "L1", 1, 1); break;
	case PADGLYPH_PS_R1:         drawTag(c, s, "R1", 1, 1); break;
	case PADGLYPH_PS_L2:         drawTag(c, s, "L2", cut, 1); break;
	case PADGLYPH_PS_R2:         drawTag(c, s, "R2", cut, 1); break;

	case PADGLYPH_NINTENDO_MINUS:   drawCenter(c, s, CENTER_MINUS); break;
	case PADGLYPH_NINTENDO_PLUS:    drawCenter(c, s, CENTER_PLUS); break;
	case PADGLYPH_NINTENDO_CAPTURE: drawCenter(c, s, CENTER_CAPTURE); break;
	case PADGLYPH_NINTENDO_L:       drawTag(c, s, "L", 1, 1); break;
	case PADGLYPH_NINTENDO_R:       drawTag(c, s, "R", 1, 1); break;
	case PADGLYPH_NINTENDO_ZL:      drawTag(c, s, "ZL", cut, 1); break;
	case PADGLYPH_NINTENDO_ZR:      drawTag(c, s, "ZR", cut, 1); break;

	case PADGLYPH_END:
		break;
	}
}

// encodes like the font sprites: 255 n skips n pixels, 254 ends a row,
// 253 skips one pixel, anything else is a pixel's shade
static void canvasToSprite(const Canvas *c, int yOffset, Sprite *out)
{
	Uint8 data[CANVAS_W * CANVAS_H * 2 + CANVAS_H];
	size_t size = 0;

	int lastRow = -1;
	for (int y = 0; y < c->h; ++y)
		for (int x = 0; x < c->w; ++x)
			if (c->px[y][x] != 0)
				lastRow = y;

	for (int y = -yOffset; y <= lastRow; ++y)
	{
		int lastX = -1;
		if (y >= 0)
			for (int x = 0; x < c->w; ++x)
				if (c->px[y][x] != 0)
					lastX = x;

		for (int x = 0; x <= lastX; )
		{
			int skip = 0;
			while (x + skip <= lastX && c->px[y][x + skip] == 0)
				++skip;

			if (skip == 1)
				data[size++] = 253;
			else if (skip > 1)
			{
				data[size++] = 255;
				data[size++] = (Uint8)skip;
			}
			else
				data[size++] = c->px[y][x];

			x += MAX(skip, 1);
		}

		// a full row moves on by itself
		if (lastX < c->w - 1)
			data[size++] = 254;
	}

	out->width = (Uint16)c->w;
	out->height = (Uint16)(c->h + yOffset);
	out->size = (Uint16)size;
	out->data = malloc(MAX(size, 1));
	memcpy(out->data, data, size);
}

void padglyphs_install(void)
{
	static const struct { unsigned int table; const Size *size; int yOffset; } fonts[] =
	{
		{ FONT_SHAPES,       &sizeNormal, 2 },
		{ SMALL_FONT_SHAPES, &sizeNormal, 0 },
		{ TINY_FONT,         &sizeSmall,  0 },
	};

	for (size_t f = 0; f < COUNTOF(fonts); ++f)
	{
		Sprite_array *table = &sprite_table[fonts[f].table];

		for (int g = PADGLYPH_FIRST; g < PADGLYPH_END; ++g)
		{
			const unsigned int index = PADGLYPH_SPRITE_FIRST + (g - PADGLYPH_FIRST);
			assert(index < COUNTOF(table->sprite));

			Sprite *sprite_ = &table->sprite[index];
			free(sprite_->data);

			Canvas canvas;
			drawGlyph(&canvas, fonts[f].size, (PadGlyph)g);
			canvasToSprite(&canvas, fonts[f].yOffset, sprite_);

			table->count = MAX(table->count, index + 1);
			fontMap[g] = (Sint16)index;
		}
	}
}
