#include <dirent.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <cairo/cairo.h>
#include <wayland-util.h>
#include <sys/types.h>

#include <twclient/image_cache.h>
#include <ctypes/sequential.h>
#include <ctypes/strops.h>
#include <ctypes/os/file.h>


static int filter_image(const struct dirent *file)
{
	if (file->d_type == DT_REG || file->d_type == DT_LNK)
		return true;
	return false;
}

static void
build_image_strings(const char *path, struct wl_array *handle_pool, struct wl_array *string_pool)
{
	struct dirent **namelist;
	int n;
	long totallen = 0;

	wl_array_init(handle_pool);
	wl_array_init(string_pool);
	n = scandir(path, &namelist, filter_image, alphasort);
	if (n < 0) {
		perror("scandir");
		return;
	}
	wl_array_add(handle_pool, n * sizeof(uint64_t));
	for (int i = 0; i < n; i++) {
		*((uint64_t *)handle_pool->data + i) = totallen;
		totallen += strlen(path) + 1 + strlen(namelist[i]->d_name) + 1;
		wl_array_add(string_pool, totallen);
		sprintf((char *)string_pool->data + *((uint64_t*)handle_pool->data+i),
			"%s/%s",path, namelist[i]->d_name);
		free(namelist[i]);
	}
	free(namelist);
}

static void
path_to_node(char output[256], const char *input)
{
	char *copy = strdup(input);
	char *base = basename(copy);
	//remove png at the tail.
	strop_ncpy(output, base, 256);
	free(copy);
}

int main(int argc, char *argv[])
{
	/* load_image(argv[1], argv[2]); */
	/* return; */
	struct wl_array handle_pool, string_pool;
	build_image_strings(argv[1], &handle_pool, &string_pool);

	struct image_cache imgcache =
		image_cache_from_arrays(&handle_pool, &string_pool,
		                        path_to_node);

	/* nk_wl_build_theme_images(&handle_pool, &string_pool, argv[2]); */
	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		imgcache.atlas, CAIRO_FORMAT_ARGB32,
		imgcache.dimension.w,
		imgcache.dimension.h, imgcache.dimension.w * 4);
	cairo_surface_write_to_png(surface, argv[2]);
	cairo_surface_destroy(surface);

	image_cache_release(&imgcache);
	wl_array_release(&handle_pool);
	wl_array_release(&string_pool);
	return 0;
}
