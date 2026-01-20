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

#include <fstream>

#include "common/bit.hpp"
#include "device/pflash_cfi01.hpp"

namespace uemu::device {

PFlashCFI01::PFlashCFI01(addr_t base, uint64_t sector_len, uint32_t num_blocks)
    : Device("pflash-cfi01", base, sector_len * num_blocks) {
    num_blocks_ = num_blocks;
    sector_len_ = sector_len;
    total_size_ = sector_len * num_blocks;
    bank_width_ = 4;
    device_width_ = max_device_width_ = 2;
    ident0_ = 0x89; // Intel manufacturer ID
    ident1_ = 0x18; // Intel 28F128J3A device ID
    ident2_ = ident3_ = 0;
    wcycle_ = 0;
    cmd_ = 0;
    status_ = 0x80;
    counter_ = 0;
    blk_offset_ = -1;
    read_mode_ = true;
    storage_.resize(total_size_, 0xFF);

    int num_devices = 2;
    uint64_t blocks_per_device = num_blocks_;
    uint64_t sector_len_per_device = sector_len_ / num_devices;
    uint64_t device_len = sector_len_per_device * blocks_per_device;

    cfi_table_.fill(0);
    cfi_table_[0x10] = 'Q';
    cfi_table_[0x11] = 'R';
    cfi_table_[0x12] = 'Y';
    cfi_table_[0x13] = 0x01;
    cfi_table_[0x14] = 0x00;
    cfi_table_[0x15] = 0x31;
    cfi_table_[0x16] = 0x00;
    cfi_table_[0x17] = 0x00;
    cfi_table_[0x18] = 0x00;
    cfi_table_[0x19] = 0x00;
    cfi_table_[0x1A] = 0x00;
    cfi_table_[0x1B] = 0x45;
    cfi_table_[0x1C] = 0x55;
    cfi_table_[0x1D] = 0x00;
    cfi_table_[0x1E] = 0x00;
    cfi_table_[0x1F] = 0x07;
    cfi_table_[0x20] = 0x07;
    cfi_table_[0x21] = 0x0A;
    cfi_table_[0x22] = 0x00;
    cfi_table_[0x23] = 0x04;
    cfi_table_[0x24] = 0x04;
    cfi_table_[0x25] = 0x04;
    cfi_table_[0x26] = 0x00;
    cfi_table_[0x27] = ctz(device_len);
    cfi_table_[0x28] = 0x02;
    cfi_table_[0x29] = 0x00;
    cfi_table_[0x2A] = 0x0B;
    cfi_table_[0x2B] = 0x00;
    cfi_table_[0x2C] = 0x01;
    cfi_table_[0x2D] = (blocks_per_device - 1) & 0xFF;
    cfi_table_[0x2E] = (blocks_per_device - 1) >> 8;
    cfi_table_[0x2F] = (sector_len_per_device >> 8) & 0xFF;
    cfi_table_[0x30] = (sector_len_per_device >> 16) & 0xFF;
    cfi_table_[0x31] = 'P';
    cfi_table_[0x32] = 'R';
    cfi_table_[0x33] = 'I';
    cfi_table_[0x34] = '1';
    cfi_table_[0x35] = '0';
    cfi_table_[0x36] = 0x00;
    cfi_table_[0x37] = 0x00;
    cfi_table_[0x38] = 0x00;
    cfi_table_[0x39] = 0x00;
    cfi_table_[0x3A] = 0x00;
    cfi_table_[0x3B] = 0x00;
    cfi_table_[0x3C] = 0x00;
    cfi_table_[0x3F] = 0x01;

    writeblock_size_ = (1U << cfi_table_[0x2A]) * num_devices;
    blk_bytes_.resize(writeblock_size_);
}

void PFlashCFI01::load(const std::filesystem::path& path, size_t offset) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Failed to open Flash file: " + path.string());

