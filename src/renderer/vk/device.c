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


static uint32_t magma_vk_score_device(VkPhysicalDevice device) {
	uint32_t score;
	VkPhysicalDeviceFeatures feats;
	VkPhysicalDeviceProperties props;

	/* These return void and doesn't seem there is a way
	 * To Actually Error check them I would guess they
	 * just grab the address of some static memory
	 */
	vkGetPhysicalDeviceFeatures(device, &feats);
	vkGetPhysicalDeviceProperties(device, &props);
	
	score = 0;

	/* TODO we could try to judge the GPU on more features 
	 * but we are a very simple program and there is no 
	 * sense in judging to much other than perfering the 
	 * Discrete GPU if the system has it.
	 */
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

static bool magma_vk_find_queue_families(VkPhysicalDevice device, struct queue_indicies *indices) {
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
	/*This is more paranoia as vulkan gurantees atleast one
	 * graphics queue to be present on a device but in the off
	 * chance that there is a buggy implementation handle it 
	 * by saying this device bailing and saying the device isn't 
	 * valid
	 */
	if(count == 0) {
		magma_log_warn("Vk Device reported no queue families\n");
		return 0;
	}

	queues = calloc(count, sizeof(VkQueueFamilyProperties));
	if(!queues) {}

	vkGetPhysicalDeviceQueueFamilyProperties(device, &count, queues);

	for(uint32_t i = 0; i < count; i++) {
		magma_log_debug("%d %d\n", queues[i].queueCount, queues[i].queueFlags);
		if(!graphics && queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			graphics = true;
			indices->graphics = i;
		}

		if(!compute && queues[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
			compute = true;
			indices->compute = i;
		}

		if(!transfer && queues[i].queueFlags & VK_QUEUE_TRANSFER_BIT) {
			transfer = true;
			indices->transfer = i;
		}
	}

	free(queues);
	return graphics & compute & transfer;
}

VkResult magma_vk_get_physical_device(magma_vk_renderer_t *renderer) {
	VkResult res;
	uint32_t dev_count, dev, score, best_score = 0;
	VkPhysicalDevice *devices, best_dev = NULL;

	res = vkEnumeratePhysicalDevices(renderer->instance, &dev_count, NULL);
	if(res || !dev_count) {
		magma_log_error("vkEnumeratePhysicalDevices Failed 1st call\n");
		return res;
	}

	devices = calloc(sizeof(VkPhysicalDevice), dev_count);
	if(!devices) {
		magma_log_error("vkEnumeratePhysicalDevices calloc failed %s\n",
				strerror(errno));
		return VK_ERROR_OUT_OF_HOST_MEMORY;
	}

	res = vkEnumeratePhysicalDevices(renderer->instance, &dev_count, devices);
	if(res || !dev_count) {
		free(devices);
		magma_log_error("vkEnumeratePhysicalDevices Failed 2st call\n");
		return res;
	}


	for(dev = 0; dev < dev_count; dev++) {

		score = magma_vk_score_device(devices[dev]);
		
		/*Set Score to 0 if this device doesn't have the queues we want*/
		score *= magma_vk_find_queue_families(devices[dev], &renderer->indicies);

		if(score > best_score) {
			best_score = score;
			best_dev = devices[dev];
		}
	}

	renderer->phy_dev = best_dev;
	
	free(devices);
	return best_dev ? VK_SUCCESS : VK_ERROR_FEATURE_NOT_PRESENT;
}

VkResult magma_vk_create_device(magma_vk_renderer_t *vk) {
	VkResult res;
	VkDeviceQueueCreateInfo queue_info = {0};
	VkPhysicalDeviceFeatures dev_feats = {0};
	VkDeviceCreateInfo device_info = {0};
	float queue_prio = 1.0f;

	queue_info.pQueuePriorities = &queue_prio;
	queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queue_info.queueCount = 1;
	queue_info.queueFamilyIndex = vk->indicies.graphics;

	device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_info.pEnabledFeatures = &dev_feats;
	device_info.pQueueCreateInfos = &queue_info;
	device_info.queueCreateInfoCount = 1;

	res = vkCreateDevice(vk->phy_dev, &device_info, vk->alloc, &vk->device);
	if(res == VK_SUCCESS) {
		vkGetDeviceQueue(vk->device, vk->indicies.graphics, 0, &vk->queue);
	}

	return res;
}
