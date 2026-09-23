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

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>
#include <vector>

#include "board_config.hpp"
#include "core/framebuffer.hpp"
#include "device/pci_host.hpp"

namespace uemu::device {

// QEMU/Bochs DISPI, without legacy VGA or optional QEMU extensions. The PCI
// host owns the programmable BAR addresses; this device owns only their data.
class BochsDisplay final : public core::Framebuffer {
public:
    static constexpr size_t VRAM_SIZE = 64 * 1024 * 1024;
    static constexpr size_t MMIO_SIZE = 0x1000;

    explicit BochsDisplay(const DisplayConfig& initial_mode);

    [[nodiscard]] PciFunction pci_function();

    [[nodiscard]] core::FramebufferGeometry geometry() const noexcept override {
        return geometry_;
    }

    [[nodiscard]] std::string_view display_name() const noexcept override {
        return "Bochs Display";
    }

    [[nodiscard]] std::unique_lock<std::mutex> lock() const override {
        return std::unique_lock(mutex_);
    }

    [[nodiscard]] const uint8_t* pixels() const override {
        return vram_.data() + scanout_offset_;
    }

private:
    [[nodiscard]] std::optional<uint64_t> read_vram(size_t offset,
                                                    size_t size) const;
    [[nodiscard]] bool write_vram(size_t offset, size_t size, uint64_t value);
    [[nodiscard]] std::optional<uint64_t> read_mmio(size_t offset,
                                                    size_t size) const;
    [[nodiscard]] bool write_mmio(size_t offset, size_t size, uint64_t value);
    [[nodiscard]] uint16_t read_register(size_t index) const;
    void write_register(size_t index, uint16_t value);
    void update_scanout();

    mutable std::mutex mutex_;
    std::vector<uint8_t> vram_;
    std::array<uint16_t, 10> registers_{};
    core::FramebufferGeometry geometry_;
    size_t scanout_offset_ = 0;
};

} // namespace uemu::device
