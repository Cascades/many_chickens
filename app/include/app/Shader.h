#pragma once
#include <filesystem>
#include <variant>
#include <vulkan/vulkan.h>
#ifdef __linux__
#include <spirv/unified1/spirv.h>
#elif VK_HEADER_VERSION >= 135
#include <spirv-headers/spirv.h>
#else
#include <vulkan/spirv.h>
#endif
#include "HelperFunctions.h"

// https://www.khronos.org/registry/spir-v/specs/1.0/SPIRV.pdf
struct Id
{
	uint32_t opcode;
	uint32_t typeId;
	uint32_t storageClass;
	uint32_t binding;
	uint32_t set;
	uint32_t constant;
	std::optional<uint32_t> inputAttachment = std::nullopt;
	bool bufferBlock = false;
};

static VkShaderStageFlagBits getShaderStage(SpvExecutionModel executionModel)
{
	switch (executionModel)
	{
	case SpvExecutionModelVertex:
		return VK_SHADER_STAGE_VERTEX_BIT;
	case SpvExecutionModelFragment:
		return VK_SHADER_STAGE_FRAGMENT_BIT;
	case SpvExecutionModelGLCompute:
		return VK_SHADER_STAGE_COMPUTE_BIT;
	case SpvExecutionModelMeshEXT:
		return VK_SHADER_STAGE_MESH_BIT_EXT;

	default:
		assert(!"Unsupported execution model");
		return VkShaderStageFlagBits(0);
	}
}

static VkDescriptorType getDescriptorType(SpvOp op, std::optional<SpvStorageClass> storageClass, std::optional<uint32_t> inputAttachmentIndex, bool bufferBlock)
{
	switch (op)
	{
	case SpvOpTypeStruct:
		if (storageClass.value_or(SpvStorageClassStorageBuffer) == SpvStorageClassStorageBuffer || bufferBlock)
		{
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		}
		else if (storageClass.value() == SpvStorageClassUniform)
		{
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		}
		throw std::runtime_error("unknown storage class");
		break;
	case SpvOpTypeImage:
		if (inputAttachmentIndex.has_value())
		{
			return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		}
		else
		{
			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		}
	case SpvOpTypeSampler:
		return VK_DESCRIPTOR_TYPE_SAMPLER;
	case SpvOpTypeSampledImage:
		return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	default:
		assert(!"Unknown resource type");
		return VkDescriptorType(0);
	}
}

namespace mc
{
	class Shader
	{
        VkDevice const device;

		std::array<VkDescriptorType, 32> resourceTypes;
		uint32_t resourceMask;

		VkShaderStageFlagBits const shaderStage;
        VkShaderModule const shaderModule;

		uint32_t localSizeX;
		uint32_t localSizeY;
		uint32_t localSizeZ;

		bool usesPushConstants;

	public:
		Shader() = delete;

        template<typename pathType>
        Shader(VkDevice& device, pathType const& path, VkShaderStageFlagBits stage)
            requires std::convertible_to<pathType, std::string> :
                device(device),
                shaderStage(stage),
				resourceMask(0),
                shaderModule(createShaderModule(readFile(std::filesystem::path(path))))
        {
			std::cout << path << std::endl;
		}

        ~Shader()
        {
            vkDestroyShaderModule(device, shaderModule, nullptr);
        }

        VkShaderModule get() { return shaderModule; }
        VkShaderStageFlagBits getShaderStage() const { return shaderStage; }
		std::array<VkDescriptorType, 32> getResourceTypes() const { return resourceTypes; }
		uint32_t getResourceMask() const { return resourceMask; }
		bool getUsesPushConstants() const { return usesPushConstants; }

	private:
        // create a VkShaderModule to encapsulate our shaders
        VkShaderModule createShaderModule(const std::vector<char>& code) {

            // create struct to hold shader module info
            VkShaderModuleCreateInfo createInfo{};
            // assign type to struct
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            // assign the size of our shader code
            createInfo.codeSize = code.size();
            // assign the code itself
            createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

            // declare a shader module
            VkShaderModule shaderModule;
            // create a shader module based off of our info struct
            // if it fails
            if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
                // throw an error
                throw std::runtime_error("failed to create shader module!");
            }

			parseShader(reinterpret_cast<const uint32_t*>(code.data()), code.size() / 4);

            //return shader module
            return shaderModule;
        }

