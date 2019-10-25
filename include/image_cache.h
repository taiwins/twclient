#ifndef TW_IMG_CACHE_H
#define TW_IMG_CACHE_H

#include <stdint.h>
#include <unistd.h>
#include "ui.h"
#include "sequential.h"


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

struct image_cache
image_cache_from_arrays(const struct wl_array *handle_array,
			const struct wl_array *str_array,
			void (*convert)(char output[256], const char *input));

/* TODO implement this two */
struct image_cache image_cache_from_fd(int fd);
void image_cache_to_fd(const struct image_cache *cache, int fd);

void image_cache_release(struct image_cache *cache);


#ifdef __cplusplus
}
#endif



#endif /* EOF */
