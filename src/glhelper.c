#include "ctypes/helpers.h"
#include <stdarg.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#include <twclient/glhelper.h>
#include <wayland-util.h>

#include <GL/gl.h>
#include <GL/glext.h>

struct tw_fb_texture {
	struct tw_gl_texture tex;
	GLenum attachment;
	uint32_t idx;
};

static inline bool
diagnose_shader(GLuint shader, GLenum type)
{
	GLint status, loglen;

	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &loglen);
	if (status != GL_TRUE) {
		char log_error[loglen+1];
		glGetShaderInfoLog(shader, loglen+1, &loglen, log_error);
	}
	return status == GL_TRUE;
}

static inline bool
diagnose_program(GLuint prog)
{
	GLint st;
	glGetProgramiv(prog, GL_LINK_STATUS, &st);

	return st == GL_TRUE;
}

static GLuint
compile_shader(GLenum type, const GLchar *src)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);
	if (!diagnose_shader(shader, type))
		return 0;
	return shader;
}

uint32_t
tw_gl_create_program(const char *vs_src, const char *fs_src,
                     const char *gs_src,
                     const char *tcs_src, const char *tes_src)
{
	GLuint p = 0, gs = 0, tcs = 0, tes = 0;
	GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src);
	GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);

	if (!vs || !fs)
		return 0;
	if (gs_src)
		gs = compile_shader(GL_GEOMETRY_SHADER, gs_src);
	if (gs_src && !gs)
		goto err_gs;

	if (tes_src)
		tes = compile_shader(GL_TESS_EVALUATION_SHADER, tes_src);
	if (tes_src && !tes)
		goto err_tes;
	if (tcs_src)
		tcs = compile_shader(GL_TESS_CONTROL_SHADER, tcs_src);
	if (tcs_src && !tcs)
		goto err_tcs;

	p = glCreateProgram();
	if (!p)
		goto err_create_prog;

	GLuint shaders[] = {vs, fs, gs, tes, tcs};
	for (unsigned i = 0; i < NUMOF(shaders); i++)
		if (shaders[i])
			glAttachShader(p, shaders[i]);
	glLinkProgram(p);
	for (unsigned i = 0; i < NUMOF(shaders); i++)
		if (shaders[i]) {
			glDetachShader(p, shaders[i]);
			glDeleteShader(shaders[i]);
		}
	if (!diagnose_program(p)) {
		glDeleteProgram(p);
		return 0;
	}
	return p;
err_create_prog:
	if (tcs)
		glDeleteProgram(tcs);
err_tcs:
	if (tes)
		glDeleteShader(tes);
err_tes:
	if (gs)
		glDeleteShader(gs);
err_gs:
	glDeleteShader(vs);
	glDeleteShader(fs);
	return 0;
}

void
tw_gl_stage_begin(struct tw_gl_stage *stage, const float clear_color[4],
                  float clear_depth, int clear_stencil)
{
	struct tw_fb_texture *fbtex;
	glBindFramebuffer(GL_FRAMEBUFFER, stage->framebuffer);

	wl_array_for_each(fbtex, &stage->buffers) {
		switch (fbtex->attachment) {
		case GL_DEPTH_ATTACHMENT:
			glClearBufferfv(GL_DEPTH, 0, &clear_depth);
			break;
		case GL_STENCIL_ATTACHMENT:
			glClearBufferiv(GL_STENCIL, 0, &clear_stencil);
			break;
		case GL_DEPTH_STENCIL_ATTACHMENT:
			glClearBufferfi(GL_DEPTH_STENCIL, 0,
			                clear_depth, clear_stencil);
			break;
		default:
			glClearBufferfv(GL_COLOR, fbtex->idx, clear_color);
			break;
		}
	}
}

void
tw_gl_stage_end(struct tw_gl_stage *stage)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


bool
tw_gl_stage_init(struct tw_gl_stage *stage, struct tw_gl_texture *depth,
                 struct tw_gl_texture *stencil, uint32_t ubuffers, ...)
{
	GLuint fb;
	struct tw_gl_texture *gltex;
	struct tw_fb_texture *fbtex;

	glGenFramebuffers(1, &fb);
	if (!fb) return false;

	wl_array_init(&stage->buffers);
	glBindFramebuffer(GL_FRAMEBUFFER, fb);

	if (depth && stencil && depth == stencil) {
		glBindTexture(depth->target, depth->tex);
		if (depth->glformat != GL_DEPTH_STENCIL)
			goto err;
		glFramebufferTexture2D(GL_FRAMEBUFFER,
		                       GL_DEPTH_STENCIL_ATTACHMENT,
		                       depth->target, depth->tex, 0);
		fbtex = wl_array_add(&stage->buffers, sizeof(*fbtex));
		fbtex->attachment = GL_DEPTH_STENCIL_ATTACHMENT;
		fbtex->tex = *depth;
	} else if (depth) {
		glBindTexture(depth->target, depth->tex);
		if (depth->glformat != GL_DEPTH_COMPONENT)
			goto err;
		glFramebufferTexture2D(GL_FRAMEBUFFER,
		                       GL_DEPTH_COMPONENT, depth->target,
		                       depth->tex, 0);
		fbtex = wl_array_add(&stage->buffers, sizeof(*fbtex));
		fbtex->attachment = GL_DEPTH_ATTACHMENT;
		fbtex->tex = *depth;

	} else if (stencil) {
		glBindTexture(stencil->target, stencil->tex);
		if (stencil->glformat != GL_STENCIL_INDEX)
			goto err;
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_STENCIL_COMPONENTS,
		                       stencil->target, stencil->tex, 0);
		fbtex = wl_array_add(&stage->buffers, sizeof(*fbtex));
		fbtex->attachment = GL_STENCIL_ATTACHMENT;
		fbtex->tex = *stencil;
	}

	va_list ap;
	va_start(ap, ubuffers);
	for (unsigned i = 0; i < ubuffers; i++) {
		gltex = va_arg(ap, struct tw_gl_texture *);
		glBindTexture(gltex->target, gltex->tex);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0+i,
		                       gltex->target, gltex->tex, 0);
		fbtex = wl_array_add(&stage->buffers, sizeof(*fbtex));
		fbtex->attachment = GL_COLOR_ATTACHMENT0 + i;
		fbtex->tex = *gltex;
	}
	va_end(ap);

	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) !=
	    GL_FRAMEBUFFER_COMPLETE)
		goto err;

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	stage->framebuffer = fb;
	return true;

err:
	wl_array_release(&stage->buffers);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fb);
	return false;
}

void
tw_gl_stage_fini(struct tw_gl_stage *stage)
{
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &stage->framebuffer);
	wl_array_release(&stage->buffers);
}
