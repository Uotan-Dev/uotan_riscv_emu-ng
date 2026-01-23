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

#include <iostream>
#include <optional>
#include <print>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace uemu::host {

class HostConsole {
public:
    HostConsole() { enable_raw_mode(); }

    ~HostConsole() { restore_mode(); }

    std::optional<char> read_char() noexcept {
#ifdef _WIN32
        if (_kbhit())
            return static_cast<char>(_getch());

        return std::nullopt;
#else
        unsigned char c;
        ssize_t nread = ::read(STDIN_FILENO, &c, 1);

        if (nread > 0)
            return static_cast<char>(c);

        return std::nullopt;
#endif
    }

    void write_char(char ch) {
        std::print("{}", ch);
        std::cout.flush();
    }

private:
    void enable_raw_mode() noexcept {
#ifdef _WIN32
        hStdin_ = GetStdHandle(STD_INPUT_HANDLE);
        GetConsoleMode(hStdin_, &originalMode_);
        DWORD rawMode =
            originalMode_ &
            ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
        SetConsoleMode(hStdin_, rawMode);
#else
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
#endif
    }

    void restore_mode() noexcept {
#ifdef _WIN32
        SetConsoleMode(hStdin_, originalMode_);
#else
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &originalTermios_);
        fcntl(STDIN_FILENO, F_SETFL, originalFlags_);
#endif
    }

#ifdef _WIN32
    HANDLE hStdin_;
    DWORD originalMode_;
#else
    struct termios originalTermios_;
    int originalFlags_;
#endif
};

} // namespace uemu::host
