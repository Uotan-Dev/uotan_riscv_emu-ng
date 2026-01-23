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

#pragma once

#include <filesystem>

#include "core/dram.hpp"

#ifdef _WIN32
using Elf64_Addr = uint64_t;
using Elf64_Off = uint64_t;
using Elf64_Half = uint16_t;
using Elf64_Word = uint32_t;
using Elf64_Xword = uint64_t;

// Conglomeration of the identification bytes, for easy testing as a word.
constexpr const char* ELFMAG = "\177ELF";
constexpr int SELFMAG = 4;

enum {
    EI_MAG0 = 0, // ELF magic value
    EI_MAG1 = 1,
    EI_MAG2 = 2,
    EI_MAG3 = 3,
    EI_CLASS = 4,   // ELF class, one of ELF_IDENT_CLASS_
    EI_DATA = 5,    // Data type of the remainder of the file
    EI_VERSION = 6, // Version of the header, ELF_IDENT_VERSION_CURRENT
    EI_OSABI = 7,
    EI_ABIVERSION = 8,
    EI_PAD = 9, // nused padding
    EI_NIDENT = 16,
};

// File class
enum {
    ELFCLASSNONE = 0, // Invalid class
    ELFCLASS32 = 1,   // 32-bit objects
    ELFCLASS64 = 2,   // 64-bit objects
    ELFCLASSNUM = 3,
};

// Section Types
enum {
    SHT_NULL = 0,     // Section header table entry unused
    SHT_PROGBITS = 1, // Program data
    SHT_SYMTAB = 2,   // Symbol table
    SHT_STRTAB = 3,   // String table
    SHT_RELA = 4,     // Relocation entries with addends
    SHT_HASH = 5,     // Symbol hash table
    SHT_DYNAMIC = 6,  // Dynamic linking information
    SHT_NOTE = 7,     // Notes
    SHT_NOBITS = 8,   // Program space with no data (bss)
    SHT_REL = 9,      // Relocation entries, no addends
    SHT_SHLIB = 10,   // Reserved
    SHT_DYNSYM = 11,  // Dynamic linker symbol table
    SHT_NUM = 12,
    SHT_LOPROC = 0x70000000, // Start of processor-specific
    SHT_HIPROC = 0x7fffffff, // End of processor-specific
    SHT_LOUSER = 0x80000000, // Start of application-specific
    SHT_HIUSER = 0xffffffff, // End of application-specific
};

// Section Attribute Flags
enum {
    SHF_WRITE = 0x1,
    SHF_ALLOC = 0x2,
    SHF_EXECINSTR = 0x4,
    SHF_MERGE = 0x10,
    SHF_STRINGS = 0x20,
    SHF_INFO_LINK = 0x40,
    SHF_LINK_ORDER = 0x80,
    SHF_OS_NONCONFORMING = 0x100,
    SHF_GROUP = 0x200,
    SHF_TLS = 0x400,
    SHF_COMPRESSED = 0x800,
    SHF_MASKOS = 0x0ff00000,
    SHF_MASKPROC = 0xf0000000,
};

// Machine architectures
enum {
    EM_RISCV = 243, // RISC-V
};

// Legal values for p_type (segment type)
enum {
    PT_NULL = 0,                  // Program header table entry unused
    PT_LOAD = 1,                  // Loadable program segment
    PT_DYNAMIC = 2,               // Dynamic linking information
    PT_INTERP = 3,                // Program interpreter
    PT_NOTE = 4,                  // Auxiliary information
    PT_SHLIB = 5,                 // Reserved
    PT_PHDR = 6,                  // Entry for header table itself
    PT_TLS = 7,                   // Thread-local storage segment
    PT_NUM = 8,                   // Number of defined types
    PT_LOOS = 0x60000000,         // Start of OS-specific
    PT_GNU_EH_FRAME = 0x6474e550, // GCC .eh_frame_hdr segment
    PT_GNU_STACK = 0x6474e551,    // Indicates stack executability
    PT_GNU_RELRO = 0x6474e552,    // Read-only after relocation
    PT_GNU_PROPERTY = 0x6474e553, // GNU property
    PT_GNU_SFRAME = 0x6474e554,   // SFrame segment
    PT_LOSUNW = 0x6ffffffa,
    PT_SUNWBSS = 0x6ffffffa,   // Sun Specific segment
    PT_SUNWSTACK = 0x6ffffffb, // Stack segment
    PT_HISUNW = 0x6fffffff,
    PT_HIOS = 0x6fffffff,   // End of OS-specific
    PT_LOPROC = 0x70000000, // Start of processor-specific
    PT_HIPROC = 0x7fffffff, // End of processor-specific
};

