#include "VulkanRenderer.h"

#include "Helper/Log.h"

#include "Rendering/Vulkan/VulkanHelper.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>

#include <chrono>
#include <algorithm>
#include <Rendering\Vulkan\ImguiVulkan.h>

VulkanRenderer* VulkanRenderer::instance = nullptr;

VulkanRenderer::VulkanRenderer(GLFWwindow* window) : Renderer(window)
{
	if (instance != nullptr)
		Logger::Log(LogSeverity::FATAL_ERROR, "There cant be 2 VulkanRenderer");

	instance = this;
	this->window = window;

	Logger::Log("Start creating vulkan state");

	vulkanInstance = std::unique_ptr<VulkanInstance>(new VulkanInstance(VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT));

	Logger::Log("Creating surface");
	if (glfwCreateWindowSurface(vulkanInstance->GetVk(), window, nullptr, &surface) != VK_SUCCESS)
	{
		Logger::Log(LogSeverity::FATAL_ERROR, "failed to create window surface!");
	}

	physicalDevice = std::unique_ptr<VulkanPhysicalDevice>(new VulkanPhysicalDevice(VkSampleCountFlagBits::VK_SAMPLE_COUNT_8_BIT));
	logicalDevice = std::unique_ptr<VulkanLogicalDevice>(new VulkanLogicalDevice());

	Logger::Log("Creating globalCommandPool");
	VulkanHelper::QueueFamilyIndices queueFamilyIndices = VulkanHelper::FindQueueFamilies();
	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
	if (vkCreateCommandPool(logicalDevice->GetVk(), &poolInfo, nullptr, &globalCommandPool) != VK_SUCCESS)
	{
		Logger::Log(LogSeverity::FATAL_ERROR, "failed to create graphics command pool!");
	}

	renderPass = std::unique_ptr<VulkanRenderPass>(new VulkanRenderPass());
	swapChain = std::unique_ptr<VulkanSwapChain>(new VulkanSwapChain(window));

	Logger::Log("Creating drawCommandPools");
	drawCommandPool.resize(swapChain->GetSwapChainFramebuffers().size());
	for (size_t i = 0; i < swapChain->GetSwapChainFramebuffers().size(); i++)
	{
		if (vkCreateCommandPool(logicalDevice->GetVk(), &poolInfo, nullptr, &drawCommandPool[i]) != VK_SUCCESS)
		{
			Logger::Log(LogSeverity::FATAL_ERROR, "failed to create graphics command pool!");
		}
	}

	imgui = std::unique_ptr<ImguiVulkan>(new ImguiVulkan(window, queueFamilyIndices.graphicsFamily.value()));

	Logger::Log("Creating test shader");
	// TestMesh and shader
	baseVertexShader = std::unique_ptr<VulkanShader>(new VulkanShader("BaseVert", VkShaderStageFlagBits::VK_SHADER_STAGE_VERTEX_BIT));
	baseFragShader = std::unique_ptr<VulkanShader>(new VulkanShader("BaseFrag", VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT));
	textureColorFragShader = std::unique_ptr<VulkanShader>(new VulkanShader("TextureColorFrag", VkShaderStageFlagBits::VK_SHADER_STAGE_FRAGMENT_BIT));

	Logger::Log("Creating test GraphicPipeline");
	basicGraphicPipeline = std::unique_ptr <VulkanGraphicPipeline>(new VulkanGraphicPipeline());
	basicGraphicPipeline->AddShader(baseVertexShader.get());
	basicGraphicPipeline->AddShader(baseFragShader.get());
	basicGraphicPipeline->Create(VkPolygonMode::VK_POLYGON_MODE_FILL);

	textureColorGraphicPipeline = std::unique_ptr <VulkanGraphicPipeline>(new VulkanGraphicPipeline());
	textureColorGraphicPipeline->AddShader(baseVertexShader.get());
	textureColorGraphicPipeline->AddShader(textureColorFragShader.get());
	textureColorGraphicPipeline->Create(VkPolygonMode::VK_POLYGON_MODE_FILL);


	Logger::Log("Creating test skybox");
	skyboxTexture = std::unique_ptr<Texture>(new Texture("Debug.jpg"));
	debugNormalTexture = std::unique_ptr<Texture>(new Texture("DebugNormalMap.jpg"));
	skyboxMesh = std::unique_ptr<Mesh>(new Mesh("SkyBoxTest.obj", Mesh::MeshFormat::OBJ));
	Model* testModel = new Model(skyboxMesh.get(), skyboxTexture.get(), debugNormalTexture.get(), textureColorGraphicPipeline.get());
	testModel->position = glm::vec3(0);
	AddModelToList(testModel);


	Logger::Log("Creating Command buffer");
	CreateCommandBuffer();

	Logger::Log("Creating sync object");
	CreateSyncObject();

	Logger::Log("Vulkan created");
}

