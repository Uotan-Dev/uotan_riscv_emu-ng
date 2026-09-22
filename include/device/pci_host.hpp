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
#include <functional>
#include <memory>
#include <optional>

#include "board_config.hpp"
#include "device/device.hpp"

namespace uemu::device {

struct PciBdf {
    uint8_t device;
    uint8_t function;
};

struct PciFunctionInfo {
    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t revision = 0;
    uint8_t prog_if = 0;
    uint8_t subclass = 0;
    uint8_t class_code = 0;
};

class PciMemoryBar {
public:
    using Read = std::function<std::optional<uint64_t>(size_t, size_t)>;
    using Write = std::function<bool(size_t, size_t, uint64_t)>;

    PciMemoryBar(size_t size, Read read, Write write);

private:
    friend class PciFunction;
    friend class PciHost;

    size_t size_;
    Read read_;
    Write write_;
};

// A conventional Type-0 PCI function. It only exposes fields the emulator
// implements: identification, Memory Space Enable, and 32-bit Memory BARs.
class PciFunction {
public:
    explicit PciFunction(PciFunctionInfo info) : info_(info) {}

    void set_memory_bar(size_t index, PciMemoryBar bar);

private:
    friend class PciHost;

    struct Bar {
        PciMemoryBar handlers;
        uint32_t address = 0;
    };

    [[nodiscard]] uint8_t read_config_byte(size_t offset) const;
    void write_config(size_t offset, size_t size, uint64_t value);
    [[nodiscard]] uint32_t bar_value(size_t index) const;
    void write_bar(size_t index, uint32_t value);

    PciFunctionInfo info_;
    uint16_t command_ = 0;
    std::array<std::optional<Bar>, 6> bars_;
};

// A one-bus generic ECAM host. Its two ordinary MMIO windows are installed on
// core::Bus; the host owns config/BAR state and routes dynamic BAR mappings
// within its fixed board-defined Memory aperture.
class PciHost {
public:
    static constexpr size_t ECAM_BUS_SIZE = 1 * 1024 * 1024;

    explicit PciHost(const PciHostConfig& config);

    void register_function(PciBdf bdf, PciFunction function);

    [[nodiscard]] std::shared_ptr<Device> ecam_device() const {
        return ecam_device_;
    }

    [[nodiscard]] std::shared_ptr<Device> mmio_device() const {
        return mmio_device_;
    }

private:
    friend class PciEcamDevice;
    friend class PciMmioDevice;

    [[nodiscard]] std::optional<uint64_t> read_config(size_t offset,
                                                      size_t size) const;
    [[nodiscard]] bool write_config(size_t offset, size_t size, uint64_t value);
    [[nodiscard]] std::optional<uint64_t> read_mmio(addr_t address,
                                                    size_t size);
    [[nodiscard]] bool write_mmio(addr_t address, size_t size, uint64_t value);

    [[nodiscard]] PciFunction* function(PciBdf bdf);
    [[nodiscard]] const PciFunction* function(PciBdf bdf) const;
    [[nodiscard]] static size_t function_index(PciBdf bdf);

    PciHostConfig config_;
    std::array<std::optional<PciFunction>, 32 * 8> functions_;
    std::shared_ptr<Device> ecam_device_;
    std::shared_ptr<Device> mmio_device_;
};

} // namespace uemu::device
