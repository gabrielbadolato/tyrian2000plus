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
 * About: what Tyrian 2000+ is, where it came from and who made it.  The
 * original game's credits are read from tyrian.cdt, so they are the game's
 * own.  Link lines open in the browser with Enter (A on a controller).
 */
#include "about.h"

#include "file.h"
#include "font.h"
#include "fonthand.h"
#include "helptext.h"
#include "joystick.h"
#include "keyboard.h"
#include "lang.h"
#include "logging.h"
#include "mouse.h"
#include "nortsong.h"
#include "opentyr.h"
#include "palette.h"
#include "picload.h"
#include "sndmast.h"
#include "update_check.h"
#include "vga256d.h"
#include "video.h"

#include "SDL.h"

#include <stdio.h>
#include <string.h>

typedef enum
{
	STYLE_BLANK,
	STYLE_HEADING,
	STYLE_TEXT,
	STYLE_NAME,
	STYLE_LINK,
	STYLE_CREDITS,  // not a line: where the original game's credits go
} AboutStyle;

typedef struct
{
	AboutStyle style;
	const char *text;
	const char *url;
} AboutEntry;

static const AboutEntry aboutEntries[] =
{
	{ STYLE_HEADING, "Tyrian 2000+" },
	{ STYLE_TEXT, "A free, unofficial fan project. It is not made, sold or endorsed by Eclipse Software, Jason Emery or the original publishers. The game data is the original freeware release, unchanged." },
	{ STYLE_BLANK },
	{ STYLE_TEXT, "Tyrian is a vertical scrolling shooter set in the year 20,031, where you play as Trent Hawkins, a fighter pilot out to save the galaxy. It was made by Eclipse Software and published by Epic MegaGames in 1995. Tyrian 2000 (1999) added a fifth episode. Jason Emery later released the game as freeware." },
	{ STYLE_BLANK },
	{ STYLE_HEADING, "A note from Gabriel" },
	{ STYLE_TEXT, "Hi! I'm Gabriel Badolato, from Brazil." },
	{ STYLE_BLANK },
	{ STYLE_TEXT, "Tyrian 2000 was a big part of my childhood. It was one of the games that made me love computers, and years later I worked with them for a living. I wanted to play it again with a modern controller, on a big screen and on the Steam Deck. So I started this project." },
	{ STYLE_BLANK },
	{ STYLE_TEXT, "I'm not a game developer. I worked in IT for years, and now I'm finishing a degree in biomedicine, hoping to work in bioinformatics. I learned to program in college, but I never wrote much code. Claude, an AI assistant made by Anthropic, wrote most of it. I tested every build on my own PC and controllers." },
	{ STYLE_BLANK },
	{ STYLE_TEXT, "Thank you to Jason Emery, Alexander Brandon, Daniel Cook and everyone at Eclipse Software, and to the OpenTyrian and OpenTyrian2000 developers who kept the game alive." },
	{ STYLE_BLANK },
	{ STYLE_TEXT, "This project is free, and it always will be." },
	{ STYLE_BLANK },
	{ STYLE_TEXT, "Thank you for playing!" },
	{ STYLE_BLANK },
	{ STYLE_HEADING, "What this version adds" },
	{ STYLE_TEXT, "Modern controller support (Xbox, PlayStation, Switch Pro, Steam Controller and others), a Controller Map, Auto Fire, controller button names in the help screens, more graphics options in Setup and in the pause menu, Portuguese menus, and ready-to-play builds for Windows, Linux and the Steam Deck." },
	{ STYLE_BLANK },
	{ STYLE_HEADING, "Tyrian 2000 - original credits" },
	{ STYLE_CREDITS },
	{ STYLE_BLANK },
	{ STYLE_HEADING, "OpenTyrian" },
	{ STYLE_TEXT, "The open-source port of Tyrian that made all of this possible. By The OpenTyrian Development Team and contributors:" },
	{ STYLE_NAME, "Carl Reinke, Yuri Kunde Schlesner, mvcixT, syntaxglitch, Alex Rodionov, Stephan T. Lavavej, emacs.hacker, Miro Kropacek, Alexandre Detiste, sweetlilmre, Sander van Dijk, a.h.vandijk, qon, Adam Vandenberg, Dmitry Marakasov, Jan Alexander Steffens, Jeff Smith, Kane, Pedro Catalao, Renato Alves, Sergii Pylypenko, George Rawlinson, mcpusc, norm2782, okkirikomi, Patryk Obara, AshTR." },
	{ STYLE_BLANK },
	{ STYLE_HEADING, "OpenTyrian2000" },
	{ STYLE_TEXT, "The fork of OpenTyrian that recreates Tyrian 2000, which this project is built on. By:" },
	{ STYLE_NAME, "Kaito Sinclaire (KScl)" },
	{ STYLE_BLANK },
	{ STYLE_HEADING, "Tyrian 2000+" },
	{ STYLE_NAME, "Gabriel Badolato, with help from Claude (Anthropic)" },
	{ STYLE_BLANK },
	{ STYLE_HEADING, "Also thanks to" },
	{ STYLE_TEXT, "Sam Lantinga and the SDL team, for Simple DirectMedia Layer. camanis.net, for keeping the freeware game data available. And the Tyrian fans who kept the game alive." },
	{ STYLE_BLANK },
	{ STYLE_HEADING, "License" },
	{ STYLE_TEXT, "The program is free software under the GNU General Public License, version 2 or later; its source code is at the first link below. The game data is freeware: \"Feel free to play it all you want and share it with friends.\" This project is free and always will be." },
	{ STYLE_BLANK },
	{ STYLE_HEADING, "Links" },
	{ STYLE_TEXT, "Tyrian 2000+ (source code):" },
	{ STYLE_LINK, "github.com/gabrielbadolato/tyrian2000plus", "https://github.com/gabrielbadolato/tyrian2000plus" },
	{ STYLE_TEXT, "Tyrian 2000+ on itch.io:" },
	{ STYLE_LINK, "gabrielbadolato.itch.io/tyrian2000plus", "https://gabrielbadolato.itch.io/tyrian2000plus" },
	{ STYLE_TEXT, "OpenTyrian2000:" },
	{ STYLE_LINK, "github.com/KScl/opentyrian2000", "https://github.com/KScl/opentyrian2000" },
	{ STYLE_TEXT, "OpenTyrian:" },
	{ STYLE_LINK, "github.com/opentyrian/opentyrian", "https://github.com/opentyrian/opentyrian" },
	{ STYLE_TEXT, "Tyrian 2000 freeware game data:" },
	{ STYLE_LINK, "www.camanis.net/tyrian", "https://www.camanis.net/tyrian/" },
	{ STYLE_TEXT, "SDL:" },
	{ STYLE_LINK, "www.libsdl.org", "https://www.libsdl.org/" },
};