VulkanRenderer::~VulkanRenderer()
{
	skyboxMesh.reset();
	swapChain.reset();
	modelList.clear();
	vkDestroySurfaceKHR(vulkanInstance->GetVk(), surface, nullptr);
	baseVertexShader.reset();
	baseFragShader.reset();

	for (size_t i = 0; i < commandBuffers.size(); i++)
	{
		vkFreeCommandBuffers(logicalDevice->GetVk(), drawCommandPool[i], 1 , &commandBuffers[i]);
	}

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		vkDestroySemaphore(logicalDevice->GetVk(), renderFinishedSemaphores[i], nullptr);
		vkDestroySemaphore(logicalDevice->GetVk(), imageAvailableSemaphores[i], nullptr);
		vkDestroyFence(logicalDevice->GetVk(), inFlightFences[i], nullptr);
	}

	vkDestroyCommandPool(logicalDevice->GetVk(), globalCommandPool, nullptr);

	for (size_t i = 0; i < drawCommandPool.size(); i++)
	{
		vkDestroyCommandPool(logicalDevice->GetVk(), drawCommandPool[i], nullptr);
	}

	for (size_t i = 0; i < meshList.size(); i++)
	{
		delete meshList[i];
	}
	meshList.clear();
	for (size_t i = 0; i < textureList.size(); i++)
	{
		delete textureList[i];
	}
	textureList.clear();

	Logger::Log("Vulkan destroyed");
}

/*void VulkanRenderer::RecreateSwapChain()
{
	int width = 0, height = 0;
	while (width == 0 || height == 0)
	{
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	vkDeviceWaitIdle(logicalDevice->GetVk());

	renderPass.reset(new VulkanRenderPass(logicalDevice->GetVk(), physicalDevice->GetVk(), surface, physicalDevice->GetMsaaSample()));
	swapChain.reset(new VulkanSwapChain(window, logicalDevice->GetVk(), physicalDevice->GetVk(), surface, physicalDevice->GetMsaaSample(), globalCommandPool, logicalDevice->GetGraphicsQueue(), renderPass->GetVk()));

	basicGraphicPipeline.reset(new VulkanGraphicPipeline());

	basicGraphicPipeline->AddShader(baseVertexShader.get());
	basicGraphicPipeline->AddShader(baseFragShader.get());

	basicGraphicPipeline->Create(logicalDevice->GetVk(), swapChain->GetVkExtent2D(), renderPass->GetVk(), physicalDevice->GetMsaaSample(), VkPolygonMode::VK_POLYGON_MODE_FILL);

	//TODO: make a function for that
	// Recreate stuff in model
	auto graphicPipelineIterator = modelList.begin();

	while (graphicPipelineIterator != modelList.end())
	{
		auto meshIterator = graphicPipelineIterator->second.begin();

		while (meshIterator != graphicPipelineIterator->second.end())
		{
			auto modelIterator = meshIterator->second.begin();

			while (modelIterator != meshIterator->second.end())
			{
				modelIterator->get()->Recreate(swapChain->GetVkImages().size());

				modelIterator++;
			}

			meshIterator++;
		}

		graphicPipelineIterator++;
	}

	for (size_t i = 0; i < commandBuffers.size(); i++)
	{
		vkFreeCommandBuffers(logicalDevice->GetVk(), drawCommandPool[i], 1, &commandBuffers[i]);
	}
	CreateCommandBuffer();
}*/

