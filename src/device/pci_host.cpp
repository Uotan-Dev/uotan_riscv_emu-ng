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

#include <bit>
#include <limits>
#include <stdexcept>
#include <utility>

#include <linux/pci_regs.h>

#include "device/pci_host.hpp"

namespace uemu::device {

namespace {

constexpr uint16_t QEMU_PCI_HOST_VENDOR_ID = 0x1b36;
constexpr uint16_t QEMU_PCI_HOST_DEVICE_ID = 0x0008;
constexpr uint8_t PCI_CLASS_BRIDGE_HOST = 0x06;
constexpr size_t PCI_CONFIG_SPACE_SIZE = PCI_CFG_SPACE_SIZE;

bool valid_access_size(size_t size) {
    return size == 1 || size == 2 || size == 4;
}

uint64_t all_ones(size_t size) {
    if (size >= sizeof(uint64_t))
        return std::numeric_limits<uint64_t>::max();
    return (uint64_t{1} << (size * 8)) - 1;
}

uint32_t replace_bytes(uint32_t old_value, size_t register_offset,
                       size_t write_offset, size_t write_size,
                       uint64_t write_value) {
    uint32_t updated = old_value;
    for (size_t byte = 0; byte < write_size; ++byte) {
        const size_t offset = write_offset + byte;
        if (offset < register_offset || offset >= register_offset + 4)
            continue;

        const size_t shift = (offset - register_offset) * 8;
        const uint32_t value =
            static_cast<uint32_t>((write_value >> (byte * 8)) & 0xff);
        updated = (updated & ~(uint32_t{0xff} << shift)) | (value << shift);
    }
    return updated;
}

} // namespace

class PciEcamDevice final : public Device {
public:
    PciEcamDevice(PciHost& host, const PciHostConfig& config)
        : Device("PCI ECAM", config.ecam_base, config.ecam_size), host_(host) {}

private:
    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override {
        return host_.read_config(offset, size);
    }

    bool write_internal(addr_t offset, size_t size, uint64_t value) override {
        return host_.write_config(offset, size, value);
    }

    PciHost& host_;
};

class PciMmioDevice final : public Device {
public:
    PciMmioDevice(PciHost& host, const PciHostConfig& config)
        : Device("PCI MMIO", config.mmio_base, config.mmio_size), host_(host),
          base_(config.mmio_base) {}

private:
    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override {
        return host_.read_mmio(base_ + offset, size);
    }

    bool write_internal(addr_t offset, size_t size, uint64_t value) override {
        return host_.write_mmio(base_ + offset, size, value);
    }

