/*
 * nk_wl_egl.c - taiwins client nuklear egl backend
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <linux/input.h>
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#include <time.h>
#include <stdbool.h>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <wayland-egl.h>
#include <wayland-client.h>

#define NK_EGL_BACKEND

#define NK_EGL_CMD_SIZE 4096
#define MAX_VERTEX_BUFFER 512 * 128
#define MAX_ELEMENT_BUFFER 128 * 128

#include <twclient/egl.h>
#include "nk_wl_internal.h"
#include <ctypes/helpers.h>

//vao layout
//I could probably use more compat format, and we need float32_t
struct nk_egl_vertex {
	float position[2];
	float uv[2];
	nk_byte col[4];
};

//define the globals
static const struct nk_draw_vertex_layout_element vertex_layout[] = {
	{NK_VERTEX_POSITION, NK_FORMAT_FLOAT,
	 NK_OFFSETOF(struct nk_egl_vertex, position)},
	{NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT,
	 NK_OFFSETOF(struct nk_egl_vertex, uv)},
	{NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8,
	 NK_OFFSETOF(struct nk_egl_vertex, col)},
	{NK_VERTEX_LAYOUT_END}
};

/* as we known that we are using the fixed memory here, the size is crucial to
 * our needs, if the size is too small, we will run into `nk_mem_alloc`
 * failed. If we are giving too much memory, it is then a certain waste. 16Mb is
 * the sweat spot that most widgets will fit */

/**
 * @brief nk_egl_backend
 *
 * nuklear EGL backend for wayland, this backend uses EGL and OpenGL 3.3 for
 * rendering the widgets. The rendering loop is a bit different than typical
 * window toolkit like GLFW/SDL.
 *
 * GFLW window toolkit traps everything in a loop. the loops blocks at the
 * system events, then process the events(intput, time for updating
 * normal), then uses the up-to-date information for rendering.
 *
 * In the our case, such loop doesn't exists(yet, we may add the loop support in
 * the future to support FPS control, but it will not be here. GLFW can do more
 * optimization, it can accumlate the events and do just one pass of rendering
 * for multiple events).
 *
 * In our case, the events triggers a frame for the rendering(no rendering if
 * there is no update). Then nuklear updates the context then OpenGL does the
 * rendering.
 */
struct nk_egl_backend {
	struct nk_wl_backend base;
	//OpenGL resource
	struct {
		struct tw_egl_env env;
		bool compiled;
		GLuint glprog, vs, fs;
		GLuint vao, vbo, ebo;
		GLuint font_tex;
		GLint attrib_pos;
		GLint attrib_uv;
		GLint attrib_col;
		//uniforms
		GLint uniform_tex;
		GLint uniform_proj;
	};
	//nuklear resources
	struct nk_buffer cmds;	//cmd to opengl vertices
	struct nk_draw_null_texture *null;
};

static const struct nk_egl_backend *CURRENT_CONTEXT = NULL;
static EGLSurface CURRENT_SURFACE = NULL;


/*******************************************************************************
 * glsl GLES shader
 ******************************************************************************/
#define GL_SHADER_VERSION "#version 330 core\n"
#define GLES_SHADER_VERSION "#version 100\n"

static const GLchar *glsl_vs =
	GL_SHADER_VERSION
	"uniform mat4 ProjMtx;\n"
	"in vec2 Position;\n"
	"in vec2 TexCoord;\n"
	"in vec4 Color;\n"
	"out vec2 Frag_UV;\n"
	"out vec4 Frag_Color;\n"
	"void main() {\n"
	"   Frag_UV = TexCoord;\n"
	"   Frag_Color = Color;\n"
	"   gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
	"}\n";

static const GLchar *glsl_fs =
	GL_SHADER_VERSION
	"precision mediump float;\n"
	"uniform sampler2D Texture;\n"
	"in vec2 Frag_UV;\n"
	"in vec4 Frag_Color;\n"
	"out vec4 Out_Color;\n"
	"void main(){\n"
	"   Out_Color = Frag_Color * texture(Texture, Frag_UV.st);\n"
	"}\n";

static const GLchar *gles_vs =
	GLES_SHADER_VERSION
	"uniform mat4 ProjMtx;\n"
	"attribute vec2 Position;\n"
	"attribute vec2 TexCoord;\n"
	"attribute vec4 Color;\n"
	"varying vec2 Frag_UV;\n"
	"varying vec4 Frag_Color;\n"
	"void main() {\n"
	"   Frag_UV = TexCoord;\n"
	"   Frag_Color = Color;\n"
	"   gl_Position = ProjMtx * vec4(Position.xy, 0, 1);\n"
	"}\n";

