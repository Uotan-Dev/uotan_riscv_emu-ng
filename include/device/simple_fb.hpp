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

#include "board_config.hpp"
#include "core/framebuffer.hpp"
#include "device/device.hpp"

namespace uemu::device {

class SimpleFB : public Device, public core::Framebuffer {
public:
    static constexpr size_t BPP = 4; // Bytes per pixel (32-bit color)

    // The framebuffer is its geometry: the address window is
    // width * height * BPP, with no separately reserved size.
    explicit SimpleFB(const SimpleFBConfig& config)
        : Device("SimpleFB", config.base, config.width * config.height * BPP),
          width_(config.width), height_(config.height) {
        vram_.resize(size());
    }

    core::FramebufferGeometry geometry() const noexcept override {
        return {.width = width_, .height = height_, .stride = width_ * BPP};
    }

    std::string_view display_name() const noexcept override { return name_; }

    const uint8_t* pixels() const override { return vram_.data(); }

    [[nodiscard]] std::unique_lock<std::mutex> lock() const override {
        return std::unique_lock<std::mutex>(simple_fb_mutex_);
    }

private:
    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override;
    bool write_internal(addr_t offset, size_t size, uint64_t value) override;

    const size_t width_;
    const size_t height_;

    mutable std::mutex simple_fb_mutex_;
    std::vector<uint8_t> vram_;
};

} // namespace uemu::device
