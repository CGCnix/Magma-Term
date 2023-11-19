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

struct magma_vk_renderer {
	VkInstance instance;
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

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

	magma_log_debug("validation layer: %s\n", pCallbackData->pMessage);

    return VK_FALSE;
}

static VkResult magma_vk_get_required_extensions(magma_backend_t *backend,
		const char ***extensions, uint32_t *size) {
	if(extensions == NULL || size == NULL || *extensions != NULL) {
		return VK_ERROR_UNKNOWN;
	}



}

static VkResult magma_vk_create_instance(VkInstance *instance) {
	VkResult res = 0;
	VkInstanceCreateInfo create_info = { 0 };
	VkApplicationInfo app_info = { 0 };
	VkDebugUtilsMessengerCreateInfoEXT debug_msg = { 0 };
	uint32_t layer_count = 0, ext_count = 0;

#ifdef MAGMA_VK_DEBUG
	/* TODO: We need to use XCB, WL,
	 * DRM. So we need to be able to 
	 * put a data structure to add stuff 
	 * like to this.
	 */
	const char *extensions[] = {
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
	};
	ext_count = 1;

	const char *layers[] = {
		"VK_LAYER_KHRONOS_validation",
	};
	layer_count = 1;

	debug_msg.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debug_msg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
	debug_msg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debug_msg.pfnUserCallback = debugCallback;
	create_info.pNext = &debug_msg;
	create_info.enabledExtensionCount = ext_count;
	create_info.ppEnabledExtensionNames = extensions;

#else
	const char **layers = NULL;
#endif

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
	create_info.enabledLayerCount = 1;

	return vkCreateInstance(&create_info, NULL, instance);
}

magma_vk_renderer_t *magma_vk_renderer_init(magma_backend_t *backend) {
	magma_vk_renderer_t *vk = calloc(1, sizeof(*vk));

	magma_vk_create_instance(&vk->instance);

	return vk;
}