static const GLchar *gles_fs =
	GLES_SHADER_VERSION
	"precision mediump float;\n"
	"uniform sampler2D Texture;\n"
	"varying vec2 Frag_UV;\n"
	"varying vec4 Frag_Color;\n"
	"void main(){\n"
	"   gl_FragColor = Frag_Color * texture2D(Texture, Frag_UV.st);\n"
	"}\n";

/*******************************************************************************
 * EGL
 ******************************************************************************/

static inline bool
is_surfless_supported(struct nk_egl_backend *bkend)
{
	const char *egl_extensions =
		eglQueryString(bkend->env.egl_display, EGL_EXTENSIONS);
	//nvidia eglcontext used to have problems
	return (strstr(egl_extensions, "EGL_KHR_create_context") != NULL &&
	        strstr(egl_extensions, "EGL_KHR_surfaceless_context") != NULL);
}

static inline bool
make_current(struct nk_egl_backend *bkend, EGLSurface eglSurface)
{
	if (eglSurface == CURRENT_SURFACE &&
	    bkend == CURRENT_CONTEXT)
		return true;
	struct tw_egl_env *env = &bkend->env;
	ASSERT(eglMakeCurrent(env->egl_display,
	                      eglSurface, eglSurface,
	                      env->egl_context));
	CURRENT_CONTEXT = bkend;
	CURRENT_SURFACE = eglSurface;
	return true;
}

static inline bool
is_opengl_context(struct tw_egl_env *env)
{
	EGLint value;
	eglQueryContext(env->egl_display, env->egl_context,
	                EGL_CONTEXT_CLIENT_TYPE, &value);
	return value == EGL_OPENGL_API;
}

static inline void
diagnose_shader(GLuint shader, const char *type)
{
	GLint status, loglen;

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &loglen);

	if (status != GL_TRUE) {
		char log_error[loglen+1];
		glGetShaderInfoLog(shader, loglen+1, &loglen, log_error);
		fprintf(stderr, "%s shader compiled error: %s\n",
		        type, log_error);
	}
	assert(status == GL_TRUE);
}

static bool
compile_backend(struct nk_egl_backend *bkend, EGLSurface eglsurface)
{
	if (bkend->compiled)
		return true;
	const GLchar *vertex_shader, *fragment_shader;

	GLint status;
	GLsizei stride;

	make_current(bkend, eglsurface);
	/// part 1) OpenGL code
	vertex_shader = is_opengl_context(&bkend->env) ? glsl_vs : gles_vs;
	fragment_shader = is_opengl_context(&bkend->env) ? glsl_fs : gles_fs;

	bkend->glprog = glCreateProgram();
	bkend->vs = glCreateShader(GL_VERTEX_SHADER);
	bkend->fs = glCreateShader(GL_FRAGMENT_SHADER);
	assert(glGetError() == GL_NO_ERROR);

	glShaderSource(bkend->vs, 1, &vertex_shader, 0);
	glCompileShader(bkend->vs);
	diagnose_shader(bkend->vs, "vertex");
	glShaderSource(bkend->fs, 1, &fragment_shader, 0);
	glCompileShader(bkend->fs);
	diagnose_shader(bkend->fs, "fragment");

	glAttachShader(bkend->glprog, bkend->vs);
	glAttachShader(bkend->glprog, bkend->fs);
	glLinkProgram(bkend->glprog);
	glGetProgramiv(bkend->glprog, GL_LINK_STATUS, &status);
	assert(status == GL_TRUE);
	/// part 2) uploading resource
	glUseProgram(bkend->glprog);
	bkend->uniform_tex = glGetUniformLocation(bkend->glprog, "Texture");
	bkend->uniform_proj = glGetUniformLocation(bkend->glprog, "ProjMtx");
	bkend->attrib_pos = glGetAttribLocation(bkend->glprog, "Position");
	bkend->attrib_uv = glGetAttribLocation(bkend->glprog, "TexCoord");
	bkend->attrib_col = glGetAttribLocation(bkend->glprog, "Color");

	assert(bkend->uniform_tex >= 0);
	assert(bkend->uniform_proj >= 0);
	assert(bkend->attrib_pos >= 0);
	assert(bkend->attrib_pos >= 0);
	assert(bkend->attrib_uv  >= 0);
	/// part 3) allocating vertex array
	stride = sizeof(struct nk_egl_vertex);
	off_t vp = offsetof(struct nk_egl_vertex, position);
	off_t vt = offsetof(struct nk_egl_vertex, uv);
	off_t vc = offsetof(struct nk_egl_vertex, col);

	glGenVertexArrays(1, &bkend->vao);
	glGenBuffers(1, &bkend->vbo);
	glGenBuffers(1, &bkend->ebo);
	glBindVertexArray(bkend->vao);
	glBindBuffer(GL_ARRAY_BUFFER, bkend->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bkend->ebo);
	glBufferData(GL_ARRAY_BUFFER, MAX_VERTEX_BUFFER, NULL, GL_STREAM_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_ELEMENT_BUFFER, NULL, GL_STREAM_DRAW);

	assert(bkend->vao);
	assert(bkend->vbo);
	assert(bkend->ebo);
	//setup the offset
	glEnableVertexAttribArray(bkend->attrib_pos);
	glVertexAttribPointer(bkend->attrib_pos, 2, GL_FLOAT, GL_FALSE, stride, (void *)vp);
	glEnableVertexAttribArray(bkend->attrib_uv);
	glVertexAttribPointer(bkend->attrib_uv, 2, GL_FLOAT, GL_FALSE, stride, (void *)vt);
	glEnableVertexAttribArray(bkend->attrib_col);
	glVertexAttribPointer(bkend->attrib_col, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, (void *)vc);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glUseProgram(0);

	return true;
}