    PciHost& host_;
    addr_t base_;
};

PciMemoryBar::PciMemoryBar(size_t size, Read read, Write write)
    : size_(size), read_(std::move(read)), write_(std::move(write)) {
    if (size_ < 16 || !std::has_single_bit(size_))
        throw std::invalid_argument(
            "PCI Memory BAR size must be a power of two of at least 16 bytes");
    if (size_ >= uint64_t{1} << 32)
        throw std::invalid_argument("PCI 32-bit Memory BAR is too large");
    if (!read_ || !write_)
        throw std::invalid_argument(
            "PCI Memory BAR needs read and write handlers");
}

void PciFunction::set_memory_bar(size_t index, PciMemoryBar bar) {
    if (index >= bars_.size())
        throw std::out_of_range("PCI BAR index is out of range");
    if (bars_[index])
        throw std::logic_error("PCI BAR is already registered");

    bars_[index].emplace(Bar{.handlers = std::move(bar)});
}

uint8_t PciFunction::read_config_byte(size_t offset) const {
    const auto byte = [offset](uint32_t value, size_t register_offset) {
        return static_cast<uint8_t>(value >> ((offset - register_offset) * 8));
    };

    if (offset < 2)
        return byte(info_.vendor_id, PCI_VENDOR_ID);
    if (offset < 4)
        return byte(info_.device_id, PCI_DEVICE_ID);
    if (offset < 6)
        return byte(command_, PCI_COMMAND);
    if (offset == PCI_REVISION_ID)
        return info_.revision;
    if (offset == PCI_CLASS_PROG)
        return info_.prog_if;
    if (offset == PCI_CLASS_DEVICE)
        return info_.subclass;
    if (offset == PCI_CLASS_DEVICE + 1)
        return info_.class_code;
    if (offset == PCI_HEADER_TYPE)
        return PCI_HEADER_TYPE_NORMAL;

    if (offset >= PCI_BASE_ADDRESS_0 &&
        offset < PCI_BASE_ADDRESS_0 + bars_.size() * sizeof(uint32_t)) {
        const size_t index = (offset - PCI_BASE_ADDRESS_0) / sizeof(uint32_t);
        return byte(bar_value(index),
                    PCI_BASE_ADDRESS_0 + index * sizeof(uint32_t));
    }

    return 0;
}

void PciFunction::write_config(size_t offset, size_t size, uint64_t value) {
    const uint32_t command =
        replace_bytes(command_, PCI_COMMAND, offset, size, value);
    command_ = static_cast<uint16_t>(command & PCI_COMMAND_MEMORY);

    for (size_t index = 0; index < bars_.size(); ++index) {
        if (!bars_[index])
            continue;

        const size_t bar_offset = PCI_BASE_ADDRESS_0 + index * sizeof(uint32_t);
        const uint32_t before = bar_value(index);
        const uint32_t after =
            replace_bytes(before, bar_offset, offset, size, value);
        if (after != before)
            write_bar(index, after);
    }
}

uint32_t PciFunction::bar_value(size_t index) const {
    if (!bars_[index])
        return 0;
    return bars_[index]->address;
}

void PciFunction::write_bar(size_t index, uint32_t value) {
    const size_t size = bars_[index]->handlers.size_;
    const uint32_t address_mask =
        static_cast<uint32_t>(~(size - 1)) & PCI_BASE_ADDRESS_MEM_MASK;
    bars_[index]->address = value & address_mask;
}

PciHost::PciHost(const PciHostConfig& config) : config_(config) {
    if (config_.ecam_size != ECAM_BUS_SIZE ||
        config_.ecam_base % ECAM_BUS_SIZE != 0 ||
        config_.ecam_base >
            std::numeric_limits<addr_t>::max() - ECAM_BUS_SIZE + 1) {
        throw std::invalid_argument(
            "PCI ECAM must be a 1 MiB-aligned one-bus window");
    }
    if (config_.mmio_size == 0 ||
        config_.mmio_base > std::numeric_limits<uint32_t>::max() ||
        config_.mmio_size > (uint64_t{1} << 32) - config_.mmio_base) {
        throw std::invalid_argument(
            "PCI MMIO aperture must fit in the 32-bit address space");
    }

    functions_[0].emplace(PciFunctionInfo{.vendor_id = QEMU_PCI_HOST_VENDOR_ID,
                                          .device_id = QEMU_PCI_HOST_DEVICE_ID,
                                          .class_code = PCI_CLASS_BRIDGE_HOST});
    ecam_device_ = std::make_shared<PciEcamDevice>(*this, config_);
    mmio_device_ = std::make_shared<PciMmioDevice>(*this, config_);
}

void PciHost::register_function(PciBdf bdf, PciFunction function) {
    if (bdf.device == 0 && bdf.function == 0)
        throw std::invalid_argument("PCI host bridge occupies 00:00.0");

    const size_t index = function_index(bdf);
    if (functions_[index])
        throw std::logic_error("PCI function is already registered");
    functions_[index].emplace(std::move(function));
}

std::optional<uint64_t> PciHost::read_config(size_t offset, size_t size) const {
    if (!valid_access_size(size))
        return std::nullopt;

    const PciBdf bdf{.device = static_cast<uint8_t>((offset >> 15) & 0x1f),
                     .function = static_cast<uint8_t>((offset >> 12) & 0x07)};
    const size_t register_offset = offset & 0xfff;
    const PciFunction* pci_function = function(bdf);
    if (!pci_function)
        return all_ones(size);

    uint64_t value = 0;
    for (size_t byte = 0; byte < size; ++byte) {
        const size_t current = register_offset + byte;
        const uint8_t data = current < PCI_CONFIG_SPACE_SIZE
                                 ? pci_function->read_config_byte(current)
                                 : 0xff;
        value |= static_cast<uint64_t>(data) << (byte * 8);
    }
    return value;
}

bool PciHost::write_config(size_t offset, size_t size, uint64_t value) {
    if (!valid_access_size(size))
        return false;

    const PciBdf bdf{.device = static_cast<uint8_t>((offset >> 15) & 0x1f),
                     .function = static_cast<uint8_t>((offset >> 12) & 0x07)};
    const size_t register_offset = offset & 0xfff;
    PciFunction* pci_function = function(bdf);
    if (pci_function && register_offset < PCI_CONFIG_SPACE_SIZE)
        pci_function->write_config(register_offset, size, value);
    return true;
}

std::optional<uint64_t> PciHost::read_mmio(addr_t address, size_t size) {
    for (auto& pci_function : functions_) {
        if (!pci_function || !(pci_function->command_ & PCI_COMMAND_MEMORY))
            continue;

        for (auto& bar : pci_function->bars_) {
            if (!bar || size > bar->handlers.size_ ||
                bar->address < config_.mmio_base ||
                bar->handlers.size_ > config_.mmio_size ||
                bar->address - config_.mmio_base >
                    config_.mmio_size - bar->handlers.size_ ||
                address < bar->address ||
                address - bar->address > bar->handlers.size_ - size)
                continue;

            return bar->handlers.read_(address - bar->address, size);
        }
    }
    // Like QEMU GPEX, CPU accesses to an uncovered part of the advertised
    // PCI Memory window are benign: reads return all ones and writes vanish.
    return all_ones(size);
}

bool PciHost::write_mmio(addr_t address, size_t size, uint64_t value) {
    for (auto& pci_function : functions_) {
        if (!pci_function || !(pci_function->command_ & PCI_COMMAND_MEMORY))
            continue;

        for (auto& bar : pci_function->bars_) {
            if (!bar || size > bar->handlers.size_ ||
                bar->address < config_.mmio_base ||
                bar->handlers.size_ > config_.mmio_size ||
                bar->address - config_.mmio_base >
                    config_.mmio_size - bar->handlers.size_ ||
                address < bar->address ||
                address - bar->address > bar->handlers.size_ - size)
                continue;

            return bar->handlers.write_(address - bar->address, size, value);
        }
    }
    return true;
}

PciFunction* PciHost::function(PciBdf bdf) {
    const size_t index = function_index(bdf);
    return functions_[index] ? &*functions_[index] : nullptr;
}

const PciFunction* PciHost::function(PciBdf bdf) const {
    const size_t index = function_index(bdf);
    return functions_[index] ? &*functions_[index] : nullptr;
}

size_t PciHost::function_index(PciBdf bdf) {
    if (bdf.device >= 32 || bdf.function >= 8)
        throw std::out_of_range("PCI BDF is outside bus 0");
    return static_cast<size_t>(bdf.device) * 8 + bdf.function;
}

} // namespace uemu::device
