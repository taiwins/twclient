/*
 * appsurf.c - taiwins client app surface functions
 *
 * Copyright (c) 2019-2020 Xichen Zhou
 *
 * This library is free software; you can redistribute it and/or modify it under
 * the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation; either version 2.1 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 */
#include <stdlib.h>
#include <assert.h>
#include <twclient/client.h>
#include <twclient/ui.h>
#include <twclient/egl.h>
#include <twclient/shmpool.h>

WL_EXPORT void
tw_appsurf_init_egl(struct tw_appsurf *surf, struct tw_egl_env *env)
{
	surf->egldisplay = env->egl_display;
	surf->eglwin = wl_egl_window_create(surf->wl_surface,
	                                    surf->allocation.w *
	                                    surf->allocation.s,
	                                    surf->allocation.h *
	                                    surf->allocation.s);
	surf->eglsurface =
		eglCreateWindowSurface(env->egl_display,
		                       env->config,
		                       (EGLNativeWindowType)surf->eglwin,
		                       NULL);
	//this should not happen in action
	assert(surf->eglsurface);
	assert(surf->eglwin);
}

WL_EXPORT void
tw_appsurf_clean_egl(struct tw_appsurf *surf, struct tw_egl_env *env)
{
	eglMakeCurrent(env->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
		       env->egl_context);
	eglDestroySurface(env->egl_display, surf->eglsurface);
	wl_egl_window_destroy(surf->eglwin);
	surf->eglwin = NULL;
	surf->egldisplay = EGL_NO_DISPLAY;
	surf->eglsurface = EGL_NO_SURFACE;
}

WL_EXPORT void
tw_appsurf_init(struct tw_appsurf *surf, struct wl_surface *wl_surface,
		 struct tw_globals *globals,
		 enum tw_appsurf_type type, const uint32_t flags)
{
	*surf = (struct tw_appsurf){0};
	surf->wl_surface = wl_surface;
	surf->type = type;
	surf->flags = flags;
	wl_surface_set_user_data(wl_surface, surf);
	surf->tw_globals = globals;
	wl_list_init(&surf->filter_head);
}

WL_EXPORT void
tw_appsurf_init_default(struct tw_appsurf *app, struct wl_surface *surf,
                        struct tw_globals *g)
{
	tw_appsurf_init(app, surf, g, TW_APPSURF_APP, 0);
}

WL_EXPORT void
tw_appsurf_release(struct tw_appsurf *surf)
{
	if (!surf)
		return;
	if (surf->destroy)
		surf->destroy(surf);
	//throw all the callbacks
	if (surf->wl_surface)
		wl_surface_destroy(surf->wl_surface);
	surf->wl_surface = NULL;
	wl_list_init(&surf->filter_head);
}

void
_tw_appsurf_run_frame(struct tw_appsurf *surf, const struct tw_app_event *e)
{
	struct tw_app_event_filter *f;
	wl_list_for_each(f, &surf->filter_head, link) {
		if (f->type == e->type &&
		    f->intercept(surf, e))
			return;
	}
	surf->do_frame(surf, e);
}

WL_EXPORT void
tw_appsurf_frame(struct tw_appsurf *surf, bool anime)
{
	struct tw_globals *g = surf->tw_globals;
	struct tw_app_event e = {
		.type = TW_FRAME_START,
		.time = (g) ? g->inputs.millisec : 0,
	};
	//this is the best we
	surf->need_animation = anime;
	if (anime)
		tw_appsurf_request_frame(surf);
	_tw_appsurf_run_frame(surf, &e);
}


WL_EXPORT void
tw_appsurf_resize(struct tw_appsurf *surf,
		   uint32_t nw, uint32_t nh,
		   uint32_t ns)
{
	if (surf->flags & TW_APPSURF_NORESIZABLE)
		return;
	struct tw_app_event e;
	e.time = surf->tw_globals->inputs.millisec;
	e.type = TW_RESIZE;
	e.resize.nw = nw;
	e.resize.nh = nh;
	e.resize.ns = ns;
	e.resize.edge = WL_SHELL_SURFACE_RESIZE_BOTTOM_RIGHT;
	e.resize.serial = surf->tw_globals->inputs.serial;
	if (surf->allocation.s != ns)
		wl_surface_set_buffer_scale(surf->wl_surface, ns);
	_tw_appsurf_run_frame(surf, &e);
}