typedef struct
{
	AboutStyle style;
	char text[80];
	const char *url;
} AboutLine;

#define MAX_LINES 256

static AboutLine lines[MAX_LINES];
static int lineCount;

enum
{
	TEXT_X = 14,
	TEXT_WIDTH = 292,
	LIST_Y = 26,
	LINE_HEIGHT = 11,
	VISIBLE_LINES = 14,
};

static Font lineFont(AboutStyle style)
{
	return style == STYLE_HEADING ? FONT_NORMAL : FONT_SMALL;
}

static void addLine(AboutStyle style, const char *text, size_t len, const char *url)
{
	if (lineCount >= MAX_LINES)
		return;

	// no blank line at the top, nor two in a row
	if (style == STYLE_BLANK && (lineCount == 0 || lines[lineCount - 1].style == STYLE_BLANK))
		return;

	AboutLine *line = &lines[lineCount++];
	line->style = style;
	len = MIN(len, sizeof(line->text) - 1);
	memcpy(line->text, text, len);
	line->text[len] = '\0';
	line->url = url;
}

// breaks text into lines that fit TEXT_WIDTH
static void addWrapped(AboutStyle style, const char *text)
{
	const Font font = lineFont(style);

	while (*text != '\0')
	{
		char buffer[80];
		size_t fit = 0;  // length of the longest prefix that fits and ends at a word
		size_t len = 0;

		for (; ; )
		{
			while (text[len] != '\0' && text[len] != ' ')
				++len;

			if (len >= sizeof(buffer))
				break;
			memcpy(buffer, text, len);
			buffer[len] = '\0';
			if (fit > 0 && JE_textWidth(buffer, font) > TEXT_WIDTH)
				break;

			fit = len;
			if (text[len] == '\0')
				break;
			++len;
		}

		if (fit == 0)  // a single word too long for a line
			fit = MIN(len, sizeof(buffer) - 1);

		addLine(style, text, fit, NULL);

		text += fit;
		while (*text == ' ')
			++text;
	}
}

