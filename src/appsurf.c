#include <assert.h>
#include <client.h>
#include <ui.h>
#include <egl.h>




void
app_surface_init_egl(struct app_surface *surf, struct egl_env *env)
{
	surf->egldisplay = env->egl_display;
	surf->eglwin = wl_egl_window_create(surf->wl_surface,
					    surf->allocation.w * surf->allocation.s,
					    surf->allocation.h * surf->allocation.s);

	surf->eglsurface =
		eglCreateWindowSurface(env->egl_display,
				       env->config,
				       (EGLNativeWindowType)surf->eglwin,
				       NULL);

	assert(surf->eglsurface);
	assert(surf->eglwin);
}

void
app_surface_clean_egl(struct app_surface *surf, struct egl_env *env)
{
	eglMakeCurrent(env->egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE,
		       env->egl_context);
	eglDestroySurface(env->egl_display, surf->eglsurface);
	wl_egl_window_destroy(surf->eglwin);
	surf->eglwin = NULL;
	surf->egldisplay = EGL_NO_DISPLAY;
	surf->eglsurface = EGL_NO_SURFACE;
}

void
app_surface_init(struct app_surface *surf, struct wl_surface *wl_surface,
		 struct wl_globals *globals,
		 enum app_surface_type type, const uint32_t flags)
{
	*surf = (struct app_surface){0};
	surf->wl_surface = wl_surface;
	surf->type = type;
	surf->flags = flags;
	wl_surface_set_user_data(wl_surface, surf);
	surf->wl_globals = globals;
	wl_list_init(&surf->filter_head);
}

void
app_surface_init_default(struct app_surface *app, struct wl_surface *surf,
			 struct wl_globals *g)
{
	app_surface_init(app, surf, g, APP_SURFACE_APP, 0);
}

void
app_surface_release(struct app_surface *surf)
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
_app_surface_run_frame(struct app_surface *surf, const struct app_event *e)
{
	struct app_event_filter *f;
	wl_list_for_each(f, &surf->filter_head, link) {
		if (f->type == e->type &&
		    f->intercept(surf, e))
			return;
	}
	surf->do_frame(surf, e);
}

void
app_surface_frame(struct app_surface *surf, bool anime)
{
	struct wl_globals *g = surf->wl_globals;
	struct app_event e = {
		.type = TW_FRAME_START,
		.time = (g) ? g->inputs.millisec : 0,
	};
	//this is the best we
	surf->need_animation = anime;
	if (anime)
		app_surface_request_frame(surf);
	_app_surface_run_frame(surf, &e);
}


void
app_surface_resize(struct app_surface *surf,
		   uint32_t nw, uint32_t nh,
		   uint32_t ns)
{
	if (surf->flags & APP_SURFACE_NORESIZABLE)
		return;
	struct app_event e;
	e.time = surf->wl_globals->inputs.millisec;
	e.type = TW_RESIZE;
	e.resize.nw = nw;
	e.resize.nh = nh;
	e.resize.ns = ns;
	e.resize.edge = WL_SHELL_SURFACE_RESIZE_BOTTOM_RIGHT;
	e.resize.serial = surf->wl_globals->inputs.serial;
	if (surf->allocation.s != ns)
		wl_surface_set_buffer_scale(surf->wl_surface, ns);
	_app_surface_run_frame(surf, &e);
}

static void
app_surface_frame_done(void *user_data, struct wl_callback *cb, uint32_t data)
{
	struct app_event e = {
		.type = TW_TIMER,
		.time = data,
	};

	fprintf(stderr, "now I am in animation\n");
	if (cb)
		wl_callback_destroy(cb);
	struct app_surface *surf = (struct app_surface *)user_data;
	if (surf->need_animation)
		app_surface_request_frame(surf);
	_app_surface_run_frame(surf, &e);

	surf->last_serial = data;
}

static struct wl_callback_listener app_surface_wl_frame_impl = {
	.done = app_surface_frame_done,
};

void
app_surface_request_frame(struct app_surface *surf)
{
	surf->need_animation = true;
	struct wl_callback *callback = wl_surface_frame(surf->wl_surface);
	wl_callback_add_listener(callback, &app_surface_wl_frame_impl, surf);
}



/**********************************************************
 *                shm_buffer_impl_surface                 *
 **********************************************************/
