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
 * Controller map: a generic gamepad, drawn pixel by pixel (no game graphics,
 * no brand's controller), with what each button does under the current
 * assignments.  Buttons light up while pressed, so it doubles as a test.
 */
#include "padmap.h"

#include "config.h"
#include "font.h"
#include "fonthand.h"
#include "joystick.h"
#include "keyboard.h"
#include "mouse.h"
#include "nortsong.h"
#include "opentyr.h"
#include "padglyph.h"
#include "palette.h"
#include "picload.h"
#include "sndmast.h"
#include "vga256d.h"
#include "video.h"

#include "SDL.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// colors (hue * 16 + shade) of the setup menu palette
#define C(hue, shade) ((Uint8)((hue) * 16 + (shade)))

enum
{
	COLOR_OUTLINE = C(3, 1),
	COLOR_LINE = C(8, 5),
	COLOR_LINE_SHADOW = C(3, 2),
	COLOR_BUTTON_OFF = C(3, 5),        // nothing assigned
	COLOR_BUTTON_OFF_LIGHT = C(3, 8),
};

typedef enum
{
	SHAPE_FACE,      // round button
	SHAPE_DPAD,
	SHAPE_STICK,
	SHAPE_SHOULDER,
	SHAPE_TRIGGER,
	SHAPE_PILL,      // small center button
} Shape;

typedef enum
{
	SIDE_LEFT,
	SIDE_RIGHT,
	SIDE_TOP_LEFT,   // above the controller, text ending at the line
	SIDE_TOP_RIGHT,  // above the controller, text starting at the line
} Side;

typedef struct
{
	const char *name;                  // NULL: the name printed on the controller
	Joystick_assignment inputs[5];     // what makes up this control (type NONE ends)
	Shape shape;
	int x, y;                          // center of the drawing
	Side side;
	int yLabel;                        // top of the label
	int xBend, yBend;                  // the leader line goes label -> bend -> anchor
	int xAnchor, yAnchor;
} PadControl;

#define BUTTON(b) { PAD_BUTTON, SDL_CONTROLLER_BUTTON_##b, false, false }
#define AXIS(a, negative) { PAD_AXIS, SDL_CONTROLLER_AXIS_##a, false, negative }
#define STICK(s, b) AXIS(s##Y, true), AXIS(s##X, false), AXIS(s##Y, false), AXIS(s##X, true), BUTTON(b)

static const int xLeftLabels = 94;    // labels on the left end here
static const int xRightLabels = 226;  // labels on the right start here

static const PadControl controls[] =
{
	{ NULL, { AXIS(TRIGGERLEFT, false) },    SHAPE_TRIGGER,  125, 59,  SIDE_LEFT,  34, 0, 0, 116, 59 },
	{ NULL, { BUTTON(LEFTSHOULDER) },        SHAPE_SHOULDER, 124, 67,  SIDE_LEFT,  54, 0, 0, 108, 67 },
	{ "D-Pad", { BUTTON(DPAD_UP), BUTTON(DPAD_RIGHT), BUTTON(DPAD_DOWN), BUTTON(DPAD_LEFT) },
	                                         SHAPE_DPAD,     126, 91,  SIDE_LEFT,  84, 0, 0, 116, 91 },
	{ "L Stick", { STICK(LEFT, LEFTSTICK) }, SHAPE_STICK,    146, 113, SIDE_LEFT,  140, 146, 0, 146, 122 },
	{ NULL, { BUTTON(BACK) },                SHAPE_PILL,     150, 84,  SIDE_TOP_LEFT,  34, 0, 0, 150, 82 },
	{ NULL, { BUTTON(START) },               SHAPE_PILL,     170, 84,  SIDE_TOP_RIGHT, 34, 0, 0, 170, 82 },
	{ NULL, { AXIS(TRIGGERRIGHT, false) },   SHAPE_TRIGGER,  195, 59,  SIDE_RIGHT, 34, 0, 0, 204, 59 },
	{ NULL, { BUTTON(RIGHTSHOULDER) },       SHAPE_SHOULDER, 196, 67,  SIDE_RIGHT, 52, 0, 0, 212, 67 },
	{ NULL, { BUTTON(Y) },                   SHAPE_FACE,     194, 82,  SIDE_RIGHT, 70, 0, 0, 198, 79 },
	{ NULL, { BUTTON(B) },                   SHAPE_FACE,     203, 91,  SIDE_RIGHT, 88, 0, 0, 208, 91 },
	{ NULL, { BUTTON(A) },                   SHAPE_FACE,     194, 100, SIDE_RIGHT, 106, 0, 0, 198, 104 },
	{ NULL, { BUTTON(X) },                   SHAPE_FACE,     185, 91,  SIDE_RIGHT, 124, 185, 0, 185, 96 },
	{ "R Stick", { STICK(RIGHT, RIGHTSTICK) }, SHAPE_STICK,  174, 113, SIDE_RIGHT, 142, 174, 0, 174, 122 },
};

static const char *const actionNames[] =
{
	"Up", "Right", "Down", "Left",
	"Fire", "Change Fire", "Left Sidekick", "Right Sidekick", "Menu", "Pause",
};

static bool sameInput(const Joystick_assignment *a, const Joystick_assignment *b)
{
	return a->type == b->type && a->num == b->num &&
	       (a->type != PAD_AXIS || a->negative_axis == b->negative_axis);
}

// bit per action (see actionNames) assigned to any input of the control
static unsigned int controlActions(const PadControl *control, Joystick_assignment assignments[10][2])
{
	unsigned int actions = 0;

	for (size_t i = 0; i < COUNTOF(control->inputs) && control->inputs[i].type != NONE; ++i)
		for (unsigned int a = 0; a < 10; ++a)
			for (unsigned int k = 0; k < 2; ++k)
				if (sameInput(&control->inputs[i], &assignments[a][k]))
					actions |= 1u << a;

	return actions;
}

// "Move", "Fire", "Up/Down", "Auto Fire/Menu" ...
static void actionsToString(unsigned int actions, char *buffer, size_t bufferSize)
{
	buffer[0] = '\0';

	if ((actions & 0xf) == 0xf)
	{
		SDL_strlcat(buffer, "Move", bufferSize);
		actions &= ~0xfu;
	}

	for (unsigned int a = 0; a < COUNTOF(actionNames); ++a)
	{
		if ((actions & (1u << a)) == 0)
			continue;

		if (buffer[0] != '\0')
			SDL_strlcat(buffer, "/", bufferSize);
		SDL_strlcat(buffer, a == 4 && autoFireToggle ? "Auto Fire" : actionNames[a], bufferSize);
	}

	if (buffer[0] == '\0')
		SDL_strlcat(buffer, "-", bufferSize);
}

static bool controlPressed(const PadControl *control, int j)
{
	if (j < 0 || joystick[j].controller == NULL)
		return false;

	SDL_GameController *pad = joystick[j].controller;

	for (size_t i = 0; i < COUNTOF(control->inputs) && control->inputs[i].type != NONE; ++i)
	{
		const Joystick_assignment *input = &control->inputs[i];

		if (input->type == PAD_BUTTON)
		{
			if (SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)input->num))
				return true;
		}
		else
		{
			const int value = SDL_GameControllerGetAxis(pad, (SDL_GameControllerAxis)input->num);
			if (input->negative_axis ? value < -16000 : value > 16000)
				return true;
		}
	}

	return false;
}

