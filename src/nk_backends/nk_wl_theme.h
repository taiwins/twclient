/*
 * nk_wl_theme.h - taiwins client nuklear theme handling functions
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

#ifndef NK_WL_THEME_H
#define NK_WL_THEME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <assert.h>
#include <cairo/cairo.h>
#include <helpers.h>

#include <theme.h>
//include itself
#include <image_cache.h>
#include "nk_wl_internal.h"


////////////////////////// inlines ////////////////////////////////
static inline struct nk_color
nk_color_from_tw(const tw_rgba_t *tc)
{
	struct nk_color nc;
	nc.r = tc->r; nc.g = tc->g;
	nc.b = tc->b; nc.a = tc->a;
	return nc;
}

static inline struct nk_vec2
nk_vec2_from_tw(const tw_vec2_t *vec)
{
	struct nk_vec2 v;
	v.x = vec->x;
	v.y = vec->y;
	return v;
}

static inline struct nk_style_item
nk_style_item_from_tw(const struct tw_style_item *item,
		      const struct nk_image *image_pool)
{
    struct nk_style_item i;

    if (item->type == TAIWINS_STYLE_IMAGE) {
	    i.data.image = image_pool[item->data.image.handle];
	    i.type = NK_STYLE_ITEM_IMAGE;
    }
    else {
	    i.data.color = nk_color_from_tw(&item->data.color);
	    i.type = NK_STYLE_ITEM_COLOR;
    }
    return i;
}

static inline nk_flags
nk_alignment_from_tw(const enum tw_text_alignment alignment)
{
	return (alignment == TAIWINS_TEXT_LEFT) ? NK_TEXT_LEFT :
		(alignment == TAIWINS_TEXT_CENTER) ? NK_TEXT_CENTERED :
		NK_TEXT_RIGHT;
}


struct image_cache
nk_wl_build_theme_images(struct tw_theme *theme)
{
	return image_cache_from_arrays(&theme->handle_pool,
	                               &theme->string_pool, NULL);
}

/******************************** build themes ***********************************/

static void
nk_button_style_from_tw(struct nk_style_button *button,
			const struct tw_style_button *src_button,
			const struct nk_image *images)
{
	memset(button, 0, sizeof(*button));
	//button
	button->normal =
		nk_style_item_from_tw(&src_button->normal.style, images);
	button->hover =
		nk_style_item_from_tw(&src_button->hover.style, images);
	button->active =
		nk_style_item_from_tw(&src_button->active.style, images);
	//text
	button->text_background =
		nk_color_from_tw(&src_button->text_background);
	button->text_normal =
		nk_color_from_tw(&src_button->text_normal);
	button->text_hover =
		nk_color_from_tw(&src_button->text_hover);
	button->text_active =
		nk_color_from_tw(&src_button->text_active);
	//properties
	button->border_color =
		nk_color_from_tw(&src_button->border_color);
	button->border = src_button->border;
	button->rounding = src_button->rounding;
	button->padding =
		nk_vec2_from_tw(&src_button->padding);
	button->image_padding =
		nk_vec2_from_tw(&src_button->image_padding);
	button->touch_padding =
		nk_vec2_from_tw(&src_button->touch_padding);
	button->text_alignment =
		nk_alignment_from_tw(src_button->text_alignment);

	button->userdata        = nk_handle_ptr(0);
	button->draw_begin      = 0;
	button->draw_end        = 0;
}

static void
nk_toggle_style_from_tw(struct nk_style_toggle *toggle,
			const struct tw_style_toggle *src_toggle,
			const struct nk_image *images)
{
	memset(toggle, 0, sizeof(*toggle));
	toggle->normal =
		nk_style_item_from_tw(&src_toggle->normal.style, images);
	toggle->hover =
		nk_style_item_from_tw(&src_toggle->hover.style, images);
	toggle->active =
		nk_style_item_from_tw(&src_toggle->active.style, images);

	toggle->text_background =
		nk_color_from_tw(&src_toggle->text_background);
	toggle->text_normal =
		nk_color_from_tw(&src_toggle->text_normal);
	toggle->text_hover =
		nk_color_from_tw(&src_toggle->text_hover);
	toggle->text_active =
		nk_color_from_tw(&src_toggle->text_active);
	toggle->text_alignment =
		nk_alignment_from_tw(src_toggle->text_alignment);
	//cursor
	toggle->cursor_normal =
		nk_style_item_from_tw(&src_toggle->cursor_normal.style, images);
	toggle->cursor_hover =
		nk_style_item_from_tw(&src_toggle->cursor_hover.style, images);
	//properties
	toggle->padding =
		nk_vec2_from_tw(&src_toggle->padding);
	toggle->touch_padding =
		nk_vec2_from_tw(&src_toggle->touch_padding);
	toggle->border_color =
		nk_color_from_tw(&src_toggle->border_color);
	toggle->border = src_toggle->border;
	toggle->spacing = src_toggle->spacing;
	toggle->userdata = nk_handle_ptr(0);
}

