#ifndef TW_EGL_IFACE_H
#define TW_EGL_IFACE_H

#include <EGL/egl.h>
#include <GL/gl.h>
#include <wayland-egl.h>
#include <stdio.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

//maybe move this to wl_globals later
struct egl_env {
	EGLDisplay egl_display;
	EGLContext egl_context;
	struct wl_display *wl_display;
	EGLConfig config;
	//I need to make reference to this
};

bool egl_env_init(struct egl_env *env, const struct wl_display *disp);
/* create shared context so we can save some data */
bool egl_env_init_shared(struct egl_env *this, const struct egl_env *another);
void egl_env_end(struct egl_env *env);

#ifdef __cplusplus
}
#endif





#endif /* EOF */
