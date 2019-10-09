#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>

#include <linux/input.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <wayland-client.h>

#include <ui.h>
#include <client.h>

extern void _app_surface_run_frame(struct app_surface *surf, const struct app_event *e);

static inline uint32_t
tw_mod_mask_from_xkb_state(struct xkb_state *state)
{
	uint32_t mask = TW_NOMOD;
	if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE))
		mask |= TW_ALT;
	if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE))
		mask |= TW_CTRL;
	if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE))
		mask |= TW_SUPER;
	if (xkb_state_mod_name_is_active(state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE))
		mask |= TW_SHIFT;
	return mask;
}

static inline xkb_keycode_t
kc_linux2xkb(uint32_t kc_linux)
{
	//this should only work on x11, but very weird it works all the time
	return kc_linux+8;
}

static int
handle_key_repeat(struct tw_event *e, int fd)
{
	struct app_surface *surf = e->data;
	struct wl_globals *globals = surf->wl_globals;

	if (globals && surf->do_frame &&
	    globals->inputs.key_pressed &&
	    e->arg.u == globals->inputs.keysym) {
		//there are two ways to generate press and release

		//1) giving two do_frame event, but in this way we may get
		//blocked(over commit)

		//2) consecutively generate on/off. Thanks god the tw_event is
		//modifable
		struct app_event ae = {
			.type = TW_KEY_BTN,
			.time = globals->inputs.millisec,
			.key = {
				.code = globals->inputs.keycode,
				.sym = globals->inputs.keysym,
				.mod = globals->inputs.modifiers,
				.state = false,
			},
		};
		_app_surface_run_frame(surf, &ae);
		ae.key.state = true;
		_app_surface_run_frame(surf, &ae);
		return TW_EVENT_NOOP;
	} else
		return TW_EVENT_DEL;
}

static inline bool
is_repeat_info_valid(const struct itimerspec ri)
{
	//add timer for the
	return  (ri.it_value.tv_nsec || ri.it_value.tv_sec) &&
		(ri.it_interval.tv_sec || ri.it_interval.tv_nsec);
}

static void
handle_key(void *data,
	   struct wl_keyboard *wl_keyboard,
	   uint32_t serial,
	   uint32_t time,
	   uint32_t key,
	   uint32_t state)
{
	struct wl_globals *globals = (struct wl_globals *)data;
	struct wl_surface *focused = globals->inputs.keyboard_focused;
	struct app_surface *appsurf = (focused) ? app_surface_from_wl_surface(focused) : NULL;

	globals->inputs.millisec = time;
	globals->inputs.keycode = kc_linux2xkb(key);
	globals->inputs.keysym  = xkb_state_key_get_one_sym(globals->inputs.kstate,
							    globals->inputs.keycode);
	globals->inputs.key_pressed = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
	if (!appsurf || !appsurf->do_frame)
		return;

	struct app_event e = {
		.time = time,
		.type = TW_KEY_BTN,
		.key = {
			.mod = globals->inputs.modifiers,
			.code = globals->inputs.keycode,
			.sym = globals->inputs.keysym,
			.state = globals->inputs.key_pressed,
		},
	};

	if (is_repeat_info_valid(globals->inputs.repeat_info) &&
		globals->inputs.key_pressed) {
		struct tw_event repeat_event = {
			.data = appsurf,
			.cb = handle_key_repeat,
			.arg = {
				.u = globals->inputs.keysym,
			},
		};
		tw_event_queue_add_timer(&globals->event_queue,
					 &globals->inputs.repeat_info,
					 &repeat_event);
	}
	_app_surface_run_frame(appsurf, &e);
}

static
void handle_modifiers(void *data,
		      struct wl_keyboard *wl_keyboard,
		      uint32_t serial,
		      uint32_t mods_depressed, //which key
		      uint32_t mods_latched,
		      uint32_t mods_locked,
		      uint32_t group)
{
	struct wl_globals *globals = (struct wl_globals *)data;
	xkb_state_update_mask(globals->inputs.kstate,
			      mods_depressed, mods_latched, mods_locked, 0, 0, group);
	globals->inputs.modifiers = tw_mod_mask_from_xkb_state(globals->inputs.kstate);
}

static void
handle_keymap(void *data, struct wl_keyboard *wl_keyboard,
	      uint32_t format,
	      int32_t fd,
	      uint32_t size)
{
	struct wl_globals *globals = (struct wl_globals *)data;

	if (globals->inputs.kcontext)
		xkb_context_unref(globals->inputs.kcontext);
	void *addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
	globals->inputs.kcontext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	globals->inputs.keymap = xkb_keymap_new_from_string(globals->inputs.kcontext,
							    (const char *)addr,
							    XKB_KEYMAP_FORMAT_TEXT_V1,
							    XKB_KEYMAP_COMPILE_NO_FLAGS);
	globals->inputs.kstate = xkb_state_new(globals->inputs.keymap);
	munmap(addr, size);
}


//shit, this must be how you implement delay.
static void
handle_repeat_info(void *data,
		   struct wl_keyboard *wl_keyboard,
		   int32_t rate,
		   int32_t delay)
{
	struct wl_globals *globals = (struct wl_globals *)data;
	globals->inputs.repeat_info = (struct itimerspec) {
		.it_value = {
			.tv_sec = 0,
			.tv_nsec = delay * 1000000,
		},
		.it_interval = {
			.tv_sec = 0,
			.tv_nsec = rate * 1000000,
		},
	};
}

static void
handle_keyboard_enter(void *data,
		      struct wl_keyboard *wl_keyboard,
		      uint32_t serial,
		      struct wl_surface *surface,
		      struct wl_array *keys)
{
	struct wl_globals *globals = (struct wl_globals *)data;
	globals->inputs.keyboard_focused = surface;
	struct app_surface *app = app_surface_from_wl_surface(surface);
	if (app)
		app->wl_globals = globals;
	fprintf(stderr, "keyboard has\n");
}

static void
handle_keyboard_leave(void *data,
		    struct wl_keyboard *wl_keyboard,
		    uint32_t serial,
		    struct wl_surface *surface)
{
	struct wl_globals *globals = (struct wl_globals *)data;
	globals->inputs.keyboard_focused = NULL;
	fprintf(stderr, "keyboard lost focus\n");
}

static
struct wl_keyboard_listener keyboard_listener = {
	.key = handle_key,
	.modifiers = handle_modifiers,
	.enter = handle_keyboard_enter,
	.leave = handle_keyboard_leave,
	.keymap = handle_keymap,
	.repeat_info = handle_repeat_info,

};


void tw_keyboard_init(struct wl_keyboard *keyboard, struct wl_globals *globals)
{
	wl_keyboard_add_listener(globals->inputs.wl_keyboard, &keyboard_listener, globals);
}

void tw_keyboard_destroy(struct wl_keyboard *keyboard)
{
	struct wl_globals *globals = wl_keyboard_get_user_data(keyboard);
	if (globals->inputs.kstate) {
		xkb_state_unref(globals->inputs.kstate);
		globals->inputs.kstate = NULL;
	}
	if (globals->inputs.keymap) {
		xkb_keymap_unref(globals->inputs.keymap);
		globals->inputs.keymap = NULL;
	}
	if (globals->inputs.kcontext) {
		xkb_context_unref(globals->inputs.kcontext);
		globals->inputs.kcontext = NULL;
	}

}
