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
#include "joystick.h"

#include "config.h"
#include "config_file.h"
#include "file.h"
#include "keyboard.h"
#include "logging.h"
#include "network.h"
#include "nortsong.h"
#include "opentyr.h"
#include "padglyph.h"

#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <string.h>

/*
 * Input devices are opened through SDL's GameController API whenever SDL
 * recognizes them (Xbox, PlayStation, Switch Pro, Steam Controller/Deck and
 * anything listed in a gamecontrollerdb.txt).  Those use standardized
 * button/axis assignments (PAD_BUTTON/PAD_AXIS), so "A" is always the bottom
 * face button no matter which controller is plugged in.  Unrecognized devices
 * fall back to the original raw joystick assignments (AXIS/BUTTON/HAT).
 *
 * Devices can be connected and disconnected while the game is running.
 */

int joystick_axis_threshold(int j, int value);
static int check_assigned(const Joystick *js, const Joystick_assignment assignment[2]);

static const char *assignment_to_code(const Joystick_assignment *assignment);
static const char *assignment_to_label(int j, const Joystick_assignment *assignment);
static const char *assignment_to_icon(int j, const Joystick_assignment *assignment);
static void code_to_assignment(Joystick_assignment *assignment, const char *buffer);

static void open_joysticks(void);
static void close_joysticks(bool save);

int joystick_repeat_delay = 300; // milliseconds, repeat delay for buttons
bool joydown = false;            // any joystick buttons down, updated by poll_joysticks()
bool ignore_joystick = false;

int joysticks = 0;
Joystick *joystick = NULL;

static const int joystick_analog_max = 32767;

static const Uint32 hotplug_check_interval = 500; // milliseconds
static Uint32 next_hotplug_check = 0;

static bool rumble_enabled = true;

/* Standardized gamepad button codes (stored in the config file) */
static const char *const pad_button_codes[] =
{
	"A", "B", "X", "Y",
	"BACK", "GUIDE", "START",
	"LS", "RS",
	"LB", "RB",
	"DUP", "DDOWN", "DLEFT", "DRIGHT",
	"MISC", "P1", "P2", "P3", "P4", "TOUCH",
};

/* Standardized gamepad axis codes; [axis][negative] */
static const char *const pad_axis_codes[][2] =
{
	{ "LX+", "LX-" },
	{ "LY+", "LY-" },
	{ "RX+", "RX-" },
	{ "RY+", "RY-" },
	{ "LT", "LT-" },
	{ "RT", "RT-" },
};

/* What gets printed on screen, which depends on the controller family
 * (Pad_family, joystick.h).  Every label must fit in 6 characters (see
 * assignment_to_code). */

static const char *const pad_button_labels[FAMILY_COUNT][COUNTOF(pad_button_codes)] =
{
	[FAMILY_XBOX] = {
		"A", "B", "X", "Y",
		"VIEW", "XBOX", "MENU",
		"LS", "RS",
		"LB", "RB",
		"D-UP", "D-DOWN", "D-LEFT", "D-RGHT",
		"SHARE", "P1", "P2", "P3", "P4", "TOUCH",
	},
	[FAMILY_PLAYSTATION] = {
		"CROSS", "CIRCLE", "SQUARE", "TRI",
		"CREATE", "PS", "OPTION",
		"L3", "R3",
		"L1", "R1",
		"D-UP", "D-DOWN", "D-LEFT", "D-RGHT",
		"MIC", "P1", "P2", "P3", "P4", "TOUCH",
	},
	[FAMILY_NINTENDO] = {
		// positional: bottom, right, left, top
		"B", "A", "Y", "X",
		"-", "HOME", "+",
		"LS", "RS",
		"L", "R",
		"D-UP", "D-DOWN", "D-LEFT", "D-RGHT",
		"CAPTUR", "P1", "P2", "P3", "P4", "TOUCH",
	},
	[FAMILY_STEAM_DECK] = {
		"A", "B", "X", "Y",
		"VIEW", "STEAM", "MENU",
		"L3", "R3",
		"L1", "R1",
		"D-UP", "D-DOWN", "D-LEFT", "D-RGHT",
		"QAM", "R4", "L4", "R5", "L5", "TOUCH",
	},
};

static const char *const pad_axis_labels[FAMILY_COUNT][COUNTOF(pad_axis_codes)][2] =
{
	[FAMILY_XBOX] = {
		{ "LS RT", "LS LT" }, { "LS DN", "LS UP" },
		{ "RS RT", "RS LT" }, { "RS DN", "RS UP" },
		{ "LT", "LT-" }, { "RT", "RT-" },
	},
	[FAMILY_PLAYSTATION] = {
		{ "LS RT", "LS LT" }, { "LS DN", "LS UP" },
		{ "RS RT", "RS LT" }, { "RS DN", "RS UP" },
		{ "L2", "L2-" }, { "R2", "R2-" },
	},
	[FAMILY_NINTENDO] = {
		{ "LS RT", "LS LT" }, { "LS DN", "LS UP" },
		{ "RS RT", "RS LT" }, { "RS DN", "RS UP" },
		{ "ZL", "ZL-" }, { "ZR", "ZR-" },
	},
	[FAMILY_STEAM_DECK] = {
		{ "LS RT", "LS LT" }, { "LS DN", "LS UP" },
		{ "RS RT", "RS LT" }, { "RS DN", "RS UP" },
		{ "L2", "L2-" }, { "R2", "R2-" },
	},
};

/* The same names as icons (padglyph.c), for families that have them; 0 keeps
 * the text label. */
#define DPAD_ICONS PADGLYPH_DPAD_UP, PADGLYPH_DPAD_DOWN, PADGLYPH_DPAD_LEFT, PADGLYPH_DPAD_RIGHT

