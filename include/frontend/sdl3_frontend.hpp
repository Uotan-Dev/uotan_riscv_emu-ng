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

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <SDL3/SDL.h>

#include "frontend/frontend.hpp"
#include "frontend/terminal.hpp"

namespace uemu::frontend {

// SDL3 window frontend: presents the guest framebuffer and translates SDL
// input events into emulator input.  The host terminal is still used for the
// guest console.
class SDL3Frontend final : public Frontend {
public:
    explicit SDL3Frontend(Emulator& emulator);
    ~SDL3Frontend() override;

protected:
    void poll_input() override;
    void present() override;

private:
    void initialize_window();
    void update_view();

    static constexpr uint32_t sdl_scancode_to_linux(SDL_Scancode code) noexcept;

    Terminal terminal_;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    size_t display_width_ = 0;
    size_t display_height_ = 0;
    std::vector<uint8_t> pixel_buffer_;

    std::chrono::steady_clock::time_point last_frame_time_;
};

} // namespace uemu::frontend
