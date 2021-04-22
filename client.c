#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <poll.h>
#include <sys/inotify.h>
#include <libudev.h>

#include <linux/input.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-names.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <wayland-util.h>

#include <sequential.h>
#include <os/buffer.h>
#include <os/file.h>

#include "client.h"
#include "ui.h"

////////////////////////////wayland listeners///////////////////////////

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


//okay, this is when we chose the best format
static void
shm_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
	//the priority of the format ARGB8888 > RGBA8888 > RGB888
	struct wl_globals *globals = (struct wl_globals *)data;
	//we have to use ARGB because cairo uses this format
	if (format == WL_SHM_FORMAT_ARGB8888) {
		globals->buffer_format = WL_SHM_FORMAT_ARGB8888;
		//fprintf(stderr, "we choosed argb8888 for the application\n");
	}
	else if (format == WL_SHM_FORMAT_RGBA8888 &&
		 globals->buffer_format != WL_SHM_FORMAT_ARGB8888) {
		globals->buffer_format = WL_SHM_FORMAT_RGBA8888;
		//fprintf(stderr, "we choosed rgba8888 for the application\n");
	}
	else if (format == WL_SHM_FORMAT_RGB888 &&
		 globals->buffer_format != WL_SHM_FORMAT_ARGB8888 &&
		 globals->buffer_format != WL_SHM_FORMAT_RGBA8888) {
		globals->buffer_format = WL_SHM_FORMAT_RGB888;
		//fprintf(stderr, "we choosed rgb888 for the application\n");
	} else {
		//fprintf(stderr, "okay, the shm_format that we don't know 0x%x\n", format);
	}
}

bool
is_shm_format_valid(uint32_t format)
{
	return format != 0xFFFFFFFF;
}

static struct wl_shm_listener shm_listener = {
	shm_format
};