static void
tw_appsurf_frame_done(void *user_data, struct wl_callback *cb, uint32_t data)
{
	struct tw_app_event e = {
		.type = TW_TIMER,
		.time = data,
	};

	fprintf(stderr, "now I am in animation\n");
	if (cb)
		wl_callback_destroy(cb);
	struct tw_appsurf *surf = (struct tw_appsurf *)user_data;
	if (surf->need_animation)
		tw_appsurf_request_frame(surf);
	_tw_appsurf_run_frame(surf, &e);

	surf->last_serial = data;
}

static struct wl_callback_listener tw_appsurf_wl_frame_impl = {
	.done = tw_appsurf_frame_done,
};

WL_EXPORT void
tw_appsurf_request_frame(struct tw_appsurf *surf)
{
	surf->need_animation = true;
	struct wl_callback *callback = wl_surface_frame(surf->wl_surface);
	wl_callback_add_listener(callback, &tw_appsurf_wl_frame_impl, surf);
}



/*******************************************************************************
 * shm_buffer_impl_surface
 ******************************************************************************/
/**
 * @brief smartly release the buffer and pool.
 *
 * We are waiting for compositor to return all the buffers. There could be cases
 * where we switched to new pool before compositor returns the buffers from
 * previous pool. In that case the previous pool becomes a wild pointer. The
 * pool will eventually be freed here if `compositor` finally returns the buffer
 * back to us.
 */
static void
shm_wl_buffer_release(void *data, struct wl_buffer *wl_buffer)
{
	struct tw_appsurf *surf = (struct tw_appsurf *)data;
	struct tw_shm_pool *pool = NULL;
	bool inuse = false;
	for (int i = 0; i < 2; i++)
		if (surf->wl_buffer[i] == wl_buffer) {
			inuse = true;
			surf->dirty[i] = false;
			surf->committed[i] = false;
			break;
		}
	if (!inuse) {
		pool = tw_shm_pool_buffer_free(wl_buffer);
		if (tw_shm_pool_release_if_unused(pool))
			free(pool);
	}
}

/* setup the pool and buffer, destroy the previous pool if there is
 */
WL_EXPORT void
shm_buffer_reallocate(struct tw_appsurf *surf, const struct tw_bbox *geo)
{
	if (surf->pool && tw_shm_pool_release_if_unused(surf->pool))
		free(surf->pool);
	surf->pool = calloc(1, sizeof(struct tw_shm_pool));
	tw_shm_pool_init(surf->pool, surf->tw_globals->shm, tw_bbox_area(geo) * 2,
		      surf->tw_globals->buffer_format);
	for (int i = 0; i < 2; i++) {
		surf->wl_buffer[i] = tw_shm_pool_alloc_buffer(
			surf->pool, geo->w * geo->s, geo->h * geo->s);
		surf->dirty[i] = NULL;
		surf->committed[i] = NULL;
		tw_shm_pool_set_buffer_release_notify(surf->wl_buffer[i],
						   shm_wl_buffer_release, surf);
	}
}

static int
shm_pool_resize_idle(struct tw_event *e, int fd)
{
	struct tw_appsurf *surf = e->data;
	struct tw_bbox *geo = &surf->pending_allocation;

	if (surf->pending_allocation.w == surf->allocation.w &&
	    surf->pending_allocation.h == surf->allocation.h &&
	    surf->pending_allocation.s == surf->allocation.s)
		return TW_EVENT_DEL;

	shm_buffer_reallocate(surf, geo);
	surf->allocation = surf->pending_allocation;

	tw_appsurf_frame(surf, surf->need_animation);
	return TW_EVENT_DEL;
}

