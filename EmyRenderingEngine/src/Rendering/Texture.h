#pragma once
#include "Header/GLFWHeader.h"

#include <string>

class Texture
{
private:
	static const std::string PATH;

	bool hasAlpha = false;
	uint32_t mipLevels = 1;

	VkImage textureImage = nullptr;
	VkImageView textureImageView = nullptr;
	VkDeviceMemory textureImageMemory = nullptr;
	VkSampler textureSampler;

public:
	Texture(std::string name, bool createMipMap = true);
	~Texture();

	bool GetHasAlpha() const;
	uint32_t GetMipLevels() const;

	VkImage GetTextureImage() const;
	VkImageView GetTextureImageView() const;
	VkSampler GetTextureSampler() const;
};