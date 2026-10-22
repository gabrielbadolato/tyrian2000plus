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
 * The "Network Game" item of the new game menu, which the original game had
 * for its DOS modem and network play and OpenTyrian left disabled (its network
 * game starts only with --net).  One player hosts and waits; the other types
 * the host's address and joins.  Only the host needs to be reachable: the
 * other player's address comes with its first packet (network.c).
 */
#include "netmenu.h"

#include "font.h"
#include "fonthand.h"
#include "joystick.h"
#include "keyboard.h"
#include "lang.h"
#include "logging.h"
#include "mouse.h"
#include "network.h"
#include "nortsong.h"
#include "osk.h"
#include "palette.h"
#include "picload.h"
#include "sprite.h"
#include "touch.h"
#include "varz.h"
#include "vga256d.h"
#include "video.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NETMENU_PORT 1333

static char playerName[21] = "";
static char joinAddress[64] = "";
static int hostPort = NETMENU_PORT;

void netmenu_load_settings(Config *config)
{
	ConfigSection *section = config_find_section(config, "network", NULL);
	if (section == NULL)
		return;

	const char *value;
	if (config_get_string_option(section, "name", &value))
		SDL_strlcpy(playerName, value, sizeof(playerName));
	if (config_get_string_option(section, "address", &value))
		SDL_strlcpy(joinAddress, value, sizeof(joinAddress));
	if (config_get_string_option(section, "port", &value))
	{
		const int port = atoi(value);
		if (port > 0 && port < 65536)
			hostPort = port;
	}
}

void netmenu_save_settings(Config *config)
{
	ConfigSection *section = config_find_or_add_section(config, "network", NULL);
	if (section == NULL)
		exit(EXIT_FAILURE);  // out of memory

	char port[8];
	snprintf(port, sizeof(port), "%d", hostPort);
	config_set_string_option(section, "name", playerName);
	config_set_string_option(section, "address", joinAddress);
	config_set_string_option(section, "port", port);
}

#ifdef WITH_NETWORK

enum { ITEM_HOST, ITEM_JOIN, ITEM_NAME, ITEM_ADDRESS, ITEM_BACK, ITEMS };

static const int xCenter = 320 / 2;
static const int yHeader = 20;
static const int yItems = 54;
static const int dyItems = 20;  // Back ends above the keyboard on the screen
static const int hItem = 13;

static void drawBackground(const char *header)
{
	JE_loadPic(VGAScreen2, 2, false);
	drawFontHvShadowAligned(VGAScreen2, xCenter, yHeader, header, FONT_LARGE, ALIGN_CENTER, 15, -3, false, 2);
}

static void drawStatus(const char *text)
{
	JE_textShade(VGAScreen, xCenter - JE_textWidth(text, TINY_FONT) / 2, 190, text, 15, 4, PART_SHADE);
}

static bool editingName = false;

static const char *defaultName(void)
{
	return playerName[0] != '\0' || editingName ? playerName : tr("Player");
}

static void itemText(int item, char *buffer, size_t size)
{
	switch (item)
	{
	case ITEM_HOST:
		snprintf(buffer, size, "%s", tr("Host Game"));
		break;
	case ITEM_JOIN:
		snprintf(buffer, size, "%s", tr("Join Game"));
		break;
	case ITEM_NAME:
		snprintf(buffer, size, "%s %s", tr("Name:"), defaultName());
		break;
	case ITEM_ADDRESS:
		snprintf(buffer, size, "%s %s", tr("Address:"), joinAddress);
		break;
	default:
		snprintf(buffer, size, "%s", tr("Back"));
		break;
	}
}

