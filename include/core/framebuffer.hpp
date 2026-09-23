/*
 * Copyright 2026 Nuo Shen, Nanjing University
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string_view>

namespace uemu::core {

struct FramebufferGeometry {
    size_t width;
    size_t height;
    size_t stride; // Bytes per row, including any padding.

    // Implementations keep stride * height within the pixel buffer.
    [[nodiscard]] size_t byte_size() const noexcept { return stride * height; }
};

// Read-only view of the guest framebuffer for the host frontend. Geometry and
// pixels must be read under the same lock so a frame sees one display mode.
class Framebuffer {
public:
    virtual ~Framebuffer() = default;

    [[nodiscard]] virtual FramebufferGeometry geometry() const noexcept = 0;
    [[nodiscard]] virtual std::string_view display_name() const noexcept = 0;

    [[nodiscard]] virtual std::unique_lock<std::mutex> lock() const = 0;
    [[nodiscard]] virtual const uint8_t* pixels() const = 0;
};

} // namespace uemu::core
