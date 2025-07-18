cmake_minimum_required(VERSION 3.19)
project(filament-android)

option(FILAMENT_SUPPORTS_VULKAN "Enables Vulkan on Android" OFF)
option(FILAMENT_ENABLE_FGVIEWER "Enables Frame Graph Viewer" OFF)
option(FILAMENT_ENABLE_MATDBG "Enables Material debugger" OFF)
option(FILAMENT_DISABLE_MATOPT "Disables material optimizations" OFF)
option(FILAMENT_SUPPORTS_WEBGPU "Enables WebGPU on Android" OFF)

set(FILAMENT_DIR ${FILAMENT_DIST_DIR})

add_library(filament STATIC IMPORTED)
set_target_properties(filament PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libfilament.a)

add_library(filament-generatePrefilterMipmap STATIC IMPORTED)
set_target_properties(filament-generatePrefilterMipmap PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libfilament-generatePrefilterMipmap.a)

add_library(backend STATIC IMPORTED)
set_target_properties(backend PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libbackend.a)

add_library(utils STATIC IMPORTED)
set_target_properties(utils PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libutils.a)

add_library(perfetto STATIC IMPORTED)
set_target_properties(perfetto PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libperfetto.a)

add_library(ibl-lite STATIC IMPORTED)
set_target_properties(ibl-lite PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libibl-lite.a)

add_library(filaflat STATIC IMPORTED)
set_target_properties(filaflat PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libfilaflat.a)

add_library(geometry STATIC IMPORTED)
set_target_properties(geometry PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libgeometry.a)

add_library(filabridge STATIC IMPORTED)
set_target_properties(filabridge PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libfilabridge.a)

add_library(bluevk STATIC IMPORTED)
set_target_properties(bluevk PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libbluevk.a)

add_library(vkshaders STATIC IMPORTED)
set_target_properties(vkshaders PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libvkshaders.a)

if (FILAMENT_SUPPORTS_WEBGPU)
        add_library(webgpu_dawn STATIC IMPORTED)
        set_target_properties(webgpu_dawn PROPERTIES IMPORTED_LOCATION
                ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libwebgpu_dawn.a)
endif()

add_library(smol-v STATIC IMPORTED)
set_target_properties(smol-v PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libsmol-v.a)

add_library(abseil STATIC IMPORTED)
set_target_properties(abseil PROPERTIES IMPORTED_LOCATION
        ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libabseil.a)

if (FILAMENT_ENABLE_FGVIEWER)
    add_library(fgviewer STATIC IMPORTED)
    set_target_properties(fgviewer PROPERTIES IMPORTED_LOCATION
            ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libfgviewer.a)
endif()

if (FILAMENT_ENABLE_MATDBG)
    add_library(matdbg STATIC IMPORTED)
    set_target_properties(matdbg PROPERTIES IMPORTED_LOCATION
            ${FILAMENT_DIR}/lib/${ANDROID_ABI}/libmatdbg.a)
endif()

set(VERSION_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/libfilament-jni.map")
set(CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} -Wl,--version-script=${VERSION_SCRIPT}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,-z,max-page-size=16384")

add_library(filament-jni SHARED
    src/main/cpp/BufferObject.cpp
    src/main/cpp/Camera.cpp
    src/main/cpp/Colors.cpp
    src/main/cpp/ColorGrading.cpp
    src/main/cpp/Engine.cpp
    src/main/cpp/EntityManager.cpp
    src/main/cpp/Fence.cpp
    src/main/cpp/IndexBuffer.cpp
    src/main/cpp/IndirectLight.cpp
    src/main/cpp/LightManager.cpp
    src/main/cpp/Material.cpp
    src/main/cpp/MaterialInstance.cpp
    src/main/cpp/MathUtils.cpp
    src/main/cpp/MorphTargetBuffer.cpp
    src/main/cpp/RenderableManager.cpp
    src/main/cpp/Renderer.cpp
    src/main/cpp/RenderTarget.cpp
    src/main/cpp/Scene.cpp
    src/main/cpp/SkyBox.cpp
    src/main/cpp/SkinningBuffer.cpp
    src/main/cpp/Stream.cpp
    src/main/cpp/SurfaceOrientation.cpp
    src/main/cpp/SwapChain.cpp
    src/main/cpp/Texture.cpp
    src/main/cpp/TextureSampler.cpp
    src/main/cpp/ToneMapper.cpp
    src/main/cpp/TransformManager.cpp
    src/main/cpp/VertexBuffer.cpp
    src/main/cpp/View.cpp
    # Android specific
    src/main/cpp/nativewindow/Android.cpp
    # Private utils
    src/main/cpp/Filament.cpp
    # Common utils
    ../common/CallbackUtils.cpp
    ../common/NioUtils.cpp
)

# Ordering is significant in the following list. The PRIVATE qualifier prevents transitive deps.
target_link_libraries(filament-jni
    PRIVATE filament-generatePrefilterMipmap
    PRIVATE filament
    PRIVATE backend
    PRIVATE filaflat
    PRIVATE filabridge
    PRIVATE ibl-lite
    PRIVATE log
    PRIVATE GLESv3
    PRIVATE EGL
    PRIVATE android
    PRIVATE jnigraphics
    PRIVATE utils
    PRIVATE perfetto
    PRIVATE abseil

    # libgeometry is PUBLIC because gltfio uses it.
    PUBLIC geometry

    $<$<STREQUAL:${FILAMENT_ENABLE_FGVIEWER},ON>:fgviewer>
    $<$<STREQUAL:${FILAMENT_ENABLE_MATDBG},ON>:matdbg>
    $<$<STREQUAL:${FILAMENT_ENABLE_MATDBG},ON>:filamat>
    $<$<STREQUAL:${FILAMENT_SUPPORTS_VULKAN},ON>:bluevk>
    $<$<STREQUAL:${FILAMENT_SUPPORTS_VULKAN},ON>:vkshaders>
    $<$<STREQUAL:${FILAMENT_SUPPORTS_WEBGPU},ON>:webgpu_dawn>
    $<$<STREQUAL:${FILAMENT_SUPPORTS_VULKAN},ON>:smol-v>
)

target_include_directories(filament-jni PRIVATE
        ..
        ${FILAMENT_DIR}/include
        ../../filament/backend/include
        ../../third_party/robin-map
        ../../third_party/perfetto
        ../../libs/utils/include)

# Force a relink when the version script is changed:
set_target_properties(filament-jni PROPERTIES LINK_DEPENDS ${VERSION_SCRIPT})
