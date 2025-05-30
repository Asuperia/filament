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

#ifndef TNT_FILAMENT_INDEXBUFFER_H
#define TNT_FILAMENT_INDEXBUFFER_H

#include <filament/FilamentAPI.h>

#include <backend/DriverEnums.h>

#include <backend/BufferDescriptor.h>

#include <utils/compiler.h>
#include <utils/StaticString.h>

#include <stdint.h>
#include <stddef.h>

namespace filament {

class FIndexBuffer;

class Engine;

/**
 * A buffer containing vertex indices into a VertexBuffer. Indices can be 16 or 32 bit.
 * The buffer itself is a GPU resource, therefore mutating the data can be relatively slow.
 * Typically these buffers are constant.
 *
 * It is possible, and even encouraged, to use a single index buffer for several Renderables.
 *
 * @see VertexBuffer, RenderableManager
 */
class UTILS_PUBLIC IndexBuffer : public FilamentAPI {
    struct BuilderDetails;

public:
    using BufferDescriptor = backend::BufferDescriptor;

    /**
     * Type of the index buffer
     */
    enum class IndexType : uint8_t {
        USHORT = uint8_t(backend::ElementType::USHORT),  //!< 16-bit indices
        UINT = uint8_t(backend::ElementType::UINT),      //!< 32-bit indices
    };

    class Builder : public BuilderBase<BuilderDetails>, public BuilderNameMixin<Builder> {
        friend struct BuilderDetails;
    public:
        Builder() noexcept;
        Builder(Builder const& rhs) noexcept;
        Builder(Builder&& rhs) noexcept;
        ~Builder() noexcept;
        Builder& operator=(Builder const& rhs) noexcept;
        Builder& operator=(Builder&& rhs) noexcept;

        /**
         * Size of the index buffer in elements.
         * @param indexCount Number of indices the IndexBuffer can hold.
         * @return A reference to this Builder for chaining calls.
         */
        Builder& indexCount(uint32_t indexCount) noexcept;

        /**
         * Type of the index buffer, 16-bit or 32-bit.
         * @param indexType Type of indices stored in the IndexBuffer.
         * @return A reference to this Builder for chaining calls.
         */
        Builder& bufferType(IndexType indexType) noexcept;

        /**
         * Associate an optional name with this IndexBuffer for debugging purposes.
         *
         * name will show in error messages and should be kept as short as possible. The name is
         * truncated to a maximum of 128 characters.
         *
         * The name string is copied during this method so clients may free its memory after
         * the function returns.
         *
         * @param name A string to identify this IndexBuffer
         * @param len Length of name, should be less than or equal to 128
         * @return This Builder, for chaining calls.
         * @deprecated Use name(utils::StaticString const&) instead.
         */
        UTILS_DEPRECATED
        Builder& name(const char* UTILS_NONNULL name, size_t len) noexcept;

        /**
         * Associate an optional name with this IndexBuffer for debugging purposes.
         *
         * name will show in error messages and should be kept as short as possible.
         *
         * @param name A string literal to identify this IndexBuffer
         * @return This Builder, for chaining calls.
         */
        Builder& name(utils::StaticString const& name) noexcept;

        /**
         * Creates the IndexBuffer object and returns a pointer to it. After creation, the index
         * buffer is uninitialized. Use IndexBuffer::setBuffer() to initialize the IndexBuffer.
         *
         * @param engine Reference to the filament::Engine to associate this IndexBuffer with.
         *
         * @return pointer to the newly created object.
         *
         * @exception utils::PostConditionPanic if a runtime error occurred, such as running out of
         *            memory or other resources.
         * @exception utils::PreConditionPanic if a parameter to a builder function was invalid.
         *
         * @see IndexBuffer::setBuffer
         */
        IndexBuffer* UTILS_NONNULL build(Engine& engine);
    private:
        friend class FIndexBuffer;
    };

    /**
     * Asynchronously copy-initializes a region of this IndexBuffer from the data provided.
     *
     * @param engine Reference to the filament::Engine to associate this IndexBuffer with.
     * @param buffer A BufferDescriptor representing the data used to initialize the IndexBuffer.
     *               BufferDescriptor points to raw, untyped data that will be interpreted as
     *               either 16-bit or 32-bits indices based on the Type of this IndexBuffer.
     * @param byteOffset Offset in *bytes* into the IndexBuffer. Must be multiple of 4.
     */
    void setBuffer(Engine& engine, BufferDescriptor&& buffer, uint32_t byteOffset = 0);

    /**
     * Returns the size of this IndexBuffer in elements.
     * @return The number of indices the IndexBuffer holds.
     */
    size_t getIndexCount() const noexcept;

protected:
    // prevent heap allocation
    ~IndexBuffer() = default;
};

} // namespace filament

#endif // TNT_FILAMENT_INDEXBUFFER_H
