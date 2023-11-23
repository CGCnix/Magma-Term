#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#include <magma/renderer/vk.h>
#include <magma/backend/backend.h>
#include <magma/logger/log.h>
#include <magma/private/renderer/vk.h>

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#ifdef MAGMA_VK_DEBUG
VkAllocationCallbacks *magma_vk_allocator(void);
void magma_vk_allocator_print_totals(void);
#endif

VkResult magmaVkCreateImageView(magma_vk_renderer_t *vk);
VkResult magmaVkCreateRenderPass(magma_vk_renderer_t *vk);
VkResult magmaVkCreatePipeline(magma_vk_renderer_t *vk);
VkResult magmaVkCreateDstImageView(magma_vk_renderer_t *vk);
VkResult magmaVkCreateCommandPool(magma_vk_renderer_t *vk);

void insertImageMemoryBarrier(
	VkCommandBuffer cmdbuffer,
	VkImage image,
	VkAccessFlags srcAccessMask,
	VkAccessFlags dstAccessMask,
	VkImageLayout oldImageLayout,
	VkImageLayout newImageLayout,
	VkPipelineStageFlags srcStageMask,
	VkPipelineStageFlags dstStageMask,
	VkImageSubresourceRange subresourceRange)
{
	VkImageMemoryBarrier imageMemoryBarrier = {0};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.srcAccessMask = srcAccessMask;
	imageMemoryBarrier.dstAccessMask = dstAccessMask;
	imageMemoryBarrier.oldLayout = oldImageLayout;
	imageMemoryBarrier.newLayout = newImageLayout;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange = subresourceRange;

	vkCmdPipelineBarrier(
		cmdbuffer,
		srcStageMask,
		dstStageMask,
		0,
		0, NULL,
		0, NULL,
		1, &imageMemoryBarrier);
}

