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

cairo_format_t
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

size_t
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

//text color does not change for buttons and UIs.
//the changing color is edit color
//text-edit-curosr changes color to text-color and text

//button color can be a little different, so as the edit,
//edit is a little darker and button can be lighter,
//the slider, cursor they are all different.
//quite unpossible to have them be the same

const struct taiwins_theme_color taiwins_dark_theme = {
	.row_size = 16,
	.text_color = {
		.r = 210, .g = 210, .b = 210, .a = 255,
	},
	.text_active_color = {
		.r = 40, .g = 58, .b = 61, .a=255,
	},
	.window_color = {
		.r = 57, .g = 67, .b = 71, .a=215,
	},
	.border_color = {
		.r = 46, .g=46, .b=46, .a=255,
	},
	.slider_bg_color = {
		.r = 50, .g=58, .b=61, .a=255,
	},
	.combo_color = {
		.r = 50, .g=58, .b=61, .a=255,
	},
	.button = {
		{.r = 48, .g=83, .b=111, .a=255},
		{.r = 58, .g=93, .b=121, .a=255},
		{.r = 63, .g =98, .b=126, .a=255},
	},
	.toggle = {
		{.r = 50, .g = 58, .b = 61, .a = 255},
		{.r = 45, .g = 53, .b = 56, .a = 255},
		{.r = 48, .g = 83, .b = 111, .a = 255},
	},
	.select = {
		.normal = {.r = 57, .g = 67, .b=61, .a=255,},
		.active = {.r = 48, .g = 83, .b=111, .a=255},
	},
	.slider = {
		{.r = 48, .g = 83, .b = 111, .a=245},
		{.r = 53, .g = 88, .b = 116, .a=255},
		{.r = 58, .g = 93, .b = 121, .a=255},
	},
	.chart = {
		{.r = 50, .g = 58, .b=61, .a=255},
		{.r = 255, .a = 255},
		{.r = 48, .g = 83, .b=111, .a=255},
	},
};
