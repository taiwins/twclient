/*
 * output.c - taiwins client wl_output implementation
 *
 * Copyright (c) 2021 Xichen Zhou
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

#include <assert.h>
#include <stdlib.h>
#include <twclient/output.h>
#include <wayland-client-core.h>
#include <wayland-client-protocol.h>
#include <wayland-util.h>

static void
handle_output_geometry(void *data, struct wl_output *wl_output,
                       int32_t x, int32_t y,
                       int32_t physical_width,
                       int32_t physical_height,
                       int32_t subpixel, const char *make, const char *model,
                       int32_t transform)
{
	struct tw_output *output = data;

	output->x = x;
	output->y = y;
	output->transform = transform;

}

static void
handle_output_mode(void *data, struct wl_output *wl_output, uint32_t flags,
                   int32_t width, int32_t height, int32_t refresh)
{
	struct tw_output *output = data;

	output->w = width;
	output->h = height;
}

static void
handle_output_scale(void *data,
                    struct wl_output *wl_output,
                    int32_t factor)
{
	struct tw_output *output = data;

	output->s = factor;
}

static void
handle_output_done(void *data, struct wl_output *wl_output)

{
	struct tw_output *output = data;
	tw_signal_emit(&output->signals.info, output);
}

static const struct wl_output_listener wl_output_listener = {
	.geometry = handle_output_geometry,
	.mode = handle_output_mode,
	.scale = handle_output_scale,
	.done = handle_output_done,
};

static inline struct tw_output *
tw_output_from_proxy(struct wl_proxy *proxy)
{
	assert(wl_proxy_get_listener(proxy) == &wl_output_listener);
	return wl_proxy_get_user_data(proxy);
}

static void
handle_wl_output_remove(struct wl_proxy *proxy)
{
	struct tw_output *output = tw_output_from_proxy(proxy);
	//the list is already removed
	free(output);
}

WL_EXPORT struct tw_output *
tw_output_create(struct wl_output *wl_output)
{
	struct tw_output *output = calloc(1, sizeof(*output));
	if (!output)
		return NULL;
	output->wl_output = wl_output;
	tw_global_init(&output->proxy, (struct wl_proxy *)wl_output,
	               handle_wl_output_remove);
	tw_signal_init(&output->signals.create);
	tw_signal_init(&output->signals.remove);
	tw_signal_init(&output->signals.info);

	wl_output_add_listener(wl_output, &wl_output_listener, output);

	tw_signal_emit(&output->signals.create, output);
        return output;
}

WL_EXPORT bool
tw_global_is_tw_output(struct tw_global *global)
{
	return wl_proxy_get_listener(global->object) == &wl_output_listener;
}

WL_EXPORT struct tw_bbox
tw_output_get_bbox(struct tw_output *output)
{
	uint32_t w, h;

	if ((output->transform % WL_OUTPUT_TRANSFORM_180) == 0) {
		w = output->w;
		h = output->h;
	} else {
		w = output->h;
		h = output->w;
	}
	w /= output->s;
	h /= output->s;

	return tw_make_bbox(output->x, output->y, w, h, output->s);
}
