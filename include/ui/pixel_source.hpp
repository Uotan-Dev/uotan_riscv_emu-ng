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

#include <mutex>

namespace uemu::ui {

class PixelSource {
public:
    virtual ~PixelSource() = default;

    virtual size_t get_width() const = 0;
    virtual size_t get_height() const = 0;
    virtual size_t get_size() const = 0;

    virtual const uint8_t* get_pixels() const = 0;

    [[nodiscard]] virtual std::unique_lock<std::mutex> acquire_lock() const = 0;
};

} // namespace uemu::ui
