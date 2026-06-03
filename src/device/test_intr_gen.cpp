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

#include "device/test_intr_gen.hpp"

namespace uemu::device {

TestIntrGen::TestIntrGen(std::shared_ptr<core::Hart> hart)
    : Device("TestIntrGen", DEFAULT_BASE, SIZE), hart_(std::move(hart)) {}

std::optional<uint64_t> TestIntrGen::read_internal(addr_t offset, size_t size) {
    // Sail SIG only supports 4-byte aligned accesses.
    if (size != 4) [[unlikely]]
        return std::nullopt;

    if (offset >= VERSION_OFFSET && offset < VERSION_OFFSET + 4) {
        // VERSION register: returns SIG_VERSION (0x00010000).
        uint32_t version = SIG_VERSION;
        uint64_t result = 0;
        read_little_endian(&version, offset - VERSION_OFFSET, size, &result);
        return result;
    }

    if (offset >= PLATFORM_OFFSET && offset < PLATFORM_OFFSET + 4) {
        // PLATFORM register: write-only, reads return 0 (matches Sail).
        return static_cast<uint64_t>(0);
    }

    return std::nullopt;
}

bool TestIntrGen::write_internal(addr_t offset, size_t size, uint64_t value) {
    // Sail SIG only supports 4-byte aligned accesses.
    if (size != 4) [[unlikely]]
        return false;

    if (offset >= VERSION_OFFSET && offset < VERSION_OFFSET + 4) {
        // VERSION register: writes are silently ignored (matches Sail).
        return true;
    }

    if (offset >= PLATFORM_OFFSET && offset < PLATFORM_OFFSET + 4) {
        // Decode: bit 31 = set(1)/clear(0), other bits map to MIP.
        uint32_t platform_val = static_cast<uint32_t>(value);
        bool set = (platform_val & SET_FLAG) != 0;
        reg_t mip_mask = platform_val & INTR_MASK;

        if (mip_mask)
            hart_->set_interrupt_pending(mip_mask, set);

        return true;
    }

    return false;
}

} // namespace uemu::device
