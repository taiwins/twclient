#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/epoll.h>

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


/*******************************************************************************
 * externs
 ******************************************************************************/
extern void tw_event_queue_close(struct tw_event_queue *queue);
extern void tw_keyboard_init(struct wl_keyboard *, struct wl_globals *);
extern void tw_keyboard_destroy(struct wl_keyboard *);

extern void tw_pointer_init(struct wl_pointer *, struct wl_globals *);
extern void tw_pointer_destroy(struct wl_pointer *);

extern void tw_touch_init(struct wl_touch *, struct wl_globals *);
extern void tw_touch_destroy(struct wl_touch *);
extern void _app_surface_run_frame(struct app_surface *surf,
				   const struct app_event *e);

/*******************************************************************************
 * wl_shm
 ******************************************************************************/

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


/*******************************************************************************
 * wl_seat
 ******************************************************************************/
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


/*******************************************************************************
 * wl_data_device
 ******************************************************************************/

struct data_offer_data {
	struct wl_globals *globals;
	struct wl_data_offer *wl_data_offer;
	struct wl_surface *surface;
	uint32_t mime_offered; //bitfield
	uint32_t action_received; //bitfield
	uint32_t serial;
	int fd;
};


/* A few different major mime type list here. The only useful one right now is
 * text
 */
enum data_mime_type {
	MIME_TYPE_TEXT = 0,
	MIME_TYPE_AUDIO = 1,
	MIME_TYPE_VIDEO = 2,
	MIME_TYPE_IMAGE = 3,
	MIME_TYPE_APPLICATION = 4,
	MIME_TYPE_MAX = 5,
};

static char *MIME_TYPES[] = {
	"text/",
	"audio/",
	"video/",
	"image/",
	"application/"
};

static void
data_offer_offered(void *data,
	     struct wl_data_offer *wl_data_offer,
	     const char *mime_type)
{
	struct data_offer_data *offer_data = data;
	for (int i = 0; i < MIME_TYPE_MAX; i++)
		if (strstr(mime_type, MIME_TYPES[i]) == mime_type)
			offer_data->mime_offered &= 1 << i;
}

