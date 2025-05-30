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

#include <private/backend/PlatformFactory.h>

#include <private/utils/Tracing.h>

#include <utils/debug.h>

// We need to keep this up top for the linux (X11) name collisions.
#if defined(FILAMENT_SUPPORTS_WEBGPU)
    #include "backend/platforms/WebGPUPlatform.h"
#endif

#if defined(__ANDROID__)
    #include <sys/system_properties.h>
    #if defined(FILAMENT_SUPPORTS_OPENGL) && !defined(FILAMENT_USE_EXTERNAL_GLES3)
        #include "backend/platforms/PlatformEGLAndroid.h"
    #endif
#elif defined(FILAMENT_IOS)
    #if defined(FILAMENT_SUPPORTS_OPENGL) && !defined(FILAMENT_USE_EXTERNAL_GLES3)
        #include "backend/platforms/PlatformCocoaTouchGL.h"
    #endif
#elif defined(__APPLE__)
    #if defined(FILAMENT_SUPPORTS_OPENGL) && !defined(FILAMENT_USE_EXTERNAL_GLES3)
        #if defined(FILAMENT_SUPPORTS_OSMESA)
            #include <backend/platforms/PlatformOSMesa.h>
        #else
            #include <backend/platforms/PlatformCocoaGL.h>
        #endif
    #endif
#elif defined(__linux__)
    #if defined(FILAMENT_SUPPORTS_X11)
        #if defined(FILAMENT_SUPPORTS_OPENGL) && !defined(FILAMENT_USE_EXTERNAL_GLES3)
            #include "backend/platforms/PlatformGLX.h"
        #endif
    #elif defined(FILAMENT_SUPPORTS_EGL_ON_LINUX)
        #if defined(FILAMENT_SUPPORTS_OPENGL) && !defined(FILAMENT_USE_EXTERNAL_GLES3)
            #include "backend/platforms/PlatformEGLHeadless.h"
        #endif
    #elif defined(FILAMENT_SUPPORTS_OSMESA)
        #if defined(FILAMENT_SUPPORTS_OPENGL) && !defined(FILAMENT_USE_EXTERNAL_GLES3)
            #include "backend/platforms/PlatformOSMesa.h"
        #endif
    #endif
#elif defined(WIN32)
    #if defined(FILAMENT_SUPPORTS_OPENGL) && !defined(FILAMENT_USE_EXTERNAL_GLES3)
        #include "backend/platforms/PlatformWGL.h"
    #endif
#elif defined(__EMSCRIPTEN__)
    #include "backend/platforms/PlatformWebGL.h"
#endif

#if defined(FILAMENT_DRIVER_SUPPORTS_VULKAN)
    #if defined(__ANDROID__)
        #include "backend/platforms/VulkanPlatformAndroid.h"
    #else
        #include "backend/platforms/VulkanPlatform.h"
    #endif
#endif

#if defined (FILAMENT_SUPPORTS_METAL)
namespace filament::backend {
filament::backend::Platform* createDefaultMetalPlatform();
}
#endif

#include "noop/PlatformNoop.h"

namespace filament::backend {

// Creates the platform-specific Platform object. The caller takes ownership and is
// responsible for destroying it. Initialization of the backend API is deferred until
// createDriver(). The passed-in backend hint is replaced with the resolved backend.
Platform* PlatformFactory::create(Backend* backend) noexcept {
    FILAMENT_TRACING_CALL(FILAMENT_TRACING_CATEGORY_FILAMENT);
    assert_invariant(backend);

#if defined(__ANDROID__)
    char scratch[PROP_VALUE_MAX + 1];
    int length = __system_property_get("debug.filament.backend", scratch);
    if (length > 0) {
        *backend = Backend(atoi(scratch));
    }
#endif

    if (*backend == Backend::DEFAULT) {
#if defined(__EMSCRIPTEN__)
        *backend = Backend::OPENGL;
#elif defined(__ANDROID__)
        *backend = Backend::OPENGL;
#elif defined(FILAMENT_IOS) || defined(__APPLE__)
        *backend = Backend::METAL;
#elif defined(FILAMENT_DRIVER_SUPPORTS_VULKAN)
        *backend = Backend::VULKAN;
#else
        *backend = Backend::OPENGL;
#endif
    }
    if (*backend == Backend::NOOP) {
        return new PlatformNoop();
    }
    if (*backend == Backend::VULKAN) {
        #if defined(FILAMENT_DRIVER_SUPPORTS_VULKAN)
            #if defined(__ANDROID__)
                return new VulkanPlatformAndroid();
            #else
                return new VulkanPlatform();
            #endif
        #else
            return nullptr;
        #endif
    }
    if (*backend == Backend::WEBGPU) {
        #if defined(FILAMENT_SUPPORTS_WEBGPU)
            return new WebGPUPlatform();
        #else
            return nullptr;
        #endif
    }
    if (*backend == Backend::METAL) {
#if defined(FILAMENT_SUPPORTS_METAL)
        return createDefaultMetalPlatform();
#else
        return nullptr;
#endif
    }
    assert_invariant(*backend == Backend::OPENGL);
    #if defined(FILAMENT_SUPPORTS_OPENGL)
        #if defined(FILAMENT_USE_EXTERNAL_GLES3)
            return nullptr;
        #elif defined(__ANDROID__)
            return new PlatformEGLAndroid();
        #elif defined(FILAMENT_IOS)
            return new PlatformCocoaTouchGL();
        #elif defined(__APPLE__)
            #if defined(FILAMENT_SUPPORTS_OSMESA)
                return new PlatformOSMesa();
            #else
                return new PlatformCocoaGL();
            #endif
        #elif defined(__linux__)
            #if defined(FILAMENT_SUPPORTS_X11)
                return new PlatformGLX();
            #elif defined(FILAMENT_SUPPORTS_EGL_ON_LINUX)
                return new PlatformEGLHeadless();
            #elif defined(FILAMENT_SUPPORTS_OSMESA)
                return new PlatformOSMesa();
            #else
                return nullptr;
            #endif
        #elif defined(WIN32)
            return new PlatformWGL();
        #elif defined(__EMSCRIPTEN__)
            return new PlatformWebGL();
        #else
            return nullptr;
        #endif
    #else
        return nullptr;
    #endif
}

// destroys a Platform created by create()
void PlatformFactory::destroy(Platform** platform) noexcept {
    delete *platform;
    *platform = nullptr;
}

} // namespace filament::backend
