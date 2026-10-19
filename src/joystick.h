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
#ifndef JOYSTICK_H
#define JOYSTICK_H

#include "config_file.h"

#include "SDL.h"

typedef enum
{
	NONE,
	AXIS,
	BUTTON,
	HAT,
	PAD_BUTTON, // standardized gamepad button (SDL_GameControllerButton)
	PAD_AXIS    // standardized gamepad axis (SDL_GameControllerAxis)
}
Joystick_assignment_types;

typedef struct
{
	Joystick_assignment_types type;
	int num;
	
	// if hat
	bool x_axis; // else y_axis
	
	// if hat, axis or pad axis
	bool negative_axis; // else positive
}
Joystick_assignment;

typedef struct
{
	SDL_Joystick *handle;
	SDL_GameController *controller; // non-NULL when SDL recognizes the device as a gamepad
	SDL_JoystickID instance_id;
	bool rumble_failed; // already logged a rumble error for this controller
	
	Joystick_assignment assignment[10][2]; // 0-3: directions, 4-9: actions
	
	bool analog;
	int sensitivity, threshold;
	
	signed int x, y;
	int analog_direction[4];
	bool direction[4], direction_pressed[4]; // up, right, down, left  (_pressed, for emulating key presses)
	
	bool confirm, cancel;
	bool action[6], action_pressed[6]; // fire, mode swap, left fire, right fire, menu, pause
	
	Uint32 joystick_delay;
	bool input_pressed;
}
Joystick;

extern int joystick_repeat_delay;
extern bool joydown;
extern bool ignore_joystick;
extern int joysticks;
extern Joystick *joystick;

int joystick_axis_reduce(int j, int value);
bool joystick_analog_angle(int j, float *angle);

void poll_joystick(int j);
void poll_joysticks(void);
void check_joystick_hotplug(void);
bool joystick_take_lost(void);
bool joystick_filter_steam_input(void);
bool joystick_steam_filter_enabled(void);
Uint32 joystick_valve_pad_last_change(void);
bool joystick_valve_pad_changed_within(Uint32 ms);
void joystick_note_filtered(bool keyboard);

// windowID of the key events push_key() sends for controller input
#define JOYSTICK_PUSHED_KEY 0xFFFFFFFFu

void push_key(SDL_Scancode key);
void push_joysticks_as_keyboard(void);

void init_joysticks(void);
void deinit_joysticks(void);

void joystick_default_gamepad_assignments(Joystick_assignment as[10][2]);
void reset_joystick_assignments(int j);
bool load_joystick_assignments(Config* config, int j);
bool save_joystick_assignments(Config* config, int j);

void joystick_assignments_to_string(int j, char *buffer, size_t buffer_len, const Joystick_assignment *assignments);

const char *joystick_display_name(int j);
int joystick_first_gamepad(void);
const char *joystick_button_label(int j, SDL_GameControllerButton button);
const char *joystick_assignment_label(int j, const Joystick_assignment *assignment);
void joystick_action_label(int j, int action, char *buffer, size_t buffer_len);
void joystick_rumble(int j, Uint16 low, Uint16 high, Uint32 duration_ms);
void joysticks_rumble_all(Uint16 low, Uint16 high, Uint32 duration_ms);

bool detect_joystick_assignment(int j, Joystick_assignment *assignment);
bool joystick_assignment_cmp(const Joystick_assignment *, const Joystick_assignment *);
bool assign_joystick_action(int j, int action);

#endif /* JOYSTICK_H */