// the original game's credits, from tyrian.cdt: a name, then what they did
static void addOriginalCredits(void)
{
	char credstr[126][65 + 1];  // as in JE_playCredits()

	File file = dataFileOpen("tyrian.cdt", "rb");
	if (file.error)
	{
		logWarn("About: failed to open 'tyrian.cdt': %s", fileGetError(&file));
		return;
	}

	for (size_t i = 0; i < COUNTOF(credstr); ++i)
		readEncryptedString(&file, credstr[i], sizeof(credstr[i]));

	const bool error = file.error;
	fileClose(&file);
	if (error)
		return;

	bool afterBlank = true;
	for (size_t i = 0; i < COUNTOF(credstr); ++i)
	{
		// the first character is the line's color in the credits roll
		const char *text = credstr[i][0] != '\0' ? &credstr[i][1] : "";

		if (credstr[i][0] == '.' || credstr[i][0] == '\0')
		{
			afterBlank = true;
			continue;
		}
		if (strstr(text, "CREDITS") != NULL || strstr(text, "The   End") != NULL)
			continue;

		// the small font has no '&'
		char buffer[80];
		size_t len = 0;
		for (const char *c = text; *c != '\0' && len + 4 < sizeof(buffer); ++c)
		{
			if (*c == '&')
			{
				memcpy(&buffer[len], "and", 3);
				len += 3;
			}
			else
			{
				buffer[len++] = *c;
			}
		}

		if (afterBlank)
			addLine(STYLE_BLANK, "", 0, NULL);
		addLine(afterBlank ? STYLE_NAME : STYLE_TEXT, buffer, len, NULL);
		afterBlank = false;
	}
}

static void buildLines(void)
{
	lineCount = 0;

	const char *newerVersion = update_check_newer_version();
	if (newerVersion != NULL)
	{
		char text[160];
		addWrapped(STYLE_HEADING, tr("New version available"));
		snprintf(text, sizeof(text), tr("Tyrian 2000+ %s is out (this is %s). Download it from one of the links below. On the Steam Deck, run atualizar-tyrian.sh again."), newerVersion, opentyrian_version);
		addWrapped(STYLE_TEXT, text);
		addLine(STYLE_LINK, tr("Download on itch.io"), strlen(tr("Download on itch.io")), UPDATE_CHECK_ITCH_URL);
		addLine(STYLE_LINK, tr("Download on GitHub"), strlen(tr("Download on GitHub")), UPDATE_CHECK_RELEASES_URL);
		addLine(STYLE_BLANK, "", 0, NULL);
	}

	for (size_t i = 0; i < COUNTOF(aboutEntries); ++i)
	{
		const AboutEntry *entry = &aboutEntries[i];

		switch (entry->style)
		{
		case STYLE_BLANK:
			addLine(STYLE_BLANK, "", 0, NULL);
			break;
		case STYLE_CREDITS:
			addOriginalCredits();
			break;
		case STYLE_LINK:
			addLine(STYLE_LINK, entry->text, strlen(entry->text), entry->url);
			break;
		default:
			addWrapped(entry->style, tr(entry->text));
			break;
		}
	}
}

