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

#include <cstdint>
#include <optional>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace uemu::ui::terminal {

namespace detail {

inline struct termios original_termios{};
inline int original_flags = -1;
inline bool raw_mode_enabled = false;
inline bool termios_saved = false;
inline bool flags_saved = false;

} // namespace detail

inline void enable_raw_mode() noexcept {
    if (detail::raw_mode_enabled)
        return;

    detail::original_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (detail::original_flags != -1 &&
        fcntl(STDIN_FILENO, F_SETFL, detail::original_flags | O_NONBLOCK) != -1)
        detail::flags_saved = true;

    struct termios original{};
    if (tcgetattr(STDIN_FILENO, &original) == 0) {
        detail::original_termios = original;

        struct termios raw = original;
        raw.c_iflag &=
            ~static_cast<tcflag_t>(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_cflag |= CS8;
        raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;

        if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == 0)
            detail::termios_saved = true;
    }

    detail::raw_mode_enabled = detail::flags_saved || detail::termios_saved;
}

inline void restore_mode() noexcept {
    if (!detail::raw_mode_enabled)
        return;

    if (detail::termios_saved)
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &detail::original_termios);

    if (detail::flags_saved)
        fcntl(STDIN_FILENO, F_SETFL, detail::original_flags);

    detail::raw_mode_enabled = false;
    detail::termios_saved = false;
    detail::flags_saved = false;
    detail::original_flags = -1;
}

inline std::optional<uint8_t> read_byte() noexcept {
    if (!detail::flags_saved)
        return std::nullopt;

    uint8_t byte;
    ssize_t nread = ::read(STDIN_FILENO, &byte, 1);

    if (nread > 0)
        return byte;

    return std::nullopt;
}

inline void write_byte(uint8_t byte) noexcept {
    ::write(STDOUT_FILENO, &byte, 1);
}

} // namespace uemu::ui::terminal
