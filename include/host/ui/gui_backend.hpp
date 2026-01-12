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

#include "host/ui/backend.hpp"

namespace uemu::host::ui {

class GuiBackend : public UIBackend {
public:
    static constexpr size_t DISPLAY_WIDTH = device::SimpleFB::DEFAULT_WIDTH;
    static constexpr size_t DISPLAY_HEIGHT = device::SimpleFB::DEFAULT_HEIGHT;

    GuiBackend(std::shared_ptr<device::SimpleFB> display,
               HaltCallback halt_callback);
    ~GuiBackend();

    void update() override;

private:
    static bool initialized_;

    SDL_Window* window_;
    SDL_Renderer* renderer_;
    SDL_Texture* texture_;
};

} // namespace uemu::host::ui
