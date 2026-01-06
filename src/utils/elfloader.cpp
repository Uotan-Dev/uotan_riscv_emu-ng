/*
 * Copyright 2025 Nuo Shen, Nanjing University
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

#include <cstring>
#include <print>

#include "utils/elfloader.hpp"
#include "utils/fileloader.hpp"

namespace uemu::utils {

bool ElfLoader::is_elf(const std::filesystem::path& p) {
    try {
        std::ifstream file(p, std::ios::binary);
        if (!file)
            return false;

        unsigned char magic[SELFMAG];
        if (!file.read(reinterpret_cast<char*>(magic), SELFMAG))
            return false;

        return std::memcmp(magic, ELFMAG, SELFMAG) == 0;
    } catch (...) { return false; }
}

uint64_t ElfLoader::load(const std::filesystem::path& p, core::Dram& dram) {
    std::vector<uint8_t> data = FileLoader::read_file(p);
    const auto* hdr = reinterpret_cast<const Elf64_Ehdr*>(data.data());

    validate_elf_header(hdr);

    const auto* phdr =
        reinterpret_cast<const Elf64_Phdr*>(data.data() + hdr->e_phoff);

    for (int i = 0; i < hdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD)
            continue;

        const uint64_t paddr = phdr[i].p_paddr;
        const size_t filesz = phdr[i].p_filesz;
        const size_t memsz = phdr[i].p_memsz;
        const uint64_t offset = phdr[i].p_offset;

        if (!dram.is_valid_addr(paddr, memsz)) [[unlikely]]
            throw std::out_of_range("Segment address outside DRAM bounds");

        if (filesz > 0)
            dram.write_bytes(paddr, data.data() + offset, filesz);

        if (memsz > filesz) {
            std::vector<uint8_t> zeros(memsz - filesz, 0);
            dram.write_bytes(paddr + filesz, zeros.data(), zeros.size());
        }
    }

    return hdr->e_entry;
}

void ElfLoader::dump_signature(const std::filesystem::path& elf_path,
                               const std::filesystem::path& sig_file_path,
                               core::Dram& dram) {
    auto data = FileLoader::read_file(elf_path);
    validate_elf_header(reinterpret_cast<const Elf64_Ehdr*>(data.data()));

    const Elf64_Sym* begin_sym = get_symbol(data.data(), "begin_signature");
    const Elf64_Sym* end_sym = get_symbol(data.data(), "end_signature");

    if (!begin_sym || !end_sym)
        throw std::runtime_error("Signature symbols not found in ELF file");

    const uint64_t start_addr = begin_sym->st_value;
    const uint64_t end_addr = end_sym->st_value;

    std::ofstream out(sig_file_path);
    if (!out)
        throw std::runtime_error("Cannot open signature output file: " +
                                 sig_file_path.string());

    std::println("Dumping signature from [{:#010x}, {:#010x})", start_addr,
                 end_addr);

    for (uint64_t addr = start_addr; addr < end_addr; addr += 4)
        out << std::hex << std::setw(8) << std::setfill('0')
            << dram.read<uint32_t>(addr) << '\n';

    std::println("Signature dumped to {}", sig_file_path.string());
}

const Elf64_Shdr* ElfLoader::get_section_header(const uint8_t* file_data,
                                                const char* name) {
    const auto* hdr = reinterpret_cast<const Elf64_Ehdr*>(file_data);
    if (hdr->e_shoff == 0 || hdr->e_shnum == 0)
        return nullptr;

    const auto* sh_tbl =
        reinterpret_cast<const Elf64_Shdr*>(file_data + hdr->e_shoff);
    const auto& str_sh = sh_tbl[hdr->e_shstrndx];
    const char* sh_str_tbl =
        reinterpret_cast<const char*>(file_data + str_sh.sh_offset);

    for (int i = 0; i < hdr->e_shnum; i++) {
        const char* sname = &sh_str_tbl[sh_tbl[i].sh_name];
        if (std::strcmp(name, sname) == 0)
            return &sh_tbl[i];
    }

    return nullptr;
}

const Elf64_Sym* ElfLoader::get_symbol(const uint8_t* file_data,
                                       const char* name) {
    const Elf64_Shdr* sym_hdr = get_section_header(file_data, ".symtab");
    const Elf64_Shdr* str_hdr = get_section_header(file_data, ".strtab");

    if (!sym_hdr || !str_hdr)
        return nullptr;

    const auto* sym_tbl =
        reinterpret_cast<const Elf64_Sym*>(file_data + sym_hdr->sh_offset);
    const char* str_tbl =
        reinterpret_cast<const char*>(file_data + str_hdr->sh_offset);
    const size_t sym_count = sym_hdr->sh_size / sizeof(Elf64_Sym);

    for (size_t i = 0; i < sym_count; i++) {
        const char* sym_name = &str_tbl[sym_tbl[i].st_name];
        if (std::strcmp(name, sym_name) == 0)
            return &sym_tbl[i];
    }

    return nullptr;
}

void ElfLoader::validate_elf_header(const Elf64_Ehdr* hdr) {
    if (std::memcmp(hdr->e_ident, ELFMAG, SELFMAG) != 0)
        throw std::runtime_error("Invalid ELF magic number");

    if (hdr->e_ident[EI_CLASS] != ELFCLASS64)
        throw std::runtime_error("Not a 64-bit ELF file");

    if (hdr->e_machine != EM_RISCV)
        throw std::runtime_error("Not a RISC-V ELF file");
}

}; // namespace uemu::utils