static const Uint8 pad_button_icons[FAMILY_COUNT][COUNTOF(pad_button_codes)] =
{
	[FAMILY_XBOX] = {
		PADGLYPH_XBOX_A, PADGLYPH_XBOX_B, PADGLYPH_XBOX_X, PADGLYPH_XBOX_Y,
		PADGLYPH_XBOX_VIEW, PADGLYPH_XBOX_GUIDE, PADGLYPH_XBOX_MENU,
		PADGLYPH_XBOX_LS, PADGLYPH_XBOX_RS,
		PADGLYPH_XBOX_LB, PADGLYPH_XBOX_RB,
		DPAD_ICONS,
	},
	[FAMILY_PLAYSTATION] = {
		PADGLYPH_PS_CROSS, PADGLYPH_PS_CIRCLE, PADGLYPH_PS_SQUARE, PADGLYPH_PS_TRIANGLE,
		PADGLYPH_PS_CREATE, PADGLYPH_PS_HOME, PADGLYPH_PS_OPTIONS,
		PADGLYPH_PS_L3, PADGLYPH_PS_R3,
		PADGLYPH_PS_L1, PADGLYPH_PS_R1,
		DPAD_ICONS,
	},
	[FAMILY_NINTENDO] = {
		// positional, like the labels: bottom B, right A, left Y, top X
		PADGLYPH_XBOX_B, PADGLYPH_XBOX_A, PADGLYPH_XBOX_Y, PADGLYPH_XBOX_X,
		PADGLYPH_NINTENDO_MINUS, PADGLYPH_PS_HOME, PADGLYPH_NINTENDO_PLUS,
		PADGLYPH_XBOX_LS, PADGLYPH_XBOX_RS,
		PADGLYPH_NINTENDO_L, PADGLYPH_NINTENDO_R,
		DPAD_ICONS,
		PADGLYPH_NINTENDO_CAPTURE,
	},
	[FAMILY_STEAM_DECK] = {
		PADGLYPH_XBOX_A, PADGLYPH_XBOX_B, PADGLYPH_XBOX_X, PADGLYPH_XBOX_Y,
		PADGLYPH_XBOX_VIEW, 0, PADGLYPH_XBOX_MENU,
		PADGLYPH_PS_L3, PADGLYPH_PS_R3,
		PADGLYPH_PS_L1, PADGLYPH_PS_R1,
		DPAD_ICONS,
	},
};

// sticks are a stick icon and an arrow; [family][axis][negative]
#define STICK_ICONS \
	{ { PADGLYPH_STICK_LEFT, PADGLYPH_ARROW_RIGHT }, { PADGLYPH_STICK_LEFT, PADGLYPH_ARROW_LEFT } }, \
	{ { PADGLYPH_STICK_LEFT, PADGLYPH_ARROW_DOWN }, { PADGLYPH_STICK_LEFT, PADGLYPH_ARROW_UP } }, \
	{ { PADGLYPH_STICK_RIGHT, PADGLYPH_ARROW_RIGHT }, { PADGLYPH_STICK_RIGHT, PADGLYPH_ARROW_LEFT } }, \
	{ { PADGLYPH_STICK_RIGHT, PADGLYPH_ARROW_DOWN }, { PADGLYPH_STICK_RIGHT, PADGLYPH_ARROW_UP } }

static const Uint8 pad_axis_icons[FAMILY_COUNT][COUNTOF(pad_axis_codes)][2][2] =
{
	[FAMILY_XBOX] = {
		STICK_ICONS,
		{ { PADGLYPH_XBOX_LT }, { 0 } }, { { PADGLYPH_XBOX_RT }, { 0 } },
	},
	[FAMILY_PLAYSTATION] = {
		STICK_ICONS,
		{ { PADGLYPH_PS_L2 }, { 0 } }, { { PADGLYPH_PS_R2 }, { 0 } },
	},
	[FAMILY_NINTENDO] = {
		STICK_ICONS,
		{ { PADGLYPH_NINTENDO_ZL }, { 0 } }, { { PADGLYPH_NINTENDO_ZR }, { 0 } },
	},
	[FAMILY_STEAM_DECK] = {
		STICK_ICONS,
		{ { PADGLYPH_PS_L2 }, { 0 } }, { { PADGLYPH_PS_R2 }, { 0 } },
	},
};

// the Steam Deck's own controls, or Steam's virtual pad for them in Game Mode
static bool is_steam_deck(const Joystick *js)
{
	const char *name = SDL_GameControllerName(js->controller);
	if (name != NULL && SDL_strstr(name, "Steam Deck") != NULL)
		return true;

	const char *deck = SDL_getenv("SteamDeck");  // set by SteamOS on the Deck
	return deck != NULL && SDL_strcmp(deck, "1") == 0 &&
	       SDL_GameControllerGetVendor(js->controller) == 0x28de;  // Valve
}

static Pad_family pad_family(const Joystick *js)
{
	if (js->controller == NULL)
		return FAMILY_XBOX;

	if (is_steam_deck(js))
		return FAMILY_STEAM_DECK;

	switch (SDL_GameControllerGetType(js->controller))
	{
	case SDL_CONTROLLER_TYPE_PS3:
	case SDL_CONTROLLER_TYPE_PS4:
#if SDL_VERSION_ATLEAST(2, 0, 14)
	case SDL_CONTROLLER_TYPE_PS5:
#endif
		return FAMILY_PLAYSTATION;

	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO:
#if SDL_VERSION_ATLEAST(2, 24, 0)
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
#endif
		return FAMILY_NINTENDO;

	default:
		return FAMILY_XBOX;  // Xbox, Steam Controller, generic XInput
	}
}

Pad_family joystick_family(int j)
{
	assert(j < joysticks);

	return j >= 0 ? pad_family(&joystick[j]) : FAMILY_XBOX;
}

const char *joystick_display_name(int j)
{
	assert(j < joysticks);

	const char *name = NULL;

	if (joystick[j].controller != NULL)
		name = SDL_GameControllerName(joystick[j].controller);
	if (name == NULL && joystick[j].handle != NULL)
		name = SDL_JoystickName(joystick[j].handle);

	if (name == NULL)
		name = joystick[j].instance_id == -1 ? "Disconnected" : "Unknown Controller";

	return name;
}

// eliminates axis movement below the threshold
int joystick_axis_threshold(int j, int value)
{
	assert(j < joysticks);

	bool negative = value < 0;
	if (negative)
		value = -value;

	if (value <= joystick[j].threshold * 1000)
		return 0;

	value -= joystick[j].threshold * 1000;

	return negative ? -value : value;
}

// converts joystick axis to sane Tyrian-usable value (based on sensitivity)
int joystick_axis_reduce(int j, int value)
{
	assert(j < joysticks);

	value = joystick_axis_threshold(j, value);

	if (value == 0)
		return 0;

	return value / (3000 - 200 * joystick[j].sensitivity);
}

