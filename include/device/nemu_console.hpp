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

#include "board_config.hpp"
#include "device/console_channel.hpp"
#include "device/device.hpp"

namespace uemu::device {

class NemuConsole : public Device {
public:
    explicit NemuConsole(const NemuConsoleConfig& config,
                         ConsoleChannel& console_channel)
        : Device("NemuConsole", config.base, config.size),
          console_channel_(console_channel),
          console_port_(console_channel_.register_port("NEMU Console", false)) {
    }

private:
    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override;
    bool write_internal(addr_t offset, size_t size, uint64_t value) override;

    ConsoleChannel& console_channel_;
    ConsoleChannel::Port console_port_;
};

} // namespace uemu::device
