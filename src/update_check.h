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
#ifndef UPDATE_CHECK_H
#define UPDATE_CHECK_H

#include <stdbool.h>

// "check" in the "updates" section of opentyrian.cfg; on by default
extern bool update_check_enabled;

// asks GitHub, in the background, for the latest published release; does
// nothing if the check is off, already done or this build has no version
void update_check_start(void);

// the newer release's tag (e.g. "v1.0.3"), or NULL if there is none, the
// answer has not arrived yet, or the check is off
const char *update_check_newer_version(void);

// where the newer release can be downloaded
#define UPDATE_CHECK_RELEASES_URL "https://github.com/gabrielbadolato/tyrian2000plus/releases/latest"

#endif /* UPDATE_CHECK_H */
