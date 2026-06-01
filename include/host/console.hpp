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

#include <fcntl.h>
#include <optional>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

namespace uemu::host {

class HostConsole {
public:
    HostConsole() { enable_raw_mode(); }

    ~HostConsole() { restore_mode(); }

    std::optional<char> read_char() noexcept {
        unsigned char c;
        ssize_t nread = ::read(STDIN_FILENO, &c, 1);

        if (nread > 0)
            return static_cast<char>(c);

        return std::nullopt;
    }

    void write_char(char ch) noexcept { ::write(STDOUT_FILENO, &ch, 1); }

private:
    void enable_raw_mode() noexcept {
        tcgetattr(STDIN_FILENO, &originalTermios_);
        struct termios raw = originalTermios_;

        raw.c_iflag &=
            ~static_cast<tcflag_t>(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_cflag |= (CS8);
        raw.c_lflag &= ~static_cast<tcflag_t>(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN] = 0;
        raw.c_cc[VTIME] = 0;

        tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);

        originalFlags_ = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, originalFlags_ | O_NONBLOCK);
    }

    void restore_mode() noexcept {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTermios_);
        fcntl(STDIN_FILENO, F_SETFL, originalFlags_);
    }

    struct termios originalTermios_;
    int originalFlags_;
};

} // namespace uemu::host
