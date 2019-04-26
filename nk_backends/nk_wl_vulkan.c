#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <linux/input.h>
#include <time.h>
#include <stdbool.h>
#include <assert.h>

#ifndef VK_USE_PLATFORM_WAYLAND_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif

#include <vulkan/vulkan.h>

#define NK_IMPLEMENTATION
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_VK_BACKEND

#define NK_EGL_CMD_SIZE 4096
#define MAX_VERTEX_BUFFER 512 * 128
#define MAX_ELEMENT_BUFFER 128 * 128

//nuklear features
#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_FONT
#define NK_ZERO_COMMAND_MEMORY

#include "../client.h"
#include "../ui.h"
#include "nk_wl_internal.h"

#ifdef __DEBUG
static const char *VALIDATION_LAYERS[] = {
	"VK_LAYER_LUNARG_standard_validation",
};

static const char *INSTANCE_EXTENSIONS[] = {
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
};

#define VL_COUNT 1
#define INS_EXT_COUNT 3

#else

static const char *INSTANCE_EXTENSIONS[] = {
	VK_KHR_SURFACE_EXTENSION_NAME,
	VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
};

#define VL_COUNT 0
#define INS_EXT_COUNT 2

#endif

static const char* DEVICE_EXTENSIONS[] = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

#define DEV_EXT_COUNT 1



struct nk_vulkan_backend {
	struct nk_wl_backend base;
	//opengl command buffer
	struct nk_buffer cmds;
	//vulkan states
	VkInstance instance;
#ifdef __DEBUG
	VkDebugUtilsMessengerEXT debug_callback;
#endif
	VkPhysicalDevice phy_device;
	//there is no way to create shader before having a logic device, and you
	//need to choose the device based on a VkSurfaceKHR, so there si no way
	//to avoid all the verbose callback

	VkDevice logic_device;
	//we need a queue that support graphics and presentation
	VkQueue graphics_queue;
	VkQueue present_queue;

	uint32_t graphics_idx;
	uint32_t present_idx;

	VkCommandPool cmd_pool;

	VkShaderModule vert_shader;
	VkShaderModule pixel_shader;

	VkAllocationCallbacks *alloc_callback;

};


static void
nk_wl_render(struct nk_wl_backend *bkend)
{
	fprintf(stderr, "this sucks\n");
}


#ifdef __DEBUG
static inline bool
check_validation_layer(const VkLayerProperties *layers, uint32_t layer_count,
		       const char *check_layers[], uint32_t clayer_count)
{

	for (int i = 0; i < clayer_count; i++) {
		const char *to_check = check_layers[i];
		bool layer_found = false;
		for (int j = 0; j < layer_count; j++) {
			if (!strcmp(to_check, layers[j].layerName)) {
				layer_found = true;
				break;
			}
		}
		if (!layer_found)
			return false;
	}
	return true;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_messenger(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	       VkDebugUtilsMessageTypeFlagsEXT messageType,
	       const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
	       void *pUserData)
{
	const char *message_types[] = {
		"ERRO", "WARN", "info", "verbos"
	};
	int message_type = 2;
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		message_type = 0;
	else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		message_type = 1;
	else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		message_type = 2;
	else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
		message_type = 3;
	fprintf(stderr, "validation layer %s: %s\n", message_types[message_type],
		pCallbackData->pMessage);

	return message_type < 2;
}

#endif


static void
init_instance(struct nk_vulkan_backend *b)
{
	VkApplicationInfo appinfo = {};
	appinfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appinfo.pApplicationName = "nk_vulkan";
	appinfo.applicationVersion = VK_MAKE_VERSION(0, 0, 0);
	appinfo.pEngineName = "No Engine";
	appinfo.engineVersion = 1;
	appinfo.apiVersion = VK_API_VERSION_1_1;

	//define extensions
	//nvidia is not supporting vk_khr_wayland

	VkInstanceCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &appinfo;
	create_info.enabledExtensionCount = INS_EXT_COUNT;
	create_info.ppEnabledExtensionNames = INSTANCE_EXTENSIONS;

#ifdef  __DEBUG
	{
		create_info.enabledLayerCount = VL_COUNT;
		create_info.ppEnabledLayerNames = VALIDATION_LAYERS;
		uint32_t layer_count;
		vkEnumerateInstanceLayerProperties(&layer_count, NULL);
		VkLayerProperties available_layers[layer_count];
		vkEnumerateInstanceLayerProperties(&layer_count,
						   available_layers);
		NK_ASSERT(check_validation_layer(available_layers,
						 layer_count,
						 VALIDATION_LAYERS,
						 1));

	}
#else
	create_info.enabledLayerCount = VL_COUNT;
#endif
	//using backend structure here
	NK_ASSERT(vkCreateInstance(&create_info, b->alloc_callback, &b->instance)
	       == VK_SUCCESS);

#ifdef __DEBUG
	VkDebugUtilsMessengerCreateInfoEXT mesg_info = {};
	mesg_info.sType =
		VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	mesg_info.messageSeverity =
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_FLAG_BITS_MAX_ENUM_EXT;
	mesg_info.messageType =
		VK_DEBUG_UTILS_MESSAGE_TYPE_FLAG_BITS_MAX_ENUM_EXT;
	mesg_info.pfnUserCallback = debug_messenger;
	mesg_info.pUserData = NULL;

	PFN_vkCreateDebugUtilsMessengerEXT mesg_create_fun =
		(PFN_vkCreateDebugUtilsMessengerEXT)
		vkGetInstanceProcAddr(b->instance, "vkCreateDebugUtilsMessengerEXT");
	if (mesg_create_fun != NULL)
		mesg_create_fun(b->instance, &mesg_info,
				b->alloc_callback,
				&b->debug_callback);
#endif

}

static int32_t
select_presentation_queue(VkPhysicalDevice dev, VkSurfaceKHR *surface)
{
	VkBool32 support_presentation;
	uint32_t qf_count;
	int32_t pres_idx = -1;
	vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, NULL);
	VkQueueFamilyProperties qf_probs[qf_count];
	vkGetPhysicalDeviceQueueFamilyProperties(dev, &qf_count, qf_probs);
	for (int i = 0; i < qf_count; i++) {
		vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, *surface, &support_presentation);
		if (support_presentation) {
			pres_idx = i;
			break;
		}
	}
	return pres_idx;
}

