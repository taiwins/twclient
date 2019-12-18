/*
 * shmpool.h - shm buffer management management header
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

#ifndef TW_SHMPOOL_H
#define TW_SHMPOOL_H


#include <wayland-client.h>
#include <wayland-cursor.h>

#include <sequential.h>
#include <os/buffer.h>


#ifdef __cplusplus
extern "C" {
#endif


/******************************************************************************
 *
 * a wl_buffer managerment solution, using a pool based approach
 *
 *****************************************************************************/
struct shm_pool {
	struct anonymous_buff_t file;
	struct wl_shm *shm;
	struct wl_shm_pool *pool;
	struct wl_list wl_buffers;
	enum wl_shm_format format;
};

//we should also add format
int shm_pool_init(struct shm_pool *pool, struct wl_shm *shm, size_t size, enum wl_shm_format format);

/**
 * /brief allocated a wl_buffer with allocated memory
 *
 * Do not set the listener or user_data for wl_buffer once it is created like
 * this
 */
struct wl_buffer *shm_pool_alloc_buffer(struct shm_pool *pool, size_t width, size_t height);

/**
 * @brief declare this buffer is not in use anymore
 * @return return the shm_pool for resource management purpose
 */
struct shm_pool *shm_pool_buffer_free(struct wl_buffer *wl_buffer);

bool shm_pool_release_if_unused(struct shm_pool *pool);


/**
 * /brief set wl_buffer_release_notify callback here since shm_pool_buffer node
 * ocuppies the user_data of wl_buffer
 */
void shm_pool_set_buffer_release_notify(struct wl_buffer *wl_buffer,
					void (*cb)(void *, struct wl_buffer *),
					void *data);

void *shm_pool_buffer_access(struct wl_buffer *wl_buffer);

size_t shm_pool_buffer_size(struct wl_buffer *wl_buffer);

void shm_pool_release(struct shm_pool *pool);


#ifdef __cplusplus
}
#endif



#endif /* EOF */