// converts analog joystick axes to an angle
// returns false if axes are centered (there is no angle)
bool joystick_analog_angle(int j, float *angle)
{
	assert(j < joysticks);

	float x = joystick_axis_threshold(j, joystick[j].x), y = joystick_axis_threshold(j, joystick[j].y);

	if (x != 0)
	{
		*angle += atanf(-y / x);
		*angle += (x < 0) ? -M_PI_2 : M_PI_2;
		return true;
	}
	else if (y != 0)
	{
		*angle += y < 0 ? M_PI : 0;
		return true;
	}

	return false;
}

/* gives back value 0..joystick_analog_max indicating that one of the assigned
 * buttons has been pressed or that one of the assigned axes/hats has been moved
 * in the assigned direction
 */
static int check_assigned(const Joystick *js, const Joystick_assignment assignment[2])
{
	int result = 0;

	for (int i = 0; i < 2; i++)
	{
		int temp = 0;

		switch (assignment[i].type)
		{
		case NONE:
			continue;

		case AXIS:
			temp = SDL_JoystickGetAxis(js->handle, assignment[i].num);

			if (assignment[i].negative_axis)
				temp = -temp;
			break;

		case BUTTON:
			temp = SDL_JoystickGetButton(js->handle, assignment[i].num) == 1 ? joystick_analog_max : 0;
			break;

		case HAT:
			temp = SDL_JoystickGetHat(js->handle, assignment[i].num);

			if (assignment[i].x_axis)
				temp &= SDL_HAT_LEFT | SDL_HAT_RIGHT;
			else
				temp &= SDL_HAT_UP | SDL_HAT_DOWN;

			if (assignment[i].negative_axis)
				temp &= SDL_HAT_LEFT | SDL_HAT_UP;
			else
				temp &= SDL_HAT_RIGHT | SDL_HAT_DOWN;

			temp = temp ? joystick_analog_max : 0;
			break;

		case PAD_BUTTON:
			if (js->controller == NULL)
				continue;
			temp = SDL_GameControllerGetButton(js->controller, (SDL_GameControllerButton)assignment[i].num) ? joystick_analog_max : 0;
			break;

		case PAD_AXIS:
			if (js->controller == NULL)
				continue;
			temp = SDL_GameControllerGetAxis(js->controller, (SDL_GameControllerAxis)assignment[i].num);

			if (assignment[i].negative_axis)
				temp = -temp;
			break;
		}

		if (temp > joystick_analog_max)
			temp = joystick_analog_max;

		if (temp > result)
			result = temp;
	}

	return result;
}

// updates joystick state
void poll_joystick(int j)
{
	assert(j < joysticks);

	if (joystick[j].handle == NULL)
		return;

	SDL_JoystickUpdate();

	// indicates that a direction/action was pressed since last poll
	joystick[j].input_pressed = false;

	// indicates that an direction/action has been held long enough to fake a repeat press
	bool repeat = joystick[j].joystick_delay < SDL_GetTicks();

	// update direction state
	for (uint d = 0; d < COUNTOF(joystick[j].direction); d++)
	{
		bool old = joystick[j].direction[d];

		joystick[j].analog_direction[d] = check_assigned(&joystick[j], joystick[j].assignment[d]);
		joystick[j].direction[d] = joystick[j].analog_direction[d] > (joystick_analog_max / 2);
		joydown |= joystick[j].direction[d];

		joystick[j].direction_pressed[d] = joystick[j].direction[d] && (!old || repeat);
		joystick[j].input_pressed |= joystick[j].direction_pressed[d];
	}

	joystick[j].x = -joystick[j].analog_direction[3] + joystick[j].analog_direction[1];
	joystick[j].y = -joystick[j].analog_direction[0] + joystick[j].analog_direction[2];

	// update action state
	for (uint d = 0; d < COUNTOF(joystick[j].action); d++)
	{
		bool old = joystick[j].action[d];

		joystick[j].action[d] = check_assigned(&joystick[j], joystick[j].assignment[d + COUNTOF(joystick[j].direction)]) > (joystick_analog_max / 2);
		joydown |= joystick[j].action[d];

		joystick[j].action_pressed[d] = joystick[j].action[d] && (!old || repeat);
		joystick[j].input_pressed |= joystick[j].action_pressed[d];
	}

	if (joystick[j].controller != NULL)
	{
		// Gamepads navigate menus the way every modern game does, regardless of the
		// in-game assignments: A/Start confirm, B/Back cancel.
		SDL_GameController *pad = joystick[j].controller;

		bool old_confirm = joystick[j].confirm,
		     old_cancel = joystick[j].cancel;

		joystick[j].confirm = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) ||
		                      SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START);
		joystick[j].cancel = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B) ||
		                     SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK);

		joydown |= joystick[j].confirm || joystick[j].cancel;
		joystick[j].input_pressed |= joystick[j].confirm && (!old_confirm || repeat);
		joystick[j].input_pressed |= joystick[j].cancel && (!old_cancel || repeat);
	}
	else
	{
		joystick[j].confirm = joystick[j].action[0] || joystick[j].action[4];
		joystick[j].cancel = joystick[j].action[1] || joystick[j].action[5];
	}

	// if new input, reset press-repeat delay
	if (joystick[j].input_pressed)
		joystick[j].joystick_delay = SDL_GetTicks() + joystick_repeat_delay;
}

// updates all joystick states
void poll_joysticks(void)
{
	joydown = false;

	check_joystick_hotplug();

	for (int j = 0; j < joysticks; j++)
		poll_joystick(j);
}

// sends SDL KEYDOWN and KEYUP events for a key
void push_key(SDL_Scancode key)
{
	SDL_Event e;

	memset(&e.key.keysym, 0, sizeof(e.key.keysym));

	e.key.keysym.scancode = key;
	e.key.state = SDL_RELEASED;

	e.type = SDL_KEYDOWN;
	SDL_PushEvent(&e);

	e.type = SDL_KEYUP;
	SDL_PushEvent(&e);
}

// helps us be lazy by pretending joysticks are a keyboard (useful for menus)
void push_joysticks_as_keyboard(void)
{
	const SDL_Scancode confirm = SDL_SCANCODE_RETURN, cancel = SDL_SCANCODE_ESCAPE;
	const SDL_Scancode direction[4] = { SDL_SCANCODE_UP, SDL_SCANCODE_RIGHT, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT };

	poll_joysticks();

	for (int j = 0; j < joysticks; j++)
	{
		if (!joystick[j].input_pressed)
			continue;

		if (joystick[j].confirm)
			push_key(confirm);
		if (joystick[j].cancel)
			push_key(cancel);

		for (uint d = 0; d < COUNTOF(joystick[j].direction_pressed); d++)
		{
			if (joystick[j].direction_pressed[d])
				push_key(direction[d]);
		}
	}
}

