/*
 * egl.c - taiwins client egl bindings function
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

#include <time.h>
#include <string.h>

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <dlfcn.h>


#include <GL/gl.h>
#include <GL/glext.h>
#include <wayland-egl.h>
#include <stdbool.h>
#include <cairo/cairo.h>
#include <client.h>
#include <egl.h>
#include <helpers.h>

/*******************************************************************************
 *                          EGL environment
 ******************************************************************************/

static const EGLint egl_context_attribs[] = {
	EGL_CONTEXT_MAJOR_VERSION, 3,
	EGL_CONTEXT_MINOR_VERSION, 3,
	EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
	EGL_NONE,
};


static void
debug_egl_config_attribs(struct tw_egl_env *env)
{
	int size;
	bool yes;

	char *extension_copy;
	char const *egl_extensions =
		eglQueryString(env->egl_display, EGL_EXTENSIONS);
	char const *egl_vendor =
		eglQueryString(env->egl_display, EGL_VENDOR);

        fprintf(stderr, "EGL_ENV: egl vendor using: %s\n", egl_vendor);
	fprintf(stderr, "EGL_ENV: egl_extensions:\n");

	extension_copy = strdup(egl_extensions);
	if (extension_copy) {
		for (char *ext = strtok(extension_copy, " "); ext;
		     ext = strtok(NULL, " "))
			fprintf(stderr, "\t%s\n", ext);
		free(extension_copy);
	}

	eglGetConfigAttrib(env->egl_display, env->config,
	                   EGL_BUFFER_SIZE, &size);
	fprintf(stderr, "EGL_ENV: cfg has buffer size %d\n", size);
	eglGetConfigAttrib(env->egl_display, env->config,
	                   EGL_DEPTH_SIZE, &size);
	fprintf(stderr, "EGL_ENV: cfg has depth size %d\n", size);
	eglGetConfigAttrib(env->egl_display, env->config,
	                   EGL_STENCIL_SIZE, &size);
	fprintf(stderr, "EGL_ENV: cfg has stencil size %d\n", size);

	yes = eglGetConfigAttrib(env->egl_display, env->config,
	                         EGL_BIND_TO_TEXTURE_RGBA, NULL);
	fprintf(stderr, "EGL_ENV: cfg can %s bound to the rgba buffer\n",
		yes ? "" : "not");
}


#ifdef _WITH_NVIDIA
//we need this entry to load the platform library
extern EGLBoolean loadEGLExternalPlatform(int major, int minor,
					  const EGLExtDriver *driver,
					  EGLExtPlatform *platform);
#endif


/*******************************************************************************
 *                          EGL OpenGL environment
 ******************************************************************************/

static const EGLint gl_config_attribs[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_ALPHA_SIZE, 8,
	EGL_DEPTH_SIZE, 24,
	EGL_STENCIL_SIZE, 8,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
	EGL_NONE,
};

static const EGLint gl45_context_attribs[] = {
	EGL_CONTEXT_MAJOR_VERSION, 4,
	EGL_CONTEXT_MINOR_VERSION, 5,
	EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
	EGL_NONE,
};

static const EGLint gl43_context_attribs[] = {
	EGL_CONTEXT_MAJOR_VERSION, 4,
	EGL_CONTEXT_MINOR_VERSION, 3,
	EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
	EGL_NONE,
};

static const EGLint gl33_context_attribs[] = {
	EGL_CONTEXT_MAJOR_VERSION, 3,
	EGL_CONTEXT_MINOR_VERSION, 3,
	EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
	EGL_NONE,
};

bool
init_opengl_context(struct tw_egl_env *env)
{
	int n;
	EGLConfig egl_cfg = {0};

	eglGetConfigs(env->egl_display, NULL, 0, &n);
	ASSERT(EGL_TRUE == eglChooseConfig(env->egl_display,
	                                   gl_config_attribs,
	                                   &egl_cfg, 1, &n));
	if (!n || !egl_cfg)
		return false;
	eglBindAPI(EGL_OPENGL_API);
	env->egl_context = eglCreateContext(env->egl_display,
					    egl_cfg,
					    EGL_NO_CONTEXT,
					    gl45_context_attribs);
	if (!env->egl_context)
		env->egl_context = eglCreateContext(env->egl_display,
		                                    egl_cfg,
		                                    EGL_NO_CONTEXT,
		                                    gl43_context_attribs);
	if (!env->egl_context)
		env->egl_context = eglCreateContext(env->egl_display,
		                                    egl_cfg,
		                                    EGL_NO_CONTEXT,
		                                    gl33_context_attribs);
	if (!env->egl_context)
		return false;
	env->config = egl_cfg;
	return true;
}

/*******************************************************************************
 *                          EGL OpenGL ES environment
 ******************************************************************************/

static const EGLint gles3_config_attribs[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 4,
	EGL_GREEN_SIZE, 4,
	EGL_BLUE_SIZE, 4,
	EGL_ALPHA_SIZE, 4,
	EGL_DEPTH_SIZE, 12,
	EGL_STENCIL_SIZE, 4,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
	EGL_NONE,
};

