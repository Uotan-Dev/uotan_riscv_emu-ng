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

#include <cstring>
#include <string>

#include "common/types.hpp"

namespace uemu::device {

class Device {
public:
    explicit Device(const std::string& name, addr_t start, size_t size)
        : name_(name), start_(start), end_(start + size - 1) {}

    virtual ~Device() = default;

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    Device(Device&&) = default;
    Device& operator=(Device&&) = default;

    std::string name() const noexcept { return name_; }

    addr_t start() const noexcept { return start_; }

    addr_t end() const noexcept { return end_; }

    size_t size() const noexcept { return end_ - start_ + 1; }

    inline bool contains(addr_t addr, size_t len = 1) const noexcept {
        if (addr < start_ || addr > end_) [[unlikely]]
            return false;
        if (len > 0 && addr + len - 1 > end_) [[unlikely]]
            return false;
        return true;
    }

    template <typename T>
    [[nodiscard]] T read(addr_t addr) noexcept {
        return read_internal(addr - start_, sizeof(T));
    }

    template <typename T>
    void write(addr_t addr, T value) noexcept {
        write_internal(addr - start_, sizeof(T), static_cast<uint64_t>(value));
    }

    virtual void tick() {}

protected:
    virtual uint64_t read_internal(addr_t offset, size_t size) = 0;
    virtual void write_internal(addr_t offset, size_t size, uint64_t value) = 0;

    static void read_little_endian(const void* src, addr_t offset, size_t size,
                                   uint64_t* out_val) {
        if (size == 0 || offset + size > 8)
            return;

        const uint8_t* base = static_cast<const uint8_t*>(src);

        uint64_t val = 0;
        for (size_t i = 0; i < size; i++)
            val |= static_cast<uint64_t>(base[offset + i]) << (8 * i);

        *out_val = val;
    }

    static void write_little_endian(void* dst, addr_t offset, size_t size,
                                    uint64_t value) {
        if (size == 0 || offset + size > 8)
            return;

        uint8_t* base = static_cast<uint8_t*>(dst);

        for (size_t i = 0; i < size; i++)
            base[offset + i] = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
    }

    std::string name_;

    addr_t start_;
    addr_t end_;
};

}; // namespace uemu::device