static int32_t
select_graphics_queue(VkPhysicalDevice dev)
{
	int32_t dev_graphics_queue = -1;
	uint32_t nqfamilies;

	vkGetPhysicalDeviceQueueFamilyProperties(dev, &nqfamilies, NULL);

	NK_ASSERT(nqfamilies);
	VkQueueFamilyProperties qf_probs[nqfamilies];
	vkGetPhysicalDeviceQueueFamilyProperties(dev, &nqfamilies, qf_probs);
	for (int j = 0; j < nqfamilies; j++) {
		if (qf_probs[j].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			dev_graphics_queue = j;
			break;
		}
	}
	return dev_graphics_queue;
}

static bool
check_phydev_feature(VkPhysicalDevice dev)
{
	bool is_nvidia_driver = false;
	VkPhysicalDeviceProperties dev_probs;
	VkPhysicalDeviceFeatures dev_features;
#if VK_HEADER_VERSION >= 86
#endif
	//this is the best we we can do now, since in this vulkan version, we
	//cannot get the driver information

	vkGetPhysicalDeviceProperties(dev, &dev_probs);
	vkGetPhysicalDeviceFeatures(dev, &dev_features);
	/* fprintf(stderr, "%d, %d, %s\n", dev_probs.deviceID,  dev_probs.vendorID, */
	/*	dev_probs.deviceName); */
	is_nvidia_driver = strstr(dev_probs.deviceName, "GeForce");
	return ( (dev_probs.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
		  dev_probs.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) &&
		 dev_features.geometryShader && (!is_nvidia_driver));
}

static bool
check_device_extensions(VkPhysicalDevice dev)
{
	uint32_t count;
	vkEnumerateDeviceExtensionProperties(dev, NULL, //layername
					     &count, NULL);
	VkExtensionProperties extensions[count];
	vkEnumerateDeviceExtensionProperties(dev, NULL, &count,
					     extensions);
	for (int i = 0; i < DEV_EXT_COUNT; i++) {
		bool found = false;
		for (int j = 0; j < count; j++) {
			if (strcmp(DEVICE_EXTENSIONS[i],
				   extensions[j].extensionName) == 0) {
				found = true;
				break;
			}
		}
		if (!found)
			return false;
	}
	return true;
}

