/* this file is included by backends */
#include <assert.h>
#include <cairo/cairo.h>
#include "../../theme/theme.h"
#include <nuklear/nuklear.h>
/* #include "nk_wl_internal.h" */


#define STB_RECT_PACK_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_rect_pack.h>
#include <stb/stb_image.h>

#ifndef MAX
#define MAX(a, b) \
	({ __typeof__ (a) _a = (a); \
		__typeof__ (b) _b = (b); \
		_a > _b ? _a : _b; })
#endif


////////////////////////// inlines ////////////////////////////////
static inline struct nk_color
nk_color_from_tw(const taiwins_rgba_t *tc)
{
	struct nk_color nc;
	nc.r = tc->r; nc.g = tc->g;
	nc.b = tc->b; nc.a = tc->a;
	return nc;
}

static inline struct nk_vec2
nk_vec2_from_tw(const taiwins_vec2_t *vec)
{
	struct nk_vec2 v;
	v.x = vec->x;
	v.y = vec->y;
	return v;
}

static inline struct nk_style_item
nk_style_item_from_tw(const struct taiwins_style_item *item,
		      const struct nk_image *image_pool)
{
    struct nk_style_item i;
    i.type = item->type == TAIWINS_STYLE_COLOR ? NK_STYLE_ITEM_COLOR : NK_STYLE_ITEM_IMAGE;
    if (item->type == TAIWINS_STYLE_COLOR)
	    i.data.color = nk_color_from_tw(&item->data.color);
    else
	    i.data.image = image_pool[item->data.image.handle];
    return i;
}

static inline nk_flags
nk_alignment_from_tw(const enum taiwins_text_alignment alignment)
{
	return (alignment == TAIWINS_TEXT_LEFT) ? NK_TEXT_LEFT :
		(alignment == TAIWINS_TEXT_CENTER) ? NK_TEXT_CENTERED :
		NK_TEXT_RIGHT;
}


static void
copy_as_subimage(unsigned char *dst, const size_t dst_width,
		 const unsigned char *src, const stbrp_rect *rect)
{
	//stb loads image in rgba byte order.
	//cairo has argb in the uint32_t, which is bgra in byte order
	union argb {
		//byte order readed
		struct {
			uint8_t r, g, b, a;
		} data;
		uint32_t code;
	};
	//cairo alpha is pre-multiplied, we need do the same here
	union argb pixel;
	for (int j = 0; j < rect->h; j++)
		for (int i = 0; i < rect->w; i++) {
			pixel.code = *((uint32_t*)src + j * rect->w + i);
			double alpha = pixel.data.a / 256.0;
			*((uint32_t *)dst + (rect->y+j) * dst_width + (rect->x+i)) =
				((uint32_t)pixel.data.a << 24) +
				((uint32_t)(pixel.data.r * alpha) << 16) +
				((uint32_t)(pixel.data.g * alpha) << 8) +
				((uint32_t)(pixel.data.b * alpha));
		}
}

