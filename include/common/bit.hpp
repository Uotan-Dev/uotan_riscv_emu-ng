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

#include <cstdint>
#include <limits>
#include <type_traits>

namespace uemu {

template <typename T = uint64_t>
constexpr T bitmask(int bits) {
    static_assert(std::is_unsigned_v<T>, "bitmask requires unsigned type");

    if (bits <= 0)
        return 0;

    if (bits >= static_cast<int>(sizeof(T) * 8))
        return std::numeric_limits<T>::max();

    return (T(1) << bits) - 1;
}

template <typename T>
constexpr std::make_unsigned_t<T> bits(T x, int hi, int lo) {
    static_assert(std::is_integral_v<T>, "bits requires integral type");
    using U = std::make_unsigned_t<T>;
    return (static_cast<U>(x) >> lo) & bitmask<U>(hi - lo + 1);
}

constexpr int64_t sext(uint64_t x, int len) {
    if (len <= 0 || len >= 64)
        return static_cast<int64_t>(x);

    int shift = 64 - len;
    return static_cast<int64_t>(x << shift) >> shift;
}

}; // namespace uemu
