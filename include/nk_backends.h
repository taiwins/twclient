#ifndef NK_BACKENDS_H
#define NK_BACKENDS_H

#include <stdbool.h>
#include <stdint.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include "ui.h"

struct nk_wl_backend;
struct nk_context;
struct nk_style;

#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS

#include "nuklear/nuklear.h"
struct xdg_toplevel;
struct xdg_surface;

typedef void (*nk_wl_drawcall_t)(struct nk_context *ctx, float width, float height, struct app_surface *app);
typedef void (*nk_wl_postcall_t)(struct app_surface *app);

/*******************************************************************************
 * shell implementation
 ******************************************************************************/
NK_API struct xdg_toplevel *
nk_wl_impl_xdg_shell_surface(struct app_surface *app,
                             struct xdg_surface *xdg_surface);

NK_API void
nk_wl_impl_wl_shell_surface(struct app_surface *app,
                            struct wl_shell_surface *protocol);

/*******************************************************************************
 * backends
 ******************************************************************************/

/* cairo_backend */
struct nk_wl_backend *
nk_cairo_create_bkend(void);

void
nk_cairo_destroy_bkend(struct nk_wl_backend *bkend);

void
nk_cairo_impl_app_surface(struct app_surface *surf, struct nk_wl_backend *bkend,
                          nk_wl_drawcall_t draw_cb,  const struct bbox geo);


/* egl_backend */
struct nk_wl_backend*
nk_egl_create_backend(const struct wl_display *display);

void
nk_egl_destroy_backend(struct nk_wl_backend *b);

void
nk_egl_impl_app_surface(struct app_surface *surf, struct nk_wl_backend *bkend,
                        nk_wl_drawcall_t draw_cb, const struct bbox geo);


/* vulkan_backend */
/* struct nk_wl_backend *nk_vulkan_backend_create(void); */
/* struct nk_wl_backend *nk_vulkan_backend_clone(struct nk_wl_backend *b); */
/* void nk_vulkan_backend_destroy(struct nk_wl_backend *b); */
/* void */
/* nk_vulkan_impl_app_surface(struct app_surface *surf, struct nk_wl_backend *bkend, */
/*			   nk_wl_drawcall_t draw_cb, const struct bbox geo); */


NK_API xkb_keysym_t
nk_wl_get_keyinput(struct nk_context *ctx);

NK_API bool
nk_wl_get_btn(struct nk_context *ctx, uint32_t *button, uint32_t *sx, uint32_t *sy);

NK_API void
nk_wl_add_idle(struct nk_context *ctx, nk_wl_postcall_t task);


NK_API const struct nk_style *
nk_wl_get_curr_style(struct nk_wl_backend *bkend);

NK_API void
nk_wl_test_draw(struct nk_wl_backend *bkend, struct app_surface *app,
		nk_wl_drawcall_t draw_call);


/*******************************************************************************
 * image loader
 ******************************************************************************/

NK_API struct nk_image *
nk_wl_load_image(const char *path, enum wl_shm_format format,
                 struct nk_wl_backend *b);

NK_API bool
nk_wl_load_image_for_buffer(const char *path, enum wl_shm_format format,
			    int width, int height, unsigned char *mem);

NK_API void nk_wl_free_image(struct nk_image *img);


/*******************************************************************************
 * font loader
 ******************************************************************************/

enum nk_wl_font_slant {
	NK_WL_SLANT_ROMAN,
	NK_WL_SLANT_ITALIC,
	NK_WL_SLANT_OBLIQUE,
};

//we accept only true type font
struct nk_wl_font_config {
	const char *name;
	enum nk_wl_font_slant slant;
	int pix_size, scale;
	int nranges;
	const nk_rune **ranges;

	//fc config offers light, medium, demibold, bold, black
	//demibold, bold and black is true, otherwise false.
	bool bold; //classified as bold
	//private: TTF only
	bool TTFonly;
};


NK_API struct nk_user_font *
nk_wl_new_font(struct nk_wl_font_config config,
               struct nk_wl_backend *backend);

NK_API void
nk_wl_destroy_font(struct nk_user_font *font);

#endif