static int
handle_key_repeat(struct tw_event *e, int fd)
{
	struct app_surface *surf = e->data;
	xkb_keysym_t keysym = e->arg.u;
	struct wl_globals *globals = surf->wl_globals;
	if (globals && surf->keycb &&
	    globals->inputs.key_pressed &&
		keysym == globals->inputs.keysym) {
		surf->keycb(surf, globals->inputs.keysym, globals->inputs.modifiers,
			    globals->inputs.key_pressed);
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

	xkb_keycode_t keycode = kc_linux2xkb(key);
	xkb_keysym_t  keysym  = xkb_state_key_get_one_sym(globals->inputs.kstate,
							  keycode);
	uint32_t modifier = tw_mod_mask_from_xkb_state(globals->inputs.kstate);

	globals->inputs.serial = serial;
	globals->inputs.key_pressed = state == WL_KEYBOARD_KEY_STATE_PRESSED;
	globals->inputs.keysym = keysym;
	globals->inputs.keycode = keycode;
	globals->inputs.modifiers = modifier;

	/* char keyname[100]; */
	/* xkb_keysym_get_name(keysym, keyname, 100); */
	/* fprintf(stderr, "the key pressed is %s\n", keyname); */
	struct wl_surface *focused = globals->inputs.focused_surface;
	struct app_surface *appsurf = (focused) ? app_surface_from_wl_surface(focused) : NULL;
	if (appsurf && is_repeat_info_valid(globals->inputs.repeat_info) && state) {
		struct tw_event repeat_event = {
			.data = appsurf,
			.cb = handle_key_repeat,
			.arg = {
				.u = keysym,
			},
		};
		tw_event_queue_add_timer(&globals->event_queue,
					 &globals->inputs.repeat_info,
					 &repeat_event);
	}

	keycb_t keycb = (appsurf && appsurf->keycb) ? appsurf->keycb : NULL;
	if (keycb)
		keycb(appsurf, keysym, modifier,
		      (state == WL_KEYBOARD_KEY_STATE_PRESSED) ? 1 : 0);
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
	globals->inputs.serial = serial;
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
	globals->inputs.focused_surface = surface;
	globals->inputs.serial = serial;
	fprintf(stderr, "keyboard got focus\n");
}

static void
handle_keyboard_leave(void *data,
		    struct wl_keyboard *wl_keyboard,
		    uint32_t serial,
		    struct wl_surface *surface)
{
	struct wl_globals *globals = (struct wl_globals *)data;
	globals->inputs.focused_surface = NULL;
	globals->inputs.serial = serial;
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

//////////////////////////////////////////////////////////////////////////
/////////////////////////////Pointer listeners////////////////////////////
//////////////////////////////////////////////////////////////////////////

//the code is accumlate for the frame event.
//the bit map info
//[1] motion.
//[2-4] 2->left. 4->right. 8->middle. 16->dclick
// 5-> axis
enum POINTER_EVENT_CODE {
	//the event doesn't need to be handle
	POINTER_ENTER = 0,
	POINTER_LEAVE = 0,
	//motion
	POINTER_MOTION = 1,

	//the btn group, no middle key handled
	POINTER_BTN_LEFT =  2,
	POINTER_BTN_RIGHT = 4,
	POINTER_BTN_MID = 8,
	POINTER_BTN_DCLICK = 16,
	//axis events, we only handle a few.
	POINTER_AXIS = 32,

};

static void
pointer_cursor_done(void *data, struct wl_callback *callback, uint32_t callback_data)
{
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
	static bool cursor_set = false;
	struct wl_globals *globals = (struct wl_globals *)data;
	globals->inputs.focused_surface = surface;
	globals->inputs.serial = serial;
	if (!cursor_set) {
		struct wl_surface *csurface = globals->inputs.cursor_surface;
		struct wl_buffer *cbuffer = globals->inputs.cursor_buffer;
		struct wl_callback *callback = wl_surface_frame(csurface);
		globals->inputs.cursor_done_listener.done = pointer_cursor_done;
		wl_callback_add_listener(callback, &globals->inputs.cursor_done_listener, NULL);
		//give a role to the the cursor_surface
		wl_pointer_set_cursor(wl_pointer, serial, csurface, 16, 16);
		wl_surface_attach(csurface, cbuffer, 0, 0);
		wl_surface_damage(csurface, 0, 0, 32, 32);
		wl_surface_commit(csurface);
	}

	globals->inputs.cursor_events = POINTER_ENTER;
}

static void
pointer_leave(void *data,
	      struct wl_pointer *wl_pointer,
	      uint32_t serial,
	      struct wl_surface *surface)
{
	struct wl_globals *globals = (struct wl_globals *)data;
	globals->inputs.focused_surface = NULL;
	globals->inputs.cursor_events = POINTER_LEAVE;
	globals->inputs.serial = serial;
}

static void
pointer_motion(void *data,
	       struct wl_pointer *wl_pointer,
	       uint32_t serial,
	       wl_fixed_t surface_x,
	       wl_fixed_t surface_y)
{
	struct wl_globals *globals = (struct wl_globals *)data;
	globals->inputs.cx = wl_fixed_to_int(surface_x);
	globals->inputs.cy = wl_fixed_to_int(surface_y);
	globals->inputs.cursor_events |= POINTER_MOTION;
	globals->inputs.serial = serial;
}

static void
pointer_frame(void *data,
	      struct wl_pointer *wl_pointer)
{
	struct wl_globals *globals = (struct wl_globals *)data;
	if (!globals->inputs.cursor_events)
		return;
	//with the line above, we won't have any null surface problem
	struct wl_surface *focused = globals->inputs.focused_surface;
	struct app_surface *appsurf = (focused) ? app_surface_from_wl_surface(focused) : NULL;
	pointron_t ptr_on = (appsurf && appsurf->pointron) ? appsurf->pointron : NULL;
	pointrbtn_t ptr_btn = (appsurf && appsurf->pointrbtn) ? appsurf->pointrbtn : NULL;
	pointraxis_t ptr_axis = (appsurf && appsurf->pointraxis) ? appsurf->pointraxis : NULL;

	uint32_t event = globals->inputs.cursor_events;
	if ((event & POINTER_AXIS) && ptr_axis)
		ptr_axis(appsurf,
			 globals->inputs.axis_pos, globals->inputs.axis,
			 globals->inputs.cx, globals->inputs.cy);

	if ((event & POINTER_MOTION) && ptr_on)
		ptr_on(appsurf, globals->inputs.cx, globals->inputs.cy);

	if ((event & POINTER_BTN_LEFT) && ptr_btn)
		ptr_btn(appsurf, TWBTN_LEFT, globals->inputs.cursor_state,
				   globals->inputs.cx, globals->inputs.cy);
	if ((event & POINTER_BTN_RIGHT) && ptr_btn)
		ptr_btn(appsurf, TWBTN_RIGHT, globals->inputs.cursor_state,
				   globals->inputs.cx, globals->inputs.cy);
	if ((event & POINTER_BTN_MID) && ptr_btn)
		ptr_btn(appsurf, TWBTN_MID, globals->inputs.cursor_state,
				   globals->inputs.cx, globals->inputs.cy);

	/* fprintf(stderr, "we are getting a cursor event it is %s.\n", */
	/*	(event & POINTER_MOTION) ? "motion" : */
	/*	(event & POINTER_BTN_LEFT) ? "button" : "other"); */

	//finally erase all previous events
	globals->inputs.cursor_events = 0;
}


static void
pointer_button(void *data,
	       struct wl_pointer *wl_pointer,
	       uint32_t serial,
	       uint32_t time,
	       uint32_t button,
	       uint32_t state)
{
	struct wl_globals *globals = (struct wl_globals *)data;
	globals->inputs.cursor_state = state;
	globals->inputs.serial = serial;

	switch (button) {
	case BTN_LEFT:
		globals->inputs.cursor_events |= POINTER_BTN_LEFT;
		break;
	case BTN_RIGHT:
		globals->inputs.cursor_events |= POINTER_BTN_RIGHT;
		break;
	case BTN_MIDDLE:
		globals->inputs.cursor_events |= POINTER_BTN_MID;
		break;
	default:
		break;
	}
}

static void
pointer_axis(void *data,
	     struct wl_pointer *wl_pointer,
	     uint32_t time,
	     uint32_t axis,
	     wl_fixed_t value)
{
	struct wl_globals *globals = (struct wl_globals *)data;
	//in the surface coordinates, (0, 0) sits on top left, so we need to
	//reverse it
	globals->inputs.axis_pos = wl_fixed_to_int(value);
	globals->inputs.axis = axis;
	globals->inputs.cursor_events |= POINTER_AXIS;
}

static void
pointer_axis_src(void *data,
		 struct wl_pointer *wl_pointer, uint32_t src)
{
//	fprintf(stderr, "axis src event\n");
}

static void
pointer_axis_stop(void *data,
		  struct wl_pointer *wl_pointer,
		  uint32_t time, uint32_t axis)
{
//	fprintf(stderr, "axis end event\n");
}

static void
pointer_axis_discret(void *data, struct wl_pointer *wl_pointer,
		     uint32_t axis, int32_t discrete)
{
	//useless for a mouse
//	fprintf(stderr, "axis discrete event\n");
}

//make all of the them available, so we don't crash
static struct wl_pointer_listener pointer_listener = {
	.enter = pointer_enter,
	.leave = pointer_leave,
	.motion = pointer_motion,
	.frame = pointer_frame,
	.button = pointer_button,
	.axis  = pointer_axis,
	.axis_source = pointer_axis_src,
	.axis_stop = pointer_axis_stop,
	.axis_discrete = pointer_axis_discret,
};


static void
seat_name(void *data, struct wl_seat *wl_seat, const char *name)
{
	struct wl_globals *globals = (struct wl_globals *)data;
	strncpy(globals->inputs.name, name,
		MIN(NUMOF(globals->inputs.name), strlen(name)));
	fprintf(stderr, "we have this seat with a name called %s\n", name);
}



static void
seat_capabilities(void *data,
		       struct wl_seat *wl_seat,
		       uint32_t capabilities)
{
	struct wl_globals *globals = (struct wl_globals *)data;
	if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD) {
		fprintf(stderr, "got a keyboard\n");
		globals->inputs.wl_keyboard = wl_seat_get_keyboard(wl_seat);
		wl_keyboard_add_listener(globals->inputs.wl_keyboard, &keyboard_listener, globals);
	}
	if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
		globals->inputs.wl_pointer = wl_seat_get_pointer(wl_seat);
		fprintf(stderr, "got a mouse\n");
		//TODO use the name in the global
		globals->inputs.cursor_theme = wl_cursor_theme_load("whiteglass", 32, globals->shm);
		globals->inputs.cursor = wl_cursor_theme_get_cursor(globals->inputs.cursor_theme, "plus");
		globals->inputs.cursor_surface = wl_compositor_create_surface(globals->compositor);
		globals->inputs.cursor_buffer = wl_cursor_image_get_buffer(globals->inputs.cursor->images[0]);
		wl_pointer_add_listener(globals->inputs.wl_pointer, &pointer_listener, globals);
	}
	if (capabilities & WL_SEAT_CAPABILITY_TOUCH) {
		globals->inputs.wl_touch = wl_seat_get_touch(wl_seat);
		fprintf(stderr, "got a touchpad\n");
	}
}