static void
release_backend(struct nk_egl_backend *bkend)
{
	//release the font
	if (bkend->compiled) {
		//opengl resource
		glDeleteBuffers(1, &bkend->vbo);
		glDeleteBuffers(1, &bkend->ebo);
		glDeleteVertexArrays(1, &bkend->vao);
		glDetachShader(GL_VERTEX_SHADER, bkend->vs);
		glDeleteShader(bkend->vs);
		glDetachShader(GL_FRAGMENT_SHADER, bkend->fs);
		glDeleteShader(bkend->fs);
		glDeleteProgram(bkend->glprog);
		//nuklear resource
		//egl free context
		eglMakeCurrent(bkend->env.egl_display, NULL, NULL, NULL);
		bkend->compiled = false;
	}
}

/*******************************************************************************
 * NK_EGL_IMAGE
 ******************************************************************************/

static struct nk_image
nk_wl_to_gpu_image(const struct nk_image *cpu_image, struct nk_wl_backend *b)
{
	struct nk_egl_backend *egl_b =
		container_of(b, struct nk_egl_backend, base);
	make_current(egl_b, EGL_NO_SURFACE);

	GLuint texture;
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
	             (GLsizei)cpu_image->w, (GLsizei)cpu_image->h, 0,
	             GL_RGBA, GL_UNSIGNED_BYTE, cpu_image->handle.ptr);
	glBindTexture(GL_TEXTURE_2D, 0);
	return nk_subimage_id(texture, cpu_image->w, cpu_image->h,
	                      nk_rect(cpu_image->region[0],
	                              cpu_image->region[1],
	                              cpu_image->region[2],
	                              cpu_image->region[2]));
}

static void
nk_wl_free_gpu_image(const struct nk_image *gpu_image)
{
	GLuint handle = gpu_image->handle.id;
	glDeleteTextures(1, &handle);
}

WL_EXPORT struct nk_image
nk_wl_image_from_buffer(unsigned char *pixels, struct nk_wl_backend *b,
                        unsigned int width, unsigned int height,
                        unsigned int stride, bool take)
{
	struct nk_image img = nk_image_ptr((void *)pixels);
	struct nk_image gpu_img;

	img.w = width;
	img.h = height;

	gpu_img = nk_wl_to_gpu_image(&img, b);
	if (take)
		free(pixels);
	return gpu_img;
}


/*******************************************************************************
 * NK_EGL_IMAGE
 ******************************************************************************/

struct nk_wl_egl_font {
	struct nk_wl_user_font wl_font;

	int size;
	int scale;
	nk_rune *merged_ranges;
	struct nk_draw_null_texture null;
	struct nk_font *font;
	struct nk_font_atlas atlas;
	struct nk_image font_image;
};

