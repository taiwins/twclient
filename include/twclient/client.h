/*
 * client.h - general client header
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

#ifndef TW_CLIENT_H
#define TW_CLIENT_H

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <poll.h>

#include <wayland-client.h>
#include <sys/timerfd.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <wayland-util.h>
#include <wayland-client.h>
#include <wayland-cursor.h>

#include "event_queue.h"
#include "ui.h"

#ifdef __cplusplus
extern "C" {
#endif

struct tw_theme;


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

/**
 * global context for one application (can be shared with multiple surface)
 *
 * It contains almost everything and app_surface should have a reference of
 * this.
 *
 * This struct is indeed rather big (currently 488 bytes), we would want to fit
 * in L1 Cache
 */
struct tw_globals {
	struct wl_compositor *compositor;
	struct wl_display *display;
	struct wl_shm *shm;
	struct wl_data_device_manager *wl_data_device_manager;
	enum wl_shm_format buffer_format;
	struct wl_inputs {
		struct wl_seat *wl_seat;
		struct wl_keyboard *wl_keyboard;
		struct wl_pointer *wl_pointer;
		struct wl_touch *wl_touch;
		struct wl_data_device *wl_data_device;
		struct wl_data_offer *wl_data_offer;
		struct itimerspec repeat_info;
		char name[64]; //seat name
		uint32_t millisec;
		uint32_t serial;
		uint32_t mime_offered;
		//keyboard
		struct {
			struct xkb_context *kcontext;
			struct xkb_keymap  *keymap;
			struct xkb_state   *kstate;
			struct wl_surface *keyboard_focused;
			//state
			uint32_t modifiers;
			bool key_pressed;
			xkb_keycode_t keycode;
			xkb_keysym_t keysym;
		};
		//pointer, touch use this as well
		struct {
			size_t cursor_size;
			char cursor_theme_name[64];
			struct wl_cursor *cursor;
			struct wl_cursor_theme *cursor_theme;
			struct wl_surface *cursor_surface;
			struct wl_buffer *cursor_buffer;
			struct wl_callback_listener cursor_done_listener;
			struct wl_surface *pointer_focused; //the surface that cursor is on
			bool cursor_set;
			//state
			uint32_t btn;
			bool btn_pressed;
			uint32_t enter_serial;
			uint32_t pointer_events;
			int16_t sx, sy; //screen coordinates
			int16_t dx, dy; //relative motion
			uint32_t dx_axis, dy_axis; //axis advance
		};

	} inputs;

	//application theme settings
	const struct tw_theme *theme;
	struct tw_event_queue event_queue;
};


/**
 * @brief taking care of common wl_globals like wl_shm, wl_seat,
 *  wl_data_device_manager
 *
 * You would call this function in your `wl_registry->globals` callback, you try
 * to register the specific globals interests you, then call this function for
 * the rest
*/
int
tw_globals_announce(struct tw_globals *globals,
                    struct wl_registry *wl_registry,
                    uint32_t name,
                    const char *interface,
                    uint32_t version);
void
tw_globals_init(struct tw_globals *globals, struct wl_display *display);

void
tw_globals_release(struct tw_globals *globals);

static inline void
tw_globals_dispatch_event_queue(struct tw_globals *globals)
{
	tw_event_queue_run(&globals->event_queue);
}

bool
is_shm_format_valid(uint32_t format);

void
tw_globals_receive_data_offer(struct wl_data_offer *offer,
                              struct wl_surface *surface,
                              bool drag_n_drop);
void
tw_globals_change_cursor(struct tw_globals *globals, const char *type);

void
tw_globals_reload_cursor_theme(struct tw_globals *globals);


#ifdef __cplusplus
}
#endif



#endif /* EOF */
