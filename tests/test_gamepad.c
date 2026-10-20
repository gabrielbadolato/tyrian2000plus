#define _POSIX_C_SOURCE 200809L  // mkdtemp, setenv
/* Headless test for the gamepad layer: virtual controllers (generic, Xbox,
 * PlayStation, Switch), mapping, button names, hotplug, config. */
#include "joystick.h"
#include "helptext.h"
#include "padglyph.h"
#include "config.h"
#include "SDL.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Config opentyrian_config;

static int failures = 0;
static char save_path[512];
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

typedef struct
{
	const char *name;
	Uint16 vendor, product;
	SDL_GameControllerType type;
	const char *labels[10];  // joystick_action_label() of each default action
	const char *confirm, *cancel;
	const char *escHelp;     // padHelpText("Press ESC to exit.") without icons
	PadGlyph confirmIcon, cancelIcon;  // 0: this family has no icons yet
}
Pad_model;

static int attach_model(const Pad_model *m)
{
	SDL_VirtualJoystickDesc d;
	SDL_zero(d);
	d.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
	d.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
	d.naxes = SDL_CONTROLLER_AXIS_MAX;
	d.nbuttons = SDL_CONTROLLER_BUTTON_MAX;
	d.vendor_id = m->vendor;
	d.product_id = m->product;
	d.name = m->name;
	return SDL_JoystickAttachVirtualEx(&d);
}

// presses one default input of a connected pad and checks that exactly the
// action it is assigned to (and nothing else) is triggered
static bool press_only(int j, int action, const Joystick_assignment *in)
{
	SDL_Joystick *h = joystick[j].handle;
	Sint16 value = in->negative_axis ? -32768 : 32767;

	if (in->type == PAD_BUTTON)
		SDL_JoystickSetVirtualButton(h, in->num, 1);
	else
		SDL_JoystickSetVirtualAxis(h, in->num, value);
	poll();

	bool ok = true;
	for (int d = 0; d < 4; d++)
		ok &= joystick[j].direction[d] == (d == action);
	for (int a = 0; a < 6; a++)
		ok &= joystick[j].action[a] == (a + 4 == action);
	ok &= joystick[j].confirm == (in->type == PAD_BUTTON &&  // A and Start also confirm in menus
	                             (in->num == SDL_CONTROLLER_BUTTON_A || in->num == SDL_CONTROLLER_BUTTON_START));

	if (in->type == PAD_BUTTON)
		SDL_JoystickSetVirtualButton(h, in->num, 0);
	else
		SDL_JoystickSetVirtualAxis(h, in->num, 0);
	poll();

	for (int d = 0; d < 4; d++)
		ok &= !joystick[j].direction[d];
	for (int a = 0; a < 6; a++)
		ok &= !joystick[j].action[a];

	if (!ok)
		printf("       action %d input type %d num %d\n", action, in->type, in->num);
	return ok;
}

static const Pad_model pad_models[] =
{
	{
		"Xbox Wireless Controller", 0x045e, 0x0b13, SDL_CONTROLLER_TYPE_XBOXONE,
		{ "LS UP/D-UP", "LS RT/D-RGHT", "LS DN/D-DOWN", "LS LT/D-LEFT",
		  "A/RT", "Y", "LB/LT", "RB/X", "MENU", "VIEW" },
		"A", "B", "Press B to exit.", PADGLYPH_XBOX_A, PADGLYPH_XBOX_B,
	},
	{
		"DualSense Wireless Controller", 0x054c, 0x0ce6, SDL_CONTROLLER_TYPE_PS5,
		{ "LS UP/D-UP", "LS RT/D-RGHT", "LS DN/D-DOWN", "LS LT/D-LEFT",
		  "CROSS/R2", "TRI", "L1/L2", "R1/SQUARE", "OPTION", "CREATE" },
		"CROSS", "CIRCLE", "Press CIRCLE to exit.", PADGLYPH_PS_CROSS, PADGLYPH_PS_CIRCLE,
	},
	{
		// positional layout (SDL_GAMECONTROLLER_USE_BUTTON_LABELS=0): the
		// bottom button fires, and on a Switch pad that one is labeled B
		"Nintendo Switch Pro Controller", 0x057e, 0x2009, SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO,
		{ "LS UP/D-UP", "LS RT/D-RGHT", "LS DN/D-DOWN", "LS LT/D-LEFT",
		  "B/ZR", "X", "L/ZL", "R/Y", "+", "-" },
		"B", "A", "Press A to exit.", PADGLYPH_XBOX_B, PADGLYPH_XBOX_A,
	},
	{
		// Valve's vendor id; the name tells the Deck from a Steam Controller
		"Steam Deck", 0x28de, 0x1205, SDL_CONTROLLER_TYPE_UNKNOWN,
		{ "LS UP/D-UP", "LS RT/D-RGHT", "LS DN/D-DOWN", "LS LT/D-LEFT",
		  "A/R2", "Y", "L1/L2", "R1/X", "MENU", "VIEW" },
		"A", "B", "Press B to exit.", PADGLYPH_XBOX_A, PADGLYPH_XBOX_B,
	},
};