static void
nk_selectable_style_from_tw(struct nk_style_selectable *select,
			    const struct tw_style_selectable *src_select,
			    const struct nk_image *images)
{
	memset(select, 0, sizeof(*select));
	//background inactive
	select->normal =
		nk_style_item_from_tw(&src_select->normal.style, images);
	select->hover =
		nk_style_item_from_tw(&src_select->hover.style, images);
	select->pressed =
		nk_style_item_from_tw(&src_select->pressed.style, images);
	//background active
	select->normal_active =
		nk_style_item_from_tw(&src_select->normal_active.style, images);
	select->hover_active =
		nk_style_item_from_tw(&src_select->hover_active.style, images);
	select->pressed_active =
		nk_style_item_from_tw(&src_select->pressed_active.style, images);

	//text inactive
	select->text_normal =
		nk_color_from_tw(&src_select->text_normal);
	select->text_hover =
		nk_color_from_tw(&src_select->text_hover);
	select->text_pressed =
		nk_color_from_tw(&src_select->text_pressed);
	//text active
	select->text_normal_active =
		nk_color_from_tw(&src_select->text_normal_active);
	select->text_hover_active =
		nk_color_from_tw(&src_select->text_hover_active);
	select->text_pressed_active =
		nk_color_from_tw(&src_select->text_pressed_active);
	select->text_background =
		nk_color_from_tw(&src_select->text_background);
	select->text_alignment =
		nk_alignment_from_tw(src_select->text_alignment);

	//properties
	select->padding =
		nk_vec2_from_tw(&src_select->padding);
	select->touch_padding =
		nk_vec2_from_tw(&src_select->touch_padding);
	select->image_padding =
		nk_vec2_from_tw(&src_select->image_padding);

	select->userdata = nk_handle_ptr(0);
	select->draw_begin      = 0;
	select->draw_end        = 0;
}

static void
nk_slider_style_from_tw(struct nk_style_slider *slider,
			const struct tw_style_slider *src_slider,
			const struct nk_image *images)
{
	memset(slider, 0, sizeof(*slider));
	//background
	slider->normal =
		nk_style_item_from_tw(&src_slider->normal.style, images);
	slider->hover =
		nk_style_item_from_tw(&src_slider->hover.style, images);
	slider->active =
		nk_style_item_from_tw(&src_slider->active.style, images);
	//bar
	slider->bar_normal =
		nk_color_from_tw(&src_slider->bar_normal);
	slider->bar_hover =
		nk_color_from_tw(&src_slider->bar_hover);
	slider->bar_active =
		nk_color_from_tw(&src_slider->bar_active);
	slider->bar_filled =
		nk_color_from_tw(&src_slider->bar_filled);
	//cursor
	slider->cursor_normal =
		nk_style_item_from_tw(&src_slider->cursor_normal.style, images);
	slider->cursor_hover =
		nk_style_item_from_tw(&src_slider->cursor_hover.style, images);
	slider->cursor_active =
		nk_style_item_from_tw(&src_slider->cursor_active.style, images);
	slider->cursor_size = nk_vec2_from_tw(&src_slider->cursor_size);
	//
	slider->inc_symbol = NK_SYMBOL_TRIANGLE_RIGHT;
	slider->dec_symbol = NK_SYMBOL_TRIANGLE_LEFT;
	//properties
	slider->padding = nk_vec2_from_tw(&src_slider->padding);
	slider->spacing = nk_vec2_from_tw(&src_slider->spacing);
	slider->show_buttons = src_slider->show_buttons;
	slider->bar_height = src_slider->bar_height;
	slider->rounding = src_slider->rounding;

	slider->userdata = nk_handle_ptr(0);
	slider->draw_begin      = 0;
	slider->draw_end        = 0;

	/* slider buttons */
	nk_button_style_from_tw(&slider->inc_button,
				&src_slider->inc_button, images);
	nk_button_style_from_tw(&slider->dec_button,
				&src_slider->dec_button, images);
}