// loads optional community mappings (gamecontrollerdb.txt) for unusual controllers
static void load_controller_mappings(File file, const char *where)
{
	if (file.error)
		return;

	long length = fileGetLength(&file);
	if (length > 0 && length < 8 * 1024 * 1024)
	{
		char *data = malloc(length);
		if (data != NULL)
		{
			fileReadExactly(&file, data, length);
			if (!file.error)
			{
				int added = SDL_GameControllerAddMappingsFromRW(SDL_RWFromConstMem(data, length), 1);
				if (added > 0)
					logInfo("Loaded %d controller mappings from the %s directory.", added, where);
			}
			free(data);
		}
	}

	fileClose(&file);
}

// marks slot j as free (no controller in it)
static void clear_joystick_slot(int j)
{
	memset(&joystick[j], 0, sizeof(joystick[j]));
	joystick[j].instance_id = -1;
}

// makes room for at least count slots; new slots are free
static void grow_joysticks(int count)
{
	if (count <= joysticks)
		return;

	Joystick *grown = realloc(joystick, (size_t)count * sizeof(*joystick));
	if (grown == NULL)
		exit(EXIT_FAILURE);  // out of memory
	joystick = grown;

	for (int j = joysticks; j < count; j++)
		clear_joystick_slot(j);
	joysticks = count;
}

// opens SDL device index device into slot j and loads (or creates) its assignments
static void open_joystick(int j, int device)
{
	clear_joystick_slot(j);
	joystick[j].instance_id = SDL_JoystickGetDeviceInstanceID(device);

	if (SDL_IsGameController(device))
	{
		joystick[j].controller = SDL_GameControllerOpen(device);
		if (joystick[j].controller != NULL)
			joystick[j].handle = SDL_GameControllerGetJoystick(joystick[j].controller);
	}

	if (joystick[j].handle == NULL)
		joystick[j].handle = SDL_JoystickOpen(device);

	if (joystick[j].handle == NULL)
	{
		logWarn("Failed to open controller %d: %s", j, SDL_GetError());
		return;  // the slot stays taken, so it is not retried until reconnected
	}

	logInfo("Controller %d: vendor %04x, product %04x, type %d, rumble %s",
		j,
		SDL_JoystickGetVendor(joystick[j].handle),
		SDL_JoystickGetProduct(joystick[j].handle),
		(int)SDL_JoystickGetType(joystick[j].handle),
		SDL_JoystickHasRumble(joystick[j].handle) ? "yes" : "no");

	if (joystick[j].controller != NULL)
	{
		logInfo("Gamepad detected: %s", joystick_display_name(j));
	}
	else
	{
		logInfo("Joystick detected: %s (%d axes, %d buttons, %d hats)",
			joystick_display_name(j),
			SDL_JoystickNumAxes(joystick[j].handle),
			SDL_JoystickNumButtons(joystick[j].handle),
			SDL_JoystickNumHats(joystick[j].handle));
	}

	if (!load_joystick_assignments(&opentyrian_config, j))
		reset_joystick_assignments(j);

	// block "press" events from buttons that were already held when connected
	joystick[j].joystick_delay = SDL_GetTicks() + joystick_repeat_delay;
}

// closes the controller in slot j (if any) and frees the slot
static void close_joystick(int j, bool save)
{
	if (joystick[j].handle != NULL)
	{
		if (save)
			save_joystick_assignments(&opentyrian_config, j);

		if (joystick[j].controller != NULL)
			SDL_GameControllerClose(joystick[j].controller);  // also closes handle
		else
			SDL_JoystickClose(joystick[j].handle);
	}

	clear_joystick_slot(j);
}

// opens every connected device
static void open_joysticks(void)
{
	joysticks = 0;
	joystick = NULL;

	const int count = SDL_NumJoysticks();
	grow_joysticks(count > 0 ? count : 0);

	for (int j = 0; j < joysticks; j++)
		open_joystick(j, j);
}

static void close_joysticks(bool save)
{
	for (int j = 0; j < joysticks; j++)
		close_joystick(j, save);

	free(joystick);
	joystick = NULL;
	joysticks = 0;
}

// set by check_joystick_hotplug() when a controller goes away
static bool joystick_lost = false;

// returns true (once) when a controller went away since the last call
bool joystick_take_lost(void)
{
	bool lost = joystick_lost;
	joystick_lost = false;
	return lost;
}

// SDL device index of the device with this instance id, or -1 if it is gone
static int joystick_device_index(SDL_JoystickID instance_id)
{
	const int count = SDL_NumJoysticks();
	for (int d = 0; d < count; d++)
		if (SDL_JoystickGetDeviceInstanceID(d) == instance_id)
			return d;

	return -1;
}

// slot holding the device with this instance id, or -1 if none
static int joystick_slot(SDL_JoystickID instance_id)
{
	for (int j = 0; j < joysticks; j++)
		if (joystick[j].instance_id == instance_id)
			return j;

	return -1;
}

/*
 * Closes controllers that were disconnected and opens new ones.  Controllers
 * that stay connected keep their slot (their index in joystick[]), so in a
 * two-player game each player keeps their controller; a new controller takes
 * the first free slot, so one that is reconnected goes back to its player.
 * The joystick array can be moved, so pointers into it must not be kept
 * across a call to this (or to poll_joysticks()).
 */
void check_joystick_hotplug(void)
{
	if (ignore_joystick)
		return;

	Uint32 now = SDL_GetTicks();
	if (!SDL_TICKS_PASSED(now, next_hotplug_check))
		return;
	next_hotplug_check = now + hotplug_check_interval;

	SDL_JoystickUpdate();  // lets SDL notice added/removed devices

	bool changed = false;
	bool lost = false;

	for (int j = 0; j < joysticks; j++)
	{
		if (joystick[j].instance_id == -1)
			continue;

		if (joystick_device_index(joystick[j].instance_id) < 0 ||
		    (joystick[j].handle != NULL && !SDL_JoystickGetAttached(joystick[j].handle)))
		{
			logInfo("Controller %d disconnected: %s", j, joystick_display_name(j));
			close_joystick(j, true);
			changed = lost = true;
		}
	}

	const int count = SDL_NumJoysticks();
	for (int d = 0; d < count; d++)
	{
		const SDL_JoystickID instance_id = SDL_JoystickGetDeviceInstanceID(d);
		if (instance_id < 0 || joystick_slot(instance_id) >= 0)
			continue;

		int j = joystick_slot(-1);
		if (j < 0)
		{
			j = joysticks;
			grow_joysticks(joysticks + 1);
		}
		open_joystick(j, d);
		changed = true;
	}

	if (!changed)
		return;

	// free slots at the end are not needed
	while (joysticks > 0 && joystick[joysticks - 1].instance_id == -1)
		--joysticks;
	if (joysticks == 0)
	{
		free(joystick);
		joystick = NULL;
	}

	joystick_lost |= lost;

	if (joysticks == 0)
		logInfo("No controllers connected.");
}

