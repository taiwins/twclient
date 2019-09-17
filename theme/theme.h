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

typedef uint32_t taiwins_flags;

typedef struct {float x,y;} taiwins_vec2_t;

typedef union {
	uint32_t code;
	struct {
		uint8_t r,g,b,a;
	};
} taiwins_rgba_t;

/*
 * to be fair. It should be a path, but if that is the case. Why don't we just
 * create a path with specific name?
 */
typedef struct {
	uint32_t handle;
} taiwins_image_t;

enum taiwins_style_type {
	TAIWINS_STYLE_COLOR = 0,
	TAIWINS_STYLE_IMAGE = 1,
};

enum taiwins_text_alignment {
	TAIWINS_TEXT_LEFT = 0,
	TAIWINS_TEXT_RIGHT = 1,
	TAIWINS_TEXT_CENTER = 2,
};

union taiwins_style_data {
	taiwins_rgba_t color;
	taiwins_image_t image;
};

struct taiwins_style_item {
	enum taiwins_style_type type;
	union taiwins_style_data data;
};

typedef OPTION(taiwins_rgba_t, rgba) theme_option_rgba;
typedef OPTION(const uint32_t, handle) theme_option_handle;
typedef OPTION(struct taiwins_style_item, style) theme_option_style;
typedef OPTION(uint32_t, font_size) theme_option_font_size;

struct taiwins_style_text {
	taiwins_rgba_t color;
	taiwins_vec2_t padding;
};

struct taiwins_style_button {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;
	taiwins_rgba_t border_color;

	/* text */
	taiwins_rgba_t text_background;
	taiwins_rgba_t text_normal;
	taiwins_rgba_t text_hover;
	taiwins_rgba_t text_active;
	taiwins_flags text_alignment;

	/* properties */
	float border;
	float rounding;
	taiwins_vec2_t padding;
	taiwins_vec2_t image_padding;
	taiwins_vec2_t touch_padding;

	/* optional user callbacks */
	/* taiwins_handle userdata; */
	/* void(*draw_begin)(struct taiwins_command_buffer*, taiwins_handle userdata); */
	/* void(*draw_end)(struct taiwins_command_buffer*, taiwins_handle userdata); */
};

struct taiwins_style_toggle {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;
	taiwins_rgba_t border_color;

	/* cursor */
	theme_option_style cursor_normal;
	theme_option_style cursor_hover;

	/* text */
	taiwins_rgba_t text_normal;
	taiwins_rgba_t text_hover;
	taiwins_rgba_t text_active;
	taiwins_rgba_t text_background;
	taiwins_flags text_alignment;

	/* properties */
	taiwins_vec2_t padding;
	taiwins_vec2_t touch_padding;
	float spacing;
	float border;

	/* optional user callbacks */
	/* taiwins_handle userdata; */
	/* void(*draw_begin)(struct taiwins_command_buffer*, taiwins_handle); */
	/* void(*draw_end)(struct taiwins_command_buffer*, taiwins_handle); */
};

struct taiwins_style_selectable {
	/* background (inactive) */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style pressed;

	/* background (active) */
	theme_option_style normal_active;
	theme_option_style hover_active;
	theme_option_style pressed_active;

	/* text color (inactive) */
	taiwins_rgba_t text_normal;
	taiwins_rgba_t text_hover;
	taiwins_rgba_t text_pressed;

	/* text color (active) */
	taiwins_rgba_t text_normal_active;
	taiwins_rgba_t text_hover_active;
	taiwins_rgba_t text_pressed_active;
	taiwins_rgba_t text_background;
	taiwins_flags text_alignment;

	/* properties */
	float rounding;
	taiwins_vec2_t padding;
	taiwins_vec2_t touch_padding;
	taiwins_vec2_t image_padding;

	/* optional user callbacks */
	/* taiwins_handle userdata; */
	/* void(*draw_begin)(struct taiwins_command_buffer*, taiwins_handle); */
	/* void(*draw_end)(struct taiwins_command_buffer*, taiwins_handle); */
};

struct taiwins_style_slider {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;
	taiwins_rgba_t border_color;

	/* background bar */
	taiwins_rgba_t bar_normal;
	taiwins_rgba_t bar_hover;
	taiwins_rgba_t bar_active;
	taiwins_rgba_t bar_filled;

	/* cursor */
	theme_option_style cursor_normal;
	theme_option_style cursor_hover;
	theme_option_style cursor_active;

