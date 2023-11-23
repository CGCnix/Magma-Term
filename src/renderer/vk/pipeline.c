#include "magma/renderer/vk.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>


#include <magma/private/renderer/vk.h>
#include <magma/logger/log.h>

static void *magmaVkReadShaderFile(const char *path, size_t *size) {
	uint8_t *shader_code;
	long length;
	FILE *fp = fopen(path, "r");
	
	fseek(fp, 0, SEEK_END);
	length = ftell(fp);
	/* We could also mmap these files if we wanted to 
	 * but this works for now 
	 */
	shader_code = calloc(sizeof(uint8_t), length);
	fseek(fp, 0, SEEK_SET);

	fread(shader_code, sizeof(uint8_t), length, fp);

	fclose(fp);

	*size = length;
	return shader_code;
}

VkResult magmaVkCreateRenderPass(magma_vk_renderer_t *vk) {
	VkAttachmentDescription colorAttachment = { 0 };
	VkAttachmentReference colorAttachmentRef = {0};
	VkSubpassDescription subpass = { 0 };
	VkRenderPassCreateInfo renderPassInfo = { 0 };

	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.format = VK_FORMAT_B8G8R8A8_SRGB;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;

	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.subpassCount = 1;

	return vkCreateRenderPass(vk->device, &renderPassInfo, vk->alloc, &vk->render_pass);
}

VkResult magmaVkCreateShader(magma_vk_renderer_t *vk, const char *path, VkShaderModule *shader) {
	size_t size;
	uint32_t *code;
	VkResult result;
	VkShaderModuleCreateInfo createInfo = {0};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

	result = VK_ERROR_UNKNOWN;

	code = magmaVkReadShaderFile(path, &size);
	
	if(code) {
		createInfo.codeSize = size;
		createInfo.pCode = code;
		result = vkCreateShaderModule(vk->device, &createInfo, vk->alloc, shader);
		free(code);
	}
	

	return result;
}

VkResult magmaVkCreatePipeline(magma_vk_renderer_t *vk) {
	VkShaderModule vertex, fragment;
	VkPipelineShaderStageCreateInfo fragStage = { 0 }, vertStage = { 0 };
	VkPipelineShaderStageCreateInfo stages[2];
	VkPipelineInputAssemblyStateCreateInfo inputAsm = {0};
	VkPipelineVertexInputStateCreateInfo vertInfo = {0};
	VkPipelineLayoutCreateInfo pipelineLayout = {0};
	VkPipelineColorBlendStateCreateInfo colorBlend = {0};
	VkPipelineColorBlendAttachmentState colorBlendAttachment = {0};
	VkPipelineMultisampleStateCreateInfo multisample = {0};
	VkPipelineRasterizationStateCreateInfo rasterizer = {0};
	VkPipelineViewportStateCreateInfo viewportState = {0};
	VkGraphicsPipelineCreateInfo graphicsPipeline = {0};
	VkDynamicState dynamicState[2];
	VkPipelineDynamicStateCreateInfo dynamicInfo = {0};

	magmaVkCreateShader(vk, "shaders/vert.spv", &vertex);
	magmaVkCreateShader(vk, "shaders/frag.spv", &fragment);

	fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragStage.pName = "main";
	fragStage.module = fragment;

	vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertStage.pName = "main";
	vertStage.module = vertex;
	
	stages[0] = vertStage;
	stages[1] = fragStage;

	dynamicState[0] = VK_DYNAMIC_STATE_SCISSOR;
	dynamicState[1] = VK_DYNAMIC_STATE_VIEWPORT;

	dynamicInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicInfo.pDynamicStates = dynamicState;
	dynamicInfo.dynamicStateCount = sizeof(dynamicState) / sizeof(dynamicState[0]);

	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.scissorCount = 1;
	viewportState.viewportCount = 1;

	inputAsm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAsm.primitiveRestartEnable = VK_FALSE;
	inputAsm.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	vertInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;	
	colorBlend.pAttachments = &colorBlendAttachment;
	colorBlend.attachmentCount = 1;

	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	
	multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;


	pipelineLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	vkCreatePipelineLayout(vk->device, &pipelineLayout, vk->alloc, &vk->pipeline_layout);

	graphicsPipeline.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipeline.pStages = stages;
	graphicsPipeline.layout = vk->pipeline_layout;
	graphicsPipeline.pDynamicState = &dynamicInfo;
	graphicsPipeline.stageCount = 2;
	graphicsPipeline.pRasterizationState = &rasterizer;
	graphicsPipeline.pInputAssemblyState = &inputAsm;
	graphicsPipeline.pViewportState = &viewportState;
	graphicsPipeline.pMultisampleState = &multisample;
	graphicsPipeline.pColorBlendState = &colorBlend;
	graphicsPipeline.pVertexInputState = &vertInfo;
	graphicsPipeline.subpass = 0;
	graphicsPipeline.renderPass = vk->render_pass;
	graphicsPipeline.layout = vk->pipeline_layout;

	vkCreateGraphicsPipelines(vk->device, VK_NULL_HANDLE, 1, &graphicsPipeline, vk->alloc, &vk->graphics_pipeline);

	vkDestroyShaderModule(vk->device, vertex, vk->alloc);
	vkDestroyShaderModule(vk->device, fragment, vk->alloc);

	return VK_SUCCESS;
}