// --- drawing ---

static bool inEllipse(int x, int y, int cx, int cy, int rx, int ry)
{
	const int dx = x - cx, dy = y - cy;
	return dx * dx * ry * ry + dy * dy * rx * rx <= rx * rx * ry * ry;
}

static bool inRoundedRect(int x, int y, int x1, int y1, int x2, int y2, int r)
{
	if (x < x1 || x > x2 || y < y1 || y > y2)
		return false;

	const int cx = x < x1 + r ? x1 + r : x > x2 - r ? x2 - r : x;
	const int cy = y < y1 + r ? y1 + r : y > y2 - r ? y2 - r : y;
	return (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r;
}

static bool inBody(int x, int y)
{
	return inRoundedRect(x, y, 102, 70, 218, 118, 14) ||
	       inEllipse(x, y, 116, 122, 17, 25) ||
	       inEllipse(x, y, 204, 122, 17, 25);
}

static bool inShoulder(int x, int y)
{
	return inRoundedRect(x, y, 108, 64, 140, 74, 3) ||
	       inRoundedRect(x, y, 180, 64, 212, 74, 3);
}

static bool inTrigger(int x, int y)
{
	return inRoundedRect(x, y, 116, 55, 134, 66, 4) ||
	       inRoundedRect(x, y, 186, 55, 204, 66, 4);
}

typedef bool (*ShapeTest)(int x, int y);

// fills a shape with a vertical gradient of one hue, with a dark outline,
// a light top-left edge and a darker bottom-right edge
static void drawShaded(SDL_Surface *surface, ShapeTest inside, int x1, int y1, int x2, int y2, int hue, int shadeTop, int shadeBottom)
{
	for (int y = y1; y <= y2; ++y)
	{
		const int shade = shadeTop + (shadeBottom - shadeTop) * (y - y1) / MAX(1, y2 - y1);

		for (int x = x1; x <= x2; ++x)
		{
			if (!inside(x, y))
				continue;

			Uint8 color;
			if (!inside(x - 1, y) || !inside(x + 1, y) || !inside(x, y - 1) || !inside(x, y + 1))
				color = COLOR_OUTLINE;
			else if (!inside(x - 1, y - 1) || !inside(x, y - 2))
				color = C(hue, MIN(15, shade + 4));
			else if (!inside(x + 1, y + 1) || !inside(x, y + 2))
				color = C(hue, MAX(1, shade - 3));
			else
				color = C(hue, shade);

			JE_pix(surface, x, y, color);
		}
	}
}

static void drawDisc(SDL_Surface *surface, int cx, int cy, int r, Uint8 fill, Uint8 light, Uint8 dark)
{
	for (int y = -r; y <= r; ++y)
	{
		for (int x = -r; x <= r; ++x)
		{
			const int d = x * x + y * y;
			if (d > r * r + r / 2)
				continue;

			Uint8 color = fill;
			if (d > (r - 1) * (r - 1) + (r - 1) / 2)
				color = COLOR_OUTLINE;
			else if (x + y < -r / 2 && d > (r - 2) * (r - 2))
				color = light;
			else if (x + y > r / 2 && d > (r - 2) * (r - 2))
				color = dark;

			JE_pix(surface, cx + x, cy + y, color);
		}
	}
}

// colors of a button: lit while pressed, colored if it does something
static void buttonColors(bool assigned, bool pressed, int hue, Uint8 *fill, Uint8 *light, Uint8 *dark)
{
	if (pressed)
	{
		*fill = C(14, 12); *light = C(14, 15); *dark = C(14, 9);
	}
	else if (assigned)
	{
		*fill = C(hue, 9); *light = C(hue, 13); *dark = C(hue, 6);
	}
	else
	{
		*fill = COLOR_BUTTON_OFF; *light = COLOR_BUTTON_OFF_LIGHT; *dark = C(3, 3);
	}
}

static void drawControl(SDL_Surface *surface, const PadControl *control, bool assigned, bool pressed)
{
	Uint8 fill, light, dark;
	buttonColors(assigned, pressed, 13, &fill, &light, &dark);

	const int x = control->x, y = control->y;

	switch (control->shape)
	{
	case SHAPE_FACE:
		drawDisc(surface, x, y, 4, fill, light, dark);
		break;

	case SHAPE_STICK:
		drawDisc(surface, x, y, 8, C(3, 3), C(3, 5), C(3, 2));  // well
		drawDisc(surface, x, y - 1, 6, fill, light, dark);       // cap
		JE_pix(surface, x - 2, y - 3, light);
		break;

	case SHAPE_DPAD:
		buttonColors(assigned, pressed, 5, &fill, &light, &dark);
		fill_rectangle_xy(surface, x - 10, y - 3, x + 10, y + 3, COLOR_OUTLINE);
		fill_rectangle_xy(surface, x - 3, y - 10, x + 3, y + 10, COLOR_OUTLINE);
		fill_rectangle_xy(surface, x - 9, y - 2, x + 9, y + 2, fill);
		fill_rectangle_xy(surface, x - 2, y - 9, x + 2, y + 9, fill);
		fill_rectangle_xy(surface, x - 9, y - 2, x + 9, y - 2, light);
		fill_rectangle_xy(surface, x - 2, y - 9, x + 2, y - 9, light);
		fill_rectangle_xy(surface, x - 9, y + 2, x - 3, y + 2, dark);
		fill_rectangle_xy(surface, x + 3, y + 2, x + 9, y + 2, dark);
		fill_rectangle_xy(surface, x - 2, y + 9, x + 2, y + 9, dark);
		JE_pix(surface, x, y, dark);
		break;

	case SHAPE_PILL:
		buttonColors(assigned, pressed, 5, &fill, &light, &dark);
		fill_rectangle_xy(surface, x - 4, y - 1, x + 4, y + 2, COLOR_OUTLINE);
		fill_rectangle_xy(surface, x - 3, y - 2, x + 3, y + 3, COLOR_OUTLINE);
		fill_rectangle_xy(surface, x - 3, y - 1, x + 3, y + 2, fill);
		fill_rectangle_xy(surface, x - 3, y - 1, x + 3, y - 1, light);
		break;

	case SHAPE_SHOULDER:
	case SHAPE_TRIGGER:
	{
		const bool shoulder = control->shape == SHAPE_SHOULDER;
		const ShapeTest inside = shoulder ? inShoulder : inTrigger;
		const int x1 = x - 16, x2 = x + 16, y1 = shoulder ? 64 : 55, y2 = shoulder ? 74 : 66;
		const int hue = pressed ? 14 : assigned ? 8 : 3;
		const int top = pressed ? 13 : assigned ? 6 : 7;
		const int bottom = pressed ? 10 : assigned ? 2 : 4;

		drawShaded(surface, inside, MAX(x1, x < 160 ? 100 : 160), y1, MIN(x2, x < 160 ? 160 : 220), y2, hue, top, bottom);
		break;
	}
	}
}

static void drawLine(SDL_Surface *surface, int x1, int y1, int x2, int y2, Uint8 color)
{
	const int dx = abs(x2 - x1), dy = -abs(y2 - y1);
	const int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
	int err = dx + dy;

	for (; ; )
	{
		JE_pix(surface, x1, y1, color);
		if (x1 == x2 && y1 == y2)
			break;

		const int e2 = 2 * err;
		if (e2 >= dy) { err += dy; x1 += sx; }
		if (e2 <= dx) { err += dx; y1 += sy; }
	}
}

static void drawLeader(SDL_Surface *surface, int x1, int y1, int xBend, int yBend, int x2, int y2)
{
	for (int pass = 0; pass < 2; ++pass)
	{
		const int o = pass == 0 ? 1 : 0;  // shadow first
		const Uint8 color = pass == 0 ? COLOR_LINE_SHADOW : COLOR_LINE;

		drawLine(surface, x1 + o, y1 + o, xBend + o, yBend + o, color);
		drawLine(surface, xBend + o, yBend + o, x2 + o, y2 + o, color);
	}

	JE_pix(surface, x2, y2, C(15, 15));
}

static void drawLabel(SDL_Surface *surface, const PadControl *control, int j, Joystick_assignment assignments[10][2], bool pressed)
{
	const char *name = control->name != NULL ? control->name : joystick_assignment_icon(j, &control->inputs[0]);

	char actions[64];
	actionsToString(controlActions(control, assignments), actions, sizeof(actions));

	const int y = control->yLabel;
	const int yLine = y + 9;

	FontAlignment align;
	int xText, xLineStart;

	switch (control->side)
	{
	case SIDE_LEFT:
		align = ALIGN_RIGHT;
		xText = xLeftLabels;
		xLineStart = xLeftLabels + 3;
		break;
	case SIDE_RIGHT:
		align = ALIGN_LEFT;
		xText = xRightLabels;
		xLineStart = xRightLabels - 4;
		break;
	case SIDE_TOP_LEFT:
		align = ALIGN_RIGHT;
		xText = control->xAnchor + 2;
		xLineStart = control->xAnchor;
		break;
	default:
		align = ALIGN_LEFT;
		xText = control->xAnchor - 1;
		xLineStart = control->xAnchor;
		break;
	}

	// custom layouts can give a button long action lists; keep them on screen
	const int wMax = align == ALIGN_RIGHT ? xText - 1 : 320 - 1 - xText;
	for (size_t len = strlen(actions); len > 4 && JE_textWidth(actions, FONT_SMALL) > wMax; --len)
		strcpy(&actions[len - 4], "...");

	if (control->side == SIDE_TOP_LEFT || control->side == SIDE_TOP_RIGHT)
		drawLeader(surface, xLineStart, y + 18, xLineStart, y + 18, control->xAnchor, control->yAnchor);
	else
	{
		const int xBend = control->xBend != 0 ? control->xBend : xLineStart;
		drawLeader(surface, xLineStart, yLine, xBend, yLine, control->xAnchor, control->yAnchor);
	}

	// icons are taller than letters: lift them clear of the actions line
	const int yName = is_padglyph((unsigned char)name[0]) ? y - 2 : y;
	drawFontHvShadowAligned(surface, xText, yName, name, FONT_SMALL, align, 14, pressed ? 6 : 2, false, 1);
	drawFontHvShadowAligned(surface, xText, y + 8, actions, FONT_SMALL, align, 15, pressed ? 6 : 3, false, 1);
}

void drawControllerMap(SDL_Surface *surface, int j)
{
	Joystick_assignment defaults[10][2];
	Joystick_assignment (*assignments)[2] = defaults;

	if (j >= 0 && joystick[j].controller != NULL)
		assignments = joystick[j].assignment;
	else
		joystick_default_gamepad_assignments(defaults);

	// triggers and shoulders go behind the body
	for (size_t i = 0; i < COUNTOF(controls); ++i)
	{
		const PadControl *control = &controls[i];
		if (control->shape == SHAPE_TRIGGER)
			drawControl(surface, control, controlActions(control, assignments) != 0, controlPressed(control, j));
	}
	for (size_t i = 0; i < COUNTOF(controls); ++i)
	{
		const PadControl *control = &controls[i];
		if (control->shape == SHAPE_SHOULDER)
			drawControl(surface, control, controlActions(control, assignments) != 0, controlPressed(control, j));
	}

	drawShaded(surface, inBody, 98, 68, 222, 148, 3, 11, 5);

	for (size_t i = 0; i < COUNTOF(controls); ++i)
	{
		const PadControl *control = &controls[i];
		const bool pressed = controlPressed(control, j);

		if (control->shape != SHAPE_TRIGGER && control->shape != SHAPE_SHOULDER)
			drawControl(surface, control, controlActions(control, assignments) != 0, pressed);
	}

	for (size_t i = 0; i < COUNTOF(controls); ++i)
		drawLabel(surface, &controls[i], j, assignments, controlPressed(&controls[i], j));
}

static const Uint32 holdToExitMs = 1000;

static bool anyGamepadButton(SDL_GameControllerButton button)
{
	for (int j = 0; j < joysticks; ++j)
		if (joystick[j].controller != NULL && SDL_GameControllerGetButton(joystick[j].controller, button))
			return true;

	return false;
}

void controllerMapScreen(int j)
{
	JE_loadPic(VGAScreen2, 2, false);
	fill_rectangle_wh(VGAScreen2, 0, 192, 320, 8, 0);

	bool fadeIn = true;
	int oldJoysticks = joysticks;
	Uint32 holdStart = 0;  // when B started being held, 0 if not held

	for (; ; )
	{
		setFrameCount(2);

		if (joysticks != oldJoysticks)  // a controller was connected or disconnected
		{
			oldJoysticks = joysticks;
			j = joystick_first_gamepad();
		}
		if (j >= joysticks)
			j = -1;

		memcpy(VGAScreen->pixels, VGAScreen2->pixels, (size_t)VGAScreen->pitch * VGAScreen->h);

		drawFontHvShadowAligned(VGAScreen, 160, 4, "Controller Map", FONT_LARGE, ALIGN_CENTER, 15, -3, false, 2);

		drawControllerMap(VGAScreen, j);

		char text[128];
		if (j >= 0 && joystick[j].controller != NULL)
		{
			// "1: Steam Controller", shortened with "..." if it does not fit
			snprintf(text, sizeof(text), "%d: %s", j + 1, joystick_display_name(j));
			for (size_t len = strlen(text); len > 4 && JE_textWidth(text, FONT_SMALL) > 300; --len)
				strcpy(&text[len - 4], "...");
		}
		else
		{
			snprintf(text, sizeof(text), "No controller connected. This is the default layout.");
		}
		drawFontHvShadowAligned(VGAScreen, 160, 160, text, FONT_SMALL, ALIGN_CENTER, 15, 2, false, 1);

		snprintf(text, sizeof(text), "In menus: %s = OK, %s = Back.",
		         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_A), joystick_button_icon(j, SDL_CONTROLLER_BUTTON_B));
		drawFontHvShadowAligned(VGAScreen, 160, 170, text, FONT_SMALL, ALIGN_CENTER, 15, 2, false, 1);

		snprintf(text, sizeof(text), "Press buttons to test. Hold %s or press Esc to exit.",
		         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_B));
		JE_textShade(VGAScreen, 45, 190, text, 15, 4, PART_SHADE);

		// how long B has been held, as a bar that fills up
		if (holdStart != 0)
		{
			const int w = MIN(60, (int)((SDL_GetTicks() - holdStart) * 60 / holdToExitMs));
			JE_rectangle(VGAScreen, 129, 179, 190, 183, COLOR_OUTLINE);
			fill_rectangle_xy(VGAScreen, 130, 180, 129 + MAX(1, w), 182, C(14, 12));
		}

		if (fadeIn)
		{
			fade_palette(colors, 10, 0, 255);
			fadeIn = false;
		}

		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();

		waitUntilElapsed();

		bool done = false;

		// B and Back also arrive as Esc, but push_joysticks_as_keyboard()
		// leaves the key symbol empty; only a real Esc key exits right away
		KeyboardInput keyboardInput;
		while (keyboardGetInput(&keyboardInput))
		{
			if (keyboardInput.scancode == SDL_SCANCODE_ESCAPE && keyboardInput.sym == SDLK_ESCAPE)
				done = true;
		}

		const bool bHeld = anyGamepadButton(SDL_CONTROLLER_BUTTON_B);

		if (!bHeld)
			holdStart = 0;
		else if (holdStart == 0)
			holdStart = MAX(1, SDL_GetTicks());
		else if (SDL_GetTicks() - holdStart >= holdToExitMs)
			done = true;

		MouseInput mouseInput;
		while (mouseGetInput(INPUT_NO_MOTION, &mouseInput))
			done = true;

		if (done)
		{
			JE_playSampleNum(S_SPRING);

			// Wait for B to be let go: while held it keeps sending Esc, which
			// would also leave the menu this screen returns to.
			while (anyGamepadButton(SDL_CONTROLLER_BUTTON_B))
			{
				setFrameCount(1);
				waitUntilElapsed();
				while (keyboardGetInput(&keyboardInput))
					;
			}
			while (keyboardGetInput(&keyboardInput))
				;
			return;
		}
	}
}
