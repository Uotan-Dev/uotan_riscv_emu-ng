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

#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include "common/address_range.hpp"
#include "core/dram.hpp"

namespace uemu::utils {

// Where an ELF asked the guest to run, and which guest physical ranges the load
// actually occupied.  The ranges are half-open and counted with p_memsz, so
// zero-filled tails are included: a later device tree must avoid them all.
struct ElfLoadResult {
    addr_t entry;
    std::vector<AddressRange> loaded_ranges;
};

class ElfLoader {
public:
    ElfLoader() = delete;
    ~ElfLoader() = delete;
    ElfLoader(const ElfLoader&) = delete;
    ElfLoader& operator=(const ElfLoader&) = delete;

    [[nodiscard]] static ElfLoadResult load(const std::filesystem::path& path,
                                            core::Dram& dram);

    // The same load from an image that is already in memory; the path overload
    // reads the file and delegates here.
    [[nodiscard]] static ElfLoadResult load(std::span<const uint8_t> image,
                                            core::Dram& dram);
};

} // namespace uemu::utils
