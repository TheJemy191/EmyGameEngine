#pragma once
#include "Header/GLFWHeader.h"

#include <vector>
#include <memory>
#include <map>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include "VulkanInstance.h"
#include "VulkanPhysicalDevice.h"
#include "VulkanLogicalDevice.h"
#include "VulkanSwapChain.h"
#include "VulkanRenderPass.h"
#include "Rendering/Vulkan/VulkanGraphicPipeline.h"
#include "VulkanShader.h"
#include "Rendering/Texture.h"
#include "Rendering/Mesh.h"
#include "Rendering/Vulkan/VulkanDescriptor.h"
#include "Rendering/GlfwManager.h"
#include "Rendering/Model.h"
#include "Rendering/UI/ImguiBase.h"
#include "Rendering/Renderer.h"

class VulkanRenderer : public Renderer
{
public:
	static const int MAX_FRAMES_IN_FLIGHT = 2;
	glm::vec3 clearColor = glm::vec3(100.0f / 255.0f, 149.0f / 255.0f, 237.0f / 255.0f);

	glm::vec3 camPos = glm::vec3(0, 5.0f, 0.0f);
	glm::vec3 camDir = glm::vec3(0, -1, 0);
	glm::vec3 lightDir = glm::vec3(0.1f, 1, 1);
	glm::vec2 lightSetting = glm::vec2(5, 1);
	glm::vec3 lightColor = glm::vec3(1);

private:
	static VulkanRenderer* instance;

	GLFWwindow* window;

	std::unique_ptr<VulkanInstance> vulkanInstance;
	VkSurfaceKHR surface = nullptr;
	std::unique_ptr<VulkanPhysicalDevice> physicalDevice;
	std::unique_ptr<VulkanLogicalDevice> logicalDevice;
	std::unique_ptr<VulkanSwapChain> swapChain;
	std::unique_ptr<VulkanRenderPass> renderPass;

	//TODO: Change this so we can have multiple and take ref in mesh
	std::unique_ptr<VulkanShader> baseVertexShader;
	std::unique_ptr<VulkanShader> baseFragShader;
	std::unique_ptr<VulkanShader> textureColorFragShader;
	std::unique_ptr <VulkanGraphicPipeline> basicGraphicPipeline;
	std::unique_ptr <VulkanGraphicPipeline> textureColorGraphicPipeline;

	std::map<VulkanGraphicPipeline*, std::map<Mesh*, std::vector<std::unique_ptr<Model>>>> modelList;

	std::unique_ptr<Texture> checkerTexture;
	std::unique_ptr<Texture> skyboxTexture;
	std::unique_ptr<Texture> debugNormalTexture;
	std::unique_ptr<Texture> testNormalTexture;
	std::unique_ptr<Mesh> skyboxMesh;
	std::unique_ptr<Mesh> cubeMesh;
	std::unique_ptr<Mesh> planeMesh;

	std::vector<Mesh*> meshList = std::vector<Mesh*>();
	std::vector<Texture*> textureList = std::vector<Texture*>();

	//TODO: Move this
	std::vector <VkCommandPool> drawCommandPool;
	std::vector<VkCommandBuffer> commandBuffers;

	VkCommandPool globalCommandPool;

	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence> inFlightFences;
	size_t currentFrame = 0;

	std::vector<Model*> modelToBeRemove;

public:
	VulkanRenderer(GLFWwindow* window);
	~VulkanRenderer();

	//void RecreateSwapChain();

	void Present(GlfwManager* window) override;

	Model* BasicLoadModel(std::string meshName, std::string textureName, glm::vec3 position, glm::vec3 rotation, glm::vec3 scale);
	void MarkModelToBeRemove(Model* model);

	void AddModelToList(Model* model);

	VulkanInstance* GetVulkanInstance() const;
	VkSurfaceKHR GetVkSurfaceKHR() const;
	VulkanPhysicalDevice* GetPhysicalDevice() const;
	VulkanLogicalDevice* GetLogicalDevice() const;
	VulkanSwapChain* GetSwapChain() const;
	VulkanRenderPass* GetRenderPass() const;
	VkCommandPool GetGlobalCommandPool() const;

	void WaitForIdle() override;

	static VulkanRenderer* GetInstance();

private:
	void RemoveModelFromList(Model* model);
	void Draw();

	void CreateCommandBuffer();// TODO: Not here?
	void CreateSyncObject();// TODO: Not here?
	void UpdateUniformBuffer(uint32_t currentImage);// TODO: Not here?
};