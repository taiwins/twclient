/*
 * event_queue.c - taiwins client event queue functions
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
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <poll.h>
#include <sys/inotify.h>
#include <libudev.h>

#include <wayland-client.h>
#include <wayland-util.h>

#include <sequential.h>
#include <os/buffer.h>
#include <os/file.h>
#include <client.h>

/*******************************************************************************
 * event queue implemnetaiton
 ******************************************************************************/
struct tw_event_source {
	struct wl_list link;
	struct epoll_event poll_event;
	struct tw_event event;
	void (* pre_hook) (struct tw_event_source *);
	void (* close)(struct tw_event_source *);
	int fd;//timer, or inotify fd
	union {
		int wd;//the actual watch id, we create one inotify per source.
		struct udev_monitor *mon;
	};

};
//file local storage
static struct udev *UDEV = NULL;


static void close_fd(struct tw_event_source *s)
{
	close(s->fd);
}

static struct tw_event_source*
alloc_event_source(struct tw_event *e, uint32_t mask, int fd)
{
	struct tw_event_source *event_source = malloc(sizeof(struct tw_event_source));
	wl_list_init(&event_source->link);
	event_source->event = *e;
	event_source->poll_event.data.ptr = event_source;
	event_source->poll_event.events = mask;
	event_source->fd = fd;
	event_source->pre_hook = NULL;
	event_source->close = close_fd;
	return event_source;
}

static inline void
destroy_event_source(struct tw_event_source *s)
{
	wl_list_remove(&s->link);
	if (s->close)
		s->close(s);
	free(s);
}

static inline struct tw_event_source*
event_source_from_fd(struct tw_event_queue *queue, int fd)
{
	struct tw_event_source *pos, *tmp;
	wl_list_for_each_safe(pos, tmp, &queue->head, link) {
		if (pos->fd == fd)
			return pos;
	}
	return NULL;
}

void
tw_event_queue_close(struct tw_event_queue *queue)
{
	struct tw_event_source *event_source, *next;
	wl_list_for_each_safe(event_source, next, &queue->head, link) {
		epoll_ctl(queue->pollfd, EPOLL_CTL_DEL, event_source->fd, NULL);
		destroy_event_source(event_source);
	}
	close(queue->pollfd);
}

WL_EXPORT void
tw_event_queue_run(struct tw_event_queue *queue)
{
	struct epoll_event events[32];
	struct tw_event_source *event_source;

	//poll->produce-event-or-timeout
	while (!queue->quit) {
		//run the idle task first
		while (!wl_list_empty(&queue->idle_tasks)) {
			event_source = container_of(queue->idle_tasks.prev,
						    struct tw_event_source, link);
			event_source->event.cb(&event_source->event, 0);
			destroy_event_source(event_source);
		}
		/* wl_display_dispatch_pending(queue->wl_display); */

		if (queue->wl_display)
			wl_display_flush(queue->wl_display);

		int count = epoll_wait(queue->pollfd, events, 32, -1);
		//right now if we run into any trouble, we just quit
		queue->quit = queue->quit && (count != -1);
		for (int i = 0; i < count; i++) {
			event_source = events[i].data.ptr;
			if (event_source->pre_hook)
				event_source->pre_hook(event_source);
			int output = event_source->event.cb(
				&event_source->event,
				event_source->fd);
			if (output == TW_EVENT_DEL) {
				epoll_ctl(queue->pollfd, EPOLL_CTL_DEL,
					  event_source->fd, NULL);
				destroy_event_source(event_source);
			}
		}

	}
	tw_event_queue_close(queue);
	//udev object is global here. So we destroy it by hand
	if (UDEV) {
		udev_unref(UDEV);
		UDEV = NULL;
	}
	return;
}

WL_EXPORT bool
tw_event_queue_init(struct tw_event_queue *queue)
{
	int fd = epoll_create1(EPOLL_CLOEXEC);
	if (fd == -1)
		return false;
	wl_list_init(&queue->head);
	wl_list_init(&queue->idle_tasks);

	queue->pollfd = fd;
	queue->quit = false;
	return true;
}

/*******************************************************************************
 * inotify
 ******************************************************************************/

static void
read_inotify(struct tw_event_source *s)
{
	ssize_t r; (void)r;
	struct inotify_event events[100];

	r = read(s->fd, events, sizeof(events));
}

static void
close_inotify_watch(struct tw_event_source *s)
{
	inotify_rm_watch(s->fd, s->wd);
	close(s->fd);
}

WL_EXPORT int
tw_event_queue_add_file(struct tw_event_queue *queue, const char *path,
			struct tw_event *e, uint32_t mask)
{
	if (!mask)
		mask = IN_MODIFY | IN_DELETE;
	if (!is_file_exist(path))
		return -1;
	int fd = inotify_init1(IN_CLOEXEC);
	struct tw_event_source *s = alloc_event_source(e, EPOLLIN | EPOLLET, fd);
	wl_list_insert(&queue->head, &s->link);
	s->close = close_inotify_watch;
	s->pre_hook = read_inotify;

	if (epoll_ctl(queue->pollfd, EPOLL_CTL_ADD, fd, &s->poll_event)) {
		destroy_event_source(s);
		return -1;
	}
	s->wd = inotify_add_watch(fd, path, mask);
	return fd;
}

/*******************************************************************************
 * udev
 ******************************************************************************/

static void
close_udev_monitor(struct tw_event_source *src)
{
	struct udev_monitor *mon = src->mon;
	udev_monitor_unref(mon);
}

