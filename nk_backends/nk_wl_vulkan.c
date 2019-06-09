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

	VkSwapchainKHR swap_chain;
	VkFormat image_format;
	VkPresentModeKHR present_mode;
	VkImage images[2];
	VkImageView image_view[2];

	uint32_t graphics_idx;
	uint32_t present_idx;

	VkCommandPool cmd_pool;
	VkCommandBuffer cmd_buffers[2];

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


static bool
check_instance_extensions(void)
{
	uint32_t num_exts = 0;
	vkEnumerateInstanceExtensionProperties(NULL, &num_exts, NULL);
	NK_ASSERT(num_exts > 0);

	VkExtensionProperties props[num_exts];
	vkEnumerateInstanceExtensionProperties(NULL, &num_exts, props);
	for (int j = 0; j < INS_EXT_COUNT; j++) {
		bool found = false;
		for  (int i = 0; i < num_exts; i++) {
			if (strcmp(INSTANCE_EXTENSIONS[j],
				   props[i].extensionName) == 0) {
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
	NK_ASSERT(check_instance_extensions());

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
	VkPhysicalDeviceProperties2 dev_probs;
	VkPhysicalDeviceFeatures dev_features;
	dev_probs.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;

#if VK_HEADER_VERSION >= 86
	VkPhysicalDeviceDriverPropertiesKHR dri_probs;
	dri_probs.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR;
	dev_probs.pNext = &dri_probs;
#endif
	//this is the best we we can do now, since in this vulkan version, we
	//cannot get the driver information
	vkGetPhysicalDeviceProperties2(dev, &dev_probs);
	vkGetPhysicalDeviceFeatures(dev, &dev_features);

#if VK_HEADER_VERSION >= 86
	bool is_nvidia_driver = (dri_probs.driverID == VK_DRIVER_ID_NVIDIA_PROPRIETARY_KHR);
#else
	bool is_nvidia_driver = strstr(dev_probs.properties.deviceName, "GeForce");
#endif

	/* fprintf(stderr, "%d, %d, %s\n", dev_probs.deviceID,  dev_probs.vendorID, */
	/*	dev_probs.deviceName); */
	return ( (dev_probs.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
		  dev_probs.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) &&
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
	float priorities = 1.0;
	uint32_t que_idx[2] = {b->graphics_idx, b->present_idx};

	//we are creating two info here, but the queue index should be unique
	VkDeviceQueueCreateInfo infos[2];
	for (int i = 0; i < 2; i++) {
		infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		infos[i].queueFamilyIndex = que_idx[i];
		infos[i].queueCount = 1;
		infos[i].pQueuePriorities = &priorities;
		infos[i].pNext = NULL;
		infos[i].flags = 0;
	}
	//device features
	VkPhysicalDeviceFeatures dev_features = {};
	VkDeviceCreateInfo dev_info = {};
	dev_info.flags = 0;
	dev_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	dev_info.pQueueCreateInfos = infos;
	dev_info.queueCreateInfoCount = (b->graphics_idx == b->present_idx) ? 1 : 2;
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

static void
create_command_buffer(struct nk_vulkan_backend *b)
{
	VkCommandBufferAllocateInfo info = {
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandPool = b->cmd_pool,
		.commandBufferCount = 2,
	};

	NK_ASSERT(vkAllocateCommandBuffers(b->logic_device, &info, b->cmd_buffers)
		  == VK_SUCCESS);

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
		result.format = VK_FORMAT_B8G8R8A8_UNORM;
		result.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
		return result;
	}
	for (int i = 0; i < nformats; i++) {
		if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
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

static VkExtent2D
choose_surface_extent(VkSurfaceCapabilitiesKHR *cap)
{
	VkExtent2D extent;

	if (cap->currentExtent.width == -1) {
		extent.width = 1000;
		extent.height = 1000;
	} else
		extent = cap->currentExtent;
	return extent;
}

static void
create_swap_chain(struct nk_vulkan_backend *vb, VkSurfaceKHR vksurf)
{
	VkSurfaceCapabilitiesKHR caps;
	NK_ASSERT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vb->phy_device, vksurf, &caps) ==
		  VK_SUCCESS);
	VkExtent2D extent = choose_surface_extent(&caps);
	VkSurfaceFormatKHR surface_format = choose_surface_format(vb, vksurf);
	VkPresentModeKHR present_mode = choose_present_mode(vb, vksurf);


	VkSwapchainCreateInfoKHR info = {
		.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.surface = vksurf,

		.minImageCount = 2,

		.imageFormat = surface_format.format,
		.imageColorSpace = surface_format.colorSpace,

		.imageExtent = extent,
		.imageArrayLayers = 1,
		.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,

		.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,

		.presentMode = present_mode,

		.clipped = VK_TRUE,
	};

	if (vb->graphics_idx != vb->present_idx) {
		uint32_t indices[2] = {vb->graphics_idx, vb->present_idx};
		info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		info.queueFamilyIndexCount  = 2;
		info.pQueueFamilyIndices = indices;

	} else {
		info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	NK_ASSERT(vkCreateSwapchainKHR(vb->logic_device, &info,
				       vb->alloc_callback, &vb->swap_chain) ==
		  VK_SUCCESS);
	vb->image_format = info.imageFormat;
	vb->present_mode = info.presentMode;
}

static void
create_image_views(struct nk_vulkan_backend *vb, VkSurfaceKHR vksurf)
{
	uint32_t num_images;

	NK_ASSERT(vkGetSwapchainImagesKHR(vb->logic_device, vb->swap_chain,
					  &num_images, NULL) == VK_SUCCESS );
	NK_ASSERT(num_images == 2);
	NK_ASSERT(vkGetSwapchainImagesKHR(vb->logic_device, vb->swap_chain, &num_images, vb->images) == VK_SUCCESS);

	for (int i = 0; i < 2; i++) {
		VkImageViewCreateInfo info = {
			.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image = vb->images[i],
			.viewType = VK_IMAGE_VIEW_TYPE_2D,
			.format = vb->image_format,
			.components = {
				.r = VK_COMPONENT_SWIZZLE_R,
				.g = VK_COMPONENT_SWIZZLE_G,
				.b = VK_COMPONENT_SWIZZLE_B,
				.a = VK_COMPONENT_SWIZZLE_A,
			},
			.subresourceRange = {
				.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
				.baseMipLevel = 0,
				.levelCount = 1,
				.baseArrayLayer = 0,
				.layerCount = 1,
			},
			.flags = 0,
		};
		NK_ASSERT(vkCreateImageView(vb->logic_device, &info, vb->alloc_callback, &vb->image_view[i]) == VK_SUCCESS);

	}
}

static void
create_render_pass(struct nk_vulkan_backend *vb)
{
	VkAttachmentDescription color_attachment = {

	};
	color_attachment.format = vb->image_format;
	//multisample settings
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	VkAttachmentReference color_ref = {
		.attachment = 0,
		.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	};
	//we need depth format
	VkAttachmentDescription depth_attachment = {
	};


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
	vkDestroyImageView(vb->logic_device, vb->image_view[0], vb->alloc_callback);
	vkDestroyImageView(vb->logic_device, vb->image_view[1], vb->alloc_callback);
	vkDestroyCommandPool(vb->logic_device, vb->cmd_pool, vb->alloc_callback);
	vkDestroyDevice(vb->logic_device, vb->alloc_callback);
	vkDestroySurfaceKHR(vb->instance, surf->vksurf, vb->alloc_callback);
}

///////////////////////////////////////// exposed APIs ////////////////////////////////////////

void
nk_vulkan_impl_app_surface(struct app_surface *surf, struct nk_wl_backend *bkend,
			   nk_wl_drawcall_t draw_cb,
			   uint32_t w, uint32_t h, uint32_t x, uint32_t y)
{
	struct nk_vulkan_backend *vb = container_of(bkend, struct nk_vulkan_backend, base);

	NK_ASSERT(surf->wl_surface);
	NK_ASSERT(surf->wl_globals);

	nk_wl_impl_app_surface(surf, bkend, draw_cb, w, h, x, y, 1);
	surf->vksurf = create_vk_surface(surf->wl_globals->display,
					 surf->wl_surface, vb->instance,
					 vb->alloc_callback);
	surf->destroy = nk_vulkan_destroy_app_surface;
	//create devices
	select_phydev(vb, &surf->vksurf);

	create_logical_dev(vb);

	create_command_pool(vb);

	create_command_buffer(vb);

	create_swap_chain(vb, surf->vksurf);

	create_image_views(vb, surf->vksurf);
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
	/* print_devices(backend); */
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
#ifdef  __DEBUG
	PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug =
		(PFN_vkDestroyDebugUtilsMessengerEXT)
		vkGetInstanceProcAddr(vb->instance, "vkDestroyDebugUtilsMessengerEXT");
	if (destroy_debug != NULL)
		destroy_debug(vb->instance, vb->debug_callback, vb->alloc_callback);
#endif
	vkDestroyInstance(vb->instance, vb->alloc_callback);
}