static void
select_phydev(struct nk_vulkan_backend *b, VkSurfaceKHR *surf)
{
	uint32_t device_count = 0;
	int device_idx = -1;
	int32_t gque = -1;
	int32_t pque = -1;
	vkEnumeratePhysicalDevices(b->instance, &device_count, NULL);
	NK_ASSERT(device_count);

	VkPhysicalDevice devices[device_count];
	vkEnumeratePhysicalDevices(b->instance, &device_count, devices);
	for (int i = 0; i < device_count; i++) {
		bool exts_and_features = check_device_extensions(devices[i]) &&
			check_phydev_feature(devices[i]);

		gque = select_graphics_queue(devices[i]);
		pque = select_presentation_queue(devices[i], surf);


		if (gque >= 0 && pque >= 0 && exts_and_features) {
			device_idx = i;
			break;
		}
	}
	NK_ASSERT(device_idx >= 0);
	b->phy_device = devices[device_idx];
	b->graphics_idx = gque;
	b->present_idx = gque;
}

static void
print_devices(struct nk_vulkan_backend *b)
{
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(b->instance, &device_count, NULL);
	NK_ASSERT(device_count);

	VkPhysicalDevice devices[device_count];
	vkEnumeratePhysicalDevices(b->instance, &device_count, devices);
	for (int i = 0; i < device_count; i++) {
		check_device_extensions(devices[i]) &&
			check_phydev_feature(devices[i]);
	}
}

static void
create_logical_dev(struct nk_vulkan_backend *b)
{
	float que_prio = 1.0;
	uint32_t que_idx[2] = {b->graphics_idx, b->present_idx};

	VkDeviceQueueCreateInfo infos[2];
	for (int i = 0; i < 2; i++) {
		infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		infos[i].queueFamilyIndex = que_idx[i];
		infos[i].queueCount = 1;
		infos[i].pQueuePriorities = &que_prio;
	}

	//device features
	VkPhysicalDeviceFeatures dev_features = {};
	VkDeviceCreateInfo dev_info = {};
	dev_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dev_info.pQueueCreateInfos = infos;
	dev_info.queueCreateInfoCount = 2;
	//TODO: add new features
	dev_info.pEnabledFeatures = &dev_features;
	//extensions
	dev_info.enabledExtensionCount = DEV_EXT_COUNT;
	dev_info.ppEnabledExtensionNames = DEVICE_EXTENSIONS;
	//layers
	dev_info.enabledLayerCount = VL_COUNT;
#ifdef __DEBUG //validation layer
	dev_info.ppEnabledLayerNames = VALIDATION_LAYERS;
#endif

	NK_ASSERT(vkCreateDevice(b->phy_device, &dev_info,
			      b->alloc_callback, &b->logic_device) == VK_SUCCESS);
	//get the queues
	vkGetDeviceQueue(b->logic_device, b->graphics_idx, 0, &b->graphics_queue);
	vkGetDeviceQueue(b->logic_device, b->present_idx, 0, &b->present_queue);
}

static VkSurfaceKHR
create_vk_surface(struct wl_display *wl_display, struct wl_surface *wl_surface, VkInstance inst,
	const VkAllocationCallbacks *cb)
{
	VkSurfaceKHR vksurf;
	VkWaylandSurfaceCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
	create_info.display = wl_display;
	create_info.surface = wl_surface;

	NK_ASSERT(vkCreateWaylandSurfaceKHR(inst, &create_info, cb, &vksurf)
		  == VK_SUCCESS);
	return vksurf;
}




static void
create_command_pool(struct nk_vulkan_backend *b)
{
	VkCommandPoolCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	//this allows the command buffer to be implicitly reset when
	//vkbegincommandbuffer is called
	info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	info.queueFamilyIndex = b->graphics_idx;

	NK_ASSERT(vkCreateCommandPool(b->logic_device, &info, b->alloc_callback,
				      &b->cmd_pool) == VK_SUCCESS);
}

static VkSurfaceFormatKHR
choose_surface_format(struct nk_vulkan_backend *vb, VkSurfaceKHR vksurf)
{
	VkSurfaceFormatKHR result;
	VkSurfaceCapabilitiesKHR caps;
	uint32_t nformats = 0;

	NK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vb->phy_device, vksurf, &caps) ==
		  VK_SUCCESS);

	NK_ASSERT(vkGetPhysicalDeviceSurfaceFormatsKHR(vb->phy_device, vksurf, &nformats, NULL) ==
		  VK_SUCCESS);
	NK_ASSERT(nformats > 0);

	VkSurfaceFormatKHR formats[nformats];
	vkGetPhysicalDeviceSurfaceFormatsKHR(vb->phy_device, vksurf, &nformats, formats);

	if (nformats == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
		result.format = VK_FORMAT_B8G8R8_UNORM;
		result.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		return result;
	}
	for (int i = 0; i < nformats; i++) {
		if (formats[i].format == VK_FORMAT_B8G8R8_UNORM &&
		    formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return formats[i];
	}
	return formats[0];
}

