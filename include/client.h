#ifndef TWCLIENT_H
#define TWCLIENT_H

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <pthread.h>
#include <poll.h>
#include <GL/gl.h>
//hopefully this shit is declared
#include <GL/glext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <wayland-client.h>
#include <sys/timerfd.h>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <wayland-util.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "event_queue.h"
#include "shmpool.h"
#include "ui.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * global context for one application (can be shared with multiple surface)
 *
 * It contains almost everything and app_surface should have a reference of
 * this.
 *
 * This struct is indeed rather big (currently 488 bytes), we would want to fit
 * in L1 Cache
 */
struct wl_globals {
	struct wl_shm *shm;
	enum wl_shm_format buffer_format;
	struct wl_compositor *compositor;
	struct wl_display *display;
	struct wl_inputs {
		struct wl_seat *wl_seat;
		struct wl_keyboard *wl_keyboard;
		struct wl_pointer *wl_pointer;
		struct wl_touch *wl_touch;
		struct itimerspec repeat_info;
		char name[64]; //seat name
		uint32_t millisec;
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
		//pointer
		struct {
			char cursor_theme_name[64];
			struct wl_cursor *cursor;
			struct wl_cursor_theme *cursor_theme;
			struct wl_surface *cursor_surface;
			struct wl_buffer *cursor_buffer;
			struct wl_callback_listener cursor_done_listener;
			struct wl_surface *pointer_focused; //the surface that cursor is on
			//state
			uint32_t pointer_events;
			uint32_t btn;
			bool btn_pressed;
			short sx, sy; //screen coordinates
			uint32_t dx_axis, dy_axis; //axis advance
			short w;
		};
		//touch
		struct {
			bool touch_down;
			short tsx, tsy; //touch point
		};

	} inputs;

	//application theme settings
	struct taiwins_theme theme;
	struct tw_event_queue event_queue;
};


/* here you need a data structure that glues the anonoymous_buff and wl_buffer wl_shm_pool */
int wl_globals_announce(struct wl_globals *globals,
			struct wl_registry *wl_registry,
			uint32_t name,
			const char *interface,
			uint32_t version);

//unless you have better method to setup the theme, I think you can simply set it up my hand

/* Constructor */
void wl_globals_init(struct wl_globals *globals, struct wl_display *display);
/* destructor */
void wl_globals_release(struct wl_globals *globals);

static inline void
wl_globals_dispatch_event_queue(struct wl_globals *globals)
{
	tw_event_queue_run(&globals->event_queue);
}

bool is_shm_format_valid(uint32_t format);

//we can have several input code
void wl_globals_get_input_state(const struct wl_globals *globals);
//you will have a few functions


#ifdef __cplusplus
}
#endif



#endif /* EOF */
