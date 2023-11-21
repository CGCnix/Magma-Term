#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <vulkan/vulkan.h>

#include <magma/logger/log.h>
#include <magma/private/renderer/vk.h>

#include <malloc.h>
#include <vulkan/vulkan_core.h>

#define UNUSED(x) ((void)x)

#ifdef MAGMA_VK_DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL magma_vk_debug_callback( 
	VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback, void* data);
#endif

static VkResult magma_vk_check_layers(const char **requested, uint32_t size) {
	VkResult res = 0;
	uint32_t count = 0, reqind, layind;
	VkLayerProperties *layers = NULL;
	

	/*Sanity Checks*/
	if(requested == NULL || size == 0) {
		return VK_SUCCESS;
	}

	res = vkEnumerateInstanceLayerProperties(&count, NULL);
	if(res != VK_SUCCESS || !count) {
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

	free(layers);
	return res;
}

static VkResult magma_vk_check_extensions(char **requested, uint32_t size) {
	VkResult res;
	uint32_t ext_count, reqind, extind;
	VkExtensionProperties *extensions;

	res = vkEnumerateInstanceExtensionProperties(NULL, &ext_count, NULL);
	if(res != VK_SUCCESS || !ext_count) {
		magma_log_error("vkEnumerateInstanceLayerProperties: %d\n", res);
		return VK_ERROR_LAYER_NOT_PRESENT;
	}

	extensions = calloc(ext_count, sizeof(*extensions));
	if(!extensions) {
		magma_log_error("calloc failed to allocate %s\n", strerror(errno));
		return VK_ERROR_LAYER_NOT_PRESENT;
	}

	vkEnumerateInstanceExtensionProperties(NULL, &ext_count, extensions);
	if(res != VK_SUCCESS || !ext_count) {
		free(extensions);
		magma_log_error("vkEnumerateInstanceLayerProperties: %d\n", res);
		return VK_ERROR_LAYER_NOT_PRESENT;
	}

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

	free(extensions);
	return res;
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

VkResult magma_vk_create_instance(magma_backend_t *backend, VkAllocationCallbacks *callbacks,
		VkInstance *instance) {
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
	
	res = magma_vk_check_layers(layers, layer_count);
	if(res != VK_SUCCESS) {
		return res;
	}

	magma_vk_get_required_extensions(backend, &extensions, &ext_count);
	
	res = magma_vk_check_extensions(extensions, ext_count);
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

	res = vkCreateInstance(&create_info, callbacks, instance);

	free(extensions);
	return res;
}

/*Debug Code*/
#ifdef MAGMA_VK_DEBUG
VkResult vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pMessenger) {
	PFN_vkCreateDebugUtilsMessengerEXT function = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	
	return function ? function(instance, pCreateInfo, pAllocator, pMessenger) : VK_ERROR_EXTENSION_NOT_PRESENT;
}

void vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT messenger, const VkAllocationCallbacks *pAllocator) {
	PFN_vkDestroyDebugUtilsMessengerEXT function = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	
	if(function) {
		function(instance, messenger, pAllocator);
	}
}

VkResult magma_vk_create_debug_messenger(VkInstance instance, VkAllocationCallbacks *callbacks, VkDebugUtilsMessengerEXT *messenger) {
	VkDebugUtilsMessengerCreateInfoEXT debug_msg = { 0 };

	debug_msg.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debug_msg.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
	debug_msg.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debug_msg.pfnUserCallback = magma_vk_debug_callback;

	return vkCreateDebugUtilsMessengerEXT(instance, &debug_msg, callbacks, messenger);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL magma_vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback,
    void* data) {

	magma_log_debug("validation layer: %s\n", callback->pMessage);

    return VK_FALSE;
	
	UNUSED(data);
	UNUSED(message_severity);
	UNUSED(message_type);
}
#endif