WL_EXPORT struct udev_device *
tw_event_get_udev_device(struct tw_event_queue *queue, int fd)
{
	//this is actually really risky, passing a pointer
	struct udev_device *dev;
	const struct tw_event_source *source =
		event_source_from_fd(queue, fd);
	if (!source)
		return NULL;
	dev = udev_monitor_receive_device(source->mon);
	return dev;
	//user has the responsibility to unref it
}

WL_EXPORT int
tw_event_queue_add_device(struct tw_event_queue *queue, const char *subsystem,
			  const char *devname, struct tw_event *e)
{
	if (!subsystem)
		return -1;
	if (!UDEV)
		UDEV = udev_new();

	struct udev_monitor *mon = udev_monitor_new_from_netlink(UDEV, "udev");
	udev_monitor_filter_add_match_subsystem_devtype(mon, subsystem, NULL);
	udev_monitor_enable_receiving(mon);
	int fd = udev_monitor_get_fd(mon);

	struct tw_event_source *s = alloc_event_source(e, EPOLLIN | EPOLLET, fd);
	s->mon = mon;
	s->close = close_udev_monitor;
	wl_list_insert(&queue->head, &s->link);

	if (epoll_ctl(queue->pollfd, EPOLL_CTL_ADD, fd, &s->poll_event)) {
		destroy_event_source(s);
		return -1;
	}
	return fd;
}

/*******************************************************************************
 * general source
 ******************************************************************************/

WL_EXPORT int
tw_event_queue_add_source(struct tw_event_queue *queue, int fd,
			  struct tw_event *e, uint32_t mask)
{
	struct tw_event_source *s;

	if (!mask)
		mask = EPOLLIN | EPOLLET;
	s = event_source_from_fd(queue, fd);
	if (s) //already added
		return fd;

	s = alloc_event_source(e, mask, fd);
	wl_list_insert(&queue->head, &s->link);

	if (epoll_ctl(queue->pollfd, EPOLL_CTL_ADD, fd, &s->poll_event)) {
		destroy_event_source(s);
		return -1;
	}
	return fd;
}

WL_EXPORT bool
tw_event_queue_remove_source(struct tw_event_queue *queue, int fd)
{
	struct tw_event_source *source =
		event_source_from_fd(queue, fd);

	if (!source)
		return false;
	else {
		epoll_ctl(queue->pollfd, EPOLL_CTL_DEL, source->fd, NULL);
		destroy_event_source(source);
		return true;
	}
}

WL_EXPORT bool
tw_event_queue_modify_source(struct tw_event_queue *queue, int fd,
                             struct tw_event *e, uint32_t mask)
{
	struct tw_event_source *source =
		event_source_from_fd(queue, fd);

	if (!source)
		return false;
	else {
		source->event = *e;
		epoll_ctl(queue->pollfd, EPOLL_CTL_MOD, fd, &source->poll_event);
		return true;
	}
}

/*******************************************************************************
 * timer
 ******************************************************************************/

static void
read_timer(struct tw_event_source *s)
{
	ssize_t r;
	uint64_t nhit;

	(void)r;
	(void)nhit;
	r = read(s->fd, &nhit, 8);
}

WL_EXPORT int
tw_event_queue_add_timer(struct tw_event_queue *queue,
			 const struct itimerspec *spec, struct tw_event *e)
{
	int fd;
	fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
	if (!fd)
		goto err;
	if (timerfd_settime(fd, 0, spec, NULL))
		goto err_settime;
	struct tw_event_source *s = alloc_event_source(e, EPOLLIN | EPOLLET, fd);
	s->pre_hook = read_timer;
	wl_list_insert(&queue->head, &s->link);
	//you ahve to read the timmer.
	if (epoll_ctl(queue->pollfd, EPOLL_CTL_ADD, fd, &s->poll_event))
		goto err_add;

	return fd;

err_add:
	destroy_event_source(s);
err_settime:
	close(fd);
err:
	return -1;
}

/*******************************************************************************
 * wayland display
 ******************************************************************************/

static int
dispatch_wl_display(struct tw_event *e, int fd)
{
	(void)fd;
	struct tw_event_queue *queue = e->data;
	struct wl_display *display = queue->wl_display;
	while (wl_display_prepare_read(display) != 0)
		wl_display_dispatch_pending(display);
	wl_display_flush(display);
	if (wl_display_read_events(display) == -1)
		queue->quit = true;
	//this quit is kinda different
	if (wl_display_dispatch_pending(display) == -1)
		queue->quit = true;
	wl_display_flush(display);
	return TW_EVENT_NOOP;
}

WL_EXPORT int
tw_event_queue_add_wl_display(struct tw_event_queue *queue, struct wl_display *display)
{
	int fd = wl_display_get_fd(display);
	queue->wl_display = display;
	struct tw_event dispatch_display = {
		.data = queue,
		.cb = dispatch_wl_display,
	};
	struct tw_event_source *s = alloc_event_source(&dispatch_display, EPOLLIN | EPOLLET, fd);
	//don't close wl_display in the end
	s->close = NULL;
	wl_list_insert(&queue->head, &s->link);

	if (epoll_ctl(queue->pollfd, EPOLL_CTL_ADD, fd, &s->poll_event)) {
		destroy_event_source(s);
		return -1;
	}
	return fd;
}


/**
 * @brief add the idle task to the queue.
 *
 * You could have a lot of allocation under one frame. So use this feature
 * carefully. For example, use a cached state to check whether it is necessary
 * to add to this list.
 *
 * an example of event would be resize event, this event is delegated from
 * a wl_event.
 */
WL_EXPORT bool
tw_event_queue_add_idle(struct tw_event_queue *queue, struct tw_event *event)
{
	struct tw_event_source *s = alloc_event_source(event, 0, -1);
	s->event = *event;
	wl_list_insert(&queue->idle_tasks, &s->link);
	return true;
}