static const EGLint gles2_config_attribs[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 4,
	EGL_GREEN_SIZE, 4,
	EGL_BLUE_SIZE, 4,
	EGL_ALPHA_SIZE, 4,
	EGL_DEPTH_SIZE, 12,
	EGL_STENCIL_SIZE, 4,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
	EGL_NONE,
};

static const EGLint gles32_context_attribs[] = {
	EGL_CONTEXT_MAJOR_VERSION, 3,
	EGL_CONTEXT_MINOR_VERSION, 2,
	EGL_NONE,
};

static const EGLint gles31_context_attribs[] = {
	EGL_CONTEXT_MAJOR_VERSION, 3,
	EGL_CONTEXT_MINOR_VERSION, 1,
	EGL_NONE,
};

static const EGLint gles30_context_attribs[] = {
	EGL_CONTEXT_MAJOR_VERSION, 3,
	EGL_CONTEXT_MINOR_VERSION, 0,
	EGL_NONE,
};

static const EGLint gles20_context_attribs[] = {
	EGL_CONTEXT_MAJOR_VERSION, 2,
	EGL_CONTEXT_MINOR_VERSION, 0,
	EGL_NONE,
};


static bool
init_opengl_es_context(struct tw_egl_env *env)
{
	int n;
	EGLConfig egl_cfg = {0};
	EGLBoolean gles3 = false;

	eglGetConfigs(env->egl_display, NULL, 0, &n);
	gles3 = eglChooseConfig(env->egl_display,
	                        gles3_config_attribs,
	                        &egl_cfg, 1, &n);
	if (!gles3 &&
	    eglChooseConfig(env->egl_display,
	                    gles2_config_attribs,
	                    &egl_cfg, 1, &n) != EGL_TRUE)
	if (!n || !egl_cfg)
		return false;

	//creating context
	eglBindAPI(EGL_OPENGL_ES_API);
	if (gles3) {
		env->egl_context = eglCreateContext(env->egl_display,
		                                    egl_cfg,
		                                    EGL_NO_CONTEXT,
		                                    gles32_context_attribs);
		if (!env->egl_context)
			env->egl_context = eglCreateContext(env->egl_display,
			                                    egl_cfg,
			                                    EGL_NO_CONTEXT,
			                                    gles31_context_attribs);
		if (!env->egl_context)
			env->egl_context = eglCreateContext(env->egl_display,
			                                    egl_cfg,
			                                    EGL_NO_CONTEXT,
			                                    gles30_context_attribs);
	} else
		env->egl_context = eglCreateContext(env->egl_display,
		                                    egl_cfg,
		                                    EGL_NO_CONTEXT,
		                                    gles20_context_attribs);

	if (!env->egl_context)
		return false;
	env->config = egl_cfg;
	return true;
}

/*******************************************************************************
 *                                egl_env API
 ******************************************************************************/
bool
tw_egl_env_init(struct tw_egl_env *env, const struct wl_display *d)
{
#ifndef EGL_VERSION_1_5
	fprintf(stderr, "the feature requires EGL 1.5 and it is not supported\n");
	return false;
#endif
	env->wl_display = (struct wl_display *)d;
	EGLint major = 0, minor = 0;
	EGLint n;
	int ret = false;
	/* EGLConfig egl_cfg = {0}; */

	env->egl_display = eglGetDisplay((EGLNativeDisplayType)env->wl_display);
	ASSERT(env->egl_display);
	ASSERT(eglInitialize(env->egl_display, &major, &minor) == EGL_TRUE);

	eglGetConfigs(env->egl_display, NULL, 0, &n);
	if (init_opengl_context(env))
		ret = true;
	else if (init_opengl_es_context(env))
		ret = true;
	debug_egl_config_attribs(env);
	return ret;
}

bool
tw_egl_env_init_shared(struct tw_egl_env *this, const struct tw_egl_env *another)
{
	this->wl_display = another->wl_display;
	this->egl_display = another->egl_display;
	this->config = another->config;
	this->egl_context = eglCreateContext(this->egl_display,
					     this->config,
					     (EGLContext)another->egl_context,
					     egl_context_attribs);
	return this->egl_context != NULL;
}


void
tw_egl_env_end(struct tw_egl_env *env)
{
	eglDestroyContext(env->egl_display, env->egl_context);
	eglTerminate(env->egl_display);
}


void *
egl_get_egl_proc_address(const char *address)
{
	const char *extensions = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
	if (extensions &&
	    (strstr(extensions, "EGL_EXT_platform_wayland") ||
	     strstr(extensions, "EGL_KHR_platform_wayland"))) {
		return (void *)eglGetProcAddress(address);
	}
	return NULL;
}


EGLSurface
egl_create_platform_surface(EGLDisplay dpy, EGLConfig config,
			    void *native_window,
			    const EGLint *attrib_list)
{
	static PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC
		create_platform_window = NULL;

	if (!create_platform_window) {
		create_platform_window = (PFNEGLCREATEPLATFORMWINDOWSURFACEEXTPROC)
	    egl_get_egl_proc_address(
		"eglCreatePlatformWindowSurfaceEXT");
	}

	if (create_platform_window)
		return create_platform_window(dpy, config,
					      native_window,
					      attrib_list);

	return eglCreateWindowSurface(dpy, config,
				      (EGLNativeWindowType) native_window,
				      attrib_list);
}
