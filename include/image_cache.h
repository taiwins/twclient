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


#ifdef __cplusplus
}
#endif



#endif /* EOF */