magma_buf_t *magma_vk_draw(magma_vk_renderer_t *vk) {
	static magma_buf_t buf;
	VkCommandBufferBeginInfo beginInfo = { 0 };
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(vk->draw_buffer, &beginInfo);

	VkRenderPassBeginInfo renderPassInfo = {0};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = vk->render_pass;
	renderPassInfo.framebuffer = vk->vkfb;
	renderPassInfo.renderArea.offset.x = 0;
	renderPassInfo.renderArea.offset.y = 0;
	renderPassInfo.renderArea.extent.height = vk->height;
	renderPassInfo.renderArea.extent.width = vk->width;
	
	VkClearValue clearColor = {{{.2f, 0.3f, 0.3f, .8f}}};
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues = &clearColor;

	vkCmdBeginRenderPass(vk->draw_buffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(vk->draw_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->graphics_pipeline);

		VkViewport viewport = {0};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float) vk->width;
		viewport.height = (float) vk->height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(vk->draw_buffer, 0, 1, &viewport);

		VkRect2D scissor = {0};
		scissor.offset.y = 0;
		scissor.offset.x = 0;
		scissor.extent.height = vk->height;
		scissor.extent.width = vk->width;
		vkCmdSetScissor(vk->draw_buffer, 0, 1, &scissor);


		vkCmdDraw(vk->draw_buffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(vk->draw_buffer);

	vkEndCommandBuffer(vk->draw_buffer);

	VkSubmitInfo submitInfo = {0};
	VkFenceCreateInfo fenceInfo = {0};
	VkFence fence;

	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &vk->draw_buffer;
	vkCreateFence(vk->device, &fenceInfo, vk->alloc, &fence);
	vkQueueSubmit(vk->queue, 1, &submitInfo, fence);
	vkWaitForFences(vk->device, 1, &fence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(vk->device, fence, vk->alloc);

	vkQueueWaitIdle(vk->queue);
	vkDeviceWaitIdle(vk->device);


	vkResetCommandBuffer(vk->draw_buffer, 0);	
	vkBeginCommandBuffer(vk->draw_buffer, &beginInfo);

	VkImageSubresourceRange ResRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

	insertImageMemoryBarrier(
		vk->draw_buffer,
		vk->dst_image,
		0,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		ResRange
		);



	VkImageCopy imageCopyRegion = {0};
	imageCopyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageCopyRegion.srcSubresource.layerCount = 1;
	imageCopyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageCopyRegion.dstSubresource.layerCount = 1;
	imageCopyRegion.extent.width = vk->width;
	imageCopyRegion.extent.height = vk->height;
	imageCopyRegion.extent.depth = 1;

	vkCmdCopyImage(vk->draw_buffer, vk->vk_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 
			vk->dst_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopyRegion);

	insertImageMemoryBarrier(
		vk->draw_buffer,
		vk->dst_image,
		VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_ACCESS_MEMORY_READ_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		ResRange);


	vkEndCommandBuffer(vk->draw_buffer);

	vkCreateFence(vk->device, &fenceInfo, vk->alloc, &fence);
	vkQueueSubmit(vk->queue, 1, &submitInfo, fence);
	vkWaitForFences(vk->device, 1, &fence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(vk->device, fence, vk->alloc);

	vkQueueWaitIdle(vk->queue);
	vkDeviceWaitIdle(vk->device);
	
	VkImageSubresource sub_resource = {0};

	sub_resource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	VkSubresourceLayout sub_resource_layout;

	void *image_data;
	
	vkGetImageSubresourceLayout(vk->device, vk->dst_image, &sub_resource, &sub_resource_layout);
	vkMapMemory(vk->device, vk->dst_mem, 0, sub_resource_layout.size, 0, &image_data);
	
	uint8_t *buffer = calloc(1, sub_resource_layout.size);
	for(uint32_t i = 0; i * sub_resource_layout.rowPitch < sub_resource_layout.size; i++) {
		memcpy(&buffer[i * vk->width*4], &((char*)image_data)[i * sub_resource_layout.rowPitch], vk->width*4);
	}

	buf.height = vk->height;
	buf.width = vk->width;
	buf.pitch = vk->width * 4;
	buf.bpp = 32;
	buf.size = sub_resource_layout.size;
	buf.buffer = buffer;

	vkUnmapMemory(vk->device, vk->dst_mem);
	return &buf;
}

void magma_vk_create_framebuffer(magma_vk_renderer_t *vk) {

	VkFramebufferCreateInfo framebufferInfo = {0};
	framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	framebufferInfo.renderPass = vk->render_pass;
	framebufferInfo.attachmentCount = 1;
	framebufferInfo.pAttachments = &vk->vk_image_view;
	framebufferInfo.width = vk->width;
	framebufferInfo.height = vk->height;
	framebufferInfo.layers = 1;
	

	vkCreateFramebuffer(vk->device, &framebufferInfo, vk->alloc, &vk->vkfb);
}

void magma_vk_handle_resize(magma_vk_renderer_t *vk, uint32_t width, uint32_t height) {
	vk->height = height;
	vk->width = width;



	vkDestroyFramebuffer(vk->device, vk->vkfb, vk->alloc);

	vkFreeMemory(vk->device, vk->dst_mem, vk->alloc);

	vkFreeMemory(vk->device, vk->src_mem, vk->alloc);

	vkDestroyImage(vk->device, vk->dst_image, vk->alloc);

	vkDestroyImageView(vk->device, vk->vk_image_view, vk->alloc);

	vkDestroyImage(vk->device, vk->vk_image, vk->alloc);


	magmaVkCreateImageView(vk);
	magmaVkCreateDstImageView(vk);
	magma_vk_create_framebuffer(vk);

}

magma_vk_renderer_t *magma_vk_renderer_init(magma_backend_t *backend) {
	VkResult res;
	magma_vk_renderer_t *vk = calloc(1, sizeof(*vk));
	if(!vk) {
		magma_log_error("calloc: Failed to allocate Vulkan renderer struct\n");
		goto error_alloc_vk_struct;
	}
	vk->width = 600;
	vk->height = 600;

#ifdef MAGMA_VK_DEBUG
	vk->alloc = magma_vk_allocator();
#else
	vk->alloc = NULL;
#endif /* ifdef MAGMA_VK_DEBUG */

	res = magma_vk_create_instance(backend, vk->alloc, &vk->instance);
	if(res) {
		magma_log_error("Failed to create vulkan interface %d\n", res);
		goto error_vk_create_instance;
	}

#ifdef MAGMA_VK_DEBUG
	res = magma_vk_create_debug_messenger(vk->instance, vk->alloc, &vk->debug_messenger);
	if(res) {
		magma_log_error("Failed to create debug messenger\n");
		goto error_vk_get_debug_msg;
	}
#endif

	res = magma_vk_get_physical_device(vk);
	if(res) {
		magma_log_error("Failed to create phsyical device\n");
		goto error_vk_get_phsyical_dev;
	}

	res = magma_vk_create_device(vk);
	if(res) {
		magma_log_error("Failed to create logical device\n");
		goto error_vk_create_device;
	}

	/*TODO: CHECK*/
	magmaVkCreateRenderPass(vk);
	magmaVkCreatePipeline(vk);
	magmaVkCreateImageView(vk);
	magmaVkCreateDstImageView(vk);
	magmaVkCreateCommandPool(vk);
	magma_vk_create_framebuffer(vk);

	return vk;
error_vk_create_device:

error_vk_get_phsyical_dev:
#ifdef MAGMA_VK_DEBUG
	vkDestroyDebugUtilsMessengerEXT(vk->instance, vk->debug_messenger, NULL);
error_vk_get_debug_msg:
#endif
	vkDestroyInstance(vk->instance, NULL);
error_vk_create_instance:
	free(vk);
error_alloc_vk_struct:
	return NULL;
}

void magma_vk_renderer_deinit(magma_vk_renderer_t *vk) {

	magma_log_warn("VK DEINIT\n");

	vkDestroyRenderPass(vk->device, vk->render_pass, vk->alloc);

	vkDestroyPipelineLayout(vk->device, vk->pipeline_layout, vk->alloc);

	vkDestroyPipeline(vk->device, vk->graphics_pipeline, vk->alloc);

	vkDestroyFramebuffer(vk->device, vk->vkfb, vk->alloc);

	vkFreeMemory(vk->device, vk->dst_mem, vk->alloc);

	vkFreeMemory(vk->device, vk->src_mem, vk->alloc);

	vkDestroyImage(vk->device, vk->dst_image, vk->alloc);

	vkDestroyImageView(vk->device, vk->vk_image_view, vk->alloc);

	vkDestroyImage(vk->device, vk->vk_image, vk->alloc);

	vkFreeCommandBuffers(vk->device, vk->command_pool, 1, &vk->draw_buffer);

	vkDestroyCommandPool(vk->device, vk->command_pool, vk->alloc);

	vkDestroyDevice(vk->device, vk->alloc);

#ifdef MAGMA_VK_DEBUG
	vkDestroyDebugUtilsMessengerEXT(vk->instance, vk->debug_messenger, vk->alloc);
#endif
	vkDestroyInstance(vk->instance, vk->alloc);

#ifdef MAGMA_VK_DEBUG
	magma_vk_allocator_print_totals();
#endif /* ifdef MACRO */

	free(vk);
}
