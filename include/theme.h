/*
 * theme.h - taiwins theme client header
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

#ifndef TW_THEME_H
#define TW_THEME_H

#include <stdint.h>
#include <wayland-util.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* we are exporting this header so lets not make a mess */
#ifndef OPTION
/* option directive give us ability to define types like std::option */
#define OPTION(type, name)					\
	struct tw_option_ ## name { bool valid; type name; }
#endif

typedef uint32_t tw_flags;

typedef struct {float x,y;} tw_vec2_t;

typedef union {
	uint32_t code;
	struct {
		uint8_t r,g,b,a;
	};
} tw_rgba_t;

/*
 * to be fair. It should be a path, but if that is the case. Why don't we just
 * create a path with specific name?
 */
typedef struct {
	uint32_t handle;
} tw_image_t;

enum tw_style_type {
	TAIWINS_STYLE_INVALID = 0,
	TAIWINS_STYLE_COLOR = 1,
	TAIWINS_STYLE_IMAGE = 2,
};

enum tw_text_alignment {
	TAIWINS_TEXT_INVALID = 0,
	TAIWINS_TEXT_LEFT = 1,
	TAIWINS_TEXT_RIGHT = 2,
	TAIWINS_TEXT_CENTER = 3,
};

union tw_style_data {
	tw_rgba_t color;
	tw_image_t image;
};

struct tw_style_item {
	enum tw_style_type type;
	union tw_style_data data;
};

typedef OPTION(tw_rgba_t, rgba) theme_option_rgba;
typedef OPTION(uint32_t, handle) theme_option_handle;
typedef OPTION(struct tw_style_item, style) theme_option_style;
typedef OPTION(uint32_t, font_size) theme_option_font_size;


/** generate basic types **/
struct tw_style_text {
	tw_rgba_t color;
	tw_vec2_t padding;
	theme_option_handle default_font;
};

struct tw_style_button {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;
	tw_rgba_t border_color;

	/* text */
	tw_rgba_t text_background;
	tw_rgba_t text_normal;
	tw_rgba_t text_hover;
	tw_rgba_t text_active;
	tw_flags text_alignment;

	/* properties */
	float border;
	float rounding;
	tw_vec2_t padding;
	tw_vec2_t image_padding;
	tw_vec2_t touch_padding;

	/* optional user callbacks */
	/* tw_handle userdata; */
	/* void(*draw_begin)(struct tw_command_buffer*, tw_handle userdata); */
	/* void(*draw_end)(struct tw_command_buffer*, tw_handle userdata); */
};

struct tw_style_toggle {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;
	tw_rgba_t border_color;

	/* cursor */
	theme_option_style cursor_normal;
	theme_option_style cursor_hover;

	/* text */
	tw_rgba_t text_normal;
	tw_rgba_t text_hover;
	tw_rgba_t text_active;
	tw_rgba_t text_background;
	tw_flags text_alignment;

	/* properties */
	tw_vec2_t padding;
	tw_vec2_t touch_padding;
	float spacing;
	float border;

	/* optional user callbacks */
	/* tw_handle userdata; */
	/* void(*draw_begin)(struct tw_command_buffer*, tw_handle); */
	/* void(*draw_end)(struct tw_command_buffer*, tw_handle); */
};

struct tw_style_selectable {
	/* background (inactive) */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style pressed;

	/* background (active) */
	theme_option_style normal_active;
	theme_option_style hover_active;
	theme_option_style pressed_active;

	/* text color (inactive) */
	tw_rgba_t text_normal;
	tw_rgba_t text_hover;
	tw_rgba_t text_pressed;

	/* text color (active) */
	tw_rgba_t text_normal_active;
	tw_rgba_t text_hover_active;
	tw_rgba_t text_pressed_active;
	tw_rgba_t text_background;
	tw_flags text_alignment;

	/* properties */
	float rounding;
	tw_vec2_t padding;
	tw_vec2_t touch_padding;
	tw_vec2_t image_padding;

	/* optional user callbacks */
	/* tw_handle userdata; */
	/* void(*draw_begin)(struct tw_command_buffer*, tw_handle); */
	/* void(*draw_end)(struct tw_command_buffer*, tw_handle); */
};

struct tw_style_slider {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;
	tw_rgba_t border_color;

	/* background bar */
	tw_rgba_t bar_normal;
	tw_rgba_t bar_hover;
	tw_rgba_t bar_active;
	tw_rgba_t bar_filled;

