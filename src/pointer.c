/*
 * pointer.c - taiwins client pointer handling functions
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
#include <wayland-cursor.h>

#include <twclient/ui.h>
#include <twclient/client.h>

/******************************************************************************
 * pointer listeners
 *****************************************************************************/

static struct wl_pointer_listener pointer_listener = {0};
static void default_pointer_grab(struct wl_pointer_listener *);
static void resize_pointer_grab(struct wl_pointer_listener *);
extern void _tw_appsurf_run_frame(struct tw_appsurf *surf,
                                  const struct tw_app_event *e);

enum POINTER_EVENT_CODE {
  // focus, unfocus
  POINTER_ENTER = 1 << 0,
  POINTER_LEAVE = 1 << 1,
  // motion
  POINTER_MOTION = 1 << 2,
  // btn
  POINTER_BTN = 1 << 3,
  // AXIS
  POINTER_AXIS = 1 << 4,
};

static inline void
pointer_event_clean(struct tw_globals *globals)
{
	//we dont change the pointer state here since you may want to query it
	//later
	globals->inputs.dx_axis = 0;
	globals->inputs.dy_axis = 0;
	globals->inputs.pointer_events = 0;
}

static void
pointer_cursor_done(void *data, struct wl_callback *callback,
                    uint32_t callback_data)
{
	wl_callback_destroy(callback);
}

static void
pointer_set_cursor(struct wl_pointer *wl_pointer)
{
	struct tw_globals *globals = wl_pointer_get_user_data(wl_pointer);
	struct wl_callback *callback =
		wl_surface_frame(globals->inputs.cursor_surface);
	globals->inputs.cursor_done_listener.done = pointer_cursor_done;
	wl_callback_add_listener(callback, &globals->inputs.cursor_done_listener, NULL);
	//give a role to the the cursor_surface
	wl_surface_attach(globals->inputs.cursor_surface,
			  globals->inputs.cursor_buffer, 0, 0);
	wl_surface_damage(globals->inputs.cursor_surface, 0, 0,
			  globals->inputs.cursor_size,
	                  globals->inputs.cursor_size);
	wl_surface_commit(globals->inputs.cursor_surface);
	wl_pointer_set_cursor(wl_pointer,
			      globals->inputs.enter_serial,
			      globals->inputs.cursor_surface,
	                      0, 0);
}

static void
pointer_enter(void *data,
	      struct wl_pointer *wl_pointer,
	      uint32_t serial,
	      struct wl_surface *surface,
	      wl_fixed_t surface_x,
	      wl_fixed_t surface_y)
{
	struct tw_globals *globals = data;
	struct tw_appsurf *app = tw_appsurf_from_wl_surface(surface);
	struct tw_app_event e = {
		.type = TW_FOCUS,
		.ptr.x = wl_fixed_to_double(surface_x),
		.ptr.y = wl_fixed_to_double(surface_y),
		.ptr.mod = globals->inputs.modifiers,
	};

	globals->inputs.pointer_focused = surface;
	globals->inputs.pointer_events = POINTER_ENTER;
	globals->inputs.enter_serial = serial;
	globals->inputs.serial = serial;
	//setting cursor at enter maynot be the best time though
	if (!globals->inputs.cursor_set) {
		pointer_set_cursor(wl_pointer);
		globals->inputs.cursor_set = true;
	}
	if (app) {
		app->tw_globals = globals;
		_tw_appsurf_run_frame(app, &e);
	}
}

static void
pointer_leave(void *data,
	      struct wl_pointer *wl_pointer,
	      uint32_t serial,
	      struct wl_surface *surface)
{
	struct tw_globals *globals = data;
	struct tw_appsurf *app = tw_appsurf_from_wl_surface(surface);
	struct tw_app_event e = {
		.type = TW_UNFOCUS,
	};

	globals->inputs.serial = serial;
	globals->inputs.pointer_focused = NULL;
	globals->inputs.pointer_events = POINTER_LEAVE;
	globals->inputs.cursor_set = false;
	if (app) {
		app->tw_globals = globals;
		_tw_appsurf_run_frame(app, &e);
	}
}

