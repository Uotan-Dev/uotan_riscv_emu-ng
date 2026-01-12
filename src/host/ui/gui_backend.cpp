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

#include <chrono>
#include <stdexcept>

#include "host/ui/gui_backend.hpp"

namespace uemu::host::ui {

bool GuiBackend::initialized_ = false;

GuiBackend::GuiBackend(std::shared_ptr<device::SimpleFB> display,
                       HaltCallback halt_callback)
    : UIBackend(std::move(display), halt_callback) {
    if (initialized_)
        throw std::runtime_error("Double GuiBackend instances.");

    initialized_ = true;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        goto ui_fail;

    if (!(window_ = SDL_CreateWindow("Uotan RISCV Emulator (Next Generation)",
                                     DISPLAY_WIDTH, DISPLAY_HEIGHT, 0)))
        goto ui_fail;

    if (!(renderer_ = SDL_CreateRenderer(window_, nullptr)))
        goto ui_fail;

    if (!(texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_XRGB8888,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       DISPLAY_WIDTH, DISPLAY_HEIGHT)))
        goto ui_fail;

    return;

ui_fail:
    std::string sdl_error = SDL_GetError();

    if (texture_)
        SDL_DestroyTexture(texture_);

    if (renderer_)
        SDL_DestroyRenderer(renderer_);

    if (window_)
        SDL_DestroyWindow(window_);

    SDL_Quit();
    initialized_ = false;

    throw std::runtime_error(std::string("UI initialization failed: ") +
                             sdl_error);
}

GuiBackend::~GuiBackend() {
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }

    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }

    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
    initialized_ = false;
}

void GuiBackend::update() {
    if (!initialized_) [[unlikely]]
        throw std::runtime_error("GuiBackend should have been initialized.");

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT: request_halt(); return;
            case SDL_EVENT_KEY_DOWN: /* fallthrough */
            case SDL_EVENT_KEY_UP: /* TODO */ break;
            default: break;
        }
    }

    using clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    static auto last_update = clock::now();
    const auto now = clock::now();
    constexpr auto frame_interval = 16ms + 648us;

    if (now - last_update < frame_interval)
        return;

    display_->lock();
    const uint8_t* vram = display_->get_vram().data();
    SDL_UpdateTexture(texture_, nullptr, vram,
                      DISPLAY_WIDTH * device::SimpleFB::BPP);
    display_->unlock();

    SDL_RenderClear(renderer_);
    SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);

    last_update = now;
}

} // namespace uemu::host::ui
