#ifndef TW_DESK_ENTRY_H
#define TW_DESK_ENTRY_H

#include <stdbool.h>

#include <sequential.h>

#ifdef __cplusplus
extern "C" {
#endif

struct xdg_app_entry {
	char name[128]; //name, all lower case
	char exec[128]; //exec commands
	char icon[128];
	char path[128];
	bool terminal_app;
	//TODO nk_image
};

/**
 * @brief loading an desktop entry from a xdg desktop file
 */
bool xdg_app_entry_from_file(const char *path, struct xdg_app_entry *entry);


/**
 * @brief gather all the application info on the system
 */
vector_t xdg_apps_gather(void);


#ifdef __cplusplus
}
#endif



#endif /* EOF */
