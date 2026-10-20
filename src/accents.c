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
 * Accented letters, built when the fonts are loaded from the game's own plain
 * letters plus an accent mark drawn here, so tyrian.shp stays unchanged.  The
 * small font already has CP437's accented letters; they are kept, except its ç, which looks like an S.
 */
#include "accents.h"

#include "fonthand.h"
#include "opentyr.h"
#include "sprite.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

enum { CANVAS_W = 16, CANVAS_H = 24 };

typedef struct
{
	int w, h;
	Uint8 px[CANVAS_H][CANVAS_W];  // shade, 0 = transparent
}
Canvas;

typedef enum
{
	MARK_NONE,
	MARK_ACUTE,
	MARK_GRAVE,
	MARK_CIRCUMFLEX,
	MARK_TILDE,
	MARK_DIAERESIS,
	MARK_RING,
	MARK_CEDILLA,
	MARK_COUNT
}
Mark;

// "#" is a pixel; one set of marks per font: large, normal, small
static const char *const marks[3][MARK_COUNT][3] =
{
	{
		[MARK_ACUTE]      = { "...###", ".###.." },
		[MARK_GRAVE]      = { "###...", "..###." },
		[MARK_CIRCUMFLEX] = { "...###...", "###...###" },
		[MARK_TILDE]      = { ".####..###", "###..####." },
		[MARK_DIAERESIS]  = { "###...###", "###...###" },
		[MARK_RING]       = { ".###.", "##.##" },
		[MARK_CEDILLA]    = { "..###", "...##", ".###." },
	},
	{
		[MARK_ACUTE]      = { "..##", ".##." },
		[MARK_GRAVE]      = { "##..", ".##." },
		[MARK_CIRCUMFLEX] = { "..##..", "##..##" },
		[MARK_TILDE]      = { ".##..#", "#..##." },
		[MARK_DIAERESIS]  = { "##..##" },
		[MARK_RING]       = { ".##.", "#..#" },
		[MARK_CEDILLA]    = { ".##", "##." },
	},
	{
		[MARK_ACUTE]      = { "..#.", ".#.." },
		[MARK_GRAVE]      = { ".#..", "..#." },
		[MARK_CIRCUMFLEX] = { "..#.", ".#.#" },
		[MARK_TILDE]      = { ".#.#", "#.#." },
		[MARK_DIAERESIS]  = { "#..#" },
		[MARK_RING]       = { ".#.", "#.#" },
		[MARK_CEDILLA]    = { "..#", ".##" },
	},
};

// accented capitals for the small font: the letter squeezed to four rows,
// under the mark (like the font's own É)
static const char *const smallCapitals[][5] =
{
	{ "A", "####", "#..#", "####", "#..#" },
	{ "E", "####", "###.", "#...", "####" },
	{ "I", "###", ".#.", ".#.", "###" },
	{ "O", "####", "#..#", "#..#", "####" },
	{ "U", "#..#", "#..#", "#..#", "####" },
};

