/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TNT_FILAMENT_BACKEND_CACHING_VULKANDESCRIPTORSETMANAGER_H
#define TNT_FILAMENT_BACKEND_CACHING_VULKANDESCRIPTORSETMANAGER_H

#include "vulkan/VulkanHandles.h"
#include "vulkan/memory/ResourcePointer.h"
#include "vulkan/utils/Definitions.h"  // For DescriptorSetMask

#include <backend/DriverEnums.h>
#include <backend/Program.h>
#include <backend/TargetBufferInfo.h>

#include <utils/bitset.h>

#include <bluevk/BlueVK.h>
#include <tsl/robin_map.h>

#include <memory>

namespace filament::backend {

// [GDSR]: Great-Descriptor-Set-Refactor: As of 03/20/24, the Filament frontend is planning to
// introduce descriptor set. This PR will arrive before that change is complete. As such, some of
// the methods introduced here will be obsolete, and certain logic will be generalized.

// Abstraction over the pool and the layout cache.
class VulkanDescriptorSetManager {
public:
    static constexpr uint8_t UNIQUE_DESCRIPTOR_SET_COUNT =
            VulkanDescriptorSetLayout::UNIQUE_DESCRIPTOR_SET_COUNT;

    using DescriptorSetLayoutArray = VulkanDescriptorSetLayout::DescriptorSetLayoutArray;

    VulkanDescriptorSetManager(VkDevice device, fvkmemory::ResourceManager* resourceManager);
    ~VulkanDescriptorSetManager();

    void terminate() noexcept;

    void updateBuffer(fvkmemory::resource_ptr<VulkanDescriptorSet> set, uint8_t binding,
            fvkmemory::resource_ptr<VulkanBufferObject> bufferObject, VkDeviceSize offset,
            VkDeviceSize size) noexcept;

    void updateSampler(fvkmemory::resource_ptr<VulkanDescriptorSet> set, uint8_t binding,
            fvkmemory::resource_ptr<VulkanTexture> texture, VkSampler sampler) noexcept;

    void updateInputAttachment(fvkmemory::resource_ptr<VulkanDescriptorSet> set,
            VulkanAttachment const& attachment) noexcept;

    void bind(uint8_t setIndex, fvkmemory::resource_ptr<VulkanDescriptorSet> set,
            backend::DescriptorSetOffsetArray&& offsets);

    void unbind(uint8_t setIndex);

    void commit(VulkanCommandBuffer* commands, VkPipelineLayout pipelineLayout,
            fvkutils::DescriptorSetMask const& setMask);

    fvkmemory::resource_ptr<VulkanDescriptorSet> createSet(Handle<HwDescriptorSet> handle,
            fvkmemory::resource_ptr<VulkanDescriptorSetLayout> layout);

    void initVkLayout(fvkmemory::resource_ptr<VulkanDescriptorSetLayout> layout);

    void clearHistory();

private:
    class DescriptorSetLayoutManager;
    class DescriptorInfinitePool;

    using DescriptorSetArray =
            std::array<fvkmemory::resource_ptr<VulkanDescriptorSet>, UNIQUE_DESCRIPTOR_SET_COUNT>;

    VkDevice mDevice;
    fvkmemory::ResourceManager* mResourceManager;
    std::unique_ptr<DescriptorSetLayoutManager> mLayoutManager;
    std::unique_ptr<DescriptorInfinitePool> mDescriptorPool;
    std::pair<VulkanAttachment, VkDescriptorImageInfo> mInputAttachment;
    DescriptorSetArray mStashedSets = {};

    struct {
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        fvkutils::DescriptorSetMask setMask;
        DescriptorSetArray boundSets = {};
    } mLastBoundInfo;
};

}// namespace filament::backend

#endif// TNT_FILAMENT_BACKEND_CACHING_VULKANDESCRIPTORSETMANAGER_H