static void
nk_progress_style_from_tw(struct nk_style_progress *style,
			  const struct tw_style_progress *src_style,
			  const struct nk_image *images)
{
	memset(style, 0, sizeof(*style));
	style->normal =
		nk_style_item_from_tw(&src_style->normal.style, images);
	style->hover =
		nk_style_item_from_tw(&src_style->hover.style, images);
	style->active =
		nk_style_item_from_tw(&src_style->active.style, images);
	style->border_color =
		nk_color_from_tw(&src_style->border_color);

	style->cursor_normal =
		nk_style_item_from_tw(&src_style->cursor_normal.style, images);
	style->cursor_hover =
		nk_style_item_from_tw(&src_style->cursor_hover.style, images);
	style->cursor_active =
		nk_style_item_from_tw(&src_style->cursor_active.style, images);
	style->cursor_border_color =
		nk_color_from_tw(&src_style->cursor_border_color);
	//properties
	style->padding = nk_vec2_from_tw(&src_style->padding);
	style->rounding = src_style->rounding;
	style->border = src_style->border;
	style->cursor_rounding = src_style->cursor_rounding;
	style->cursor_border = src_style->cursor_border;

	style->userdata          = nk_handle_ptr(0);
	style->draw_begin        = 0;
	style->draw_end          = 0;
}

static void
nk_scrollbar_style_from_tw(struct nk_style_scrollbar *style,
			   const struct tw_style_scrollbar *src_style,
			   const struct nk_image *images)
{
	memset(style, 0, sizeof(*style));
	//background
	style->normal =
		nk_style_item_from_tw(&src_style->normal.style, images);
	style->hover =
		nk_style_item_from_tw(&src_style->hover.style, images);
	style->active =
		nk_style_item_from_tw(&src_style->active.style, images);
	style->border_color =
		nk_color_from_tw(&src_style->border_color);
	//cursor
	style->cursor_normal =
		nk_style_item_from_tw(&src_style->cursor_normal.style, images);
	style->cursor_hover =
		nk_style_item_from_tw(&src_style->cursor_hover.style, images);
	style->cursor_active =
		nk_style_item_from_tw(&src_style->cursor_active.style, images);
	style->cursor_border_color =
		nk_color_from_tw(&src_style->cursor_border_color);

	style->dec_symbol      = NK_SYMBOL_CIRCLE_SOLID;
	style->inc_symbol      = NK_SYMBOL_CIRCLE_SOLID;
	//properties
	style->border = src_style->border;
	style->rounding = src_style->rounding;
	style->rounding_cursor = src_style->rounding_cursor;
	style->border_cursor = src_style->border_cursor;
	style->padding = nk_vec2_from_tw(&src_style->padding);
	style->show_buttons = src_style->show_buttons;
	//user data
	style->userdata        = nk_handle_ptr(0);
	style->draw_begin      = 0;
	style->draw_end        = 0;

	nk_button_style_from_tw(&style->inc_button, &src_style->inc_button, images);
	nk_button_style_from_tw(&style->dec_button, &src_style->dec_button, images);
}

static void
nk_edit_style_from_tw(struct nk_style_edit *style,
		      const struct tw_style_edit *src_style,
		      const struct nk_image *images)
{
	memset(style, 0, sizeof(*style));
	//background
	style->normal =
		nk_style_item_from_tw(&src_style->normal.style, images);
	style->hover =
		nk_style_item_from_tw(&src_style->hover.style, images);
	style->active =
		nk_style_item_from_tw(&src_style->active.style, images);
	style->border_color =
		nk_color_from_tw(&src_style->border_color);
	//cursor
	style->cursor_normal =
		nk_color_from_tw(&src_style->cursor_normal);
	style->cursor_hover =
		nk_color_from_tw(&src_style->cursor_hover);
	style->cursor_text_normal =
		nk_color_from_tw(&src_style->cursor_text_normal);
	style->cursor_text_hover =
		nk_color_from_tw(&src_style->cursor_text_hover);
	//text unselected
	style->text_normal =
		nk_color_from_tw(&src_style->text_normal);
	style->text_hover =
		nk_color_from_tw(&src_style->text_hover);
	style->text_active =
		nk_color_from_tw(&src_style->text_active);
	//text selected
	style->selected_normal =
		nk_color_from_tw(&src_style->selected_normal);
	style->selected_hover =
		nk_color_from_tw(&src_style->selected_hover);
	style->selected_text_normal =
		nk_color_from_tw(&src_style->selected_text_normal);
	style->selected_text_hover =
		nk_color_from_tw(&src_style->selected_text_hover);

	style->border = src_style->border;
	style->rounding = src_style->rounding;
	style->cursor_size = src_style->cursor_size;
	style->scrollbar_size = nk_vec2_from_tw(&src_style->scrollbar_size);
	style->padding = nk_vec2_from_tw(&src_style->padding);
	style->row_padding = src_style->row_padding;

	nk_scrollbar_style_from_tw(&style->scrollbar, &src_style->scrollbar, images);
}