static void drawLines(int top, int cursor)
{
	// darken the background behind the text so it reads well
	JE_barShade(VGAScreen, 4, LIST_Y - 3, 315, LIST_Y + VISIBLE_LINES * LINE_HEIGHT);
	JE_barShade(VGAScreen, 4, LIST_Y - 3, 315, LIST_Y + VISIBLE_LINES * LINE_HEIGHT);

	for (int i = 0; i < VISIBLE_LINES && top + i < lineCount; ++i)
	{
		const AboutLine *line = &lines[top + i];
		const int y = LIST_Y + i * LINE_HEIGHT;
		const bool selected = top + i == cursor;

		if (selected)
		{
			JE_barBright(VGAScreen, 6, y - 2, 309, y + LINE_HEIGHT - 3);
			JE_barBright(VGAScreen, 6, y - 2, 309, y + LINE_HEIGHT - 3);
		}

		switch (line->style)
		{
		case STYLE_HEADING:
		{
			// the normal font has no '+' ("Tyrian 2000+"): draw it as a small cross
			char text[sizeof(line->text)];
			strcpy(text, line->text);
			const size_t len = strlen(text);
			const bool plus = len > 0 && text[len - 1] == '+';
			if (plus)
				text[len - 1] = '\0';

			drawFontHvShadow(VGAScreen, TEXT_X, y - 1, text, FONT_NORMAL, 15, -1, false, 2);

			if (plus)
			{
				const int x = TEXT_X + JE_textWidth(text, FONT_NORMAL) + 1;
				for (int shadow = 1; shadow >= 0; --shadow)
				{
					const Uint8 color = shadow ? 0 : 15 * 16 + 14;
					const int d = shadow ? 2 : 0;
					fill_rectangle_xy(VGAScreen, x + d + 3, y + d, x + d + 4, y + d + 7, color);
					fill_rectangle_xy(VGAScreen, x + d, y + d + 3, x + d + 7, y + d + 4, color);
				}
			}
			break;
		}
		case STYLE_NAME:
			drawFontHvShadow(VGAScreen, TEXT_X, y, line->text, FONT_SMALL, 15, 6, false, 1);
			break;
		case STYLE_LINK:
			drawFontHvShadow(VGAScreen, TEXT_X + 8, y, line->text, FONT_SMALL, 13, selected ? 6 : 2, false, 1);
			break;
		default:
			drawFontHvShadow(VGAScreen, TEXT_X, y, line->text, FONT_SMALL, 15, selected ? 4 : 2, false, 1);
			break;
		}
	}

	// scroll bar
	if (lineCount > VISIBLE_LINES)
	{
		const int h = VISIBLE_LINES * LINE_HEIGHT;
		const int barH = MAX(6, h * VISIBLE_LINES / lineCount);
		const int barY = LIST_Y + (h - barH) * top / (lineCount - VISIBLE_LINES);
		fill_rectangle_xy(VGAScreen, 312, LIST_Y, 313, LIST_Y + h - 1, 0);
		fill_rectangle_xy(VGAScreen, 312, barY, 313, barY + barH - 1, 15 * 16 + 8);
	}
}

static bool openLink(const AboutLine *line)
{
	if (line->url == NULL)
		return false;

	if (SDL_OpenURL(line->url) != 0)
	{
		logWarn("About: could not open '%s': %s", line->url, SDL_GetError());
		return false;
	}

	logInfo("About: opened '%s'", line->url);
	return true;
}