    size_t file_size = file.tellg();
    if (offset + file_size > storage_.size())
        throw std::runtime_error("File is too large.");

    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(storage_.data() + offset),
                   file_size))
        throw std::runtime_error("Failed to read Flash file: " + path.string());
}

std::optional<uint64_t> PFlashCFI01::read_internal(addr_t offset, size_t size) {
    if (size == 8) {
        std::optional<uint64_t> lo = read_internal(offset, 4);
        if (lo == std::nullopt) [[unlikely]]
            return std::nullopt;

        std::optional<uint64_t> hi = read_internal(offset + 4, 4);
        if (hi == std::nullopt) [[unlikely]]
            return std::nullopt;

        return *lo | (*hi << 32);
    }

    if (read_mode_)
        return data_read(offset, size);

    uint32_t ret = 0xFFFFFFFF;

    switch (cmd_) {
        case 0x00: ret = data_read(offset, size); break;
        case 0x10: // Single byte program
        case 0x20: // Block erase
        case 0x28: // Block erase
        case 0x40: // single byte program
        case 0x50: // Clear status register
        case 0x60: // Block (un)lock
        case 0x70: // Status Register
        case 0xe8: // Write block
            ret = status_;
            // device_width_ is always 2
            if (size > device_width_) {
                size_t shift = device_width_ * 8;
                while (shift + device_width_ * 8 <= size * 8) {
                    ret |= static_cast<uint32_t>(status_) << shift;
                    shift += device_width_ * 8;
                }
            }
            break;
        case 0x90:
            ret = 0;
            for (size_t i = 0; i < size; i += bank_width_)
                ret = deposit(ret, 8 * i, bank_width_ * 8,
                              device_id_query(offset + i * bank_width_));
            break;
        case 0x98: // Query mode
            ret = 0;
            for (size_t i = 0; i < size; i += bank_width_)
                ret = deposit(ret, 8 * i, bank_width_ * 8,
                              cfi_query(offset + i * bank_width_));
            break;
        default: // Unknown state: reset and treat as read
            wcycle_ = 0;
            cmd_ = 0x00;
            ret = data_read(offset, size);
            break;
    }

    return ret;
}

bool PFlashCFI01::write_internal(addr_t offset, size_t size, uint64_t value) {
    if (size == 8)
        return write_internal(offset, 4, value & 0xFFFFFFFF) &&
               write_internal(offset + 4, 4, value >> 32);

    value &= 0xFFFFFFFF;

    if (!wcycle_)
        read_mode_ = false;

    uint8_t cmd = value & 0xFF;

    switch (wcycle_) {
        case 0:
            switch (cmd) {
                case 0x00: goto mode_read_array;
                case 0x10:
                case 0x40: break;
                case 0x20:
                case 0x28:
                    offset &= ~(sector_len_ - 1);
                    std::memset(storage_.data() + offset, 0xFF, sector_len_);
                    status_ |= 0x80;
                    break;
                case 0x50: status_ = 0; goto mode_read_array;
                case 0x60: break;
                case 0x70:
                case 0x90: cmd_ = cmd; return true;
                case 0x98: break;
                case 0xE8: status_ |= 0x80; break;
                case 0xF0:
                case 0xFF: goto mode_read_array;
                default: goto mode_read_array;
            }
            wcycle_ = 1;
            cmd_ = cmd;
            break;

        case 1:
            switch (cmd_) {
                case 0x10:
                case 0x40:
                    data_write(offset, value, size);
                    status_ |= 0x80;
                    wcycle_ = 0;
                    break;
                case 0x20:
                case 0x28:
                    if (cmd == 0xD0) {
                        wcycle_ = 0;
                        status_ |= 0x80;
                    } else {
                        goto mode_read_array;
                    }
                    break;
                case 0xE8:
                    counter_ = value & ((1U << (device_width_ * 8)) - 1);
                    wcycle_ = 2;
                    break;
                case 0x60:
                    if (cmd == 0xD0 || cmd == 0x01) {
                        wcycle_ = 0;
                        status_ |= 0x80;
                    } else if (cmd == 0xFF) {
                        goto mode_read_array;
                    }
                    break;
                case 0x98:
                    if (cmd == 0xFF)
                        goto mode_read_array;
                    break;
                default: goto mode_read_array;
            }
            break;

        case 2:
            if (cmd_ == 0xE8) {
                if (blk_offset_ == -1 && counter_)
                    blk_write_start(offset);

                if (blk_offset_ != -1)
                    data_write(offset, static_cast<uint32_t>(value), size);
                else
                    status_ |= 0x10;

                status_ |= 0x80;

                if (counter_ == 0)
                    wcycle_ = 3;
                else
                    counter_--;
            } else {
                goto mode_read_array;
            }
            break;

        case 3:
            switch (cmd_) {
                case 0xE8:
                    if (cmd == 0xD0 && !(status_ & 0x10)) {
                        blk_write_flush();
                        wcycle_ = 0;
                        status_ |= 0x80;
                    } else {
                        blk_write_abort();
                        goto mode_read_array;
                    }
                    break;
                default: blk_write_abort(); goto mode_read_array;
            }
            break;

        default: goto mode_read_array;
    }

    return true;

mode_read_array:
    read_mode_ = true;
    wcycle_ = 0;
    cmd_ = 0x00;
    return true;
}

