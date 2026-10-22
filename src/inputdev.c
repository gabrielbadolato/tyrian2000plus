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
#include "inputdev.h"

#include "config.h"
#include "helptext.h"
#include "joystick.h"
#include "lang.h"
#include "mouse.h"

#include <stdio.h>

bool input_device_available(JE_byte device)
{
	if (device == INPUT_DEVICE_KEYBOARD || device == INPUT_DEVICE_KEYBOARD2)
		return true;
	if (device == INPUT_DEVICE_MOUSE)
		return has_mouse;
	if (input_device_is_joystick(device))
	{
		const int j = device - INPUT_DEVICE_JOYSTICK;
		return j < joysticks && joystick[j].handle != NULL;
	}
	return false;
}

// the devices to choose from, in menu order
static int list_devices(JE_byte *list)
{
	int n = 0;
	list[n++] = INPUT_DEVICE_KEYBOARD;
	list[n++] = INPUT_DEVICE_KEYBOARD2;
	if (has_mouse)
		list[n++] = INPUT_DEVICE_MOUSE;
	for (int j = 0; j < joysticks && n < 32; j++)
		if (joystick[j].handle != NULL)
			list[n++] = INPUT_DEVICE_JOYSTICK + j;
	return n;
}

void input_devices_default(void)
{
	int pads[2], count = 0;
	for (int j = 0; j < joysticks && count < 2; j++)
		if (joystick[j].handle != NULL)
			pads[count++] = j;

	if (count >= 2)
	{
		inputDevice[0] = INPUT_DEVICE_JOYSTICK + pads[0];
		inputDevice[1] = INPUT_DEVICE_JOYSTICK + pads[1];
	}
	else if (count == 1 && running_on_steam_deck())
	{
		// the Deck's own controls; whoever has the keyboard flies the other ship
		inputDevice[0] = INPUT_DEVICE_JOYSTICK + pads[0];
		inputDevice[1] = INPUT_DEVICE_KEYBOARD;
	}
	else if (count == 1)
	{
		inputDevice[0] = INPUT_DEVICE_KEYBOARD;
		inputDevice[1] = INPUT_DEVICE_JOYSTICK + pads[0];
	}
	else
	{
		inputDevice[0] = INPUT_DEVICE_KEYBOARD;
		inputDevice[1] = has_mouse ? INPUT_DEVICE_MOUSE : INPUT_DEVICE_KEYBOARD2;
	}
}

void input_devices_check(void)
{
	for (int p = 0; p < 2; p++)
	{
		if (input_device_available(inputDevice[p]) && inputDevice[p] != inputDevice[1 - p])
			continue;

		// the first free device, but a controller before the keyboards and the mouse
		JE_byte list[32];
		const int n = list_devices(list);
		JE_byte choice = 0;
		for (int i = 0; i < n; i++)
		{
			if (list[i] == inputDevice[1 - p])
				continue;
			if (choice == 0 || (input_device_is_joystick(list[i]) && !input_device_is_joystick(choice)))
				choice = list[i];
		}
		inputDevice[p] = choice != 0 ? choice : INPUT_DEVICE_KEYBOARD;
	}
}

void input_device_step(int p, int dir)
{
	JE_byte list[32];
	const int n = list_devices(list);

	int at = 0;
	for (int i = 0; i < n; i++)
		if (list[i] == inputDevice[p])
			at = i;

	if (n <= 2)
	{
		input_device_take(p, list[(at + n + dir) % n]);
		return;
	}

	do
		at = (at + n + dir) % n;
	while (list[at] == inputDevice[1 - p]);
	inputDevice[p] = list[at];
}

void input_device_take(int p, JE_byte device)
{
	if (inputDevice[1 - p] == device)
		inputDevice[1 - p] = inputDevice[p];
	inputDevice[p] = device;
}

const char *joystick_kind_name(int j)
{
	if (joystick[j].controller == NULL)
		return inputDevices[2];  // "Joystick", from the game's texts

	switch (joystick_family(j))
	{
	case FAMILY_STEAM_DECK:
		return "Steam Deck";
	case FAMILY_PLAYSTATION:
		return "PlayStation";
	case FAMILY_NINTENDO:
		return "Nintendo";
	default:
		break;
	}

	switch (SDL_GameControllerGetType(joystick[j].controller))
	{
	case SDL_CONTROLLER_TYPE_XBOX360:
	case SDL_CONTROLLER_TYPE_XBOXONE:
		return "Xbox";
	default:
		return tr("Controller");
	}
}

void input_device_name(JE_byte device, char *buffer, size_t size)
{
	if (device == INPUT_DEVICE_KEYBOARD2)
		snprintf(buffer, size, "%s", tr("Keyboard 2"));
	else if (input_device_is_joystick(device))
	{
		const int j = device - INPUT_DEVICE_JOYSTICK;
		if (j < joysticks && joystick[j].handle != NULL)
			snprintf(buffer, size, "%d: %s", j + 1, joystick_kind_name(j));
		else
			snprintf(buffer, size, "%d: %s", j + 1, tr("Disconnected"));
	}
	else if (device >= INPUT_DEVICE_KEYBOARD && device <= INPUT_DEVICE_MOUSE)
		snprintf(buffer, size, "%s", inputDevices[device - 1]);
	else
		snprintf(buffer, size, "?");
}

void input_device_feedback(int p)
{
	if (input_device_is_joystick(inputDevice[p]))
		joystick_rumble(inputDevice[p] - INPUT_DEVICE_JOYSTICK, 0x8000, 0x8000, 250);
}
