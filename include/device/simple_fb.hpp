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

#include "device/device.hpp"
#include "ui/pixel_source.hpp"

namespace uemu::device {

class SimpleFB : public Device, public ui::PixelSource {
public:
    static constexpr size_t DEFAULT_WIDTH = 1024;
    static constexpr size_t DEFAULT_HEIGHT = 768;
    static constexpr size_t BPP = 4; // Bytes per pixel (32-bit color)

    static constexpr addr_t DEFAULT_BASE = 0x50000000;
    static constexpr size_t SIZE = DEFAULT_WIDTH * DEFAULT_HEIGHT * BPP;

    SimpleFB() : Device("SimpleFB", DEFAULT_BASE, SIZE) { vram_.resize(SIZE); }

    size_t get_width() const override { return DEFAULT_WIDTH; }

    size_t get_height() const override { return DEFAULT_HEIGHT; }

    size_t get_size() const override {
        return DEFAULT_WIDTH * DEFAULT_HEIGHT * BPP;
    }

    const uint8_t* get_pixels() const override { return vram_.data(); }

    [[nodiscard]] std::unique_lock<std::mutex> acquire_lock() const override {
        return std::unique_lock<std::mutex>(simple_fb_mutex_);
    }

private:
    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override;
    bool write_internal(addr_t offset, size_t size, uint64_t value) override;

    mutable std::mutex simple_fb_mutex_;
    std::vector<uint8_t> vram_;
};

} // namespace uemu::device
