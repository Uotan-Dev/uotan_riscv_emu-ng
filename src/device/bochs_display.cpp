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

#include <algorithm>
#include <stdexcept>

#include "device/bochs_display.hpp"

namespace uemu::device {

namespace {
constexpr size_t DISPI_OFFSET = 0x500;
constexpr size_t ID = 0;
constexpr size_t XRES = 1;
constexpr size_t YRES = 2;
constexpr size_t BPP = 3;
constexpr size_t ENABLE = 4;
constexpr size_t VIRT_WIDTH = 6;
constexpr size_t X_OFFSET = 8;
constexpr size_t Y_OFFSET = 9;
constexpr size_t VIDEO_MEMORY_64K = 10;
constexpr uint16_t DISPI_ID5 = 0xb0c5;
constexpr uint16_t ENABLED = 0x01;
constexpr uint16_t GETCAPS = 0x02;
constexpr uint16_t NOCLEARMEM = 0x80;
constexpr size_t MAX_DIMENSION = 8192;

bool valid_access(size_t offset, size_t size, size_t limit) {
    return (size == 1 || size == 2 || size == 4 || size == 8) &&
           offset <= limit && size <= limit - offset;
}
} // namespace

BochsDisplay::BochsDisplay(const DisplayConfig& initial_mode)
    : vram_(VRAM_SIZE), geometry_{.width = initial_mode.width,
                                  .height = initial_mode.height,
                                  .stride = initial_mode.width * 4} {
    if (initial_mode.width < 64 || initial_mode.width > MAX_DIMENSION ||
        initial_mode.height < 64 || initial_mode.height > MAX_DIMENSION ||
        initial_mode.width > VRAM_SIZE / 4 / initial_mode.height)
        throw std::invalid_argument("Bochs initial mode exceeds VRAM");

    registers_[XRES] = static_cast<uint16_t>(initial_mode.width);
    registers_[YRES] = static_cast<uint16_t>(initial_mode.height);
    registers_[BPP] = 32;
    registers_[ENABLE] = ENABLED;
    registers_[VIRT_WIDTH] = static_cast<uint16_t>(initial_mode.width);
}

PciFunction BochsDisplay::pci_function() {
    // Display Other avoids advertising legacy VGA I/O. Revision 1 avoids
    // advertising the revision-2 QEMU byte-order extension at BAR2+0x600.
    PciFunction function({.vendor_id = 0x1234,
                          .device_id = 0x1111,
                          .revision = 1,
                          .subclass = 0x80,
                          .class_code = 0x03});
    function.set_memory_bar(
        0, PciMemoryBar(
               VRAM_SIZE,
               [this](size_t offset, size_t size) {
                   return read_vram(offset, size);
               },
               [this](size_t offset, size_t size, uint64_t value) {
                   return write_vram(offset, size, value);
               }));
    function.set_memory_bar(
        2, PciMemoryBar(
               MMIO_SIZE,
               [this](size_t offset, size_t size) {
                   return read_mmio(offset, size);
               },
               [this](size_t offset, size_t size, uint64_t value) {
                   return write_mmio(offset, size, value);
               }));
    return function;
}

std::optional<uint64_t> BochsDisplay::read_vram(size_t offset,
                                                size_t size) const {
    if (!valid_access(offset, size, vram_.size()))
        return std::nullopt;
    std::scoped_lock lock(mutex_);
    uint64_t value = 0;
    for (size_t byte = 0; byte < size; ++byte)
        value |= uint64_t{vram_[offset + byte]} << (byte * 8);
    return value;
}

bool BochsDisplay::write_vram(size_t offset, size_t size, uint64_t value) {
    if (!valid_access(offset, size, vram_.size()))
        return false;
    std::scoped_lock lock(mutex_);
    for (size_t byte = 0; byte < size; ++byte)
        vram_[offset + byte] = static_cast<uint8_t>(value >> (byte * 8));
    return true;
}

uint16_t BochsDisplay::read_register(size_t index) const {
    if (index == ID)
        return DISPI_ID5;
    if (index == VIDEO_MEMORY_64K)
        return VRAM_SIZE / (64 * 1024);
    if (index >= registers_.size())
        return 0xffff;
    if (registers_[ENABLE] & GETCAPS) {
        if (index == XRES || index == YRES)
            return MAX_DIMENSION;
        if (index == BPP)
            return 32;
    }
    return registers_[index];
}

std::optional<uint64_t> BochsDisplay::read_mmio(size_t offset,
                                                size_t size) const {
    if (!valid_access(offset, size, MMIO_SIZE) || size == 8)
        return std::nullopt;
    std::scoped_lock lock(mutex_);
    uint64_t value = 0;
    for (size_t byte = 0; byte < size; ++byte) {
        const size_t address = offset + byte;
        uint8_t data = 0xff;
        if (address >= DISPI_OFFSET &&
            address < DISPI_OFFSET + (VIDEO_MEMORY_64K + 1) * 2) {
            const uint16_t reg = read_register((address - DISPI_OFFSET) / 2);
            data = static_cast<uint8_t>(reg >> ((address & 1) * 8));
        }
        value |= uint64_t{data} << (byte * 8);
    }
    return value;
}

void BochsDisplay::update_scanout() {
    if (!(registers_[ENABLE] & ENABLED) || registers_[BPP] != 32)
        return;

    const size_t width = registers_[XRES];
    const size_t height = registers_[YRES];
    const size_t virtual_width =
        std::max<size_t>(width, registers_[VIRT_WIDTH]);
    if (width < 64 || height < 64 || width > MAX_DIMENSION ||
        height > MAX_DIMENSION || virtual_width > MAX_DIMENSION)
        return;

    const size_t stride = virtual_width * 4;
    const size_t offset = size_t{registers_[Y_OFFSET]} * stride +
                          size_t{registers_[X_OFFSET]} * 4;
    if (offset > VRAM_SIZE || stride > (VRAM_SIZE - offset) / height)
        return;

    geometry_ = {.width = width, .height = height, .stride = stride};
    scanout_offset_ = offset;
}

void BochsDisplay::write_register(size_t index, uint16_t value) {
    if (index == ID || index == VIDEO_MEMORY_64K || index >= registers_.size())
        return;
    if (index == BPP && value != 32)
        return;
    if ((index == XRES || index == YRES || index == BPP) &&
        (registers_[ENABLE] & ENABLED))
        return;
    if (index == ENABLE) {
        const bool was_enabled = registers_[ENABLE] & ENABLED;
        registers_[ENABLE] = value & (ENABLED | GETCAPS | 0x40 | NOCLEARMEM);
        if (!was_enabled && (registers_[ENABLE] & ENABLED) &&
            !(registers_[ENABLE] & NOCLEARMEM))
            std::fill(vram_.begin(), vram_.end(), 0);
    } else {
        registers_[index] = value;
    }
    update_scanout();
}

bool BochsDisplay::write_mmio(size_t offset, size_t size, uint64_t value) {
    if (!valid_access(offset, size, MMIO_SIZE) || size == 8)
        return false;
    std::scoped_lock lock(mutex_);
    // BAR2+0x400 is the unimplemented VGA compatibility window: Linux and
    // EDK2 issue harmless blank/unblank accesses there, without VGA legacy.
    const size_t begin = std::max(offset, DISPI_OFFSET);
    const size_t end = std::min(offset + size, DISPI_OFFSET + 20);
    if (begin >= end)
        return true;

    const size_t first = (begin - DISPI_OFFSET) / 2;
    const size_t last = (end - 1 - DISPI_OFFSET) / 2;
    for (size_t index = first; index <= last; ++index) {
        uint16_t updated = registers_[index];
        for (size_t byte = 0; byte < size; ++byte) {
            const size_t address = offset + byte;
            if (address < DISPI_OFFSET + index * 2 ||
                address >= DISPI_OFFSET + (index + 1) * 2)
                continue;
            const size_t shift = (address & 1) * 8;
            updated = (updated & ~(uint16_t{0xff} << shift)) |
                      (static_cast<uint16_t>(value >> (byte * 8)) & 0xff)
                          << shift;
        }
        write_register(index, updated);
    }
    return true;
}

} // namespace uemu::device