WL_EXPORT void
shm_buffer_resize(struct tw_appsurf *surf, const struct tw_app_event *e)
{
	surf->pending_allocation.w = e->resize.nw;
	surf->pending_allocation.h = e->resize.nh;
	surf->pending_allocation.s = e->resize.ns;
	struct tw_event re = {
		.data = surf,
		.cb = shm_pool_resize_idle,
	};
	tw_event_queue_add_idle(&surf->tw_globals->event_queue, &re);
}

static void
shm_buffer_surface_swap(struct tw_appsurf *surf, const struct tw_app_event *e)
{
	//for shm_buffer, if resize is requested, you would want call
	//tw_appsurf_frame again right after.
	bool ret = true;
	switch (e->type) {
	case TW_FRAME_START:
		ret = false;
		break;
	case TW_RESIZE:
		ret = true;
		shm_buffer_resize(surf, e);
		break;
	default:
		ret = true;
		break;
	}
	if (ret)
		return;

	struct wl_buffer *free_buffer = NULL;
	struct tw_bbox damage;
	shm_buffer_draw_t draw_cb = surf->user_data;
	bool *committed; bool *dirty;

	for (int i = 0; i < 2; i++) {
		if (surf->committed[i] || surf->dirty[i])
			continue;
		free_buffer = surf->wl_buffer[i];
		dirty = &surf->dirty[i];
		committed = &surf->committed[i];
		break;
	}
	if (!free_buffer) //I should never be here, should I stop in this function?
		return;
	*dirty = true;
	//also, we should have frame callback here.
	if (surf->need_animation)
		tw_appsurf_request_frame(surf);
	wl_surface_attach(surf->wl_surface, free_buffer, 0, 0);
	draw_cb(surf, free_buffer, &damage);
	wl_surface_damage(surf->wl_surface,
			  damage.x, damage.y, damage.w, damage.h);
	//if output has transform, we need to add it here as well.
	//wl_surface_set_buffer_transform && wl_surface_set_buffer_scale
	wl_surface_commit(surf->wl_surface);
	*committed = true;
	*dirty = false;
}

WL_EXPORT void
shm_buffer_destroy_app_surface(struct tw_appsurf *surf)
{
	for (int i = 0; i < 2; i++) {
		tw_shm_pool_buffer_free(surf->wl_buffer[i]);
		surf->dirty[i] = false;
		surf->committed[i] = false;
	}
	tw_shm_pool_release(surf->pool);
	free(surf->pool);
	surf->pool = NULL;
	surf->user_data = NULL;
}

WL_EXPORT void
shm_buffer_impl_app_surface(struct tw_appsurf *surf,
                            shm_buffer_draw_t draw_call,
                            const struct tw_bbox geo)
{
	surf->do_frame = shm_buffer_surface_swap;
	surf->user_data = draw_call;
	surf->destroy = shm_buffer_destroy_app_surface;
	surf->pool = NULL;
	surf->allocation = geo;
	surf->pending_allocation = geo;
	wl_surface_set_buffer_scale(surf->wl_surface, geo.s);
	shm_buffer_reallocate(surf, &geo);
}

/*******************************************************************************
 * embeded_buffer_impl_surface
 ******************************************************************************/

static void
embeded_app_surface_do_frame(struct tw_appsurf *surf,
                             const struct tw_app_event *e)
{
	if (!surf->parent)
		return;
	surf->parent->do_frame(surf->parent, e);
}

static void
embeded_app_unhook(struct tw_appsurf *surf)
{
	surf->parent = NULL;
	surf->tw_globals = NULL;
	surf->allocation = (struct tw_bbox){0};
	surf->pending_allocation = (struct tw_bbox){0};
}

WL_EXPORT void
embeded_impl_app_surface(struct tw_appsurf *surf, struct tw_appsurf *parent,
                         const struct tw_bbox geo)

{
	surf->wl_surface = NULL;
	/* surf->protocol = NULL; */
	surf->parent = parent;
	surf->tw_globals = parent->tw_globals;
	surf->do_frame = embeded_app_surface_do_frame;
	surf->allocation = geo;
	surf->pending_allocation = geo;
	surf->destroy = embeded_app_unhook;
	wl_list_init(&surf->filter_head);
}