static void
pointer_motion(void *data,
	       struct wl_pointer *wl_pointer,
	       uint32_t serial,
	       wl_fixed_t x,
	       wl_fixed_t y)
{
	struct tw_globals *globals = data;

	if (wl_pointer != globals->inputs.wl_pointer)
		return;
	globals->inputs.serial = serial;
	globals->inputs.dx = wl_fixed_to_int(x) - globals->inputs.sx;
	globals->inputs.dy = wl_fixed_to_int(y) - globals->inputs.sy;
	globals->inputs.sx = wl_fixed_to_int(x);
	globals->inputs.sy = wl_fixed_to_int(y);
	globals->inputs.pointer_events |=  POINTER_MOTION;
}

static inline struct tw_appsurf *
pointer_button_meta(struct tw_globals *globals,
		    uint32_t serial,
		    uint32_t time,
		    uint32_t button,
		    uint32_t state)
{
	globals->inputs.millisec = time;
	globals->inputs.btn_pressed = (state == WL_POINTER_BUTTON_STATE_PRESSED);
	globals->inputs.btn = button;
	globals->inputs.pointer_events |= POINTER_BTN;
	globals->inputs.serial = serial;

	struct wl_surface *surf = globals->inputs.pointer_focused;
	struct tw_appsurf *app = (!surf) ? NULL :
		tw_appsurf_from_wl_surface(surf);
	return app;
}

static void
pointer_button(void *data,
	       struct wl_pointer *wl_pointer,
	       uint32_t serial,
	       uint32_t time,
	       uint32_t button,
	       uint32_t state)
{
	struct tw_globals *globals = data;
	struct tw_appsurf *app =
		pointer_button_meta(globals, serial, time, button,
				    state);
	if (wl_pointer != globals->inputs.wl_pointer || !app)
		return;

	//test if at right cornor,
	//XXX I don't seem need to apply the scale here
	if ((float)globals->inputs.sx > 0.9 * (float)app->allocation.w &&
	    (float)globals->inputs.sy > 0.9 * (float)app->allocation.h &&
	    state == WL_POINTER_BUTTON_STATE_PRESSED &&
	    button == BTN_LEFT) {
		resize_pointer_grab(&pointer_listener);
	}

}

static void
pointer_axis(void *data,
	     struct wl_pointer *wl_pointer,
	     uint32_t time,
	     uint32_t axis, //vertial or horizental
	     wl_fixed_t value)
{
	struct tw_globals *globals = data;

	if (wl_pointer != globals->inputs.wl_pointer)
		return;
	globals->inputs.millisec = time;

	globals->inputs.dx_axis +=
		(axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL) ?
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
	struct tw_globals *globals = data;

	if (wl_pointer != globals->inputs.wl_pointer)
		return;
	globals->inputs.millisec = time;
	//we do not implement kinect scrolling
}

static void
pointer_axis_discrete(void *data, struct wl_pointer *wl_pointer,
		     uint32_t axis, int32_t discrete)
{
	//the discrete representation of the axis event, for example(a travel
	//of 102 pixel represent) 2 times of wheel scrolling, the axis event is
	//more useful.
}

/* once a frame event generate, we need to accumelate all previous pointer
 * events and then send them all. Call frame on them, all though you should not
 * receive mo re than one type of pointer event
 */
static inline struct tw_appsurf *
pointer_frame_meta(struct tw_globals *globals)
{
	if (!globals->inputs.pointer_events)
		return NULL;
	struct wl_surface *focused = globals->inputs.pointer_focused;
	struct tw_appsurf *appsurf = (focused) ?
		tw_appsurf_from_wl_surface(focused) : NULL;
	if (!appsurf || !appsurf->do_frame)
		return NULL;
	return appsurf;
}

static void
pointer_frame(void *data,
	      struct wl_pointer *wl_pointer)
{
	//we need somehow generate a timestamp
	struct tw_app_event e;
	uint32_t event;
	struct tw_globals *globals = data;
	struct tw_appsurf *appsurf = pointer_frame_meta(globals);

        if (wl_pointer != globals->inputs.wl_pointer || !appsurf)
		return;

	e.time = globals->inputs.millisec;
	event = globals->inputs.pointer_events;
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
		_tw_appsurf_run_frame(appsurf, &e);
	}
	if (event & POINTER_BTN) {
		e.type = TW_POINTER_BTN;
		e.ptr.btn = globals->inputs.btn;
		e.ptr.x = globals->inputs.sx;
		e.ptr.y = globals->inputs.sy;;
		e.ptr.state = globals->inputs.btn_pressed;
		e.ptr.mod = globals->inputs.modifiers;
		_tw_appsurf_run_frame(appsurf, &e);
	}
	pointer_event_clean(globals);
}

