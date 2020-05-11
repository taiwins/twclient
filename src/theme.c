/*
 * theme.c - taiwins client theme functions
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
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <os/os-compatibility.h>
#include <os/buffer.h>

#include <twclient/theme.h>
#include <wayland-util.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

/*******************************************************************************
 * tw_theme_color
 ******************************************************************************/

struct widget_colors {
	tw_rgba_t normal;
	tw_rgba_t hover;
	tw_rgba_t active;
};

struct tw_theme_color {
	uint32_t row_size; //this defines the text size as well
	tw_rgba_t window_color;
	tw_rgba_t header_color;
	tw_rgba_t border_color;
	//General colors
	tw_rgba_t text_color;
	tw_rgba_t edit_color;
	tw_rgba_t slider_color;
	tw_rgba_t combo_color;
	tw_rgba_t scroll_color;
	tw_rgba_t property_color;
	//widget color
	struct widget_colors button;
	struct widget_colors toggle;
	struct widget_colors select;
	struct widget_colors chart;
	struct widget_colors slider_cursor;
	struct widget_colors scroll_cursor;
	//spectials
	tw_rgba_t toggle_cursor;
	//we can contain a font name here. eventually the icon font is done by
	//searching all the svg in the widgets
	char font[64];
};

static const struct tw_theme_color taiwins_default_theme;

/*******************************************************************************
 * tw_theme_component
 ******************************************************************************/

static inline tw_rgba_t
tw_make_rgba(int r, int g, int b, int a)
{
	return (tw_rgba_t){
		.r = r, .g = g, .b = b, .a = a,
	};
}

static inline tw_vec2_t
tw_make_vec2(float x, float y)
{
	return (tw_vec2_t){.x = x, .y = y};
}

static inline theme_option_style
tw_make_color_item(tw_rgba_t rgba)
{
	return (theme_option_style){
		.valid = true,
		.style.type = TAIWINS_STYLE_COLOR,
		.style.data.color = rgba,
	};
}

static inline theme_option_style
tw_make_transparent_item()
{
	return (theme_option_style){
		.valid = true,
		.style.type = TAIWINS_STYLE_COLOR,
		.style.data.color.code = 0,
	};
}


static void
tw_make_trans_button(struct tw_style_button *button, tw_rgba_t bg_color,
                     tw_rgba_t text_color,
                     float padding, float rounding)
{
	//use this for now, if (0,0,0,0) works, we may be able to add different
	//text color
	button->normal = tw_make_color_item(bg_color);
	button->hover = tw_make_color_item(bg_color);
	button->active = tw_make_color_item(bg_color);
	button->border_color = tw_make_rgba(0, 0, 0, 0);
	button->text_background = bg_color;
	button->text_normal = text_color;
	button->text_hover = text_color;
	button->text_active = text_color;
	button->padding = tw_make_vec2(padding, padding);
	button->image_padding = tw_make_vec2(0.0f, 0.0f);
	button->text_alignment = TAIWINS_TEXT_CENTER;
	button->border = 0.0f;
	button->rounding = rounding;
}

static void
tw_make_trans_edit(struct tw_style_edit *edit, tw_rgba_t bg_color,
                   tw_rgba_t text_color, tw_rgba_t edit_color,
                   struct tw_style_scrollbar *scroll,
                   tw_vec2_t bar_size, float padding, float rounding)
{
	//use this for now, if (0,0,0,0) works, we may be able to add different
	//text color
	edit->normal = tw_make_color_item(bg_color);
	edit->hover = tw_make_color_item(bg_color);
	edit->active = tw_make_color_item(bg_color);
	edit->cursor_normal = text_color;
	edit->cursor_hover = text_color;
	edit->cursor_text_normal = edit_color;
	edit->cursor_text_hover = edit_color;
	edit->border_color = tw_make_rgba(0, 0, 0, 0);
	edit->text_normal = text_color;
	edit->text_hover = text_color;
	edit->text_active = text_color;
	edit->selected_normal = text_color;
	edit->selected_text_normal = edit_color;
	edit->selected_text_hover = edit_color;
	edit->scrollbar = *scroll;
	edit->scrollbar_size = bar_size;
	edit->padding = tw_make_vec2(padding, padding);
	edit->row_padding =  padding;
	edit->cursor_size = 4.0;
	edit->border = 0.0;
	edit->rounding = rounding;
}

/*******************************************************************************
 * tw_theme
 ******************************************************************************/