int main(int argc, char *argv[])
{
	(void)argc; (void)argv;

	// user files (tyrian.sav) go to a fresh folder
	char user_dir[] = "/tmp/test_gamepad_XXXXXX";
	if (mkdtemp(user_dir) == NULL)
		return 1;
	setenv("XDG_CONFIG_HOME", user_dir, 1);
	snprintf(save_path, sizeof save_path, "%s/opentyrian2000/tyrian.sav", user_dir);

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
	char expected[64];
	joystick_assignments_to_icons(0, buf, sizeof(buf), joystick[0].assignment[4]);
	snprintf(expected, sizeof(expected), "%c, %c", PADGLYPH_XBOX_A, PADGLYPH_XBOX_RT);
	CHECK(strcmp(buf, expected) == 0);
	joystick_assignments_to_icons(0, buf, sizeof(buf), joystick[0].assignment[0]);
	snprintf(expected, sizeof(expected), "%c%c, %c", PADGLYPH_STICK_LEFT, PADGLYPH_ARROW_UP, PADGLYPH_DPAD_UP);
	CHECK(strcmp(buf, expected) == 0);

	printf("[3a] controller map\n");
	Joystick_assignment defaults[10][2];
	joystick_default_gamepad_assignments(defaults);
	bool sameDefaults = true;
	for (int a = 0; a < 10; a++)
		for (int i = 0; i < 2; i++)
			if (defaults[a][i].type != joystick[0].assignment[a][i].type ||
			    (defaults[a][i].type != NONE && !joystick_assignment_cmp(&defaults[a][i], &joystick[0].assignment[a][i])))
				sameDefaults = false;
	CHECK(sameDefaults);  // the map without a controller shows the real default layout
	CHECK(strcmp(joystick_assignment_label(-1, &defaults[4][1]), "RT") == 0);  // no controller: Xbox names
	CHECK(strcmp(joystick_button_label(-1, SDL_CONTROLLER_BUTTON_B), "B") == 0);

	printf("[3b] help texts\n");
	snprintf(expected, sizeof(expected), "Press %c to exit.", PADGLYPH_XBOX_B);
	CHECK(strcmp(padHelpText("Press ESC to exit."), expected) == 0);
	CHECK(strcmp(padHelpText("Change volume with the left/right arrow keys."), "Change volume with the D-pad left/right.") == 0);
	CHECK(strcmp(padHelpText("Load a saved game.  (Shortcut is ALT-L)"), "Load a saved game.") == 0);
	snprintf(expected, sizeof(expected), "[%c] Rear Weapon Mode", PADGLYPH_XBOX_Y);
	CHECK(strcmp(padHelpText("[/] Rear Weapon Mode"), expected) == 0);
	snprintf(expected, sizeof(expected), "press ~%c~.", PADGLYPH_XBOX_A);
	CHECK(strcmp(padHelpText("press Enter or the spacebar."), expected) == 0);
	snprintf(expected, sizeof(expected), "~%c/%c~ and ~%c/%c~", PADGLYPH_XBOX_LB, PADGLYPH_XBOX_LT, PADGLYPH_XBOX_RB, PADGLYPH_XBOX_X);
	CHECK(strstr(padHelpText("usually fired with separate buttons."), expected) != NULL);
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

	printf("[4a] bad values in the config file\n");
	{
		ConfigSection *section = config_find_section(&opentyrian_config, "gamepad", joystick_display_name(0));
		CHECK(section != NULL);
		config_set_int_option(section, "sensitivity", 15);  // would divide by zero
		config_set_int_option(section, "threshold", -4);
		ConfigOption *option = config_set_option(section, "fire", NULL);
		option = config_set_value(option, NULL);
		option = config_add_value(option, "BTN -3");        // negative button number
		CHECK(load_joystick_assignments(&opentyrian_config, 0));
		CHECK(joystick[0].sensitivity == 10);
		CHECK(joystick[0].threshold == 0);
		CHECK(joystick[0].assignment[4][0].type == NONE);
		CHECK(joystick_axis_reduce(0, 32767) != 0);  // no crash, still moves
		reset_joystick_assignments(0);
	}

	printf("[5] hotplug\n");
	int dev1 = attach_pad("Virtual Second Pad");
	CHECK(dev1 >= 0);
	advance(600);
	poll();
	CHECK(joysticks == 2);
	CHECK(!joystick_take_lost());          // connecting does not pause the game
	SDL_Joystick *second = joystick[1].handle;
	joystick[1].assignment[5][0].type = NONE;  // the second pad has a custom layout

	SDL_JoystickDetachVirtual(dev0);
	advance(600);
	poll();
	CHECK(joystick_take_lost());           // disconnecting pauses the game...
	CHECK(!joystick_take_lost());          // ...once
	CHECK(joysticks == 2);                 // each controller keeps its slot (player)
	CHECK(joystick[0].handle == NULL);
	CHECK(strcmp(joystick_display_name(0), "Disconnected") == 0);
	CHECK(joystick[1].handle == second);
	CHECK(strcmp(joystick_display_name(1), "Virtual Second Pad") == 0);
	CHECK(joystick[1].assignment[5][0].type == NONE);  // not reloaded from the config
	poll_joystick(0);                      // a free slot is harmless
	joystick_rumble(0, 0xFFFF, 0xFFFF, 10);

	dev0 = attach_pad("Virtual Xbox Pad");  // reconnected: back to its player
	advance(600);
	poll();
	CHECK(joysticks == 2);
	CHECK(!joystick_take_lost());
	CHECK(joystick[0].handle != NULL);
	CHECK(strcmp(joystick_display_name(0), "Virtual Xbox Pad") == 0);
	CHECK(joystick[1].handle == second);

	for (int d = SDL_NumJoysticks() - 1; d >= 0; d--)  // disconnect the first one again
		if (SDL_JoystickGetDeviceInstanceID(d) == joystick[0].instance_id)
			SDL_JoystickDetachVirtual(d);
	advance(600);
	poll();
	CHECK(joysticks == 2 && joystick[0].handle == NULL);
	SDL_JoystickDetachVirtual(0);          // and the second: no slots left
	advance(600);
	poll();
	CHECK(joysticks == 0);
	CHECK(joystick_take_lost());
	joysticks_rumble_all(0xFFFF, 0xFFFF, 100); // must be harmless with nothing connected
	CHECK(strcmp(padHelpText("Press ESC to exit."), "Press ESC to exit.") == 0);  // keyboard texts again

	printf("[6] Xbox, PlayStation, Switch and Steam Deck controllers\n");
	for (size_t m = 0; m < SDL_arraysize(pad_models); m++)
	{
		const Pad_model *model = &pad_models[m];
		printf("     %s\n", model->name);

		int dev = attach_model(model);
		CHECK(dev >= 0);
		advance(600);  // hotplug check + connect-time repeat guard
		poll();
		CHECK(joysticks == 1);
		if (joysticks != 1)
			continue;

		CHECK(joystick[0].controller != NULL);
		if (model->type != SDL_CONTROLLER_TYPE_UNKNOWN)  // the Deck's type depends on the SDL version
			CHECK(SDL_GameControllerGetType(joystick[0].controller) == model->type);
		CHECK(SDL_strstr(joystick_display_name(0), model->name) != NULL);
		CHECK(joystick_first_gamepad() == 0);

		bool labels = true;
		for (int a = 0; a < 10; a++)
		{
			char label[30];
			joystick_action_label(0, a, label, sizeof(label));
			if (strcmp(label, model->labels[a]) != 0)
			{
				printf("       action %d: '%s', expected '%s'\n", a, label, model->labels[a]);
				labels = false;
			}
		}
		CHECK(labels);
		CHECK(strcmp(joystick_button_label(0, SDL_CONTROLLER_BUTTON_A), model->confirm) == 0);
		CHECK(strcmp(joystick_button_label(0, SDL_CONTROLLER_BUTTON_B), model->cancel) == 0);
		char escHelp[64];
		if (model->cancelIcon != 0)
			snprintf(escHelp, sizeof(escHelp), "Press %c to exit.", model->cancelIcon);
		else
			SDL_strlcpy(escHelp, model->escHelp, sizeof(escHelp));
		CHECK(strcmp(padHelpText("Press ESC to exit."), escHelp) == 0);
		if (model->confirmIcon != 0)
			CHECK((unsigned char)joystick_button_icon(0, SDL_CONTROLLER_BUTTON_A)[0] == model->confirmIcon);
		else
			CHECK(strcmp(joystick_button_icon(0, SDL_CONTROLLER_BUTTON_A), model->confirm) == 0);

		bool actions = true;
		for (int a = 0; a < 10; a++)
			for (int i = 0; i < 2; i++)
				if (joystick[0].assignment[a][i].type != NONE)
					actions &= press_only(0, a, &joystick[0].assignment[a][i]);
		CHECK(actions);  // every default button/trigger/stick does its own action only

		SDL_JoystickSetVirtualButton(joystick[0].handle, SDL_CONTROLLER_BUTTON_B, 1);
		poll();
		CHECK(joystick[0].cancel && !joystick[0].confirm);
		SDL_JoystickSetVirtualButton(joystick[0].handle, SDL_CONTROLLER_BUTTON_B, 0);
		poll();

		SDL_JoystickDetachVirtual(SDL_NumJoysticks() - 1);
		advance(600);
		poll();
		CHECK(joysticks == 0);
	}

	printf("[7] an unreadable tyrian.sav is kept as tyrian.sav.bak\n");
	{
		FILE *f = fopen(save_path, "wb");
		fputs("not a save file", f);
		fclose(f);
		loadSaves();
		char bak[600];
		snprintf(bak, sizeof bak, "%s.bak", save_path);
		f = fopen(bak, "rb");
		char text[32] = "";
		if (f != NULL)
		{
			fgets(text, sizeof text, f);
			fclose(f);
		}
		CHECK(strcmp(text, "not a save file") == 0);
	}

	deinit_joysticks();
	printf(failures ? "\n%d FAILURE(S)\n" : "\nALL PASSED\n", failures);
	return failures != 0;
}