static void
default_pointer_grab(struct wl_pointer_listener *grab)
{
	grab->enter = pointer_enter;
	grab->leave = pointer_leave;
	grab->motion = pointer_motion;
	grab->frame = pointer_frame;
	grab->button = pointer_button;
	grab->axis = pointer_axis;
	grab->axis_source = pointer_axis_src;
	grab->axis_stop = pointer_axis_stop;
	grab->axis_discrete = pointer_axis_discrete;
}

/******************************************************************************
 * resize grab
 *****************************************************************************/

static void
resize_pointer_button(void *data,
		      struct wl_pointer *wl_pointer,
		      uint32_t serial,
		      uint32_t time,
		      uint32_t button,
		      uint32_t state)
{
	//this is the ugly part we have to copy them again
	struct tw_globals *globals = data;
        if (wl_pointer != globals->inputs.wl_pointer)
		return;

	pointer_button_meta(globals, serial, time, button, state);

	/* struct tw_globals *globals = data; */
	if (state == WL_POINTER_BUTTON_STATE_RELEASED &&
	    button == BTN_LEFT) {
		default_pointer_grab(&pointer_listener);
	}
}

static void
resize_pointer_frame(void *data,
		     struct wl_pointer *wl_pointer)
{
	//we need somehow generate a timestamp
	struct tw_globals *globals = data;
	uint32_t event = globals->inputs.pointer_events;
	struct tw_appsurf *app = pointer_frame_meta(globals);

	if (wl_pointer != globals->inputs.wl_pointer || !app)
		return;

	//REPEAT CODE
	if (event & POINTER_MOTION)
		tw_appsurf_resize(app,
				   (int)app->allocation.w + globals->inputs.dx,
				   (int)app->allocation.h + globals->inputs.dy,
				   app->allocation.s);

	pointer_event_clean(globals);
}

static void
resize_pointer_grab(struct wl_pointer_listener *grab)
{
	grab->button = resize_pointer_button;
	grab->frame = resize_pointer_frame;
}

/******************************************************************************
 * cursor theme
 *****************************************************************************/
WL_EXPORT void
tw_globals_change_cursor(struct tw_globals *globals, const char *type)
{
	struct wl_cursor *new_cursor = NULL;

        if (!globals->inputs.cursor_theme ||
            !globals->inputs.wl_pointer)
		return;

        new_cursor = wl_cursor_theme_get_cursor(globals->inputs.cursor_theme,
                                                type);
        if (!new_cursor)
	        return;

        globals->inputs.cursor = new_cursor;
        globals->inputs.cursor_buffer =
	        wl_cursor_image_get_buffer(globals->inputs.cursor->images[0]);
        pointer_set_cursor(globals->inputs.wl_pointer);
}

WL_EXPORT void
tw_globals_reload_cursor_theme(struct tw_globals *globals)
{
	//clean up
	if (globals->inputs.cursor_surface)
		wl_surface_destroy(globals->inputs.cursor_surface);
	if (globals->inputs.cursor_theme)
		wl_cursor_theme_destroy(globals->inputs.cursor_theme);
	globals->inputs.cursor_set = false;

	//reload
	globals->inputs.cursor_theme =
		wl_cursor_theme_load(globals->inputs.cursor_theme_name,
		                     globals->inputs.cursor_size,
		                     globals->shm);
	globals->inputs.cursor =
		wl_cursor_theme_get_cursor(globals->inputs.cursor_theme,
		                           "left_ptr");

	//the theme you asked is not there we will load the default theme then
	if (!globals->inputs.cursor) {
		wl_cursor_theme_destroy(globals->inputs.cursor_theme);
		globals->inputs.cursor_theme =
			wl_cursor_theme_load(NULL, globals->inputs.cursor_size,
			                     globals->shm);
		globals->inputs.cursor =
			wl_cursor_theme_get_cursor(globals->inputs.cursor_theme,
			                           "left_ptr");
	}

	globals->inputs.cursor_surface =
		wl_compositor_create_surface(globals->compositor);
	globals->inputs.cursor_buffer =
		wl_cursor_image_get_buffer(globals->inputs.cursor->images[0]);
}

/******************************************************************************
 * constructor/destructor
 *****************************************************************************/

