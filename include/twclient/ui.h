/*
 * ui.h - taiwins client gui header
 *
 * Copyright (c) 2019-2021 Xichen Zhou
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the
 * Free Software Foundation; either version 2.1 of the License, or (at your
 * option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */

#ifndef TW_UI_H
#define TW_UI_H

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <EGL/egl.h>
#include <wayland-egl.h>
#include <stdio.h>
#include <stdbool.h>

//doesnt support jpeg in this way, but there is a cairo-jpeg project
#include <cairo/cairo.h>
#include <wayland-client.h>
#include "ui_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
 * client style
 ******************************************************************************/

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

/*******************************************************************************
 * taiwins geometry
 ******************************************************************************/
struct tw_point2d {
	unsigned int x;
	unsigned int y;
};

/**
 * @brief a rectangle area that supports scales.
 */
struct tw_bbox {
	uint16_t x; uint16_t y;
	uint16_t w; uint16_t h;
	uint8_t s;
};

static inline bool
tw_bbox_contain_point(const struct tw_bbox *box, unsigned int x, unsigned int y)
{
	return ((x >= box->x) &&
	        (x < (unsigned)(box->x + box->w * box->s)) &&
		(y >= box->y) &&
	        (y < (unsigned)(box->y + box->h * box->s)));
}

static inline bool
tw_bboxs_intersect(const struct tw_bbox *ba, const struct tw_bbox *bb)
{
	return (ba->x < bb->x + bb->w*bb->s) &&
		(ba->x + ba->w*ba->s > bb->x) &&
		(ba->y < bb->y + bb->h*bb->s) &&
		(ba->y + ba->h*ba->s > bb->y);
}

static inline unsigned long
tw_bbox_area(const struct tw_bbox *box)
{
	return box->w * box->s * box->h * box->s;
}

static inline struct tw_bbox
tw_make_bbox(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t s)
{
	return (struct tw_bbox){x, y, w, h, s};
}

static inline struct tw_bbox
tw_make_bbox_origin(uint16_t w, uint16_t h, uint16_t s)
{
	return (struct tw_bbox){0, 0, w, h, s};
}

/*******************************************************************************
 * app surface
 ******************************************************************************/
struct tw_globals;
struct tw_appsurf;
struct tw_egl_env;
struct tw_shm_pool;

enum tw_appsurf_type {
	TW_APPSURF_BACKGROUND,
	TW_APPSURF_PANEL,
	TW_APPSURF_WIDGET, /* no header, max/minimize button, no title */
	TW_APPSURF_LOCKER,
	TW_APPSURF_APP, /* desktop like application with panel, button, title */
};

enum taiwins_modifier_mask {
	TW_NOMOD = 0,
	TW_ALT = 1,
	TW_CTRL = 2,
	TW_SUPER = 4,
	TW_SHIFT = 8,
};

enum tw_appsurf_flag {
	TW_APPSURF_NORESIZABLE = 1 << 0,
	/* complex gui with multiple sub window inside */
	TW_APPSURF_COMPOSITE = 1 << 1,
	TW_APPSURF_NOINPUT = 1 << 2,
};

enum tw_appsurf_mime_type {
	TW_MIME_TYPE_TEXT = 0,
	TW_MIME_TYPE_AUDIO = 1,
	TW_MIME_TYPE_VIDEO = 2,
	TW_MIME_TYPE_IMAGE = 3,
	TW_MIME_TYPE_APPLICATION = 4,
	TW_MIME_TYPE_MAX = 5,
};

typedef const char * app_mime[TW_MIME_TYPE_MAX];

typedef void (*frame_t)(struct tw_appsurf *, const struct tw_app_event *e);

struct tw_app_event_filter {
	struct wl_list link;
	enum tw_app_event_type type;
	bool (*intercept)(struct tw_appsurf *, const struct tw_app_event *e);
};

/**
 * /brief Abstract wl_surface container
 *
 * @brief Abstract wl_surface container
 *
 * The design goal of the surface is to use tw_appsurf as general wl_surface
 * handler to take care the tedious work and also provides necessary
 * flexibilities. When one backend implements tw_appsurf, usually user has very
 * little to do.
 *
 * By default tw_appsurf runs in passive mode which it only runs on events,
 * default action is the frame callback (if you do not have any grab) and
 * backends usually take care of the frame callback. Users can intercept those
 * events by providing custom grabs(in rare cases).
 */
struct tw_appsurf {
	enum tw_appsurf_type type;
	uint32_t flags;

	//geometry information
	struct tw_bbox allocation;
	struct tw_bbox pending_allocation;

	app_mime known_mimes;
	struct tw_globals *tw_globals;
	struct wl_output *wl_output;
	struct wl_surface *wl_surface;
	bool need_animation;
	uint32_t last_serial;