static bool
nk_egl_bake_font(struct nk_wl_egl_font *font,
                 struct nk_wl_backend *b,
                 const struct nk_wl_font_config *config,
                 const char *path)
{
	int atlas_width, atlas_height;
	const void *image_data;

	struct nk_font_config cfg =
		nk_font_config(config->pix_size * config->scale);
	cfg.range = font->merged_ranges;
	cfg.merge_mode = nk_false;

	nk_font_atlas_init_default(&font->atlas);
	nk_font_atlas_begin(&font->atlas);
	font->font = nk_font_atlas_add_from_file(&font->atlas,
	                                         path,
	                                         config->pix_size *
	                                         config->scale,
	                                         &cfg);
	image_data = nk_font_atlas_bake(&font->atlas,
	                                &atlas_width,
	                                &atlas_height,
	                                NK_FONT_ATLAS_RGBA32);
	font->font_image = nk_subimage_ptr(
		(void *)image_data,
		atlas_width, atlas_height,
		nk_rect(0, 0, atlas_width, atlas_height));
	font->font_image = nk_wl_to_gpu_image(&font->font_image, b);

	nk_font_atlas_end(&font->atlas,
	                  nk_handle_id(font->font_image.handle.id),
	                  &font->null);
	nk_font_atlas_cleanup(&font->atlas);
	font->wl_font.user_font = font->font->handle;
	return true;
}

static bool
nk_egl_font_cal_ranges(struct nk_wl_egl_font *font,
                       const struct nk_wl_font_config *config)
{
	int total_range = 0;
	//calculate ranges
	for (int i = 0; i < config->nranges; i++)
		total_range += nk_range_count(config->ranges[i]);
	total_range = 2 * total_range + 1;

	font->merged_ranges = malloc(sizeof(nk_rune) * total_range);
	if (!font->merged_ranges)
		return false;
	//create the ranges
	memcpy(font->merged_ranges, config->ranges[0],
	       sizeof(nk_rune) * (2 * nk_range_count(config->ranges[0]) + 1));
	//additional range
	for (int i = 1; i < config->nranges; i++)
		merge_unicode_range(font->merged_ranges, config->ranges[i],
		                    font->merged_ranges);
	return true;
}

WL_EXPORT struct nk_user_font *
nk_wl_new_font(struct nk_wl_font_config config, struct nk_wl_backend *b)
{
	char *font_path;
	struct nk_egl_backend *egl_b =
		container_of(b, struct nk_egl_backend, base);
	const nk_rune *default_ranges[] = {nk_font_default_glyph_ranges()};
	//make current
	if (!egl_b->env.egl_context)
		return NULL;
	if (!config.name)
		config.name = "Vera";
	if (!config.pix_size)
		config.pix_size = 16;
	if (!config.scale)
		config.scale = 1;
	if (!config.ranges) {
		config.nranges = 1;
		config.ranges = default_ranges;
	}
	config.TTFonly = true;
	if ((font_path = nk_wl_find_font(&config)) == NULL)
	    return NULL;

	struct nk_wl_egl_font *user_font =
		malloc(sizeof(struct nk_wl_egl_font));
	if (!user_font)
		goto err_font;
	//setup textures
	wl_list_init(&(user_font->wl_font.link));
	user_font->size = config.pix_size;
	user_font->scale = config.scale;

	if (!nk_egl_font_cal_ranges(user_font, &config))
		goto err_range;

	if (!nk_egl_bake_font(user_font, b, &config, font_path))
		goto err_bake;
	wl_list_insert(&b->fonts, &user_font->wl_font.link);
	free(font_path);
	return &user_font->wl_font.user_font;
err_bake:
	free(user_font->merged_ranges);
err_range:
	free(user_font);
err_font:
	free(font_path);
	return NULL;
}

WL_EXPORT void
nk_wl_destroy_font(struct nk_user_font *font)
{
	struct nk_wl_user_font *wl_font =
		container_of(font, struct nk_wl_user_font, user_font);
	struct nk_wl_egl_font *user_font =
		container_of(wl_font, struct nk_wl_egl_font, wl_font);
	wl_list_remove(&user_font->wl_font.link);
	free(user_font->merged_ranges);
	nk_font_atlas_clear(&user_font->atlas);
	nk_wl_free_gpu_image(&user_font->font_image);
	free(user_font);
}