	/* properties */
	float border;
	float rounding;
	float bar_height;
	taiwins_vec2_t padding;
	taiwins_vec2_t spacing;
	taiwins_vec2_t cursor_size;

	/* optional buttons */
	int show_buttons;
	struct taiwins_style_button inc_button;
	struct taiwins_style_button dec_button;
	/* enum taiwins_symbol_type inc_symbol; */
	/* enum taiwins_symbol_type dec_symbol; */

	/* optional user callbacks */
	/* taiwins_handle userdata; */
	/* void(*draw_begin)(struct taiwins_command_buffer*, taiwins_handle); */
	/* void(*draw_end)(struct taiwins_command_buffer*, taiwins_handle); */
};

struct taiwins_style_progress {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;
	taiwins_rgba_t border_color;

	/* cursor */
	theme_option_style cursor_normal;
	theme_option_style cursor_hover;
	theme_option_style cursor_active;
	taiwins_rgba_t cursor_border_color;

	/* properties */
	float rounding;
	float border;
	float cursor_border;
	float cursor_rounding;
	taiwins_vec2_t padding;

	/* optional user callbacks */
	/* taiwins_handle userdata; */
	/* void(*draw_begin)(struct taiwins_command_buffer*, taiwins_handle); */
	/* void(*draw_end)(struct taiwins_command_buffer*, taiwins_handle); */
};

struct taiwins_style_scrollbar {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;
	taiwins_rgba_t border_color;

	/* cursor */
	theme_option_style cursor_normal;
	theme_option_style cursor_hover;
	theme_option_style cursor_active;
	taiwins_rgba_t cursor_border_color;

	/* properties */
	float border;
	float rounding;
	float border_cursor;
	float rounding_cursor;
	taiwins_vec2_t padding;

	/* optional buttons */
	int show_buttons;
	struct taiwins_style_button inc_button;
	struct taiwins_style_button dec_button;
	/* enum taiwins_symbol_type inc_symbol; */
	/* enum taiwins_symbol_type dec_symbol; */

	/* optional user callbacks */
	/* taiwins_handle userdata; */
	/* void(*draw_begin)(struct taiwins_command_buffer*, taiwins_handle); */
	/* void(*draw_end)(struct taiwins_command_buffer*, taiwins_handle); */
};

struct taiwins_style_edit {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;
	taiwins_rgba_t border_color;
	struct taiwins_style_scrollbar scrollbar;

	/* cursor  */
	taiwins_rgba_t cursor_normal;
	taiwins_rgba_t cursor_hover;
	taiwins_rgba_t cursor_text_normal;
	taiwins_rgba_t cursor_text_hover;

	/* text (unselected) */
	taiwins_rgba_t text_normal;
	taiwins_rgba_t text_hover;
	taiwins_rgba_t text_active;

	/* text (selected) */
	taiwins_rgba_t selected_normal;
	taiwins_rgba_t selected_hover;
	taiwins_rgba_t selected_text_normal;
	taiwins_rgba_t selected_text_hover;

	/* properties */
	float border;
	float rounding;
	float cursor_size;
	taiwins_vec2_t scrollbar_size;
	taiwins_vec2_t padding;
	float row_padding;
};

struct taiwins_style_property {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;
	taiwins_rgba_t border_color;

	/* text */
	taiwins_rgba_t label_normal;
	taiwins_rgba_t label_hover;
	taiwins_rgba_t label_active;

	/* symbols */
	/* enum taiwins_symbol_type sym_left; */
	/* enum taiwins_symbol_type sym_right; */

	/* properties */
	float border;
	float rounding;
	taiwins_vec2_t padding;

	struct taiwins_style_edit edit;
	struct taiwins_style_button inc_button;
	struct taiwins_style_button dec_button;

	/* optional user callbacks */
	/* taiwins_handle userdata; */
	/* void(*draw_begin)(struct taiwins_command_buffer*, taiwins_handle); */
	/* void(*draw_end)(struct taiwins_command_buffer*, taiwins_handle); */
};

struct taiwins_style_chart {
	/* colors */
	theme_option_style background;
	taiwins_rgba_t border_color;
	taiwins_rgba_t selected_color;
	taiwins_rgba_t color;

	/* properties */
	float border;
	float rounding;
	taiwins_vec2_t padding;
};

