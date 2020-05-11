#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <dirent.h>

#include <unistd.h>
#include <ctypes/vector.h>
#include <ctypes/helpers.h>
#include <ctypes/os/file.h>
#include <twclient/desktop_entry.h>


int main(int argc, char *argv[])
{
	const char *desktop_dir = "/usr/share/applications";

	DIR *dir = opendir(desktop_dir);
	for (struct dirent *entry = readdir(dir); entry; entry = readdir(dir)) {
		if (entry->d_type != DT_REG)
			continue;

		struct xdg_app_entry app_entry = {0};
		char total_path[1000] = "/usr/share/applications";
		path_concat(total_path, 999, 1, entry->d_name);
		if (strstr(total_path, ".desktop") &&
			xdg_app_entry_from_file(total_path, &app_entry)) {
			fprintf(stdout, "app name: %s\n", app_entry.name);
			fprintf(stdout, "app exec: %s\n", app_entry.exec);
			fprintf(stdout, "app path: %s\n", app_entry.path);
			fprintf(stdout, "app icon: %s\n", app_entry.icon);
			fprintf(stdout, "\n");
		} else if (strstr(total_path, ".desktop")) {
			fprintf(stdout, "failed to read desktop entry: %s", total_path);
			fprintf(stdout, "\n");
		}
	}
	closedir(dir);

	return 0;
}