void VulkanRenderer::Draw()
{
	for (size_t i = 0; i < commandBuffers.size(); i++)
	{
		vkResetCommandPool(logicalDevice->GetVk(), drawCommandPool[i], 0);

		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
		{
			Logger::Log(LogSeverity::FATAL_ERROR, "failed to begin recording command buffer!");
		}

		VkRenderPassBeginInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = renderPass->GetVk();
		renderPassInfo.framebuffer = swapChain->GetSwapChainFramebuffers()[i];
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = swapChain->GetVkExtent2D();

		std::array<VkClearValue, 2> clearValues = {};
		clearValues[0].color = {clearColor.r, clearColor.g, clearColor.b, 1.0f};
		clearValues[1].depthStencil = {1.0f, 0};

		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		//TODO: make a function for that
		// Draw model
		auto graphicPipelineIterator = modelList.begin();

		while (graphicPipelineIterator != modelList.end())
		{
			auto meshIterator = graphicPipelineIterator->second.begin();

			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicPipelineIterator->first->GetVkPipeline());

			while (meshIterator != graphicPipelineIterator->second.end())
			{
				auto modelIterator = meshIterator->second.begin();

				meshIterator->first->CmdBind(commandBuffers[i]);

				while (modelIterator != meshIterator->second.end())
				{
					modelIterator->get()->Draw(commandBuffers[i], i);

					modelIterator++;
				}

				meshIterator++;
			}

			graphicPipelineIterator++;
		}

		dynamic_cast<ImguiVulkan*>(imgui.get())->Draw(commandBuffers[i]);

		vkCmdEndRenderPass(commandBuffers[i]);

		if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
		{
			Logger::Log(LogSeverity::FATAL_ERROR, "failed to record command buffer!");
		}
	}
}

