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
#include <sequential.h>
#include "ui_event.h"
#include "theme.h"

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////Application style definition/////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
struct shm_pool;
extern const struct taiwins_theme_color taiwins_dark_theme;

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
struct wl_globals;
struct app_surface;
struct egl_env;
struct app_surface;

enum app_surface_type {
	APP_SURFACE_BACKGROUND,
	APP_SURFACE_PANEL,
	APP_SURFACE_WIDGET, /* no header, max/minimize button, no title */
	APP_SURFACE_LOCKER,
	APP_SURFACE_APP, /* desktop like application with panel, button, title */
};

enum taiwins_modifier_mask {
	TW_NOMOD = 0,
	TW_ALT = 1,
	TW_CTRL = 2,
	TW_SUPER = 4,
	TW_SHIFT = 8,
};

enum app_surface_flag {
	APP_SURFACE_NORESIZABLE = 1 << 0,
	/* complex gui with multiple sub window inside */
	APP_SURFACE_COMPOSITE = 1 << 1,
	APP_SURFACE_NOINPUT = 1 << 2,
};

enum app_surface_mime_type {
	MIME_TYPE_TEXT = 0,
	MIME_TYPE_AUDIO = 1,
	MIME_TYPE_VIDEO = 2,
	MIME_TYPE_IMAGE = 3,
	MIME_TYPE_APPLICATION = 4,
	MIME_TYPE_MAX = 5,
};

typedef const char * app_mime[MIME_TYPE_MAX];


typedef void (*frame_t)(struct app_surface *, const struct app_event *e);


struct app_event_filter {
	struct wl_list link;
	enum app_event_type type;
	bool (*intercept)(struct app_surface *, const struct app_event *e);
};


/**
 * /brief Abstract wl_surface container
 *
 * @brief Abstract wl_surface container
 *
 * The design goal of the surface is to use app_surface as general wl_surface
 * handler to take care the tedious work and also provides necessary
 * flexibilities. When one backend implements app_surface, usually user has very
 * little to do.
 *
 * By default app_surface runs in passive mode which it only runs on events,
 * default action is the frame callback (if you do not have any grab) and
 * backends usually take care of the frame callback. Users can intercept those
 * events by providing custom grabs(in rare cases).
 */
struct app_surface {
	enum app_surface_type type;
	uint32_t flags;

	//geometry information
	struct bbox allocation;
	struct bbox pending_allocation;

	app_mime known_mimes;
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
	struct wl_list filter_head;
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
 * @brief clean start a new appsurface
 */
void app_surface_init(struct app_surface *app, struct wl_surface *surf,
		      struct wl_globals *g,
		      enum app_surface_type type, const uint32_t flags);

/**
 * @breif create a default desktop like application
 */
void app_surface_init_default(struct app_surface *, struct wl_surface *,
			      struct wl_globals *g);

/**
 * @brief the universal release function
 *
 * It releases all the resources but user is responsible for destroy the proxy
 * first before calling this method
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
void app_surface_resize(struct app_surface *surf, uint32_t nw, uint32_t nh, uint32_t ns);

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
	return s ? (struct app_surface *)wl_surface_get_user_data(s) :
		NULL;
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