void
shm_buffer_destroy_app_surface(struct app_surface *surf)
{
	for (int i = 0; i < 2; i++) {
		shm_pool_buffer_free(surf->wl_buffer[i]);
		surf->dirty[i] = false;
		surf->committed[i] = false;
	}
	shm_pool_release(surf->pool);
	free(surf->pool);
	surf->pool = NULL;
	surf->user_data = NULL;
}

/* now the method needs to smartly release the buffer, I may have nk_wl_cairo
 * use this interface as well
 */
static void
shm_wl_buffer_release(void *data, struct wl_buffer *wl_buffer)
{
	struct app_surface *surf = (struct app_surface *)data;
	struct shm_pool *pool = NULL;
	bool inuse = false;
	for (int i = 0; i < 2; i++)
		if (surf->wl_buffer[i] == wl_buffer) {
			inuse = true;
			surf->dirty[i] = false;
			surf->committed[i] = false;
			break;
		}
	if (!inuse) {
		pool = shm_pool_buffer_free(wl_buffer);
		if (shm_pool_release_if_unused(pool))
			free(pool);
	}
}

/* setup the pool and buffer, destroy the previous pool if there is
 */
void
shm_buffer_reallocate(struct app_surface *surf, const struct bbox *geo)
{
	if (surf->pool && shm_pool_release_if_unused(surf->pool))
		free(surf->pool);
	surf->pool = calloc(1, sizeof(struct shm_pool));
	shm_pool_init(surf->pool, surf->wl_globals->shm, bbox_area(geo) * 2,
		      surf->wl_globals->buffer_format);
	for (int i = 0; i < 2; i++) {
		surf->wl_buffer[i] = shm_pool_alloc_buffer(
			surf->pool, geo->w * geo->s, geo->h * geo->s);
		surf->dirty[i] = NULL;
		surf->committed[i] = NULL;
		shm_pool_set_buffer_release_notify(surf->wl_buffer[i],
						   shm_wl_buffer_release, surf);
	}
}


static int
shm_pool_resize_idle(struct tw_event *e, int fd)
{
	struct app_surface *surf = e->data;
	struct bbox *geo = &surf->pending_allocation;

	if (surf->pending_allocation.w == surf->allocation.w &&
	    surf->pending_allocation.h == surf->allocation.h &&
	    surf->pending_allocation.s == surf->allocation.s)
		return TW_EVENT_DEL;

	shm_buffer_reallocate(surf, geo);
	surf->allocation = surf->pending_allocation;

	app_surface_frame(surf, surf->need_animation);
	return TW_EVENT_DEL;
}


void
shm_buffer_resize(struct app_surface *surf, const struct app_event *e)
{
	surf->pending_allocation.w = e->resize.nw;
	surf->pending_allocation.h = e->resize.nh;
	surf->pending_allocation.s = e->resize.ns;
	struct tw_event re = {
		.data = surf,
		.cb = shm_pool_resize_idle,
	};
	tw_event_queue_add_idle(&surf->wl_globals->event_queue, &re);
}

static void
shm_buffer_surface_swap(struct app_surface *surf, const struct app_event *e)
{
	//for shm_buffer, if resize is requested, you would want call
	//app_surface_frame again right after.
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
	struct bbox damage;
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
		app_surface_request_frame(surf);
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


void
shm_buffer_impl_app_surface(struct app_surface *surf, shm_buffer_draw_t draw_call,
			    const struct bbox geo)
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



/**********************************************************
 *              embeded_buffer_impl_surface               *
 **********************************************************/

static void
embeded_app_surface_do_frame(struct app_surface *surf, const struct app_event *e)
{
	if (!surf->parent)
		return;
	surf->parent->do_frame(surf->parent, e);
}

static void
embeded_app_unhook(struct app_surface *surf)
{
	surf->parent = NULL;
	surf->wl_globals = NULL;
	surf->allocation = (struct bbox){0};
	surf->pending_allocation = (struct bbox){0};
}

void
embeded_impl_app_surface(struct app_surface *surf, struct app_surface *parent,
			 const struct bbox geo)

{
	surf->wl_surface = NULL;
	/* surf->protocol = NULL; */
	surf->parent = parent;
	surf->wl_globals = parent->wl_globals;
	surf->do_frame = embeded_app_surface_do_frame;
	surf->allocation = geo;
	surf->pending_allocation = geo;
	surf->destroy = embeded_app_unhook;
	wl_list_init(&surf->filter_head);
}