static void
nk_property_style_from_tw(struct nk_style_property *style,
			  const struct tw_style_property *src_style,
			  const struct nk_image *images)
{
	memset(style, 0, sizeof(*style));
	//background
	style->normal =
		nk_style_item_from_tw(&src_style->normal.style, images);
	style->hover =
		nk_style_item_from_tw(&src_style->hover.style, images);
	style->active =
		nk_style_item_from_tw(&src_style->active.style, images);
	style->border_color =
		nk_color_from_tw(&src_style->border_color);
	//text
	style->label_normal =
		nk_color_from_tw(&src_style->label_normal);
	style->label_hover =
		nk_color_from_tw(&src_style->label_hover);
	style->label_active =
		nk_color_from_tw(&src_style->label_active);

	style->border = src_style->border;
	style->rounding = src_style->rounding;
	style->padding = nk_vec2_from_tw(&src_style->padding);

	style->sym_left      = NK_SYMBOL_TRIANGLE_LEFT;
	style->sym_right     = NK_SYMBOL_TRIANGLE_RIGHT;

	style->userdata      = nk_handle_ptr(0);
	style->draw_begin    = 0;
	style->draw_end      = 0;

	nk_button_style_from_tw(&style->inc_button, &src_style->inc_button, images);
	nk_button_style_from_tw(&style->dec_button, &src_style->dec_button, images);
	nk_edit_style_from_tw(&style->edit, &src_style->edit, images);
}

static void
nk_chart_style_from_tw(struct nk_style_chart *style,
		       const struct tw_style_chart *src_style,
		       const struct nk_image *images)
{
	memset(style, 0, sizeof(*style));
	style->background =
		nk_style_item_from_tw(&src_style->background.style, images);
	style->border_color =
		nk_color_from_tw(&src_style->border_color);
	style->selected_color =
		nk_color_from_tw(&src_style->selected_color);
	style->color =
		nk_color_from_tw(&src_style->color);

	style->border = src_style->border;
	style->rounding = src_style->rounding;
	style->padding = nk_vec2_from_tw(&src_style->padding);
}

static void
nk_combo_style_from_tw(struct nk_style_combo *style,
		       const struct tw_style_combo *src_style,
		       const struct nk_image *images)
{
	//background
	style->normal =
		nk_style_item_from_tw(&src_style->normal.style, images);
	style->hover =
		nk_style_item_from_tw(&src_style->hover.style, images);
	style->active =
		nk_style_item_from_tw(&src_style->active.style, images);
	style->border_color =
		nk_color_from_tw(&src_style->border_color);
	//text
	style->label_normal =
		nk_color_from_tw(&src_style->label_normal);
	style->label_hover =
		nk_color_from_tw(&src_style->label_hover);
	style->label_active =
		nk_color_from_tw(&src_style->label_active);
	//symbol
	style->symbol_normal =
		nk_color_from_tw(&src_style->symbol_normal);
	style->symbol_hover =
		nk_color_from_tw(&src_style->symbol_hover);
	style->symbol_active =
		nk_color_from_tw(&src_style->symbol_active);

	style->sym_normal       = NK_SYMBOL_TRIANGLE_DOWN;
	style->sym_hover        = NK_SYMBOL_TRIANGLE_DOWN;
	style->sym_active       = NK_SYMBOL_TRIANGLE_DOWN;


	style->border = src_style->border;
	style->rounding = src_style->rounding;

	style->spacing  = nk_vec2_from_tw(&src_style->spacing);
	style->content_padding = nk_vec2_from_tw(&src_style->content_padding);
	style->button_padding = nk_vec2_from_tw(&src_style->button_padding);

	nk_button_style_from_tw(&style->button, &src_style->button, images);
}

