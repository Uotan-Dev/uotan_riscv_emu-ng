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

#include <cstdint>
#include <format>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "core/dram.hpp"
#include "device/sifive_test.hpp"
#include "device/simple_fb.hpp"
#include "fdt_generator.hpp"
#include "utils/fdt.hpp"

namespace uemu {

namespace {

// Phandles are part of the guest-visible machine ABI: keep them stable.
constexpr uint32_t CPU_PHANDLE = 1;
constexpr uint32_t CPU_INTC_PHANDLE = 2;
constexpr uint32_t PLIC_PHANDLE = 3;
constexpr uint32_t SIFIVE_TEST_PHANDLE = 4;

// Interrupt numbers the tree names for the two interrupt controllers: the
// machine and supervisor external interrupts the PLIC raises on the hart
// contexts, and the software and timer interrupts the CLINT raises through the
// hart's own interrupt controller.
constexpr uint32_t IRQ_M_SOFT = 3;
constexpr uint32_t IRQ_M_TIMER = 7;
constexpr uint32_t IRQ_S_EXT = 9;
constexpr uint32_t IRQ_M_EXT = 11;

// "uart@10000000" for the node sitting at `address` in `parent`, e.g.
// "/soc/uart@10000000" or, for a direct child of the root, "/memory@80000000".
std::string at(std::string_view parent, std::string_view name, addr_t address) {
    const std::string_view prefix = parent == "/" ? std::string_view() : parent;
    return std::format("{}/{}@{:x}", prefix, name, address);
}

uint64_t checked_multiply(uint64_t lhs, uint64_t rhs,
                          std::string_view description) {
    if (lhs != 0 && rhs > std::numeric_limits<uint64_t>::max() / lhs)
        throw std::overflow_error(std::string(description) + " is too large");
    return lhs * rhs;
}

// Every node carrying a reg is a child of the root or of /soc, and both declare
// two address cells and two size cells, so one 64-bit address and one 64-bit
// size are four cells.
void set_reg(utils::FdtBuilder& fdt, std::string_view path, uint64_t address,
             uint64_t size) {
    fdt.set_cells(path, "reg",
                  {utils::upper_cell(address), utils::lower_cell(address),
                   utils::upper_cell(size), utils::lower_cell(size)});
}

} // namespace

std::vector<uint8_t> FdtGenerator::generate() const {
    utils::FdtBuilder fdt;

    if (config_.clint.freq_hz > std::numeric_limits<uint32_t>::max())
        throw std::overflow_error("timebase frequency does not fit the FDT");

    // Root: two cells per address and size, like QEMU's riscv virt machine.
    fdt.set_u32("/", "#address-cells", 2);
    fdt.set_u32("/", "#size-cells", 2);
    fdt.set_string("/", "compatible", "riscv-virt");
    fdt.set_string("/", "model", "uemu-ng");
    // Firmware reads the running emulator's version from the machine
    // description, so it comes from the build rather than a second constant.
    fdt.set_string("/", "uotan,emulator-version", UEMU_VERSION);

    // CPUs: the single hart and its own interrupt controller.
    fdt.add_node("/cpus");
    fdt.set_u32("/cpus", "#address-cells", 1);
    fdt.set_u32("/cpus", "#size-cells", 0);
    fdt.set_u32("/cpus", "timebase-frequency",
                static_cast<uint32_t>(config_.clint.freq_hz));

    fdt.add_node("/cpus/cpu-map");
    fdt.add_node("/cpus/cpu-map/cluster0");
    fdt.add_node("/cpus/cpu-map/cluster0/core0");
    fdt.set_u32("/cpus/cpu-map/cluster0/core0", "cpu", CPU_PHANDLE);

    constexpr std::string_view cpu = "/cpus/cpu@0";
    fdt.add_node(cpu);
    fdt.set_phandle(cpu, CPU_PHANDLE);
    fdt.set_string(cpu, "device_type", "cpu");
    fdt.set_u32(cpu, "reg", 0);
    fdt.set_string(cpu, "status", "okay");
    fdt.set_string(cpu, "compatible", "riscv");
    fdt.set_string(cpu, "mmu-type", "riscv,sv39"); // the MMU implements Sv39
    // The HPM registers exist as read-only zero, which the specification
    // allows (norm:mhpmcounter_mhpmevent_rdonly0); both the legacy ISA string
    // and the extension list describe what the hart implements.
    fdt.set_string(cpu, "riscv,isa", "rv64imafdc_zicntr_zicsr_zifencei_zihpm");
    fdt.set_string(cpu, "riscv,isa-base", "rv64i");
    fdt.set_string_list(
        cpu, "riscv,isa-extensions",
        {"i", "m", "a", "f", "d", "c", "zicntr", "zicsr", "zifencei", "zihpm"});

    const std::string cpu_intc = std::string(cpu) + "/interrupt-controller";
    fdt.add_node(cpu_intc);
    fdt.set_u32(cpu_intc, "#interrupt-cells", 1);
    fdt.set_empty(cpu_intc, "interrupt-controller");
    fdt.set_string(cpu_intc, "compatible", "riscv,cpu-intc");
    fdt.set_phandle(cpu_intc, CPU_INTC_PHANDLE);

    // Memory: the architectural DRAM base, sized by the board.
    const std::string memory = at("/", "memory", core::Dram::DRAM_BASE);
    fdt.add_node(memory);
    fdt.set_string(memory, "device_type", "memory");
    set_reg(fdt, memory, core::Dram::DRAM_BASE, config_.dram.size);

    // Flash: one cfi-flash split over the two 32 MiB banks the board wires up,
    // each sized from its own geometry.
    const uint64_t flash0_size = checked_multiply(
        config_.flash0.sector_len, config_.flash0.num_blocks, "flash0 size");
    const uint64_t flash1_size = checked_multiply(
        config_.flash1.sector_len, config_.flash1.num_blocks, "flash1 size");
    const std::string flash = at("/", "flash", config_.flash0.base);
    fdt.add_node(flash);
    fdt.set_u32(flash, "bank-width", 4);
    fdt.set_cells(
        flash, "reg",
        {utils::upper_cell(config_.flash0.base),
         utils::lower_cell(config_.flash0.base), utils::upper_cell(flash0_size),
         utils::lower_cell(flash0_size), utils::upper_cell(config_.flash1.base),
         utils::lower_cell(config_.flash1.base), utils::upper_cell(flash1_size),
         utils::lower_cell(flash1_size)});
    fdt.set_string(flash, "compatible", "cfi-flash");

    // SoC: everything the guest finds through MMIO.  The windows are 1:1
    // mappings, so the child cells match the root's.
    fdt.add_node("/soc");
    fdt.set_u32("/soc", "#address-cells", 2);
    fdt.set_u32("/soc", "#size-cells", 2);
    fdt.set_string("/soc", "compatible", "simple-bus");
    fdt.set_empty("/soc", "ranges");

    // PCI: the host has one ECAM bus and only a non-prefetchable 32-bit
    // Memory window. There is no interrupt nexus until the board implements
    // PCI interrupt delivery, so it intentionally has no interrupt-map or
    // #interrupt-cells property.
    const std::string pci = at("/soc", "pci", config_.pci.ecam_base);
    fdt.add_node(pci);
    fdt.set_string(pci, "compatible", "pci-host-ecam-generic");
    fdt.set_string(pci, "device_type", "pci");
    fdt.set_u32(pci, "linux,pci-domain", 0);
    fdt.set_u32(pci, "#address-cells", 3);
    fdt.set_u32(pci, "#size-cells", 2);
    fdt.set_cells(pci, "bus-range", {0, 0});
    set_reg(fdt, pci, config_.pci.ecam_base, config_.pci.ecam_size);
    fdt.set_cells(pci, "ranges",
                  {0x02000000, utils::upper_cell(config_.pci.mmio_base),
                   utils::lower_cell(config_.pci.mmio_base),
                   utils::upper_cell(config_.pci.mmio_base),
                   utils::lower_cell(config_.pci.mmio_base),
                   utils::upper_cell(config_.pci.mmio_size),
                   utils::lower_cell(config_.pci.mmio_size)});

    const std::string plic =
        at("/soc", "interrupt-controller", config_.plic.base);
    fdt.add_node(plic);
    fdt.set_phandle(plic, PLIC_PHANDLE);
    fdt.set_u32(plic, "riscv,ndev", config_.plic.ndev);
    set_reg(fdt, plic, config_.plic.base, config_.plic.size);
    fdt.set_cells(plic, "interrupts-extended",
                  {CPU_INTC_PHANDLE, IRQ_M_EXT, CPU_INTC_PHANDLE, IRQ_S_EXT});
    fdt.set_empty(plic, "interrupt-controller");
    fdt.set_string(plic, "compatible", "riscv,plic0");
    fdt.set_u32(plic, "#interrupt-cells", 1);
    fdt.set_u32(plic, "#address-cells", 0);

    const std::string clint = at("/soc", "clint", config_.clint.base);
    fdt.add_node(clint);
    fdt.set_string(clint, "compatible", "riscv,clint0");
    fdt.set_cells(
        clint, "interrupts-extended",
        {CPU_INTC_PHANDLE, IRQ_M_SOFT, CPU_INTC_PHANDLE, IRQ_M_TIMER});
    set_reg(fdt, clint, config_.clint.base, config_.clint.size);

    const std::string uart = at("/soc", "uart", config_.uart.base);
    fdt.add_node(uart);
    fdt.set_string(uart, "compatible", "ns16550a");
    set_reg(fdt, uart, config_.uart.base, config_.uart.size);
    fdt.set_u32(uart, "interrupts", config_.uart.interrupt_id);
    fdt.set_u32(uart, "interrupt-parent", PLIC_PHANDLE);
    fdt.set_u32(uart, "clock-frequency", config_.uart.clock_hz);
    fdt.set_u32(uart, "reg-shift", config_.uart.reg_shift);
    fdt.set_u32(uart, "reg-io-width", config_.uart.reg_io_width);

    const std::string sifive_test =
        at("/soc", "sifive_test", config_.sifive_test.base);
    fdt.add_node(sifive_test);
    fdt.set_phandle(sifive_test, SIFIVE_TEST_PHANDLE);
    set_reg(fdt, sifive_test, config_.sifive_test.base,
            config_.sifive_test.size);
    fdt.set_string_list(sifive_test, "compatible",
                        {"sifive,test1", "sifive,test0", "syscon"});

    const std::string rtc = at("/soc", "rtc", config_.rtc.base);
    fdt.add_node(rtc);
    fdt.set_string(rtc, "compatible", "google,goldfish-rtc");
    set_reg(fdt, rtc, config_.rtc.base, config_.rtc.size);
    fdt.set_u32(rtc, "interrupts", config_.rtc.interrupt_id);
    fdt.set_u32(rtc, "interrupt-parent", PLIC_PHANDLE);

    // The block device exists only when the board names a disk, exactly as in
    // Emulator's device construction.
    if (!config_.virtio_blk.image.empty()) {
        const std::string virtio =
            at("/soc", "virtio_blk", config_.virtio_blk.base);
        fdt.add_node(virtio);
        fdt.set_string(virtio, "compatible", "virtio,mmio");
        set_reg(fdt, virtio, config_.virtio_blk.base, config_.virtio_blk.size);
        fdt.set_u32(virtio, "interrupts", config_.virtio_blk.interrupt_id);
        fdt.set_u32(virtio, "interrupt-parent", PLIC_PHANDLE);
    }

    const std::string events = at("/soc", "events", config_.input.base);
    fdt.add_node(events);
    fdt.set_string(events, "compatible", "google,goldfish-events-keypad");
    set_reg(fdt, events, config_.input.base, config_.input.size);
    fdt.set_u32(events, "interrupts", config_.input.interrupt_id);
    fdt.set_u32(events, "interrupt-parent", PLIC_PHANDLE);
    fdt.set_string(events, "label", "goldfish-events");
    fdt.set_string(events, "status", "okay");

    const std::string battery =
        at("/soc", "goldfish_battery", config_.battery.base);
    fdt.add_node(battery);
    fdt.set_string(battery, "compatible", "google,goldfish-battery");
    set_reg(fdt, battery, config_.battery.base, config_.battery.size);
    fdt.set_u32(battery, "interrupts", config_.battery.interrupt_id);
    fdt.set_u32(battery, "interrupt-parent", PLIC_PHANDLE);
    fdt.set_string(battery, "status", "okay");

    const std::string rng = at("/soc", "rng", config_.rng.base);
    fdt.add_node(rng);
    fdt.set_string(rng, "compatible", "brcm,bcm2835-rng");
    set_reg(fdt, rng, config_.rng.base, config_.rng.size);

    // Framebuffer: the window is exactly the pixels the device manages, so its
    // size and stride follow from the geometry.
    const uint64_t stride = checked_multiply(
        config_.framebuffer.width, device::SimpleFB::BPP, "framebuffer stride");
    const uint64_t framebuffer_size = checked_multiply(
        stride, config_.framebuffer.height, "framebuffer size");
    if (config_.framebuffer.width > std::numeric_limits<uint32_t>::max() ||
        config_.framebuffer.height > std::numeric_limits<uint32_t>::max() ||
        stride > std::numeric_limits<uint32_t>::max())
        throw std::overflow_error("framebuffer geometry does not fit the FDT");

    const std::string framebuffer =
        at("/soc", "frame-buffer", config_.framebuffer.base);
    fdt.add_node(framebuffer);
    fdt.set_string(framebuffer, "compatible", "simple-framebuffer");
    set_reg(fdt, framebuffer, config_.framebuffer.base, framebuffer_size);
    fdt.set_u32(framebuffer, "width",
                static_cast<uint32_t>(config_.framebuffer.width));
    fdt.set_u32(framebuffer, "height",
                static_cast<uint32_t>(config_.framebuffer.height));
    fdt.set_u32(framebuffer, "stride", static_cast<uint32_t>(stride));
    fdt.set_string(framebuffer, "format", "x8r8g8b8");
    fdt.set_string(framebuffer, "status", "okay");
    fdt.set_string(framebuffer, "linux,fb-type", "simple");

    // Power control: the two magic values that take the SiFive test device to
    // PASS or to a reset.
    fdt.add_node("/poweroff");
    fdt.set_u32("/poweroff", "value", device::SiFiveTest::Status::PASS);
    fdt.set_u32("/poweroff", "offset", 0);
    fdt.set_u32("/poweroff", "regmap", SIFIVE_TEST_PHANDLE);
    fdt.set_string("/poweroff", "compatible", "syscon-poweroff");

    fdt.add_node("/reboot");
    fdt.set_u32("/reboot", "value", device::SiFiveTest::Status::RESET);
    fdt.set_u32("/reboot", "offset", 0);
    fdt.set_u32("/reboot", "regmap", SIFIVE_TEST_PHANDLE);
    fdt.set_string("/reboot", "compatible", "syscon-reboot");

    return fdt.finish();
}

} // namespace uemu
