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
        return read_internal(addr, sizeof(T));
    }

    template <typename T>
    void write(addr_t addr, T value) noexcept {
        write_internal(addr, sizeof(T), static_cast<uint64_t>(value));
    }

protected:
    virtual uint64_t read_internal(addr_t addr, size_t size) = 0;
    virtual void write_internal(addr_t addr, size_t size, uint64_t value) = 0;

    std::string name_;

    addr_t start_;
    addr_t end_;
};

}; // namespace uemu::device