/* used in clients */
WL_EXPORT void
tw_theme_init_from_fd(struct tw_theme *theme, int fd, size_t size)
{
	void *addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	memcpy(theme, addr, sizeof(struct tw_theme));
	if (theme->handle_pool.size == 0)
		return;
	//initialize
	size_t handles_size = theme->handle_pool.size;
	size_t strings_size = theme->string_pool.size;
	wl_array_init(&theme->handle_pool);
	wl_array_init(&theme->string_pool);

	if (handles_size + strings_size + sizeof(struct tw_theme) != size)
		return;

	const void *handle_addr = (const char *)addr + sizeof(struct tw_theme);
	const void *strings_addr = (const char *)handle_addr +
		handles_size;

	wl_array_add(&theme->handle_pool, handles_size);
	wl_array_add(&theme->string_pool, strings_size);

	memcpy(theme->handle_pool.data, handle_addr, handles_size);
	memcpy(theme->string_pool.data, strings_addr, strings_size);
}

/* used in server */
WL_EXPORT int
tw_theme_to_fd(struct tw_theme *theme)
{
	void *mapped, *ummaped;
	size_t mapsize = sizeof(struct tw_theme) + theme->handle_pool.size +
		theme->string_pool.size;
	int fd = os_create_anonymous_file(mapsize);
	if (fd < 0)
		return -1;
	mapped = mmap(NULL, mapsize, PROT_WRITE, MAP_SHARED, fd, 0);
	if (!mapped) {
		close(fd);
		return -1;
	}
	ummaped = mapped;
	memcpy(mapped, theme, sizeof(struct tw_theme));
	((struct tw_theme *)mapped)->handle_pool.data = NULL;
	((struct tw_theme *)mapped)->string_pool.data = NULL;
	mapped = (char *)mapped + sizeof(struct tw_theme);
	memcpy(mapped, theme->handle_pool.data, theme->handle_pool.size);
	mapped = (char *)mapped + theme->handle_pool.size;
	memcpy(mapped, theme->string_pool.data, theme->string_pool.size);

	munmap(ummaped, mapsize);
	return fd;
}

