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
#include <wayland-util.h>

#include <sequential.h>
#include <os/buffer.h>
#include <os/file.h>

#include <client.h>
#include <ui.h>


////////////////////////////////////////////////////////////////////////
// externs
////////////////////////////////////////////////////////////////////////
extern void tw_event_queue_close(struct tw_event_queue *queue);
extern void tw_keyboard_init(struct wl_keyboard *, struct wl_globals *);
extern void tw_keyboard_destroy(struct wl_keyboard *);

extern void tw_pointer_init(struct wl_pointer *, struct wl_globals *);
extern void tw_pointer_destroy(struct wl_pointer *);

extern void tw_touch_init(struct wl_touch *, struct wl_globals *);
extern void tw_touch_destroy(struct wl_touch *);


/*************************************************************
 *                  wl_shm implementation                    *
 *************************************************************/

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


/*************************************************************
 *                  wl_seat implementation                   *
 *************************************************************/

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
		tw_keyboard_init(globals->inputs.wl_keyboard, globals);
	}
	if (capabilities & WL_SEAT_CAPABILITY_POINTER) {
		globals->inputs.wl_pointer = wl_seat_get_pointer(wl_seat);
		tw_pointer_init(globals->inputs.wl_pointer, globals);
		fprintf(stderr, "got a mouse\n");
	}
	if (capabilities & WL_SEAT_CAPABILITY_TOUCH) {
		globals->inputs.wl_touch = wl_seat_get_touch(wl_seat);
		fprintf(stderr, "got a touchpad\n");
	}
}

static void
seat_destroy(struct wl_seat *wl_seat, struct wl_globals *globals)
{
	if (globals->inputs.wl_pointer)
		tw_pointer_destroy(globals->inputs.wl_pointer);
	if (globals->inputs.wl_keyboard)
		tw_keyboard_destroy(globals->inputs.wl_keyboard);
	if (globals->inputs.wl_touch)
		wl_touch_destroy(globals->inputs.wl_touch);
	wl_seat_destroy(wl_seat);
}


static struct wl_seat_listener seat_listener = {
	.capabilities = seat_capabilities,
	.name = seat_name,
};


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
	seat_destroy(globals->inputs.wl_seat, globals);
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