static void
data_offer_source_actions(void *data,
			  struct wl_data_offer *wl_data_offer,
			  uint32_t source_actions)
{
	struct data_offer_data *offer_data = data;
	if (source_actions == WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE)
		wl_data_offer_accept(wl_data_offer, offer_data->serial,
				     NULL);
	else
		wl_data_offer_set_actions(wl_data_offer,
					  WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
					  WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE,
					  WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
}

static void
data_offer_action(void *data,
		  struct wl_data_offer *wl_data_offer,
		  uint32_t dnd_action)
{
	struct data_offer_data *offer_data = data;
	offer_data->action_received = dnd_action;

}

static struct wl_data_offer_listener data_offer_listener = {
	.offer = data_offer_offered,
	.source_actions = data_offer_source_actions,
	.action = data_offer_action,
};

static inline void
data_offer_create(struct wl_data_offer *offer, struct wl_globals *globals)
{
	struct data_offer_data *offer_data =
		malloc(sizeof(offer_data));
	offer_data->globals = globals;
	offer_data->action_received = 0;
	offer_data->mime_offered = 0;
	offer_data->serial = 0;
	offer_data->fd = -1;
	offer_data->wl_data_offer = offer;
	wl_data_offer_add_listener(offer, &data_offer_listener, offer_data);
}

static inline void
data_offer_destroy(struct wl_data_offer *offer)
{
	struct data_offer_data *data =
		wl_data_offer_get_user_data(offer);
	data->globals->inputs.wl_data_offer = NULL;
	free(data);
	wl_data_offer_destroy(offer);
}

static void
data_offer(void *data,
	   struct wl_data_device *wl_data_device,
	   struct wl_data_offer *id)
{
	struct wl_globals *globals = data;
	globals->inputs.wl_data_offer = id;
	data_offer_create(id, globals);
}

static void
data_enter(void *data,
	   struct wl_data_device *wl_data_device,
	   uint32_t serial,
	   struct wl_surface *surface,
	   wl_fixed_t x,
	   wl_fixed_t y,
	   struct wl_data_offer *id)
{
	struct wl_globals *globals = data;
	struct wl_data_offer *offer = globals->inputs.wl_data_offer;
	struct data_offer_data *offer_data =
		wl_data_offer_get_user_data(offer);
	offer_data->serial = serial;
	offer_data->surface = surface;
	wl_data_offer_set_actions(offer,
				  WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
				  WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE,
				  WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY);
}

static void
data_leave(void *data,
	   struct wl_data_device *wl_data_device)
{
	struct wl_globals *globals = data;
	data_offer_destroy(globals->inputs.wl_data_offer);
	globals->inputs.wl_data_offer = NULL;
}

static void
data_motion(void *data,
	    struct wl_data_device *wl_data_device,
	    uint32_t time,
	    wl_fixed_t x,
	    wl_fixed_t y)
{}

static int
data_write_finished(struct tw_event *event, int fd)
{
	char buff[1024];
	struct data_offer_data *data = event->data;
	struct app_surface *app =
		app_surface_from_wl_surface(data->surface);
	while (read(fd, buff, 1024) != 0) {
		struct app_event e = {
			.type = TW_PASTE,
			.clipboard.data = buff,
			.clipboard.size = 1024,
		};
		_app_surface_run_frame(app, &e);
	}
	//finish the this data offer
	wl_data_offer_finish(data->wl_data_offer);
	data_offer_destroy(data->wl_data_offer);
	data->globals->inputs.wl_data_offer = NULL;

	return TW_EVENT_DEL;
}

static void
data_drop(void *data,
	  struct wl_data_device *wl_data_device)
{
	struct wl_globals *globals = data;
	struct wl_data_offer *offer = globals->inputs.wl_data_offer;
	struct data_offer_data *offer_data =
		wl_data_offer_get_user_data(offer);
	struct tw_event_queue *queue = &globals->event_queue;
	struct tw_event event = {
		.data = offer_data,
		.cb = data_write_finished,
	};

	uint32_t best_option = ffs(offer_data->mime_offered);
	if (best_option && (offer_data->action_received &
	    (WL_DATA_DEVICE_MANAGER_DND_ACTION_COPY |
	     WL_DATA_DEVICE_MANAGER_DND_ACTION_MOVE))) {
		int pipef[2];
		if (pipe2(pipef, O_CLOEXEC))
			goto cancel_receive;
		offer_data->fd = pipef[0];
		wl_data_offer_receive(offer,
				      MIME_TYPES[best_option],
				      pipef[1]);
		close(pipef[1]);
		tw_event_queue_add_source(queue, pipef[0], &event, EPOLLIN);

		return;
	}
cancel_receive:
	wl_data_offer_accept(offer, offer_data->serial, NULL);
	wl_data_offer_destroy(offer);
	offer_data->globals->inputs.wl_data_offer = NULL;
	free(offer_data);
}

static void
data_selection(void *data,
	       struct wl_data_device *wl_data_device,
	       struct wl_data_offer *id)
{}

static struct wl_data_device_listener data_device_listener = {
	.data_offer = data_offer,
	.enter = data_enter,
	.leave = data_leave,
	.motion = data_motion,
	.drop = data_drop,
	.selection = data_selection,
};


/*******************************************************************************
 * wl_globals
 ******************************************************************************/

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
	wl_data_device_release(globals->inputs.wl_data_device);
	wl_data_device_destroy(globals->inputs.wl_data_device);
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
		if (globals->wl_data_device_manager) {
			globals->inputs.wl_data_device =
				wl_data_device_manager_get_data_device(globals->wl_data_device_manager,
								       globals->inputs.wl_seat);
			wl_data_device_add_listener(globals->inputs.wl_data_device, &data_device_listener,
						    globals);
		}
	} else if (strcmp(interface, wl_compositor_interface.name) == 0) {
		globals->compositor = wl_registry_bind(wl_registry, name, &wl_compositor_interface, version);
	} else if (strcmp(interface, wl_shm_interface.name) == 0)  {
		globals->shm = wl_registry_bind(wl_registry, name, &wl_shm_interface, version);
		wl_shm_add_listener(globals->shm, &shm_listener, globals);
	} else if (strcmp(interface, wl_data_device_manager_interface.name) == 0) {
		globals->wl_data_device_manager = wl_registry_bind(wl_registry, name,
								   &wl_data_device_manager_interface,
								   version);
		if (globals->inputs.wl_seat) {
			globals->inputs.wl_data_device =
				wl_data_device_manager_get_data_device(globals->wl_data_device_manager,
								       globals->inputs.wl_seat);
			wl_data_device_add_listener(globals->inputs.wl_data_device, &data_device_listener,
						    globals);
		}
	} else {
		fprintf(stderr, "announcing global %s\n", interface);
		return 0;
	}
	return 1;
}