// all the items on VGAScreen (selected lit); returns the widths for the mouse
static void drawItems(int selected, int *widths)
{
	for (int i = 0; i < ITEMS; ++i)
	{
		char line[96];
		itemText(i, line, sizeof(line));
		widths[i] = JE_textWidth(line, FONT_NORMAL);
		const bool disabled = i == ITEM_JOIN && joinAddress[0] == '\0';
		drawFontHvShadow(VGAScreen, xCenter - widths[i] / 2, yItems + dyItems * i, line, FONT_NORMAL, 15, -4 + (i == selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
	}
}

// "Press Esc to cancel.", or the controller's cancel button
static void drawCancelHint(void)
{
	char text[64];
	snprintf(text, sizeof(text), "%s", tr("Press Esc to cancel."));
	for (int j = 0; j < joysticks; ++j)
	{
		if (joystick[j].controller != NULL)
		{
			snprintf(text, sizeof(text), tr("Press %s to cancel."), joystick_button_icon(j, SDL_CONTROLLER_BUTTON_B));
			break;
		}
	}
	drawStatus(text);
}

static const char *itemHelp(int item)
{
	switch (item)
	{
	case ITEM_HOST: return tr("Wait here for the other player to join.");
	case ITEM_JOIN: return tr("Join the game of the address below.");
	case ITEM_NAME: return tr("Your name, shown to the other player.");
	case ITEM_ADDRESS: return tr("The address of the computer hosting the game.");
	default: return tr("Return to the previous menu.");
	}
}

// edits the text of an item in place, with the keyboard or the keyboard on the
// screen; false if canceled (the text is then as before)
static bool editItem(int item, char *text, size_t size, bool address)
{
	char before[64];
	SDL_strlcpy(before, text, sizeof(before));
	size_t len = strlen(text);

	static Osk osk;
	osk_init(&osk, VGAScreen2);
	SDL_StartTextInput();

	bool done = false, accepted = false;
	int blink = 0;
	while (!done)
	{
		setFrameCount(1);

		memcpy(VGAScreen->pixels, VGAScreen2->pixels, (size_t)VGAScreen->pitch * VGAScreen->h);

		int widths[ITEMS];
		drawItems(item, widths);
		const int x = xCenter + widths[item] / 2, y = yItems + dyItems * item;
		if ((++blink / 10) % 2 == 0)
			fill_rectangle_xy(VGAScreen, x + 2, y + 1, x + 6, y + hItem - 2, 15 * 16 + 12);

		osk_draw(&osk, VGAScreen);
		if (joysticks == 0 && !touch_shown())  // else the keyboard on the screen is there
			drawStatus(tr("Type, then press Enter (Esc to cancel)."));

		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();

		waitUntilElapsed();

		Uint8 ch = 0;
		OskResult result = OSK_NONE;

		MouseInput mouseInput;
		KeyboardInput keyboardInput;
		if (mouseGetInput(INPUT_NO_MOTION, &mouseInput))
			result = osk_mouse(&osk, &mouseInput, &ch);
		else if (keyboardGetInput(&keyboardInput))
		{
			result = osk_key(&osk, &keyboardInput, &ch);
			if (result == OSK_NONE)
			{
				switch (keyboardInput.scancode)
				{
				case SDL_SCANCODE_BACKSPACE:
				case SDL_SCANCODE_DELETE:
					result = OSK_DELETE;
					break;
				case SDL_SCANCODE_RETURN:
				case SDL_SCANCODE_KP_ENTER:
					result = OSK_ACCEPT;
					break;
				case SDL_SCANCODE_ESCAPE:
					result = OSK_CANCEL;
					break;
				default:
					if (keyboardInput.ch != 0)
					{
						ch = keyboardInput.ch;
						result = OSK_CHAR;
					}
					break;
				}
			}
		}

		switch (result)
		{
		case OSK_CHAR:
		{
			// an address takes what host names and ports have; a name, what the font has
			const bool fits = address
				? (SDL_isalnum(ch) || ch == '.' || ch == '-' || ch == ':')
				: (ch == ' ' || (ch < 128 && fontMap[ch] != -1));
			if (fits && len + 1 < size && len < (address ? 40 : 15))
			{
				text[len++] = address ? (char)SDL_tolower(ch) : (char)ch;
				text[len] = '\0';
				JE_playSampleNum(S_CURSOR);
			}
			break;
		}
		case OSK_DELETE:
			if (len > 0)
			{
				text[--len] = '\0';
				JE_playSampleNum(S_CLICK);
			}
			break;
		case OSK_ACCEPT:
			JE_playSampleNum(S_SELECT);
			done = accepted = true;
			break;
		case OSK_CANCEL:
			JE_playSampleNum(S_SPRING);
			SDL_strlcpy(text, before, size);
			done = true;
			break;
		default:
			break;
		}
	}

	SDL_StopTextInput();
	osk_close(&osk, VGAScreen2);

	return accepted;
}

// a full screen of centered lines while connecting
static void showConnecting(const char *header, const char *const *lines, int count)
{
	drawBackground(header);
	memcpy(VGAScreen->pixels, VGAScreen2->pixels, (size_t)VGAScreen->pitch * VGAScreen->h);
	for (int i = 0; i < count; ++i)
	{
		if (lines[i] == NULL || lines[i][0] == '\0')
			continue;
		// the addresses brightest: they are what the other player needs
		const int brightness = i == 0 ? 2 : SDL_isdigit((unsigned char)lines[i][0]) ? 4 : 0;
		drawFontHvShadowAligned(VGAScreen, xCenter, 60 + 16 * i, lines[i], FONT_SMALL, ALIGN_CENTER, 15, brightness, false, 1);
	}
	drawCancelHint();
	JE_showVGA();
	fade_palette(colors, 10, 0, 255);
}

static void showError(const char *text)
{
	drawBackground(tr("Network Game"));
	memcpy(VGAScreen->pixels, VGAScreen2->pixels, (size_t)VGAScreen->pitch * VGAScreen->h);
	drawFontHvShadowAligned(VGAScreen, xCenter, 100, text, FONT_SMALL, ALIGN_CENTER, 15, 0, false, 1);
	JE_showVGA();
	fade_palette(colors, 10, 0, 255);
	waitUntilGetInput();
	fade_black(10);
}

// opens the network and connects; false if it failed or was canceled
static bool startNetwork(bool host, const char *address)
{
	static char name[21], hostName[64];
	SDL_strlcpy(name, defaultName(), sizeof(name));

	network_from_menu = true;
	network_player_name = name;
	thisPlayerNum = host ? 1 : 2;
	network_delay = 1 + 1;
	network_delay_fixed = false;

	if (host)
	{
		network_opponent_host = NULL;
		network_player_port = (Uint16)hostPort;
	}
	else
	{
		SDL_strlcpy(hostName, address, sizeof(hostName));
		network_opponent_port = NETMENU_PORT;
		char *colon = strchr(hostName, ':');
		if (colon != NULL)
		{
			*colon = '\0';
			const int port = atoi(colon + 1);
			if (port > 0 && port < 65536)
				network_opponent_port = (Uint16)port;
		}
		network_opponent_host = hostName;
	}

	if (network_init() != 0)
	{
		char text[80];
		snprintf(text, sizeof(text), tr("Could not open port %d."), hostPort);
		showError(host ? text : tr("Network connection failed."));
		network_close();
		return false;
	}

	isNetworkGame = true;

	if (host)
	{
		// this computer's addresses in the local network, for the other player to type
		char lines[6][64];
		const char *shown[6] = { NULL };
		int count = 0;
		snprintf(lines[count], sizeof(lines[count]), "%s", tr("Waiting for the other player..."));
		shown[count] = lines[count]; count++;
		snprintf(lines[count], sizeof(lines[count]), "%s", tr("Address of this computer:"));
		shown[count] = lines[count]; count++;

		IPaddress addresses[8];
		const int n = SDLNet_GetLocalAddresses(addresses, (int)COUNTOF(addresses));
		int listed = 0;
		for (int i = 0; i < n && listed < 2; ++i)
		{
			const Uint8 *a = (const Uint8 *)&addresses[i].host;
			if (a[0] == 127 || a[0] == 0 || (a[0] == 169 && a[1] == 254))
				continue;  // this computer itself, or no network
			if (hostPort == NETMENU_PORT)
				snprintf(lines[count], sizeof(lines[count]), "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
			else
				snprintf(lines[count], sizeof(lines[count]), "%u.%u.%u.%u:%d", a[0], a[1], a[2], a[3], hostPort);
			shown[count] = lines[count]; count++;
			listed++;
		}
		if (listed == 0)
		{
			snprintf(lines[count], sizeof(lines[count]), "%s", tr("(not found)"));
			shown[count] = lines[count]; count++;
		}
		snprintf(lines[count], sizeof(lines[count]), "%s", tr("Over the internet: see the README."));
		shown[count + 1] = lines[count];
		showConnecting(tr("Host Game"), shown, count + 2);
	}
	else
	{
		char line[96];
		snprintf(line, sizeof(line), tr("Connecting to %s..."), address);
		const char *shown[1] = { line };
		showConnecting(tr("Join Game"), shown, 1);
	}

	if (network_connect() != 0)
	{
		network_close();
		return false;
	}

	logInfo("Network game: connected as player %u, with %s.", thisPlayerNum, network_opponent_name);
	return true;
}

bool networkMenu(void)
{
	int selected = ITEM_HOST;
	int widths[ITEMS] = { 0 };
	bool restart = true;

	for (; ; )
	{
		setFrameCount(1);

		if (restart)
			drawBackground(tr("Network Game"));

		memcpy(VGAScreen->pixels, VGAScreen2->pixels, (size_t)VGAScreen->pitch * VGAScreen->h);

		drawItems(selected, widths);
		drawStatus(itemHelp(selected));

		if (restart)
		{
			mouseCursor = MOUSE_POINTER_NORMAL;
			fade_palette(colors, 10, 0, 255);
			restart = false;
		}

		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();

		waitUntilElapsed();
		waitUntilHasInput(INPUT_ANY);

		bool action = false, cancel = false;

		MouseInput mouseInput;
		KeyboardInput keyboardInput;
		if (mouseGetInput(INPUT_ANY, &mouseInput))
		{
			for (int i = 0; i < ITEMS; ++i)
			{
				const int x = xCenter - widths[i] / 2;
				const int y = yItems + dyItems * i - (dyItems - hItem) / 2;
				if (mouseInput.x >= x && mouseInput.x < x + widths[i] && mouseInput.y >= y && mouseInput.y < y + dyItems)
				{
					if (selected != i)
					{
						JE_playSampleNum(S_CURSOR);
						selected = i;
					}
					if (mouseInput.button == SDL_BUTTON_LEFT)
						action = true;
					break;
				}
			}
			if (mouseInput.button == SDL_BUTTON_RIGHT)
				cancel = true;
		}
		else if (keyboardGetInput(&keyboardInput))
		{
			switch (keyboardInput.scancode)
			{
			case SDL_SCANCODE_UP:
				JE_playSampleNum(S_CURSOR);
				selected = selected == 0 ? ITEMS - 1 : selected - 1;
				break;
			case SDL_SCANCODE_DOWN:
				JE_playSampleNum(S_CURSOR);
				selected = selected == ITEMS - 1 ? 0 : selected + 1;
				break;
			case SDL_SCANCODE_SPACE:
			case SDL_SCANCODE_RETURN:
				action = true;
				break;
			case SDL_SCANCODE_ESCAPE:
				cancel = true;
				break;
			default:
				break;
			}
		}

		if (action)
		{
			switch (selected)
			{
			case ITEM_HOST:
			case ITEM_JOIN:
				if (selected == ITEM_JOIN && joinAddress[0] == '\0')
				{
					// first say where to
					JE_playSampleNum(S_CLICK);
					selected = ITEM_ADDRESS;
					if (!editItem(ITEM_ADDRESS, joinAddress, sizeof(joinAddress), true) || joinAddress[0] == '\0')
						break;
					selected = ITEM_JOIN;
				}
				JE_playSampleNum(S_SELECT);
				saveConfiguration();  // the name and the address are kept
				fade_black(10);
				if (startNetwork(selected == ITEM_HOST, joinAddress))
					return true;
				restart = true;
				break;
			case ITEM_NAME:
				JE_playSampleNum(S_CLICK);
				editingName = true;
				editItem(ITEM_NAME, playerName, sizeof(playerName), false);
				editingName = false;
				break;
			case ITEM_ADDRESS:
				JE_playSampleNum(S_CLICK);
				editItem(ITEM_ADDRESS, joinAddress, sizeof(joinAddress), true);
				break;
			default:
				cancel = true;
				break;
			}
		}

		if (cancel)
		{
			JE_playSampleNum(S_SPRING);
			fade_black(10);
			return false;
		}
	}
}

#endif /* WITH_NETWORK */