uint32_t PFlashCFI01::cfi_query(addr_t offset) {
    addr_t boff = offset >> (ctz(bank_width_) + ctz(max_device_width_) -
                             ctz(device_width_));

    if (boff >= CFI_TABLE_SIZE)
        return 0;

    uint32_t resp = cfi_table_[boff];

    for (int i = device_width_; i < bank_width_; i += device_width_)
        resp = deposit(resp, 8 * i, 8 * device_width_, resp);

    return resp;
}

uint32_t PFlashCFI01::device_id_query(addr_t offset) {
    addr_t boff = offset >> (ctz(bank_width_) + ctz(max_device_width_) -
                             ctz(device_width_));
    uint32_t resp = 0;

    switch (boff & 0xFF) {
        case 0: resp = ident0_; break;
        case 1: resp = ident1_; break;
        default: return 0;
    }

    for (int i = device_width_; i < bank_width_; i += device_width_)
        resp = deposit(resp, 8 * i, 8 * device_width_, resp);

    return resp;
}

uint32_t PFlashCFI01::data_read(addr_t offset, size_t width) {
    if (width > 4 || offset + width > storage_.size()) [[unlikely]]
        return 0xFFFFFFFF;

    uint32_t ret = 0;
    std::memcpy(&ret, storage_.data() + offset, width);
    return ret;
}

void PFlashCFI01::data_write(addr_t offset, uint32_t value, size_t width) {
    if (width > 4) [[unlikely]]
        return;

    uint8_t* p = nullptr;

    if (blk_offset_ != -1) {
        if (offset < static_cast<uint64_t>(blk_offset_) ||
            offset + width >
                static_cast<uint64_t>(blk_offset_) + writeblock_size_) {
            status_ |= 0x10; // Programming error
            return;
        }

        p = blk_bytes_.data() + (offset - blk_offset_);
    } else {
        if (offset + width > storage_.size()) [[unlikely]]
            return;

        p = storage_.data() + offset;
    }

    std::memcpy(p, &value, width);
}

void PFlashCFI01::blk_write_start(addr_t offset) {
    blk_offset_ = offset & ~(writeblock_size_ - 1);
    std::memcpy(blk_bytes_.data(), storage_.data() + blk_offset_,
                writeblock_size_);
}

void PFlashCFI01::blk_write_flush() {
    if (blk_offset_ == -1)
        return;

    std::memcpy(storage_.data() + blk_offset_, blk_bytes_.data(),
                writeblock_size_);
    blk_offset_ = -1;
}

void PFlashCFI01::blk_write_abort() { blk_offset_ = -1; }

} // namespace uemu::device
