/*
 * Copyright (C) 2015 The Android Open Source Project
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

//! \file

#ifndef TNT_FILAMENT_BACKEND_PIXELBUFFERDESCRIPTOR_H
#define TNT_FILAMENT_BACKEND_PIXELBUFFERDESCRIPTOR_H

#include <backend/BufferDescriptor.h>
#include <backend/DriverEnums.h>

#include <utils/compiler.h>
#include <utils/debug.h>

#include <stddef.h>
#include <stdint.h>

namespace utils::io {
class ostream;
} // namespace utils::io

namespace filament::backend {

/**
 * A descriptor to an image in main memory, typically used to transfer image data from the CPU
 * to the GPU.
 *
 * A PixelBufferDescriptor owns the memory buffer it references, therefore PixelBufferDescriptor
 * cannot be copied, but can be moved.
 *
 * PixelBufferDescriptor releases ownership of the memory-buffer when it's destroyed.
 */
class UTILS_PUBLIC PixelBufferDescriptor : public BufferDescriptor {
public:
    using PixelDataFormat = backend::PixelDataFormat;
    using PixelDataType = backend::PixelDataType;

    PixelBufferDescriptor() = default;

    /**
     * Creates a new PixelBufferDescriptor referencing an image in main memory
     *
     * @param buffer    Virtual address of the buffer containing the image
     * @param size      Size in bytes of the buffer containing the image
     * @param format    Format of the image pixels
     * @param type      Type of the image pixels
     * @param alignment Alignment in bytes of pixel rows
     * @param left      Left coordinate in pixels
     * @param top       Top coordinate in pixels
     * @param stride    Stride of a row in pixels
     * @param handler   Handler to dispatch the callback or nullptr for the default handler
     * @param callback  A callback used to release the CPU buffer
     * @param user      An opaque user pointer passed to the callback function when it's called
     */
    PixelBufferDescriptor(void const* buffer, size_t size,
            PixelDataFormat format, PixelDataType type, uint8_t alignment,
            uint32_t left, uint32_t top, uint32_t stride,
            CallbackHandler* handler, Callback callback, void* user = nullptr) noexcept
            : BufferDescriptor(buffer, size, handler, callback, user),
              left(left), top(top), stride(stride),
              format(format), type(type), alignment(alignment) {
    }

    PixelBufferDescriptor(void const* buffer, size_t size,
            PixelDataFormat format, PixelDataType type, uint8_t alignment = 1,
            uint32_t left = 0, uint32_t top = 0, uint32_t stride = 0,
            Callback callback = nullptr, void* user = nullptr) noexcept
            : BufferDescriptor(buffer, size, callback, user),
              left(left), top(top), stride(stride),
              format(format), type(type), alignment(alignment) {
    }

    /**
     * Creates a new PixelBufferDescriptor referencing an image in main memory
     *
     * @param buffer    Virtual address of the buffer containing the image
     * @param size      Size in bytes of the buffer containing the image
     * @param format    Format of the image pixels
     * @param type      Type of the image pixels
     * @param handler   Handler to dispatch the callback or nullptr for the default handler
     * @param callback  A callback used to release the CPU buffer
     * @param user      An opaque user pointer passed to the callback function when it's called
     */
    PixelBufferDescriptor(void const* buffer, size_t size,
            PixelDataFormat format, PixelDataType type,
            CallbackHandler* handler, Callback callback, void* user = nullptr) noexcept
            : BufferDescriptor(buffer, size, handler, callback, user),
              stride(0), format(format), type(type), alignment(1) {
    }

    PixelBufferDescriptor(void const* buffer, size_t size,
            PixelDataFormat format, PixelDataType type,
            Callback callback, void* user = nullptr) noexcept
            : BufferDescriptor(buffer, size, callback, user),
              stride(0), format(format), type(type), alignment(1) {
    }


