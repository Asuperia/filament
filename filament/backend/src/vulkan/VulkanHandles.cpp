/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "VulkanHandles.h"

#include "VulkanConstants.h"

// TODO: remove this by moving DebugUtils out of VulkanDriver
#include "VulkanDriver.h"

#include "VulkanMemory.h"
#include "vulkan/memory/ResourcePointer.h"
#include "vulkan/utils/Conversion.h"
#include "vulkan/utils/Definitions.h"
#include "vulkan/utils/Image.h"
#include "vulkan/utils/Spirv.h"

#include <backend/platforms/VulkanPlatform.h>

#include <utils/compiler.h> // UTILS_FALLTHROUGH
#include <utils/Panic.h>    // ASSERT_POSTCONDITION

using namespace bluevk;

namespace filament::backend {

namespace {

inline VulkanBufferUsage getBufferObjectUsage(BufferObjectBinding bindingType) noexcept {
    switch (bindingType) {
        case BufferObjectBinding::VERTEX:
            return VulkanBufferUsage::VERTEX;
        case BufferObjectBinding::UNIFORM:
            return VulkanBufferUsage::UNIFORM;
        case BufferObjectBinding::SHADER_STORAGE:
            return VulkanBufferUsage::SHADER_STORAGE;
            // when adding more buffer-types here, make sure to update VulkanBuffer::loadFromCpu()
            // if necessary.
    }
    return VulkanBufferUsage::UNKNOWN;
}

void flipVertically(VkViewport* rect, uint32_t framebufferHeight) {
    rect->y = framebufferHeight - rect->y - rect->height;
}

void clampToFramebuffer(VkRect2D* rect, uint32_t fbWidth, uint32_t fbHeight) {
    rect->offset.y = fbHeight - rect->offset.y - rect->extent.height;
    int32_t x = std::max(rect->offset.x, 0);
    int32_t y = std::max(rect->offset.y, 0);
    int32_t right = std::min(rect->offset.x + (int32_t) rect->extent.width, (int32_t) fbWidth);
    int32_t top = std::min(rect->offset.y + (int32_t) rect->extent.height, (int32_t) fbHeight);
    rect->offset.x = std::min(x, (int32_t) fbWidth);
    rect->offset.y = std::min(y, (int32_t) fbHeight);
    rect->extent.width = std::max(right - x, 0);
    rect->extent.height = std::max(top - y, 0);
}

template<typename Bitmask>
inline void fromStageFlags(backend::ShaderStageFlags stage, descriptor_binding_t binding,
        Bitmask& mask) {
    if ((bool) (stage & ShaderStageFlags::VERTEX)) {
        mask.set(binding + fvkutils::getVertexStageShift<Bitmask>());
    }
    if ((bool) (stage & ShaderStageFlags::FRAGMENT)) {
        mask.set(binding + fvkutils::getFragmentStageShift<Bitmask>());
    }
}

inline VkShaderStageFlags getVkStage(backend::ShaderStage stage) {
    switch(stage) {
        case backend::ShaderStage::VERTEX:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case backend::ShaderStage::FRAGMENT:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case backend::ShaderStage::COMPUTE:
            PANIC_POSTCONDITION("Unsupported stage");
    }
}

using BitmaskGroup = VulkanDescriptorSetLayout::Bitmask;
BitmaskGroup fromBackendLayout(DescriptorSetLayout const& layout) {
    BitmaskGroup mask;
    for (auto const& binding: layout.bindings) {
        switch (binding.type) {
            case DescriptorType::UNIFORM_BUFFER: {
                if ((binding.flags & DescriptorFlags::DYNAMIC_OFFSET) != DescriptorFlags::NONE) {
                    fromStageFlags(binding.stageFlags, binding.binding, mask.dynamicUbo);
                } else {
                    fromStageFlags(binding.stageFlags, binding.binding, mask.ubo);
                }
                break;
            }
            case DescriptorType::SAMPLER_EXTERNAL:
                fromStageFlags(binding.stageFlags, binding.binding, mask.externalSampler);
                UTILS_FALLTHROUGH;

            case DescriptorType::SAMPLER_2D_FLOAT:
            case DescriptorType::SAMPLER_2D_INT:
            case DescriptorType::SAMPLER_2D_UINT:
            case DescriptorType::SAMPLER_2D_DEPTH:
            case DescriptorType::SAMPLER_2D_ARRAY_FLOAT:
            case DescriptorType::SAMPLER_2D_ARRAY_INT:
            case DescriptorType::SAMPLER_2D_ARRAY_UINT:
            case DescriptorType::SAMPLER_2D_ARRAY_DEPTH:
            case DescriptorType::SAMPLER_CUBE_FLOAT:
            case DescriptorType::SAMPLER_CUBE_INT:
            case DescriptorType::SAMPLER_CUBE_UINT:
            case DescriptorType::SAMPLER_CUBE_DEPTH:
            case DescriptorType::SAMPLER_CUBE_ARRAY_FLOAT:
            case DescriptorType::SAMPLER_CUBE_ARRAY_INT:
            case DescriptorType::SAMPLER_CUBE_ARRAY_UINT:
            case DescriptorType::SAMPLER_CUBE_ARRAY_DEPTH:
            case DescriptorType::SAMPLER_3D_FLOAT:
            case DescriptorType::SAMPLER_3D_INT:
            case DescriptorType::SAMPLER_3D_UINT:
            case DescriptorType::SAMPLER_2D_MS_FLOAT:
            case DescriptorType::SAMPLER_2D_MS_INT:
            case DescriptorType::SAMPLER_2D_MS_UINT:
            case DescriptorType::SAMPLER_2D_MS_ARRAY_FLOAT:
            case DescriptorType::SAMPLER_2D_MS_ARRAY_INT:
            case DescriptorType::SAMPLER_2D_MS_ARRAY_UINT: {
                fromStageFlags(binding.stageFlags, binding.binding, mask.sampler);
                break;
            }
            case DescriptorType::INPUT_ATTACHMENT: {
                fromStageFlags(binding.stageFlags, binding.binding, mask.inputAttachment);
                break;
            }
            case DescriptorType::SHADER_STORAGE_BUFFER:
                PANIC_POSTCONDITION("Shader storage is not supported");
                break;
        }
    }
    return mask;
}

fvkmemory::resource_ptr<VulkanTexture> initMsaaTexture(
        fvkmemory::resource_ptr<VulkanTexture> texture, VkDevice device,
        VkPhysicalDevice physicalDevice, VulkanContext const& context, VmaAllocator allocator,
        VulkanCommands* commands, fvkmemory::ResourceManager* resManager, uint8_t levels,
        uint8_t samples, VulkanStagePool& stagePool) {
    assert_invariant(texture);
    auto msTexture = texture->getSidecar();
    if (UTILS_UNLIKELY(!msTexture)) {
        // Clear all usage flags that are not related to attachments, so that we can
        // use the transient usage flag.
        const TextureUsage usage = texture->usage & TextureUsage::ALL_ATTACHMENTS;
        assert_invariant(static_cast<uint16_t>(usage) != 0U);

        msTexture = resource_ptr<VulkanTexture>::construct(resManager, device, physicalDevice,
                context, allocator, resManager, commands, texture->target, levels, texture->format,
                samples, texture->width, texture->height, texture->depth, usage, stagePool);
        texture->setSidecar(msTexture);
    }
    return msTexture;
}

VulkanAttachment createSwapchainAttachment(const fvkmemory::resource_ptr<VulkanTexture> texture) {
    return VulkanAttachment {
        .texture = texture,
        .level = 0,
        .layerCount = static_cast<uint8_t>(texture ? texture->getPrimaryViewRange().layerCount : 1),
        .layer = 0,
    };
}

} // anonymous namespace

VulkanDescriptorSetLayout::VulkanDescriptorSetLayout(DescriptorSetLayout&& layout,
        VkDescriptorSetLayout vkLayout)
    : bitmask(fromBackendLayout(layout)),
      count(Count::fromLayoutBitmask(bitmask)),
      mVkLayout(vkLayout) {}

VulkanDescriptorSetLayout::Bitmask VulkanDescriptorSetLayout::Bitmask::fromLayoutDescription(
        DescriptorSetLayout const& layout) {
    return fromBackendLayout(layout);
}

// This method will store an age associated with this command buffer into the VulkanBuffer, which
// will allow us to determine whether a barrier is necessary or not.
void VulkanDescriptorSet::referencedBy(VulkanCommandBuffer& commands) {
    mUboMask.forEachSetBit([this, &commands](size_t index) {
        auto& res = mResources[index];
        fvkmemory::resource_ptr<VulkanBufferObject> bo =
                fvkmemory::resource_ptr<VulkanBufferObject>::cast((VulkanBufferObject*) res.get());
        bo->referencedBy(commands);
    });
}

PushConstantDescription::PushConstantDescription(backend::Program const& program) {
    mRangeCount = 0;
    for (auto stage : { ShaderStage::VERTEX, ShaderStage::FRAGMENT, ShaderStage::COMPUTE }) {
        auto const& constants = program.getPushConstants(stage);
        if (constants.empty()) {
            continue;
        }

        // We store the type of the constant for type-checking when writing.
        auto& types = mTypes[(uint8_t) stage];
        types.reserve(constants.size());
        std::for_each(constants.cbegin(), constants.cend(), [&types] (Program::PushConstant t) {
            types.push_back(t.type);
        });

        mRanges[mRangeCount++] = {
            .stageFlags = getVkStage(stage),
            .offset = 0,
            .size = (uint32_t) constants.size() * ENTRY_SIZE,
        };
    }
}

void PushConstantDescription::write(VkCommandBuffer cmdbuf, VkPipelineLayout layout,
        backend::ShaderStage stage, uint8_t index, backend::PushConstantVariant const& value) {

    uint32_t binaryValue = 0;
    UTILS_UNUSED_IN_RELEASE auto const& types = mTypes[(uint8_t) stage];
    if (std::holds_alternative<bool>(value)) {
        assert_invariant(types[index] == ConstantType::BOOL);
        bool const bval = std::get<bool>(value);
        binaryValue = static_cast<uint32_t const>(bval ? VK_TRUE : VK_FALSE);
    } else if (std::holds_alternative<float>(value)) {
        assert_invariant(types[index] == ConstantType::FLOAT);
        float const fval = std::get<float>(value);
        binaryValue = *reinterpret_cast<uint32_t const*>(&fval);
    } else {
        assert_invariant(types[index] == ConstantType::INT);
        int const ival = std::get<int>(value);
        binaryValue = *reinterpret_cast<uint32_t const*>(&ival);
    }
    vkCmdPushConstants(cmdbuf, layout, getVkStage(stage), index * ENTRY_SIZE, ENTRY_SIZE,
            &binaryValue);
}

VulkanProgram::VulkanProgram(VkDevice device, Program const& builder) noexcept
    : HwProgram(builder.getName()),
      mInfo(new(std::nothrow) PipelineInfo(builder)),
      mDevice(device) {

    Program::ShaderSource const& blobs = builder.getShadersSource();
    auto& modules = mInfo->shaders;
    auto const& specializationConstants = builder.getSpecializationConstants();
    std::vector<uint32_t> shader;

    static_assert(static_cast<ShaderStage>(0) == ShaderStage::VERTEX &&
            static_cast<ShaderStage>(1) == ShaderStage::FRAGMENT &&
            MAX_SHADER_MODULES == 2);

    for (size_t i = 0; i < MAX_SHADER_MODULES; i++) {
        Program::ShaderBlob const& blob = blobs[i];

        uint32_t* data = (uint32_t*) blob.data();
        size_t dataSize = blob.size();

        if (!specializationConstants.empty()) {
            fvkutils::workaroundSpecConstant(blob, specializationConstants, shader);
            data = (uint32_t*) shader.data();
            dataSize = shader.size() * 4;
        }

        VkShaderModule& module = modules[i];
        VkShaderModuleCreateInfo moduleInfo = {
            .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
            .codeSize = dataSize,
            .pCode = data,
        };
        VkResult result = vkCreateShaderModule(mDevice, &moduleInfo, VKALLOC, &module);
        FILAMENT_CHECK_POSTCONDITION(result == VK_SUCCESS)
                << "Unable to create shader module."
                << " error=" << static_cast<int32_t>(result);

#if FVK_ENABLED(FVK_DEBUG_DEBUG_UTILS)
        std::string name{ builder.getName().c_str(), builder.getName().size() };
        switch (static_cast<ShaderStage>(i)) {
            case ShaderStage::VERTEX:
                name += "_vs";
                break;
            case ShaderStage::FRAGMENT:
                name += "_fs";
                break;
            default:
                PANIC_POSTCONDITION("Unexpected stage");
                break;
        }
        VulkanDriver::DebugUtils::setName(VK_OBJECT_TYPE_SHADER_MODULE,
                reinterpret_cast<uint64_t>(module), name.c_str());
#endif
    }

#if FVK_ENABLED(FVK_DEBUG_SHADER_MODULE)
    FVK_LOGD << "Created VulkanProgram " << builder << ", shaders = (" << modules[0]
             << ", " << modules[1] << ")";
#endif
}

VulkanProgram::~VulkanProgram() {
    for (auto shader: mInfo->shaders) {
        vkDestroyShaderModule(mDevice, shader, VKALLOC);
    }
    delete mInfo;
}

// Creates a special "default" render target (i.e. associated with the swap chain)
VulkanRenderTarget::VulkanRenderTarget()
    : HwRenderTarget(0, 0),
      mOffscreen(false),
      mProtected(false),
      mInfo(std::make_unique<Auxiliary>()) {
    mInfo->rpkey.samples = mInfo->fbkey.samples = 1;
}

VulkanRenderTarget::~VulkanRenderTarget() = default;

void VulkanRenderTarget::bindToSwapChain(fvkmemory::resource_ptr<VulkanSwapChain> swapchain) {
    assert_invariant(!mOffscreen);

    VkExtent2D const extent = swapchain->getExtent();
    width = extent.width;
    height = extent.height;
    mProtected = swapchain->isProtected();

    VulkanAttachment color = createSwapchainAttachment(swapchain->getCurrentColor());
    mInfo->attachments = {color};

    auto& fbkey = mInfo->fbkey;
    auto& rpkey = mInfo->rpkey;

    rpkey.colorFormat[0] = color.getFormat();
    rpkey.viewCount = color.layerCount;
    fbkey.width = width;
    fbkey.height = height;
    fbkey.color[0] = color.getImageView();
    fbkey.resolve[0] = VK_NULL_HANDLE;

    if (swapchain->getDepth()) {
        VulkanAttachment depth = createSwapchainAttachment(swapchain->getDepth());
        mInfo->attachments.push_back(depth);
        mInfo->depthIndex = 1;

        rpkey.depthFormat = depth.getFormat();
        fbkey.depth = depth.getImageView();
    } else {
        rpkey.depthFormat = VK_FORMAT_UNDEFINED;
        fbkey.depth = VK_NULL_HANDLE;
    }
    mInfo->colors.set(0);
}

VulkanRenderTarget::VulkanRenderTarget(VkDevice device, VkPhysicalDevice physicalDevice,
        VulkanContext const& context, fvkmemory::ResourceManager* resourceManager,
        VmaAllocator allocator, VulkanCommands* commands, uint32_t width, uint32_t height,
        uint8_t samples, VulkanAttachment color[MRT::MAX_SUPPORTED_RENDER_TARGET_COUNT],
        VulkanAttachment depthStencil[2], VulkanStagePool& stagePool, uint8_t layerCount)
    : HwRenderTarget(width, height),
      mOffscreen(true),
      mProtected(false),
      mInfo(std::make_unique<Auxiliary>()) {
    auto& depth = depthStencil[0];

    // Constrain the sample count according to both kinds of sample count masks obtained from
    // VkPhysicalDeviceProperties. This is consistent with the VulkanTexture constructor.
    auto const& limits = context.getPhysicalDeviceLimits();
    samples = fvkutils::reduceSampleCount(samples,
            limits.framebufferDepthSampleCounts & limits.framebufferColorSampleCounts);

    auto& rpkey = mInfo->rpkey;
    rpkey.samples = samples;
    rpkey.depthFormat = depth.getFormat();
    rpkey.viewCount = layerCount;

    auto& fbkey = mInfo->fbkey;
    fbkey.width = width;
    fbkey.height = height;
    fbkey.samples = samples;

    std::vector<VulkanAttachment>& attachments = mInfo->attachments;
    std::vector<VulkanAttachment> msaaAttachments;

    for (int index = 0; index < MRT::MAX_SUPPORTED_RENDER_TARGET_COUNT; index++) {
        VulkanAttachment& attachment = color[index];
        auto texture = attachment.texture;
        if (!texture) {
            rpkey.colorFormat[index] = VK_FORMAT_UNDEFINED;
            continue;
        }

        mProtected |= texture->getIsProtected();

        attachments.push_back(attachment);
        mInfo->colors.set(index);

        rpkey.colorFormat[index] = attachment.getFormat();
        fbkey.color[index] = attachment.getImageView();
        fbkey.resolve[index] = VK_NULL_HANDLE;

        if (samples > 1) {
            VulkanAttachment msaaAttachment = {};
            if (texture->samples == 1) {
                auto msaaTexture = initMsaaTexture(texture, device, physicalDevice, context,
                        allocator, commands, resourceManager, texture->levels, samples, stagePool);
                if (msaaTexture && msaaTexture->isTransientAttachment()) {
                    rpkey.usesLazilyAllocatedMemory |= (1 << index);
                }
                if (attachment.texture->samples == 1) {
                    rpkey.needsResolveMask |= (1 << index);
                }
                msaaAttachment = {
                    .texture = msaaTexture,
                    .layerCount = layerCount,
                };

                fbkey.resolve[index] = attachment.getImageView();
            } else {
                msaaAttachment = {
                    .texture = texture,
                    .layerCount = layerCount,
                };
            }
            fbkey.color[index] = msaaAttachment.getImageView();
            msaaAttachments.push_back(msaaAttachment);
        }
    }

    if (attachments.size() > 0 && samples > 1 && msaaAttachments.size() > 0) {
        mInfo->msaaIndex = (uint8_t) attachments.size();
        attachments.insert(attachments.end(), msaaAttachments.begin(), msaaAttachments.end());
    }

    if (depth.texture) {
        auto depthTexture = depth.texture;
        mInfo->depthIndex = (uint8_t) attachments.size();
        attachments.push_back(depth);
        fbkey.depth = depth.getImageView();
        if (samples > 1) {
            mInfo->msaaDepthIndex = mInfo->depthIndex;
            if (depthTexture->samples == 1) {
                // MSAA depth texture must have the mipmap count of 1
                uint8_t const msLevel = 1;
                // Create sidecar MSAA texture for the depth attachment if it does not already
                // exist.
                auto msaaTexture = initMsaaTexture(depthTexture, device, physicalDevice, context,
                        allocator, commands, resourceManager, msLevel, samples, stagePool);
                mInfo->msaaDepthIndex = (uint8_t) attachments.size();
                VulkanAttachment msaaAttachment = {
                    .texture = msaaTexture,
                    .layerCount = layerCount,
                };
                attachments.push_back(msaaAttachment);
                fbkey.depth = msaaAttachment.getImageView();
            }
        }
    }
}

void VulkanRenderTarget::transformClientRectToPlatform(VkRect2D* bounds) const {
    auto const& extent = getExtent();
    clampToFramebuffer(bounds, extent.width, extent.height);
}

void VulkanRenderTarget::transformViewportToPlatform(VkViewport* bounds) const {
    flipVertically(bounds, getExtent().height);
}

uint8_t VulkanRenderTarget::getColorTargetCount(const VulkanRenderPass& pass) const {
    if (!mOffscreen) {
        return 1;
    }
    if (pass.currentSubpass == 1) {
        return mInfo->colors.count();
    }
    uint8_t count = 0;
    mInfo->colors.forEachSetBit([&count, &pass](size_t index) {
        if (!(pass.params.subpassMask & (1 << index))) {
            count++;
        }
    });
    return count;
}

void VulkanRenderTarget::emitBarriersBeginRenderPass(VulkanCommandBuffer& commands) {
    auto& attachments = mInfo->attachments;
    auto samples = mInfo->fbkey.samples;
    auto barrier = [&commands](VulkanAttachment& attachment, VulkanLayout const layout) {
        auto tex = attachment.texture;
        auto const& range = attachment.getSubresourceRange();
        if (tex->getLayout(range.baseMipLevel, range.baseArrayLayer) != layout &&
                !tex->transitionLayout(&commands, range, layout)) {
            // If the layout transition did not emit a barrier, we do it manually here.
            tex->samplerToAttachmentBarrier(&commands, range);
        }
    };

    for (size_t i = 0, count = mInfo->colors.count(); i < count; ++i) {
        auto& attachment = attachments[i];
        auto tex = attachment.texture;
        if (samples == 1 || tex->samples == 1) {
            barrier(attachment, VulkanLayout::COLOR_ATTACHMENT);
        }
    }
    if (mInfo->msaaIndex != Auxiliary::UNDEFINED_INDEX) {
        for (size_t i = mInfo->msaaIndex, count = mInfo->msaaIndex + mInfo->colors.count();
                i < count; ++i) {
            barrier(attachments[i], VulkanLayout::COLOR_ATTACHMENT);
        }
    }
    if (mInfo->depthIndex != Auxiliary::UNDEFINED_INDEX) {
        barrier(attachments[mInfo->depthIndex], VulkanLayout::DEPTH_ATTACHMENT);
    }
    if (mInfo->msaaDepthIndex != Auxiliary::UNDEFINED_INDEX) {
        barrier(attachments[mInfo->msaaDepthIndex], VulkanLayout::DEPTH_ATTACHMENT);
    }
}

void VulkanRenderTarget::emitBarriersEndRenderPass(VulkanCommandBuffer& commands) {
    if (isSwapChain()) {
        return;
    }

    for (auto& attachment: mInfo->attachments) {
        auto const& range = attachment.getSubresourceRange();
        bool const isDepth = attachment.isDepth();
        auto texture = attachment.texture;
        if (isDepth) {
            texture->setLayout(range, VulkanFboCache::FINAL_DEPTH_ATTACHMENT_LAYOUT);
            if (!texture->transitionLayout(&commands, range, VulkanLayout::DEPTH_SAMPLER)) {
                texture->attachmentToSamplerBarrier(&commands, range);
            }
        } else {
            texture->setLayout(range, VulkanFboCache::FINAL_COLOR_ATTACHMENT_LAYOUT);
            if (texture->isSampleable() &&
                    !texture->transitionLayout(&commands, range, VulkanLayout::FRAG_READ)) {
                texture->attachmentToSamplerBarrier(&commands, range);
            }
        }
    }
}

VulkanVertexBufferInfo::VulkanVertexBufferInfo(
        uint8_t bufferCount, uint8_t attributeCount, AttributeArray const& attributes)
    : HwVertexBufferInfo(bufferCount, attributeCount),
      mInfo(attributes.size()) {
    auto attribDesc = mInfo.mSoa.data<PipelineInfo::ATTRIBUTE_DESCRIPTION>();
    auto bufferDesc = mInfo.mSoa.data<PipelineInfo::BUFFER_DESCRIPTION>();
    auto offsets = mInfo.mSoa.data<PipelineInfo::OFFSETS>();
    auto attribToBufferIndex = mInfo.mSoa.data<PipelineInfo::ATTRIBUTE_TO_BUFFER_INDEX>();
    std::fill(mInfo.mSoa.begin<PipelineInfo::ATTRIBUTE_TO_BUFFER_INDEX>(),
            mInfo.mSoa.end<PipelineInfo::ATTRIBUTE_TO_BUFFER_INDEX>(), -1);

    for (uint32_t attribIndex = 0; attribIndex < attributes.size(); attribIndex++) {
        Attribute attrib = attributes[attribIndex];
        bool const isInteger = attrib.flags & Attribute::FLAG_INTEGER_TARGET;
        bool const isNormalized = attrib.flags & Attribute::FLAG_NORMALIZED;
        VkFormat vkformat = fvkutils::getVkFormat(attrib.type, isNormalized, isInteger);

        // HACK: Re-use the positions buffer as a dummy buffer for disabled attributes. Filament's
        // vertex shaders declare all attributes as either vec4 or uvec4 (the latter for bone
        // indices), and positions are always at least 32 bits per element. Therefore we can assign
        // a dummy type of either R8G8B8A8_UINT or R8G8B8A8_SNORM, depending on whether the shader
        // expects to receive floats or ints.
        if (attrib.buffer == Attribute::BUFFER_UNUSED) {
            vkformat = isInteger ? VK_FORMAT_R8G8B8A8_UINT : VK_FORMAT_R8G8B8A8_SNORM;
            attrib = attributes[0];
        }
        offsets[attribIndex] = attrib.offset;
        attribDesc[attribIndex] = {
            .location = attribIndex,// matches the GLSL layout specifier
            .binding = attribIndex, // matches the position within vkCmdBindVertexBuffers
            .format = vkformat,
        };
        bufferDesc[attribIndex] = {
            .binding = attribIndex,
            .stride = attrib.stride,
        };
        attribToBufferIndex[attribIndex] = attrib.buffer;
    }
}

VulkanVertexBuffer::VulkanVertexBuffer(VulkanContext& context, VulkanStagePool& stagePool,
        uint32_t vertexCount, fvkmemory::resource_ptr<VulkanVertexBufferInfo> vbi)
    : HwVertexBuffer(vertexCount),
      vbi(vbi),
      // TODO: Seems a bit wasteful. can we do better here?
      mBuffers(MAX_VERTEX_BUFFER_COUNT) {
}

void VulkanVertexBuffer::setBuffer(fvkmemory::resource_ptr<VulkanBufferObject> bufferObject,
        uint32_t index) {
    size_t const count = vbi->getAttributeCount();
    VkBuffer* const vkbuffers = getVkBuffers();
    int8_t const* const attribToBuffer = vbi->getAttributeToBuffer();
    for (uint8_t attribIndex = 0; attribIndex < count; attribIndex++) {
        if (attribToBuffer[attribIndex] == static_cast<int8_t>(index)) {
            vkbuffers[attribIndex] = bufferObject->getVkBuffer();
        }
    }
    mResources.push_back(bufferObject);
}

VulkanBufferObject::VulkanBufferObject(VulkanContext const& context, VmaAllocator allocator,
        VulkanStagePool& stagePool, VulkanBufferCache& bufferCache, uint32_t byteCount,
        BufferObjectBinding bindingType)
    : HwBufferObject(byteCount),
      bindingType(bindingType),
      mBuffer(context, allocator, stagePool, bufferCache, getBufferObjectUsage(bindingType), byteCount) {}

VulkanRenderPrimitive::VulkanRenderPrimitive(PrimitiveType pt,
        fvkmemory::resource_ptr<VulkanVertexBuffer> vb,
        fvkmemory::resource_ptr<VulkanIndexBuffer> ib)
    : HwRenderPrimitive{ .type = pt },
      vertexBuffer(vb),
      indexBuffer(ib) {}

} // namespace filament::backend
