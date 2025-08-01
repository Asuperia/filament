/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef TNT_FILAMENT_BACKEND_VULKANCONTEXT_H
#define TNT_FILAMENT_BACKEND_VULKANCONTEXT_H

#include "vulkan/utils/Image.h"
#include "vulkan/utils/Definitions.h"

#include "vulkan/memory/ResourcePointer.h"

#include <utils/bitset.h>
#include <utils/FixedCapacityVector.h>
#include <utils/Mutex.h>
#include <utils/Slice.h>

#include <bluevk/BlueVK.h>

VK_DEFINE_HANDLE(VmaAllocator)
VK_DEFINE_HANDLE(VmaPool)

namespace filament::backend {

struct VulkanCommandBuffer;
struct VulkanRenderTarget;
struct VulkanSwapChain;
struct VulkanTexture;
class VulkanStagePool;
struct VulkanTimerQuery;

struct VulkanAttachment {
    fvkmemory::resource_ptr<VulkanTexture> texture;
    uint8_t level = 0;
    uint8_t layerCount = 1;
    uint8_t layer = 0;

    bool isDepth() const;
    VkImage getImage() const;
    VkFormat getFormat() const;
    VulkanLayout getLayout() const;
    VkExtent2D getExtent2D() const;
    VkImageView getImageView();
    // TODO: maybe embed aspect into the attachment or texture itself.
    VkImageSubresourceRange getSubresourceRange() const;
};

struct VulkanRenderPass {
    // Between the begin and end command render pass we cache the command buffer
    VulkanCommandBuffer* commandBuffer;
    fvkmemory::resource_ptr<VulkanRenderTarget> renderTarget;
    VkRenderPass renderPass;
    RenderPassParams params;
    int currentSubpass;
};

// This is a collection of immutable data about the vulkan context. This actual handles to the
// context are stored in VulkanPlatform.
struct VulkanContext {
public:
    static uint32_t selectMemoryType(VkPhysicalDeviceMemoryProperties const& memoryProperties,
            uint32_t types, VkFlags reqs) {
        for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
            if (types & 1) {
                if ((memoryProperties.memoryTypes[i].propertyFlags & reqs) == reqs) {
                    return i;
                }
            }
            types >>= 1;
        }
        return (uint32_t) VK_MAX_MEMORY_TYPES;
    }

    inline uint32_t selectMemoryType(uint32_t types, VkFlags reqs) const {
        if ((reqs & VK_MEMORY_PROPERTY_PROTECTED_BIT) != 0) {
            assert_invariant(isProtectedMemorySupported());
        }
        return selectMemoryType(mMemoryProperties, types, reqs);
    }

    inline fvkutils::VkFormatList const& getAttachmentDepthStencilFormats() const {
        return mDepthStencilFormats;
    }

    inline fvkutils::VkFormatList const& getBlittableDepthStencilFormats() const {
        return mBlittableDepthStencilFormats;
    }

    inline VkPhysicalDeviceLimits const& getPhysicalDeviceLimits() const noexcept {
        return mPhysicalDeviceProperties.properties.limits;
    }

    inline uint32_t getPhysicalDeviceVendorId() const noexcept {
        return mPhysicalDeviceProperties.properties.vendorID;
    }

    inline bool isImageCubeArraySupported() const noexcept {
        return mPhysicalDeviceFeatures.features.imageCubeArray == VK_TRUE;
    }

    inline bool isDepthClampSupported() const noexcept {
        return mPhysicalDeviceFeatures.features.depthClamp == VK_TRUE;
    }

    inline bool isDebugMarkersSupported() const noexcept {
        return mDebugMarkersSupported;
    }

    inline bool isDebugUtilsSupported() const noexcept {
        return mDebugUtilsSupported;
    }

    inline bool isMultiviewEnabled() const noexcept {
        return mPhysicalDeviceVk11Features.multiview == VK_TRUE;
    }

    inline bool isClipDistanceSupported() const noexcept {
        return mPhysicalDeviceFeatures.features.shaderClipDistance == VK_TRUE;
    }

    inline bool isLazilyAllocatedMemorySupported() const noexcept {
        return mLazilyAllocatedMemorySupported;
    }

    inline bool isProtectedMemorySupported() const noexcept {
        return mProtectedMemorySupported;
    }

    inline bool isImageView2DOn3DImageSupported() const noexcept {
        return mPortabilitySubsetFeatures.imageView2DOn3DImage == VK_TRUE;
    }

    inline bool isUnifiedMemoryArchitecture() const noexcept {
        return mIsUnifiedMemoryArchitecture;
    }

    inline bool stagingBufferBypassEnabled() const noexcept {
        return mStagingBufferBypassEnabled;
    }

private:
    VkPhysicalDeviceMemoryProperties mMemoryProperties = {};
    VkPhysicalDeviceProperties2 mPhysicalDeviceProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    };
    VkPhysicalDeviceVulkan11Features mPhysicalDeviceVk11Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
    };
    VkPhysicalDeviceFeatures2 mPhysicalDeviceFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
    };
    VkPhysicalDevicePortabilitySubsetFeaturesKHR mPortabilitySubsetFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR,
        // By default, on platforms where we don't have portability subset, then this feature must
        // exists.  We only fill this struct only when portability subset is needed (i.e.
        // non-conformant vulkan implementation).
        .imageView2DOn3DImage = VK_TRUE,
    };
    bool mDebugMarkersSupported = false;
    bool mDebugUtilsSupported = false;
    bool mLazilyAllocatedMemorySupported = false;
    bool mProtectedMemorySupported = false;
    bool mIsUnifiedMemoryArchitecture = false;
    bool mStagingBufferBypassEnabled = false;

    fvkutils::VkFormatList mDepthStencilFormats;
    fvkutils::VkFormatList mBlittableDepthStencilFormats;

    // For convenience so that VulkanPlatform can initialize the private fields.
    friend class VulkanPlatform;
};

}// namespace filament::backend

#endif// TNT_FILAMENT_BACKEND_VULKANCONTEXT_H