    /**
     * Creates a new PixelBufferDescriptor referencing a compressed image in main memory
     *
     * @param buffer    Virtual address of the buffer containing the image
     * @param size      Size in bytes of the buffer containing the image
     * @param format    Compressed format of the image
     * @param imageSize Compressed size of the image
     * @param handler   Handler to dispatch the callback or nullptr for the default handler
     * @param callback  A callback used to release the CPU buffer
     * @param user      An opaque user pointer passed to the callback function when it's called
     */
    PixelBufferDescriptor(void const* buffer, size_t size,
            backend::CompressedPixelDataType format, uint32_t imageSize,
            CallbackHandler* handler, Callback callback, void* user = nullptr) noexcept
            : BufferDescriptor(buffer, size, handler, callback, user),
              imageSize(imageSize), compressedFormat(format), type(PixelDataType::COMPRESSED),
              alignment(1) {
    }

    PixelBufferDescriptor(void const* buffer, size_t size,
            backend::CompressedPixelDataType format, uint32_t imageSize,
            Callback callback, void* user = nullptr) noexcept
            : BufferDescriptor(buffer, size, callback, user),
              imageSize(imageSize), compressedFormat(format), type(PixelDataType::COMPRESSED),
              alignment(1) {
    }

    // --------------------------------------------------------------------------------------------

    template<typename T, void(T::*method)(void const*, size_t)>
    static PixelBufferDescriptor make(void const* buffer, size_t size,
            PixelDataFormat format, PixelDataType type, uint8_t alignment,
            uint32_t left, uint32_t top, uint32_t stride, T* data,
            CallbackHandler* handler = nullptr) noexcept {
        return { buffer, size, format, type, alignment, left, top, stride,
                handler, [](void* b, size_t s, void* u) {
                    (static_cast<T*>(u)->*method)(b, s); }, data };
    }

    template<typename T, void(T::*method)(void const*, size_t)>
    static PixelBufferDescriptor make(void const* buffer, size_t size,
            PixelDataFormat format, PixelDataType type, T* data,
            CallbackHandler* handler = nullptr) noexcept {
        return { buffer, size, format, type, handler, [](void* b, size_t s, void* u) {
                    (static_cast<T*>(u)->*method)(b, s); }, data };
    }

    template<typename T, void(T::*method)(void const*, size_t)>
    static PixelBufferDescriptor make(void const* buffer, size_t size,
            backend::CompressedPixelDataType format, uint32_t imageSize, T* data,
            CallbackHandler* handler = nullptr) noexcept {
        return { buffer, size, format, imageSize, handler, [](void* b, size_t s, void* u) {
                    (static_cast<T*>(u)->*method)(b, s); }, data
        };
    }

    template<typename T>
    static PixelBufferDescriptor make(void const* buffer, size_t size,
            PixelDataFormat format, PixelDataType type, uint8_t alignment,
            uint32_t left, uint32_t top, uint32_t stride, T&& functor,
            CallbackHandler* handler = nullptr) noexcept {
        return { buffer, size, format, type, alignment, left, top, stride,
                handler, [](void* b, size_t s, void* u) {
                    T* const that = static_cast<T*>(u);
                    that->operator()(b, s);
                    delete that;
                }, new T(std::forward<T>(functor))
        };
    }

    template<typename T>
    static PixelBufferDescriptor make(void const* buffer, size_t size,
            PixelDataFormat format, PixelDataType type, T&& functor,
            CallbackHandler* handler = nullptr) noexcept {
        return { buffer, size, format, type,
                 handler, [](void* b, size_t s, void* u) {
                    T* const that = static_cast<T*>(u);
                    that->operator()(b, s);
                    delete that;
                }, new T(std::forward<T>(functor))
        };
    }

    template<typename T>
    static PixelBufferDescriptor make(void const* buffer, size_t size,
            backend::CompressedPixelDataType format, uint32_t imageSize, T&& functor,
            CallbackHandler* handler = nullptr) noexcept {
        return { buffer, size, format, imageSize,
                 handler, [](void* b, size_t s, void* u) {
                    T* const that = static_cast<T*>(u);
                    that->operator()(b, s);
                    delete that;
                }, new T(std::forward<T>(functor))
        };
    }
    /**
     * Computes the size in bytes for a pixel of given dimensions and format
     *
     * @param format    Format of the image pixels
     * @param type      Type of the image pixels
     * @return The size of the specified pixel in bytes
     */