// initializes SDL joystick system and loads assignments for joysticks found
void init_joysticks(void)
{
	if (ignore_joystick)
		return;

	// Keep the face buttons positional on Nintendo controllers (bottom = "A" slot),
	// so the default layout feels the same on every controller.
	SDL_SetHint(SDL_HINT_GAMECONTROLLER_USE_BUTTON_LABELS, "0");
	// Keep reading the controller if the window loses focus for a moment (overlays etc.)
	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");

	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0)
	{
		logWarn("Failed to initialize SDL game controller: %s", SDL_GetError());
		ignore_joystick = true;
		return;
	}

	SDL_JoystickEventState(SDL_IGNORE);
	SDL_GameControllerEventState(SDL_IGNORE);

	// optional community mappings for unusual controllers
	load_controller_mappings(dataFileOpen("gamecontrollerdb.txt", "rb"), "data");
	load_controller_mappings(userFileOpen("gamecontrollerdb.txt", "rb"), "user");

	open_joysticks();
	next_hotplug_check = SDL_GetTicks() + hotplug_check_interval;

	if (joysticks == 0)
		logInfo("No joysticks detected.");
}

// deinitializes SDL joystick system and saves joystick assignments
void deinit_joysticks(void)
{
	if (ignore_joystick)
		return;

	close_joysticks(true);

	SDL_QuitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
}

void joystick_rumble(int j, Uint16 low, Uint16 high, Uint32 duration_ms)
{
	if (!rumble_enabled || j < 0 || j >= joysticks || joystick[j].handle == NULL)
		return;

	if (SDL_JoystickRumble(joystick[j].handle, low, high, duration_ms) != 0 && !joystick[j].rumble_failed)
	{
		// only report the first failure per controller to keep the log short
		logWarn("Rumble failed on %s: %s", joystick_display_name(j), SDL_GetError());
		joystick[j].rumble_failed = true;
	}
}

void joysticks_rumble_all(Uint16 low, Uint16 high, Uint32 duration_ms)
{
	for (int j = 0; j < joysticks; j++)
		joystick_rumble(j, low, high, duration_ms);
}

static void set_assignment(Joystick_assignment *a, Joystick_assignment_types type, int num, bool negative)
{
	a->type = type;
	a->num = num;
	a->x_axis = false;
	a->negative_axis = negative;
}

// the default layout of a standardized gamepad (also shown by the controller map)
void joystick_default_gamepad_assignments(Joystick_assignment as[10][2])
{
	for (uint a = 0; a < 10; a++)
		for (uint i = 0; i < 2; i++)
			as[a][i].type = NONE;

	// directions: left stick + d-pad
	set_assignment(&as[0][0], PAD_AXIS, SDL_CONTROLLER_AXIS_LEFTY, true);
	set_assignment(&as[0][1], PAD_BUTTON, SDL_CONTROLLER_BUTTON_DPAD_UP, false);
	set_assignment(&as[1][0], PAD_AXIS, SDL_CONTROLLER_AXIS_LEFTX, false);
	set_assignment(&as[1][1], PAD_BUTTON, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, false);
	set_assignment(&as[2][0], PAD_AXIS, SDL_CONTROLLER_AXIS_LEFTY, false);
	set_assignment(&as[2][1], PAD_BUTTON, SDL_CONTROLLER_BUTTON_DPAD_DOWN, false);
	set_assignment(&as[3][0], PAD_AXIS, SDL_CONTROLLER_AXIS_LEFTX, true);
	set_assignment(&as[3][1], PAD_BUTTON, SDL_CONTROLLER_BUTTON_DPAD_LEFT, false);

	// fire: A or right trigger
	set_assignment(&as[4][0], PAD_BUTTON, SDL_CONTROLLER_BUTTON_A, false);
	set_assignment(&as[4][1], PAD_AXIS, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, false);
	// change rear weapon mode: Y
	set_assignment(&as[5][0], PAD_BUTTON, SDL_CONTROLLER_BUTTON_Y, false);
	// left sidekick: LB or left trigger
	set_assignment(&as[6][0], PAD_BUTTON, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, false);
	set_assignment(&as[6][1], PAD_AXIS, SDL_CONTROLLER_AXIS_TRIGGERLEFT, false);
	// right sidekick: RB or X
	set_assignment(&as[7][0], PAD_BUTTON, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, false);
	set_assignment(&as[7][1], PAD_BUTTON, SDL_CONTROLLER_BUTTON_X, false);
	// in-game menu: Start
	set_assignment(&as[8][0], PAD_BUTTON, SDL_CONTROLLER_BUTTON_START, false);
	// pause: Back/View
	set_assignment(&as[9][0], PAD_BUTTON, SDL_CONTROLLER_BUTTON_BACK, false);
}

void reset_joystick_assignments(int j)
{
	assert(j < joysticks);

	// clear assignments
	for (uint a = 0; a < COUNTOF(joystick[j].assignment); a++)
		for (uint i = 0; i < COUNTOF(joystick[j].assignment[a]); i++)
			joystick[j].assignment[a][i].type = NONE;

	if (joystick[j].controller != NULL)
	{
		joystick_default_gamepad_assignments(joystick[j].assignment);

		// analog stick gives proportional movement (and aims the Link gun)
		joystick[j].analog = true;
		joystick[j].sensitivity = 5;
		joystick[j].threshold = 4;
		return;
	}

	// raw joystick defaults: first 2 axes, first hat, first 6 buttons
	for (uint a = 0; a < COUNTOF(joystick[j].assignment); a++)
	{
		if (a < 4)
		{
			if (SDL_JoystickNumAxes(joystick[j].handle) >= 2)
			{
				joystick[j].assignment[a][0].type = AXIS;
				joystick[j].assignment[a][0].num = (a + 1) % 2;
				joystick[j].assignment[a][0].negative_axis = (a == 0 || a == 3);
			}

			if (SDL_JoystickNumHats(joystick[j].handle) >= 1)
			{
				joystick[j].assignment[a][1].type = HAT;
				joystick[j].assignment[a][1].num = 0;
				joystick[j].assignment[a][1].x_axis = (a == 1 || a == 3);
				joystick[j].assignment[a][1].negative_axis = (a == 0 || a == 3);
			}
		}
		else
		{
			if (a - 4 < (unsigned)SDL_JoystickNumButtons(joystick[j].handle))
			{
				joystick[j].assignment[a][0].type = BUTTON;
				joystick[j].assignment[a][0].num = a - 4;
			}
		}
	}

	joystick[j].analog = false;
	joystick[j].sensitivity = 5;
	joystick[j].threshold = 5;
}

