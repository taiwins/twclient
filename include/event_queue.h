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
 * @brief remove items from event queue
 *
 */
void tw_event_queue_remove_source(struct tw_event_queue *queue, struct tw_event *e);

/**
 * @brief add directly a epoll fd to event queue
 *
 * epoll fd are usually stream files, for normal files (except procfs and
 * sysfs), use `tw_event_queue_add_file`
 */
bool tw_event_queue_add_source(struct tw_event_queue *queue, int fd,
			       struct tw_event *event, uint32_t mask);
/**
 * @brief add a file to inotify watch system
 *
 * this adds a inotify fd to event queue, this does not work for sysfs
 */
bool tw_event_queue_add_file(struct tw_event_queue *queue, const char *path,
			     struct tw_event *e, uint32_t mask);

/**
 * @brief add a device to udev monitoring system
 *
 * subsystem shouldn't be none.
 */
bool tw_event_queue_add_device(struct tw_event_queue *queue, const char *subsystem,
			  const char *devname, struct tw_event *e);

/**
 * @brief returns an allocated udev_device
 *
 * you have the responsibility to free the allocated `udev_device` by calling
 * `udev_device_unref`. And you should only call this function with udev event,
 * otherwise the behavior is undefined
 */
struct udev_device *tw_event_get_udev_device(const struct tw_event *e);

bool tw_event_queue_add_timer(struct tw_event_queue *queue, const struct itimerspec *interval,
			      struct tw_event *event);

bool tw_event_queue_add_wl_display(struct tw_event_queue *queue, struct wl_display *d);

bool tw_event_queue_add_idle(struct tw_event_queue *queue, struct tw_event *e);


#ifdef __cplusplus
}
#endif

#endif /* EOF */