static void
nk_tab_style_from_tw(struct nk_style_tab *style,
		     const struct tw_style_tab *src_style,
		     const struct nk_image *images)
{
	memset(style, 0, sizeof(*style));
	style->background =
		nk_style_item_from_tw(&src_style->background.style, images);
	style->border_color =
		nk_color_from_tw(&src_style->border_color);
	style->text = nk_color_from_tw(&src_style->text);

	style->border = src_style->border;
	style->rounding = src_style->rounding;
	style->indent = src_style->indent;
	style->padding = nk_vec2_from_tw(&src_style->padding);
	style->spacing = nk_vec2_from_tw(&src_style->spacing);

	style->sym_minimize       = NK_SYMBOL_TRIANGLE_RIGHT;
	style->sym_maximize       = NK_SYMBOL_TRIANGLE_DOWN;
	//buttons
	nk_button_style_from_tw(&style->tab_maximize_button, &src_style->tab_button, images);
	nk_button_style_from_tw(&style->tab_minimize_button, &src_style->tab_button, images);
	nk_button_style_from_tw(&style->node_maximize_button, &src_style->node_button, images);
	nk_button_style_from_tw(&style->node_minimize_button, &src_style->node_button, images);
}

static void
nk_window_header_style_from_tw(struct nk_style_window_header *style,
			       const struct tw_style_window_header *src_style,
			       const struct nk_image *images)
{
	style->align = NK_HEADER_RIGHT;
	style->close_symbol = NK_SYMBOL_X;
	style->minimize_symbol = NK_SYMBOL_MINUS;
	style->maximize_symbol = NK_SYMBOL_PLUS;

	//background
	style->normal =
		nk_style_item_from_tw(&src_style->normal.style, images);
	style->hover =
		nk_style_item_from_tw(&src_style->hover.style, images);
	style->active =
		nk_style_item_from_tw(&src_style->active.style, images);
	//text
	style->label_normal =
		nk_color_from_tw(&src_style->label_normal);
	style->label_hover =
		nk_color_from_tw(&src_style->label_hover);
	style->label_active =
		nk_color_from_tw(&src_style->label_active);
	//properties
	style->padding = nk_vec2_from_tw(&src_style->padding);
	style->label_padding = nk_vec2_from_tw(&src_style->label_padding);
	style->spacing = nk_vec2_from_tw(&src_style->spacing);

	//sub buttons
	nk_button_style_from_tw(&style->close_button, &src_style->button, images);
	nk_button_style_from_tw(&style->minimize_button, &src_style->button, images);
}

static void
nk_window_style_from_tw(struct nk_style_window *style,
			const struct tw_style_window *src_style,
			const struct nk_image *images)
{
	style->background =
		nk_color_from_tw(&src_style->background.style.data.color);
	style->fixed_background =
		nk_style_item_from_tw(&src_style->background.style, images);

	style->border_color =
		nk_color_from_tw(&src_style->border_color);
	style->popup_border_color =
		nk_color_from_tw(&src_style->popup_border_color);
	style->combo_border_color =
		nk_color_from_tw(&src_style->combo_border_color);
	style->contextual_border_color =
		nk_color_from_tw(&src_style->contextual_border_color);
	style->menu_border_color =
		nk_color_from_tw(&src_style->menu_border_color);
	style->group_border_color =
		nk_color_from_tw(&src_style->group_border_color);
	style->tooltip_border_color =
		nk_color_from_tw(&src_style->tooltip_border_color);
	style->scaler = nk_style_item_from_tw(&src_style->scaler.style, images);
	//paddings
	style->padding = nk_vec2_from_tw(&src_style->padding);
	style->group_padding = nk_vec2_from_tw(&src_style->group_padding);
	style->popup_padding = nk_vec2_from_tw(&src_style->popup_padding);
	style->combo_padding = nk_vec2_from_tw(&src_style->combo_padding);
	style->contextual_padding = nk_vec2_from_tw(&src_style->contextual_padding);
	style->menu_padding = nk_vec2_from_tw(&src_style->menu_padding);
	style->tooltip_padding = nk_vec2_from_tw(&src_style->tooltip_padding);
	//border
	style->border = style->border;
	style->combo_border = style->combo_border;
	style->contextual_border = style->contextual_border;
	style->menu_border = style->menu_border;
	style->group_border = style->group_border;
	style->tooltip_border = style->tooltip_border;
	style->popup_border = style->popup_border;
	style->min_row_height_padding = style->min_row_height_padding;
	//properties
	style->rounding = style->rounding;
	style->spacing = nk_vec2_from_tw(&src_style->spacing);
	style->scrollbar_size = nk_vec2_from_tw(&src_style->scrollbar_size);
	style->min_size = nk_vec2_from_tw(&src_style->min_size);
}

