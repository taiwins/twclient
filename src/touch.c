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

}

static void
handle_touch_up(void *data,
		struct wl_touch *wl_touch,
		uint32_t serial,
		uint32_t time,
		int32_t id)
{

}

static void
handle_touch_motion(void *data,
		    struct wl_touch *wl_touch,
		    uint32_t time,
		    int32_t id,
		    wl_fixed_t x,
		    wl_fixed_t y)
{

}


static void
handle_touch_cancel(void *data,
		    struct wl_touch *wl_touch)
{
	//sent when compositor desides this is not an event you should have
}


static void
handle_touch_frame(void *data,
		   struct wl_touch *wl_touch)
{

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
