/*
 * data_source.c - taiwins client data source functions
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/epoll.h>
#include <wayland-client.h>

#include <linux/input.h>

#include <sequential.h>
#include <os/buffer.h>
#include <os/file.h>

#include <client.h>
#include <ui.h>


/*******************************************************************************
 * externs
 ******************************************************************************/
extern void _app_surface_run_frame(struct app_surface *surf,
				   const struct app_event *e);

struct data_source {
	struct wl_data_source *source;
	struct wl_surface *surface;
	struct wl_globals *globals;
	uint32_t action_received;
};


static struct wl_data_source_listener data_source_listener;


//APIs
static inline struct data_source *
data_source_create(struct wl_data_source *source,
		   struct wl_surface *surface,
		   struct wl_globals *globals)
{
	struct data_source *s =
		malloc(sizeof(struct data_source));
	s->action_received = 0;
	s->surface = surface;
	s->globals = globals;
	wl_data_source_add_listener(source, &data_source_listener, s);
}

//what else

static inline void
data_source_destroy(struct wl_data_source *source)
{
	struct data_source *s =
		wl_data_source_get_user_data(source);
	free(s);
	wl_data_source_destroy(source);
}

static void
data_source_target(void *data,
		   struct wl_data_source *wl_data_source,
		   const char *mime_type)
{
	//no action need
}

static void
data_source_send(void *data,
		 struct wl_data_source *wl_data_source,
		 const char *mime_type,
		 int32_t fd)
{
	struct data_source *s = data;
	struct app_surface *app = app_surface_from_wl_surface(s->surface);
	struct app_event event= {
		.type = TW_COPY,
		.clipboard_source.write_fd = fd,
		.clipboard_source.mime_type = mime_type,
	};
	_app_surface_run_frame(app, &event);
}

static void
data_source_cancelled(void *data,
		      struct wl_data_source *wl_data_source)
{
	data_source_destroy(wl_data_source);
}

static void
data_source_dnd_drop_performed(void *data,
			       struct wl_data_source *wl_data_source)
{
	struct data_source *data_source = data;
	if (data_source->action_received ==
		WL_DATA_DEVICE_MANAGER_DND_ACTION_NONE)
		data_source_destroy(wl_data_source);
}

static void
data_source_dnd_finished(void *data,
			 struct wl_data_source *wl_data_source)
{
	data_source_destroy(wl_data_source);
}

static void
data_source_action(void *data,
		   struct wl_data_source *wl_data_source,
		   uint32_t dnd_action)
{
	struct data_source *source_data =
		wl_data_source_get_user_data(wl_data_source);
	source_data->action_received = dnd_action;
}


static struct wl_data_source_listener data_source_listener = {
	.target = data_source_target,
	.send = data_source_send,
	.cancelled = data_source_cancelled,
	.dnd_drop_performed = data_source_dnd_drop_performed,
	.dnd_finished = data_source_dnd_finished,
	.action = data_source_action,
};