WL_EXPORT void
tw_theme_init_default(struct tw_theme *theme)
{
	struct tw_style_text *text;
	struct tw_style_button *button;
	struct tw_style_toggle *toggle;
	struct tw_style_selectable *select;
	struct tw_style_slider *slider;
	struct tw_style_progress *prog;
	struct tw_style_property *prop;
	struct tw_style_edit *edit;
	struct tw_style_chart *chart;
	struct tw_style_scrollbar *scroll;
	struct tw_style_tab *tab;
	struct tw_style_combo *combo;
	struct tw_style_window_header *header;
	struct tw_style_window *window;
        const struct tw_theme_color *df = &taiwins_default_theme;

	float window_border = 2.0;
	float widget_border = 1.0;
	float no_border = 0.0;
	float rounding = 4.0;
	float no_rounding = 0.0;

	tw_vec2_t padding = tw_make_vec2(4.0f, 4.0f);
	tw_vec2_t half_padding = tw_make_vec2(2.0f, 2.0f);
	tw_vec2_t no_padding = tw_make_vec2(0.0f, 0.0f);
	tw_vec2_t scrollbar_size = tw_make_vec2(df->row_size / 2.0,
	                                        df->row_size / 2.0);

        memset(theme, 0, sizeof(struct tw_theme));
	wl_array_init(&theme->handle_pool);
	wl_array_init(&theme->string_pool);

	//text
	text = &theme->text;
        text->color = df->text_color;
        text->padding = no_padding;
	//text->default_font

	//button
	button = &theme->button;
        button->normal = tw_make_color_item(df->button.normal);
        button->hover = tw_make_color_item(df->button.hover);
        button->active = tw_make_color_item(df->button.active);
        button->border_color = df->border_color;
        button->text_background = df->button.normal;
        button->text_normal = df->text_color;
        button->text_hover = df->text_color;
        button->text_active = df->text_color;
        button->text_alignment = TAIWINS_TEXT_CENTER;
        button->border = widget_border;
        button->rounding = rounding;
        button->padding = half_padding;
        button->image_padding = half_padding;

	// contextual button
	button = &theme->contextual_button;
        button->normal = tw_make_color_item(df->window_color);
        button->hover = tw_make_color_item(df->button.hover);
        button->active = tw_make_color_item(df->button.active);
        button->border_color = df->window_color;
        button->text_background = df->window_color;
        button->text_normal = df->text_color;
        button->text_hover = df->text_color;
        button->text_active = df->text_color;
        button->text_alignment = TAIWINS_TEXT_CENTER;
        button->border = no_border;
        button->rounding = no_rounding;
        button->padding = half_padding;
        button->image_padding = no_padding;

	// menu button
	button = &theme->menu_button;
        button->normal = tw_make_color_item(df->window_color);
        button->hover = tw_make_color_item(df->window_color);
        button->active = tw_make_color_item(df->window_color);
        button->border_color = df->window_color;
        button->text_background = df->window_color;
        button->text_normal = df->text_color;
        button->text_hover = df->text_color;
        button->text_active = df->text_color;
        button->text_alignment = TAIWINS_TEXT_CENTER;
        button->border = no_border;
        button->rounding = no_rounding;
        button->padding = half_padding;
        button->image_padding = no_padding;

	// checkbox
	toggle = &theme->checkbox;
	toggle->normal = tw_make_color_item(df->toggle.normal);
	toggle->hover = tw_make_color_item(df->toggle.hover);
	toggle->active = tw_make_color_item(df->toggle.active);
	toggle->cursor_normal = tw_make_color_item(df->toggle_cursor);
	toggle->cursor_hover = toggle->cursor_normal;
	toggle->text_background = df->window_color;
	toggle->text_normal = df->text_color;
	toggle->text_hover = df->text_color;
	toggle->text_active = df->text_color;
	toggle->padding = half_padding;
	toggle->border_color = tw_make_rgba(0, 0, 0, 0);
	toggle->border = no_border;
	toggle->spacing = 4.0;

	// option
	toggle = &theme->option;
	*toggle = theme->checkbox;

	// seletable
	select = &theme->selectable;
	select->normal = tw_make_color_item(df->select.normal);
	select->hover = tw_make_color_item(df->select.normal);
	select->pressed = tw_make_color_item(df->select.normal);
	select->normal_active = tw_make_color_item(df->select.active);
	select->hover_active = tw_make_color_item(df->select.active);
	select->pressed_active = tw_make_color_item(df->select.active);
	select->text_normal = df->text_color;
	select->text_hover = df->text_color;
	select->text_pressed = df->text_color;
	select->text_normal_active = df->text_color;
	select->text_hover_active = df->text_color;
	select->text_pressed_active = df->text_color;
	select->padding = half_padding;
	select->image_padding = half_padding;
	select->rounding = no_rounding;

	// slider
	slider = &theme->slider;
	slider->normal = tw_make_transparent_item();
	slider->hover = tw_make_transparent_item();
	slider->active = tw_make_transparent_item();
	slider->bar_normal = df->slider_color;
	slider->bar_hover = df->slider_color;
	slider->bar_active = df->slider_color;
	slider->bar_filled = df->slider_color;
	slider->cursor_normal = tw_make_color_item(df->slider_cursor.normal);
	slider->cursor_hover = tw_make_color_item(df->slider_cursor.hover);
	slider->cursor_active = tw_make_color_item(df->slider_cursor.active);
	slider->cursor_size = tw_make_vec2(df->row_size, df->row_size);
	slider->padding = half_padding;
	slider->spacing = half_padding;
	slider->show_buttons = 0;
	slider->rounding = no_rounding;
	tw_make_trans_button(&slider->inc_button, df->slider_color,
	                     df->text_color, half_padding.x, no_rounding);
	tw_make_trans_button(&slider->dec_button, df->slider_color,
	                     df->text_color, half_padding.x, no_rounding);

	// progress bar
	prog = &theme->progress;
	prog->normal = tw_make_color_item(df->slider_color);
	prog->hover = tw_make_color_item(df->slider_color);
	prog->active = tw_make_color_item(df->slider_color);
	prog->cursor_normal = tw_make_color_item(df->slider_cursor.normal);
	prog->cursor_hover = tw_make_color_item(df->slider_cursor.hover);
	prog->cursor_active = tw_make_color_item(df->slider_cursor.active);
	prog->border_color = tw_make_rgba(0, 0, 0, 0);
	prog->padding = padding;
	prog->rounding = no_rounding;
	prog->border = no_border;
	prog->cursor_rounding = rounding;
	prog->cursor_border = no_border;

	// scrollbar
	scroll = &theme->scrollh;
	scroll->normal = tw_make_color_item(df->scroll_color);
	scroll->hover = tw_make_color_item(df->scroll_color);
	scroll->active = tw_make_color_item(df->scroll_color);
	scroll->cursor_normal = tw_make_color_item(df->scroll_cursor.normal);
	scroll->cursor_hover = tw_make_color_item(df->scroll_cursor.hover);
	scroll->cursor_active = tw_make_color_item(df->scroll_cursor.active);
	scroll->border_color = df->scroll_color;
	scroll->cursor_border_color = df->scroll_color;
	scroll->padding = no_padding;
	scroll->show_buttons = 0;
	scroll->border = no_border;
	scroll->rounding = rounding;
	scroll->border_cursor = no_border;
	scroll->rounding_cursor = rounding;
	tw_make_trans_button(&scroll->inc_button, df->scroll_color,
	                     df->text_color, half_padding.x, no_rounding);
	tw_make_trans_button(&scroll->dec_button, df->scroll_color,
	                     df->text_color, half_padding.x, no_rounding);
	theme->scrollv = theme->scrollh;

	// edit
	edit = &theme->edit;
	edit->normal = tw_make_color_item(df->edit_color);
	edit->hover = tw_make_color_item(df->edit_color);
	edit->active = tw_make_color_item(df->edit_color);
	edit->cursor_normal = df->text_color;
	edit->cursor_hover = df->text_color;
	edit->cursor_text_normal = df->edit_color;
	edit->cursor_text_hover = df->edit_color;
	edit->border_color = df->border_color;
	edit->text_normal = df->text_color;
	edit->text_hover = df->text_color;
	edit->text_active = df->text_color;
	edit->selected_normal = df->text_color;
	edit->selected_text_normal = df->edit_color;
	edit->selected_text_hover = df->edit_color;
	edit->scrollbar = theme->scrollh;
	edit->scrollbar_size = scrollbar_size;
	edit->padding = padding;
	edit->row_padding =  half_padding.x;
	edit->cursor_size = 4.0;
	edit->border = widget_border;
	edit->rounding = no_rounding;

	// property
	prop = &theme->property;
	prop->normal = tw_make_color_item(df->property_color);
	prop->hover = tw_make_color_item(df->property_color);
	prop->active = tw_make_color_item(df->property_color);
	prop->border_color = df->border_color;
	prop->label_normal = df->text_color;
	prop->label_hover = df->text_color;
	prop->label_active = df->text_color;
	prop->padding = padding;
	prop->border = widget_border;
	prop->rounding = rounding;
	tw_make_trans_button(&prop->inc_button, df->property_color,
	                     df->text_color,
	                     no_padding.x, no_rounding);
	tw_make_trans_button(&prop->dec_button, df->property_color,
	                     df->text_color, no_padding.x, no_rounding);
	tw_make_trans_edit(&prop->edit, df->property_color, df->text_color,
	                   df->edit_color, &theme->scrollv, scrollbar_size,
	                   no_padding.x, no_rounding);

	// chart
	chart = &theme->chart;
	chart->background = tw_make_color_item(df->chart.normal);
	chart->color = df->chart.active;
	chart->selected_color = df->chart.hover;
	chart->border_color = df->border_color;
	chart->padding = padding;
	chart->border = no_border;
	chart->rounding = no_rounding;

	//combo
	combo = &theme->combo;
	combo->normal = tw_make_color_item(df->combo_color);
	combo->hover = tw_make_color_item(df->combo_color);
	combo->active = tw_make_color_item(df->combo_color);
	combo->border_color = df->border_color;
	combo->label_normal = df->text_color;
	combo->label_hover = df->text_color;
	combo->label_active = df->text_color;
	combo->content_padding = padding;
	//why?
	combo->button_padding = tw_make_vec2(0, 4);
	combo->spacing = tw_make_vec2(4, 0);
	combo->border =  widget_border;
	combo->rounding = no_rounding;
	tw_make_trans_button(&combo->button, df->combo_color, df->text_color,
	                     half_padding.x, no_rounding);

	// tab
	tab = &theme->tab;
	tab->background = tw_make_color_item(df->header_color);
	tab->border_color = df->border_color;
	tab->text = df->text_color;
	tab->padding = padding;
	tab->spacing = padding;
	tab->indent = df->row_size / 2.0;
	tab->border = widget_border;
	tab->rounding = no_rounding;
	tw_make_trans_button(&tab->tab_button, df->header_color, df->text_color,
	                     half_padding.x, no_rounding);
	tw_make_trans_button(&tab->node_button, df->header_color, df->text_color,
	                     half_padding.x, no_rounding);

	// header
	header = &theme->window.header;
	header->align = TAIWINS_HEADER_RIGHT;
	header->normal =  tw_make_color_item(df->header_color);
	header->hover =  tw_make_color_item(df->header_color);
	header->active =  tw_make_color_item(df->header_color);
	header->label_normal = df->text_color;
	header->label_hover = df->text_color;
	header->label_active = df->text_color;
	header->label_padding = padding;
	header->padding = padding;
	header->spacing = no_padding;
	tw_make_trans_button(&header->button, df->header_color, df->text_color,
	                     0, no_rounding);

	// window
	window = &theme->window;
	window->background = tw_make_color_item(df->window_color);
	window->border_color = df->border_color;
	window->popup_border_color = df->border_color;
	window->combo_border_color = df->border_color;
	window->contextual_border_color = df->border_color;
	window->menu_border_color = df->border_color;
	window->group_border_color = df->border_color;
	window->tooltip_border_color = df->border_color;
	window->scaler = tw_make_color_item(df->text_color);

	window->border = window_border;
	window->combo_border = widget_border;
	window->contextual_border = widget_border;
	window->menu_border = widget_border;
	window->group_border = widget_border;
	window->tooltip_border = widget_border;
	window->popup_border = widget_border;

	window->rounding = no_rounding;
	window->spacing = padding;
	window->scrollbar_size = scrollbar_size;
	window->min_size = tw_make_vec2(64.0, 64.0);
	window->min_row_height_padding = df->row_size / 2.0;

	window->padding = padding;
	window->group_padding = padding;
	window->popup_padding = padding;
	window->combo_padding = padding;
	window->contextual_padding = padding;
	window->menu_padding = padding;
	window->tooltip_padding = padding;
}

