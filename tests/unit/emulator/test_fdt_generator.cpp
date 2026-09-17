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
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <libfdt.h>

#include "board_config.hpp"
#include "core/dram.hpp"
#include "device/simple_fb.hpp"
#include "fdt_generator.hpp"
#include "utils/fdt.hpp"

namespace uemu::test {

namespace {

int find_node(const std::vector<uint8_t>& dtb, const std::string& path) {
    const int node = fdt_path_offset(dtb.data(), path.c_str());
    EXPECT_GE(node, 0) << path << ": " << fdt_strerror(node);
    return node;
}

uint32_t read_u32(const std::vector<uint8_t>& dtb, int node, const char* name) {
    int length = 0;
    const auto* value = static_cast<const fdt32_t*>(
        fdt_getprop(dtb.data(), node, name, &length));
    EXPECT_NE(value, nullptr) << name;
    EXPECT_EQ(length, static_cast<int>(sizeof(fdt32_t))) << name;
    return value ? fdt32_to_cpu(*value) : 0;
}

std::string read_string(const std::vector<uint8_t>& dtb, int node,
                        const char* name) {
    int length = 0;
    const auto* value =
        static_cast<const char*>(fdt_getprop(dtb.data(), node, name, &length));
    EXPECT_NE(value, nullptr) << name;
    return value ? std::string(value) : std::string();
}

std::vector<uint32_t> read_cells(const std::vector<uint8_t>& dtb, int node,
                                 const char* name) {
    int length = 0;
    const auto* value = static_cast<const fdt32_t*>(
        fdt_getprop(dtb.data(), node, name, &length));
    EXPECT_NE(value, nullptr) << name;
    EXPECT_EQ(length % static_cast<int>(sizeof(fdt32_t)), 0) << name;

    std::vector<uint32_t> cells;
    if (!value)
        return cells;
    for (int i = 0; i < length / static_cast<int>(sizeof(fdt32_t)); ++i)
        cells.push_back(fdt32_to_cpu(value[i]));
    return cells;
}

void expect_reg(const std::vector<uint8_t>& dtb, const std::string& path,
                uint64_t address, uint64_t size) {
    EXPECT_EQ(read_cells(dtb, find_node(dtb, path), "reg"),
              (std::vector<uint32_t>{static_cast<uint32_t>(address >> 32),
                                     static_cast<uint32_t>(address),
                                     static_cast<uint32_t>(size >> 32),
                                     static_cast<uint32_t>(size)}));
}

} // namespace

TEST(FdtGeneratorTest, DefaultTreeDescribesTheVirtualBoard) {
    const BoardConfig config;
    const std::vector<uint8_t> dtb = FdtGenerator(config).generate();

    ASSERT_EQ(fdt_check_header(dtb.data()), 0);
    EXPECT_EQ(static_cast<size_t>(fdt_totalsize(dtb.data())), dtb.size());
    EXPECT_EQ(read_string(dtb, 0, "compatible"), "riscv-virt");
    EXPECT_EQ(read_string(dtb, 0, "model"), "uemu-ng");
    EXPECT_EQ(read_string(dtb, 0, "uotan,emulator-version"), UEMU_VERSION);

    const int cpu = find_node(dtb, "/cpus/cpu@0");
    EXPECT_EQ(read_string(dtb, cpu, "mmu-type"), "riscv,sv39");
    EXPECT_EQ(read_string(dtb, cpu, "riscv,isa"),
              "rv64imafdc_zicntr_zicsr_zifencei_zihpm");
    EXPECT_EQ(read_u32(dtb, find_node(dtb, "/cpus"), "timebase-frequency"),
              config.clint.freq_hz);

    expect_reg(dtb, "/memory@80000000", core::Dram::DRAM_BASE,
               config.dram.size);
    expect_reg(dtb, "/soc/clint@2000000", config.clint.base, config.clint.size);
    expect_reg(dtb, "/soc/interrupt-controller@c000000", config.plic.base,
               config.plic.size);
    expect_reg(dtb, "/soc/uart@10000000", config.uart.base, config.uart.size);

    const uint64_t framebuffer_size = config.framebuffer.width *
                                      config.framebuffer.height *
                                      device::SimpleFB::BPP;
    expect_reg(dtb, "/soc/frame-buffer@50000000", config.framebuffer.base,
               framebuffer_size);
    EXPECT_GE(fdt_path_offset(dtb.data(), "/soc/rtc@101000"), 0);
    EXPECT_GE(fdt_path_offset(dtb.data(), "/soc/events@10002000"), 0);
    EXPECT_GE(fdt_path_offset(dtb.data(), "/soc/goldfish_battery@10003000"), 0);
    EXPECT_GE(fdt_path_offset(dtb.data(), "/soc/rng@10004000"), 0);
    EXPECT_GE(fdt_path_offset(dtb.data(), "/soc/sifive_test@100000"), 0);
    EXPECT_GE(fdt_path_offset(dtb.data(), "/poweroff"), 0);
    EXPECT_GE(fdt_path_offset(dtb.data(), "/reboot"), 0);
    EXPECT_EQ(fdt_path_offset(dtb.data(), "/chosen"), -FDT_ERR_NOTFOUND);

    EXPECT_EQ(fdt_path_offset(dtb.data(), "/soc/virtio_blk@10001000"),
              -FDT_ERR_NOTFOUND);
    EXPECT_EQ(fdt_path_offset(dtb.data(), "/soc/test-intr-gen@40000000"),
              -FDT_ERR_NOTFOUND);
    EXPECT_EQ(fdt_path_offset(dtb.data(), "/soc/nemu-console@10008000"),
              -FDT_ERR_NOTFOUND);
}

TEST(FdtGeneratorTest, TreeTracksBoardConfigOverrides) {
    BoardConfig config;
    config.dram.size = 256 * 1024 * 1024;
    config.clint.base = 0x3000000;
    config.clint.size = 0x20000;
    config.clint.freq_hz = 5000000;
    config.plic.base = 0xd000000;
    config.plic.size = 0x800000;
    config.plic.ndev = 23;
    config.uart.base = 0x11000000;
    config.uart.size = 0x200;
    config.uart.interrupt_id = 6;
    config.uart.clock_hz = 1843200;
    config.uart.reg_shift = 2;
    config.uart.reg_io_width = 4;
    config.flash0.base = 0x24000000;
    config.flash0.num_blocks = 16;
    config.flash1.base = 0x24100000;
    config.flash1.num_blocks = 8;
    config.framebuffer.base = 0x51000000;
    config.framebuffer.width = 640;
    config.framebuffer.height = 480;
    config.virtio_blk.base = 0x10005000;
    config.virtio_blk.image = "disk.img";

    const std::vector<uint8_t> dtb = FdtGenerator(config).generate();

    expect_reg(dtb, "/memory@80000000", core::Dram::DRAM_BASE,
               config.dram.size);
    expect_reg(dtb, "/soc/clint@3000000", config.clint.base, config.clint.size);
    const int plic = find_node(dtb, "/soc/interrupt-controller@d000000");
    EXPECT_EQ(read_u32(dtb, plic, "riscv,ndev"), config.plic.ndev);
    expect_reg(dtb, "/soc/interrupt-controller@d000000", config.plic.base,
               config.plic.size);

    const int uart = find_node(dtb, "/soc/uart@11000000");
    EXPECT_EQ(read_u32(dtb, uart, "interrupts"), config.uart.interrupt_id);
    EXPECT_EQ(read_u32(dtb, uart, "clock-frequency"), config.uart.clock_hz);
    EXPECT_EQ(read_u32(dtb, uart, "reg-shift"), config.uart.reg_shift);
    EXPECT_EQ(read_u32(dtb, uart, "reg-io-width"), config.uart.reg_io_width);

    EXPECT_EQ(read_cells(dtb, find_node(dtb, "/flash@24000000"), "reg"),
              (std::vector<uint32_t>{0, 0x24000000, 0, 0x100000, 0, 0x24100000,
                                     0, 0x80000}));
    expect_reg(dtb, "/soc/frame-buffer@51000000", config.framebuffer.base,
               640 * 480 * device::SimpleFB::BPP);
    expect_reg(dtb, "/soc/virtio_blk@10005000", config.virtio_blk.base,
               config.virtio_blk.size);
}

// Firmware resolves the interrupt topology through phandles, so the references
// have to point at the nodes that own them.
TEST(FdtGeneratorTest, PhandlesLinkTheInterruptTopology) {
    const BoardConfig config;
    const std::vector<uint8_t> dtb = FdtGenerator(config).generate();

    const uint32_t cpu_phandle =
        read_u32(dtb, find_node(dtb, "/cpus/cpu@0"), "phandle");
    const uint32_t intc_phandle = read_u32(
        dtb, find_node(dtb, "/cpus/cpu@0/interrupt-controller"), "phandle");
    const uint32_t plic_phandle = read_u32(
        dtb, find_node(dtb, "/soc/interrupt-controller@c000000"), "phandle");
    const uint32_t test_phandle =
        read_u32(dtb, find_node(dtb, "/soc/sifive_test@100000"), "phandle");

    EXPECT_NE(cpu_phandle, 0u);
    EXPECT_NE(intc_phandle, 0u);
    EXPECT_NE(plic_phandle, 0u);
    EXPECT_NE(test_phandle, 0u);
    EXPECT_NE(cpu_phandle, intc_phandle);
    EXPECT_NE(cpu_phandle, plic_phandle);
    EXPECT_NE(cpu_phandle, test_phandle);
    EXPECT_NE(intc_phandle, plic_phandle);
    EXPECT_NE(intc_phandle, test_phandle);
    EXPECT_NE(plic_phandle, test_phandle);

    EXPECT_EQ(
        read_cells(dtb, find_node(dtb, "/cpus/cpu-map/cluster0/core0"), "cpu"),
        (std::vector<uint32_t>{cpu_phandle}));
    EXPECT_EQ(read_cells(dtb, find_node(dtb, "/soc/clint@2000000"),
                         "interrupts-extended"),
              (std::vector<uint32_t>{intc_phandle, 3, intc_phandle, 7}));
    EXPECT_EQ(read_cells(dtb,
                         find_node(dtb, "/soc/interrupt-controller@c000000"),
                         "interrupts-extended"),
              (std::vector<uint32_t>{intc_phandle, 11, intc_phandle, 9}));

    for (const char* path :
         {"/soc/uart@10000000", "/soc/rtc@101000", "/soc/events@10002000",
          "/soc/goldfish_battery@10003000"})
        EXPECT_EQ(read_u32(dtb, find_node(dtb, path), "interrupt-parent"),
                  plic_phandle)
            << path;

    EXPECT_EQ(read_u32(dtb, find_node(dtb, "/poweroff"), "regmap"),
              test_phandle);
    EXPECT_EQ(read_u32(dtb, find_node(dtb, "/reboot"), "regmap"), test_phandle);
}

// The board can be given more than 4 GiB of DRAM, so the upper half of a 64-bit
// reg cell must not be lost.
TEST(FdtGeneratorTest, SixtyFourBitCellsKeepTheirUpperHalf) {
    BoardConfig config;
    config.dram.size = 8ULL * 1024 * 1024 * 1024;
    const std::vector<uint8_t> dtb = FdtGenerator(config).generate();

    EXPECT_EQ(utils::upper_cell(0x1'0000'0000ULL), 1u);
    EXPECT_EQ(utils::lower_cell(0x1'0000'0000ULL), 0u);
    expect_reg(dtb, "/memory@80000000", core::Dram::DRAM_BASE,
               config.dram.size);
    EXPECT_EQ(read_cells(dtb, find_node(dtb, "/memory@80000000"), "reg"),
              (std::vector<uint32_t>{0, 0x80000000, 2, 0}));
}

} // namespace uemu::test
