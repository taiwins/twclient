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

//////////////////////////////////////////////////////////////////////////
/////////////////////////////Pointer listeners////////////////////////////
//////////////////////////////////////////////////////////////////////////
enum POINTER_EVENT_CODE {
	//focus, unfocus
	POINTER_ENTER = 1 << 0,
	POINTER_LEAVE = 1 << 1,
	//motion
	POINTER_MOTION = 1 << 2,
	//btn
	POINTER_BTN = 1 << 3,
	//AXIS
	POINTER_AXIS = 1 << 4,
};

static inline void
pointer_event_clean(struct wl_globals *globals)
{
	//we dont change the pointer state here since you may want to query it
	//later
	globals->inputs.dx_axis = 0;
	globals->inputs.dy_axis = 0;
	globals->inputs.pointer_events = 0;
}



static void
pointer_cursor_done(void *data, struct wl_callback *callback, uint32_t callback_data)
{
	//you do necessary need this
	/* fprintf(stderr, "cursor set!\n"); */
	wl_callback_destroy(callback);
}

static void
pointer_enter(void *data,
	      struct wl_pointer *wl_pointer,
	      uint32_t serial,
	      struct wl_surface *surface,
	      wl_fixed_t surface_x,
	      wl_fixed_t surface_y)
{
	struct wl_globals *globals = data;
	struct app_surface *app = app_surface_from_wl_surface(surface);
	globals->inputs.pointer_focused = surface;
	app->wl_globals = globals;

	globals->inputs.pointer_events = POINTER_ENTER;
	//this works only if you have one surface, we may need to set cursor
	//every time
	static bool cursor_set = false;
	if (!cursor_set) {
		struct wl_callback *callback =
			wl_surface_frame(globals->inputs.cursor_surface);
		globals->inputs.cursor_done_listener.done = pointer_cursor_done;
		wl_callback_add_listener(callback, &globals->inputs.cursor_done_listener, NULL);
		//give a role to the the cursor_surface
		wl_pointer_set_cursor(wl_pointer,
				      serial, globals->inputs.cursor_surface,
				      globals->inputs.w/2, globals->inputs.w/2);
		wl_surface_attach(globals->inputs.cursor_surface,
				  globals->inputs.cursor_buffer, 0, 0);
		wl_surface_damage(globals->inputs.cursor_surface, 0, 0,
				  globals->inputs.w, globals->inputs.w);
		wl_surface_commit(globals->inputs.cursor_surface);
	}
}

static void
pointer_leave(void *data,
	      struct wl_pointer *wl_pointer,
	      uint32_t serial,
	      struct wl_surface *surface)
{
	struct wl_globals *globals = data;
	globals->inputs.pointer_focused = NULL;
}

static void
pointer_motion(void *data,
	       struct wl_pointer *wl_pointer,
	       uint32_t serial,
	       wl_fixed_t x,
	       wl_fixed_t y)
{
	struct wl_globals *globals = data;
	globals->inputs.sx = wl_fixed_to_int(x);
	globals->inputs.sy = wl_fixed_to_int(y);
	globals->inputs.pointer_events |=  POINTER_MOTION;
}


static void
pointer_button(void *data,
	       struct wl_pointer *wl_pointer,
	       uint32_t serial,
	       uint32_t time,
	       uint32_t button,
	       uint32_t state)
{
	struct wl_globals *globals = data;
	globals->inputs.millisec = time;
	globals->inputs.btn_pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);
	globals->inputs.btn = button;
	globals->inputs.pointer_events |= POINTER_BTN;
}

static void
pointer_axis(void *data,
	     struct wl_pointer *wl_pointer,
	     uint32_t time,
	     uint32_t axis, //vertial or horizental
	     wl_fixed_t value)
{
	struct wl_globals *globals = data;
	globals->inputs.millisec = time;

	globals->inputs.dx_axis += (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) ?
		wl_fixed_to_int(value) : 0;
	globals->inputs.dy_axis += (axis == WL_POINTER_AXIS_VERTICAL_SCROLL) ?
		wl_fixed_to_int(value) : 0;
	globals->inputs.pointer_events |= POINTER_AXIS;
}