static const struct
{
	unsigned char ch;
	char base;
	Mark mark;
}
letters[] =
{
	// CP437
	{ 128, 'C', MARK_CEDILLA },
	{ 129, 'u', MARK_DIAERESIS },
	{ 130, 'e', MARK_ACUTE },
	{ 131, 'a', MARK_CIRCUMFLEX },
	{ 132, 'a', MARK_DIAERESIS },
	{ 133, 'a', MARK_GRAVE },
	{ 134, 'a', MARK_RING },
	{ 135, 'c', MARK_CEDILLA },
	{ 136, 'e', MARK_CIRCUMFLEX },
	{ 137, 'e', MARK_DIAERESIS },
	{ 138, 'e', MARK_GRAVE },
	{ 139, 'i', MARK_DIAERESIS },
	{ 140, 'i', MARK_CIRCUMFLEX },
	{ 141, 'i', MARK_GRAVE },
	{ 142, 'A', MARK_DIAERESIS },
	{ 143, 'A', MARK_RING },
	{ 144, 'E', MARK_ACUTE },
	{ 145, 'a', MARK_NONE },       // æ
	{ 146, 'A', MARK_NONE },       // Æ
	{ 147, 'o', MARK_CIRCUMFLEX },
	{ 148, 'o', MARK_DIAERESIS },
	{ 149, 'o', MARK_GRAVE },
	{ 150, 'u', MARK_CIRCUMFLEX },
	{ 151, 'u', MARK_GRAVE },
	{ 152, 'y', MARK_DIAERESIS },
	{ 153, 'O', MARK_DIAERESIS },
	{ 154, 'U', MARK_DIAERESIS },
	{ 155, 'c', MARK_NONE },       // ¢
	{ 156, 'L', MARK_NONE },       // £
	{ 157, 'Y', MARK_NONE },       // ¥
	{ 158, 'P', MARK_NONE },       // ₧
	{ 159, 'f', MARK_NONE },       // ƒ
	{ 160, 'a', MARK_ACUTE },
	{ 161, 'i', MARK_ACUTE },
	{ 162, 'o', MARK_ACUTE },
	{ 163, 'u', MARK_ACUTE },
	{ 164, 'n', MARK_TILDE },
	{ 165, 'N', MARK_TILDE },
	{ 166, 'a', MARK_NONE },       // ª
	{ 167, 'o', MARK_NONE },       // º
	{ 168, '?', MARK_NONE },       // ¿

	// added letters
	{ ACCENT_A_TILDE_SMALL, 'a', MARK_TILDE },
	{ ACCENT_O_TILDE_SMALL, 'o', MARK_TILDE },
	{ ACCENT_A_ACUTE,       'A', MARK_ACUTE },
	{ ACCENT_A_GRAVE,       'A', MARK_GRAVE },
	{ ACCENT_A_CIRCUMFLEX,  'A', MARK_CIRCUMFLEX },
	{ ACCENT_A_TILDE,       'A', MARK_TILDE },
	{ ACCENT_E_CIRCUMFLEX,  'E', MARK_CIRCUMFLEX },
	{ ACCENT_I_ACUTE,       'I', MARK_ACUTE },
	{ ACCENT_O_ACUTE,       'O', MARK_ACUTE },
	{ ACCENT_O_CIRCUMFLEX,  'O', MARK_CIRCUMFLEX },
	{ ACCENT_O_TILDE,       'O', MARK_TILDE },
	{ ACCENT_U_ACUTE,       'U', MARK_ACUTE },
};

// decodes a font sprite (see blit_sprite())
static bool spriteToCanvas(unsigned int table, int index, Canvas *c)
{
	memset(c, 0, sizeof(*c));

	if (index < 0 || !sprite_exists(table, (unsigned int)index))
		return false;

	const Sprite *s = sprite(table, (unsigned int)index);
	if (s->width > CANVAS_W || s->height > CANVAS_H)
		return false;

	c->w = s->width;
	c->h = s->height;

	int x = 0, y = 0;
	for (const Uint8 *data = s->data; data < s->data + s->size && y < c->h; ++data)
	{
		switch (*data)
		{
		case 255:
			++data;
			x += *data;
			break;
		case 254:
			x = c->w;
			break;
		case 253:
			++x;
			break;
		default:
			if (x < c->w)
				c->px[y][x] = *data;
			++x;
			break;
		}
		if (x >= c->w)
		{
			x = 0;
			++y;
		}
	}
	return true;
}

