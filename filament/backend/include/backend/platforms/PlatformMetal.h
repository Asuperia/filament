/*
 * Copyright (C) 2021 The Android Open Source Project
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

#ifndef TNT_FILAMENT_BACKEND_PRIVATE_PLATFORMMETAL_H
#define TNT_FILAMENT_BACKEND_PRIVATE_PLATFORMMETAL_H

#include <backend/DriverEnums.h>
#include <backend/Platform.h>

namespace filament::backend {

struct PlatformMetalImpl;

// In order for this header to be compatible with Objective-C and C++, we use these wrappers around
// id<MTL*> objects.
// See PlatformMetal-Objc.h.
struct MetalDevice;
struct MetalCommandQueue;
struct MetalCommandBuffer;

class PlatformMetal final : public Platform {
public:
    PlatformMetal();
    ~PlatformMetal() noexcept override;

    Driver* createDriver(void* sharedContext, const Platform::DriverConfig& driverConfig) noexcept override;
    int getOSVersion() const noexcept override { return 0; }

    /**
     * Optionally initializes the Metal platform by acquiring resources necessary for rendering.
     *
     * This method attempts to acquire a Metal device and command queue, returning true if both are
     * successfully obtained, or false otherwise. Typically, these objects are acquired when
     * the Metal backend is initialized. This method allows clients to check for their availability
     * earlier.
     *
     * Calling initialize() is optional and safe to do so multiple times. After initialize() returns
     * true, subsequent calls will continue to return true but have no effect.
     *
     * initialize() must be called from the main thread.
     *
     * @returns true if the device and command queue have been successfully obtained; false
     * otherwise.
     */
    bool initialize() noexcept;

    /**
     * Obtain the preferred Metal device object for the backend to use.
     *
     * On desktop platforms, there may be multiple GPUs suitable for rendering, and this method is
     * free to decide which one to use. On mobile systems with a single GPU, implementations should
     * simply return the result of MTLCreateSystemDefaultDevice();
     *
     * createDevice is called by the Metal backend from the backend thread.
     */
    virtual void createDevice(MetalDevice& outDevice) noexcept;

    /**
     * Create a command submission queue on the Metal device object.
     *
     * createCommandQueue is called by the Metal backend from the backend thread.
     *
     * @param device The device which was returned from createDevice()
     */
    virtual void createCommandQueue(
            MetalDevice& device, MetalCommandQueue& outCommandQueue) noexcept;

    /**
     * Obtain a MTLCommandBuffer enqueued on this Platform's MTLCommandQueue. The command buffer is
     * guaranteed to execute before all subsequent command buffers created either by Filament, or
     * further calls to this method.
     *
     * createAndEnqueueCommandBuffer must be called from the main thread.
     */
    void createAndEnqueueCommandBuffer(MetalCommandBuffer& outCommandBuffer) noexcept;

    /**
     * The action to take if a Drawable cannot be acquired.
     *
     * Each frame rendered requires a CAMetalDrawable texture, which is presented on-screen at the
     * completion of each frame. These are limited and provided round-robin style by the system.
     *
     * setDrawableFailureBehavior must be called from the main thread.
     */
    enum class DrawableFailureBehavior : uint8_t {
        /**
         * Terminates the application and reports an error message (default).
         */
        PANIC,
        /*
         * Aborts execution of the current frame. The Metal backend will attempt to acquire a new
         * drawable at the next frame.
         */
        ABORT_FRAME
    };
    void setDrawableFailureBehavior(DrawableFailureBehavior behavior) noexcept;
    DrawableFailureBehavior getDrawableFailureBehavior() const noexcept;

private:
    PlatformMetalImpl* pImpl = nullptr;
};

} // namespace filament::backend

#endif // TNT_FILAMENT_BACKEND_PRIVATE_PLATFORMMETAL_H