	/* cursor */
	theme_option_style cursor_normal;
	theme_option_style cursor_hover;
	theme_option_style cursor_active;

	/* properties */
	float border;
	float rounding;
	float bar_height;
	tw_vec2_t padding;
	tw_vec2_t spacing;
	tw_vec2_t cursor_size;

	/* optional buttons */
	int show_buttons;
	struct tw_style_button inc_button;
	struct tw_style_button dec_button;
	/* enum tw_symbol_type inc_symbol; */
	/* enum tw_symbol_type dec_symbol; */

	/* optional user callbacks */
	/* tw_handle userdata; */
	/* void(*draw_begin)(struct tw_command_buffer*, tw_handle); */
	/* void(*draw_end)(struct tw_command_buffer*, tw_handle); */
};

struct tw_style_progress {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;
	tw_rgba_t border_color;

	/* cursor */
	theme_option_style cursor_normal;
	theme_option_style cursor_hover;
	theme_option_style cursor_active;
	tw_rgba_t cursor_border_color;

	/* properties */
	float rounding;
	float border;
	float cursor_border;
	float cursor_rounding;
	tw_vec2_t padding;

	/* optional user callbacks */
	/* tw_handle userdata; */
	/* void(*draw_begin)(struct tw_command_buffer*, tw_handle); */
	/* void(*draw_end)(struct tw_command_buffer*, tw_handle); */
};

struct tw_style_scrollbar {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;
	tw_rgba_t border_color;

	/* cursor */
	theme_option_style cursor_normal;
	theme_option_style cursor_hover;
	theme_option_style cursor_active;
	tw_rgba_t cursor_border_color;

	/* properties */
	float border;
	float rounding;
	float border_cursor;
	float rounding_cursor;
	tw_vec2_t padding;

	/* optional buttons */
	int show_buttons;
	struct tw_style_button inc_button;
	struct tw_style_button dec_button;
	/* enum tw_symbol_type inc_symbol; */
	/* enum tw_symbol_type dec_symbol; */
};

struct tw_style_edit {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;
	tw_rgba_t border_color;
	struct tw_style_scrollbar scrollbar;

	/* cursor  */
	tw_rgba_t cursor_normal;
	tw_rgba_t cursor_hover;
	tw_rgba_t cursor_text_normal;
	tw_rgba_t cursor_text_hover;

	/* text (unselected) */
	tw_rgba_t text_normal;
	tw_rgba_t text_hover;
	tw_rgba_t text_active;

	/* text (selected) */
	tw_rgba_t selected_normal;
	tw_rgba_t selected_hover;
	tw_rgba_t selected_text_normal;
	tw_rgba_t selected_text_hover;

	/* properties */
	float border;
	float rounding;
	float cursor_size;
	tw_vec2_t scrollbar_size;
	tw_vec2_t padding;
	float row_padding;
};

struct tw_style_property {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;
	tw_rgba_t border_color;

	/* text */
	tw_rgba_t label_normal;
	tw_rgba_t label_hover;
	tw_rgba_t label_active;

	/* symbols */
	/* enum tw_symbol_type sym_left; */
	/* enum tw_symbol_type sym_right; */

	/* properties */
	float border;
	float rounding;
	tw_vec2_t padding;

	struct tw_style_edit edit;
	struct tw_style_button inc_button;
	struct tw_style_button dec_button;

	/* optional user callbacks */
	/* tw_handle userdata; */
	/* void(*draw_begin)(struct tw_command_buffer*, tw_handle); */
	/* void(*draw_end)(struct tw_command_buffer*, tw_handle); */
};

struct tw_style_chart {
	/* colors */
	theme_option_style background;
	tw_rgba_t border_color;
	tw_rgba_t selected_color;
	tw_rgba_t color;

	/* properties */
	float border;
	float rounding;
	tw_vec2_t padding;
};

struct tw_style_combo {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;
	tw_rgba_t border_color;

	/* label */
	tw_rgba_t label_normal;
	tw_rgba_t label_hover;
	tw_rgba_t label_active;

	/* symbol */
	tw_rgba_t symbol_normal;
	tw_rgba_t symbol_hover;
	tw_rgba_t symbol_active;

	/* button */
	struct tw_style_button button;
	/* enum tw_symbol_type sym_normal; */
	/* enum tw_symbol_type sym_hover; */
	/* enum tw_symbol_type sym_active; */

