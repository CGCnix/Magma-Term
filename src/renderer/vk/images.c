#include <errno.h>
#include <string.h>
#include <vulkan/vulkan.h>

#include <magma/private/renderer/vk.h>
#include <magma/renderer/vk.h>
#include <magma/logger/log.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <vulkan/vulkan_core.h>


uint32_t getMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t typeBits, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);
	for (uint32_t i = 0; i < deviceMemoryProperties.memoryTypeCount; i++) {
		if ((typeBits & 1) == 1) {
			if ((deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}
		typeBits >>= 1;
	}
	return 0;
}


VkResult magmaVkCreateDstImageView(magma_vk_renderer_t *vk) {
	VkImageCreateInfo imageInfo = { 0 };
	VkMemoryAllocateInfo memAlloc = { 0 };
	VkMemoryRequirements memReqs = { 0 };

	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageInfo.extent.depth = 1;
	imageInfo.extent.width = vk->width;
	imageInfo.extent.height = vk->height;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_LINEAR;
	imageInfo.arrayLayers = 1;
	imageInfo.mipLevels = 1;
	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	vkCreateImage(vk->device, &imageInfo, vk->alloc, &vk->dst_image);

	vkGetImageMemoryRequirements(vk->device, vk->dst_image, &memReqs);

	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = getMemoryTypeIndex(vk->phy_dev, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	vkAllocateMemory(vk->device, &memAlloc, vk->alloc, &vk->dst_mem);


	vkBindImageMemory(vk->device, vk->dst_image, vk->dst_mem, 0);

	return 0;
}
VkResult magmaVkCreateImageView(magma_vk_renderer_t *vk) {
	VkImageCreateInfo imageInfo = { 0 };
	VkMemoryAllocateInfo memAlloc = { 0 };
	VkMemoryRequirements memReqs = { 0 };

	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
	imageInfo.extent.depth = 1;
	imageInfo.extent.width = vk->width;
	imageInfo.extent.height = vk->height;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.arrayLayers = 1;
	imageInfo.mipLevels = 1;
	imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	vkCreateImage(vk->device, &imageInfo, vk->alloc, &vk->vk_image);

	vkGetImageMemoryRequirements(vk->device, vk->vk_image, &memReqs);

	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = getMemoryTypeIndex(vk->phy_dev, memReqs.memoryTypeBits, 0);
	vkAllocateMemory(vk->device, &memAlloc, vk->alloc, &vk->src_mem);
	vkBindImageMemory(vk->device, vk->vk_image, vk->src_mem, 0);

	VkImageViewCreateInfo imageView = { 0 };
	imageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageView.format = VK_FORMAT_B8G8R8A8_SRGB;
	imageView.image = vk->vk_image;
	imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageView.subresourceRange.levelCount = 1;
	imageView.subresourceRange.layerCount = 1;


	return vkCreateImageView(vk->device, &imageView, vk->alloc, &vk->vk_image_view);
}
