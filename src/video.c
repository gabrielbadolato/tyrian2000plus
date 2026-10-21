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
#include "video.h"

#include "keyboard.h"
#include "logging.h"
#include "opentyr.h"
#include "video_scale.h"

#include <assert.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#endif
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

const char *const scaling_mode_names[ScalingMode_MAX] = {
	"Center",
	"Integer",
	"Fit 8:5",
	"Fit 4:3",
};

int fullscreen_display;
ScalingMode scaling_mode = SCALE_ASPECT_4_3;  // matches the original 4:3 CRT presentation
bool vsync_enabled = true;  // sync presentation to the display refresh (no tearing)
bool sharp_bilinear_enabled = false;  // even pixels when the window is not an exact multiple
static SDL_Rect last_output_rect = { 0, 0, vga_width, vga_height };

// the window's size and position when it is not fullscreen, remembered between sessions
WindowGeometry window_geometry = { 0, 0, 0, 0, false, false };

SDL_Surface *VGAScreen, *VGAScreenSeg;
SDL_Surface *VGAScreen2;
SDL_Surface *game_screen;

SDL_Window *main_window = NULL;
static SDL_Renderer *main_window_renderer = NULL;
SDL_PixelFormat *main_window_tex_format = NULL;
static SDL_Texture *main_window_texture = NULL;

// Sharp bilinear: the scaler output is first enlarged by a whole number with nearest-neighbor
// (so every pixel stays a crisp block) and then fitted to the window with bilinear filtering,
// which only blends the one-pixel seam between blocks.  This keeps the pixels the same size
// when the window is not an exact multiple of the game resolution (e.g. Fit 4:3 at 1280x800).
static SDL_Texture *prescale_texture = NULL;
static int prescale_w = 0, prescale_h = 0;

static ScalerFunction scaler_function;

static void init_renderer(void);
void set_vsync(bool enabled)
{
	vsync_enabled = enabled;

	if (main_window_renderer != NULL && SDL_RenderSetVSync(main_window_renderer, enabled ? 1 : 0) != 0)
		logWarn("Failed to %s VSync: %s", enabled ? "enable" : "disable", SDL_GetError());
}

static void deinit_renderer(void);
static void init_texture(void);
static void deinit_texture(void);

static int window_get_display_index(void);
static void window_center_in_display(int display_index);
static bool window_restore_geometry(void);
static void calc_dst_render_rect(SDL_Surface *src_surface, SDL_Rect *dst_rect);
static void scale_and_flip(SDL_Surface *);

#ifdef __EMSCRIPTEN__
// In the browser the "window" is the canvas, and the page sets its size (all
// of the page, or of the itch.io frame): the window always takes that size.
static void window_fit_page(void)
{
	double w, h;
	if (emscripten_get_element_css_size("#canvas", &w, &h) == EMSCRIPTEN_RESULT_SUCCESS && w >= 1 && h >= 1)
		SDL_SetWindowSize(main_window, (int)w, (int)h);
}
#endif