void VulkanRenderer::Present(GlfwManager* window)
{
	// Remove model from model list
	if (modelToBeRemove.size() != 0)
	{
		for (size_t i = 0; i < modelToBeRemove.size(); i++)
		{
			RemoveModelFromList(modelToBeRemove[i]);
		}
		modelToBeRemove.clear();
	}

	vkWaitForFences(logicalDevice->GetVk(), 1, &inFlightFences[currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());

	uint32_t imageIndex;
	VkResult result = vkAcquireNextImageKHR(logicalDevice->GetVk(), swapChain->GetVkSwapchainKHR(), std::numeric_limits<uint64_t>::max(), imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		//RecreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		Logger::Log(LogSeverity::FATAL_ERROR, "failed to acquire swap chain image!");
	}

	UpdateUniformBuffer(imageIndex);

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
	VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

	VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	vkResetFences(logicalDevice->GetVk(), 1, &inFlightFences[currentFrame]);
	
	Draw();
	if (vkQueueSubmit(logicalDevice->GetGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
	{
		Logger::Log(LogSeverity::FATAL_ERROR, "failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = {swapChain->GetVkSwapchainKHR()};
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;

	presentInfo.pImageIndices = &imageIndex;

	result = vkQueuePresentKHR(logicalDevice->GetPresentQueue(), &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window->HasWindowResize())
	{
		window->ResetWindowHasResize();
		//RecreateSwapChain();
	}
	else if (result != VK_SUCCESS)
	{
		Logger::Log(LogSeverity::FATAL_ERROR, "failed to present swap chain image!");
	}

	currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;


	vkQueueWaitIdle(logicalDevice->GetPresentQueue());
}

Model* VulkanRenderer::BasicLoadModel(std::string meshName, std::string textureName, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale)
{
	Mesh* newMesh = new Mesh(meshName + ".obj", Mesh::MeshFormat::OBJ);
	meshList.push_back(newMesh);

	Texture* newTexture = new Texture(textureName);
	textureList.push_back(newTexture);

	Model* testModel = new Model(newMesh, newTexture, debugNormalTexture.get(), basicGraphicPipeline.get());

	testModel->position = position;
	testModel->rotation = rotation;
	testModel->scale = scale;
	testModel->meshName = meshName;
	testModel->textureName = textureName;

	AddModelToList(testModel);

	return testModel;
}

void VulkanRenderer::MarkModelToBeRemove(Model* model)
{
	modelToBeRemove.push_back(model);
}

void VulkanRenderer::AddModelToList(Model* model)
{
	using namespace std;
	if (modelList.count(model->graphicPipeline) == 0)
	{
		modelList.insert(pair<VulkanGraphicPipeline*, map<Mesh*, vector<unique_ptr<Model>>>>(
			model->graphicPipeline, map<Mesh*, vector<unique_ptr<Model>>>()
			));
	}

	if (modelList[model->graphicPipeline].count(model->mesh) == 0)
	{
		modelList[model->graphicPipeline].insert(pair<Mesh*, vector<unique_ptr<Model>>>(
			model->mesh, vector<unique_ptr<Model>>()
			));
	}

	modelList[model->graphicPipeline][model->mesh].push_back(unique_ptr<Model>(model));
}

void VulkanRenderer::RemoveModelFromList(Model* model)
{
	for (size_t i = 0; i < modelList[model->graphicPipeline][model->mesh].size(); i++)
	{
		if (modelList[model->graphicPipeline][model->mesh][i].get() == model)
		{
			modelList[model->graphicPipeline][model->mesh].erase(modelList[model->graphicPipeline][model->mesh].begin() + i);
			break;
		}
	}

	if (modelList[model->graphicPipeline][model->mesh].size() <= 0)
		modelList[model->graphicPipeline].erase(model->mesh);

	if (modelList[model->graphicPipeline].size() <= 0)
		modelList.erase(model->graphicPipeline);
}

VulkanInstance* VulkanRenderer::GetVulkanInstance() const
{
	return vulkanInstance.get();
}

VkSurfaceKHR VulkanRenderer::GetVkSurfaceKHR() const
{
	return surface;
}

VulkanPhysicalDevice* VulkanRenderer::GetPhysicalDevice() const
{
	return physicalDevice.get();
}

VulkanLogicalDevice* VulkanRenderer::GetLogicalDevice() const
{
	return logicalDevice.get();
}

VulkanSwapChain* VulkanRenderer::GetSwapChain() const
{
	return swapChain.get();
}

VulkanRenderPass* VulkanRenderer::GetRenderPass() const
{
	return renderPass.get();
}

VkCommandPool VulkanRenderer::GetGlobalCommandPool() const
{
	return globalCommandPool;
}

void VulkanRenderer::WaitForIdle()
{
	vkDeviceWaitIdle(logicalDevice->GetVk());
}

VulkanRenderer* VulkanRenderer::GetInstance()
{
	return instance;
}

void VulkanRenderer::CreateCommandBuffer()
{
	commandBuffers.resize(swapChain->GetSwapChainFramebuffers().size());

	for (size_t i = 0; i < commandBuffers.size(); i++)
	{
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = drawCommandPool[i];
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(logicalDevice->GetVk(), &allocInfo, &commandBuffers[i]) != VK_SUCCESS)
		{
			Logger::Log(LogSeverity::FATAL_ERROR, "failed to allocate command buffers!");
		}
	}
}

void VulkanRenderer::CreateSyncObject()
{
	imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

	VkSemaphoreCreateInfo semaphoreInfo = {};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo = {};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		if (vkCreateSemaphore(logicalDevice->GetVk(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			vkCreateSemaphore(logicalDevice->GetVk(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			vkCreateFence(logicalDevice->GetVk(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
		{
			Logger::Log(LogSeverity::FATAL_ERROR, "failed to create synchronization objects for a frame!");
		}
	}
}

void VulkanRenderer::UpdateUniformBuffer(uint32_t currentImage)
{
	static auto startTime = std::chrono::high_resolution_clock::now();

	auto currentTime = std::chrono::high_resolution_clock::now();
	float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

	VulkanHelper::UniformBufferObject ubo = {};
	ubo.viewPos = camPos;
	ubo.lightDir = lightDir;
	ubo.lightColor = lightColor;
	ubo.lightSetting = lightSetting;
	ubo.proj = glm::perspective(glm::radians(45.0f), swapChain->GetVkExtent2D().width / (float)swapChain->GetVkExtent2D().height, 0.1f, 10000.0f);
	ubo.view = glm::lookAt(camPos, camPos + glm::normalize(camDir), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj[1][1] *= -1;

	//TODO: make a function for that
	// Recreate stuff in model
	auto graphicPipelineIterator = modelList.begin();

	while (graphicPipelineIterator != modelList.end())
	{
		auto meshIterator = graphicPipelineIterator->second.begin();

		while (meshIterator != graphicPipelineIterator->second.end())
		{
			auto modelIterator = meshIterator->second.begin();

			while (modelIterator != meshIterator->second.end())
			{
				ubo.model = glm::translate(glm::mat4(1.0), modelIterator->get()->position);
				ubo.model *= glm::mat4_cast(glm::quat(glm::radians(modelIterator->get()->rotation)));
				ubo.model = glm::scale(ubo.model, modelIterator->get()->scale);

				modelIterator->get()->UpdateUniformBuffer(currentImage, &ubo);

				modelIterator++;
			}

			meshIterator++;
		}

		graphicPipelineIterator++;
	}
}
