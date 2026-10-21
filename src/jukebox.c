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
#include "jukebox.h"

#include "font.h"
#include "joystick.h"
#include "keyboard.h"
#include "lang.h"
#include "lds_play.h"
#include "loudness.h"
#include "mtrand.h"
#include "musmast.h"
#include "nortsong.h"
#include "palette.h"
#include "starlib.h"
#include "vga_palette.h"
#include "video.h"

#include <stdio.h>

// the jukebox's own controller buttons (A, B and the directions arrive as keys)
static const SDL_GameControllerButton padButtons[] = {
	SDL_CONTROLLER_BUTTON_X,
	SDL_CONTROLLER_BUTTON_Y,
	SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
	SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
};

// a button of padButtons that has just been pressed on any controller, or SDL_CONTROLLER_BUTTON_INVALID
static SDL_GameControllerButton padButtonPressed(bool held[COUNTOF(padButtons)])
{
	SDL_GameControllerButton pressed = SDL_CONTROLLER_BUTTON_INVALID;

	for (size_t i = 0; i < COUNTOF(padButtons); ++i)
	{
		bool down = false;
		for (int j = 0; j < joysticks; ++j)
			if (joystick[j].controller != NULL && SDL_GameControllerGetButton(joystick[j].controller, padButtons[i]))
				down = true;

		if (down && !held[i] && pressed == SDL_CONTROLLER_BUTTON_INVALID)
			pressed = padButtons[i];
		held[i] = down;
	}

	return pressed;
}

static void drawPadHelp(int j, int x)
{
	char buffer[128];

	snprintf(buffer, sizeof(buffer), tr("%s quit   %s stop/play   %s hide text"),
	         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_B),
	         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_X),
	         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_Y));
	drawFontHvAligned(VGAScreen, x, 170, buffer, FONT_SMALL, ALIGN_CENTER, 1, 0);

	snprintf(buffer, sizeof(buffer), tr("%s song   %s%s sound effects"),
	         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_DPAD_LEFT),
	         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_LEFTSHOULDER),
	         joystick_button_icon(j, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));
	drawFontHvAligned(VGAScreen, x, 180, buffer, FONT_SMALL, ALIGN_CENTER, 1, 0);
}