/*******************************************************************************
 * NK_EGL_RENDER
 ******************************************************************************/

static void
_nk_egl_draw_begin(struct nk_egl_backend *bkend,
		   struct nk_buffer *vbuf, struct nk_buffer *ebuf,
		   int width, int height)
{
	void *vertices = NULL;
	void *elements = NULL;
	//NOTE update uniform
	GLfloat ortho[4][4] = {
		{ 2.0f,  0.0f,  0.0f, 0.0f},
		{ 0.0f, -2.0f,  0.0f, 0.0f},
		{ 0.0f,  0.0f, -1.0f, 0.0f},
		{-1.0f,  1.0f,  0.0f, 1.0f},
	};
	ortho[0][0] /= (GLfloat)width;
	ortho[1][1] /= (GLfloat)height;
	//use program
	glUseProgram(bkend->glprog);
	glValidateProgram(bkend->glprog);
	glClearColor(bkend->base.main_color.r / 255.0,
		     bkend->base.main_color.g / 255.0,
		     bkend->base.main_color.b / 255.0,
		     bkend->base.main_color.a / 255.0);
	glClear(GL_COLOR_BUFFER_BIT);
	//switches
	glEnable(GL_BLEND);
	glBlendEquation(GL_FUNC_ADD);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_SCISSOR_TEST);

	//uniforms
	glActiveTexture(GL_TEXTURE0);
	glUniform1i(bkend->uniform_tex, 0);
	glUniformMatrix4fv(bkend->uniform_proj, 1, GL_FALSE, &ortho[0][0]);
	//vertex buffers
	//it could be actually a bottle neck
	glBindVertexArray(bkend->vao);
	glBindBuffer(GL_ARRAY_BUFFER, bkend->vbo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bkend->ebo);
	/* assert(glGetError() == GL_NO_ERROR); */
	//I guess it is not really a good idea to allocate buffer every frame.
	//if we already have the glBufferData, we would just mapbuffer
	glBufferData(GL_ARRAY_BUFFER, MAX_VERTEX_BUFFER, NULL, GL_STREAM_DRAW);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_ELEMENT_BUFFER, NULL, GL_STREAM_DRAW);

	vertices = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	elements = glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
	//getting null texture
	struct nk_wl_user_font *wl_font =
		container_of(bkend->base.fonts.next,
		             struct nk_wl_user_font, link);
	struct nk_wl_egl_font *user_font =
		container_of(wl_font, struct nk_wl_egl_font, wl_font);
	struct nk_draw_null_texture null = user_font->null;
	{
		struct nk_convert_config config;
		nk_memset(&config, 0, sizeof(config));
		config.vertex_layout = vertex_layout;
		config.vertex_size = sizeof(struct nk_egl_vertex);
		config.vertex_alignment = NK_ALIGNOF(struct nk_egl_vertex);
		config.null = null;
		config.circle_segment_count = 2;;
		config.curve_segment_count = 22;
		config.arc_segment_count = 2;;
		config.global_alpha = 1.0f;
		config.shape_AA = NK_ANTI_ALIASING_ON;
		config.line_AA = NK_ANTI_ALIASING_ON;
		nk_buffer_init_fixed(vbuf, vertices, MAX_VERTEX_BUFFER);
		nk_buffer_init_fixed(ebuf, elements, MAX_ELEMENT_BUFFER);
		nk_convert(&bkend->base.ctx, &bkend->cmds, vbuf, ebuf, &config);
	}
	glUnmapBuffer(GL_ARRAY_BUFFER);
	glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
}

