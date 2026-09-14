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

#include <cstddef>
#include <string>
#include <string_view>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace uemu::frontend {

// Host terminal I/O, owned by the main thread.  Switches stdin into raw,
// non-blocking mode for the lifetime of the object and restores it on
// destruction; a stdin that is not a terminal is tolerated.
class Terminal {
public:
    Terminal() { enable_raw_mode(); }

    ~Terminal() { restore_mode(); }

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    Terminal(Terminal&&) = delete;
    Terminal& operator=(Terminal&&) = delete;

    // Non-blocking: returns the host input bytes that are currently available.
    [[nodiscard]] std::string read_input() {
        std::string bytes;

        if (eof_)
            return bytes;

        char buffer[256];

        while (true) {
            ssize_t nread = ::read(STDIN_FILENO, buffer, sizeof(buffer));

            if (nread > 0) {
                bytes.append(buffer, static_cast<size_t>(nread));
                continue;
            }

            // A closed pipe stays closed; a terminal just has nothing to read.
            if (nread == 0)
                eof_ = true;

            break;
        }

        return bytes;
    }

    void write(std::string_view bytes) const {
        size_t offset = 0;

        while (offset < bytes.size()) {
            ssize_t written = ::write(STDOUT_FILENO, bytes.data() + offset,
                                      bytes.size() - offset);

            if (written <= 0)
                return;

            offset += static_cast<size_t>(written);
        }
    }

private:
    void enable_raw_mode() noexcept {
        tcgetattr(STDIN_FILENO, &original_mode_);
        struct termios raw = original_mode_;

        raw.c_iflag &=
            ~static_cast<tcflag_t>(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_cflag |= (CS8);
        raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;

        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

        original_flags_ = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, original_flags_ | O_NONBLOCK);
    }

    void restore_mode() noexcept {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_mode_);
        fcntl(STDIN_FILENO, F_SETFL, original_flags_);
    }

    struct termios original_mode_{};
    int original_flags_ = 0;
    bool eof_ = false;
};

} // namespace uemu::frontend