static void
pointer_axis_src(void *data,
		 struct wl_pointer *wl_pointer, uint32_t src)
{
	//distinguish the type of wheel or pad, we do not handle that
}

static void
pointer_axis_stop(void *data,
		  struct wl_pointer *wl_pointer,
		  uint32_t time, uint32_t axis)
{
	struct wl_globals *globals = data;
	globals->inputs.millisec = time;
	//we do not implement kinect scrolling
}

static void
pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer,
		     uint32_t axis, int32_t discrete)
{
	//the discrete representation of the axis event, for example(a travel of
	//102 pixel represent) 2 times of wheel scrolling, the axis event is
	//more useful.
}

//once a frame event generate, we need to accumelate all previous pointer events
//and then send them all. Call frame on them, all though you should not receive
//more than one type of pointer event
static void
pointer_frame(void *data,
	      struct wl_pointer *wl_pointer)
{
	//we need somehow generate a timestamp
	struct app_event e;
	struct wl_globals *globals = data;

	if (!globals->inputs.pointer_events)
		return;

	struct wl_surface *focused = globals->inputs.pointer_focused;
	struct app_surface *appsurf = (focused) ?
		app_surface_from_wl_surface(focused) : NULL;
	if (!appsurf || !appsurf->do_frame)
		return;

	e.time = globals->inputs.millisec;
	uint32_t event = globals->inputs.pointer_events;
	if (event & POINTER_AXIS) {
		e.type = TW_POINTER_AXIS;
		e.axis.mod = globals->inputs.modifiers;
		e.axis.dx = globals->inputs.dx_axis;
		e.axis.dy = globals->inputs.dy_axis;
	}
	if (event & POINTER_MOTION) {
		e.type = TW_POINTER_MOTION;
		e.ptr.x = globals->inputs.sx;
		e.ptr.y = globals->inputs.sy;
		e.ptr.mod = globals->inputs.modifiers;
		appsurf->do_frame(appsurf, &e);
	}
	if (event & POINTER_BTN) {
		e.type = TW_POINTER_BTN;
		e.ptr.btn = globals->inputs.btn;
		e.ptr.x = globals->inputs.sx;
		e.ptr.y = globals->inputs.sy;;
		e.ptr.state = globals->inputs.btn_pressed;
		e.ptr.mod = globals->inputs.modifiers;
		appsurf->do_frame(appsurf, &e);
	}
	pointer_event_clean(globals);
}


//the DEFAULT grab of pointer, other type grab(resize), or types like
//data_device. There should be other type of grab
static struct wl_pointer_listener pointer_listener = {
	.enter = pointer_enter,
	.leave = pointer_leave,
	.motion = pointer_motion,
	.frame = pointer_frame,
	.button = pointer_button,
	.axis  = pointer_axis,
	.axis_source = pointer_axis_src,
	.axis_stop = pointer_axis_stop,
	.axis_discrete = pointer_axis_discrete,
};



void
tw_pointer_init(struct wl_pointer *wl_pointer, struct wl_globals *globals)
{
	if (!globals)
		return;

	wl_pointer_add_listener(wl_pointer, &pointer_listener, globals);
	globals->inputs.cursor_theme = wl_cursor_theme_load("whiteglass", 32, globals->shm);
	globals->inputs.w = 32;
	globals->inputs.cursor = wl_cursor_theme_get_cursor(
		globals->inputs.cursor_theme, "plus");
	globals->inputs.cursor_surface =
		wl_compositor_create_surface(globals->compositor);
	globals->inputs.cursor_buffer =
		wl_cursor_image_get_buffer(globals->inputs.cursor->images[0]);
	pointer_event_clean(globals);
}



void
tw_pointer_destroy(struct wl_pointer *wl_pointer)
{
	struct wl_globals *globals = wl_pointer_get_user_data(wl_pointer);
	wl_cursor_theme_destroy(globals->inputs.cursor_theme);
	wl_surface_destroy(globals->inputs.cursor_surface);

	wl_pointer_destroy(wl_pointer);
}