static struct wl_seat_listener seat_listener = {

	.capabilities = seat_capabilities,
	.name = seat_name,
};


/*************************************************************
 *              event queue implementation                   *
 *************************************************************/
struct tw_event_source {
	struct wl_list link;
	struct epoll_event poll_event;
	struct tw_event event;
	void (* pre_hook) (struct tw_event_source *);
	void (* close)(struct tw_event_source *);
	int fd;//timer, or inotify fd
	union {
		int wd;//the actual watch id, we create one inotify per source.
		struct udev_monitor *mon;
	};

};

static void close_fd(struct tw_event_source *s)
{
	close(s->fd);
}


static struct tw_event_source*
alloc_event_source(struct tw_event *e, uint32_t mask, int fd)
{
	struct tw_event_source *event_source = malloc(sizeof(struct tw_event_source));
	wl_list_init(&event_source->link);
	event_source->event = *e;
	event_source->poll_event.data.ptr = event_source;
	event_source->poll_event.events = mask;
	event_source->fd = fd;
	event_source->pre_hook = NULL;
	event_source->close = close_fd;
	return event_source;
}

static inline void
destroy_event_source(struct tw_event_source *s)
{
	wl_list_remove(&s->link);
	if (s->close)
		s->close(s);
	free(s);
}

static inline void
tw_event_queue_close(struct tw_event_queue *queue)
{
	struct tw_event_source *event_source, *next;
	wl_list_for_each_safe(event_source, next, &queue->head, link) {
		epoll_ctl(queue->pollfd, EPOLL_CTL_DEL, event_source->fd, NULL);
		destroy_event_source(event_source);
	}
	close(queue->pollfd);
}