struct taiwins_style_combo {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;
	taiwins_rgba_t border_color;

	/* label */
	taiwins_rgba_t label_normal;
	taiwins_rgba_t label_hover;
	taiwins_rgba_t label_active;

	/* symbol */
	taiwins_rgba_t symbol_normal;
	taiwins_rgba_t symbol_hover;
	taiwins_rgba_t symbol_active;

	/* button */
	struct taiwins_style_button button;
	/* enum taiwins_symbol_type sym_normal; */
	/* enum taiwins_symbol_type sym_hover; */
	/* enum taiwins_symbol_type sym_active; */

	/* properties */
	float border;
	float rounding;
	taiwins_vec2_t content_padding;
	taiwins_vec2_t button_padding;
	taiwins_vec2_t spacing;
};

struct taiwins_style_tab {
	/* background */
	theme_option_style background;
	taiwins_rgba_t border_color;
	taiwins_rgba_t text;

	/* button */
	struct taiwins_style_button tab_maximize_button;
	struct taiwins_style_button tab_minimize_button;
	struct taiwins_style_button node_maximize_button;
	struct taiwins_style_button node_minimize_button;
	/* enum taiwins_symbol_type sym_minimize; */
	/* enum taiwins_symbol_type sym_maximize; */

	/* properties */
	float border;
	float rounding;
	float indent;
	taiwins_vec2_t padding;
	taiwins_vec2_t spacing;
};

enum taiwins_style_header_align {
	TAIWINS_HEADER_LEFT,
	TAIWINS_HEADER_RIGHT
};
struct taiwins_style_window_header {
	/* background */
	theme_option_style normal;
	theme_option_style hover;
	theme_option_style active;

	/* button */
	struct taiwins_style_button close_button;
	struct taiwins_style_button minimize_button;
	/* enum taiwins_symbol_type close_symbol; */
	/* enum taiwins_symbol_type minimize_symbol; */
	/* enum taiwins_symbol_type maximize_symbol; */

	/* title */
	taiwins_rgba_t label_normal;
	taiwins_rgba_t label_hover;
	taiwins_rgba_t label_active;

	/* properties */
	enum taiwins_style_header_align align;
	taiwins_vec2_t padding;
	taiwins_vec2_t label_padding;
	taiwins_vec2_t spacing;
};

struct taiwins_style_window {
	struct taiwins_style_window_header header;
	theme_option_style fixed_background;
	taiwins_rgba_t background;

	taiwins_rgba_t border_color;
	taiwins_rgba_t popup_border_color;
	taiwins_rgba_t combo_border_color;
	taiwins_rgba_t contextual_border_color;
	taiwins_rgba_t menu_border_color;
	taiwins_rgba_t group_border_color;
	taiwins_rgba_t tooltip_border_color;
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
	taiwins_vec2_t spacing;
	taiwins_vec2_t scrollbar_size;
	taiwins_vec2_t min_size;

	taiwins_vec2_t padding;
	taiwins_vec2_t group_padding;
	taiwins_vec2_t popup_padding;
	taiwins_vec2_t combo_padding;
	taiwins_vec2_t contextual_padding;
	taiwins_vec2_t menu_padding;
	taiwins_vec2_t tooltip_padding;
};

//server is responsible for creating such a
struct taiwins_theme {
	theme_option_font_size pending_font_size;
	theme_option_handle text_font;
	theme_option_handle icon_font;
	struct taiwins_style_text text;
	struct taiwins_style_button button;
	struct taiwins_style_button contextual_button;
	struct taiwins_style_button menu_button;
	struct taiwins_style_toggle option;
	struct taiwins_style_toggle checkbox;
	struct taiwins_style_selectable selectable;
	struct taiwins_style_slider slider;
	struct taiwins_style_progress progress;
	struct taiwins_style_property property;
	struct taiwins_style_edit edit;
	struct taiwins_style_chart chart;
	struct taiwins_style_scrollbar scrollh;
	struct taiwins_style_scrollbar scrollv;
	struct taiwins_style_tab tab;
	struct taiwins_style_combo combo;
	struct taiwins_style_window window;
	/* handles for images, which will be mark as start point in the string
	   pool, it works as a sparse string */
	struct wl_array handle_pool;
	/* strings with '\0' at the end. */
	struct wl_array string_pool;
};

#ifdef __cplusplus
}
#endif

#endif /* EOF */