	/* data holder */
	union {
		struct {
			struct tw_shm_pool *pool;
			struct wl_buffer  *wl_buffer[2];
			bool dirty[2];
			bool committed[2];
			//add pixman_region to accumelate the damage
		};
		struct {
			/* we do not necessary need it but when it comes to
			 * surface destroy you will need it */
			EGLContext eglcontext;
			EGLDisplay egldisplay;
			struct wl_egl_window *eglwin;
			EGLSurface eglsurface;
		};
		/**
		 * the parent pointer, means you will have to call the
		 * parent->frame function to do the work
		 */
		struct tw_appsurf *parent;
	};
	struct wl_list filter_head;
	frame_t do_frame; /**< note that the callback is called mutiple times
	                   * during one page filp */

	//destructor
	void (*destroy)(struct tw_appsurf *);
	/** this user_data is controlled by implementation, do not set it */
	void *user_data;
};

/**
 * @brief clean start a new appsurface
 */
void
tw_appsurf_init(struct tw_appsurf *app, struct wl_surface *surf,
                struct tw_globals *g,
                enum tw_appsurf_type type, const uint32_t flags);
/**
 * @breif create a default desktop like application
 */
void
tw_appsurf_init_default(struct tw_appsurf *, struct wl_surface *,
                        struct tw_globals *g);
/**
 * @brief the universal release function
 *
 * It releases all the resources but user is responsible for destroy the proxy
 * first before calling this method
 */
void
tw_appsurf_release(struct tw_appsurf *surf);

/**
 * @brief request a frame for the appsurface
 *
 *  You can use this callback to start the animation sequence for the surface,
 *  so frame the next draw call, the app will be in animation. You can stop the
 *  animation by calling `tw_appsurf_end_frame_request`. The function is
 *  intentioned to be used directly in the `frame_callback` kick start the
 *  animation.
 */
void
tw_appsurf_request_frame(struct tw_appsurf *surf);

/**
 * @brief request a event to run at next frame.
 *
 * as compared to idle events, this callback could help reducing the amount of
 * unecessary requests.
 */
void
tw_appsurf_request_frame_event(struct tw_appsurf *surf,
                               struct tw_app_event *event);

/**
 * @brief kick off the drawing for the surface
 *
 * user call this function to start drawing. It triggers the frames untils
 * tw_appsurf_release is called.
 */
void
tw_appsurf_frame(struct tw_appsurf *surf, bool anime);

/**
 * @brief start the resize of app surface.
 */
void
tw_appsurf_resize(struct tw_appsurf *surf, uint32_t nw, uint32_t nh,
                  uint32_t ns);

static inline void
tw_appsurf_end_frame_request(struct tw_appsurf *surf)
{
	surf->need_animation = false;
}

static inline struct tw_appsurf *
tw_appsurf_from_wl_surface(struct wl_surface *s)
{
	return s ? (struct tw_appsurf *)wl_surface_get_user_data(s) :
		NULL;
}

/**
 * @brief a helper function if you have egl_env
 *
 * I do not want to include this but there is a fixed way to allocate
 * `wl_egl_window` and `EGLSurface`, even with nuklear
 */
void
tw_appsurf_init_egl(struct tw_appsurf *surf, struct tw_egl_env *env);

void
tw_appsurf_clean_egl(struct tw_appsurf *surf, struct tw_egl_env *env);


/**
 * @brief one of the implementation of tw_appsurf
 *
 * In this case, the user_data is taken and used for callback, so we do not need
 * to allocate new memory, since you won't have extra space for ther other
 * user_data, expecting to embend tw_appsurf in another data structure.
 *
 */
typedef void (*tw_shm_buffer_draw_t)(struct tw_appsurf *surf,
                                     struct wl_buffer *buffer,
                                     struct tw_bbox *geo);

void
tw_shm_buffer_impl_app_surface(struct tw_appsurf *surf,
                               tw_shm_buffer_draw_t draw_call,
                               const struct tw_bbox geo);

/**
 * @brief we can expose part of shm_buffer implementation for any shm_pool
 * double buffer based implementation
 */
bool
tw_shm_buffer_reallocate(struct tw_appsurf *surf, const struct tw_bbox *geo);

void
tw_shm_buffer_resize(struct tw_appsurf *surf, const struct tw_app_event *e);

void
tw_shm_buffer_destroy_app_surface(struct tw_appsurf *surf);


typedef void (*tw_eglwin_draw_t)(struct tw_appsurf *surf, struct tw_bbox *geo);

void
tw_eglwin_impl_app_surface(struct tw_appsurf *surf, tw_eglwin_draw_t draw_call,
                           const struct tw_bbox geo, struct tw_egl_env *env);

void
tw_eglwin_resize(struct tw_appsurf *surf, const struct tw_app_event *e);

/**
 * /brief second implementation we provide here is the parent surface
 */
void
tw_embeded_impl_app_surface(struct tw_appsurf *surf, struct tw_appsurf *parent,
                            const struct tw_bbox geo);

cairo_format_t
tw_translate_wl_shm_format(enum wl_shm_format format);

size_t
tw_stride_of_wl_shm_format(enum wl_shm_format format);


#ifdef __cplusplus
}
#endif



#endif /* EOF */
