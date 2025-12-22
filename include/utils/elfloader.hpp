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

#include <elf.h>
#include <filesystem>

#include "core/dram.hpp"

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