void
tw_event_queue_run(struct tw_event_queue *queue)
{
	struct epoll_event events[32];
	struct tw_event_source *event_source;

	//poll->produce-event-or-timeout
	while (!queue->quit) {
		if (queue->wl_display) {
			wl_display_flush(queue->wl_display);
		}
		int count = epoll_wait(queue->pollfd, events, 32, -1);
		//right now if we run into any trouble, we just quit, I don't
		//think it is a good idea
		queue->quit = queue->quit && (count != -1);

		for (int i = 0; i < count; i++) {
			event_source = events[i].data.ptr;
			if (event_source->pre_hook)
				event_source->pre_hook(event_source);
			int output = event_source->event.cb(
				&event_source->event,
				event_source->fd);
			if (output == TW_EVENT_DEL)
				destroy_event_source(event_source);
		}

	}
	tw_event_queue_close(queue);
	return;
}

bool
tw_event_queue_init(struct tw_event_queue *queue)
{
	int fd = epoll_create1(EPOLL_CLOEXEC);
	if (fd == -1)
		return false;
	wl_list_init(&queue->head);

	queue->pollfd = fd;
	queue->quit = false;
	return true;
}


//////////////////////// INOTIFY //////////////////////////////

static void
read_inotify(struct tw_event_source *s)
{
	struct inotify_event events[100];
	read(s->fd, events, sizeof(events));
}

static void
close_inotify_watch(struct tw_event_source *s)
{
	inotify_rm_watch(s->fd, s->wd);
	close(s->fd);
}


bool
tw_event_queue_add_file(struct tw_event_queue *queue, const char *path,
			struct tw_event *e, uint32_t mask)
{
	if (!mask)
		mask = IN_MODIFY | IN_DELETE;
	if (!is_file_exist(path))
		return false;
	int fd = inotify_init1(IN_CLOEXEC);
	struct tw_event_source *s = alloc_event_source(e, EPOLLIN | EPOLLET, fd);
	s->close = close_inotify_watch;
	s->pre_hook = read_inotify;

	if (epoll_ctl(queue->pollfd, EPOLL_CTL_ADD, fd, &s->poll_event)) {
		destroy_event_source(s);
		return false;
	}
	s->wd = inotify_add_watch(fd, path, mask);
	return true;
}


//////////////////// GENERAL SOURCE ////////////////////////////

bool
tw_event_queue_add_source(struct tw_event_queue *queue, int fd,
			  struct tw_event *e, uint32_t mask)
{
	if (!mask)
		mask = EPOLLIN | EPOLLET;
	struct tw_event_source *s = alloc_event_source(e, mask, fd);
	wl_list_insert(&queue->head, &s->link);
	/* s->pre_hook = read_inotify; */

	if (epoll_ctl(queue->pollfd, EPOLL_CTL_ADD, fd, &s->poll_event)) {
		destroy_event_source(s);
		return false;
	}
	e->cb(e->data, s->fd);
	return true;
}

/////////////////////////// TIMER //////////////////////////////

static void
read_timer(struct tw_event_source *s)
{
	uint64_t nhit;
	read(s->fd, &nhit, 8);
}

