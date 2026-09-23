/*
 * Copyright 2025-2026 Nuo Shen, Nanjing University
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

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

#include "common/types.hpp"

namespace uemu {

// The virtual board: where every device sits and how it is parameterised.
// Emulator turns this into runtime devices, and a device tree generator can
// describe the same machine from the same values, so the defaults below are the
// board's single source of truth.
//
// Selected defaults are guest-visible: FdtGenerator describes the fixed
// topology, while PCI endpoints are discovered through ECAM. A device
// keeps what its own protocol defines (register offsets, bit fields, virtio
// constants) and only reads its placement and parameters from here.
//
// DRAM itself lives at the architectural address core::Dram::DRAM_BASE, which
// is also the hart's reset PC, so the board only chooses its size.

struct DramConfig {
    size_t size = 512 * 1024 * 1024;
};

struct NS16550Config {
    addr_t base = 0x10000000; // uart@10000000, "ns16550a"
    size_t size = 0x100;
    uint32_t interrupt_id = 10;
    uint32_t clock_hz = 3686400;
    uint32_t reg_shift = 0;
    uint32_t reg_io_width = 1;
};

struct ClintConfig {
    addr_t base = 0x2000000; // clint@2000000, "riscv,clint0"
    size_t size = 0x10000;
    uint64_t freq_hz = 10000000; // timebase-frequency
};

struct PlicConfig {
    addr_t base = 0xc000000; // interrupt-controller@c000000, "riscv,plic0"
    size_t size = 0x1000000;
    uint32_t ndev = 31; // riscv,ndev
};

struct PFlashConfig {
    addr_t base = 0x20000000;      // flash@20000000, "cfi-flash"
    uint64_t sector_len = 0x10000; // 64 KiB sectors
    uint32_t num_blocks = 512;     // 512 * 64 KiB = 32 MiB
    std::filesystem::path image;   // empty: an erased (0xff) bank
};

struct DisplayConfig {
    // Only SimpleFB uses a fixed MMIO base; both displays use the initial
    // geometry. The SimpleFB window is width * height * SimpleFB::BPP.
    addr_t simple_fb_base =
        0x50000000; // frame-buffer@50000000, "simple-framebuffer"
    size_t width = 1024;
    size_t height = 768;
};

enum class DisplayDevice { SimpleFB, BochsDisplay };

struct VirtioBlkConfig {
    addr_t base = 0x10001000; // virtio_blk@10001000, "virtio,mmio"
    size_t size = 0x1000;
    uint32_t interrupt_id = 12;
    std::filesystem::path image; // empty: no block device
};

// Generic ECAM host bridge. These are board-level windows; PCI endpoint BARs
// are guest-programmable and deliberately do not have fixed board addresses.
struct PciHostConfig {
    addr_t ecam_base = 0x30000000; // pci@30000000, one ECAM bus
    size_t ecam_size = 0x100000;
    addr_t mmio_base = 0x60000000; // non-prefetchable 32-bit Memory window
    size_t mmio_size = 0x10000000;
};

struct GoldfishRtcConfig {
    addr_t base = 0x101000; // rtc@101000, "google,goldfish-rtc"
    size_t size = 0x100;
    uint32_t interrupt_id = 11;
};

struct GoldfishEventsConfig {
    addr_t base = 0x10002000; // events@10002000
    size_t size = 0x1000;
    uint32_t interrupt_id = 2;
    std::string device_name = "qwerty2"; // reported to the guest
};

struct GoldfishBatteryConfig {
    addr_t base = 0x10003000; // goldfish_battery@10003000
    size_t size = 0x1000;
    uint32_t interrupt_id = 3;
    uint32_t capacity = 96;
};

struct Bcm2835RngConfig {
    addr_t base = 0x10004000; // rng@10004000, "brcm,bcm2835-rng"
    size_t size = 0x10;
};

struct NemuConsoleConfig {
    addr_t base = 0x10008000; // Debug console, see NEMU
    size_t size = 8;
};

struct SiFiveTestConfig {
    addr_t base = 0x100000; // sifive_test@100000, "sifive,test1"
    size_t size = 0x1000;
};

struct TestIntrGenConfig {
    addr_t base = 0x40000000; // Sail-style interrupt generator, ACT tests only
    size_t size = 0x1000;
};

// The default uemu-ng board.
struct BoardConfig {
    DramConfig dram;
    NS16550Config uart;
    ClintConfig clint;
    PlicConfig plic;
    PFlashConfig flash0;
    PFlashConfig flash1;
    DisplayConfig framebuffer;
    DisplayDevice display_device = DisplayDevice::BochsDisplay;
    VirtioBlkConfig virtio_blk;
    PciHostConfig pci;
    GoldfishRtcConfig rtc;
    GoldfishEventsConfig input;
    GoldfishBatteryConfig battery;
    Bcm2835RngConfig rng;
    NemuConsoleConfig nemu_console;
    SiFiveTestConfig sifive_test;
    TestIntrGenConfig test_intr_gen;

    // flash0/flash1 are the two 32 MiB banks of one cfi-flash node: they share
    // the geometry above and differ only in the window the board wires them to.
    BoardConfig() { flash1.base = 0x22000000; }

    void set_display_device(std::string_view value) {
        if (value == "simple-fb")
            display_device = DisplayDevice::SimpleFB;
        else if (value == "bochs-display")
            display_device = DisplayDevice::BochsDisplay;
        else
            throw std::invalid_argument("unknown display device");
    }

    void set_display_size(std::string_view value) {
        const size_t separator = value.find('x');
        if (separator == std::string_view::npos)
            throw std::invalid_argument("display size must be WIDTHxHEIGHT");

        const auto parse_dimension = [](std::string_view text) {
            size_t dimension = 0;
            const auto [end, error] = std::from_chars(
                text.data(), text.data() + text.size(), dimension);
            if (error != std::errc{} || end != text.data() + text.size() ||
                dimension < 64 || dimension > 8192)
                throw std::invalid_argument(
                    "display dimensions must be 64..8192");
            return dimension;
        };

        const size_t width = parse_dimension(value.substr(0, separator));
        const size_t height = parse_dimension(value.substr(separator + 1));
        constexpr size_t MAX_BYTES = 64 * 1024 * 1024;
        constexpr size_t BYTES_PER_PIXEL = 4;
        if (width > MAX_BYTES / BYTES_PER_PIXEL / height)
            throw std::invalid_argument("display framebuffer exceeds 64 MiB");

        const size_t bytes = width * height * BYTES_PER_PIXEL;
        if (display_device == DisplayDevice::SimpleFB &&
            (bytes - 1 > std::numeric_limits<addr_t>::max() -
                             framebuffer.simple_fb_base ||
             framebuffer.simple_fb_base >= pci.mmio_base ||
             bytes > pci.mmio_base - framebuffer.simple_fb_base))
            throw std::invalid_argument(
                "display framebuffer overlaps the PCI MMIO window");

        framebuffer.width = width;
        framebuffer.height = height;
    }
};

} // namespace uemu
