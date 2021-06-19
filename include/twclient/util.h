/*
 * util.h - general client header
 *
 * Copyright (c) 2021 Xichen Zhou
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef TW_CLIENT_UTIL_H
#define TW_CLIENT_UTIL_H

#include <stdbool.h>
#include <wayland-client.h>


#ifdef __cplusplus
extern "C" {
#endif


typedef void (*tw_global_func_t)(struct wl_proxy *);

struct tw_global {
	struct wl_list link; /* tw_globals:globals */
	struct wl_proxy *object; /* correspond to a global object */
	tw_global_func_t global_remove;
};


static inline void
tw_global_init(struct tw_global *global, struct wl_proxy *proxy,
               tw_global_func_t global_remove)
{
	wl_list_init(&global->link);
	global->object = proxy;
}


/**
 * @brief classic observer pattern,
 *
 * much like wl_signal wl_listener, but not available for wayland clients
 */
struct tw_signal {
	struct wl_list head;
};

/**
 * @brief classic observer pattern,
 *
 * much like wl_signal wl_listener, but not available for wayland clients
 */
struct tw_observer {
	struct wl_list link;
	void (*update)(struct tw_observer *listener, void *data);
};

static inline void
tw_signal_init(struct tw_signal *signal)
{
	wl_list_init(&signal->head);
}

static inline void
tw_observer_init(struct tw_observer *observer,
                 void (*notify)(struct tw_observer *observer, void *data))
{
	wl_list_init(&observer->link);
	observer->update = notify;
}

static inline void
tw_observer_add(struct tw_signal *signal, struct tw_observer *observer)
{
	wl_list_insert(&signal->head, &observer->link);
}

static inline void
tw_signal_emit(struct tw_signal *signal, void *data)
{
	struct tw_observer *observer;
	wl_list_for_each(observer, &signal->head, link)
		observer->update(observer, data);
}

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

#ifdef __cplusplus
}
#endif


#endif /* EOF */