static VkPresentModeKHR
choose_present_mode(struct nk_vulkan_backend *vb, VkSurfaceKHR vksurf)
{
	uint32_t npresentmod = 0;

	NK_ASSERT(vkGetPhysicalDeviceSurfacePresentModesKHR(vb->phy_device, vksurf, &npresentmod, NULL) ==
		  VK_SUCCESS);
	NK_ASSERT(npresentmod > 0);

	VkPresentModeKHR presents[npresentmod];
	vkGetPhysicalDeviceSurfacePresentModesKHR(vb->phy_device, vksurf, &npresentmod, presents);

	for (int i = 0; i < npresentmod; i++) {
		if (presents[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			return presents[i];
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}


static void
create_shaders(struct nk_vulkan_backend *b)
{
	//use shaderc to load shaders from string

	VkShaderModuleCreateInfo info = {};
	info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	info.codeSize = 0;
	info.pCode = NULL;
	assert(vkCreateShaderModule(b->logic_device, &info,
				    b->alloc_callback,
				    &b->vert_shader) == VK_SUCCESS);
	assert(vkCreateShaderModule(b->logic_device, &info,
				    b->alloc_callback,
				    &b->pixel_shader) == VK_SUCCESS);
	VkPipelineShaderStageCreateInfo vstage_info = {};
	vstage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vstage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vstage_info.module = b->vert_shader;
}

static void
nk_vulkan_destroy_app_surface(struct app_surface *surf)
{
	struct nk_wl_backend *b = surf->user_data;
	struct nk_vulkan_backend *vb = container_of(b, struct nk_vulkan_backend, base);
	vkDestroySurfaceKHR(vb->instance, surf->vksurf, vb->alloc_callback);
	vkDestroyDevice(vb->logic_device, vb->alloc_callback);


}

///exposed APIS
void
nk_vulkan_impl_app_surface(struct app_surface *surf, struct nk_wl_backend *bkend,
			   nk_wl_drawcall_t draw_cb,
			   uint32_t w, uint32_t h, uint32_t x, uint32_t y)
{
	struct nk_vulkan_backend *vb = container_of(bkend, struct nk_vulkan_backend, base);

	NK_ASSERT(surf->wl_surface);
	NK_ASSERT(surf->wl_globals);

	nk_wl_impl_app_surface(surf, bkend, draw_cb, w, h, x, y, 1);
	surf->vksurf = create_vk_surface(surf->wl_globals->display, surf->wl_surface, vb->instance,
					 vb->alloc_callback);
}

struct nk_wl_backend *
nk_vulkan_backend_create(void)
{
	struct nk_vulkan_backend *backend = malloc(sizeof(struct nk_vulkan_backend));
	backend->alloc_callback = NULL;
	init_instance(backend);
	//we only initialize the instance here. physical device needs the surface to work
	//you cannot create devices without a surface though
	//yeah, creating device, okay, I do not need to
	print_devices(backend);
	return &backend->base;
}

//this function can be used to accelerate the
struct nk_wl_backend *
nk_vulkan_backend_clone(struct nk_wl_backend *b)
{
	//maybe we can simply uses the same backend...
	return NULL;
}

void
nk_vulkan_backend_destroy(struct nk_wl_backend *b)
{
	struct nk_vulkan_backend *vb = container_of(b, struct nk_vulkan_backend, base);
	vkDestroyDevice(vb->logic_device, vb->alloc_callback);

#ifdef  __DEBUG
	PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug =
		(PFN_vkDestroyDebugUtilsMessengerEXT)
		vkGetInstanceProcAddr(vb->instance, "vkDestroyDebugUtilsMessengerEXT");
	if (destroy_debug != NULL)
		destroy_debug(vb->instance, vb->debug_callback, vb->alloc_callback);
#endif
	vkDestroyInstance(vb->instance, vb->alloc_callback);
}
