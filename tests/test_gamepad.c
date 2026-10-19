/* Headless test for the gamepad layer: virtual controllers, mapping, hotplug, config. */
#include "joystick.h"
#include "helptext.h"
#include "config.h"
#include "SDL.h"
#include <stdio.h>
#include <string.h>

extern Config opentyrian_config;

static int failures = 0;
#define CHECK(cond) do { if (cond) printf("  ok   %s\n", #cond); else { printf("  FAIL %s  (line %d)\n", #cond, __LINE__); failures++; } } while (0)

static int attach_pad(const char *name)
{
	SDL_VirtualJoystickDesc d;
	SDL_zero(d);
	d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
	d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
	d.naxes = SDL_CONTROLLER_AXIS_MAX;
	d.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
	d.name = name;
	return SDL_JoystickAttachVirtualEx(&d);
}

static void advance(Uint32 ms) { SDL_Delay(ms); }

static void poll(void) { SDL_Delay(1); poll_joysticks(); }

int main(int argc, char *argv[])
{
	(void)argc; (void)argv;
	SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
	SDL_Init(0);

	printf("[1] detection\n");
	int dev0 = attach_pad("Virtual Xbox Pad");
	CHECK(dev0 >= 0);
	init_joysticks();
	CHECK(joysticks == 1);
	CHECK(joystick[0].controller != NULL);
	CHECK(joystick[0].analog == true);

	SDL_Joystick *h = joystick[0].handle;

	printf("[2] default layout\n");
	advance(350);  // let the connect-time repeat guard expire
	SDL_JoystickSetVirtualButton(h, SDL_CONTROLLER_BUTTON_A, 1);
	poll();
	CHECK(joystick[0].action[0]);         // fire
	CHECK(joystick[0].confirm);
	SDL_JoystickSetVirtualButton(h, SDL_CONTROLLER_BUTTON_A, 0);
	poll();
	CHECK(!joystick[0].action[0]);

	SDL_JoystickSetVirtualAxis(h, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 32767);
	poll();
	CHECK(joystick[0].action[0]);         // RT also fires
	SDL_JoystickSetVirtualAxis(h, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 0);

	SDL_JoystickSetVirtualAxis(h, SDL_CONTROLLER_AXIS_LEFTY, -32768);
	poll();
	CHECK(joystick[0].direction[0]);      // up
	CHECK(joystick[0].y < 0);
	SDL_JoystickSetVirtualAxis(h, SDL_CONTROLLER_AXIS_LEFTY, 0);

	SDL_JoystickSetVirtualButton(h, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 1);
	poll();
	CHECK(joystick[0].direction[1]);      // right via d-pad
	SDL_JoystickSetVirtualButton(h, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, 0);

	SDL_JoystickSetVirtualButton(h, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 1);
	SDL_JoystickSetVirtualButton(h, SDL_CONTROLLER_BUTTON_Y, 1);
	poll();
	CHECK(joystick[0].action[2]);         // left sidekick
	CHECK(joystick[0].action[1]);         // change rear mode
	SDL_JoystickSetVirtualButton(h, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, 0);
	SDL_JoystickSetVirtualButton(h, SDL_CONTROLLER_BUTTON_Y, 0);

	SDL_JoystickSetVirtualButton(h, SDL_CONTROLLER_BUTTON_B, 1);
	poll();
	CHECK(joystick[0].cancel);
	CHECK(joystick[0].input_pressed);     // B press reaches menus
	SDL_JoystickSetVirtualButton(h, SDL_CONTROLLER_BUTTON_B, 0);
	SDL_JoystickSetVirtualButton(h, SDL_CONTROLLER_BUTTON_START, 1);
	poll();
	CHECK(joystick[0].action[4]);         // in-game menu
	SDL_JoystickSetVirtualButton(h, SDL_CONTROLLER_BUTTON_START, 0);
	poll();

	printf("[3] on-screen labels\n");
	char buf[30];
	joystick_assignments_to_string(0, buf, sizeof(buf), joystick[0].assignment[4]);
	printf("       fire: '%s'\n", buf);
	CHECK(strcmp(buf, "A, RT") == 0);
	joystick_assignments_to_string(0, buf, sizeof(buf), joystick[0].assignment[0]);
	printf("       up:   '%s'\n", buf);
	CHECK(strcmp(buf, "LS UP, D-UP") == 0);

	printf("[3b] help texts\n");
	CHECK(strcmp(padHelpText("Press ESC to exit."), "Press B to exit.") == 0);
	CHECK(strcmp(padHelpText("Change volume with the left/right arrow keys."), "Change volume with the D-pad left/right.") == 0);
	CHECK(strcmp(padHelpText("Load a saved game.  (Shortcut is ALT-L)"), "Load a saved game.") == 0);
	CHECK(strcmp(padHelpText("[/] Rear Weapon Mode"), "[Y] Rear Weapon Mode") == 0);
	CHECK(strcmp(padHelpText("press Enter or the spacebar."), "press ~A~.") == 0);
	CHECK(strstr(padHelpText("usually fired with separate buttons."), "~LB/LT~ and ~RB/X~") != NULL);
	CHECK(strcmp(padHelpText("Quit the game."), "Quit the game.") == 0);

	printf("[4] config round trip\n");
	save_joystick_assignments(&opentyrian_config, 0);
	Joystick_assignment saved[10][2];
	memcpy(saved, joystick[0].assignment, sizeof(saved));
	memset(joystick[0].assignment, 0, sizeof(saved));
	joystick[0].analog = false;
	CHECK(load_joystick_assignments(&opentyrian_config, 0));
	bool same = true;
	for (int a = 0; a < 10; a++)
		for (int i = 0; i < 2; i++)
			same &= joystick_assignment_cmp(&saved[a][i], &joystick[0].assignment[a][i]);
	CHECK(same);
	CHECK(joystick[0].analog == true);

	printf("[5] hotplug\n");
	int dev1 = attach_pad("Virtual Second Pad");
	CHECK(dev1 >= 0);
	advance(600);
	poll();
	CHECK(joysticks == 2);
	SDL_JoystickDetachVirtual(dev0);
	advance(600);
	poll();
	CHECK(joysticks == 1);
	CHECK(strcmp(joystick_display_name(0), "Virtual Second Pad") == 0);
	SDL_JoystickDetachVirtual(SDL_NumJoysticks() > 0 ? 0 : 0);
	advance(600);
	poll();
	CHECK(joysticks == 0);
	joysticks_rumble_all(0xFFFF, 0xFFFF, 100); // must be harmless with nothing connected
	CHECK(strcmp(padHelpText("Press ESC to exit."), "Press ESC to exit.") == 0);  // keyboard texts again

	deinit_joysticks();
	printf(failures ? "\n%d FAILURE(S)\n" : "\nALL PASSED\n", failures);
	return failures != 0;
}