	/* properties */
	float border;
	float rounding;
	tw_vec2_t content_padding;
	tw_vec2_t button_padding;
	tw_vec2_t spacing;
};

struct tw_style_tab {
	/* background */
	theme_option_style background;
	tw_rgba_t border_color;
	tw_rgba_t text;

	/* button */
	struct tw_style_button tab_button; /**> non highlighted tree tab */
	struct tw_style_button node_button; /**> highlighted tree tab */
	/* enum tw_symbol_type sym_minimize; */
	/* enum tw_symbol_type sym_maximize; */

	/* properties */
	float border;
	float rounding;
	float indent;
	tw_vec2_t padding;
	tw_vec2_t spacing;
};

enum tw_style_header_align {
	TAIWINS_HEADER_LEFT,
	TAIWINS_HEADER_RIGHT
};
struct tw_style_window_header {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;

	/* button */
	struct tw_style_button button;
	/* enum tw_symbol_type close_symbol; */
	/* enum tw_symbol_type minimize_symbol; */
	/* enum tw_symbol_type maximize_symbol; */

	/* title */
	tw_rgba_t label_normal;
	tw_rgba_t label_hover;
	tw_rgba_t label_active;

	/* properties */
	enum tw_style_header_align align;
	tw_vec2_t padding;
	tw_vec2_t label_padding;
	tw_vec2_t spacing;
};

struct tw_style_window {
	struct tw_style_window_header header;
	theme_option_style background;

	tw_rgba_t border_color;
	tw_rgba_t popup_border_color;
	tw_rgba_t combo_border_color;
	tw_rgba_t contextual_border_color;
	tw_rgba_t menu_border_color;
	tw_rgba_t group_border_color;
	tw_rgba_t tooltip_border_color;
	theme_option_style scaler;

	float border;
	float combo_border;
	float contextual_border;
	float menu_border;
	float group_border;
	float tooltip_border;
	float popup_border;
	float min_row_height_padding;

	float rounding;
	tw_vec2_t spacing;
	tw_vec2_t scrollbar_size;
	tw_vec2_t min_size;

	tw_vec2_t padding;
	tw_vec2_t group_padding;
	tw_vec2_t popup_padding;
	tw_vec2_t combo_padding;
	tw_vec2_t contextual_padding;
	tw_vec2_t menu_padding;
	tw_vec2_t tooltip_padding;
};

/* we don't need to follow exactly what nuklear does, many of the elements are
 * redundent, we can certainly clean it up later
 */
struct tw_theme {
	theme_option_font_size pending_font_size;
	struct tw_style_text text;
	struct tw_style_button button;
	struct tw_style_button contextual_button;
	struct tw_style_button menu_button;
	struct tw_style_toggle option;
	struct tw_style_toggle checkbox;
	struct tw_style_selectable selectable;
	struct tw_style_slider slider;
	struct tw_style_progress progress;
	struct tw_style_property property;
	struct tw_style_edit edit;
	struct tw_style_chart chart;
	struct tw_style_scrollbar scrollh;
	struct tw_style_scrollbar scrollv;
	struct tw_style_tab tab;
	struct tw_style_combo combo;
	struct tw_style_window window;
	/* handles for images, which will be mark as start point in the string
	   pool, it works as a sparse string */
	struct wl_array handle_pool;
	/* strings with '\0' at the end. */
	struct wl_array string_pool;
};

struct widget_colors {
	tw_rgba_t normal;
	tw_rgba_t hover;
	tw_rgba_t active;
};

struct tw_theme_color {
	uint32_t row_size; //this defines the text size as well
	tw_rgba_t window_color;
	tw_rgba_t border_color;
	//text colors
	tw_rgba_t text_color;
	tw_rgba_t edit_color;
	//widget color
	struct widget_colors button;
	struct widget_colors toggle;
	struct widget_colors select;
	struct widget_colors chart;
	struct widget_colors slider;
	//spectials
	tw_rgba_t combo_color;
	tw_rgba_t slider_bg_color;
	//we can contain a font name here. eventually the icon font is done by
	//searching all the svg in the widgets
	char font[64];
};

void tw_theme_init_default(struct tw_theme *theme);

void tw_theme_init_from_fd(struct tw_theme *theme, int fd, size_t size);

int tw_theme_to_fd(struct tw_theme *theme);

#ifdef __cplusplus
}
#endif

#endif /* EOF */
