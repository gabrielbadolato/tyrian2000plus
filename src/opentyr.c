/*
 * OpenTyrian: A modern cross-platform port of Tyrian
 * Copyright (C) The OpenTyrian Development Team
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
#include "opentyr.h"

#include "about.h"
#include "config.h"
#include "demo.h"
#include "destruct.h"
#include "editship.h"
#include "episodes.h"
#include "file.h"
#include "font.h"
#include "fonthand.h"
#include "helptext.h"
#include "joystick.h"
#include "jukebox.h"
#include "keyboard.h"
#include "lang.h"
#include "logging.h"
#include "loudness.h"
#include "mainint.h"
#include "mouse.h"
#include "mtrand.h"
#include "network.h"
#include "padmap.h"
#include "nortsong.h"
#include "nortvars.h"
#include "opentyrian_version.h"
#include "palette.h"
#include "params.h"
#include "picload.h"
#include "sprite.h"
#include "tyrian2.h"
#include "update_check.h"
#include "varz.h"
#include "vga256d.h"
#include "video.h"
#include "video_scale.h"
#include "xmas.h"

#include "SDL.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

const char *opentyrian_str = "Tyrian 2000+";
const char *opentyrian_version = OPENTYRIAN_VERSION;

static size_t getDisplayPickerItemsCount(void)
{
	return 1 + (size_t)SDL_GetNumVideoDisplays();
}

static const char *getDisplayPickerItem(size_t i, char *buffer, size_t bufferSize)
{
	if (i == 0)
		return tr("Window");

	snprintf(buffer, bufferSize, tr("Display %d"), (int)i);
	return buffer;
}

static size_t getScalerPickerItemsCount(void)
{
	return (size_t)scalers_count;
}

static const char *getScalerPickerItem(size_t i, char *buffer, size_t bufferSize)
{
	(void)buffer, (void)bufferSize;

	return scalers[i].name;
}

static size_t getOffOnPickerItemsCount(void)
{
	return 2;
}

static const char *getOffOnPickerItem(size_t i, char *buffer, size_t bufferSize)
{
	(void)buffer, (void)bufferSize;

	return i == 0 ? tr("Off") : tr("On");
}

static const char *const brightnessNames[] = { "Normal", "Bright", "Brighter", "Brightest" };

static size_t getBrightnessPickerItemsCount(void)
{
	return COUNTOF(brightnessNames);
}

static const char *getBrightnessPickerItem(size_t i, char *buffer, size_t bufferSize)
{
	(void)buffer, (void)bufferSize;

	return tr(brightnessNames[i]);
}

static const char *const mouseActionNames[] =
{
	"Fire Main Weapon",
	"Left Sidekick",
	"Right Sidekick",
	"Both Sidekicks",
	"Change Rear Mode",
};

static size_t getMouseActionPickerItemsCount(void)
{
	return COUNTOF(mouseActionNames);
}

static const char *getMouseActionPickerItem(size_t i, char *buffer, size_t bufferSize)
{
	(void)buffer, (void)bufferSize;

	return tr(mouseActionNames[i]);
}

static size_t getScalingModePickerItemsCount(void)
{
	return (size_t)ScalingMode_MAX;
}

static const char *getScalingModePickerItem(size_t i, char *buffer, size_t bufferSize)
{
	(void)buffer, (void)bufferSize;

	return tr(scaling_mode_names[i]);
}

static size_t getLanguagePickerItemsCount(void)
{
	return lang_setting_count();
}

static const char *getLanguagePickerItem(size_t i, char *buffer, size_t bufferSize)
{
	(void)buffer, (void)bufferSize;

	return lang_setting_name(i);
}

// waits for a key press and assigns it to keySettings[i], swapping with the
// action that used it before; the value being changed is at (x, y)
static void captureKeySetting(size_t i, int x, int y, int w, int xStatus, const char *status)
{
	// replace the status text
	memcpy((Uint8 *)VGAScreen->pixels + VGAScreen->pitch * 188, (Uint8 *)VGAScreen2->pixels + VGAScreen2->pitch * 188, (size_t)VGAScreen->pitch * 12);
	JE_textShade(VGAScreen, xStatus, 190, tr(status), 15, 4, PART_SHADE);

	int col = 248, colC = 1;

	bool joyHeld = joydown;
	while (true)
	{
		joyHeld &= joydown;

		setFrameCount(1);

		col += colC;
		if (col < 243 || col > 248)
			colC *= -1;
		JE_rectangle(VGAScreen, x - 3, y - 2, x + w, y + 10, col);

		JE_showVGA();

		delayUntilElapsed();

		poll_joysticks();  // Notably, not push_joysticks_as_keyboard.
		handleSdlEvents();

		if (hasInput(INPUT_NO_MOTION) || (joydown && !joyHeld))
			break;
	}

	KeyboardInput keyboardInput;

	if (mouseGetInput(INPUT_NO_MOTION, NULL) || !keyboardGetInput(&keyboardInput))
		return;  // cancel

	const SDL_Scancode scancode = keyboardInput.scancode;
	if (scancode == SDL_SCANCODE_ESCAPE)
	{
		JE_playSampleNum(S_SPRING);
		return;
	}
	if (scancode == SDL_SCANCODE_F11 ||  // reserved for gamma
	    scancode == SDL_SCANCODE_P)      // reserved for pause
	{
		JE_playSampleNum(S_CLINK);
		return;
	}

	// already used? then swap
	for (size_t j = 0; j < COUNTOF(keySettings); ++j)
	{
		if (keySettings[j] == scancode)
		{
			keySettings[j] = keySettings[i];
			break;
		}
	}

	JE_playSampleNum(S_CLICK);
	keySettings[i] = scancode;
}

// controlsOnly: open straight on Controls and leave when backing out of it
// (the pause menu during a level)
static void runSetupMenu(bool controlsOnly)
{
	typedef enum
	{
		MENU_ITEM_NONE = 0,
		MENU_ITEM_DONE,
		MENU_ITEM_GRAPHICS,
		MENU_ITEM_SOUND,
		MENU_ITEM_CONTROLS,
		MENU_ITEM_JUKEBOX,
		MENU_ITEM_DESTRUCT,
		MENU_ITEM_ABOUT,
		MENU_ITEM_DISPLAY,
		MENU_ITEM_SCALER,
		MENU_ITEM_SCALING_MODE,
		MENU_ITEM_VSYNC,
		MENU_ITEM_SHARP_BILINEAR,
		MENU_ITEM_BRIGHTNESS,
		MENU_ITEM_AUTO_FIRE,
		MENU_ITEM_UPDATE_CHECK,
		MENU_ITEM_LANGUAGE,
		MENU_ITEM_KEYBOARD,
		MENU_ITEM_JOYSTICK,
		MENU_ITEM_CONTROLLER_MAP,
		MENU_ITEM_MOUSE,
		MENU_ITEM_KEY,                                      // + action (0-7)
		MENU_ITEM_KEY_LAST = MENU_ITEM_KEY + 7,
		MENU_ITEM_KEYS_RESET,
		MENU_ITEM_JOYSTICK_NUMBER,
		MENU_ITEM_JOYSTICK_ANALOG,
		MENU_ITEM_JOYSTICK_SENSITIVITY,
		MENU_ITEM_JOYSTICK_THRESHOLD,
		MENU_ITEM_JOYSTICK_BUTTONS,
		MENU_ITEM_JOYSTICK_RESET,
		MENU_ITEM_JOYSTICK_ACTION,                          // + action (0-10)
		MENU_ITEM_JOYSTICK_ACTION_LAST = MENU_ITEM_JOYSTICK_ACTION + JOYSTICK_ASSIGNMENTS - 1,
		MENU_ITEM_MOUSE_BUTTON,                             // + button (0-2)
		MENU_ITEM_MOUSE_BUTTON_LAST = MENU_ITEM_MOUSE_BUTTON + 2,
		MENU_ITEM_MOUSE_RESET,
		MENU_ITEM_MUSIC_VOLUME,
		MENU_ITEM_SOUND_VOLUME,
	} MenuItemId;

	// items that stand for one of several actions share the first one's id
#define MENU_ITEM_BASE(id) \
	((id) >= MENU_ITEM_KEY && (id) <= MENU_ITEM_KEY_LAST ? MENU_ITEM_KEY : \
	 (id) >= MENU_ITEM_JOYSTICK_ACTION && (id) <= MENU_ITEM_JOYSTICK_ACTION_LAST ? MENU_ITEM_JOYSTICK_ACTION : \
	 (id) >= MENU_ITEM_MOUSE_BUTTON && (id) <= MENU_ITEM_MOUSE_BUTTON_LAST ? MENU_ITEM_MOUSE_BUTTON : \
	 (id))

	typedef enum
	{
		MENU_NONE = 0,
		MENU_SETUP,
		MENU_GRAPHICS,
		MENU_SOUND,
		MENU_CONTROLS,
		MENU_KEYBOARD,
		MENU_JOYSTICK,
		MENU_JOYSTICK_BUTTONS,
		MENU_MOUSE,
	} MenuId;

	typedef struct
	{
		MenuItemId id;
		const char *name;
		const char *description;
		size_t (*getPickerItemsCount)(void);
		const char *(*getPickerItem)(size_t i, char *buffer, size_t bufferSize);
	} MenuItem;

	typedef struct
	{
		const char *header;
		bool compact;  // tighter rows, for long lists
		int wItemName; // 0 = default
		const MenuItem items[13];
	} Menu;

	static const Menu menus[] = {
		[MENU_SETUP] = {
			.header = "Setup",
			.items = {
				{ MENU_ITEM_GRAPHICS, "Graphics...", "Change the graphics settings." },
				{ MENU_ITEM_SOUND, "Sound...", "Change the sound settings." },
				{ MENU_ITEM_CONTROLS, "Controls...", "Change the control settings." },
				{ MENU_ITEM_JUKEBOX, "Jukebox", "Listen to the music of Tyrian." },
				// { MENU_ITEM_DESTRUCT, "Destruct", "Play a bonus mini-game." },
				{ MENU_ITEM_LANGUAGE, "Language:", "Choose the language of the menus and help.", getLanguagePickerItemsCount, getLanguagePickerItem },
				{ MENU_ITEM_UPDATE_CHECK, "Check Updates:", "Look for a new version when the game starts.", getOffOnPickerItemsCount, getOffOnPickerItem },
				{ MENU_ITEM_ABOUT, "About...", "What this is, where it came from, and credits." },
				{ MENU_ITEM_DONE, "Done", "Return to the previous screen." },
				{ -1 }
			},
		},
		[MENU_GRAPHICS] = {
			.header = "Graphics",
			.items = {
				{ MENU_ITEM_DISPLAY, "Display:", "Change the display mode.", getDisplayPickerItemsCount, getDisplayPickerItem },
				{ MENU_ITEM_SCALER, "Scaler:", "Change the pixel art scaling algorithm.", getScalerPickerItemsCount, getScalerPickerItem },
				{ MENU_ITEM_SCALING_MODE, "Scaling Mode:", "Change the scaling mode.", getScalingModePickerItemsCount, getScalingModePickerItem },
				{ MENU_ITEM_SHARP_BILINEAR, "Sharp Bilinear:", "Keep pixels even when the scale is not exact.", getOffOnPickerItemsCount, getOffOnPickerItem },
				{ MENU_ITEM_VSYNC, "VSync:", "Sync to the display to prevent tearing.", getOffOnPickerItemsCount, getOffOnPickerItem },
				{ MENU_ITEM_BRIGHTNESS, "Brightness:", "Brighten the levels (F11 also works in game).", getBrightnessPickerItemsCount, getBrightnessPickerItem },
				{ MENU_ITEM_DONE, "Done", "Return to the previous menu." },
				{ -1 }
			},
		},
		[MENU_SOUND] = {
			.header = "Sound",
			.items = {
				{ MENU_ITEM_MUSIC_VOLUME, "Music Volume", "Change volume with the left/right arrow keys." },
				{ MENU_ITEM_SOUND_VOLUME, "Sound Volume", "Change volume with the left/right arrow keys." },
				{ MENU_ITEM_DONE, "Done", "Return to the previous menu." },
				{ -1 }
			},
		},
		[MENU_CONTROLS] = {
			.header = "Controls",
			.items = {
				{ MENU_ITEM_KEYBOARD, "Keyboard...", "Change the keyboard keys." },
				{ MENU_ITEM_JOYSTICK, "Controller...", "Change the controller (gamepad) settings." },
				{ MENU_ITEM_CONTROLLER_MAP, "Controller Map...", "See what each controller button does." },
				{ MENU_ITEM_MOUSE, "Mouse...", "Change the mouse buttons." },
				{ MENU_ITEM_AUTO_FIRE, "Auto Fire:", "Press fire once to start or stop firing.", getOffOnPickerItemsCount, getOffOnPickerItem },
				{ MENU_ITEM_DONE, "Done", "Return to the previous menu." },
				{ -1 }
			},
		},
		[MENU_KEYBOARD] = {
			.header = "Keyboard",
			.compact = true,
			.items = {
				{ MENU_ITEM_KEY + 0, "Up", "Press Enter, then the new key." },
				{ MENU_ITEM_KEY + 1, "Down", "Press Enter, then the new key." },
				{ MENU_ITEM_KEY + 2, "Left", "Press Enter, then the new key." },
				{ MENU_ITEM_KEY + 3, "Right", "Press Enter, then the new key." },
				{ MENU_ITEM_KEY + 4, "Fire", "Press Enter, then the new key." },
				{ MENU_ITEM_KEY + 5, "Change Fire", "Press Enter, then the new key." },
				{ MENU_ITEM_KEY + 6, "Left Sidekick", "Press Enter, then the new key." },
				{ MENU_ITEM_KEY + 7, "Right Sidekick", "Press Enter, then the new key." },
				{ MENU_ITEM_KEYS_RESET, "Reset to Defaults", "Restore the default keys." },
				{ MENU_ITEM_DONE, "Done", "Return to the previous menu." },
				{ -1 }
			},
		},
		[MENU_JOYSTICK] = {
			.header = "Controller",
			.wItemName = 105,
			.items = {
				{ MENU_ITEM_JOYSTICK_NUMBER, "Controller:", "Choose which controller to change." },
				{ MENU_ITEM_JOYSTICK_ANALOG, "Analog:", "Use the stick for precise movement.", getOffOnPickerItemsCount, getOffOnPickerItem },
				{ MENU_ITEM_JOYSTICK_SENSITIVITY, "Sensitivity:", "Change with the left/right arrow keys." },
				{ MENU_ITEM_JOYSTICK_THRESHOLD, "Threshold:", "Change with the left/right arrow keys." },
				{ MENU_ITEM_JOYSTICK_BUTTONS, "Buttons...", "Choose the button for each action." },
				{ MENU_ITEM_JOYSTICK_RESET, "Reset to Defaults", "Restore the default controller settings." },
				{ MENU_ITEM_DONE, "Done", "Return to the previous menu." },
				{ -1 }
			},
		},
		[MENU_JOYSTICK_BUTTONS] = {
			.header = "Buttons",
			.compact = true,
			.wItemName = 112,
			.items = {
				{ MENU_ITEM_JOYSTICK_ACTION + 0, "Up", "Press Enter, then a button to add or remove." },
				{ MENU_ITEM_JOYSTICK_ACTION + 2, "Down", "Press Enter, then a button to add or remove." },
				{ MENU_ITEM_JOYSTICK_ACTION + 3, "Left", "Press Enter, then a button to add or remove." },
				{ MENU_ITEM_JOYSTICK_ACTION + 1, "Right", "Press Enter, then a button to add or remove." },
				{ MENU_ITEM_JOYSTICK_ACTION + 4, "Fire", "Press Enter, then a button to add or remove." },
				{ MENU_ITEM_JOYSTICK_ACTION + 5, "Change Fire", "Press Enter, then a button to add or remove." },
				{ MENU_ITEM_JOYSTICK_ACTION + 6, "Left Sidekick", "Press Enter, then a button to add or remove." },
				{ MENU_ITEM_JOYSTICK_ACTION + 7, "Right Sidekick", "Press Enter, then a button to add or remove." },
				{ MENU_ITEM_JOYSTICK_ACTION + 10, "Both Sidekicks", "Press Enter, then a button to add or remove." },
				{ MENU_ITEM_JOYSTICK_ACTION + 8, "Menu", "Press Enter, then a button to add or remove." },
				{ MENU_ITEM_JOYSTICK_ACTION + 9, "Pause", "Press Enter, then a button to add or remove." },
				{ MENU_ITEM_DONE, "Done", "Return to the previous menu." },
				{ -1 }
			},
		},
		[MENU_MOUSE] = {
			.header = "Mouse",
			.wItemName = 115,
			.items = {
				{ MENU_ITEM_MOUSE_BUTTON + 0, "Left Button:", "Choose what this button does.", getMouseActionPickerItemsCount, getMouseActionPickerItem },
				{ MENU_ITEM_MOUSE_BUTTON + 1, "Right Button:", "Choose what this button does.", getMouseActionPickerItemsCount, getMouseActionPickerItem },
				{ MENU_ITEM_MOUSE_BUTTON + 2, "Middle Button:", "Choose what this button does.", getMouseActionPickerItemsCount, getMouseActionPickerItem },
				{ MENU_ITEM_MOUSE_RESET, "Reset to Defaults", "Restore the default mouse buttons." },
				{ MENU_ITEM_DONE, "Done", "Return to the previous menu." },
				{ -1 }
			},
		},
	};

	char buffer[100];

	if (shopSpriteSheet.data == NULL)
		JE_loadCompShapes(&shopSpriteSheet, '1');  // need mouse pointer sprites

	bool restart = true;

	MenuId menuParents[COUNTOF(menus)] = { MENU_NONE };
	size_t selectedMenuItemIndexes[COUNTOF(menus)] = { 0 };
	MenuId currentMenu = controlsOnly ? MENU_CONTROLS : MENU_SETUP;
	MenuItemId currentPicker = MENU_ITEM_NONE;
	size_t pickerSelectedIndex = 0;

	const int xCenter = 320 / 2;
	const int yMenuHeader = 4;
	const int xMenuItem = 45;
	const int xMenuItemName = xMenuItem;
	const int hMenuItem = 13;

	int joystickIndex = 0;  // controller being changed

	// changes the controller number, sensitivity or threshold by one step, wrapping around
#define stepJoystickSetting(id, step) \
	do { \
		if (joysticks == 0) \
			break; \
		if ((id) == MENU_ITEM_JOYSTICK_NUMBER) \
			joystickIndex = (joystickIndex + joysticks + (step)) % joysticks; \
		else if (!joystick[joystickIndex].analog) \
			break; \
		else if ((id) == MENU_ITEM_JOYSTICK_SENSITIVITY) \
			joystick[joystickIndex].sensitivity = (joystick[joystickIndex].sensitivity + 11 + (step)) % 11; \
		else \
			joystick[joystickIndex].threshold = (joystick[joystickIndex].threshold + 11 + (step)) % 11; \
		JE_playSampleNum(S_CURSOR); \
	} while (false)

	for (; ; )
	{
		setFrameCount(1);

		if (restart)
		{
			JE_loadPic(VGAScreen2, 2, false);
			fill_rectangle_wh(VGAScreen2, 0, 192, 320, 8, 0);
		}

		// Restore background.
		memcpy(VGAScreen->pixels, VGAScreen2->pixels, (size_t)VGAScreen->pitch * VGAScreen->h);

		const Menu *menu = &menus[currentMenu];

		const int wMenuItemName = menu->wItemName != 0 ? menu->wItemName : 135;
		const int xMenuItemValue = xMenuItemName + wMenuItemName;
		const int wMenuItemValue = menu->wItemName != 0 ? 320 - 10 - xMenuItemValue : 95;
		const int wMenuItem = wMenuItemName + wMenuItemValue;
		size_t itemsCount = 0;
		while (menu->items[itemsCount].id != (MenuItemId)-1)
			++itemsCount;

		const int yMenuItems = menu->compact ? 30 : 37;
		// a long menu closes up so its last item stays above the description
		const int dyMenuItems = menu->compact ? 13 : itemsCount > 7 ? (176 - yMenuItems) / (int)(itemsCount - 1) : 21;

		if (joystickIndex >= joysticks)  // a controller was disconnected
			joystickIndex = 0;
		Joystick *joy = joysticks > 0 ? &joystick[joystickIndex] : NULL;

		// Draw header.
		drawFontHvShadowAligned(VGAScreen, xCenter, yMenuHeader, tr(menu->header), FONT_LARGE, ALIGN_CENTER, 15, -3, false, 2);

		int yPicker = 0;
		const int dyPickerItem = 15;
		const int dyPickerItemPadding = 2;
		const int hPickerItem = dyPickerItem - dyPickerItemPadding;

		size_t *const selectedMenuItemIndex = &selectedMenuItemIndexes[currentMenu];
		const MenuItem *const menuItems = menu->items;

		// Draw menu items.

		size_t menuItemsCount = 0;
		for (size_t i = 0; menuItems[i].id != (MenuItemId)-1; ++i)
		{
			menuItemsCount += 1;

			const MenuItem *const menuItem = &menuItems[i];

			const int y = yMenuItems + dyMenuItems * i;

			const bool selected = i == *selectedMenuItemIndex;
			const bool disabled = currentPicker != MENU_ITEM_NONE && !selected;

			if (selected)
				yPicker = y;

			const char *const name = tr(menuItem->name);

			drawFontHvShadow(VGAScreen, xMenuItemName, y, name, FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);

			switch (MENU_ITEM_BASE(menuItem->id))
			{
			case MENU_ITEM_DISPLAY:;
				const char *value = tr("Window");
				if (fullscreen_display >= 0)
				{
					snprintf(buffer, sizeof(buffer), tr("Display %d"), fullscreen_display + 1);
					value = buffer;
				}

				drawFontHvShadow(VGAScreen, xMenuItemValue, y, value, FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_SCALER:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, scalers[scaler].name, FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_SCALING_MODE:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, tr(scaling_mode_names[scaling_mode]), FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_SHARP_BILINEAR:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, sharp_bilinear_enabled ? tr("On") : tr("Off"), FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_VSYNC:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, vsync_enabled ? tr("On") : tr("Off"), FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_BRIGHTNESS:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, tr(brightnessNames[gammaCorrection % COUNTOF(brightnessNames)]), FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_AUTO_FIRE:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, autoFireToggle ? tr("On") : tr("Off"), FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_LANGUAGE:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, lang_setting_name((size_t)lang_setting), FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_UPDATE_CHECK:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, update_check_enabled ? tr("On") : tr("Off"), FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_KEY:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, SDL_GetScancodeName(keySettings[menuItem->id - MENU_ITEM_KEY]), FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_JOYSTICK_NUMBER:
			case MENU_ITEM_JOYSTICK_ANALOG:
			case MENU_ITEM_JOYSTICK_SENSITIVITY:
			case MENU_ITEM_JOYSTICK_THRESHOLD:
			case MENU_ITEM_JOYSTICK_ACTION:;
				const char *joyValue = buffer;
				bool joyDisabled = disabled || joy == NULL;
				if (joy == NULL)
					joyValue = menuItem->id == MENU_ITEM_JOYSTICK_NUMBER ? tr("None") : "-";
				else if (menuItem->id == MENU_ITEM_JOYSTICK_NUMBER)
				{
					// "1: Steam Controller", shortened with "..." if it does not fit
					snprintf(buffer, sizeof(buffer), "%d: %s", joystickIndex + 1, joystick_display_name(joystickIndex));
					for (size_t len = strlen(buffer); len > 4 && JE_textWidth(buffer, FONT_NORMAL) > wMenuItemValue; --len)
						strcpy(&buffer[len - 4], "...");
				}
				else if (menuItem->id == MENU_ITEM_JOYSTICK_ANALOG)
					joyValue = joy->analog ? tr("On") : tr("Off");
				else if (menuItem->id == MENU_ITEM_JOYSTICK_SENSITIVITY || menuItem->id == MENU_ITEM_JOYSTICK_THRESHOLD)
				{
					snprintf(buffer, sizeof(buffer), "%d", menuItem->id == MENU_ITEM_JOYSTICK_SENSITIVITY ? joy->sensitivity : joy->threshold);
					joyDisabled |= !joy->analog;
				}
				else
					joystick_assignments_to_icons(joystickIndex, buffer, sizeof(buffer), joy->assignment[menuItem->id - MENU_ITEM_JOYSTICK_ACTION]);

				drawFontHvShadow(VGAScreen, xMenuItemValue, y, joyValue, FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (joyDisabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_MOUSE_BUTTON:
				drawFontHvShadow(VGAScreen, xMenuItemValue, y, tr(mouseActionNames[mouseSettings[menuItem->id - MENU_ITEM_MOUSE_BUTTON] - 1]), FONT_NORMAL, 15, -3 + (selected ? 2 : 0) + (disabled ? -4 : 0), false, 2);
				break;

			case MENU_ITEM_MUSIC_VOLUME:
				JE_barDrawShadow(VGAScreen, xMenuItemValue, y, 1, music_disabled ? 170 : 174, (tyrMusicVolume + 4) / 8, 2, 10);
				JE_rectangle(VGAScreen, xMenuItemValue - 2, y - 2, xMenuItemValue + 96, y + 11, 242);
				break;

			case MENU_ITEM_SOUND_VOLUME:
				JE_barDrawShadow(VGAScreen, xMenuItemValue, y, 1, samples_disabled ? 170 : 174, (fxVolume + 4) / 8, 2, 10);
				JE_rectangle(VGAScreen, xMenuItemValue - 2, y - 2, xMenuItemValue + 96, y + 11, 242);
				break;

			default:
				break;
			}
		}

		// Draw status text.
		const char *description = tr(menuItems[*selectedMenuItemIndex].description);
		if (menuItems[*selectedMenuItemIndex].id == MENU_ITEM_JOYSTICK_NUMBER && joy != NULL)
			description = joystick_display_name(joystickIndex);
		JE_textShade(VGAScreen, xMenuItemName, 190, description, 15, 4, PART_SHADE);

		// Draw picker box and items.

		if (currentPicker != MENU_ITEM_NONE)
		{
			const MenuItem *selectedMenuItem = &menuItems[*selectedMenuItemIndex];
			const size_t pickerItemsCount = selectedMenuItem->getPickerItemsCount();

			const int hPicker = dyPickerItem * pickerItemsCount - dyPickerItemPadding;
			yPicker = MIN(yPicker, 200 - 10 - (hPicker + 5 + 2));

			JE_rectangle(VGAScreen, xMenuItemValue - 5, yPicker- 3, xMenuItemValue + wMenuItemValue + 5 - 1, yPicker + hPicker + 3 - 1, 248);
			JE_rectangle(VGAScreen, xMenuItemValue - 4, yPicker- 4, xMenuItemValue + wMenuItemValue + 4 - 1, yPicker + hPicker + 4 - 1, 250);
			JE_rectangle(VGAScreen, xMenuItemValue - 3, yPicker- 5, xMenuItemValue + wMenuItemValue + 3 - 1, yPicker + hPicker + 5 - 1, 248);
			fill_rectangle_wh(VGAScreen, xMenuItemValue - 2, yPicker - 2, wMenuItemValue + 2 + 2, hPicker + 2 + 2, 224);

			for (size_t i = 0; i < pickerItemsCount; ++i)
			{
				const int y = yPicker + dyPickerItem * (int)i;

				const bool selected = i == pickerSelectedIndex;

				const char *value = selectedMenuItem->getPickerItem(i, buffer, sizeof buffer);

				drawFontHvShadow(VGAScreen, xMenuItemValue, y, value, FONT_NORMAL, 15, -3 + (selected ? 2 : 0), false, 2);
			}
		}

		if (restart)
		{
			mouseCursor = MOUSE_POINTER_NORMAL;

			fade_palette(colors, 10, 0, 255);

			restart = false;
		}

		JE_mouseStart();
		JE_showVGA();
		JE_mouseReplace();

		int oldFullscreenDisplay = fullscreen_display;
		while (true)
		{
			waitUntilElapsed();

			// If full-screen is toggled via keyboard shortcut then display
			// setting needs to be updated.
			if (fullscreen_display != oldFullscreenDisplay)
				break;

			if (hasInput(INPUT_ANY))
				break;

			setFrameCount(1);
		}

		// Waiting polls the controllers, which rebuilds the list when one is
		// connected or disconnected, so look the selected one up again.
		if (joystickIndex >= joysticks)
			joystickIndex = 0;
		joy = joysticks > 0 ? &joystick[joystickIndex] : NULL;

		if (currentPicker == MENU_ITEM_NONE)
		{
			// Handle menu item interaction.

			bool action = false;

			MouseInput mouseInput;
			KeyboardInput keyboardInput;

			if (mouseGetInput(INPUT_ANY, &mouseInput))
			{
				// Find menu item name or value that was hovered or clicked.
				if (mouseInput.x >= xMenuItem && mouseInput.x < xMenuItem + wMenuItem)
				{
					for (size_t i = 0; i < menuItemsCount; ++i)
					{
						const int yMenuItem = yMenuItems + dyMenuItems * i;
						if (mouseInput.y >= yMenuItem && mouseInput.y < yMenuItem + hMenuItem)
						{
							if (*selectedMenuItemIndex != i)
							{
								JE_playSampleNum(S_CURSOR);

								*selectedMenuItemIndex = i;
							}

							if (mouseInput.button == SDL_BUTTON_LEFT &&
							    mouseInput.y >= yMenuItem && mouseInput.y < yMenuItem + hMenuItem)
							{
								// Act on menu item via name.
								if (mouseInput.x >= xMenuItemName && mouseInput.x < xMenuItemName + wMenuItemName)
								{
									action = true;
								}

								// Act on menu item via value.
								else if (mouseInput.x >= xMenuItemValue && mouseInput.x < xMenuItemValue + wMenuItemValue)
								{
									switch (MENU_ITEM_BASE(menuItems[*selectedMenuItemIndex].id))
									{
									case MENU_ITEM_DISPLAY:
									case MENU_ITEM_SCALER:
									case MENU_ITEM_SCALING_MODE:
									case MENU_ITEM_SHARP_BILINEAR:
									case MENU_ITEM_VSYNC:
									case MENU_ITEM_BRIGHTNESS:
									case MENU_ITEM_AUTO_FIRE:
									case MENU_ITEM_UPDATE_CHECK:
									case MENU_ITEM_LANGUAGE:
									case MENU_ITEM_KEY:
									case MENU_ITEM_JOYSTICK_NUMBER:
									case MENU_ITEM_JOYSTICK_ANALOG:
									case MENU_ITEM_JOYSTICK_SENSITIVITY:
									case MENU_ITEM_JOYSTICK_THRESHOLD:
									case MENU_ITEM_JOYSTICK_ACTION:
									case MENU_ITEM_MOUSE_BUTTON:
									{
										action = true;
										break;
									}
									case MENU_ITEM_MUSIC_VOLUME:
									{
										JE_playSampleNum(S_CURSOR);

										int value = (mouseInput.x - xMenuItemValue) * 255 / (wMenuItemValue - 1);
										tyrMusicVolume = MIN(MAX(0, value), 255);

										set_volume(tyrMusicVolume, fxVolume);
										break;
									}
									case MENU_ITEM_SOUND_VOLUME:
									{
										int value = (mouseInput.x - xMenuItemValue) * 255 / (wMenuItemValue - 1);
										fxVolume = MIN(MAX(0, value), 255);

										set_volume(tyrMusicVolume, fxVolume);

										JE_playSampleNum(S_CURSOR);
										break;
									}
									default:
										break;
									}
								}
							}

							break;
						}
					}
				}

				if (mouseInput.button == SDL_BUTTON_RIGHT)
				{
					JE_playSampleNum(S_SPRING);

					currentMenu = menuParents[currentMenu];
				}
			}
			else if (keyboardGetInput(&keyboardInput))
			{
				switch (keyboardInput.scancode)
				{
				case SDL_SCANCODE_UP:
				{
					JE_playSampleNum(S_CURSOR);

					*selectedMenuItemIndex = *selectedMenuItemIndex == 0
						? menuItemsCount - 1
						: *selectedMenuItemIndex - 1;
					break;
				}
				case SDL_SCANCODE_DOWN:
				{
					JE_playSampleNum(S_CURSOR);

					*selectedMenuItemIndex = *selectedMenuItemIndex == menuItemsCount - 1
						? 0
						: *selectedMenuItemIndex + 1;
					break;
				}
				case SDL_SCANCODE_LEFT:
				{
					switch (menuItems[*selectedMenuItemIndex].id)
					{
					case MENU_ITEM_JOYSTICK_NUMBER:
					case MENU_ITEM_JOYSTICK_SENSITIVITY:
					case MENU_ITEM_JOYSTICK_THRESHOLD:
					{
						stepJoystickSetting(menuItems[*selectedMenuItemIndex].id, -1);
						break;
					}
					case MENU_ITEM_MUSIC_VOLUME:
					{
						JE_playSampleNum(S_CURSOR);

						JE_changeVolume(&tyrMusicVolume, -8, &fxVolume, 0);
						break;
					}
					case MENU_ITEM_SOUND_VOLUME:
					{
						JE_changeVolume(&tyrMusicVolume, 0, &fxVolume, -8);

						JE_playSampleNum(S_CURSOR);
						break;
					}
					default:
						break;
					}
					break;
				}
				case SDL_SCANCODE_RIGHT:
				{
					switch (menuItems[*selectedMenuItemIndex].id)
					{
					case MENU_ITEM_JOYSTICK_NUMBER:
					case MENU_ITEM_JOYSTICK_SENSITIVITY:
					case MENU_ITEM_JOYSTICK_THRESHOLD:
					{
						stepJoystickSetting(menuItems[*selectedMenuItemIndex].id, +1);
						break;
					}
					case MENU_ITEM_MUSIC_VOLUME:
					{
						JE_playSampleNum(S_CURSOR);

						JE_changeVolume(&tyrMusicVolume, 8, &fxVolume, 0);
						break;
					}
					case MENU_ITEM_SOUND_VOLUME:
					{
						JE_changeVolume(&tyrMusicVolume, 0, &fxVolume, 8);

						JE_playSampleNum(S_CURSOR);
						break;
					}
					default:
						break;
					}
					break;
				}
				case SDL_SCANCODE_SPACE:
				case SDL_SCANCODE_RETURN:
				{
					action = true;
					break;
				}
				case SDL_SCANCODE_ESCAPE:
				{
					JE_playSampleNum(S_SPRING);

					currentMenu = menuParents[currentMenu];
					break;
				}
				default:
					break;
				}
			}

			if (action)
			{
				const MenuItemId selectedMenuItemId = menuItems[*selectedMenuItemIndex].id;
				const int ySelected = yMenuItems + dyMenuItems * (int)*selectedMenuItemIndex;

				MenuId subMenu = MENU_NONE;

				switch (MENU_ITEM_BASE(selectedMenuItemId))
				{
				case MENU_ITEM_KEYBOARD:
					subMenu = MENU_KEYBOARD;
					break;
				case MENU_ITEM_JOYSTICK:
					subMenu = MENU_JOYSTICK;
					break;
				case MENU_ITEM_JOYSTICK_BUTTONS:
					subMenu = MENU_JOYSTICK_BUTTONS;
					break;
				case MENU_ITEM_MOUSE:
					subMenu = MENU_MOUSE;
					break;
				case MENU_ITEM_CONTROLLER_MAP:
				{
					JE_playSampleNum(S_SELECT);

					fade_black(10);

					// the controller chosen in Controller..., if it is a gamepad
					controllerMapScreen(joy != NULL && joy->controller != NULL ? joystickIndex : joystick_first_gamepad());

					fade_black(10);

					restart = true;
					break;
				}
				case MENU_ITEM_KEY:
				{
					JE_playSampleNum(S_CLICK);

					captureKeySetting(selectedMenuItemId - MENU_ITEM_KEY, xMenuItemValue, ySelected, wMenuItemValue, xMenuItemName, "Press the new key (Esc to cancel).");
					break;
				}
				case MENU_ITEM_KEYS_RESET:
				{
					JE_playSampleNum(S_CLICK);

					memcpy(keySettings, defaultKeySettings, sizeof(keySettings));
					break;
				}
				case MENU_ITEM_JOYSTICK_NUMBER:
				case MENU_ITEM_JOYSTICK_SENSITIVITY:
				case MENU_ITEM_JOYSTICK_THRESHOLD:
				{
					stepJoystickSetting(selectedMenuItemId, +1);
					break;
				}
				case MENU_ITEM_JOYSTICK_ANALOG:
				{
					if (joy == NULL)
						break;

					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = joy->analog ? 1 : 0;
					break;
				}
				case MENU_ITEM_JOYSTICK_RESET:
				{
					if (joy == NULL)
						break;

					JE_playSampleNum(S_CLICK);

					reset_joystick_assignments(joystickIndex);
					break;
				}
				case MENU_ITEM_JOYSTICK_ACTION:
				{
					if (joy == NULL)
						break;

					JE_playSampleNum(S_CLICK);

					memcpy((Uint8 *)VGAScreen->pixels + VGAScreen->pitch * 188, (Uint8 *)VGAScreen2->pixels + VGAScreen2->pitch * 188, (size_t)VGAScreen->pitch * 12);
					JE_textShade(VGAScreen, xMenuItemName, 190, tr("Press a button on the controller (Esc to cancel)."), 15, 4, PART_SHADE);
					JE_rectangle(VGAScreen, xMenuItemValue - 3, ySelected - 2, xMenuItemValue + wMenuItemValue, ySelected + 10, 248);
					JE_showVGA();

					if (!assign_joystick_action(joystickIndex, selectedMenuItemId - MENU_ITEM_JOYSTICK_ACTION))
						getInput();  // discard the key or click that cancelled
					break;
				}
				case MENU_ITEM_MOUSE_BUTTON:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = mouseSettings[selectedMenuItemId - MENU_ITEM_MOUSE_BUTTON] - 1;
					break;
				}
				case MENU_ITEM_MOUSE_RESET:
				{
					JE_playSampleNum(S_CLICK);

					memcpy(mouseSettings, defaultMouseSettings, sizeof(mouseSettings));
					break;
				}
				case MENU_ITEM_DONE:
				{
					JE_playSampleNum(S_SELECT);

					currentMenu = menuParents[currentMenu];
					break;
				}
				case MENU_ITEM_GRAPHICS:
				{
					JE_playSampleNum(S_SELECT);

					menuParents[MENU_GRAPHICS] = currentMenu;
					currentMenu = MENU_GRAPHICS;
					selectedMenuItemIndexes[currentMenu] = 0;
					break;
				}
				case MENU_ITEM_SOUND:
				{
					JE_playSampleNum(S_SELECT);

					menuParents[MENU_SOUND] = currentMenu;
					currentMenu = MENU_SOUND;
					selectedMenuItemIndexes[currentMenu] = 0;
					break;
				}
				case MENU_ITEM_CONTROLS:
				{
					JE_playSampleNum(S_SELECT);

					menuParents[MENU_CONTROLS] = currentMenu;
					currentMenu = MENU_CONTROLS;
					selectedMenuItemIndexes[currentMenu] = 0;
					break;
				}
				case MENU_ITEM_ABOUT:
				{
					JE_playSampleNum(S_SELECT);

					fade_black(10);

					aboutScreen();

					fade_black(10);

					restart = true;
					break;
				}
				case MENU_ITEM_JUKEBOX:
				{
					JE_playSampleNum(S_SELECT);

					fade_black(10);

					jukebox();

					restart = true;
					break;
				}
				case MENU_ITEM_DESTRUCT:
				{
					JE_playSampleNum(S_SELECT);

					fade_black(10);

					JE_destructGame();

					restart = true;
					break;
				}
				case MENU_ITEM_DISPLAY:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = (size_t)(fullscreen_display + 1);
					break;
				}
				case MENU_ITEM_SCALER:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = scaler;
					break;
				}
				case MENU_ITEM_SCALING_MODE:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = scaling_mode;
					break;
				}
				case MENU_ITEM_SHARP_BILINEAR:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = sharp_bilinear_enabled ? 1 : 0;
					break;
				}
				case MENU_ITEM_VSYNC:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = vsync_enabled ? 1 : 0;
					break;
				}
				case MENU_ITEM_BRIGHTNESS:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = gammaCorrection % COUNTOF(brightnessNames);
					break;
				}
				case MENU_ITEM_AUTO_FIRE:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = autoFireToggle ? 1 : 0;
					break;
				}
				case MENU_ITEM_UPDATE_CHECK:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = update_check_enabled ? 1 : 0;
					break;
				}
				case MENU_ITEM_LANGUAGE:
				{
					JE_playSampleNum(S_CLICK);

					currentPicker = selectedMenuItemId;
					pickerSelectedIndex = (size_t)lang_setting;
					break;
				}
				case MENU_ITEM_MUSIC_VOLUME:
				{
					JE_playSampleNum(S_CLICK);

					music_disabled = !music_disabled;
					if (!music_disabled)
						restart_song();
					break;
				}
				case MENU_ITEM_SOUND_VOLUME:
				{
					samples_disabled = !samples_disabled;

					JE_playSampleNum(S_CLICK);
					break;
				}
				default:
					break;
				}

				if (subMenu != MENU_NONE)
				{
					JE_playSampleNum(S_SELECT);

					menuParents[subMenu] = currentMenu;
					currentMenu = subMenu;
					selectedMenuItemIndexes[currentMenu] = 0;
				}
			}

			if (currentMenu == MENU_NONE)
			{
				fade_black(10);

				return;
			}
		}
		else
		{
			const MenuItem *selectedMenuItem = &menuItems[*selectedMenuItemIndex];

			// Handle picker interaction.

			bool action = false;

			MouseInput mouseInput;
			KeyboardInput keyboardInput;

			if (mouseGetInput(INPUT_ANY, &mouseInput))
			{
				const size_t pickerItemsCount = selectedMenuItem->getPickerItemsCount();

				// Find picker item that was hovered or clicked.
				if (mouseInput.x >= xMenuItemValue && mouseInput.x < xMenuItemValue + wMenuItemValue)
				{
					for (size_t i = 0; i < pickerItemsCount; ++i)
					{
						const int yPickerItem = yPicker + dyPickerItem * i;

						if (mouseInput.y >= yPickerItem && mouseInput.y < yPickerItem + hPickerItem)
						{
							if (pickerSelectedIndex != i)
							{
								JE_playSampleNum(S_CURSOR);

								pickerSelectedIndex = i;
							}

							// Act on picker item.
							if (mouseInput.button == SDL_BUTTON_LEFT &&
							    mouseInput.x >= xMenuItemValue && mouseInput.y < xMenuItemValue + wMenuItemName &&
							    mouseInput.y >= yPickerItem && mouseInput.y < yPickerItem + hPickerItem)
							{
								action = true;
							}
						}
					}
				}

				if (mouseInput.button == SDL_BUTTON_RIGHT)
				{
					JE_playSampleNum(S_SPRING);

					currentPicker = MENU_ITEM_NONE;
				}
			}
			else if (keyboardGetInput(&keyboardInput))
			{
				switch (keyboardInput.scancode)
				{
				case SDL_SCANCODE_UP:
				{
					JE_playSampleNum(S_CURSOR);

					const size_t pickerItemsCount = selectedMenuItem->getPickerItemsCount();

					pickerSelectedIndex = pickerSelectedIndex == 0
						? pickerItemsCount - 1
						: pickerSelectedIndex - 1;
					break;
				}
				case SDL_SCANCODE_DOWN:
				{
					JE_playSampleNum(S_CURSOR);

					const size_t pickerItemsCount = selectedMenuItem->getPickerItemsCount();

					pickerSelectedIndex = pickerSelectedIndex == pickerItemsCount - 1
						? 0
						: pickerSelectedIndex + 1;
					break;
				}
				case SDL_SCANCODE_SPACE:
				case SDL_SCANCODE_RETURN:
				{
					action = true;
					break;
				}
				case SDL_SCANCODE_ESCAPE:
				{
					JE_playSampleNum(S_SPRING);

					currentPicker = MENU_ITEM_NONE;
					break;
				}
				default:
					break;
				}
			}

			if (action)
			{
				JE_playSampleNum(S_CLICK);

				switch (MENU_ITEM_BASE(selectedMenuItem->id))
				{
				case MENU_ITEM_JOYSTICK_ANALOG:
				{
					if (joy != NULL)
						joy->analog = pickerSelectedIndex == 1;
					break;
				}
				case MENU_ITEM_MOUSE_BUTTON:
				{
					mouseSettings[selectedMenuItem->id - MENU_ITEM_MOUSE_BUTTON] = pickerSelectedIndex + 1;
					break;
				}
				case MENU_ITEM_DISPLAY:
				{
					if ((int)pickerSelectedIndex - 1 != fullscreen_display)
						reinit_fullscreen((int)pickerSelectedIndex - 1);
					break;
				}
				case MENU_ITEM_SCALER:
				{
					if (pickerSelectedIndex != scaler)
					{
						const int oldScaler = scaler;
						if (!init_scaler(pickerSelectedIndex) &&  // try new scaler
						    !init_scaler(oldScaler))              // revert on fail
						{
							exit(EXIT_FAILURE);
						}
					}
					break;
				}
				case MENU_ITEM_SCALING_MODE:
				{
					scaling_mode = pickerSelectedIndex;
					break;
				}
				case MENU_ITEM_SHARP_BILINEAR:
				{
					set_sharp_bilinear(pickerSelectedIndex == 1);
					break;
				}
				case MENU_ITEM_VSYNC:
				{
					set_vsync(pickerSelectedIndex == 1);
					break;
				}
				case MENU_ITEM_BRIGHTNESS:
				{
					gammaCorrection = pickerSelectedIndex;  // applied when a level starts
					break;
				}
				case MENU_ITEM_AUTO_FIRE:
				{
					autoFireToggle = pickerSelectedIndex == 1;
					break;
				}
				case MENU_ITEM_UPDATE_CHECK:
				{
					update_check_enabled = pickerSelectedIndex == 1;
					update_check_start();  // if it was off when the game started
					break;
				}
				case MENU_ITEM_LANGUAGE:
				{
					if ((int)pickerSelectedIndex != lang_setting)
					{
						lang_set((int)pickerSelectedIndex);
						lang_reload_texts();
					}
					break;
				}
				default:
					break;
				}

				currentPicker = MENU_ITEM_NONE;
			}
		}
	}

#undef stepJoystickSetting
#undef MENU_ITEM_BASE
}

static File logFile;
static SDL_LogOutputFunction defaultLogOutput;
static void *defaultLogUserdata;

void setupMenu(void)
{
	runSetupMenu(false);
}

void controlsMenu(void)
{
	runSetupMenu(true);
}

// copies every log message to opentyrian.log in the user directory
static void logToFile(void *userdata, int category, SDL_LogPriority priority, const char *message)
{
	(void)userdata;

	// SDL 2.32 on Windows logs this on every relative mouse motion; it is noise
	if (strncmp(message, "speed = ", 8) == 0)
		return;

	if (logFile.f != NULL)
	{
		fprintf(logFile.f, "%s\n", message);
		fflush(logFile.f);
	}

	if (defaultLogOutput != NULL)
		defaultLogOutput(defaultLogUserdata, category, priority, message);
}

static void closeLogFile(void)
{
	SDL_LogSetOutputFunction(defaultLogOutput, defaultLogUserdata);
	fileClose(&logFile);
}

int main(int argc, char *argv[])
{
#ifndef NDEBUG
	SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);
#endif

	logFile = userFileOpen("opentyrian.log", "w");
	if (logFile.f != NULL)
	{
		SDL_LogGetOutputFunction(&defaultLogOutput, &defaultLogUserdata);
		SDL_LogSetOutputFunction(logToFile, NULL);
		atexit(closeLogFile);
	}

	mt_srand(time(NULL));

	logInfo("%s", "");
	logInfo("Welcome to... >> %s %s <<", opentyrian_str, opentyrian_version);
	logInfo("%s", "");
	logInfo("Copyright (C) The OpenTyrian Development Team");
	logInfo("Copyright (C) Kaito Sinclaire");
	logInfo("Copyright (C) 2026 Tyrian 2000+ contributors");
	logInfo("%s", "");
	logInfo("This program comes with ABSOLUTELY NO WARRANTY.");
	logInfo("This is free software, and you are welcome to redistribute it");
	logInfo("under certain conditions.  See the file COPYING for details.");
	logInfo("%s", "");

	if (SDL_Init(0) != 0)
	{
		logFatal("Failed to initialize SDL: %s", SDL_GetError());
		return EXIT_FAILURE;
	}

	atexit(SDL_Quit);

	loadConfiguration();
	lang_set(lang_setting);

	xmas = xmas_time();  // arg handler may override

	JE_paramCheck(argc, argv);

	update_check_start();  // in the background; the title screen shows the result

	if (!findDataFiles())
	{
		logFatal("The Tyrian 2000 data files were not found.  OpenTyrian2000 requires the Tyrian 2000 data files.");
		return EXIT_FAILURE;
	}

	File file = dataFileOpen("tyrian.shp", "rb");
	Uint16 temp = fileReadU16(&file);
	fileClose(&file);

	if (temp != 13)
	{
		logFatal("The Tyrian v1.x/v2.x data files were found.  OpenTyrian2000 requires the Tyrian 2000 data files.");
		return EXIT_FAILURE;
	}

	// Tyrian 2000 requires help text to be loaded before the saves,
	// because the default high score names are stored in help text.
	JE_loadHelpText();

	loadSaves();

	JE_scanForEpisodes();

	init_video();
	init_keyboard();
	init_joysticks();
	if (has_mouse)
		logInfo("Assuming mouse detected.");  // SDL can't tell us if there isn't one.

	if (xmas && (!dataFileExists("tyrianc.shp") || !dataFileExists("voicesc.snd")))
	{
		xmas = false;

		logWarn("Christmas is missing.");
	}

	loadPals();
	JE_loadMainShapeTables(xmas ? "tyrianc.shp" : "tyrian.shp");

	if (xmas && !override_xmas && !xmas_prompt())
	{
		xmas = false;

		free_main_shape_tables();
		JE_loadMainShapeTables("tyrian.shp");
	}

	/* Default Options */
	youAreCheating = false;
	smoothScroll = true;
	loadDestruct = false;

	if (!audio_disabled)
	{
		if (init_audio())
			loadSndFile(xmas);
	}
	else
	{
		logInfo("Audio is disabled.");
	}

	if (recordDemo)
		logInfo("Game will be recorded.");

	loadExtraShapes();  /*Editship*/

	if (isNetworkGame)
	{
#ifdef WITH_NETWORK
		if (network_init())
		{
			network_tyrian_halt(3, false);
		}
#else
		logFatal("OpenTyrian was compiled without networking support.");
		return EXIT_FAILURE;
#endif
	}

	for (; ; )
	{
#ifdef NDEBUG
		if (!isNetworkGame && !stoppedDemo)
			intro_logos();
#endif

		JE_initPlayerData();
		JE_sortHighScores();

		playDemo = false;
		stoppedDemo = false;

		gameLoaded = false;
		jumpSection = false;

#ifdef WITH_NETWORK
		if (isNetworkGame)
		{
			networkStartScreen();
		}
		else
#endif
		{
			if (!titleScreen())
			{
				// Player quit from title screen.
				break;
			}
		}

		if (loadDestruct)
		{
			JE_destructGame();

			loadDestruct = false;
		}
		else
		{
			JE_main();

			if (trentWin)
			{
				// Player beat SuperTyrian.
				break;
			}
		}
	}

	JE_tyrianHalt(0);

	return 0;
}
