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

#include <cstddef>
#include <cstring>
#include <memory>
#include <stdexcept>

#include "common/types.hpp"

namespace uemu::core {

class Dram {
public:
    static constexpr addr_t DRAM_BASE = 0x80000000;

    explicit Dram(size_t size) : mem_(new uint8_t[size]()), size_(size) {}

    Dram(const Dram&) = delete;
    Dram& operator=(const Dram&) = delete;
    Dram(Dram&&) = default;
    Dram& operator=(Dram&&) = default;

    size_t size() const { return size_; }

    inline bool is_valid_addr(addr_t addr, size_t len = 1) const noexcept {
        if (addr < DRAM_BASE) [[unlikely]]
            return false;

        auto offset = addr - DRAM_BASE;
        if (len > size_ || offset > size_ - len) [[unlikely]]
            return false;

        return true;
    }

    template <typename T>
    [[nodiscard]] T read(addr_t addr) const noexcept {
        T value;
        std::memcpy(&value, mem_.get() + (addr - DRAM_BASE), sizeof(T));
        return value;
    }

    template <typename T>
    void write(addr_t addr, T value) noexcept {
        std::memcpy(mem_.get() + (addr - DRAM_BASE), &value, sizeof(T));
    }

    void write_bytes(addr_t addr, const void* src, size_t len) {
        if (!is_valid_addr(addr, len))
            throw std::out_of_range(
                "Memory write_bytes out of bounds at address 0x" +
                std::to_string(addr) + ", length " + std::to_string(len));

        std::memcpy(mem_.get() + (addr - DRAM_BASE), src, len);
    }

    void read_bytes(addr_t addr, void* dst, size_t len) const {
        if (!is_valid_addr(addr, len))
            throw std::out_of_range(
                "Memory read_bytes out of bounds at address 0x" +
                std::to_string(addr) + ", length " + std::to_string(len));

        std::memcpy(dst, mem_.get() + (addr - DRAM_BASE), len);
    }

private:
    std::unique_ptr<uint8_t[]> mem_;
    size_t size_;
};

} // namespace uemu::core
