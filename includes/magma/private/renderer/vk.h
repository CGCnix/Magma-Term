#pragma once

#include <magma/renderer/vk.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

struct queue_indicies {
	uint32_t compute, graphics, transfer;
};


struct magma_vk_renderer {
	VkInstance instance;

	#ifdef MAGMA_VK_DEBUG
	VkDebugUtilsMessengerEXT debug_messenger;
	#endif /* ifdef MAGMA_VK_DEBUG */

	VkAllocationCallbacks *alloc;

	VkSurfaceKHR surface;

	VkPhysicalDevice phy_dev;
	VkDevice device;

	struct queue_indicies indicies;
	VkQueue queue; /*TODO: is it guranteeded we can relie on all queue 
					*features being supported on one queue?
					*/
};

VkResult magma_vk_create_instance(magma_backend_t *backend, VkAllocationCallbacks *callbacks, VkInstance *instance);
VkResult magma_vk_create_debug_messenger(VkInstance instance, VkAllocationCallbacks *callbacks, VkDebugUtilsMessengerEXT *messenger);
VkResult magma_vk_get_physical_device(magma_vk_renderer_t *renderer);
VkResult magma_vk_create_device(magma_vk_renderer_t *vk);
