/*
 * icon_search.c - dekstop icons search functions
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

#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <wayland-util.h>

#include <strops.h>
#include <sequential.h>
#include <os/file.h>

#include <image_cache.h>

#define MAX_DIR_LEN 32

enum icon_section_type {
	SEC_ICON_THEME,
	SEC_ICON_APP,
	SEC_ICON_MIMETYPES,
	SEC_ICON_PLACES,
	SEC_ICON_STATUS,
	SEC_ICON_DEVICES,
	SEC_ICON_UNKNOWN,
};

struct icon_dir {
	char dir[MAX_DIR_LEN];
	uint32_t size;
};

#define WL_ARRAY_NMEMB(array, type) (array)->size / sizeof(type)

static inline void
copy_icon_dir(struct icon_dir *dir, const uint32_t size, const char *path)
{
	strcpy(dir->dir, path);
	dir->size = size;
}


WL_EXPORT void
icontheme_dir_init(struct icontheme_dir *theme, const char *path)
{
	strcpy(theme->theme_dir, path);
	wl_array_init(&theme->apps);
	wl_array_init(&theme->mimes);
	wl_array_init(&theme->places);
	wl_array_init(&theme->status);
	wl_array_init(&theme->devices);
}

WL_EXPORT void
icontheme_dir_release(struct icontheme_dir *theme)
{
	wl_array_release(&theme->apps);
	wl_array_release(&theme->mimes);
	wl_array_release(&theme->places);
	wl_array_release(&theme->status);
	wl_array_release(&theme->devices);
}

static inline char *
brace_remove(char *line)
{
	if (line[strlen(line)-1] == ']')
		line[strlen(line)-1] = '\0';
	if (*line == '[')
		return line+1;
	else
		return line;
}

static inline bool
is_section_name(const char *line)
{
	return (*line == '[' && line[strlen(line)-1] == ']');
}

static inline enum icon_section_type
get_section_type(const char *section)
{
	if (strcmp(section, "[Icon Theme]") == 0)
		return SEC_ICON_THEME;
	else if (strcasestr(section, "apps"))
		return SEC_ICON_APP;
	else if (strcasestr(section, "mime"))
		return SEC_ICON_MIMETYPES;
	else if (strcasestr(section, "places"))
		return SEC_ICON_PLACES;
	else if (strcasestr(section, "devices"))
		return SEC_ICON_DEVICES;
	else if (strcasestr(section, "status"))
		return SEC_ICON_STATUS;;
	return SEC_ICON_UNKNOWN;
}

struct size_state {
	char currdir[MAX_DIR_LEN];
	int curr_size, curr_max_size, curr_min_size;
	int curr_scale;
};

static inline bool
is_size_right(const int min_res, const int max_res,
	      const struct size_state *curr)
{
	return INBOUND(curr->curr_size, min_res, max_res);
}

static inline void
size_state_reset(struct size_state *state, const char *path)
{
	strcpy(state->currdir, path);
	state->curr_max_size = -1;
	state->curr_min_size = -1;
	state->curr_size = -1;
	state->curr_scale = 1;
}

static int
cmp_icon_dir(const void *l, const void *r)
{
	const struct icon_dir *ld = l, *rd = r;
	return rd->size - ld->size;
}


static bool
obj_exists(const char *filename,
           const struct wl_array *handle_pool,
           const struct wl_array *string_pool)
{
	off_t *off;
	char namecpy[strlen(filename)+1];
	strcpy(namecpy, filename);
	*strrchr(namecpy, '.') = '\0';
	const char *ptr = string_pool->data;
	wl_array_for_each(off, handle_pool) {
		const char *path = ptr + *off;
		if (strstr(path, namecpy))
			return true;
	}
	return false;
}

WL_EXPORT int
search_icon_imgs_subdir(struct wl_array *handle_pool,
                        struct wl_array *string_pool,
                        const char *dir_path)
{
	int count = 0;
	char filepath[1024];
	DIR *dir = opendir(dir_path);
	if (!dir)
		return 0;
	for (struct dirent *entry = readdir(dir); entry;
	     entry = readdir(dir)) {
		if (!(entry->d_type == DT_REG ||
		      entry->d_type == DT_LNK) ||
		    (!is_file_type(entry->d_name, ".svg") &&
		     !is_file_type(entry->d_name, ".png")))
			continue;
		if (obj_exists(entry->d_name, handle_pool, string_pool))
			continue;
		if (strlen(entry->d_name) + strlen(dir_path) + 1 >= 1024)
			continue;
		strcpy(filepath, dir_path);
		path_concat(filepath, 1024, 1, entry->d_name);
		char *tocpy = wl_array_add(string_pool, strlen(filepath)+1);
		*(off_t *)wl_array_add(handle_pool, sizeof(off_t)) =
			(tocpy - (char *)string_pool->data);
		strcpy(tocpy, filepath);
		count++;
	}
	closedir(dir);
	return count;
}

WL_EXPORT void
search_icon_imgs(struct wl_array *handles, struct wl_array *strings,
		 const char *themepath, const struct wl_array *icondir)
{
	//handles needs to be initialized
	char absdir[1024];
	struct icon_dir *icons = NULL;
	int count = 0;

	if (strlen(themepath) + MAX_DIR_LEN + 2 >= 1024)
		return;
	wl_array_for_each(icons, icondir) {
		strcpy(absdir, themepath);
		path_concat(absdir, sizeof(absdir), 1, icons->dir);
		count += search_icon_imgs_subdir(handles, strings, absdir);
	}
}

WL_EXPORT void
search_icon_dirs(struct icontheme_dir *output,
	const int min_res, const int max_res)
{
	struct size_state state = {{0}, -1, -1, -1, 1};
	char theme_file[1000];
	size_t allocated = 1000;
	ssize_t len = 0;
	DIR *dir = opendir(output->theme_dir);
	if (!dir)
		return;

	char *rawline = malloc(1000);
	strcpy(theme_file, output->theme_dir);
	path_concat(theme_file, 999, 1, "index.theme");
	if (!is_file_exist(theme_file))
		goto out;

	FILE *file = fopen(theme_file, "r");
	enum icon_section_type curr_section = SEC_ICON_UNKNOWN;

	while ((len = getline(&rawline, &allocated, file)) != -1) {
		char *line = strop_ltrim(rawline); strop_rtrim(line);
		char *comment_point = strstr(line, "#");
		if (comment_point)
			*comment_point = '\0';

		if (strlen(line) == 0)
			continue;
		//get section
		if (is_section_name(line)) {
			curr_section = get_section_type(line);
			line = brace_remove(line);
			//skip over the dirs that too long
			if (strlen(line) >= MAX_DIR_LEN-1)
				curr_section = SEC_ICON_UNKNOWN;
			else
				size_state_reset(&state, line);
			continue;
		}
		if (curr_section == SEC_ICON_UNKNOWN)
			continue;
		// theme also has the inherits attributes, we do not deal with
		// that here
		char *equal = NULL;
		equal = strstr(line, "=");
		if (equal)
			*equal = '\0';

		//deal with key value
		char *key = line;
		char *value = equal + 1;
		//size is a required key, so we are good
		if (strcasecmp(key, "size") == 0) {
			state.curr_size = state.curr_scale * atoi(value);
			continue;
		} else if (strcasecmp(key, "minsize") == 0) {
			state.curr_min_size = atoi(value);
			continue;
		} else if (strcasecmp(key, "maxsize") == 0) {
			state.curr_max_size = atoi(value);
			continue;
		} else if (strcasecmp(key, "scale") == 0) {
			state.curr_scale = atoi(value);
		}
		if (curr_section == SEC_ICON_APP &&
		    is_size_right(min_res, max_res, &state))
			copy_icon_dir(wl_array_add(&output->apps,
			                           sizeof(struct icon_dir)),
				      state.curr_size, state.currdir);

		else if (curr_section == SEC_ICON_MIMETYPES &&
		    is_size_right(min_res, max_res, &state))
			copy_icon_dir(wl_array_add(&output->mimes,
			                           sizeof(struct icon_dir)),
				      state.curr_size, state.currdir);

		else if (curr_section == SEC_ICON_PLACES &&
		    is_size_right(min_res, max_res, &state))
			copy_icon_dir(wl_array_add(&output->places,
			                           sizeof(struct icon_dir)),
				      state.curr_size, state.currdir);

		else if (curr_section == SEC_ICON_DEVICES &&
		    is_size_right(min_res, max_res, &state))
			copy_icon_dir(wl_array_add(&output->devices,
			                           sizeof(struct icon_dir)),
				      state.curr_size, state.currdir);

		else if (curr_section == SEC_ICON_STATUS &&
		    is_size_right(min_res, max_res, &state))
			copy_icon_dir(wl_array_add(&output->status,
			                           sizeof(struct icon_dir)),
				      state.curr_size, state.currdir);
	}

	qsort(output->apps.data, WL_ARRAY_NMEMB(&output->apps, struct icon_dir),
	      sizeof(struct icon_dir), cmp_icon_dir);
	qsort(output->mimes.data, WL_ARRAY_NMEMB(&output->mimes, struct icon_dir),
	      sizeof(struct icon_dir), cmp_icon_dir);
	qsort(output->places.data, WL_ARRAY_NMEMB(&output->places, struct icon_dir),
	      sizeof(struct icon_dir), cmp_icon_dir);
	qsort(output->devices.data, WL_ARRAY_NMEMB(&output->devices, struct icon_dir),
	      sizeof(struct icon_dir), cmp_icon_dir);
	qsort(output->status.data, WL_ARRAY_NMEMB(&output->status, struct icon_dir),
	      sizeof(struct icon_dir), cmp_icon_dir);

out:
	free(rawline);
	closedir(dir);
}
