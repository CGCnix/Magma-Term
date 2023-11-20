#include "magma/vt.h"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <magma/renderer/vk.h>
#include <magma/backend/backend.h>
#include <magma/logger/log.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

struct queue_indicies {
	uint32_t compute, graphics, transfer;
};

struct magma_vk_renderer {
	VkInstance instance;

	#ifdef MAGMA_VK_DEBUG
	VkDebugUtilsMessengerEXT debug_messenger;
	#endif /* ifdef MAGMA_VK_DEBUG */

	VkPhysicalDevice phy_dev;
	VkDevice device;

	struct queue_indicies indices;
};


static VkResult __magma_vk_check_layers(const char **requested, uint32_t size) {
	VkResult res = 0;
	uint32_t count = 0, reqind, layind;
	VkLayerProperties *layers = NULL;

	/*Sanity Checks*/
	if(requested == NULL || size == 0) {
		return VK_SUCCESS;
	}

	res = vkEnumerateInstanceLayerProperties(&count, NULL);
	if(res != VK_SUCCESS) {
		magma_log_error("vkEnumerateInstanceLayerProperties: %d\n", res);
		return VK_ERROR_LAYER_NOT_PRESENT;
	}

	layers = calloc(count, sizeof(VkLayerProperties));
	if(!layers) {
		magma_log_error("calloc: %s\n", strerror(errno));
		return VK_ERROR_LAYER_NOT_PRESENT;
	}

	res = vkEnumerateInstanceLayerProperties(&count, layers);
	if(res != VK_SUCCESS) {
		free(layers);
		magma_log_error("vkEnumerateInstanceLayerProperties: %d\n", res);
		return VK_ERROR_LAYER_NOT_PRESENT;
	}


	for(reqind = 0; reqind < size; reqind++) {
		res = VK_ERROR_LAYER_NOT_PRESENT;

		for(layind = 0; layind < count; layind++) {
			if(strcmp(requested[reqind], layers[layind].layerName) == 0) {
				magma_log_warn("VkLayer: %s found\n", requested[reqind]);
				res = VK_SUCCESS;
			}
		}

		if(res == VK_ERROR_LAYER_NOT_PRESENT) {
			magma_log_error("VkLayer: %s not found\n", requested[reqind]);
			break;
		}
	}


	return res;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL magma_vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback,
    void* data) {

	magma_log_debug("validation layer: %s\n", callback->pMessage);

    return VK_FALSE;
}

