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
#ifndef OPLCHIP_H
#define OPLCHIP_H

#include <stdbool.h>

// Setup > Sound > Music Chip: true = Nuked OPL3 (close to a real Sound
// Blaster), false = the DOSBox emulator OpenTyrian always used (the default)
extern bool opl_accurate;

// true once the player picked the music chip in Setup; until then the
// default applies even if the config says otherwise (v1.1 and v1.2 always
// saved "accurate", their default)
extern bool opl_chosen;

void opl_set_accurate(bool accurate);

#endif /* OPLCHIP_H */
