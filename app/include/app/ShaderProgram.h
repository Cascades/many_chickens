#pragma once
#include <vulkan/vulkan.h>

#include "app/Shader.h"

namespace mc
{
	class ShaderProgram
	{
		VkDevice device = nullptr;
		VkPipelineLayout layout = nullptr;
		VkDescriptorSetLayout setLayout = nullptr;
		VkShaderStageFlags pushConstantStages = 0;

	public:
		ShaderProgram() {};

		ShaderProgram(
			VkDevice device,
			mc::Shaders shaders,
			size_t pushConstantSize = 0) :
			device(device)
		{
			for (const auto shader : shaders)
			{
				if (shader->getUsesPushConstants())
				{
					pushConstantStages |= shader->getShaderStage();
				}
			}

			setLayout = createSetLayout(shaders);
			assert(setLayout);

			layout = createPipelineLayout(pushConstantSize);
			assert(layout);
		}

		ShaderProgram& operator=(ShaderProgram&& other)
		{
			std::swap(other.device, this->device);
			std::swap(other.layout, this->layout);
			std::swap(other.setLayout, this->setLayout);
			std::swap(other.pushConstantStages, this->pushConstantStages);
			return *this;
		}

		~ShaderProgram()
		{
			if (layout)
			{
				vkDestroyPipelineLayout(device, layout, nullptr);
			}
			if (setLayout)
			{
				vkDestroyDescriptorSetLayout(device, setLayout, nullptr);
			}
		}

		const VkPipelineLayout& getLayout() const { return layout; }
		const VkDescriptorSetLayout& getSetLayout() const { return setLayout; }
		const VkShaderStageFlags& getPushConstantStages() const { return pushConstantStages; }
	private:
		uint32_t gatherResources(Shaders shaders, VkDescriptorType(&resourceTypes)[32])
		{
			uint32_t resourceMask = 0;

			for (auto const shader : shaders)
			{
				for (uint32_t i = 0; i < 32; ++i)
				{
					if (shader->getResourceMask() & (1 << i))
					{
						if (resourceMask & (1 << i))
						{
							assert(resourceTypes[i] == shader->getResourceTypes()[i]);
						}
						else
						{
							resourceTypes[i] = shader->getResourceTypes()[i];
							resourceMask |= 1 << i;
						}
					}
				}
			}

			return resourceMask;
		}

		VkDescriptorSetLayout createSetLayout(Shaders shaders)
		{
			std::vector<VkDescriptorSetLayoutBinding> setBindings;

			VkDescriptorType resourceTypes[32] = {};
			uint32_t resourceMask = gatherResources(shaders, resourceTypes);

			for (uint32_t i = 0; i < 32; ++i)
			{
				if (resourceMask & (1 << i))
				{
					VkDescriptorSetLayoutBinding binding = {};
					binding.binding = i;
					binding.descriptorType = resourceTypes[i];
					binding.descriptorCount = 1;

					binding.stageFlags = 0;
					for (auto const shader : shaders)
					{
						if (shader->getResourceMask() & (1 << i))
						{
							binding.stageFlags |= shader->getShaderStage();
						}
					}

					setBindings.push_back(binding);
					std::cout << "------------" << std::endl;
					std::cout << binding.binding << std::endl;
					switch (binding.descriptorType)
					{
					case VK_DESCRIPTOR_TYPE_SAMPLER:
						std::cout << "VK_DESCRIPTOR_TYPE_SAMPLER" << std::endl;
						break;
					case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
						std::cout << "VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER" << std::endl;
						break;
					case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
						std::cout << "VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE" << std::endl;
						break;
					case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
						std::cout << "VK_DESCRIPTOR_TYPE_STORAGE_IMAGE" << std::endl;
						break;
					case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
						std::cout << "VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER" << std::endl;
						break;
					case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
						std::cout << "VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER" << std::endl;
						break;
					case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
						std::cout << "VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER" << std::endl;
						break;
					case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
						std::cout << "VK_DESCRIPTOR_TYPE_STORAGE_BUFFER" << std::endl;
						break;
					}
					std::cout << binding.descriptorCount << std::endl;
					switch (binding.stageFlags)
					{
					case VK_SHADER_STAGE_VERTEX_BIT:
						std::cout << "VK_SHADER_STAGE_VERTEX_BIT" << std::endl;
						break;
					case VK_SHADER_STAGE_FRAGMENT_BIT:
						std::cout << "VK_SHADER_STAGE_FRAGMENT_BIT" << std::endl;
						break;
					case VK_SHADER_STAGE_GEOMETRY_BIT:
						std::cout << "VK_SHADER_STAGE_GEOMETRY_BIT" << std::endl;
						break;
					case VK_SHADER_STAGE_COMPUTE_BIT:
						std::cout << "VK_SHADER_STAGE_COMPUTE_BIT" << std::endl;
						break;
					}
				}
			}

			VkDescriptorSetLayoutCreateInfo setCreateInfo = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
			setCreateInfo.flags = 0;
			setCreateInfo.bindingCount = uint32_t(setBindings.size());
			setCreateInfo.pBindings = setBindings.data();

			VkDescriptorSetLayout setLayout = 0;
			if (vkCreateDescriptorSetLayout(device, &setCreateInfo, 0, &setLayout) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create descriptor set layout!");
			}

			return setLayout;
		}

		VkPipelineLayout createPipelineLayout(size_t pushConstantSize)
		{
			VkPipelineLayoutCreateInfo createInfo = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
			createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			createInfo.setLayoutCount = 1;
			createInfo.pSetLayouts = &setLayout;
			createInfo.pushConstantRangeCount = 0;

			VkPushConstantRange pushConstantRange = {};

			if (pushConstantSize)
			{
				pushConstantRange.stageFlags = pushConstantStages;
				pushConstantRange.size = uint32_t(pushConstantSize);

				createInfo.pushConstantRangeCount = 1;
				createInfo.pPushConstantRanges = &pushConstantRange;
			}

			VkPipelineLayout layout = 0;
			if (vkCreatePipelineLayout(device, &createInfo, 0, &layout) != VK_SUCCESS) {
				throw std::runtime_error("failed to create pipeline layout!");
			}

			return layout;
		}
	};
}