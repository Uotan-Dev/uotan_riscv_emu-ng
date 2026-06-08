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

#include <SDL3/SDL.h>

#include "ui/ui_backend.hpp"

namespace uemu::ui {

class SDL3Backend : public UIBackend {
public:
    SDL3Backend(std::shared_ptr<ui::PixelSource> pixel_source,
                std::shared_ptr<ui::InputSink> input_sink,
                ExitCallback exit_callback);
    ~SDL3Backend() override;

    void update() override;

private:
    void update_view();

    static constexpr InputSink::linux_event_code_t
    sdl_scancode_to_linux(SDL_Scancode code) noexcept;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    size_t display_width_ = 0;
    size_t display_height_ = 0;
    std::vector<uint8_t> pixel_buffer_;
};

} // namespace uemu::ui
