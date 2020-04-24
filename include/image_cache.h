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

#include "ui.h"


#ifdef __cplusplus
extern "C" {
#endif



struct image_cache {
	struct tw_bbox dimension;
	/**> an array of tw_box **/
	struct wl_array image_boxes;
	/**> rgb pixels **/
	unsigned char *atlas;

	/**> storage for query image by name */
	struct wl_array handles;
	struct wl_array strings;
};

/**
 * @brief icontheme_dir
 *
 * generate icon file paths for you
 */
struct icontheme_dir {
	struct wl_array apps;
	struct wl_array mimes;
	struct wl_array places;
	struct wl_array status;
	struct wl_array devices;
	char theme_dir[256];
};

/**
 * @brief loading atlas texture from a list of image paths.
 */
struct image_cache
image_cache_from_arrays(const struct wl_array *handle_array,
			const struct wl_array *str_array,
			void (*convert)(char output[256], const char *input));
/**
 * @brief loading atlas texture from a list of image paths, with filtering
 * ability
 */
struct image_cache
image_cache_from_arrays_filtered(const struct wl_array *handle_array,
                                 const struct wl_array *str_array,
                                 void (*convert)(char output[256], const char *input),
                                 bool (*filter)(const char *input, void *data),
                                 void *user_data);
/**
 * @brief image cache file IO reading
 *
 * very slow, could be on another thread
 */
struct image_cache
image_cache_from_fd(int fd);

/**
 * @brief image cache file IO writing
 *
 * very slow, could be on another thread
 */
void
image_cache_to_fd(const struct image_cache *cache, int fd);

void
image_cache_release(struct image_cache *cache);

/* icon cache loading routines */
void
icontheme_dir_init(struct icontheme_dir *theme, const char *path);

void
icontheme_dir_release(struct icontheme_dir *theme);

void
search_icon_dirs(struct icontheme_dir *output,
                 const int min_res, const int max_res);
int
search_icon_imgs_subdir(struct wl_array *handle_pool,
                        struct wl_array *string_pool,
                        const char *dir_path);
void
search_icon_imgs(struct wl_array *handles, struct wl_array *strings,
                 const char *themepath, const struct wl_array *icondir);

/* image loaders */
void
image_info(const char *path, int *w, int *h, int *nchannels);

unsigned char *
image_load(const char *path, int *w, int *h, int *nchannels);

/**
 * @brief this function loads the image and resize it to given size
 */
bool
image_load_for_buffer(const char *path, enum wl_shm_format format,
                      int width, int height, unsigned char *mem);


#ifdef __cplusplus
}
#endif



#endif /* EOF */