    static constexpr size_t computePixelSize(PixelDataFormat format, PixelDataType type) noexcept {
        if (type == PixelDataType::COMPRESSED) {
            return 0;
        }

        size_t n = 0;
        switch (format) {
            case PixelDataFormat::R:
            case PixelDataFormat::R_INTEGER:
            case PixelDataFormat::DEPTH_COMPONENT:
            case PixelDataFormat::ALPHA:
                n = 1;
                break;
            case PixelDataFormat::RG:
            case PixelDataFormat::RG_INTEGER:
            case PixelDataFormat::DEPTH_STENCIL:
                n = 2;
                break;
            case PixelDataFormat::RGB:
            case PixelDataFormat::RGB_INTEGER:
                n = 3;
                break;
            case PixelDataFormat::UNUSED:// shouldn't happen (used to be rgbm)
            case PixelDataFormat::RGBA:
            case PixelDataFormat::RGBA_INTEGER:
                n = 4;
                break;
        }

        size_t bpp = n;
        switch (type) {
            case PixelDataType::COMPRESSED:// Impossible -- to squash the IDE warnings
            case PixelDataType::UBYTE:
            case PixelDataType::BYTE:
                // nothing to do
                break;
            case PixelDataType::USHORT:
            case PixelDataType::SHORT:
            case PixelDataType::HALF:
                bpp *= 2;
                break;
            case PixelDataType::UINT:
            case PixelDataType::INT:
            case PixelDataType::FLOAT:
                bpp *= 4;
                break;
            case PixelDataType::UINT_10F_11F_11F_REV:
                // Special case, format must be RGB and uses 4 bytes
                assert_invariant(format == PixelDataFormat::RGB);
                bpp = 4;
                break;
            case PixelDataType::UINT_2_10_10_10_REV:
                // Special case, format must be RGBA and uses 4 bytes
                assert_invariant(format == PixelDataFormat::RGBA);
                bpp = 4;
                break;
            case PixelDataType::USHORT_565:
                // Special case, format must be RGB and uses 2 bytes
                assert_invariant(format == PixelDataFormat::RGB);
                bpp = 2;
                break;
        }
        return bpp;
    }

    // --------------------------------------------------------------------------------------------

    /**
     * Computes the size in bytes needed to fit an image of given dimensions and format
     *
     * @param format    Format of the image pixels
     * @param type      Type of the image pixels
     * @param stride    Stride of a row in pixels
     * @param height    Height of the image in rows
     * @param alignment Alignment in bytes of pixel rows
     * @return The buffer size needed to fit this image in bytes
     */
    static constexpr size_t computeDataSize(PixelDataFormat format, PixelDataType type,
            size_t stride, size_t height, size_t alignment) noexcept {
        assert_invariant(alignment);

        size_t bpp = computePixelSize(format, type);
        size_t const bpr = bpp * stride;
        size_t const bprAligned = (bpr + (alignment - 1)) & (~alignment + 1);
        return bprAligned * height;
    }

    //! left coordinate in pixels
    uint32_t left = 0;
    //! top coordinate in pixels
    uint32_t top = 0;
    union {
        struct {
            //! stride in pixels
            uint32_t stride;
            //! Pixel data format
            PixelDataFormat format;
        };
        struct {
            //! compressed image size
            uint32_t imageSize;
            //! compressed image format
            backend::CompressedPixelDataType compressedFormat;
        };
    };
    //! pixel data type
    PixelDataType type : 4;
    //! row alignment in bytes
    uint8_t alignment  : 4;
};

} // namespace backend::filament

#if !defined(NDEBUG)
utils::io::ostream& operator<<(utils::io::ostream& out, const filament::backend::PixelBufferDescriptor& b);
#endif

#endif // TNT_FILAMENT_BACKEND_PIXELBUFFERDESCRIPTOR_H
