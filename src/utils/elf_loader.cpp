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

#include <cstdint>
#include <cstring>
#include <elf.h>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "utils/elf_loader.hpp"
#include "utils/fileloader.hpp"

namespace uemu::utils {

namespace {

template <typename T>
T read_object(std::span<const uint8_t> data, size_t offset,
              std::string_view description) {
    if (offset > data.size() || sizeof(T) > data.size() - offset)
        throw std::runtime_error("Truncated ELF " + std::string(description));

    T value;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

void validate_header(const Elf64_Ehdr& header) {
    if (std::memcmp(header.e_ident, ELFMAG, SELFMAG) != 0)
        throw std::runtime_error("Invalid ELF magic number");
    if (header.e_ident[EI_CLASS] != ELFCLASS64)
        throw std::runtime_error("Not a 64-bit ELF file");
    if (header.e_ident[EI_DATA] != ELFDATA2LSB)
        throw std::runtime_error("Not a little-endian ELF file");
    if (header.e_machine != EM_RISCV)
        throw std::runtime_error("Not a RISC-V ELF file");
    if (header.e_ehsize != sizeof(Elf64_Ehdr))
        throw std::runtime_error("Invalid ELF header size");
    if (header.e_phnum != 0 && header.e_phentsize != sizeof(Elf64_Phdr))
        throw std::runtime_error("Invalid ELF program header size");
}

} // namespace

ElfLoadResult ElfLoader::load(const std::filesystem::path& path,
                              core::Dram& dram) {
    const std::vector<uint8_t> data = FileLoader::read_file(path);
    return load(data, dram);
}

ElfLoadResult ElfLoader::load(std::span<const uint8_t> image,
                              core::Dram& dram) {
    const Elf64_Ehdr header = read_object<Elf64_Ehdr>(image, 0, "header");
    validate_header(header);

    if (header.e_phnum != 0 &&
        (header.e_phoff > image.size() ||
         header.e_phnum > (image.size() - header.e_phoff) / sizeof(Elf64_Phdr)))
        throw std::runtime_error("ELF program header table is out of bounds");

    ElfLoadResult result{.entry = header.e_entry, .loaded_ranges = {}};
    result.loaded_ranges.reserve(header.e_phnum);

    for (size_t i = 0; i < header.e_phnum; ++i) {
        const size_t offset =
            static_cast<size_t>(header.e_phoff) + i * sizeof(Elf64_Phdr);
        const Elf64_Phdr segment =
            read_object<Elf64_Phdr>(image, offset, "program header");

        if (segment.p_type != PT_LOAD)
            continue;
        if (segment.p_filesz > segment.p_memsz)
            throw std::runtime_error(
                "ELF PT_LOAD file size exceeds its memory size");
        if (segment.p_offset > image.size() ||
            segment.p_filesz > image.size() - segment.p_offset)
            throw std::runtime_error("ELF PT_LOAD data is out of bounds");
        if (segment.p_memsz >
            std::numeric_limits<addr_t>::max() - segment.p_paddr)
            throw std::overflow_error("ELF PT_LOAD address range overflows");
        if (!std::in_range<size_t>(segment.p_memsz))
            throw std::overflow_error("ELF PT_LOAD size is too large");

        const size_t file_size = static_cast<size_t>(segment.p_filesz);
        const size_t memory_size = static_cast<size_t>(segment.p_memsz);
        if (memory_size == 0)
            continue;

        if (!dram.is_valid_addr(segment.p_paddr, memory_size))
            throw std::out_of_range("ELF PT_LOAD segment is outside DRAM");

        if (file_size != 0)
            dram.write_bytes(segment.p_paddr, image.data() + segment.p_offset,
                             file_size);

        if (memory_size > file_size) {
            std::vector<uint8_t> zeros(memory_size - file_size);
            dram.write_bytes(segment.p_paddr + file_size, zeros.data(),
                             zeros.size());
        }

        result.loaded_ranges.push_back(
            {segment.p_paddr, segment.p_paddr + segment.p_memsz});
    }

    return result;
}

} // namespace uemu::utils
