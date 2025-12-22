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

#include "device/sifive_test.hpp"

namespace uemu::device {

uint64_t SiFiveTest::read_internal([[maybe_unused]] addr_t addr,
                                   [[maybe_unused]] size_t size) {
    return 0;
}

void SiFiveTest::write_internal(addr_t addr, [[maybe_unused]] size_t size,
                                uint64_t value) {
    addr_t offset = addr - start_;

    if (offset == 0) {
        uint16_t status = value & 0xFFFF;
        uint16_t code = (value >> 16) & 0xFFFF;

        switch (status) {
            case Status::FAIL:
            case Status::PASS:
            case Status::RESET:
                on_shutdown_(code, static_cast<Status>(status));
                break;
            default: [[unlikely]] break;
        }
    }
}

}; // namespace uemu::device
