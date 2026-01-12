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

#include <SFML/Graphics.hpp>

#include "ui/ui_backend.hpp"

namespace uemu::ui {

class SFML3Backend : public UIBackend {
public:
    SFML3Backend(std::shared_ptr<ui::PixelSource> pixel_source,
                 ExitCallback exit_callback);
    ~SFML3Backend() override;

    void update() override;

private:
    static bool initialized_;

    std::unique_ptr<sf::RenderWindow> window_;
    std::unique_ptr<sf::Texture> texture_;
    std::unique_ptr<sf::Sprite> sprite_;

    size_t display_width_;
    size_t display_height_;
};

} // namespace uemu::ui
