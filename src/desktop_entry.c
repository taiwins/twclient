#include "vector.h"
#include <ctype.h>
#include <stdlib.h>
#include <dirent.h>

#include <helpers.h>
#include <os/file.h>
#include <desktop_entry.h>

#define MAX_ENTRY_LEN 127


enum xdg_app_section_type {
	XDG_APP_SEC_ENTRY,
	XDG_APP_SEC_ACTION,
	XDG_APP_SEC_UNKNOWN,
};


static inline bool
is_section_name(const char *line)
{
	return (*line == '[' && line[strlen(line)-1] == ']');
}

static inline enum xdg_app_section_type
get_section_type(const char *section)
{
	if (strcasestr(section, "desktop entry"))
		return XDG_APP_SEC_ENTRY;
	else if (strcasestr(section, "desktop action"))
		return XDG_APP_SEC_ACTION;
	return XDG_APP_SEC_UNKNOWN;
}

static inline bool
is_empty_line(const char *line)
{
	return strlen(line) == 0;
}

static inline void
rtrim(char *line)
{
	char *ptr = line + strlen(line) - 1;
	while(ptr >= line && *ptr && isspace(*ptr)) *ptr-- = '\0';
}

static inline char *
ltrim(char *line)
{
	char *ptr = line;
	while (*ptr && isspace(*ptr)) ptr++;
	return ptr;
}


static inline bool
xdg_app_entry_empty(const struct xdg_app_entry *entry)
{
	return strlen(entry->name) == 0 ||
		strlen(entry->exec) == 0;
}

bool
xdg_app_entry_from_file(const char *path, struct xdg_app_entry *entry)

{
	FILE *file = NULL;
	size_t len = 0, allocated = 1000;
	char *rawline = malloc(allocated);
	enum xdg_app_section_type curr_section = XDG_APP_SEC_UNKNOWN;

	if (!is_file_exist(path))
		return false;
	file = fopen(path, "r");
	if (!file)
		return false;

	strcpy(entry->path, "/");
	entry->terminal_app =false;
	while ((len = getline(&rawline, &allocated, file)) != -1) {
		char *line = ltrim(rawline); rtrim(line);
		//get rid of comments first
		char *comment_point = strstr(line, "#");
		char *equal = NULL;
		if (comment_point)
			*comment_point = '\0';

		if (is_empty_line(line))
			continue;
		if (is_section_name(line)) {
			curr_section = get_section_type(line);
			continue;
		}
		if (curr_section != XDG_APP_SEC_ENTRY)
			continue;
		//get key value pair
		equal = strstr(line, "=");
		if (equal)
			*equal = '\0';
		else //a single line
			continue;
		char *key = line;
		char *value = equal + 1;
		bool name_to_long = false;

		rtrim(value);
		//get the entries
		if (strcasecmp(key, "name") == 0) {
			strncpy(entry->name, value,
				MIN(MAX_ENTRY_LEN, strlen(value)));
			name_to_long |= strlen(value) > MAX_ENTRY_LEN;
		} else if (strcasecmp(key, "exec") == 0) {
			strncpy(entry->exec, value,
				MIN(MAX_ENTRY_LEN, strlen(value)));
			name_to_long |= strlen(value) > MAX_ENTRY_LEN;
		} else if (strcasecmp(key, "icon") == 0) {
			strncpy(entry->icon, value,
				MIN(MAX_ENTRY_LEN, strlen(value)));
			name_to_long |= strlen(value) > MAX_ENTRY_LEN;
		} else if (strcasecmp(key, "path") == 0 &&
			   strlen(value) < MAX_ENTRY_LEN) {
			name_to_long |= strlen(value) > MAX_ENTRY_LEN;
			strcpy(entry->path, value);
		} else if (strcasecmp(key, "terminal"))
			entry->terminal_app =
				(strcmp(value, "true") == 0) ? true : false;

		if (name_to_long) {
			free(rawline);
			return false;
		}
	}

	free(rawline);
	fclose(file);
	return !xdg_app_entry_empty(entry);
}


vector_t
xdg_apps_gather(void)
{
	vector_t apps;
	vector_init(&apps, sizeof(struct xdg_app_entry), NULL);

	const char *desktop_dir = "/usr/share/applications";
	int surfix_len = strlen(".desktop");

	DIR *dir = opendir(desktop_dir);
	for (struct dirent *entry = readdir(dir); entry; entry = readdir(dir)) {
		if (entry->d_type != DT_REG)
			continue;

		struct xdg_app_entry app_entry = {0};
		char total_path[1000] = "/usr/share/applications";
		path_concat(total_path, 999, 1, entry->d_name);
		if (strstr(entry->d_name, ".desktop") + surfix_len ==
		    entry->d_name + strlen(entry->d_name) &&
		    xdg_app_entry_from_file(total_path, &app_entry))
			vector_append(&apps, &app_entry);
	}

	return apps;
}
