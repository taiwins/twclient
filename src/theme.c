/*
 * theme.c - taiwins client theme functions
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

#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <os/buffer.h>

#include <theme.h>

void
tw_theme_init_from_fd(struct tw_theme *theme, int fd, size_t size)
{
	void *addr = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
	memcpy(theme, addr, sizeof(struct tw_theme));
	if (theme->handle_pool.size == 0)
		return;
	//initialize
	size_t handles_size = theme->handle_pool.size;
	size_t strings_size = theme->string_pool.size;
	assert(handles_size + strings_size + sizeof(struct tw_theme) == size);

	const void *handle_addr = (const char *)addr + sizeof(struct tw_theme);
	const void *strings_addr = (const char *)handle_addr +
		handles_size;

	wl_array_init(&theme->handle_pool);
	wl_array_add(&theme->handle_pool, handles_size);
	wl_array_init(&theme->string_pool);
	wl_array_add(&theme->string_pool, strings_size);

	memcpy(theme->handle_pool.data, handle_addr, handles_size);
	memcpy(theme->string_pool.data, strings_addr, strings_size);
}

/* this function should be in the server */
void
tw_theme_to_fd(struct tw_theme *theme)
{
	struct anonymous_buff_t buff;
	void *mapped;
	size_t mapsize = sizeof(struct tw_theme) + theme->handle_pool.size +
		theme->string_pool.size;
	int fd = anonymous_buff_new(&buff, mapsize, PROT_READ | PROT_WRITE, MAP_SHARED);
	if (fd < 0)
		return;
	mapped = buff.addr;
	memcpy(mapped, theme, sizeof(struct tw_theme));
	((struct tw_theme *)mapped)->handle_pool.data = NULL;
	((struct tw_theme *)mapped)->string_pool.data = NULL;
	mapped = (char *)mapped + sizeof(struct tw_theme);
	memcpy(mapped, theme->handle_pool.data, theme->handle_pool.size);
	mapped = (char *)mapped + theme->handle_pool.size;
	memcpy(mapped, theme->string_pool.data, theme->string_pool.size);
	anonymous_buff_close_file(&buff);
}
