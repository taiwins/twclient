/*
 * event_queue.h - event queue header
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

#ifndef TW_EVENT_QUE_H
#define TW_EVENT_QUE_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include <wayland-util.h>
#include <wayland-client.h>
#include <wayland-cursor.h>


#ifdef __cplusplus
extern "C" {
#endif

struct udev_device;

//this is accessible API
enum tw_event_op { TW_EVENT_NOOP, TW_EVENT_DEL };

struct tw_event {
	void *data;
	union wl_argument arg;
	//this return TW_EVENT_NOOP or tw_event_del
	int (*cb)(struct tw_event *event, int fd);
};

//client side event processor
struct tw_event_queue {
	struct wl_display *wl_display;
	int pollfd;
	struct wl_list head;
	struct wl_list idle_tasks;
	bool quit;


};

void tw_event_queue_run(struct tw_event_queue *queue);
bool tw_event_queue_init(struct tw_event_queue *queue);


/**
 * @brief add directly a epoll fd to event queue
 *
 * epoll fd are usually stream files, for normal files (except procfs and
 * sysfs), use `tw_event_queue_add_file`
 */
int tw_event_queue_add_source(struct tw_event_queue *queue, int fd,
                              struct tw_event *event, uint32_t mask);

/**
 * @brief remove items from event queue
 *
 */
bool tw_event_queue_remove_source(struct tw_event_queue *queue, int fd);

bool tw_event_queue_modify_source(struct tw_event_queue *queue, int fd,
                                  struct tw_event *e, uint32_t mask);

/**
 * @brief add a file to inotify watch system
 *
 * this adds a inotify fd to event queue, this does not work for sysfs
 */
int tw_event_queue_add_file(struct tw_event_queue *queue, const char *path,
			     struct tw_event *e, uint32_t mask);

/**
 * @brief add a device to udev monitoring system
 *
 * subsystem shouldn't be none.
 */
int tw_event_queue_add_device(struct tw_event_queue *queue, const char *subsystem,
                              const char *devname, struct tw_event *e);

/**
 * @brief returns an allocated udev_device
 *
 * you have the responsibility to free the allocated `udev_device` by calling
 * `udev_device_unref`. And you should only call this function with udev event,
 * otherwise the behavior is undefined
 */
struct udev_device *tw_event_get_udev_device(struct tw_event_queue *queue, int fd);

int tw_event_queue_add_timer(struct tw_event_queue *queue, const struct itimerspec *interval,
			      struct tw_event *event);

int tw_event_queue_add_wl_display(struct tw_event_queue *queue, struct wl_display *d);

bool tw_event_queue_add_idle(struct tw_event_queue *queue, struct tw_event *e);


#ifdef __cplusplus
}
#endif

#endif /* EOF */
