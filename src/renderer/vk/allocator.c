#include <magma/logger/log.h>
#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>
#include <stdlib.h>
#include <assert.h>

#include <vulkan/vk_platform.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

/* Based on the Mesa Vk Allocator:
 * https://gitlab.freedesktop.org/mesa/mesa/-/blob/main/src/vulkan/util/vk_alloc.c 
 * Just making this to log allocations as they happen to the built in logger when a 
 * debug build is built and in theory the build should print just as many frees as 
 * it does allocations. Thats the hope anyway and if it doesn't well then there is 
 * probably a leak either with me or the driver.
 */

#define MAX_ALIGN alignof(uint64_t)
#define UNUSED(x) ((void)x)

static uint32_t total_allocates;
static uint32_t total_frees;
static uint32_t total_realloc;

#define alloc_scope_str(scope) case VK_SYSTEM_ALLOCATION_SCOPE_ ## scope : return #scope;
static const char *magma_vk_get_allocscope_str(VkSystemAllocationScope scope) {
	switch (scope) {
		alloc_scope_str(INSTANCE);
		alloc_scope_str(CACHE);
		alloc_scope_str(DEVICE);
		alloc_scope_str(OBJECT);
		alloc_scope_str(COMMAND);
		default: return "Unknown";
	}
}

static VKAPI_ATTR void * VKAPI_CALL magma_vk_allocate(void *pUserData, size_t size, 
		size_t alignment, VkSystemAllocationScope allocationScope) {
	void *addr;
	total_allocates++;	
	assert(MAX_ALIGN % alignment == 0);


	addr = malloc(size);

	magma_log_meminfo("Address: %p Allocated Size: %lu, Scope: %s\n", 
			addr, size, magma_vk_get_allocscope_str(allocationScope));

	return addr;
	UNUSED(pUserData);
}

static VKAPI_ATTR void * VKAPI_CALL magma_vk_reallocate(void *pUserData, void *pOriginal,
		size_t size, size_t alignment, VkSystemAllocationScope allocationScope) {
	void *tmp;
	uintptr_t original = (uintptr_t)pOriginal;
	total_realloc++;
	
	assert(MAX_ALIGN % alignment == 0);

	tmp = realloc(pOriginal, size);

	magma_log_meminfo("Address: %p reallocated to %p\n"
			"\tNew Size: %lu, Alignment: %lu\n"
			"\tScope: %s\n", original, tmp, size, 
			alignment, magma_vk_get_allocscope_str(allocationScope));

	return tmp;
	UNUSED(pUserData);
}


static VKAPI_ATTR void VKAPI_CALL magma_vk_free(void *pUserData, void *pMemory) {
	
	magma_log_meminfo("Address: %p Released\n", pMemory);

	total_frees++;
	free(pMemory);
	UNUSED(pUserData);
}

VkAllocationCallbacks *magma_vk_allocator(void) {
	static struct VkAllocationCallbacks allocator = {
		.pfnFree = magma_vk_free,
		.pfnAllocation = magma_vk_allocate,
		.pfnReallocation = magma_vk_reallocate,
	};	

	return &allocator;
}

void magma_vk_allocator_print_totals(void) {
	magma_log_debug("Allocator Totals:\n"
			"\t  Allocates: %u\n\tReallocates: %u\n"
			"\t      Frees: %u\n", total_allocates, total_realloc, total_frees);;
}
