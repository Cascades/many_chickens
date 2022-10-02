#pragma once
#include <type_traits>
#include <vulkan/vulkan.h>

namespace mc
{
	template <typename VulkanDescriptorType>
	class DescriptorInfo
	{
		VulkanDescriptorType info;

	public:
		DescriptorInfo() = default;

		DescriptorInfo(VkImageView imageView, VkImageLayout imageLayout)
			requires std::is_same<VulkanDescriptorType, VkDescriptorImageInfo>::value
		{
			info.sampler = VK_NULL_HANDLE;
			info.imageView = imageView;
			info.imageLayout = imageLayout;
		}

		DescriptorInfo(VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
		requires std::is_same<VulkanDescriptorType, VkDescriptorImageInfo>::value
		{
			info.sampler = sampler;
			info.imageView = imageView;
			info.imageLayout = imageLayout;
		}

		DescriptorInfo(VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
		requires std::is_same<VulkanDescriptorType, VkDescriptorBufferInfo>::value
		{
			info.buffer = buffer;
			info.offset = offset;
			info.range = range;
		}

		DescriptorInfo(VkBuffer buffer)
		requires std::is_same<VulkanDescriptorType, VkDescriptorBufferInfo>::value
		{
			info.buffer = buffer;
			info.offset = 0;
			info.range = VK_WHOLE_SIZE;
		}

		VulkanDescriptorType get() { return info; }
		VulkanDescriptorType* getPtr() { return &info; }
	};
}