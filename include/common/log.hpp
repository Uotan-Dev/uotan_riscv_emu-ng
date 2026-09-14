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

#include <cstdio>
#include <format>
#include <print>
#include <string_view>
#include <utility>

namespace uemu::log {

// Host-side diagnostics.  stdout is reserved for guest-visible console output
// (UART and friends), so every message here goes to stderr.
enum class Level { Info, Warn, Error };

namespace detail {

[[nodiscard]] constexpr std::string_view name(Level level) noexcept {
    switch (level) {
        case Level::Info: return "info";
        case Level::Warn: return "warn";
        case Level::Error: return "error";
    }

    return "info";
}

inline void emit(Level level, std::string_view message) {
    std::println(stderr, "[{}] {}", name(level), message);
}

} // namespace detail

template <typename... Args>
void info(std::format_string<Args...> fmt, Args&&... args) {
    detail::emit(Level::Info, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void warn(std::format_string<Args...> fmt, Args&&... args) {
    detail::emit(Level::Warn, std::format(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void error(std::format_string<Args...> fmt, Args&&... args) {
    detail::emit(Level::Error, std::format(fmt, std::forward<Args>(args)...));
}

} // namespace uemu::log
