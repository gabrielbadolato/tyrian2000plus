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
#ifndef INPUTDEV_H
#define INPUTDEV_H

#include "opentyr.h"

/* The device each player uses in a 2-player game (inputDevice[] in config.h):
 * 0 any, 1 keyboard, 2 mouse, 3 + j controller j (its slot in joystick[]),
 * and, new here, the second keyboard keys (keySettings2), which the original
 * game did not have.  A save keeps the device; other versions of the game
 * see an unknown value and choose another device, as for a missing controller. */
#define INPUT_DEVICE_ANY       0
#define INPUT_DEVICE_KEYBOARD  1
#define INPUT_DEVICE_MOUSE     2
#define INPUT_DEVICE_JOYSTICK  3
#define INPUT_DEVICE_KEYBOARD2 255

static inline bool input_device_is_joystick(JE_byte device)
{
	return device >= INPUT_DEVICE_JOYSTICK && device != INPUT_DEVICE_KEYBOARD2;
}

// a device someone can use now (a controller must be connected)
bool input_device_available(JE_byte device);

// sets both players' devices for a new 2-player game from what is connected:
// two controllers, one each; one controller, keyboard and controller (on a
// Steam Deck, whose own controls are the controller, the other way round);
// none, keyboard and mouse, as in the original game
void input_devices_default(void);

// after loading a game or when a controller goes away: a player whose device
// cannot be used gets one that can
void input_devices_check(void);

// the next (dir +1) or previous (-1) device for player p, skipping the other
// player's; with only two devices, the players swap them
void input_device_step(int p, int dir);

// player p takes the device; the other player gets p's old one if it was the same
void input_device_take(int p, JE_byte device);

// a short vibration of player p's controller, so the players see whose it is
void input_device_feedback(int p);

// "Keyboard", "Mouse", "1: Xbox"... for the ship menu (game font)
void input_device_name(JE_byte device, char *buffer, size_t size);

// short name of controller j's kind: "Xbox", "PlayStation", "Nintendo", "Steam Deck", "Controller"
const char *joystick_kind_name(int j);

#endif /* INPUTDEV_H */
