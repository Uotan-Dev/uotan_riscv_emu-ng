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

#pragma once

#include <filesystem>

#include "device/device.hpp"

namespace uemu::device {

class PFlashCFI01 : public Device {
public:
    static constexpr size_t CFI_TABLE_SIZE = 0x52;

    PFlashCFI01(addr_t base, uint64_t sector_len, uint32_t num_blocks);

    void load(const std::filesystem::path& path, size_t offset);

private:
    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override;
    bool write_internal(addr_t offset, size_t size, uint64_t value) override;

    uint32_t cfi_query(addr_t offset);
    uint32_t device_id_query(addr_t offset);

    uint32_t data_read(addr_t offset, size_t width);
    void data_write(addr_t offset, uint32_t value, size_t width);

    void blk_write_start(addr_t offset);
    void blk_write_flush();
    void blk_write_abort();

    uint32_t num_blocks_;
    uint64_t sector_len_;
    uint64_t total_size_;
    uint8_t bank_width_;       // Width in bytes, default 4
    uint8_t device_width_;     // Individual device width, calculated
    uint8_t max_device_width_; // Max width device supports
    uint16_t ident0_;          // Manufacturer ID (Intel = 0x89)
    uint16_t ident1_;          // Device ID
    uint16_t ident2_;          // Additional ID
    uint16_t ident3_;          // Additional ID

    // Flash storage
    std::vector<uint8_t> storage_;

    // CFI table
    std::array<uint8_t, CFI_TABLE_SIZE> cfi_table_;

    uint8_t wcycle_;   // Write cycle counter
    uint8_t cmd_;      // Current command
    uint8_t status_;   // Status register (0x80 = ready)
    uint64_t counter_; // Counter for multi-byte operations

    std::vector<uint8_t> blk_bytes_;
    uint32_t writeblock_size_;
    int64_t blk_offset_;

    bool read_mode_;
};

} // namespace uemu::device
