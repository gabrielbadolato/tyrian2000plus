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
#include "osk.h"

#include "font.h"
#include "fonthand.h"
#include "joystick.h"
#include "lang.h"
#include "nortsong.h"
#include "sndmast.h"
#include "sprite.h"
#include "vga256d.h"

#include <ctype.h>
#include <string.h>

enum
{
	COLS = 13,
	ROWS = 4,  // three of characters and one of special keys
	W_KEY = 15,
	H_KEY = 10,
	X_KEYS = (320 - COLS * W_KEY) / 2,
	Y_KEYS = OSK_Y + 2,
};

static const char keys[ROWS - 1][COLS + 1] =
{
	"ABCDEFGHIJKLM",
	"NOPQRSTUVWXYZ",
	"0123456789.-!",
};

// the special keys of the last row and the columns they take
typedef enum { SPECIAL_SHIFT, SPECIAL_SPACE, SPECIAL_DELETE, SPECIAL_OK, SPECIALS } Special;
static const int specialCol[SPECIALS + 1] = { 0, 3, 8, 11, COLS };

static Special specialAt(int col)
{
	Special s = SPECIAL_SHIFT;
	while (s + 1 < SPECIALS && col >= specialCol[s + 1])
		++s;
	return s;
}

static const char *specialName(const Osk *osk, Special s)
{
	switch (s)
	{
	case SPECIAL_SHIFT:  return osk->lowercase ? "ABC" : "abc";
	case SPECIAL_SPACE:  return tr("Space");
	case SPECIAL_DELETE: return tr("Delete");
	default:             return "OK";
	}
}

static Uint8 keyChar(const Osk *osk, int row, int col)
{
	const Uint8 ch = (Uint8)keys[row][col];
	return osk->lowercase ? (Uint8)tolower(ch) : ch;
}

static bool shown(void)
{
	return joysticks > 0;
}

void osk_init(Osk *osk, SDL_Surface *surface)
{
	osk->row = 0;
	osk->col = 0;
	osk->lowercase = false;

	for (int y = OSK_Y; y < 200; ++y)
		memcpy(&osk->background[320 * (y - OSK_Y)], (Uint8 *)surface->pixels + surface->pitch * y, 320);
	osk->saved = true;
}

void osk_close(Osk *osk, SDL_Surface *surface)
{
	if (!osk->saved)
		return;

	for (int y = OSK_Y; y < 200; ++y)
		memcpy((Uint8 *)surface->pixels + surface->pitch * y, &osk->background[320 * (y - OSK_Y)], 320);
}

// one key: light text, or dark text on the cursor's color when selected
static void drawKey(SDL_Surface *surface, int x, int y, int w, const char *label, bool selected)
{
	if (selected)
		fill_rectangle_xy(surface, x + 1, y, x + w - 1, y + H_KEY - 2, 8 * 16 + 10);

	const int xText = x + (w - JE_textWidth(label, TINY_FONT)) / 2 + 1;
	JE_outText(surface, xText, y + 1, label, 15, selected ? -1 : 3);
}

void osk_draw(Osk *osk, SDL_Surface *surface)
{
	osk_close(osk, surface);  // the background, so the shades do not pile up

	if (!shown())
		return;

	fill_rectangle_xy(surface, X_KEYS - 4, OSK_Y, X_KEYS + COLS * W_KEY + 3, 199, 0);

	char label[2] = { 0, 0 };
	for (int row = 0; row < ROWS - 1; ++row)
	{
		for (int col = 0; col < COLS; ++col)
		{
			label[0] = (char)keyChar(osk, row, col);
			drawKey(surface, X_KEYS + col * W_KEY, Y_KEYS + row * H_KEY, W_KEY, label, row == osk->row && col == osk->col);
		}
	}

	const Special selected = osk->row == ROWS - 1 ? specialAt(osk->col) : SPECIALS;
	for (Special s = 0; s < SPECIALS; ++s)
	{
		const int w = (specialCol[s + 1] - specialCol[s]) * W_KEY;
		drawKey(surface, X_KEYS + specialCol[s] * W_KEY, Y_KEYS + (ROWS - 1) * H_KEY, w, specialName(osk, s), s == selected);
	}
}

static OskResult press(Osk *osk, int row, int col, Uint8 *ch)
{
	if (row < ROWS - 1)
	{
		*ch = keyChar(osk, row, col);
		return OSK_CHAR;
	}

	switch (specialAt(col))
	{
	case SPECIAL_SHIFT:
		osk->lowercase = !osk->lowercase;
		JE_playSampleNum(S_CLICK);
		return OSK_HANDLED;
	case SPECIAL_SPACE:
		*ch = ' ';
		return OSK_CHAR;
	case SPECIAL_DELETE:
		return OSK_DELETE;
	default:
		return OSK_ACCEPT;
	}
}

static bool padButtonHeld(SDL_GameControllerButton button)
{
	for (int j = 0; j < joysticks; ++j)
		if (joystick[j].controller != NULL && SDL_GameControllerGetButton(joystick[j].controller, button))
			return true;
	return false;
}

OskResult osk_key(Osk *osk, const KeyboardInput *input, Uint8 *ch)
{
	// only the presses the controller makes (they have no key symbol)
	if (!shown() || input->sym != 0)
		return OSK_NONE;

	switch (input->scancode)
	{
	case SDL_SCANCODE_UP:
		osk->row = (osk->row + ROWS - 1) % ROWS;
		break;
	case SDL_SCANCODE_DOWN:
		osk->row = (osk->row + 1) % ROWS;
		break;
	case SDL_SCANCODE_LEFT:
		if (osk->row == ROWS - 1)
			osk->col = specialCol[(specialAt(osk->col) + SPECIALS - 1) % SPECIALS];
		else
			osk->col = (osk->col + COLS - 1) % COLS;
		break;
	case SDL_SCANCODE_RIGHT:
		if (osk->row == ROWS - 1)
			osk->col = specialCol[(specialAt(osk->col) + 1) % SPECIALS];
		else
			osk->col = (osk->col + 1) % COLS;
		break;
	case SDL_SCANCODE_RETURN:
		// Start finishes, A presses the key
		if (padButtonHeld(SDL_CONTROLLER_BUTTON_START))
			return OSK_ACCEPT;
		return press(osk, osk->row, osk->col, ch);
	case SDL_SCANCODE_ESCAPE:
		// Back leaves, B erases
		if (padButtonHeld(SDL_CONTROLLER_BUTTON_BACK))
			return OSK_CANCEL;
		return OSK_DELETE;
	default:
		return OSK_NONE;
	}

	JE_playSampleNum(S_CURSOR);
	return OSK_HANDLED;
}

OskResult osk_mouse(Osk *osk, const MouseInput *input, Uint8 *ch)
{
	if (!shown() || input->button != SDL_BUTTON_LEFT)
		return OSK_NONE;

	if (input->x < X_KEYS || input->x >= X_KEYS + COLS * W_KEY ||
	    input->y < Y_KEYS || input->y >= Y_KEYS + ROWS * H_KEY)
		return OSK_NONE;

	osk->row = (input->y - Y_KEYS) / H_KEY;
	osk->col = (input->x - X_KEYS) / W_KEY;
	return press(osk, osk->row, osk->col, ch);
}
