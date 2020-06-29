/*
 * gl.h - GL(ES) rendering helpers header
 *
 * Copyright (c) 2020 Xichen Zhou
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

#ifndef TW_GL_HELPER_H
#define TW_GL_HELPER_H

#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES
#endif
#if defined (_TW_USE_GL)
#include <GL/gl.h>
#include <GL/glext.h>
#elif defined(_TW_USE_GLES)
#include <GLES3/gl3.h>
#include <GLES3/gl31.h>
#include <GLES3/gl32.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#endif

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <wayland-client.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tw_gl_texture {
	GLenum target;
	GLuint tex;
	GLuint glformat;
	uint32_t samples;
	uint32_t width, height;
};

enum tw_gl_stage_option {
	TW_GL_STAGE_HAS_STENCIL = (1 << 0),
	TW_GL_STAGE_HAS_DEPTH = (1 << 1),
};

/**
 * @brief gl stage represents a stage of a frame buffer. it have its own inputs
 * and outputs, specified in the the format.
 *
 * framebuffer represents a collection of multiple render buffers, on resized
 */
struct tw_gl_stage {
	GLuint framebuffer;
	struct wl_array buffers;
	uint32_t option;
};

/**
 * @brief init a framebuffer using given textures, if given depth and stencil
 * buffer is the same. Then the texture has to be depth_stencil buffer
 */
bool
tw_gl_stage_init(struct tw_gl_stage *stage, struct tw_gl_texture *depth,
                 struct tw_gl_texture *stencil, uint32_t ubuffers, ...);
void
tw_gl_stage_fini(struct tw_gl_stage *stage);

void
tw_gl_stage_begin(struct tw_gl_stage *stage, const float clear_color[4],
                  float clear_depth, int clear_stecil);
void
tw_gl_stage_end(struct tw_gl_stage *stage);

uint32_t
tw_gl_create_program(const char *vs, const char *fs, const char *gs,
                     const char *tcs, const char *tes);


#ifdef __cplusplus
}
#endif


#endif /* EOF */
