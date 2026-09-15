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

#include "common/types.hpp"

namespace uemu {

// A half-open guest physical address range, [begin, end).  An empty range
// (begin == end) overlaps nothing, so it stands for "occupied nothing".
struct AddressRange {
    addr_t begin = 0;
    addr_t end = 0;

    [[nodiscard]] bool overlaps(const AddressRange& other) const noexcept {
        return begin < other.end && other.begin < end;
    }
};

} // namespace uemu