void jukebox(void)  // FKA Setup.jukeboxGo
{
	bool trigger_quit = false,  // true when user wants to quit
	     quitting = false;
	
	bool hide_text = false;

	bool fade_looped_songs = true, fading_song = false;
	bool stopped = false;

	bool fx = false;
	int fx_num = 0;

	int palette_fade_steps = 15;

	int diff[256][3];
	init_step_fade_palette(diff, vga_palette, 0, 255);

	JE_starlib_init();

	int fade_volume = tyrMusicVolume;

	bool padHeld[COUNTOF(padButtons)];
	for (size_t i = 0; i < COUNTOF(padButtons); ++i)
		padHeld[i] = true;  // a button still held from the Setup menu is not a press
	
	for (; ; )
	{
		if (!stopped && !audio_disabled)
		{
			if (songlooped && fade_looped_songs)
				fading_song = true;

			if (fading_song)
			{
				if (fade_volume > 5)
				{
					fade_volume -= 2;
				}
				else
				{
					fade_volume = tyrMusicVolume;

					fading_song = false;
				}

				set_volume(fade_volume, fxVolume);
			}

			if (!playing || (songlooped && fade_looped_songs && !fading_song))
				play_song(mt_rand() % MUSIC_NUM);
		}

		setFrameCount(1);

		SDL_FillRect(VGAScreenSeg, NULL, 0);

		KeyboardInput keyboardInput;

		bool gotKeyboardInput = starLibMain(&keyboardInput);

		if (!hide_text)
		{
			char buffer[60];
			
			if (fx)
				snprintf(buffer, sizeof(buffer), "%d %s", fx_num + 1, soundTitle[fx_num]);
			else
				snprintf(buffer, sizeof(buffer), "%d %s", song_playing + 1, musicTitle[song_playing]);
			
			const int x = VGAScreen->w / 2;
			
			const int pad = joystick_first_gamepad();
			if (pad >= 0)
			{
				drawPadHelp(pad, x);
			}
			else
			{
				drawFontHvAligned(VGAScreen, x, 170, tr("Press ESC to quit the jukebox."), FONT_SMALL, ALIGN_CENTER, 1, 0);
				drawFontHvAligned(VGAScreen, x, 180, tr("Arrow keys change the song being played."), FONT_SMALL, ALIGN_CENTER, 1, 0);
			}
			drawFontHvAligned(VGAScreen, x, 190, buffer,                                     FONT_SMALL, ALIGN_CENTER, 1, 4);
		}

		if (palette_fade_steps > 0)
			step_fade_palette(diff, palette_fade_steps--, 0, 255);
		
		JE_showVGA();

		waitUntilElapsed();

		// Quit on mouse click.
		if (mouseGetInput(INPUT_NO_MOTION, NULL))
			trigger_quit = true;

		switch (padButtonPressed(padHeld))
		{
		case SDL_CONTROLLER_BUTTON_X:
			if (stopped)
				restart_song();
			else
				stop_song();
			stopped = !stopped;
			break;
		case SDL_CONTROLLER_BUTTON_Y:
			hide_text = !hide_text;
			break;
		case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
			// the first press shows the effect last heard; the next ones pick another
			if (fx && --fx_num < 0)
				fx_num = SOUND_COUNT - 1;
			fx = true;
			JE_playSampleNum(fx_num + 1);
			break;
		case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
			if (fx && ++fx_num >= SOUND_COUNT)
				fx_num = 0;
			fx = true;
			JE_playSampleNum(fx_num + 1);
			break;
		default:
			break;
		}

		if (gotKeyboardInput)
		{
			switch (KEY_COMBO(keyboardInput.mod, keyboardInput.scancode))
			{
			case SDL_SCANCODE_ESCAPE:
			case SDL_SCANCODE_Q:
			case KEY_COMBO(KMOD_SHIFT, SDL_SCANCODE_Q):
				trigger_quit = true;
				break;

			case SDL_SCANCODE_SPACE:
				hide_text = !hide_text;
				break;

			case SDL_SCANCODE_F:
			case KEY_COMBO(KMOD_SHIFT, SDL_SCANCODE_F):
				fading_song = !fading_song;
				break;
			case SDL_SCANCODE_N:
			case KEY_COMBO(KMOD_SHIFT, SDL_SCANCODE_N):
				fade_looped_songs = !fade_looped_songs;
				break;
			case SDL_SCANCODE_V:
			case KEY_COMBO(KMOD_SHIFT, SDL_SCANCODE_V):
				// Not implemented.
				break;
			case SDL_SCANCODE_T:
			case KEY_COMBO(KMOD_SHIFT, SDL_SCANCODE_T):
				// Not implemented.
				break;

			case SDL_SCANCODE_SLASH:
				fx = !fx;
				break;
			case SDL_SCANCODE_COMMA:
				if (fx && --fx_num < 0)
					fx_num = SOUND_COUNT - 1;
				break;
			case SDL_SCANCODE_PERIOD:
				if (fx && ++fx_num >= SOUND_COUNT)
					fx_num = 0;
				break;
			case SDL_SCANCODE_SEMICOLON:
				if (fx)
					JE_playSampleNum(fx_num + 1);
				break;

			case SDL_SCANCODE_LEFT:
			case SDL_SCANCODE_UP:
				play_song((song_playing > 0 ? song_playing : MUSIC_NUM) - 1);
				stopped = false;
				fx = false;
				break;
			case SDL_SCANCODE_RETURN:
			case SDL_SCANCODE_RIGHT:
			case SDL_SCANCODE_DOWN:
				play_song((song_playing + 1) % MUSIC_NUM);
				stopped = false;
				fx = false;
				break;
			case SDL_SCANCODE_S:
			case KEY_COMBO(KMOD_SHIFT, SDL_SCANCODE_S):
				stop_song();
				stopped = true;
				break;
			case SDL_SCANCODE_R:
			case KEY_COMBO(KMOD_SHIFT, SDL_SCANCODE_R):
				restart_song();
				stopped = false;
				break;

			default:
				break;
			}
		}
		
		// user wants to quit, start fade-out
		if (trigger_quit && !quitting)
		{
			palette_fade_steps = 15;
			
			SDL_Color black = { 0, 0, 0 };
			init_step_fade_solid(diff, black, 0, 255);
			
			quitting = true;
		}
		
		// if fade-out finished, we can finally quit
		if (quitting && palette_fade_steps == 0)
			break;
	}

	set_volume(tyrMusicVolume, fxVolume);
}
