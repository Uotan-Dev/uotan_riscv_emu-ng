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

#include "device/bcm2835_rng.hpp"

namespace uemu::device {

std::optional<uint64_t> BCM2835Rng::read_internal(addr_t offset, size_t size) {
    if (size == 8) {
        std::optional<uint64_t> lo = read_internal(offset, 4);

        if (lo == std::nullopt) [[unlikely]]
            return std::nullopt;

        std::optional<uint64_t> hi = read_internal(offset + 4, 4);

        if (hi == std::nullopt) [[unlikely]]
            return std::nullopt;

        return *lo | (*hi << 32);
    }

    if (size != 4) [[unlikely]]
        return std::nullopt;

    switch (offset) {
        case RNG_CTRL: return rng_ctrl_;
        case RNG_STATUS: return rng_status_ | (1u << 24);
        case RNG_DATA: return get_random_bytes();
        default: return std::nullopt;
    }

    return std::nullopt;
}

bool BCM2835Rng::write_internal(addr_t offset, size_t size, uint64_t value) {
    if (size == 8)
        return write_internal(offset, 4, value & 0xFFFFFFFF) &&
               write_internal(offset + 4, 4, value >> 32);

    if (size != 4)
        return false;

    value &= 0xFFFFFFFF;

    switch (offset) {
        case RNG_CTRL: rng_ctrl_ = value; return true;
        case RNG_STATUS:
            rng_status_ &= ~0xFFFFF;
            rng_status_ |= value & 0xFFFFF;
            return true;
        default: return false;
    }

    return false;
}

} // namespace uemu::device
