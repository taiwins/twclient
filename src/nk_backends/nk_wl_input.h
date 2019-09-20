#ifndef NK_WL_INPUT_H
#define NK_WL_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nk_wl_internal.h"


/* Clear the retained input state
 *
 * unfortunatly we have to reset the input after the input so we do get retained
 * intput state
 */
static inline void
nk_input_reset(struct nk_context *ctx)
{
	nk_input_begin(ctx);
	nk_input_end(ctx);
}

//this is so verbose

static void
nk_keycb(struct app_surface *surf, const struct app_event *e)
	 /* xkb_keysym_t keysym, uint32_t modifier, int state) */
{
	//nk_input_key and nk_input_unicode are different, you kinda need to
	//registered all the keys
	struct nk_wl_backend *bkend = (struct nk_wl_backend *)surf->user_data;
	uint32_t keycode = xkb_keysym_to_utf32(e->key.sym);
	uint32_t keysym = e->key.sym;
	uint32_t modifier = e->key.mod;
	bool state = e->key.state;
	nk_input_begin(&bkend->ctx);

	//now we deal with the ctrl-keys
	if (modifier & TW_CTRL) {
		//the emacs keybindings
		nk_input_key(&bkend->ctx, NK_KEY_TEXT_LINE_START, (keysym == XKB_KEY_a) && state);
		nk_input_key(&bkend->ctx, NK_KEY_TEXT_LINE_END, (keysym == XKB_KEY_e) && state);
		nk_input_key(&bkend->ctx, NK_KEY_LEFT, (keysym == XKB_KEY_b) && state);
		nk_input_key(&bkend->ctx, NK_KEY_RIGHT, (keysym == XKB_KEY_f) && state);
		nk_input_key(&bkend->ctx, NK_KEY_TEXT_UNDO, (keysym == XKB_KEY_slash) && state);
		//we should also support the clipboard later
	}
	else if (modifier & TW_ALT) {
		nk_input_key(&bkend->ctx, NK_KEY_TEXT_WORD_LEFT, (keysym == XKB_KEY_b) && state);
		nk_input_key(&bkend->ctx, NK_KEY_TEXT_WORD_RIGHT, (keysym == XKB_KEY_f) && state);
	}
	//no tabs, we don't essentially need a buffer here, give your own buffer. That is it.
	else if (keycode >= 0x20 && keycode < 0x7E && state)
		nk_input_unicode(&bkend->ctx, keycode);
	else {
		nk_input_key(&bkend->ctx, NK_KEY_DEL, (keysym == XKB_KEY_Delete) && state);
		nk_input_key(&bkend->ctx, NK_KEY_ENTER, (keysym == XKB_KEY_Return) && state);
		nk_input_key(&bkend->ctx, NK_KEY_TAB, keysym == XKB_KEY_Tab && state);
		nk_input_key(&bkend->ctx, NK_KEY_BACKSPACE, (keysym == XKB_KEY_BackSpace) && state);
		nk_input_key(&bkend->ctx, NK_KEY_UP, (keysym == XKB_KEY_UP) && state);
		nk_input_key(&bkend->ctx, NK_KEY_DOWN, (keysym == XKB_KEY_DOWN) && state);
		nk_input_key(&bkend->ctx, NK_KEY_SHIFT, (keysym == XKB_KEY_Shift_L ||
							 keysym == XKB_KEY_Shift_R) && state);
		nk_input_key(&bkend->ctx, NK_KEY_TEXT_LINE_START, (keysym == XKB_KEY_Home) && state);
		nk_input_key(&bkend->ctx, NK_KEY_TEXT_LINE_END, (keysym == XKB_KEY_End) && state);
		nk_input_key(&bkend->ctx, NK_KEY_LEFT, (keysym == XKB_KEY_Left) && state);
		nk_input_key(&bkend->ctx, NK_KEY_RIGHT, (keysym == XKB_KEY_Right) && state);
	}
	if (state)
		bkend->ckey = keysym;
	else
		bkend->ckey = XKB_KEY_NoSymbol;
	nk_input_end(&bkend->ctx);
}

static void
nk_pointron(struct app_surface *surf, const struct app_event *e)
{
	struct nk_wl_backend *bkend = (struct nk_wl_backend *)surf->user_data;
	nk_input_begin(&bkend->ctx);
	nk_input_motion(&bkend->ctx, e->ptr.x, e->ptr.y);
	nk_input_end(&bkend->ctx);
	bkend->sx = e->ptr.x;
	bkend->sy = e->ptr.y;
}

static void
nk_pointrbtn(struct app_surface *surf, const struct app_event *e)
{
	struct nk_wl_backend *bkend = (struct nk_wl_backend *)surf->user_data;
	enum nk_buttons b;
	switch (e->ptr.btn) {
	case BTN_LEFT:
		b = NK_BUTTON_LEFT;
		break;
	case BTN_RIGHT:
		b = NK_BUTTON_RIGHT;
		break;
	case BTN_MIDDLE:
		b = NK_BUTTON_MIDDLE;
		break;
		//case TWBTN_DCLICK:
		//b = NK_BUTTON_DOUBLE;
		//break;
	default:
		b = NK_BUTTON_MAX;
		break;
	}

	nk_input_begin(&bkend->ctx);
	nk_input_button(&bkend->ctx, b, e->ptr.x, e->ptr.y, e->ptr.state);
	nk_input_end(&bkend->ctx);

	/* bkend->cbtn = (state) ? b : -2; */
	/* bkend->sx = sx; */
	/* bkend->sy = sy; */
}

static void
nk_pointraxis(struct app_surface *surf, const struct app_event *e)
{
	struct nk_wl_backend *bkend = (struct nk_wl_backend *)surf->user_data;
	nk_input_begin(&bkend->ctx);
	nk_input_scroll(&bkend->ctx, nk_vec2(e->axis.dx, e->axis.dy));
	nk_input_end(&bkend->ctx);
}


#ifdef __cplusplus
}
#endif


#endif /* EOF */
