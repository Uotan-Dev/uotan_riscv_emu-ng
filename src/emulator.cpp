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

#include <exception>
#include <limits>
#include <stdexcept>
#include <thread>

#include <libfdt.h>

#include "common/log.hpp"
#include "core/mmu.hpp"
#include "device/bcm2835_rng.hpp"
#include "device/clint.hpp"
#include "device/goldfish_battery.hpp"
#include "device/goldfish_events.hpp"
#include "device/goldfish_rtc.hpp"
#include "device/nemu_console.hpp"
#include "device/ns16550.hpp"
#include "device/pflash_cfi01.hpp"
#include "device/plic.hpp"
#include "device/sifive_test.hpp"
#include "device/simple_fb.hpp"
#include "device/test_intr_gen.hpp"
#include "device/virtio_blk.hpp"
#include "emulator.hpp"
#include "utils/elf_loader.hpp"
#include "utils/fileloader.hpp"

namespace uemu {

Emulator::Emulator(const BoardConfig& config)
    : dram_(std::make_shared<core::Dram>(config.dram.size)),
      hart_(std::make_shared<core::Hart>()),
      bus_(std::make_shared<core::Bus>(dram_)),
      mmu_(std::make_shared<core::MMU>(hart_.get(), bus_)),
      cpu_(*hart_, *mmu_, stop_source_), device_thread_(*bus_, stop_source_) {
    hart_->connect_mmu(mmu_.get());

    // Clint
    bus_->add_device(std::make_shared<device::Clint>(config.clint, hart_));

    // TestIntrGen — Sail-style simple interrupt generator for ACT tests
    bus_->add_device(
        std::make_shared<device::TestIntrGen>(config.test_intr_gen, hart_));

    // Plic
    auto plic = std::make_shared<device::Plic>(config.plic, hart_);
    bus_->add_device(plic);
    auto request_irq = [plic](uint32_t id, bool lvl) -> void {
        plic->set_interrupt_level(id, lvl);
    };

    // SiFiveTest
    bus_->add_device(std::make_shared<device::SiFiveTest>(
        config.sifive_test,
        [this](uint16_t code, device::SiFiveTest::Status status) -> void {
            halt_from_guest(code, static_cast<uint16_t>(status));
        }));

    // NS16550; console bytes reach the host through the frontend
    console_ = std::make_shared<device::NS16550>(config.uart, request_irq,
                                                 console_channel_);
    bus_->add_device(console_);

    // SimpleFB
    framebuffer_ = std::make_shared<device::SimpleFB>(config.framebuffer);
    bus_->add_device(framebuffer_);

    // VirtioBLK; absent unless the board names a disk image
    if (!config.virtio_blk.image.empty())
        bus_->add_device(std::make_shared<device::VirtioBlk>(
            config.virtio_blk, dram_, request_irq));

    // pflash_cfi01, the two banks of the one cfi-flash node
    bus_->add_device(std::make_shared<device::PFlashCFI01>(config.flash0));
    bus_->add_device(std::make_shared<device::PFlashCFI01>(config.flash1));

    // GoldfishEvents
    input_ =
        std::make_shared<device::GoldfishEvents>(config.input, request_irq);
    bus_->add_device(input_);

    // GoldfishRTC
    bus_->add_device(
        std::make_shared<device::GoldfishRTC>(config.rtc, request_irq));

    // GoldfishBattery
    bus_->add_device(
        std::make_shared<device::GoldfishBattery>(config.battery, request_irq));

    // BCM2835Rng
    bus_->add_device(std::make_shared<device::BCM2835Rng>(config.rng));

    // NemuConsole
    bus_->add_device(std::make_shared<device::NemuConsole>(config.nemu_console,
                                                           console_channel_));
}

Emulator::~Emulator() {
    // The workers must stop before the objects they reference are destroyed;
    // asking here also makes the shutdown explicit when the guest never halted.
    stop_source_.request_stop();
}

void Emulator::start() {
    if (started_)
        throw std::logic_error("Emulator::start() called twice");

    started_ = true;
    cpu_.start();

    try {
        device_thread_.start();
    } catch (...) {
        stop_source_.request_stop();
        static_cast<void>(cpu_.join());
        throw;
    }
}

void Emulator::run(std::chrono::milliseconds timeout) {
    start();

    const bool timed = timeout.count() > 0;
    const auto start_time = std::chrono::steady_clock::now();

    while (!finished()) {
        // Compare in the timeout's own unit: promoting the timeout to the
        // clock's finer duration would overflow for very large values.
        const auto elapsed =
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start_time);

        if (timed && elapsed >= timeout)
            break;

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    request_shutdown();
    wait();
}

void Emulator::request_shutdown() noexcept { stop_source_.request_stop(); }

bool Emulator::finished() const noexcept {
    return stop_source_.stop_requested();
}

void Emulator::wait() {
    std::exception_ptr cpu_error = cpu_.join();
    std::exception_ptr device_error = device_thread_.join();

    if (cpu_error)
        std::rethrow_exception(cpu_error);

    if (device_error)
        std::rethrow_exception(device_error);
}

void Emulator::halt_from_guest(uint16_t code, uint16_t status) noexcept {
    shutdown_code_ = code;
    shutdown_status_ = status;
    stop_source_.request_stop();
}

void Emulator::console_input(std::string_view bytes) {
    for (char byte : bytes)
        console_channel_.push_input(static_cast<uint8_t>(byte));
}

std::string Emulator::console_output() {
    return console_channel_.drain_output();
}

void Emulator::push_key_event(core::KeyEvent event) {
    input_->push_key_event(event);
}

core::Framebuffer& Emulator::framebuffer() noexcept { return *framebuffer_; }

void Emulator::load_elf(const std::filesystem::path& path) {
    utils::ElfLoadResult result = utils::ElfLoader::load(path, *dram_);

    hart_->pc = result.entry;
    loaded_ranges_.insert(loaded_ranges_.end(), result.loaded_ranges.begin(),
                          result.loaded_ranges.end());
    log::info("ELF loaded: {}\n"
              "      entry PC = 0x{:016x}",
              path.string(), result.entry);
}

addr_t Emulator::find_fdt_address(size_t size) const {
    // Firmware likes the device tree high in DRAM and 2 MiB aligned; QEMU's
    // riscv virt machine places it the same way, and OpenSBI knows how to keep
    // it out of the way once it has received it in a1.
    constexpr addr_t FDT_ALIGNMENT = 2 * 1024 * 1024;
    constexpr addr_t FDT_ALIGNMENT_MASK = FDT_ALIGNMENT - 1;
    constexpr addr_t dram_begin = core::Dram::DRAM_BASE;

    if (dram_->size() > std::numeric_limits<addr_t>::max() - dram_begin)
        throw std::overflow_error("DRAM address range overflows");

    const addr_t dram_end = dram_begin + dram_->size();
    if (size == 0 || size > dram_->size())
        throw std::runtime_error("DTB does not fit in DRAM");

    // Walk downwards: try the highest aligned slot first, and when it collides
    // retry just below the lowest range it hit.  Ranges being moved past can
    // never hold the blob (the placement overlapped them, so the free space
    // between two of them is necessarily smaller than the blob), so the search
    // cannot skip a slot that would have fit, and the ceiling strictly
    // decreases, so it terminates.
    addr_t ceiling = dram_end;
    while (ceiling >= dram_begin && size <= ceiling - dram_begin) {
        const addr_t candidate = (ceiling - size) & ~FDT_ALIGNMENT_MASK;
        if (candidate < dram_begin)
            break;

        const AddressRange placement{candidate, candidate + size};
        const AddressRange* collision = nullptr;
        for (const AddressRange& range : loaded_ranges_) {
            if (placement.overlaps(range) &&
                (!collision || range.begin < collision->begin))
                collision = &range;
        }

        if (!collision)
            return candidate;

        ceiling = collision->begin;
    }

    throw std::runtime_error("No free DRAM region is available for the DTB");
}

addr_t Emulator::install_fdt(std::span<const uint8_t> blob) {
    // Device trees are written once, before the hart starts: the firmware
    // receives the address in a1 and owns it from then on.  Call this after
    // every guest image has been loaded so the placement can avoid them.
    if (started_)
        throw std::logic_error("Cannot install a DTB after execution starts");
    if (fdt_installed_)
        throw std::logic_error("A DTB has already been installed");
    if (blob.size() < sizeof(fdt_header) ||
        fdt_check_full(blob.data(), blob.size()) != 0)
        throw std::runtime_error("Invalid DTB");

    const size_t total_size = fdt_totalsize(blob.data());
    if (total_size == 0 || total_size > blob.size())
        throw std::runtime_error("Truncated DTB");

    const addr_t address = find_fdt_address(total_size);
    dram_->write_bytes(address, blob.data(), total_size);
    // a1 is the flattened device tree address; a0 is still 0, the boot hart id.
    hart_->gprs.write(11, address);
    fdt_installed_ = true;

    log::info("DTB installed: {} bytes at 0x{:016x}", total_size, address);
    return address;
}

void Emulator::load(addr_t addr, const void* p, size_t n) {
    if (!p)
        throw std::invalid_argument("p is nullptr");

    if (n == 0)
        return;

    dram_->write_bytes(addr, p, n);

    // Everything written here is guest state, so the device tree must avoid it
    // too.  Recording happens after the write so a rejected range leaves the
    // list untouched.
    loaded_ranges_.push_back({addr, addr + n});
}

void Emulator::read(addr_t addr, void* p, size_t n) const {
    if (!p)
        throw std::invalid_argument("p is nullptr");

    if (n == 0)
        return;

    dram_->read_bytes(addr, p, n);
}

void Emulator::load(addr_t addr, const std::filesystem::path& path) {
    auto data = utils::FileLoader::read_file(path);

    if (!data.empty())
        load(addr, data.data(), data.size());
}

} // namespace uemu
