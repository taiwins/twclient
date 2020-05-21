/*
 * desktop_entry.c - taiwins client desktop entry functions
 *
 * Copyright (c) 2019-2020 Xichen Zhou
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#include <ctype.h>
#include <stdlib.h>
#include <dirent.h>

#include <string.h>
#include <wayland-util.h>
#include <ctypes/strops.h>
#include <ctypes/helpers.h>
#include <ctypes/os/file.h>
#include <ctypes/vector.h>
#include <twclient/desktop_entry.h>

#define MAX_ENTRY_LEN 128


enum xdg_app_section_type {
	XDG_APP_SEC_ENTRY,
	XDG_APP_SEC_ACTION,
	XDG_APP_SEC_UNKNOWN,
};

static inline void
vector2array(struct wl_array *arr, vector_t *vec)
{
	arr->data = vec->elems;
	arr->size = vec->elemsize * vec->len;
	arr->alloc = vec->elemsize * vec->alloc_len;
	vector_init_zero(vec, vec->elemsize, vec->free);
}

static inline bool
is_section_name(const char *line)
{
	return (*line == '[' && line[strlen(line)-1] == ']');
}

static inline enum xdg_app_section_type
get_section_type(const char *section)
{
	if (strcasestr(section, "desktop entry"))
		return XDG_APP_SEC_ENTRY;
	else if (strcasestr(section, "desktop action"))
		return XDG_APP_SEC_ACTION;
	return XDG_APP_SEC_UNKNOWN;
}

static inline bool
is_empty_line(const char *line)
{
	return strlen(line) == 0;
}

static inline bool
xdg_app_entry_empty(const struct xdg_app_entry *entry)
{
	return strlen(entry->name) == 0 ||
		strlen(entry->exec) == 0;
}

static inline struct xdg_app_entry *
xdg_app_entry_exists(const vector_t *v, const struct xdg_app_entry *app)
{
	struct xdg_app_entry *entry;
	vector_for_each(entry, v) {
		if (strncmp(entry->name, app->name, NUMOF(app->name)) == 0)
			return entry;
	}
	return NULL;
}

static void
xdg_app_entry_complete(struct xdg_app_entry *dst, struct xdg_app_entry *src)
{
	if (strlen(dst->exec) == 0) {
		memcpy(dst->exec, src->exec, 128);
		dst->exec[127] = '\0';
	}
	if (strlen(dst->icon) == 0) {
		memcpy(dst->icon, src->icon, 128);
		dst->icon[127] = '\0';
	}
	if (strlen(dst->path) == 0) {
		memcpy(dst->path, src->path, 128);
		dst->icon[127] = '\0';
	}
}

WL_EXPORT bool
xdg_app_entry_from_file(const char *path, struct xdg_app_entry *entry)
{
	FILE *file = NULL;
	ssize_t len = 0;
	size_t allocated = 1000;
	char *rawline = malloc(allocated);
	enum xdg_app_section_type curr_section = XDG_APP_SEC_UNKNOWN;

	if (!is_file_exist(path))
		return false;
	file = fopen(path, "r");
	if (!file)
		return false;

	strcpy(entry->path, "/");
	entry->terminal_app =false;
	while ((len = getline(&rawline, &allocated, file)) != -1) {
		char *line = strop_ltrim(rawline); strop_rtrim(line);
		//get rid of comments first
		char *comment_point = strstr(line, "#");
		char *equal = NULL;
		if (comment_point)
			*comment_point = '\0';

		if (is_empty_line(line))
			continue;
		if (is_section_name(line)) {
			curr_section = get_section_type(line);
			continue;
		}
		if (curr_section != XDG_APP_SEC_ENTRY)
			continue;
		//get key value pair
		equal = strstr(line, "=");
		if (equal)
			*equal = '\0';
		else //a single line
			continue;
		char *key = line;
		char *value = equal + 1;
		bool name_to_long = false;

		strop_rtrim(value);
		//get the entries
		if (strcasecmp(key, "name") == 0) {
			strop_ncpy(entry->name, value, MAX_ENTRY_LEN);
			name_to_long |= strlen(value) > MAX_ENTRY_LEN;
		} else if (strcasecmp(key, "exec") == 0) {
			strop_ncpy(entry->exec, value, MAX_ENTRY_LEN);
			name_to_long |= strlen(value) > MAX_ENTRY_LEN;
		} else if (strcasecmp(key, "icon") == 0) {
			strop_ncpy(entry->icon, value, MAX_ENTRY_LEN);
			name_to_long |= strlen(value) > MAX_ENTRY_LEN;
		} else if (strcasecmp(key, "path") == 0 &&
			   strlen(value) < MAX_ENTRY_LEN) {
			name_to_long |= strlen(value) > MAX_ENTRY_LEN;
			strcpy(entry->path, value);
		} else if (strcasecmp(key, "terminal"))
			entry->terminal_app =
				(strcmp(value, "true") == 0) ? true : false;

		if (name_to_long) {
			free(rawline);
			return false;
		}
	}

	free(rawline);
	fclose(file);
	return !xdg_app_entry_empty(entry);
}

WL_EXPORT struct wl_array
xdg_apps_gather(void)
{
	struct wl_array ret;
	vector_t apps;
	vector_init(&apps, sizeof(struct xdg_app_entry), NULL);

	const char *desktop_dir = "/usr/share/applications";

	DIR *dir = opendir(desktop_dir);
	for (struct dirent *entry = readdir(dir); entry; entry = readdir(dir)) {
		if (entry->d_type != DT_REG)
			continue;

		struct xdg_app_entry app_entry = {0}, *query;
		char total_path[1000] = "/usr/share/applications";
		path_concat(total_path, 999, 1, entry->d_name);
		if (is_file_type(entry->d_name, ".desktop") &&
		    xdg_app_entry_from_file(total_path, &app_entry)) {
			if ((query = xdg_app_entry_exists(&apps, &app_entry)))
				xdg_app_entry_complete(query, &app_entry);
			else
				vector_append(&apps, &app_entry);

		}
	}
	//now we just need to move
	vector2array(&ret, &apps);
	return ret;
}
