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

#include "device/simple_fb.hpp"

namespace uemu::device {

std::optional<uint64_t> SimpleFB::read_internal(addr_t offset, size_t size) {
    if (size > 8 || offset + size > SIZE) [[unlikely]]
        return std::nullopt;

    uint64_t v = 0;

    {
        std::lock_guard<std::mutex> lock(simple_fb_mutex_);

        for (size_t i = 0; i < size; i++)
            v |= static_cast<uint64_t>(vram_[offset + i]) << (8 * i);
    }

    return v;
}

bool SimpleFB::write_internal(addr_t offset, size_t size, uint64_t value) {
    if (size > 8 || offset + size > SIZE) [[unlikely]]
        return false;

    {
        std::lock_guard<std::mutex> lock(simple_fb_mutex_);

        for (size_t i = 0; i < size; i++)
            vram_[offset + i] = static_cast<uint8_t>((value >> (8 * i)) & 0xFF);
    }

    return true;
}

}; // namespace uemu::device