void
tw_pointer_init(struct wl_pointer *wl_pointer, struct tw_globals *globals)
{
	if (!globals)
		return;

	default_pointer_grab(&pointer_listener);
	wl_pointer_add_listener(wl_pointer, &pointer_listener, globals);

	tw_globals_reload_cursor_theme(globals);

	pointer_event_clean(globals);
}

void
tw_pointer_destroy(struct wl_pointer *wl_pointer)
{
	struct tw_globals *globals = wl_pointer_get_user_data(wl_pointer);

        if (globals->inputs.cursor_theme) {
		wl_cursor_theme_destroy(globals->inputs.cursor_theme);
		globals->inputs.cursor = NULL;
		globals->inputs.cursor_buffer = NULL;
		globals->inputs.cursor_theme = NULL;
	}
	if (globals->inputs.cursor_surface) {
		wl_surface_destroy(globals->inputs.cursor_surface);
		globals->inputs.cursor_surface = NULL;
	}

	wl_pointer_destroy(wl_pointer);
}


/******************************************************************************
 * touch impl
 *****************************************************************************/

static void
handle_touch_down(void *data,
		  struct wl_touch *wl_touch,
		  uint32_t serial,
		  uint32_t time,
		  struct wl_surface *surface,
		  int32_t id,
		  wl_fixed_t x,
		  wl_fixed_t y)
{
	(void)id;
	struct tw_globals *g = wl_touch_get_user_data(wl_touch);
	if (!g->inputs.wl_pointer)
		return;
	//setup the position and surface
	g->inputs.sx = wl_fixed_to_int(x);
	g->inputs.sy = wl_fixed_to_int(y);
	g->inputs.pointer_focused = surface;

	pointer_button(data, g->inputs.wl_pointer, serial, time,
		       BTN_LEFT,
		WL_POINTER_BUTTON_STATE_PRESSED);
	//well. The thing we could do we simulate the pointer events with touch for now

}

static void
handle_touch_up(void *data,
		struct wl_touch *wl_touch,
		uint32_t serial,
		uint32_t time,
		int32_t id)
{
	(void)id;
	struct tw_globals *g = wl_touch_get_user_data(wl_touch);
	if (!g->inputs.wl_pointer)
		return;
	/* g->inputs.pointer_focused = NULL; */
	pointer_button(data, g->inputs.wl_pointer, serial, time,
		       BTN_LEFT,
		WL_POINTER_BUTTON_STATE_RELEASED);
}

static void
handle_touch_motion(void *data,
		    struct wl_touch *wl_touch,
		    uint32_t time,
		    int32_t id,
		    wl_fixed_t x,
		    wl_fixed_t y)
{
	(void)id;
	struct tw_globals *g = wl_touch_get_user_data(wl_touch);
	if (!g->inputs.wl_pointer)
		return;
	pointer_motion(data, g->inputs.wl_pointer, time, x, y);
}

static void
handle_touch_cancel(void *data,
		    struct wl_touch *wl_touch)
{
	struct tw_globals *g = wl_touch_get_user_data(wl_touch);
	if (!g->inputs.wl_pointer)
		return;
	pointer_event_clean(g);
}

static void
handle_touch_frame(void *data,
		   struct wl_touch *wl_touch)
{
	struct tw_globals *g = wl_touch_get_user_data(wl_touch);
	if (!g->inputs.wl_pointer)
		return;
	pointer_frame(data, g->inputs.wl_pointer);
}

static void
handle_touch_shape(void *data,
		   struct wl_touch *wl_touch,
		   int32_t id,
		   wl_fixed_t major,
		   wl_fixed_t minor)
{

}
static void
handle_touch_orientation(void *data,
			 struct wl_touch *wl_touch,
			 int32_t id,
			 wl_fixed_t orientation)
{
}


//default touch grap.
static struct wl_touch_listener touch_listener = {
	.down = handle_touch_down,
	.up = handle_touch_up,
	.motion = handle_touch_motion,
	.frame = handle_touch_frame,
	.cancel = handle_touch_cancel,
	.shape = handle_touch_shape,
	.orientation = handle_touch_orientation,
};


void
tw_touch_init(struct wl_touch *wl_touch, struct tw_globals *globals)
{
	wl_touch_add_listener(wl_touch, &touch_listener, globals);
}
void
tw_touch_destroy(struct wl_touch *wl_touch)
{
	wl_touch_destroy(wl_touch);
}
