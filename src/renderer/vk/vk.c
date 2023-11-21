#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <magma/renderer/vk.h>
#include <magma/backend/backend.h>
#include <magma/logger/log.h>
#include <magma/private/renderer/vk.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#ifdef MAGMA_VK_DEBUG
VkAllocationCallbacks *magma_vk_allocator(void);
void magma_vk_allocator_print_totals(void);
#endif


magma_vk_renderer_t *magma_vk_renderer_init(magma_backend_t *backend) {
	VkResult res;
	magma_vk_renderer_t *vk = calloc(1, sizeof(*vk));
	if(!vk) {
		magma_log_error("calloc: Failed to allocate Vulkan renderer struct\n");
		goto error_alloc_vk_struct;
	}

#ifdef MAGMA_VK_DEBUG
	vk->alloc = magma_vk_allocator();
#else
	vk->alloc = NULL;
#endif /* ifdef MAGMA_VK_DEBUG */

	res = magma_vk_create_instance(backend, vk->alloc, &vk->instance);
	if(res) {
		magma_log_error("Failed to create vulkan interface\n");
		goto error_vk_create_instance;
	}

#ifdef MAGMA_VK_DEBUG
	magma_vk_create_debug_messenger(vk->instance, vk->alloc, &vk->debug_messenger);
#endif

	magma_vk_get_physical_device(vk);

	return vk;

#ifdef MAGMA_VK_DEBUG
	vkDestroyDebugUtilsMessengerEXT(vk->instance, vk->debug_messenger, NULL);
#endif
	vkDestroyInstance(vk->instance, NULL);
error_vk_create_instance:
	free(vk);
error_alloc_vk_struct:
	return NULL;
}

void magma_vk_renderer_deinit(magma_vk_renderer_t *vk) {

	magma_log_warn("VK DEINIT\n");
	
#ifdef MAGMA_VK_DEBUG
	vkDestroyDebugUtilsMessengerEXT(vk->instance, vk->debug_messenger, vk->alloc);
#endif
	vkDestroyInstance(vk->instance, vk->alloc);

#ifdef MAGMA_VK_DEBUG
	magma_vk_allocator_print_totals();
#endif /* ifdef MACRO */

	free(vk);
}
