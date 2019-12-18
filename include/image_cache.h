/*
 * image_cache.h - image cache generation header
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

#ifndef TW_IMG_CACHE_H
#define TW_IMG_CACHE_H

#include <stdint.h>
#include <unistd.h>

#include <sequential.h>
#include "ui.h"


#ifdef __cplusplus
extern "C" {
#endif



struct image_cache {
	struct bbox dimension;
	//an array of images
	struct wl_array image_boxes;
	unsigned char *atlas;
	struct wl_array handles;
	struct wl_array strings;
};

/**
 * @brief icontheme_dir
 *
 * generate icon file paths for you
 */
struct icontheme_dir {
	vector_t apps;
	vector_t mimes;
	vector_t places;
	vector_t status;
	vector_t devices;
	char theme_dir[256];
};

struct image_cache
image_cache_from_arrays(const struct wl_array *handle_array,
			const struct wl_array *str_array,
			void (*convert)(char output[256], const char *input));

struct image_cache image_cache_from_fd(int fd);

void image_cache_to_fd(const struct image_cache *cache, int fd);

void image_cache_release(struct image_cache *cache);

void icontheme_dir_init(struct icontheme_dir *theme, const char *path);

void icontheme_dir_release(struct icontheme_dir *theme);

void search_icon_dirs(struct icontheme_dir *output,
		      const int min_res, const int max_res);

void search_icon_imgs(struct wl_array *handles, struct wl_array *strings,
		      const char *themepath, const vector_t *icondir);

//image loaders
void image_info(const char *path, int *w, int *h, int *nchannels);

//image allocated by malloc, use free to release the memory
unsigned char *
image_load(const char *path, int *w, int *h, int *nchannels);


#ifdef __cplusplus
}
#endif



#endif /* EOF */