// encodes like the font sprites: 255 n skips n pixels, 254 ends a row,
// 253 skips one pixel, anything else is a pixel's shade
static void canvasToSprite(const Canvas *c, Sprite *out)
{
	Uint8 data[CANVAS_W * CANVAS_H * 2 + CANVAS_H];
	size_t size = 0;

	int lastRow = -1;
	for (int y = 0; y < c->h; ++y)
		for (int x = 0; x < c->w; ++x)
			if (c->px[y][x] != 0)
				lastRow = y;

	for (int y = 0; y <= lastRow; ++y)
	{
		int lastX = -1;
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
	out->height = (Uint16)c->h;
	out->size = (Uint16)size;
	out->data = malloc(MAX(size, 1));
	memcpy(out->data, data, size);
}

static bool rowEmpty(const Canvas *c, int y)
{
	for (int x = 0; x < c->w; ++x)
		if (c->px[y][x] != 0)
			return false;
	return true;
}

// the most common shade in the rows from y on, for the mark to match the letter
static Uint8 letterShade(const Canvas *c, int y)
{
	int counts[256] = { 0 };
	for (; y < c->h; ++y)
		for (int x = 0; x < c->w; ++x)
			if (c->px[y][x] != 0)
				++counts[c->px[y][x]];

	Uint8 best = 0;
	for (int i = 1; i < 256; ++i)
		if (counts[i] > counts[best])
			best = (Uint8)i;
	return best != 0 ? best : 15;
}

// draws rows of "#" with their middle over column xCenter2 / 2, cut to the canvas
static void drawPattern(Canvas *c, const char *const *rows, int y, int xCenter2, Uint8 shade)
{
	for (int r = 0; r < 3 && rows[r] != NULL; ++r, ++y)
	{
		const int w = (int)strlen(rows[r]);
		const int x0 = (xCenter2 - w) / 2;
		for (int i = 0; i < w; ++i)
		{
			const int x = x0 + i;
			if (rows[r][i] == '#' && x >= 0 && x < c->w && y >= 0 && y < CANVAS_H)
				c->px[y][x] = shade;
		}
	}
}

static int patternHeight(const char *const *rows)
{
	int h = 0;
	while (h < 3 && rows[h] != NULL)
		++h;
	return h;
}

// builds the accented letter in c; false if the base letter is missing
static bool buildLetter(unsigned int table, size_t f, char base, Mark mark, Canvas *c)
{
	if (!spriteToCanvas(table, fontMap[(unsigned char)base], c))
		return false;

	if (mark == MARK_NONE)
		return true;

	const char *const *rows = marks[f][mark];
	const int ph = patternHeight(rows);

	if (mark == MARK_CEDILLA)
	{
		if (c->h + ph > CANVAS_H)
			return true;
		drawPattern(c, rows, c->h, c->w, letterShade(c, 0));
		c->h += ph;
		return true;
	}

	// small capitals: squeezed under the mark
	if (f == 2 && base >= 'A' && base <= 'Z')
	{
		for (size_t i = 0; i < COUNTOF(smallCapitals); ++i)
		{
			if (smallCapitals[i][0][0] != base)
				continue;

			const Uint8 shade = letterShade(c, 0);
			memset(c->px, 0, sizeof(c->px));
			c->w = (int)strlen(smallCapitals[i][1]);
			c->h = 6;
			drawPattern(c, rows, 0, c->w, shade);
			drawPattern(c, &smallCapitals[i][1], 2, c->w, shade);
			drawPattern(c, &smallCapitals[i][4], 5, c->w, shade);
			return true;
		}
	}

	// the letter's body starts after the empty rows on top (and, for i and
	// j, after their dot, which the mark replaces)
	int top = 0;
	while (top < c->h && rowEmpty(c, top))
		++top;
	if (base == 'i' || base == 'j')
	{
		int gap = top;
		while (gap < c->h && !rowEmpty(c, gap))
			++gap;
		if (gap < c->h)
		{
			for (int y = 0; y < gap; ++y)
				memset(c->px[y], 0, sizeof(c->px[y]));
			top = gap;
			while (top < c->h && rowEmpty(c, top))
				++top;
		}
	}

	// no room above (larger capitals): the plain letter
	if (top < ph)
		return true;

	const int y = top > ph ? top - ph - 1 : 0;
	drawPattern(c, rows, y, c->w, letterShade(c, top));
	return true;
}

// The small font's own accent marks are brighter than its letters, so they
// overflow into another colour (and vanish) on highlighted text: no pixel may
// be brighter than the plain letter's brightest.
static void dimNative(unsigned int table, Sprite *s, char base)
{
	Canvas c;
	if (!spriteToCanvas(table, fontMap[(unsigned char)base], &c))
		return;

	Uint8 brightest = 0;
	for (int y = 0; y < c.h; ++y)
		for (int x = 0; x < c.w; ++x)
			brightest = MAX(brightest, c.px[y][x]);
	if (brightest == 0)
		return;

	for (Uint8 *data = s->data; data < s->data + s->size; ++data)
	{
		if (*data == 255)
			++data;
		else if (*data != 254 && *data != 253 && *data > brightest)
			*data = brightest;
	}
}

void accents_install(void)
{
	static const unsigned int tables[3] = { FONT_SHAPES, SMALL_FONT_SHAPES, TINY_FONT };

	for (size_t i = 0; i < COUNTOF(letters); ++i)
	{
		const unsigned char ch = letters[i].ch;
		const bool added = ch >= ACCENT_FIRST;
		const unsigned int index = added ? (unsigned int)(ACCENT_SPRITE_FIRST + (ch - ACCENT_FIRST)) : (unsigned int)fontMap[ch];

		for (size_t f = 0; f < COUNTOF(tables); ++f)
		{
			Sprite_array *table = &sprite_table[tables[f]];
			assert(index < COUNTOF(table->sprite));

			// the small font's own accented letters (but its ç looks like an S)
			if (index < table->count && table->sprite[index].data != NULL && ch != 135)
			{
				dimNative(tables[f], &table->sprite[index], letters[i].base);
				continue;
			}

			Canvas canvas;
			if (!buildLetter(tables[f], f, letters[i].base, letters[i].mark, &canvas))
				continue;

			Sprite *sprite_ = &table->sprite[index];
			free(sprite_->data);
			canvasToSprite(&canvas, sprite_);
			table->count = MAX(table->count, index + 1);
		}

		if (added)
			fontMap[ch] = (Sint16)index;
	}
}