VkResult vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pMessenger) {
	PFN_vkCreateDebugUtilsMessengerEXT function = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	
	return function ? function(instance, pCreateInfo, pAllocator, pMessenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
}


static VkResult magma_vk_check_extensions(char **requested, uint32_t size) {
	VkResult res;
	uint32_t ext_count, reqind, extind;
	VkExtensionProperties *extensions;

	vkEnumerateInstanceExtensionProperties(NULL, &ext_count, NULL);

	extensions = calloc(ext_count, sizeof(*extensions));

	vkEnumerateInstanceExtensionProperties(NULL, &ext_count, extensions);

	for(reqind = 0; reqind < size; reqind++) {
		res = VK_ERROR_EXTENSION_NOT_PRESENT;
		for(extind = 0; extind < ext_count; extind++) {
			if(strcmp(requested[reqind], extensions[extind].extensionName) == 0) {
				magma_log_debug("Vulkan Extension: %s found\n", requested[reqind]);
				res = VK_SUCCESS;
			}
		}

		if(res != VK_SUCCESS) {
			magma_log_warn("Vulkan Extension: %s not found\n", requested[reqind]);
			break;
		}
	}

	return VK_SUCCESS;
}

static VkResult magma_vk_get_required_extensions(magma_backend_t *backend,
		char ***extensions, uint32_t *size) {
	char **backend_extensions;
	uint32_t backend_ext_sz = 0;

	if(extensions == NULL || size == NULL || *extensions != NULL) {
		return VK_ERROR_UNKNOWN;
	}

	magma_backend_get_vk_exts(backend, &backend_extensions, &backend_ext_sz);	

	*extensions = calloc(sizeof(char *), backend_ext_sz + 1);

	for(uint32_t i = 0; i < backend_ext_sz; i++) {
		extensions[0][i] = backend_extensions[i];
	}


#ifdef MAGMA_VK_DEBUG
	extensions[0][backend_ext_sz] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
	backend_ext_sz++;
#endif
	*size = backend_ext_sz;


	for(uint32_t i = 0; i < backend_ext_sz; i++) {
		magma_log_debug("Extension(%s) wanted\n", extensions[0][i]);
	}

	return 0;
}

static VkResult magma_vk_create_instance(magma_backend_t *backend, VkInstance *instance) {
	VkResult res = 0;
	VkInstanceCreateInfo create_info = { 0 };
	VkApplicationInfo app_info = { 0 };
	VkDebugUtilsMessengerCreateInfoEXT debug_msg = { 0 };
	uint32_t layer_count = 0, ext_count = 0;
	char **extensions = NULL;

#ifdef MAGMA_VK_DEBUG
	/* TODO: We need to use XCB, WL,
	 * DRM. So we need to be able to 
	 * put a data structure to addi stuff 
	 * like to this.
	 */

	const char *layers[] = {
		"VK_LAYER_KHRONOS_validation",
	};
	layer_count = 1;

	debug_msg.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debug_msg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
	debug_msg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debug_msg.pfnUserCallback = magma_vk_debug_callback;
	create_info.pNext = &debug_msg;

#else
	const char **layers = NULL;
#endif

	magma_vk_get_required_extensions(backend, &extensions, &ext_count);
	
	res = magma_vk_check_extensions(extensions, ext_count);
	if(res != VK_SUCCESS) {
		return res;
	}

	res = __magma_vk_check_layers(layers, layer_count);
	if(res != VK_SUCCESS) {
		return res;
	}

	app_info.pEngineName = "MagmaVK";
	app_info.engineVersion = 1;
	app_info.pApplicationName = "Magma";
	app_info.applicationVersion = 1;
	app_info.apiVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;

	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	create_info.ppEnabledLayerNames = layers;
	create_info.enabledLayerCount = layer_count;
	create_info.ppEnabledExtensionNames = (const char **)extensions;
	create_info.enabledExtensionCount = ext_count;

	return vkCreateInstance(&create_info, NULL, instance);
}

VkResult magma_vk_create_debug_messenger(VkInstance instance, VkDebugUtilsMessengerEXT *messenger) {
	VkDebugUtilsMessengerCreateInfoEXT debug_msg = { 0 };

	debug_msg.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debug_msg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
	debug_msg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debug_msg.pfnUserCallback = magma_vk_debug_callback;

	return vkCreateDebugUtilsMessengerEXT(instance, &debug_msg, NULL, messenger);
}

uint32_t magma_vk_score_device(VkPhysicalDevice device) {
	uint32_t score;
	VkPhysicalDeviceFeatures feats;
	VkPhysicalDeviceProperties props;

	vkGetPhysicalDeviceFeatures(device, &feats);
	vkGetPhysicalDeviceProperties(device, &props);

	score = 0;

	switch(props.deviceType) {
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			score += 1000;
			break;
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			score += 500;
			break;
		case VK_PHYSICAL_DEVICE_TYPE_CPU:
		case VK_PHYSICAL_DEVICE_TYPE_OTHER:
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		default:
			score += 0;
	}

	score *= feats.geometryShader;
	return score;
}

uint32_t magma_vk_find_queue_families(VkPhysicalDevice device, struct queue_indicies *indices) {
	uint32_t count = 0;
	VkQueueFamilyProperties *queues;
	bool graphics, compute, transfer;
	/*TODO: Should we actually try to choose queues in a better way?
	 * like if there is a deticated queue for each thing we use should
	 * we use those deticated queues rather than just the first 
	 * queues that support what we
	 */
	graphics = 0;
	compute = 0;
	transfer = 0;

	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, NULL);
	
	queues = calloc(count, sizeof(VkQueueFamilyProperties));

	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, queues);

	for(uint32_t i = 0; i < count; i++) {
		magma_log_debug("%d %d\n", queues[i].queueCount, queues[i].queueFlags);
	}

	return 0;
}

VkResult magma_vk_get_physical_device(magma_vk_renderer_t *renderer) {
	VkResult res;
	uint32_t dev_count, dev, score, best_score = 0;
	VkPhysicalDevice *devices, best_dev;

	vkEnumeratePhysicalDevices(renderer->instance, &dev_count, NULL);

	devices = calloc(sizeof(VkPhysicalDevice), dev_count);
	
	vkEnumeratePhysicalDevices(renderer->instance, &dev_count, devices);

	for(dev = 0; dev < dev_count; dev++) {
		VkPhysicalDeviceProperties props;
		vkGetPhysicalDeviceProperties(devices[dev], &props);
		
		score = magma_vk_score_device(devices[dev]);
		
		magma_vk_find_queue_families(devices[dev], &renderer->indices);

		if(score > best_score) {
			best_score = score;
			best_dev = devices[dev];}
		magma_log_debug("Device(%d): %s, Score: %d\n", dev, props.deviceName, score);
	}

	
}

magma_vk_renderer_t *magma_vk_renderer_init(magma_backend_t *backend) {
	VkResult res;
	magma_vk_renderer_t *vk = calloc(1, sizeof(*vk));

	res = magma_vk_create_instance(backend, &vk->instance);
	if(res) {
		magma_log_error("Failed to create vulkan interface\n");
	}

	magma_vk_create_debug_messenger(vk->instance, &vk->debug_messenger);

	magma_vk_get_physical_device(vk);

	return vk;
}