static const char* const assignment_names[] =
{
	"up",
	"right",
	"down",
	"left",
	"fire",
	"change fire",
	"left sidekick",
	"right sidekick",
	"menu",
	"pause",
};

// gamepads get their own config section, so old raw-button configs don't clash
static const char *config_section_type(int j)
{
	return joystick[j].controller != NULL ? "gamepad" : "joystick";
}

bool load_joystick_assignments(Config *config, int j)
{
	ConfigSection *section = config_find_section(config, config_section_type(j), joystick_display_name(j));
	if (section == NULL)
		return false;

	if (!config_get_bool_option(section, "analog", &joystick[j].analog))
		joystick[j].analog = false;

	// same range as the menu; out of range values would divide by zero or
	// invert the stick (joystick_axis_reduce)
	const int sensitivity = config_get_or_set_int_option(section, "sensitivity", 5);
	joystick[j].sensitivity = sensitivity < 0 ? 0 : sensitivity > 10 ? 10 : sensitivity;

	const int threshold = config_get_or_set_int_option(section, "threshold", 5);
	joystick[j].threshold = threshold < 0 ? 0 : threshold > 10 ? 10 : threshold;

	for (size_t a = 0; a < COUNTOF(assignment_names); ++a)
	{
		for (unsigned int i = 0; i < COUNTOF(joystick[j].assignment[a]); ++i)
			joystick[j].assignment[a][i].type = NONE;

		ConfigOption *option = config_get_option(section, assignment_names[a]);
		if (option == NULL)
			continue;

		foreach_option_i_value(i, value, option)
		{
			if (i >= COUNTOF(joystick[j].assignment[a]))
				break;

			code_to_assignment(&joystick[j].assignment[a][i], value);
		}
	}

	return true;
}

bool save_joystick_assignments(Config *config, int j)
{
	ConfigSection *section = config_find_or_add_section(config, config_section_type(j), joystick_display_name(j));
	if (section == NULL)
		exit(EXIT_FAILURE);  // out of memory

	config_set_bool_option(section, "analog", joystick[j].analog, NO_YES);

	config_set_int_option(section, "sensitivity", joystick[j].sensitivity);

	config_set_int_option(section, "threshold", joystick[j].threshold);

	for (size_t a = 0; a < COUNTOF(assignment_names); ++a)
	{
		ConfigOption *option = config_set_option(section, assignment_names[a], NULL);
		if (option == NULL)
			exit(EXIT_FAILURE);  // out of memory

		option = config_set_value(option, NULL);
		if (option == NULL)
			exit(EXIT_FAILURE);  // out of memory

		for (size_t i = 0; i < COUNTOF(joystick[j].assignment[a]); ++i)
		{
			if (joystick[j].assignment[a][i].type == NONE)
				continue;

			option = config_add_value(option, assignment_to_code(&joystick[j].assignment[a][i]));
			if (option == NULL)
				exit(EXIT_FAILURE);  // out of memory
		}
	}

	return true;
}

static void assignments_to_string(int j, char *buffer, size_t buffer_len, const Joystick_assignment *assignments, bool icons)
{
	strncpy(buffer, "", buffer_len);

	bool comma = false;
	for (uint i = 0; i < COUNTOF(*joystick->assignment); ++i)
	{
		if (assignments[i].type == NONE)
			continue;

		size_t len = snprintf(buffer, buffer_len, "%s%s",
		                      comma ? ", " : "",
		                      icons ? assignment_to_icon(j, &assignments[i]) : assignment_to_label(j, &assignments[i]));
		if (len >= buffer_len)
			break;
		buffer += len;
		buffer_len -= len;

		comma = true;
	}
}

// fills buffer with comma separated list of assigned joystick functions
void joystick_assignments_to_string(int j, char *buffer, size_t buffer_len, const Joystick_assignment *assignments)
{
	assignments_to_string(j, buffer, buffer_len, assignments, false);
}

// like joystick_assignments_to_string(), with the button icons
void joystick_assignments_to_icons(int j, char *buffer, size_t buffer_len, const Joystick_assignment *assignments)
{
	assignments_to_string(j, buffer, buffer_len, assignments, true);
}

// reverse of assignment_to_code()
static void code_to_assignment(Joystick_assignment *assignment, const char *buffer)
{
	memset(assignment, 0, sizeof(*assignment));

	while (isspace((unsigned char)*buffer))
		++buffer;

	// standardized gamepad codes
	for (size_t b = 0; b < COUNTOF(pad_button_codes); ++b)
	{
		if (b < SDL_CONTROLLER_BUTTON_MAX && strcmp(buffer, pad_button_codes[b]) == 0)
		{
			set_assignment(assignment, PAD_BUTTON, (int)b, false);
			return;
		}
	}
	for (size_t a = 0; a < COUNTOF(pad_axis_codes); ++a)
	{
		for (int neg = 0; neg < 2; ++neg)
		{
			if (strcmp(buffer, pad_axis_codes[a][neg]) == 0)
			{
				set_assignment(assignment, PAD_AXIS, (int)a, neg);
				return;
			}
		}
	}

	// raw joystick codes
	char axis = 0, direction = 0;

	if (sscanf(buffer, "AX %d%c", &assignment->num, &direction) == 2)
		assignment->type = AXIS;
	else if (sscanf(buffer, "BTN %d", &assignment->num) == 1)
		assignment->type = BUTTON;
	else if (sscanf(buffer, "H %d%c%c", &assignment->num, &axis, &direction) == 3)
		assignment->type = HAT;

	if (assignment->num <= 0)  // codes count from 1; anything else is not valid
	{
		assignment->type = NONE;
		assignment->num = 0;
	}
	else
		--assignment->num;

	assignment->x_axis = (toupper((unsigned char)axis) == 'X');
	assignment->negative_axis = (toupper((unsigned char)direction) == '-');
}

