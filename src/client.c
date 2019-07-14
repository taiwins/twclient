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

#include <client.h>
#include <ui.h>

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
	struct wl_globals *globals = surf->wl_globals;

	if (globals && surf->do_frame &&
	    globals->inputs.key_pressed &&
		globals->inputs.keysym == globals->inputs.keysym) {
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
				.state = (bool)e->arg.u,
			},
		};
		e->arg.u = !(bool)e->arg.u;
		surf->do_frame(surf, &ae);
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
				.u = false, //next we generate release event
			},
		};
		tw_event_queue_add_timer(&globals->event_queue,
					 &globals->inputs.repeat_info,
					 &repeat_event);
	}
	appsurf->do_frame(appsurf, &e);
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
pointer_init(struct wl_pointer *wl_pointer)
{
	struct wl_globals *globals = wl_pointer_get_user_data(wl_pointer);
	if (!globals)
		return;
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

static void
pointer_destroy(struct wl_pointer *wl_pointer)
{
	struct wl_globals *globals = wl_pointer_get_user_data(wl_pointer);
	wl_cursor_theme_destroy(globals->inputs.cursor_theme);
	wl_surface_destroy(globals->inputs.cursor_surface);

	wl_pointer_destroy(wl_pointer);
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


//the default grab of pointer, other type grab(resize), or types like
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
		wl_pointer_add_listener(globals->inputs.wl_pointer, &pointer_listener, globals);
		pointer_init(globals->inputs.wl_pointer);
		fprintf(stderr, "got a mouse\n");
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
#ifdef _GNU_SOURCE
/* we want to fit wl_globals inside L1 cache */
_Static_assert(sizeof(struct wl_globals) <= 32 * 1024, "wl_globals is too big");
#endif

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
	if (globals->inputs.wl_pointer)
		pointer_destroy(globals->inputs.wl_pointer);
	if (globals->inputs.wl_keyboard)
		wl_keyboard_destroy(globals->inputs.wl_keyboard);
	if (globals->inputs.wl_touch)
		wl_touch_destroy(globals->inputs.wl_touch);

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

	wl_seat_destroy(globals->inputs.wl_seat);
	wl_shm_destroy(globals->shm);
	wl_compositor_destroy(globals->compositor);
	tw_event_queue_close(&globals->event_queue);
}

//we need to have a global remove function here
//if we have our own configurator, the wl_globals can be really useful.
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