static void
_nk_egl_draw_end(struct nk_egl_backend *bkend)
{
	glUseProgram(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	glDisable(GL_BLEND);
	glDisable(GL_SCISSOR_TEST);
	//do I need this?
	//nk_buffer_clear(&bkend->cmds);
}

static int
nk_egl_resize(struct tw_event *e, int fd)
{
	struct tw_appsurf *surf = e->data;
	if (surf->pending_allocation.w == surf->allocation.w &&
	    surf->pending_allocation.h == surf->allocation.h &&
	    surf->pending_allocation.s == surf->allocation.s)
		return TW_EVENT_DEL;
	wl_egl_window_resize(surf->eglwin,
			     surf->pending_allocation.w * surf->pending_allocation.s,
			     surf->pending_allocation.h * surf->pending_allocation.s,
			     0, 0);
	surf->allocation = surf->pending_allocation;
	return TW_EVENT_DEL;
}

void
nk_wl_resize(struct tw_appsurf *surf, const struct tw_app_event *e)
{
	surf->pending_allocation.w = e->resize.nw;
	surf->pending_allocation.h = e->resize.nh;
	surf->pending_allocation.s = e->resize.ns;

	struct tw_event re = {
		.data = surf,
		.cb = nk_egl_resize,
	};
	tw_event_queue_add_idle(&surf->tw_globals->event_queue, &re);
}

static void
nk_wl_render(struct nk_wl_backend *b)
{
	struct nk_egl_backend *bkend = container_of(b, struct nk_egl_backend, base);
	struct tw_egl_env *env = &bkend->env;
	struct tw_appsurf *app = b->app_surface;
	struct nk_context *ctx = &b->ctx;
	int width = app->allocation.w;
	int height = app->allocation.h;
	int scale  = app->allocation.s;

	if (nk_wl_maybe_skip(b))
		return;
	make_current(bkend, app->eglsurface);
	glViewport(0, 0, width * scale, height * scale);

	const struct nk_draw_command *cmd;
	nk_draw_index *offset = NULL;
	struct nk_buffer vbuf, ebuf;
	//be awere of this.

	_nk_egl_draw_begin(bkend, &vbuf, &ebuf, width, height);
	//TODO MESA driver has a problem, the first draw call did not work, we can
	//avoid it by draw a quad that does nothing
	nk_draw_foreach(cmd, ctx, &bkend->cmds) {
		if (!cmd->elem_count)
			continue;
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
		//TODO detect if we are drawing using font here, then we can switch
		//use SDF shader
		GLint scissor_region[4] = {
			(GLint)(cmd->clip_rect.x * scale),
			(GLint)((height - (cmd->clip_rect.y + cmd->clip_rect.h)) *
				scale),
			(GLint)(cmd->clip_rect.w * scale),
			(GLint)(cmd->clip_rect.h * scale),
		};
		glScissor(scissor_region[0], scissor_region[1],
			  scissor_region[2], scissor_region[3]);
		glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count,
			       GL_UNSIGNED_SHORT, offset);
		offset += cmd->elem_count;
	}

	_nk_egl_draw_end(bkend);
	eglSwapBuffers(env->egl_display,
		       app->eglsurface);
}

static void
nk_egl_destroy_app_surface(struct tw_appsurf *app)
{
	struct nk_wl_backend *b = app->user_data;
	struct nk_egl_backend *bkend = container_of(b, struct nk_egl_backend, base);
	if (!is_surfless_supported(bkend)) {
		release_backend(bkend);
		nk_wl_release_resources(&bkend->base);
	}
	tw_appsurf_clean_egl(app, &bkend->env);
	nk_wl_clean_app_surface(b);
}

/********************* exposed APIS *************************/
WL_EXPORT void
nk_egl_impl_app_surface(struct tw_appsurf *surf, struct nk_wl_backend *bkend,
			nk_wl_drawcall_t draw_cb, const struct tw_bbox box)

{
	struct nk_egl_backend *b =
		container_of(bkend, struct nk_egl_backend, base);

	nk_wl_impl_app_surface(surf, bkend, draw_cb, box);
	surf->destroy = nk_egl_destroy_app_surface;
	tw_appsurf_init_egl(surf, &b->env);

	if (!is_surfless_supported(b)) {
		b->compiled = compile_backend(b, surf->eglsurface);
		nk_wl_release_resources(bkend);
		struct nk_user_font *user_font =
			nk_wl_new_font(default_config, bkend);
		nk_style_set_font(&bkend->ctx, user_font);
	}
}

WL_EXPORT struct nk_wl_backend*
nk_egl_create_backend(const struct wl_display *display)
{
	//we do not have any font here,
	struct nk_egl_backend *bkend = calloc(1, sizeof(*bkend));
	struct nk_user_font *default_font = NULL;

	nk_wl_backend_init(&bkend->base);
	tw_egl_env_init(&bkend->env, display);
	bkend->compiled = false;
	//compile bkends if supported
	if (is_surfless_supported(bkend)) {
		bkend->compiled = compile_backend(bkend, EGL_NO_SURFACE);
		default_font =
			nk_wl_new_font(default_config, &bkend->base);
		nk_style_set_font(&bkend->base.ctx, default_font);
	}
	nk_buffer_init_default(&bkend->cmds);

	return &bkend->base;
}