bool
tw_event_queue_add_timer(struct tw_event_queue *queue,
			 const struct itimerspec *spec, struct tw_event *e)
{
	int fd;
	fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (!fd)
		goto err;
	if (timerfd_settime(fd, 0, spec, NULL))
		goto err_settime;
	struct tw_event_source *s = alloc_event_source(e, EPOLLIN | EPOLLET, fd);
	s->pre_hook = read_timer;
	wl_list_insert(&queue->head, &s->link);
	//you ahve to read the timmer.
	if (epoll_ctl(queue->pollfd, EPOLL_CTL_ADD, fd, &s->poll_event))
		goto err_add;

	return true;

err_add:
	destroy_event_source(s);
err_settime:
	close(fd);
err:
	return false;
}

//////////////////////// WL_DISPLAY ////////////////////////////

static int
dispatch_wl_display(struct tw_event *e, int fd)
{
	(void)fd;
	struct tw_event_queue *queue = e->data;
	struct wl_display *display = queue->wl_display;
	while (wl_display_prepare_read(display) != 0)
		wl_display_dispatch_pending(display);
	wl_display_flush(display);
	if (wl_display_read_events(display) == -1)
		queue->quit = true;
	//this quit is kinda different
	if (wl_display_dispatch_pending(display) == -1)
		queue->quit = true;
	wl_display_flush(display);
	return TW_EVENT_NOOP;
}

bool
tw_event_queue_add_wl_display(struct tw_event_queue *queue, struct wl_display *display)
{
	int fd = wl_display_get_fd(display);
	queue->wl_display = display;
	struct tw_event dispatch_display = {
		.data = queue,
		.cb = dispatch_wl_display,
	};
	struct tw_event_source *s = alloc_event_source(&dispatch_display, EPOLLIN | EPOLLET, fd);
	//don't close wl_display in the end
	s->close = NULL;
	wl_list_insert(&queue->head, &s->link);

	if (epoll_ctl(queue->pollfd, EPOLL_CTL_ADD, fd, &s->poll_event)) {
		destroy_event_source(s);
		return false;
	}
	return true;
}

/*************************************************************
 *               wl_globals implementation                   *
 *************************************************************/

void
wl_globals_init(struct wl_globals *globals, struct wl_display *display)
{
	//do this first, so all the pointers are null
	*globals = (struct wl_globals){0};
	globals->display = display;
	globals->buffer_format = 0xFFFFFFFF;
	tw_event_queue_init(&globals->event_queue);
	globals->event_queue.quit =
		!tw_event_queue_add_wl_display(&globals->event_queue, display);
}

void
wl_globals_release(struct wl_globals *globals)
{
	//we distroy everything except wl_display, since we get that from user,
	//it is up to user to clear it

	//destroy the input
	if (globals->inputs.cursor_theme) {
		//there is no need to destroy the cursor wl_buffer or wl_cursor,
		//it gets cleaned up automatically in theme_destroy
		wl_cursor_theme_destroy(globals->inputs.cursor_theme);
		wl_surface_destroy(globals->inputs.cursor_surface);
		globals->inputs.cursor_theme = NULL;
		globals->inputs.cursor = NULL;
		globals->inputs.cursor_buffer = NULL;
		globals->inputs.cursor_surface = NULL;
		globals->inputs.focused_surface = NULL;
	}
	if (globals->inputs.wl_pointer) {
		wl_pointer_destroy(globals->inputs.wl_pointer);
	}
	if (globals->inputs.wl_keyboard)
		wl_keyboard_destroy(globals->inputs.wl_keyboard);
	wl_seat_destroy(globals->inputs.wl_seat);
	wl_shm_destroy(globals->shm);
	wl_compositor_destroy(globals->compositor);
	tw_event_queue_close(&globals->event_queue);
}

//we need to have a global remove function here

int
wl_globals_announce(struct wl_globals *globals,
		    struct wl_registry *wl_registry,
		    uint32_t name,
		    const char *interface,
		    uint32_t version)
{
	if (strcmp(interface, wl_seat_interface.name) == 0) {
		globals->inputs.wl_seat = wl_registry_bind(wl_registry, name, &wl_seat_interface, version);
		wl_seat_add_listener(globals->inputs.wl_seat, &seat_listener, globals);
	} else if (strcmp(interface, wl_compositor_interface.name) == 0) {
		globals->compositor = wl_registry_bind(wl_registry, name, &wl_compositor_interface, version);
	} else if (strcmp(interface, wl_shm_interface.name) == 0)  {
		globals->shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, version);
		wl_shm_add_listener(globals->shm, &shm_listener, globals);
	} else {
		fprintf(stderr, "announcing global %s\n", interface);
		return 0;
	}
	return 1;
}
