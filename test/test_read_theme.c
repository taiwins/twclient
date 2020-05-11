#include "helpers.h"
#include "vector.h"
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
#include <twclient/image_cache.h>

#define MAX_DIR_LEN 32
struct icon_dir {
	char dir[MAX_DIR_LEN];
	uint32_t size;
};

int main(int argc, char *argv[])
{
	struct wl_array string_pool, handle_pool;
	struct icontheme_dir themedir;
	struct icon_dir *icon = NULL;
	off_t *off = NULL;
	icontheme_dir_init(&themedir, "/usr/share/icons/hicolor");
	search_icon_dirs(&themedir, 31, 128);
	//print
	wl_array_for_each(icon, &themedir.apps)
		printf("%s\t", icon->dir);
	printf("\n");
	wl_array_for_each(icon, &themedir.mimes)
		printf("%s\t", icon->dir);
	printf("\n");
	wl_array_for_each(icon, &themedir.places)
		printf("%s\t", icon->dir);
	printf("\n");
	wl_array_for_each(icon, &themedir.devices)
		printf("%s\t", icon->dir);
	printf("\n");
	wl_array_for_each(icon, &themedir.status)
		printf("%s\t", icon->dir);
	printf("\n");

	//doing search on
	struct wl_array *to_searches[] = {
		&themedir.apps, &themedir.mimes, &themedir.devices,
		&themedir.places, &themedir.status
	};
	for (unsigned i = 0; i < NUMOF(to_searches); i++) {
		wl_array_init(&string_pool);
		wl_array_init(&handle_pool);
		search_icon_imgs(&handle_pool, &string_pool,
		                 themedir.theme_dir, to_searches[i]);
		wl_array_for_each(off, &handle_pool) {
			fprintf(stdout, "%s\n", (char *)string_pool.data + *off);
		}

		wl_array_release(&string_pool);
		wl_array_release(&handle_pool);
	}
	icontheme_dir_release(&themedir);
	return 0;
}