// Elf64 header
struct Elf64_Ehdr {
    uint8_t e_ident[EI_NIDENT];
    Elf64_Half e_type;      // Object file type
    Elf64_Half e_machine;   // Architecture
    Elf64_Word e_version;   // Object file version
    Elf64_Addr e_entry;     // Entry point virtual address
    Elf64_Off e_phoff;      // Program header table file offset
    Elf64_Off e_shoff;      // Section header table file offset
    Elf64_Word e_flags;     // Processor-specific flags
    Elf64_Half e_ehsize;    // ELF header size in bytes
    Elf64_Half e_phentsize; // Program header table entry size
    Elf64_Half e_phnum;     // Program header table entry count
    Elf64_Half e_shentsize; // Section header table entry size
    Elf64_Half e_shnum;     // Section header table entry count
    Elf64_Half e_shstrndx;  // Section header string table index
};

// Elf64 program header table
struct Elf64_Phdr {
    Elf64_Word p_type;    // Type, a combination of ELF_PROGRAM_TYPE_
    Elf64_Word p_flags;   // Type-specific flags
    Elf64_Off p_offset;   // Offset in the file of the program image
    Elf64_Addr p_vaddr;   // Virtual address in memory
    Elf64_Addr p_paddr;   // Optional physical address in memory
    Elf64_Xword p_filesz; // Size of the image in the file
    Elf64_Xword p_memsz;  // Size of the image in memory
    Elf64_Xword p_align;  // Memory alignment in bytes
};

// Elf64 section header table
struct Elf64_Shdr {
    Elf64_Word sh_name;       // Section name
    Elf64_Word sh_type;       // Section type
    Elf64_Xword sh_flags;     // Section flags
    Elf64_Addr sh_addr;       // Section virtual addr at execution
    Elf64_Off sh_offset;      // Section file offset
    Elf64_Xword sh_size;      // Section size in bytes
    Elf64_Word sh_link;       // Link to another section
    Elf64_Word sh_info;       // Additional section information
    Elf64_Xword sh_addralign; // Section alignment
    Elf64_Xword sh_entsize;   // Entry size if section holds table
};

struct Elf64_Sym {
    Elf64_Word st_name;
    uint8_t st_info;
    uint8_t st_other;
    Elf64_Half st_shndx;
    Elf64_Addr st_value;
    Elf64_Xword st_size;
};
#else
#include <elf.h>
#endif

namespace uemu::utils {

class ElfLoader {
public:
    ElfLoader() = delete;
    ~ElfLoader() = delete;
    ElfLoader(const ElfLoader&) = delete;
    ElfLoader& operator=(const ElfLoader&) = delete;

    static bool is_elf(const std::filesystem::path& p);

    static uint64_t load(const std::filesystem::path& p, core::Dram& dram);

    static void dump_signature(const std::filesystem::path& elf_path,
                               const std::filesystem::path& sig_file_path,
                               core::Dram& dram);

private:
    static const Elf64_Shdr* get_section_header(const uint8_t* file_data,
                                                const char* name);

    static const Elf64_Sym* get_symbol(const uint8_t* file_data,
                                       const char* name);

    static void validate_elf_header(const Elf64_Ehdr* hdr);
};

} // namespace uemu::utils