static struct nk_image *
nk_images_from_cache(struct image_cache *cache, struct nk_wl_backend *b)
{
	struct tw_bbox *box;
	struct nk_image *images;
	struct nk_image gpu_image;
	size_t n_images, i;
	nk_handle handle;

	gpu_image = nk_image_from_buffer(cache->atlas, b, cache->dimension.h,
	                                 cache->dimension.w,
	                                 cache->dimension.w * 4);
	handle = gpu_image.handle;
	n_images = cache->image_boxes.size / sizeof(struct tw_bbox);
	nk_wl_add_image(gpu_image, b);

	images = malloc(sizeof(struct nk_image) * n_images);
	if (!images)
		return NULL;

	i = 0;
	wl_array_for_each(box, &cache->image_boxes)
		images[i++] = nk_subimage_handle(handle, cache->dimension.w,
		                                 cache->dimension.h,
		                                 nk_rect_from_bbox(box));

	return images;
}

static void
nk_wl_apply_theme(struct nk_wl_backend *b,
                  const struct tw_theme *theme)
{
	struct nk_style_text *text;
	struct nk_style_button *button;
	struct nk_style_toggle *toggle;
	struct nk_style_selectable *select;
	struct nk_style_slider *slider;
	struct nk_style_progress *prog;
	struct nk_style_scrollbar *scroll;
	struct nk_style_edit *edit;
	struct nk_style_property *property;
	struct nk_style_combo *combo;
	struct nk_style_chart *chart;
	struct nk_style_tab *tab;
	struct nk_style_window *win;
	struct image_cache cache;
	struct nk_image *images;
	struct nk_style *style;

	style = &b->ctx.style;
	cache = image_cache_from_arrays(&theme->handle_pool,
	                                &theme->string_pool, NULL);
	if (!cache.atlas || !cache.dimension.w || !cache.dimension.h)
		images = NULL;
	else
		images = nk_images_from_cache(&cache, b);

	/* default text */
	text = &style->text;
	text->color = nk_color_from_tw(&theme->text.color);
	text->padding = nk_vec2_from_tw(&theme->text.padding);

	/* default button */
	button = &style->button;
	nk_button_style_from_tw(button, &theme->button, images);
	/* contextual button */
	button = &style->contextual_button;
	nk_button_style_from_tw(button, &theme->contextual_button, images);
	/* menu button */
	button = &style->menu_button;
	nk_button_style_from_tw(button, &theme->menu_button, images);

	/* checkbox toggle */
	toggle = &style->checkbox;
	nk_toggle_style_from_tw(toggle, &theme->checkbox, images);
	/* option toggle */
	toggle = &style->option;
	nk_toggle_style_from_tw(toggle, &theme->option, images);

	/* selectable */
	select = &style->selectable;
	nk_selectable_style_from_tw(select, &theme->selectable, images);

	/* slider */
	slider = &style->slider;
	nk_slider_style_from_tw(slider, &theme->slider, images);

	/* progressbar */
	prog = &style->progress;
	nk_progress_style_from_tw(prog, &theme->progress, images);

	/* scrollbars */
	scroll = &style->scrollh;
	nk_scrollbar_style_from_tw(scroll, &theme->scrollh, images);
	scroll = &style->scrollv;
	nk_scrollbar_style_from_tw(scroll, &theme->scrollv, images);

	/* edit */
	edit = &style->edit;
	nk_edit_style_from_tw(edit, &theme->edit, images);

	/* property */
	property = &style->property;
	nk_property_style_from_tw(property, &theme->property, images);

	/* chart */
	chart = &style->chart;
	nk_chart_style_from_tw(chart, &theme->chart, images);

	/* combo */
	combo = &style->combo;
	nk_combo_style_from_tw(combo, &theme->combo, images);

	/* tab */
	tab = &style->tab;
	nk_tab_style_from_tw(tab, &theme->tab, images);

	/* window */
	win = &style->window;
	nk_window_header_style_from_tw(&win->header, &theme->window.header, images);
	nk_window_style_from_tw(win, &theme->window, images);
}

