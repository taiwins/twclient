#include <libudev.h>
#include <stdio.h>
#include <string.h>
#include <sys/epoll.h>
#include <poll.h>

#include <event_queue.h>

static int counter = 0;

static struct tw_event_queue queue = {0};

static int
on_device_change(struct tw_event *e, int fd)
{
	counter += 1;

	struct udev_device *dev = tw_event_get_udev_device(e);
	const char *name = udev_device_get_sysname(dev);
		//do our actions
		fprintf(stderr, "device type: %s\n", udev_device_get_devtype(dev));
		fprintf(stderr, "device action: %s\n", udev_device_get_action(dev));
		fprintf(stderr, "device path: %s\n", udev_device_get_syspath(dev));

	/* if (strstr(name, "BAT")) { */

	/* } */
	udev_device_unref(dev);
	if (counter >= 3)
		queue.quit = true;
	return TW_EVENT_NOOP;

}

static struct tw_event e = {
	.data = NULL,
	.cb = on_device_change,
};

int main(int argc, char *argv[])
{
	tw_event_queue_init(&queue);
	/* int fd = open(file, O_RDONLY); */
	tw_event_queue_add_device(&queue, "power_supply", "BAT", &e);
	tw_event_queue_run(&queue);
	return 0;

}





/*
int main(int argc, char *argv[])
{
	struct udev *udev;
	struct udev_enumerate *enumerate;
	struct udev_list_entry *devices, *dev_list_entry;
	struct udev_device *dev;

	udev = udev_new();
	if (!udev)
		return -1;

	enumerate = udev_enumerate_new(udev);
	fprintf(stderr, "add subsystem returns %d\n",
		udev_enumerate_add_match_subsystem(enumerate, "power_supply"));
	fprintf(stderr, "scan devices returns %d\n",
		udev_enumerate_scan_devices(enumerate));
	devices = udev_enumerate_get_list_entry(enumerate);
	udev_list_entry_foreach(dev_list_entry, devices) {
		const char *path;
		const char *name;
		path = udev_list_entry_get_name(dev_list_entry);
		dev = udev_device_new_from_syspath(udev, path);
		name = udev_device_get_sysname(dev);
		//power supply wouldn't have a node
		printf("Device node path %s\n", name);
		if (strstr(name, "BAT")) {
			fprintf(stderr, "found battery");
		}
		//now we just need to get the fd

		//okay, we need more specific
		udev_device_unref(dev);
	}
	//we need to get the fd, but not sure if we want to keep the

	udev_enumerate_unref(enumerate);

	udev_unref(udev);
	return 0;
}
*/
