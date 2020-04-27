/*
 * ui.c - taiwins client gui related functions
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

#include <ui.h>
#include <string.h>
#include <stdio.h>
#include <cairo/cairo.h>
#include <wayland-client.h>
#include <fontconfig/fontconfig.h>

WL_EXPORT cairo_format_t
translate_wl_shm_format(enum wl_shm_format format)
{
	switch (format) {
	case WL_SHM_FORMAT_ARGB8888:
		return CAIRO_FORMAT_ARGB32;
		break;
	case WL_SHM_FORMAT_XRGB8888:
		//based on doc, xrgb8888 does not include alpha
		return CAIRO_FORMAT_RGB24;
		break;
	case WL_SHM_FORMAT_RGB565:
		return CAIRO_FORMAT_RGB16_565;
		break;
	case WL_SHM_FORMAT_RGBA8888:
		return CAIRO_FORMAT_INVALID;
		break;
	default:
		return CAIRO_FORMAT_INVALID;
	}
}

WL_EXPORT size_t
stride_of_wl_shm_format(enum wl_shm_format format)
{
	switch(format) {
	case WL_SHM_FORMAT_ARGB8888:
		return 4;
		break;
	case WL_SHM_FORMAT_RGB888:
		return 3;
		break;
	case WL_SHM_FORMAT_RGB565:
		return 2;
		break;
	case WL_SHM_FORMAT_RGBA8888:
		return 4;
		break;
	case WL_SHM_FORMAT_ABGR1555:
		return 2;
	default:
		return 0;
	}
}
