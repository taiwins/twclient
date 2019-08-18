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
		 struct wl_proxy *proxy, struct wl_globals *globals)
{

	*surf = (struct app_surface){0};
	surf->wl_surface = wl_surface;
	surf->protocol = proxy;
	wl_surface_set_user_data(wl_surface, surf);
	surf->wl_globals = globals;
}

void
app_surface_release(struct app_surface *surf)
{
	if (surf->destroy)
		surf->destroy(surf);
	//throw all the callbacks
	wl_surface_destroy(surf->wl_surface);
	if (surf->protocol)
		wl_proxy_destroy(surf->protocol);
	surf->protocol = NULL;
	surf->wl_surface = NULL;
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
	surf->do_frame(surf, &e);
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
	surf->do_frame(surf, &e);

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

static void
shm_buffer_surface_swap(struct app_surface *surf, const struct app_event *e)
{
	if (e->type != TW_FRAME_START)
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

static void
shm_buffer_destroy_app_surface(struct app_surface *surf)
{
	for (int i = 0; i < 2; i++) {
		shm_pool_buffer_free(surf->wl_buffer[i]);
		surf->dirty[i] = false;
		surf->committed[i] = false;
	}
	surf->pool = NULL;
	surf->user_data = NULL;
}

static void
shm_wl_buffer_release(void *data,
		      struct wl_buffer *wl_buffer)
{
	struct app_surface *surf = (struct app_surface *)data;
	for (int i = 0; i < 2; i++)
		if (surf->wl_buffer[i] == wl_buffer) {
			surf->dirty[i] = false;
			surf->committed[i] = false;
			break;
		}
}

static struct wl_buffer_listener shm_wl_buffer_impl = {
	.release = shm_wl_buffer_release,
};


void
shm_buffer_impl_app_surface(struct app_surface *surf, struct shm_pool *pool,
			    shm_buffer_draw_t draw_call, const struct bbox geo)
{
	surf->do_frame = shm_buffer_surface_swap;
	surf->user_data = draw_call;
	surf->destroy = shm_buffer_destroy_app_surface;
	surf->pool = pool;
	surf->allocation = geo;
	surf->pending_allocation = geo;
	wl_surface_set_buffer_scale(surf->wl_surface, geo.s);
	for (int i = 0; i < 2; i++) {
		surf->wl_buffer[i] = shm_pool_alloc_buffer(pool, geo.w*geo.s, geo.h*geo.s);
		wl_buffer_add_listener(surf->wl_buffer[i], &shm_wl_buffer_impl, surf);
		surf->dirty[i] = false;
		surf->committed[i] = false;
		shm_pool_set_buffer_release_notify(surf->wl_buffer[i],
						   shm_wl_buffer_release, surf);
	}
	//TODO we should be able to resize the surface as well.
}






/**********************************************************
 *              embeded_buffer_impl_surface               *
 **********************************************************/

static void
embeded_app_surface_do_frame(struct app_surface *surf, const struct app_event *e)
{
	surf->parent->do_frame(surf->parent, e);
}

void
embeded_impl_app_surface(struct app_surface *surf, struct app_surface *parent,
			 const struct bbox geo)

{
	surf->wl_surface = NULL;
	surf->protocol = NULL;
	surf->parent = parent;
	surf->wl_globals = parent->wl_globals;
	surf->do_frame = embeded_app_surface_do_frame;
	surf->allocation = geo;
	surf->pending_allocation = geo;
}
