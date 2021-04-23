/*
 * desktop_entry.h - desktop entry reader header
 *
 * Copyright (c) 2019 Xichen Zhou
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#ifndef TW_DESK_ENTRY_H
#define TW_DESK_ENTRY_H

#include <stdbool.h>
#include <wayland-util.h>

#ifdef __cplusplus
extern "C" {
#endif

struct xdg_app_entry {
	char name[128]; //name, all lower case
	char exec[128]; //exec commands
	char icon[128];
	char path[128];
	bool terminal_app;
};

/**
 * @brief loading an desktop entry from a xdg desktop file
 */
bool xdg_app_entry_from_file(const char *path, struct xdg_app_entry *entry);


/**
 * @brief gather all the application info on the system
 */
struct wl_array xdg_apps_gather(void);


#ifdef __cplusplus
}
#endif



#endif /* EOF */