/* gives the short (6 or less characters) identifier for a joystick assignment
 *
 * two of these per direction/action is all that can fit on the joystick config screen,
 * assuming two digits for the axis/button/hat number
 */
static const char *assignment_to_code(const Joystick_assignment *assignment)
{
	static char name[7];

	switch (assignment->type)
	{
	case NONE:
		strcpy(name, "");
		break;

	case AXIS:
		snprintf(name, sizeof(name), "AX %d%c",
		         assignment->num + 1,
		         assignment->negative_axis ? '-' : '+');
		break;

	case BUTTON:
		snprintf(name, sizeof(name), "BTN %d",
		         assignment->num + 1);
		break;

	case HAT:
		snprintf(name, sizeof(name), "H %d%c%c",
		         assignment->num + 1,
		         assignment->x_axis ? 'X' : 'Y',
		         assignment->negative_axis ? '-' : '+');
		break;

	case PAD_BUTTON:
		if (assignment->num >= 0 && (size_t)assignment->num < COUNTOF(pad_button_codes))
			snprintf(name, sizeof(name), "%s", pad_button_codes[assignment->num]);
		else
			strcpy(name, "");
		break;

	case PAD_AXIS:
		if (assignment->num >= 0 && (size_t)assignment->num < COUNTOF(pad_axis_codes))
			snprintf(name, sizeof(name), "%s", pad_axis_codes[assignment->num][assignment->negative_axis ? 1 : 0]);
		else
			strcpy(name, "");
		break;
	}

	return name;
}

// like assignment_to_code(), but uses the labels printed on the player's controller
static const char *assignment_to_label(int j, const Joystick_assignment *assignment)
{
	Pad_family family = j >= 0 ? pad_family(&joystick[j]) : FAMILY_XBOX;

	switch (assignment->type)
	{
	case PAD_BUTTON:
		if (assignment->num >= 0 && (size_t)assignment->num < COUNTOF(pad_button_codes))
			return pad_button_labels[family][assignment->num];
		break;

	case PAD_AXIS:
		if (assignment->num >= 0 && (size_t)assignment->num < COUNTOF(pad_axis_codes))
			return pad_axis_labels[family][assignment->num][assignment->negative_axis ? 1 : 0];
		break;

	default:
		break;
	}

	return assignment_to_code(assignment);
}

// like assignment_to_label(), but with the button icons where there are some
static const char *assignment_to_icon(int j, const Joystick_assignment *assignment)
{
	static char icons[8][3];  // a few can be in use at once (ex. in one snprintf)
	static unsigned int next = 0;

	Pad_family family = j >= 0 ? pad_family(&joystick[j]) : FAMILY_XBOX;
	const unsigned int num = (unsigned int)assignment->num;

	const Uint8 *glyphs = NULL;
	Uint8 glyph[2] = { 0, 0 };

	if (assignment->type == PAD_BUTTON && num < COUNTOF(pad_button_codes))
	{
		glyph[0] = pad_button_icons[family][num];
		glyphs = glyph;
	}
	else if (assignment->type == PAD_AXIS && num < COUNTOF(pad_axis_codes))
	{
		glyphs = pad_axis_icons[family][num][assignment->negative_axis ? 1 : 0];
	}

	if (glyphs == NULL || glyphs[0] == 0)
		return assignment_to_label(j, assignment);

	char *out = icons[next];
	next = (next + 1) % COUNTOF(icons);

	out[0] = (char)glyphs[0];
	out[1] = (char)glyphs[1];
	out[2] = '\0';
	return out;
}

// index of the first connected gamepad (SDL_GameController), or -1 if none
int joystick_first_gamepad(void)
{
	for (int j = 0; j < joysticks; j++)
		if (joystick[j].controller != NULL)
			return j;

	return -1;
}

// the name printed on the controller for a standardized button (ex. "A", "CROSS")
const char *joystick_button_label(int j, SDL_GameControllerButton button)
{
	assert(j < joysticks);

	Joystick_assignment assignment;
	set_assignment(&assignment, PAD_BUTTON, button, false);
	return assignment_to_label(j, &assignment);
}

// the name printed on the controller for one assignment (ex. "A", "RT");
// with j < 0 (no controller), the names of an Xbox-style controller
const char *joystick_assignment_label(int j, const Joystick_assignment *assignment)
{
	assert(j < joysticks);

	return assignment_to_label(j, assignment);
}

// the button's icon (see padglyph.h), or its name if it has none
const char *joystick_button_icon(int j, SDL_GameControllerButton button)
{
	assert(j < joysticks);

	Joystick_assignment assignment;
	set_assignment(&assignment, PAD_BUTTON, button, false);
	return assignment_to_icon(j, &assignment);
}

const char *joystick_assignment_icon(int j, const Joystick_assignment *assignment)
{
	assert(j < joysticks);

	return assignment_to_icon(j, assignment);
}

static void action_label(int j, int action, char *buffer, size_t buffer_len, bool icons)
{
	assert(j < joysticks);
	assert(action >= 0 && (size_t)action < COUNTOF(joystick[j].assignment));

	SDL_strlcpy(buffer, "", buffer_len);

	for (uint i = 0; i < COUNTOF(joystick[j].assignment[action]); ++i)
	{
		const Joystick_assignment *assignment = &joystick[j].assignment[action][i];
		if (assignment->type == NONE)
			continue;

		if (buffer[0] != '\0')
			SDL_strlcat(buffer, "/", buffer_len);
		SDL_strlcat(buffer, icons ? assignment_to_icon(j, assignment) : assignment_to_label(j, assignment), buffer_len);
	}
}

// like joystick_assignments_to_string(), but compact: "A/RT"
void joystick_action_label(int j, int action, char *buffer, size_t buffer_len)
{
	action_label(j, action, buffer, buffer_len, false);
}

// like joystick_action_label(), with the button icons
void joystick_action_icons(int j, int action, char *buffer, size_t buffer_len)
{
	action_label(j, action, buffer, buffer_len, true);
}