WL_EXPORT void
tw_theme_fini(struct tw_theme *theme)
{
	if (theme->handle_pool.data)
		wl_array_release(&theme->handle_pool);
	if (theme->string_pool.data)
		wl_array_release(&theme->string_pool);
	memset(theme, 0, sizeof(struct tw_theme));
}

/*******************************************************************************
 * tw_theme_color templates
 ******************************************************************************/

static const struct tw_theme_color taiwins_default_theme = {
	.row_size = 16,
	.text_color = {
		.r=210, .g=210, .b=210, .a=255,
	},
	.window_color = {
		.r=57, .g=67, .b=71, .a=255,
	},
	.header_color = {
		.r=51, .g=51, .b=56, .a=255,
	},
	.edit_color = {
		.r=40, .g=58, .b=61, .a=255,
	},
	.border_color = {
		.r=46, .g=46, .b=46, .a=255,
	},
	.slider_color = {
		.r=50, .g=58, .b=61, .a=255,
	},
	.combo_color = {
		.r=50, .g=58, .b=61, .a=255,
	},
	.scroll_color = {
		.r=50, .g=58, .b=61, .a=255,
	},
	.property_color = {
		.r=50, .g=58, .b=61, .a=255,
	},
	.button = {
		{.r = 48, .g=83, .b=111, .a=255},
		{.r = 58, .g=93, .b=121, .a=255},
		{.r = 63, .g =98, .b=126, .a=255},
	},
	.toggle = {
		{.r=50, .g=58, .b=61, .a=255},
		{.r=45, .g=53, .b=56, .a=255},
		{.r=48, .g=83, .b=111, .a=255},
	},
	.select = {
		.normal = {.r=57, .g=67, .b=61, .a=255,},
		.active = {.r=48, .g=83, .b=111, .a=255,},
	},
	.chart = {
		{.r=50, .g=58, .b=61, .a=255},
		{.r=255, .a=255},
		{.r=48, .g=83, .b=111, .a=255},
	},
	.slider_cursor = {
		{.r=48, .g=83, .b=111, .a=245},
		{.r=53, .g=88, .b=116, .a=255},
		{.r=58, .g=93, .b=121, .a=255},
	},
	.scroll_cursor = {
		{.r=48, .g=83, .b=111, .a=255,},
		{.r=53, .g=88, .b=116, .a=255,},
		{.r=58, .g=93, .b=121, .a=255},
	},
	.toggle_cursor = {
		.r = 48, .g = 83, .b = 111, .a = 255,
	},
};