WL_EXPORT void
nk_egl_destroy_backend(struct nk_wl_backend *b)
{
	struct nk_egl_backend *bkend =
		container_of(b, struct nk_egl_backend, base);
	nk_wl_backend_cleanup(b);
	release_backend(bkend);
	nk_buffer_free(&bkend->cmds);
	tw_egl_env_end(&bkend->env);
	free(bkend);
}

WL_EXPORT const struct tw_egl_env *
nk_egl_get_current_env(struct nk_wl_backend *b)
{
	struct nk_egl_backend *bkend =
		container_of(b, struct nk_egl_backend, base);
	return &bkend->env;
}


#ifdef __DEBUG
/*
void
nk_egl_resize(struct nk_egl_backend *bkend, int32_t width, int32_t height)
{
	struct tw_appsurf *app_surface = bkend->app_surface;
	app_surface->w = width;
	app_surface->h = height;
	wl_egl_window_resize(app_surface->eglwin, width, height, 0, 0);
	glViewport(0, 0, width, height);
	glScissor(0, 0, width, height);
}
*/
/*
void nk_egl_capture_framebuffer(struct nk_context *ctx, const char *path)
{
	EGLint gl_pack_alignment;
	//okay, I can use glreadpixels, so I don't need additional framebuffer
	struct nk_egl_backend *bkend = container_of(ctx, struct nk_egl_backend, ctx);
	int width = bkend->app_surface->w;
	int height = bkend->app_surface->h;
	int scale = bkend->app_surface->s;

	//create rgba8 data
	unsigned char *data = malloc(width * height * 4);
	cairo_surface_t *s = cairo_image_surface_create_for_data(
		data, CAIRO_FORMAT_ARGB32,
		width, height,
		cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, width));

	glGetIntegerv(GL_PACK_ALIGNMENT, &gl_pack_alignment);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height,
		     GL_RGBA, GL_UNSIGNED_BYTE, data);
	glPixelStorei(GL_PACK_ALIGNMENT, gl_pack_alignment);

	//now flip the image
	cairo_surface_t *s1 = cairo_image_surface_create(
		CAIRO_FORMAT_RGB24, width, height);
	cairo_t *cr = cairo_create(s1);
	cairo_matrix_t matrix;
	cairo_matrix_init(&matrix,
			  1, 0, 0, -1, 0, height);
	cairo_transform(cr, &matrix);
	cairo_set_source_surface(cr, s, 0, 0);
	cairo_paint(cr);
	cairo_surface_write_to_png(s1, path);
	cairo_destroy(cr);
	cairo_surface_destroy(s1);
	cairo_surface_destroy(s);
	free(data);
}
*/
/*
void
nk_egl_debug_command(struct nk_egl_backend *bkend)
{
	const char *command_type[] = {
		"NK_COMMAND_NOP",
		"NK_COMMAND_SCISSOR",
		"NK_COMMAND_LINE",
		"NK_COMMAND_CURVE",
		"NK_COMMAND_RECT",
		"NK_COMMAND_RECT_FILLED",
		"NK_COMMAND_RECT_MULTI_COLOR",
		"NK_COMMAND_CIRCLE",
		"NK_COMMAND_CIRCLE_FILLED",
		"NK_COMMAND_ARC",
		"NK_COMMAND_ARC_FILLED",
		"NK_COMMAND_TRIANGLE",
		"NK_COMMAND_TRIANGLE_FILLED",
		"NK_COMMAND_POLYGON",
		"NK_COMMAND_POLYGON_FILLED",
		"NK_COMMAND_POLYLINE",
		"NK_COMMAND_TEXT",
		"NK_COMMAND_IMAGE",
		"NK_COMMAND_CUSTOM",
	};
	const struct nk_command *cmd = 0;
	int idx = 0;
	nk_foreach(cmd, &bkend->ctx) {
		fprintf(stderr, "%d command: %s \t", idx++, command_type[cmd->type]);
	}
	fprintf(stderr, "\n");
}
*/

/* void */
/* nk_egl_debug_draw_command(struct nk_egl_backend *bkend) */
/* { */
/*	const struct nk_draw_command *cmd; */
/*	size_t stride = sizeof(struct nk_egl_vertex); */

/*	nk_draw_foreach(cmd, &bkend->ctx, &bkend->cmds) { */

/*	} */
/* } */

#endif
