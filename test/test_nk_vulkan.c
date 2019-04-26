#include "../client.h"
#include "../ui.h"
#include "../nk_backends.h"


int main(int argc, char *argv[])
{
	struct nk_wl_backend *backend = nk_vulkan_backend_create();
	nk_vulkan_backend_destroy(backend);
	free(backend);
	return 0;
}