/* this really needs to be in another thread */
struct nk_image *
nk_wl_build_theme_images(struct taiwins_theme *theme)
{
	size_t nimages = theme->handle_pool.size / sizeof(uint64_t);
	size_t context_height, context_width = 1000;
	int row_x = 0, row_y = 0;
	stbrp_rect *rects = malloc(sizeof(stbrp_rect) * nimages);
	stbrp_node *nodes = malloc(sizeof(stbrp_node) * context_width + 10);
	stbrp_context context;
	struct nk_image *subimages;

	//first pass: collects rects and decide texture size
	for (int i = 0; i < nimages; i++) {
		int pos;
		int w, h, nchannels;
		pos = *((uint64_t *)theme->handle_pool.data + i);
		const char *path =
			(const char *)theme->string_pool.data + pos;
		stbi_info(path, &w, &h, &nchannels);
		rects[i].w = w; rects[i].h = h;
		//advance in tile. If current row still works, we go forward.
		//otherwise, start a new row
		row_x = (row_x + w > context_width) ?
			w : row_x + w;
		row_y = (row_x + w > context_width) ?
			row_y + h : MAX(row_y, h);
	}

	context_height = row_y;
	stbrp_init_target(&context, context_width, context_height, nodes,
			  context_width+10);
	stbrp_setup_allow_out_of_mem(&context, 0);
	stbrp_pack_rects(&context, rects, nimages);

	//create image data
	unsigned char *dst = calloc(1, context_width * context_height *
				    sizeof(uint32_t));
	subimages = malloc(sizeof(struct nk_image) * nimages);
	//second pass: copy images
	for (int i = 0; i < nimages; i++) {
	  intptr_t pos;
	  int x, y, comp; // channels
	  pos = *((uint64_t *)theme->handle_pool.data + i);
	  const char *path = (const char *)theme->string_pool.data + pos;
	  unsigned char *pixels =
	      stbi_load(path, &x, &y, &comp, STBI_rgb_alpha);
	  copy_as_subimage(dst, context_width, pixels, &rects[i]);
	  stbi_image_free(pixels);
	  subimages[i] = nk_subimage_ptr(
	      dst, context_width, context_height,
	      nk_rect(rects[i].x, rects[i].h, rects[i].w, rects[i].h));
	}
	free(nodes);
	free(rects);
	return subimages;
}


static void
nk_button_style_from_tw(struct nk_style_button *button,
			const struct taiwins_style_button *src_button,
			const struct nk_image *images)
{
	nk_zero_struct(*button);
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
			const struct taiwins_style_toggle *src_toggle,
			const struct nk_image *images)
{
	nk_zero_struct(*toggle);
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
			    const struct taiwins_style_selectable *src_select,
			    const struct nk_image *images)
{
	nk_zero_struct(*select);
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
			const struct taiwins_style_slider *src_slider,
			const struct nk_image *images)
{
	nk_zero_struct(*slider);
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
			  const struct taiwins_style_progress *src_style,
			  const struct nk_image *images)
{
	nk_zero_struct(*style);
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
			   const struct taiwins_style_scrollbar *src_style,
			   const struct nk_image *images)
{
	nk_zero_struct(*style);
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
		      const struct taiwins_style_edit *src_style,
		      const struct nk_image *images)
{
	nk_zero_struct(*style);
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
			  const struct taiwins_style_property *src_style,
			  const struct nk_image *images)
{
	nk_zero_struct(*style);
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
		       const struct taiwins_style_chart *src_style,
		       const struct nk_image *images)
{
	nk_zero_struct(*style);
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
		       const struct taiwins_style_combo *src_style,
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
		     const struct taiwins_style_tab *src_style,
		     const struct nk_image *images)
{
	nk_zero_struct(*style);
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
	nk_button_style_from_tw(&style->tab_maximize_button, &src_style->tab_maximize_button, images);
	nk_button_style_from_tw(&style->tab_minimize_button, &src_style->tab_minimize_button, images);
	nk_button_style_from_tw(&style->node_maximize_button, &src_style->node_maximize_button, images);
	nk_button_style_from_tw(&style->node_minimize_button, &src_style->node_minimize_button, images);
}

static void
nk_window_header_style_from_tw(struct nk_style_window_header *style,
			       const struct taiwins_style_window_header *src_style,
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
	nk_button_style_from_tw(&style->close_button, &src_style->close_button, images);
	nk_button_style_from_tw(&style->minimize_button, &src_style->minimize_button, images);
}

static void
nk_window_style_from_tw(struct nk_style_window *style,
			const struct taiwins_style_window *src_style,
			const struct nk_image *images)
{
	style->background =
		nk_color_from_tw(&src_style->background);
	style->fixed_background =
		nk_style_item_from_tw(&src_style->fixed_background.style, images);

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


void
nk_style_init_from_tw(struct nk_context *ctx,
		      const struct taiwins_theme *theme,
		      const struct nk_image *images)
{
	struct nk_style *style;
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

	assert(ctx);
	if (!ctx) return;
	style = &ctx->style;

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
