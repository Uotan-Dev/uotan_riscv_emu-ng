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

#include <array>
#include <cstring>
#include <elf.h>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "core/dram.hpp"
#include "utils/elf_loader.hpp"

namespace uemu::test {

namespace {

struct SegmentSpec {
    addr_t address;
    std::vector<uint8_t> data;
    size_t memory_size;
};

// A minimal but valid ELF64 image, built in memory: the tests must not depend
// on the filesystem (a shared temporary path made them fail when several tests
// ran as separate processes in parallel).
std::vector<uint8_t> make_elf_image(addr_t entry,
                                    const std::vector<SegmentSpec>& segments) {
    constexpr size_t DATA_OFFSET = 0x200;
    constexpr size_t DATA_STRIDE = 0x40;
    std::vector<uint8_t> image(DATA_OFFSET + DATA_STRIDE * segments.size());

    Elf64_Ehdr header{};
    std::memcpy(header.e_ident, ELFMAG, SELFMAG);
    header.e_ident[EI_CLASS] = ELFCLASS64;
    header.e_ident[EI_DATA] = ELFDATA2LSB;
    header.e_ident[EI_VERSION] = EV_CURRENT;
    header.e_type = ET_EXEC;
    header.e_machine = EM_RISCV;
    header.e_version = EV_CURRENT;
    header.e_entry = entry;
    header.e_phoff = sizeof(Elf64_Ehdr);
    header.e_ehsize = sizeof(Elf64_Ehdr);
    header.e_phentsize = sizeof(Elf64_Phdr);
    header.e_phnum = static_cast<Elf64_Half>(segments.size());
    std::memcpy(image.data(), &header, sizeof(header));

    for (size_t i = 0; i < segments.size(); ++i) {
        const size_t data_offset = DATA_OFFSET + i * DATA_STRIDE;
        const SegmentSpec& spec = segments[i];
        if (spec.data.size() > DATA_STRIDE)
            throw std::logic_error("ELF test segment data is too large");

        Elf64_Phdr segment{};
        segment.p_type = PT_LOAD;
        segment.p_flags = PF_R | PF_W | PF_X;
        segment.p_offset = data_offset;
        segment.p_vaddr = spec.address;
        segment.p_paddr = spec.address;
        segment.p_filesz = spec.data.size();
        segment.p_memsz = spec.memory_size;
        segment.p_align = 0x1000;
        std::memcpy(image.data() + header.e_phoff + i * sizeof(Elf64_Phdr),
                    &segment, sizeof(segment));
        if (!spec.data.empty())
            std::memcpy(image.data() + data_offset, spec.data.data(),
                        spec.data.size());
    }

    return image;
}

} // namespace

TEST(ElfLoaderTest, ReportsEveryLoadedSegmentAndInitializesBss) {
    constexpr addr_t first = core::Dram::DRAM_BASE + 0x1000;
    constexpr addr_t second = core::Dram::DRAM_BASE + 0x5000;
    const std::vector<uint8_t> image =
        make_elf_image(first, {{first, {0x13, 0x00, 0x00, 0x00}, 8},
                               {second, {0xaa, 0x55}, 2}});
    core::Dram dram(64 * 1024);

    const utils::ElfLoadResult result = utils::ElfLoader::load(image, dram);

    EXPECT_EQ(result.entry, first);
    ASSERT_EQ(result.loaded_ranges.size(), 2u);
    EXPECT_EQ(result.loaded_ranges[0].begin, first);
    EXPECT_EQ(result.loaded_ranges[0].end, first + 8);
    EXPECT_EQ(result.loaded_ranges[1].begin, second);
    EXPECT_EQ(result.loaded_ranges[1].end, second + 2);

    std::array<uint8_t, 8> first_data{};
    dram.read_bytes(first, first_data.data(), first_data.size());
    EXPECT_EQ(first_data,
              (std::array<uint8_t, 8>{0x13, 0x00, 0x00, 0x00, 0, 0, 0, 0}));
}

TEST(ElfLoaderTest, RejectsAFileImageLargerThanItsMemorySegment) {
    constexpr addr_t address = core::Dram::DRAM_BASE + 0x1000;
    const std::vector<uint8_t> image =
        make_elf_image(address, {{address, {1, 2, 3, 4}, 2}});
    core::Dram dram(64 * 1024);

    EXPECT_THROW(static_cast<void>(utils::ElfLoader::load(image, dram)),
                 std::runtime_error);
}

} // namespace uemu::test
