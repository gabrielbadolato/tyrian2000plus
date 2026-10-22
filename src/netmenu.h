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
#ifndef NETMENU_H
#define NETMENU_H

#include "config_file.h"
#include "opentyr.h"

// the player's name and the last address typed (section "network" of opentyrian.cfg)
void netmenu_load_settings(Config *config);
void netmenu_save_settings(Config *config);

#ifdef WITH_NETWORK
// Host Game / Join Game; true once connected to the other player (isNetworkGame set)
bool networkMenu(void);
#endif

#endif /* NETMENU_H */
