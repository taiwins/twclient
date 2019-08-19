#ifndef TW_UI_H
#define TW_UI_H

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <wayland-egl.h>
#include <stdio.h>
#include <stdbool.h>
#include <vulkan/vulkan.h>

//doesnt support jpeg in this way, but there is a cairo-jpeg project
#include <cairo/cairo.h>
#include <wayland-client.h>
#include <wayland-xdg-shell-client-protocol.h>
#include <sequential.h>
#include "ui_event.h"

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////Application style definition/////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
struct shm_pool;

struct tw_rgba_t {
	uint8_t r,g,b,a;
};

struct widget_colors {
	struct tw_rgba_t normal;
	struct tw_rgba_t hover;
	struct tw_rgba_t active;
};

/* the simpler version of style config, it is not gonna be as fancy as
 * nuklear(supporting nk_style_time(image or color) for all gui elements), but
 * it will have a consistent look, the taiwins_theme struct is kinda a big blob,
 * TODO we need a hashing technique to quick check if the theme is the same
 * nuklear has a MumurHash function, we can use that actually
 *
 * I should rename it to widget_theme
 */
struct taiwins_theme {
	uint32_t row_size; //this defines the text size as well
	struct tw_rgba_t window_color;
	struct tw_rgba_t border_color;
	//text colors
	struct tw_rgba_t text_color; //text_edit_cursor as well
	struct tw_rgba_t text_active_color;
	//widget color
	struct widget_colors button;
	struct widget_colors toggle;
	struct widget_colors select;
	struct tw_rgba_t slider_bg_color;
	struct widget_colors slider;
	struct widget_colors chart;
	struct tw_rgba_t combo_color;
	//we can contain a font name here. eventually the icon font is done by
	//searching all the svg in the widgets
	char font[64];
};

extern const struct taiwins_theme taiwins_dark_theme;
extern const struct taiwins_theme taiwins_light_theme;


/**
 * this function exams the theme(color and fonts are valid and do some convert)
 */
bool tw_validate_theme(struct taiwins_theme *);

/* return -1 if path is not long enough, even if you feed a empty string we should return a font */
int tw_find_font_path(const char *font_name, char *path, size_t path_size);


static inline int
tw_font_pt2px(int pt_size, int ppi)
{
	if (ppi < 0)
		ppi = 96;
	return (int) (ppi / 72.0 * pt_size);
}

static inline int
tw_font_px2pt(int px_size, int ppi)
{
	if (ppi < 0)
		ppi = 96;
	return (int) (72.0 * px_size / ppi);
}



unsigned char *load_image(const char *path, const enum wl_shm_format wlformat,
	   int width, int height, unsigned char *data);

//////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////geometry definition////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
struct point2d {
	unsigned int x;
	unsigned int y;
};

/**
 * @brief a rectangle area that supports scales.
 */
struct bbox {
	uint16_t x; uint16_t y;
	uint16_t w; uint16_t h;
	uint8_t s;
};

static inline bool
bbox_contain_point(const struct bbox *box, unsigned int x, unsigned int y)
{
	return ((x >= box->x) &&
		(x < box->x + box->w * box->s) &&
		(y >= box->y) &&
		(y < box->y + box->h * box->s));
}

static inline bool
bboxs_intersect(const struct bbox *ba, const struct bbox *bb)
{
	return (ba->x < bb->x + bb->w*bb->s) &&
		(ba->x + ba->w*ba->s > bb->x) &&
		(ba->y < bb->y + bb->h*bb->s) &&
		(ba->y + ba->h*ba->s > bb->y);
}

static inline unsigned long
bbox_area(const struct bbox *box)
{
	return box->w * box->s * box->h * box->s;
}

static inline struct bbox
make_bbox(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t s)
{
	return (struct bbox){x, y, w, h, s};
}

static inline struct bbox
make_bbox_origin(uint16_t w, uint16_t h, uint16_t s)
{
	return (struct bbox){0, 0, w, h, s};
}

//////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////APPSURFACE/////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

enum APP_SURFACE_TYPE {
	APP_BACKGROUND,
	APP_PANEL,
	APP_WIDGET,
	APP_LOCKER,
};

enum taiwins_modifier_mask {
	TW_NOMOD = 0,
	TW_ALT = 1,
	TW_CTRL = 2,
	TW_SUPER = 4,
	TW_SHIFT = 8,
};


struct wl_globals;
struct app_surface;
struct egl_env;

typedef void (*frame_t)(struct app_surface *, const struct app_event *e);