		void parseShader(const uint32_t* code, uint32_t codeSize)
		{
			resourceMask = 0;
			assert(code[0] == SpvMagicNumber);

			uint32_t idBound = code[3];

			std::vector<Id> ids(idBound);

			int localSizeIdX = -1;
			int localSizeIdY = -1;
			int localSizeIdZ = -1;

			const uint32_t* insn = code + 5;

			while (insn != code + codeSize)
			{
				uint16_t opcode = uint16_t(insn[0]);
				uint16_t wordCount = uint16_t(insn[0] >> 16);

				switch (opcode)
				{
				case SpvOpEntryPoint:
				{
					assert(wordCount >= 2);
					//stage = getShaderStage(SpvExecutionModel(insn[1]));
				} break;
				case SpvOpExecutionMode:
				{
					assert(wordCount >= 3);
					uint32_t mode = insn[2];

					switch (mode)
					{
					case SpvExecutionModeLocalSize:
						assert(wordCount == 6);
						localSizeX = insn[3];
						localSizeY = insn[4];
						localSizeZ = insn[5];
						break;
					}
				} break;
				case SpvOpExecutionModeId:
				{
					assert(wordCount >= 3);
					uint32_t mode = insn[2];

					switch (mode)
					{
					case SpvExecutionModeLocalSizeId:
						assert(wordCount == 6);
						localSizeIdX = int(insn[3]);
						localSizeIdY = int(insn[4]);
						localSizeIdZ = int(insn[5]);
						break;
					}
				} break;
				case SpvOpDecorate:
				{
					assert(wordCount >= 3);

					uint32_t id = insn[1];
					assert(id < idBound);

					switch (insn[2])
					{
					case SpvDecorationDescriptorSet:
						assert(wordCount == 4);
						ids[id].set = insn[3];
						break;
					case SpvDecorationBinding:
						assert(wordCount == 4);
						ids[id].binding = insn[3];
						break;
					case SpvDecorationInputAttachmentIndex:
						assert(wordCount == 4);
						ids[id].inputAttachment = insn[3];
						break;
					case SpvDecorationBufferBlock:
						assert(wordCount == 3);
						ids[id].bufferBlock = true;
						break;
					}
				} break;
				case SpvOpTypeStruct:
				case SpvOpTypeImage:
				case SpvOpTypeSampler:
				case SpvOpTypeSampledImage:
				{
					assert(wordCount >= 2);

					uint32_t id = insn[1];
					assert(id < idBound);

					assert(ids[id].opcode == 0);
					ids[id].opcode = opcode;
				} break;
				case SpvOpTypePointer:
				{
					assert(wordCount == 4);

					uint32_t id = insn[1];
					assert(id < idBound);

					assert(ids[id].opcode == 0);
					ids[id].opcode = opcode;
					ids[id].typeId = insn[3];
					ids[id].storageClass = insn[2];
				} break;
				case SpvOpConstant:
				{
					assert(wordCount >= 4); // we currently only correctly handle 32-bit integer constants

					uint32_t id = insn[2];
					assert(id < idBound);

					assert(ids[id].opcode == 0);
					ids[id].opcode = opcode;
					ids[id].typeId = insn[1];
					ids[id].constant = insn[3]; // note: this is the value, not the id of the constant
				} break;
				case SpvOpVariable:
				{
					assert(wordCount >= 4);

					uint32_t id = insn[2];
					assert(id < idBound);

					assert(ids[id].opcode == 0);
					ids[id].opcode = opcode;
					ids[id].typeId = insn[1];
					ids[id].storageClass = insn[3];
				} break;
				}

				assert(insn + wordCount <= code + codeSize);
				insn += wordCount;
			}

			for (auto& id : ids)
			{
				if (id.opcode == SpvOpVariable && (id.storageClass == SpvStorageClassUniform || id.storageClass == SpvStorageClassUniformConstant || id.storageClass == SpvStorageClassStorageBuffer))
				{
					assert(id.set == 0);
					assert(id.binding < 32);
					assert(ids[id.typeId].opcode == SpvOpTypePointer);

					uint32_t typeKind = ids[ids[id.typeId].typeId].opcode;
					std::cout << "=========" << std::endl;
					std::cout << typeKind << std::endl;
					std::cout << id.opcode << std::endl;
					std::cout << id.typeId << std::endl;
					std::cout << id.storageClass << std::endl;
					std::cout << id.binding << std::endl;
					std::cout << id.set << std::endl;
					std::cout << id.constant << std::endl;
					std::cout << id.bufferBlock << std::endl;
					std::cout << ids[id.typeId].bufferBlock << std::endl;
					std::cout << ids[ids[id.typeId].typeId].bufferBlock << std::endl;
					VkDescriptorType resourceType = getDescriptorType(SpvOp(typeKind), SpvStorageClass(id.storageClass), id.inputAttachment, ids[ids[id.typeId].typeId].bufferBlock);

					//assert((resourceMask & (1 << id.binding)) == 0 || resourceTypes[id.binding] == resourceType);

					resourceTypes[id.binding] = resourceType;
					std::cout << "Binding: " << id.binding << " | ";
					resourceMask |= 1 << id.binding;
					switch (resourceType)
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
				}

				if (id.opcode == SpvOpVariable && id.storageClass == SpvStorageClassPushConstant)
				{
					usesPushConstants = true;
				}
			}

			if (shaderStage == VK_SHADER_STAGE_COMPUTE_BIT)
			{
				if (localSizeIdX >= 0)
				{
					assert(ids[localSizeIdX].opcode == SpvOpConstant);
					localSizeX = ids[localSizeIdX].constant;
				}

				if (localSizeIdY >= 0)
				{
					assert(ids[localSizeIdY].opcode == SpvOpConstant);
					localSizeY = ids[localSizeIdY].constant;
				}

				if (localSizeIdZ >= 0)
				{
					assert(ids[localSizeIdZ].opcode == SpvOpConstant);
					localSizeZ = ids[localSizeIdZ].constant;
				}

				assert(localSizeX && localSizeY && localSizeZ);
			}
		}
	};

    using Shaders = std::initializer_list<std::shared_ptr<Shader const> const>;
}