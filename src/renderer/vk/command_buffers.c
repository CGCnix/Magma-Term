#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>


#include <magma/private/renderer/vk.h>
#include <magma/logger/log.h>


VkResult magmaVkCreateCommandPool(magma_vk_renderer_t *vk) {
	VkCommandPoolCreateInfo createInfo = { 0 };
	VkCommandBufferAllocateInfo allocInfo = { 0 };


	createInfo.queueFamilyIndex = vk->indicies.graphics;
	createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;


	vkCreateCommandPool(vk->device, &createInfo, vk->alloc, &vk->command_pool);

	allocInfo.commandPool = vk->command_pool;

	return vkAllocateCommandBuffers(vk->device, &allocInfo, &vk->draw_buffer);
}