void init_video(void)
{
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
	{
		logFatal("Failed to initialize SDL video: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	// Create the software surfaces that the game renders to. These are all 320x200x8 regardless
	// of the window size or monitor resolution.
	VGAScreen = VGAScreenSeg = SDL_CreateRGBSurface(0, vga_width, vga_height, 8, 0, 0, 0, 0);
	VGAScreen2 = SDL_CreateRGBSurface(0, vga_width, vga_height, 8, 0, 0, 0, 0);
	game_screen = SDL_CreateRGBSurface(0, vga_width, vga_height, 8, 0, 0, 0, 0);

	// The game code writes to surface->pixels directly without locking, so make sure that we
	// indeed created software surfaces that support this.
	assert(!SDL_MUSTLOCK(VGAScreen));
	assert(!SDL_MUSTLOCK(VGAScreen2));
	assert(!SDL_MUSTLOCK(game_screen));

	JE_clr256(VGAScreen);

	// Create the window with a temporary initial size, hidden until we set up the
	// scaler and find the true window size
	main_window = SDL_CreateWindow(opentyrian_str,
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		vga_width, vga_height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);

	if (main_window == NULL)
	{
		logFatal("Failed to create window: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}

	reinit_fullscreen(fullscreen_display);
	init_renderer();
	init_texture();
	init_scaler(scaler);

	const bool restored = fullscreen_display == -1 && window_restore_geometry();

	SDL_ShowWindow(main_window);

	if (restored && window_geometry.maximized)
		SDL_MaximizeWindow(main_window);

	SDL_SetRenderDrawColor(main_window_renderer, 0, 0, 0, 255);
	SDL_RenderClear(main_window_renderer);
	SDL_RenderPresent(main_window_renderer);
}

void deinit_video(void)
{
	video_remember_window();

	deinit_texture();
	deinit_renderer();

	SDL_DestroyWindow(main_window);

	SDL_FreeSurface(VGAScreenSeg);
	SDL_FreeSurface(VGAScreen2);
	SDL_FreeSurface(game_screen);

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

static void init_renderer(void)
{
	main_window_renderer = SDL_CreateRenderer(main_window, -1, vsync_enabled ? SDL_RENDERER_PRESENTVSYNC : 0);

	if (main_window_renderer == NULL)
	{
		logFatal("Failed to create renderer: %s", SDL_GetError());
		exit(EXIT_FAILURE);
	}
}

static void deinit_prescale_texture(void)
{
	if (prescale_texture != NULL)
	{
		SDL_DestroyTexture(prescale_texture);
		prescale_texture = NULL;
	}
	prescale_w = prescale_h = 0;
}

// the prescale texture could not be made; draw without it until the option is set again
static bool prescale_failed = false;

void set_sharp_bilinear(bool enabled)
{
	sharp_bilinear_enabled = enabled;
	prescale_failed = false;

	if (!enabled)
		deinit_prescale_texture();
}

static void deinit_renderer(void)
{
	deinit_prescale_texture();

	if (main_window_renderer != NULL)
	{
		SDL_DestroyRenderer(main_window_renderer);
		main_window_renderer = NULL;
	}
}

static void init_texture(void)
{
	assert(main_window_renderer != NULL);

	int bpp = 32; // TODOSDL2
	Uint32 format = bpp == 32 ? SDL_PIXELFORMAT_RGB888 : SDL_PIXELFORMAT_RGB565;
	int scaler_w = scalers[scaler].width;
	int scaler_h = scalers[scaler].height;

	main_window_tex_format = SDL_AllocFormat(format);

	main_window_texture = SDL_CreateTexture(main_window_renderer, format, SDL_TEXTUREACCESS_STREAMING, scaler_w, scaler_h);

	if (main_window_texture == NULL)
	{
		logFatal("Failed to create scaler texture (%dx%dx%s): %s", scaler_w, scaler_h, SDL_GetPixelFormatName(format), SDL_GetError());
		exit(EXIT_FAILURE);
	}
}

static void deinit_texture(void)
{
	if (main_window_texture != NULL)
	{
		SDL_DestroyTexture(main_window_texture);
		main_window_texture = NULL;
	}

	if (main_window_tex_format != NULL)
	{
		SDL_FreeFormat(main_window_tex_format);
		main_window_tex_format = NULL;
	}
}

static int window_get_display_index(void)
{
	return SDL_GetWindowDisplayIndex(main_window);
}

// The scaler's output size, but never under 640x400: "None" (the default) is only 320x200
// and the scaling mode enlarges it anyway.
static void window_set_scaler_size(void)
{
	SDL_SetWindowSize(main_window, MAX((int)scalers[scaler].width, 2 * vga_width),
	                               MAX((int)scalers[scaler].height, 2 * vga_height));
}

static void window_center_in_display(int display_index)
{
	int win_w, win_h;
	SDL_GetWindowSize(main_window, &win_w, &win_h);

	SDL_Rect bounds;
	SDL_GetDisplayBounds(display_index, &bounds);

	SDL_SetWindowPosition(main_window, bounds.x + (bounds.w - win_w) / 2, bounds.y + (bounds.h - win_h) / 2);
}

// Wayland does not let programs read or choose where their window is
static bool window_position_supported(void)
{
	const char *driver = SDL_GetCurrentVideoDriver();
	return driver == NULL || strcmp(driver, "wayland") != 0;
}

void video_remember_window(void)
{
	if (main_window == NULL || fullscreen_display != -1)
		return;

	const Uint32 flags = SDL_GetWindowFlags(main_window);
	if (flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN))
		return;

	window_geometry.maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
	if (window_geometry.maximized)
		return;  // keep the size and position it had before, for when it is restored

	SDL_GetWindowSize(main_window, &window_geometry.w, &window_geometry.h);

	window_geometry.has_position = window_position_supported();
	if (window_geometry.has_position)
		SDL_GetWindowPosition(main_window, &window_geometry.x, &window_geometry.y);
}

// the display that shows enough of the top of a window at x, y (its title bar
// is just above), or -1 if none does, e.g. a monitor that was unplugged
static int display_showing_window_top(int x, int y, int w)
{
	const SDL_Rect top = { x, y, w, 32 };

	for (int i = 0; i < SDL_GetNumVideoDisplays(); ++i)
	{
		SDL_Rect bounds, visible;
		if (SDL_GetDisplayUsableBounds(i, &bounds) == 0 &&
		    SDL_IntersectRect(&top, &bounds, &visible) &&
		    visible.w >= 64 && visible.h >= 16)
			return i;
	}

	return -1;
}

// puts the window back where it was in the last session; false if nothing was remembered
static bool window_restore_geometry(void)
{
	if (window_geometry.w <= 0 || window_geometry.h <= 0)
		return false;

	int display = window_geometry.has_position && window_position_supported()
		? display_showing_window_top(window_geometry.x, window_geometry.y, window_geometry.w)
		: -1;

	// no bigger than the display it goes to (it may be a smaller one now), no smaller than the scaler
	SDL_Rect bounds;
	if (SDL_GetDisplayUsableBounds(display >= 0 ? display : MAX(0, window_get_display_index()), &bounds) != 0)
		bounds = (SDL_Rect){ 0, 0, window_geometry.w, window_geometry.h };

	const int w = MAX(MIN(window_geometry.w, bounds.w), (int)scalers[scaler].width),
	          h = MAX(MIN(window_geometry.h, bounds.h), (int)scalers[scaler].height);
	SDL_SetWindowSize(main_window, w, h);

	if (display >= 0)
		SDL_SetWindowPosition(main_window, window_geometry.x, window_geometry.y);
	else
		window_center_in_display(window_get_display_index());

	return true;
}

void reinit_fullscreen(int new_display)
{
	video_remember_window();  // while it is still a window

	fullscreen_display = new_display;

	if (fullscreen_display >= SDL_GetNumVideoDisplays())
	{
		fullscreen_display = 0;
	}

	SDL_SetWindowFullscreen(main_window, SDL_FALSE);
	window_set_scaler_size();

	if (fullscreen_display == -1)
	{
#ifdef __EMSCRIPTEN__
		window_fit_page();
#else
		if (!window_restore_geometry())
			window_center_in_display(window_get_display_index());
#endif
	}
	else
	{
		window_center_in_display(fullscreen_display);

		if (SDL_SetWindowFullscreen(main_window, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0)
		{
			reinit_fullscreen(-1);
			return;
		}
	}
}

void video_on_win_resize(void)
{
	int w, h;
	int scaler_w, scaler_h;

	// Tell video to reinit if the window was manually resized by the user.
	// Also enforce a minimum size on the window.

#ifdef __EMSCRIPTEN__
	// The page decides the size; just show the screen again at the new one.
	(void)w; (void)h; (void)scaler_w; (void)scaler_h;
	JE_showVGA();
	return;
#endif

	SDL_GetWindowSize(main_window, &w, &h);
	scaler_w = scalers[scaler].width;
	scaler_h = scalers[scaler].height;

	if (w < scaler_w || h < scaler_h)
	{
		w = w < scaler_w ? scaler_w : w;
		h = h < scaler_h ? scaler_h : h;

		SDL_SetWindowSize(main_window, w, h);
	}
}

void toggle_fullscreen(void)
{
	if (fullscreen_display != -1)
		reinit_fullscreen(-1);
	else
		reinit_fullscreen(SDL_GetWindowDisplayIndex(main_window));
}

bool init_scaler(unsigned int new_scaler)
{
	int w = scalers[new_scaler].width,
	    h = scalers[new_scaler].height;
	int bpp = main_window_tex_format->BitsPerPixel; // TODOSDL2

	scaler = new_scaler;

	deinit_texture();
	init_texture();

	if (fullscreen_display == -1)
	{
		// Changing scalers, when not in fullscreen mode, forces the window
		// to resize to exactly match the scaler's output dimensions.
#ifdef __EMSCRIPTEN__
		(void)w; (void)h;
		window_fit_page();
#else
		(void)w; (void)h;
		window_set_scaler_size();
		window_center_in_display(window_get_display_index());
#endif
	}

	switch (bpp)
	{
	case 32:
		scaler_function = scalers[scaler].scaler32;
		break;
	case 16:
		scaler_function = scalers[scaler].scaler16;
		break;
	default:
		scaler_function = NULL;
		break;
	}

	if (scaler_function == NULL)
	{
		assert(false);
		return false;
	}

	return true;
}

bool set_scaling_mode_by_name(const char *name)
{
	for (int i = 0; i < ScalingMode_MAX; ++i)
	{
		 if (strcmp(name, scaling_mode_names[i]) == 0)
		 {
			 scaling_mode = i;
			 return true;
		 }
	}
	return false;
}

void JE_clr256(SDL_Surface *screen)
{
	SDL_FillRect(screen, NULL, 0);
}

void JE_showVGA(void)
{
	scale_and_flip(VGAScreen);
}

static void calc_dst_render_rect(SDL_Surface *const src_surface, SDL_Rect *const dst_rect)
{
	// Decides how the logical output texture (after software scaling applied) will fit
	// in the window.

	int win_w, win_h;
	SDL_GetWindowSize(main_window, &win_w, &win_h);

	int maxh_width, maxw_height;

	switch (scaling_mode)
	{
	case SCALE_CENTER:
		SDL_QueryTexture(main_window_texture, NULL, NULL, &dst_rect->w, &dst_rect->h);
		break;
	case SCALE_INTEGER:
		dst_rect->w = src_surface->w;
		dst_rect->h = src_surface->h;
		while (dst_rect->w + src_surface->w <= win_w && dst_rect->h + src_surface->h <= win_h)
		{
			dst_rect->w += src_surface->w;
			dst_rect->h += src_surface->h;
		}
		break;
	case SCALE_ASPECT_8_5:
		maxh_width = win_h * (8.f / 5.f);
		maxw_height = win_w * (5.f / 8.f);

		if (maxh_width > win_w)
		{
			dst_rect->w = win_w;
			dst_rect->h = maxw_height;
		}
		else
		{
			dst_rect->w = maxh_width;
			dst_rect->h = win_h;
		}
		break;
	case SCALE_ASPECT_4_3:
		maxh_width = win_h * (4.f / 3.f);
		maxw_height = win_w * (3.f / 4.f);

		if (maxh_width > win_w)
		{
			dst_rect->w = win_w;
			dst_rect->h = maxw_height;
		}
		else
		{
			dst_rect->w = maxh_width;
			dst_rect->h = win_h;
		}
		break;
	case ScalingMode_MAX:
		assert(false);
		break;
	}

	dst_rect->x = (win_w - dst_rect->w) / 2;
	dst_rect->y = (win_h - dst_rect->h) / 2;
}

/** Returns the texture to draw at dst_rect: the scaler output, or its sharp bilinear prescale. */
static SDL_Texture *sharp_bilinear_prescale(const SDL_Rect *const dst_rect)
{
	if (!sharp_bilinear_enabled || prescale_failed)
		return main_window_texture;

	int tex_w, tex_h;
	SDL_QueryTexture(main_window_texture, NULL, NULL, &tex_w, &tex_h);

	// Exact multiples (and Center) already look sharp with nearest-neighbor.
	if (dst_rect->w % tex_w == 0 && dst_rect->h % tex_h == 0)
		return main_window_texture;

	if (!SDL_RenderTargetSupported(main_window_renderer))
		return main_window_texture;

	// Round each axis up, so the final bilinear step shrinks slightly (by less than one block)
	// instead of enlarging; that keeps the blended seam at most one window pixel wide.
	int factor_x = (dst_rect->w + tex_w - 1) / tex_w;
	int factor_y = (dst_rect->h + tex_h - 1) / tex_h;

	SDL_RendererInfo info;
	if (SDL_GetRendererInfo(main_window_renderer, &info) == 0)
	{
		while (factor_x > 1 && info.max_texture_width > 0 && tex_w * factor_x > info.max_texture_width)
			--factor_x;
		while (factor_y > 1 && info.max_texture_height > 0 && tex_h * factor_y > info.max_texture_height)
			--factor_y;
	}

	const int w = tex_w * factor_x;
	const int h = tex_h * factor_y;

	if (prescale_texture == NULL || prescale_w != w || prescale_h != h)
	{
		deinit_prescale_texture();

		prescale_texture = SDL_CreateTexture(main_window_renderer, main_window_tex_format->format, SDL_TEXTUREACCESS_TARGET, w, h);
		if (prescale_texture == NULL)
		{
			logWarn("Failed to create sharp bilinear texture (%dx%d): %s", w, h, SDL_GetError());
			prescale_failed = true;  // not saved: the setting stays as the player chose
			return main_window_texture;
		}
		SDL_SetTextureScaleMode(prescale_texture, SDL_ScaleModeLinear);
		prescale_w = w;
		prescale_h = h;
	}

	SDL_SetTextureScaleMode(main_window_texture, SDL_ScaleModeNearest);

	if (SDL_SetRenderTarget(main_window_renderer, prescale_texture) != 0)
		return main_window_texture;
	SDL_RenderCopy(main_window_renderer, main_window_texture, NULL, NULL);
	SDL_SetRenderTarget(main_window_renderer, NULL);

	return prescale_texture;
}

static void scale_and_flip(SDL_Surface *src_surface)
{
	assert(src_surface->format->BitsPerPixel == 8);

	// Do software scaling
	assert(scaler_function != NULL);
	scaler_function(src_surface, main_window_texture);

	SDL_Rect dst_rect;
	calc_dst_render_rect(src_surface, &dst_rect);

	SDL_Texture *const output_texture = sharp_bilinear_prescale(&dst_rect);

	// Clear the window and blit the output texture to it
	SDL_SetRenderDrawColor(main_window_renderer, 0, 0, 0, 255);
	SDL_RenderClear(main_window_renderer);
	SDL_RenderCopy(main_window_renderer, output_texture, NULL, &dst_rect);
	SDL_RenderPresent(main_window_renderer);
#ifdef __EMSCRIPTEN__
	// The browser shows the frame only once the game hands control back to
	// it.  The game's own waits do that (SDL_Delay), but a frame that runs
	// late skips its wait, so give the browser a turn after every frame.
	emscripten_sleep(0);
#endif

	// Save output rect to be used by mouse functions
	last_output_rect = dst_rect;
}

/** Maps a specified point in game screen coordinates to window coordinates. */
void mapScreenPointToWindow(Sint32 *const inout_x, Sint32 *const inout_y)
{
	*inout_x = (2 * *inout_x + 1) * last_output_rect.w / (2 * VGAScreen->w) + last_output_rect.x;
	*inout_y = (2 * *inout_y + 1) * last_output_rect.h / (2 * VGAScreen->h) + last_output_rect.y;
}

/** Maps a specified point in window coordinates to game screen coordinates. */
void mapWindowPointToScreen(Sint32 *const inout_x, Sint32 *const inout_y)
{
	*inout_x = (2 * (*inout_x - last_output_rect.x) + 1) * VGAScreen->w / (2 * last_output_rect.w);
	*inout_y = (2 * (*inout_y - last_output_rect.y) + 1) * VGAScreen->h / (2 * last_output_rect.h);
}

/** Scales a distance in window coordinates to game screen coordinates. */
void scaleWindowDistanceToScreen(Sint32 *const inout_x, Sint32 *const inout_y)
{
	*inout_x = (2 * *inout_x + 1) * VGAScreen->w / (2 * last_output_rect.w);
	*inout_y = (2 * *inout_y + 1) * VGAScreen->h / (2 * last_output_rect.h);
}