/**
 * /brief Abstract wl_surface container
 *
 * The design goal of the surface is to use app_surface as general wl_surface
 * handler for both buffer based application and graphics based
 * application. User no longer need to explictly call
 * wl_surface_damage/attach/commit. Instead, user will use the fixed routine
 * app_surface_frame to draw a frame.
 *
 * In the passive mode, the app_surface should draw on input. But there are case
 * you want to drive the frame, wayland provides this facility, then the input
 * is accumulated and the frame callback apply the input. Sadly, the appsurface
 * does not apply to this structure now. We will need the actual refactoring for
 * that.
 *
 * Also, you need to use the timer to us
 */
struct app_surface {
	//the structure to store wl_shell_surface, xdg_shell_surface or tw_ui
	struct wl_proxy *protocol;

	//geometry information
	struct bbox allocation;
	struct bbox pending_allocation;

	enum APP_SURFACE_TYPE type;

	struct wl_globals *wl_globals;
	struct wl_output *wl_output;
	struct wl_surface *wl_surface;
	bool need_animation;
	uint32_t last_serial;

	/* data holder */
	union {
		struct {
			struct shm_pool *pool;
			struct wl_buffer  *wl_buffer[2];
			bool dirty[2];
			bool committed[2];
			//add pixman_region to accumelate the damage
		};
		struct {
			/* we do not necessary need it but when it comes to
			 * surface destroy you will need it */
			EGLDisplay egldisplay;
			struct wl_egl_window *eglwin;
			EGLSurface eglsurface;
		};
		/**
		 * the parent pointer, means you will have to call the
		 * parent->frame function to do the work
		 */
		struct app_surface *parent;
		/* if we have an vulkan surface, it would be the same thing */
		struct {
			VkSurfaceKHR vksurf;
		};
	};

	frame_t do_frame;

	//destructor
	void (*destroy)(struct app_surface *);
	//XXX this user_data is controlled by implementation, do not set it!!!
	void *user_data;
};

/** TODO
 * new APIs for the appsurface
 */

/**
 * /brief clean start a new appsurface
 */
void app_surface_init(struct app_surface *surf, struct wl_surface *, struct wl_proxy *proxy,
	struct wl_globals *globals);

/**
 * /brief the universal release function
 */
void
app_surface_release(struct app_surface *surf);


/**
 * /brief request a frame for the appsurface
 *
 *  You can use this callback to start the animation sequence for the surface,
 *  so frame the next draw call, the app will be in animation. You can stop the
 *  animation by calling `app_surface_end_frame_request`. The function is
 *  intentioned to be used directly in the `frame_callback` kick start the
 *  animation.
 */
void app_surface_request_frame(struct app_surface *surf);

/**
 * /brief kick off the drawing for the surface
 *
 * user call this function to start drawing. It triggers the frames untils
 * app_surface_release is called.
 */
void app_surface_frame(struct app_surface *surf, bool anime);

/**
 * @brief start the resize of app surface.
 */
void app_surface_resize(struct app_surface *surf, unsigned int nw, unsigned int nh);

static inline void
app_surface_end_frame_request(struct app_surface *surf)
{
	surf->need_animation = false;
}



/**
 * /brief a helper function if you have egl_env
 *
 * I do not want to include this but there is a fixed way to allocate
 * `wl_egl_window` and `EGLSurface`, even with nuklear
 */
void app_surface_init_egl(struct app_surface *surf, struct egl_env *env);

void app_surface_clean_egl(struct app_surface *surf, struct egl_env *env);

cairo_format_t
translate_wl_shm_format(enum wl_shm_format format);

size_t
stride_of_wl_shm_format(enum wl_shm_format format);


static inline struct app_surface *
app_surface_from_wl_surface(struct wl_surface *s)
{
	return (struct app_surface *)wl_surface_get_user_data(s);
}


/**
 * @brief one of the implementation of app_surface
 *
 * In this case, the user_data is taken and used for callback, so we do not need
 * to allocate new memory, since you won't have extra space for ther other
 * user_data, expecting to embend app_surface in another data structure.
 *
 */
typedef void (*shm_buffer_draw_t)(struct app_surface *surf, struct wl_buffer *buffer,
				  struct bbox *geo);

void
shm_buffer_impl_app_surface(struct app_surface *surf, shm_buffer_draw_t draw_call,
			    const struct bbox geo);

/**
 * @brief we can expose part of shm_buffer implementation for any shm_pool
 * double buffer based implementation
 */
void shm_buffer_reallocate(struct app_surface *surf, const struct bbox *geo);

void shm_buffer_resize(struct app_surface *surf, const struct app_event *e);

void shm_buffer_destroy_app_surface(struct app_surface *surf);


/**
 * /brief second implementation we provide here is the parent surface
 */
void embeded_impl_app_surface(struct app_surface *surf, struct app_surface *parent,
			      const struct bbox geo);



#ifdef __cplusplus
}
#endif



#endif /* EOF */
