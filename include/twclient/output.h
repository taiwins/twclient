/*
 * output.h - taiwins client wl_output interface
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

#ifndef TW_CLIENT_OUTPUT_H
#define TW_CLIENT_OUTPUT_H

#include "util.h"

#ifdef __cplusplus
extern "C" {
#endif

struct tw_globals;

struct tw_output {
	struct tw_global proxy;
	struct wl_output *wl_output;

	int x, y, s;
	unsigned int w, h;
	enum wl_output_transform transform;

	struct {
		struct tw_signal remove;
		struct tw_signal info;
	} signals;
};

struct tw_output *
tw_output_create(struct wl_output *output);

struct tw_bbox
tw_output_get_bbox(struct tw_output *output);

bool
tw_global_is_tw_output(struct tw_global *global);

void
tw_output_for_each(struct tw_globals *globals,
                   tw_global_iterator_func_t iterator, void *user_data);

#ifdef __cplusplus
}
#endif




#endif /* EOF */