static void
nk_wl_apply_color(struct nk_wl_backend *bkend,
		  const struct tw_theme_color *theme)
{
	if (theme->row_size == 0)
		return;
	bkend->row_size = theme->row_size;

	//TODO this is a shitty hack, somehow the first draw call did not work,
	//we have to hack it in the background color
	bkend->main_color = nk_color_from_tw(&theme->window_color);
	struct nk_color table[NK_COLOR_COUNT];

	table[NK_COLOR_TEXT] =
		nk_color_from_tw(&theme->text_color);
	table[NK_COLOR_WINDOW] =
		nk_color_from_tw(&theme->window_color);
	//header
	table[NK_COLOR_HEADER] =
		nk_color_from_tw(&theme->window_color);
	table[NK_COLOR_BORDER] =
		nk_color_from_tw(&theme->border_color);
	//button

	table[NK_COLOR_BUTTON] =
		nk_color_from_tw(&theme->button.normal);
	table[NK_COLOR_BUTTON_HOVER] =
		nk_color_from_tw(&theme->button.hover);
	table[NK_COLOR_BUTTON_ACTIVE] =
		nk_color_from_tw(&theme->button.active);
	//toggle
	table[NK_COLOR_TOGGLE] =
		nk_color_from_tw(&theme->toggle.normal);
	table[NK_COLOR_TOGGLE_HOVER] =
		nk_color_from_tw(&theme->toggle.hover);
	table[NK_COLOR_TOGGLE_CURSOR] =
		nk_color_from_tw(&theme->toggle.active);
	//select
	table[NK_COLOR_SELECT] =
		nk_color_from_tw(&theme->select.normal);
	table[NK_COLOR_SELECT_ACTIVE] =
		nk_color_from_tw(&theme->select.active);
	//slider
	table[NK_COLOR_SLIDER] =
		nk_color_from_tw(&theme->slider_bg_color);
	table[NK_COLOR_SLIDER_CURSOR] =
		nk_color_from_tw(&theme->slider.normal);
	table[NK_COLOR_SLIDER_CURSOR_HOVER] =
		nk_color_from_tw(&theme->slider.hover);
	table[NK_COLOR_SLIDER_CURSOR_ACTIVE] =
		nk_color_from_tw(&theme->slider.active);
	//property
	table[NK_COLOR_PROPERTY] = table[NK_COLOR_SLIDER];
	//edit
	table[NK_COLOR_EDIT] =
		nk_color_from_tw(&theme->edit_color);
	table[NK_COLOR_EDIT_CURSOR] =
		nk_color_from_tw(&theme->text_color);
	//combo
	table[NK_COLOR_COMBO] =
		nk_color_from_tw(&theme->combo_color);
	//chart
	table[NK_COLOR_CHART] =
		nk_color_from_tw(&theme->chart.normal);
	table[NK_COLOR_CHART_COLOR] =
		nk_color_from_tw(&theme->chart.active);
	table[NK_COLOR_CHART_COLOR_HIGHLIGHT] =
		nk_color_from_tw(&theme->chart.hover);
	//scrollbar
	table[NK_COLOR_SCROLLBAR] = table[NK_COLOR_WINDOW];
	table[NK_COLOR_SCROLLBAR_CURSOR] = table[NK_COLOR_WINDOW];
	table[NK_COLOR_SCROLLBAR_CURSOR_ACTIVE] = table[NK_COLOR_WINDOW];
	table[NK_COLOR_SCROLLBAR_CURSOR_HOVER] = table[NK_COLOR_WINDOW];
	table[NK_COLOR_TAB_HEADER] = table[NK_COLOR_WINDOW];
	nk_style_from_table(&bkend->ctx, table);
}

static inline nk_hash
nk_wl_hash_colors(const struct tw_theme_color* theme)
{
	return nk_murmur_hash(theme, sizeof(struct tw_theme_color), NK_FLAG(7));
}

static inline nk_hash
nk_wl_hash_theme(const struct tw_theme *theme)
{
	return nk_murmur_hash(theme, sizeof(struct tw_theme), NK_FLAG(7));
}



#ifdef __cplusplus
}
#endif

#endif /* EOF */