void aboutScreen(void)
{
	JE_loadPic(VGAScreen2, 2, false);
	fill_rectangle_wh(VGAScreen2, 0, 192, 320, 8, 0);

	buildLines();

	bool fadeIn = true;
	int cursor = 0;
	int top = 0;
	const char *status = NULL;  // shown instead of the help line after opening a link

	for (; ; )
	{
		setFrameCount(2);

		if (cursor < top)
			top = cursor;
		else if (cursor >= top + VISIBLE_LINES)
			top = cursor - VISIBLE_LINES + 1;

		memcpy(VGAScreen->pixels, VGAScreen2->pixels, (size_t)VGAScreen->pitch * VGAScreen->h);

		drawFontHvShadowAligned(VGAScreen, 160, 4, tr("About"), FONT_LARGE, ALIGN_CENTER, 15, -3, false, 2);

		drawLines(top, cursor);

		const int j = joystick_first_gamepad();
		char help[128];
		if (status != NULL)
			snprintf(help, sizeof(help), "%s", tr(status));
		else if (lines[cursor].style == STYLE_LINK)
			snprintf(help, sizeof(help), tr("Enter (%s): open this link in your browser."), joystick_button_icon(j, SDL_CONTROLLER_BUTTON_A));
		else
			snprintf(help, sizeof(help), tr("Up/Down: scroll. Left/Right: page. Esc (%s): back."), joystick_button_icon(j, SDL_CONTROLLER_BUTTON_B));
		JE_textShade(VGAScreen, TEXT_X, 190, help, 15, 4, PART_SHADE);

		if (fadeIn)
		{
			fade_palette(colors, 10, 0, 255);
			fadeIn = false;
		}

		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();

		waitUntilElapsed();

		const int oldCursor = cursor;
		bool done = false;

		KeyboardInput keyboardInput;
		while (keyboardGetInput(&keyboardInput))
		{
			switch (keyboardInput.scancode)
			{
			case SDL_SCANCODE_UP:
				cursor -= 1;
				break;
			case SDL_SCANCODE_DOWN:
				cursor += 1;
				break;
			case SDL_SCANCODE_LEFT:
			case SDL_SCANCODE_PAGEUP:
				cursor -= VISIBLE_LINES - 1;
				break;
			case SDL_SCANCODE_RIGHT:
			case SDL_SCANCODE_PAGEDOWN:
				cursor += VISIBLE_LINES - 1;
				break;
			case SDL_SCANCODE_HOME:
				cursor = 0;
				break;
			case SDL_SCANCODE_END:
				cursor = lineCount - 1;
				break;
			case SDL_SCANCODE_RETURN:
			case SDL_SCANCODE_SPACE:
				if (lines[cursor].url != NULL)
				{
					JE_playSampleNum(S_SELECT);
					status = openLink(&lines[cursor]) ? "Opening the link in your browser..." : "Could not open the browser. The link is shown above.";
				}
				break;
			case SDL_SCANCODE_ESCAPE:
				done = true;
				break;
			default:
				break;
			}
		}

		MouseInput mouseInput;
		while (mouseGetInput(INPUT_NO_MOTION, &mouseInput))
		{
			if (mouseInput.button == SDL_BUTTON_RIGHT)
			{
				done = true;
			}
			else if (mouseInput.button == SDL_BUTTON_LEFT)
			{
				const int i = top + (mouseInput.y - LIST_Y) / LINE_HEIGHT;
				if (mouseInput.y >= LIST_Y && i < lineCount && i < top + VISIBLE_LINES)
				{
					cursor = i;
					if (lines[i].url != NULL)
					{
						JE_playSampleNum(S_SELECT);
						status = openLink(&lines[i]) ? "Opening the link in your browser..." : "Could not open the browser. The link is shown above.";
					}
				}
			}
		}

		cursor = MAX(0, MIN(cursor, lineCount - 1));
		if (cursor != oldCursor)
		{
			JE_playSampleNum(S_CURSOR);
			status = NULL;
		}

		if (done)
		{
			JE_playSampleNum(S_SPRING);
			return;
		}
	}
}