// captures gamepad input for configuring assignments (standardized buttons/axes)
static bool detect_gamepad_assignment(int j, Joystick_assignment *assignment)
{
	SDL_GameController *pad = joystick[j].controller;

	Sint16 axis[SDL_CONTROLLER_AXIS_MAX];
	for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX; i++)
		axis[i] = SDL_GameControllerGetAxis(pad, (SDL_GameControllerAxis)i);

	Uint8 button[SDL_CONTROLLER_BUTTON_MAX];
	for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX; i++)
		button[i] = SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)i);

	bool detected = false;

	while (true)
	{
		setFrameCount(1);

		NETWORK_KEEP_ALIVE();

		delayUntilElapsed();

		handleSdlEvents();

		SDL_GameControllerUpdate();

		for (int i = 0; i < SDL_CONTROLLER_AXIS_MAX && !detected; ++i)
		{
			Sint16 temp = SDL_GameControllerGetAxis(pad, (SDL_GameControllerAxis)i);

			if (abs(temp - axis[i]) > joystick_analog_max * 2 / 3)
			{
				set_assignment(assignment, PAD_AXIS, i, temp < axis[i]);
				detected = true;
			}
		}

		for (int i = 0; i < SDL_CONTROLLER_BUTTON_MAX && !detected; ++i)
		{
			Uint8 new_button = SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)i);

			if (new_button == button[i])
				continue;

			if (new_button == 0)  // button was released
			{
				button[i] = new_button;
			}
			else                  // button was pressed
			{
				set_assignment(assignment, PAD_BUTTON, i, false);
				detected = true;
			}
		}

		if (detected || hasInput(INPUT_NO_MOTION) || !SDL_GameControllerGetAttached(pad))
			break;
	}

	return detected;
}

// captures joystick input for configuring assignments
// returns false if non-joystick input was detected
// TODO: input from joystick other than the one being configured probably should not be ignored
bool detect_joystick_assignment(int j, Joystick_assignment *assignment)
{
	if (joystick[j].handle == NULL)
		return false;

	if (joystick[j].controller != NULL)
		return detect_gamepad_assignment(j, assignment);

	// get initial joystick state to compare against to see if anything was pressed

	const int axes = SDL_JoystickNumAxes(joystick[j].handle);
	Sint16 *axis = malloc(axes * sizeof(*axis));
	for (int i = 0; i < axes; i++)
		axis[i] = SDL_JoystickGetAxis(joystick[j].handle, i);

	const int buttons = SDL_JoystickNumButtons(joystick[j].handle);
	Uint8 *button = malloc(buttons * sizeof(*button));
	for (int i = 0; i < buttons; i++)
		button[i] = SDL_JoystickGetButton(joystick[j].handle, i);

	const int hats = SDL_JoystickNumHats(joystick[j].handle);
	Uint8 *hat = malloc(hats * sizeof(*hat));
	for (int i = 0; i < hats; i++)
		hat[i] = SDL_JoystickGetHat(joystick[j].handle, i);

	bool detected = false;

	while (true)
	{
		setFrameCount(1);

		NETWORK_KEEP_ALIVE();

		delayUntilElapsed();

		handleSdlEvents();

		SDL_JoystickUpdate();

		for (int i = 0; i < axes; ++i)
		{
			Sint16 temp = SDL_JoystickGetAxis(joystick[j].handle, i);

			if (abs(temp - axis[i]) > joystick_analog_max * 2 / 3)
			{
				assignment->type = AXIS;
				assignment->num = i;
				assignment->negative_axis = temp < axis[i];
				detected = true;
				break;
			}
		}

		for (int i = 0; i < buttons; ++i)
		{
			Uint8 new_button = SDL_JoystickGetButton(joystick[j].handle, i),
			      changed = button[i] ^ new_button;

			if (!changed)
				continue;

			if (new_button == 0) // button was released
			{
				button[i] = new_button;
			}
			else                 // button was pressed
			{
				assignment->type = BUTTON;
				assignment->num = i;
				detected = true;
				break;
			}
		}

		for (int i = 0; i < hats; ++i)
		{
			Uint8 new_hat = SDL_JoystickGetHat(joystick[j].handle, i),
			      changed = hat[i] ^ new_hat;

			if (!changed)
				continue;

			if ((new_hat & changed) == SDL_HAT_CENTERED) // hat was centered
			{
				hat[i] = new_hat;
			}
			else
			{
				assignment->type = HAT;
				assignment->num = i;
				assignment->x_axis = changed & (SDL_HAT_LEFT | SDL_HAT_RIGHT);
				assignment->negative_axis = changed & (SDL_HAT_LEFT | SDL_HAT_UP);
				detected = true;
			}
		}

		if (detected || hasInput(INPUT_NO_MOTION))
			break;
	}
	free(axis);
	free(button);
	free(hat);

	return detected;
}

// waits for a press on joystick j and toggles it in the assignments of an action
// (0-3: up, right, down, left; 4-9: fire, mode swap, left fire, right fire, menu, pause)
bool assign_joystick_action(int j, int action)
{
	Joystick_assignment temp;
	if (!detect_joystick_assignment(j, &temp))
		return false;

	Joystick_assignment *const assignment = joystick[j].assignment[action];
	const uint count = COUNTOF(*joystick->assignment);

	// if the detected assignment was already set, unset it
	for (uint i = 0; i < count; i++)
	{
		if (joystick_assignment_cmp(&temp, &assignment[i]))
		{
			assignment[i].type = NONE;
			goto done;
		}
	}

	// if there is an empty assignment, set it
	for (uint i = 0; i < count; i++)
	{
		if (assignment[i].type == NONE)
		{
			assignment[i] = temp;
			goto done;
		}
	}

	// if no assignments are empty, shift them all forward and set the last one
	for (uint i = 0; i < count - 1; i++)
		assignment[i] = assignment[i + 1];
	assignment[count - 1] = temp;

done:
	poll_joysticks();
	return true;
}

// compares relevant parts of joystick assignments for equality
bool joystick_assignment_cmp(const Joystick_assignment *a, const Joystick_assignment *b)
{
	if (a->type == b->type)
	{
		switch (a->type)
		{
		case NONE:
			return true;
		case AXIS:
		case PAD_AXIS:
			return (a->num == b->num) &&
			       (a->negative_axis == b->negative_axis);
		case BUTTON:
		case PAD_BUTTON:
			return (a->num == b->num);
		case HAT:
			return (a->num == b->num) &&
			       (a->x_axis == b->x_axis) &&
			       (a->negative_axis == b->negative_axis);
		}
	}

	return false;
}
