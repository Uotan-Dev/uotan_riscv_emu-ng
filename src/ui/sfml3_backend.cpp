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

#include <cstring>
#include <stdexcept>

#include "ui/sfml3_backend.hpp"

namespace uemu::ui {

bool SFML3Backend::initialized_ = false;

SFML3Backend::SFML3Backend(std::shared_ptr<ui::PixelSource> pixel_source,
                           ExitCallback exit_callback)
    : UIBackend(pixel_source, exit_callback) {
    if (initialized_)
        throw std::runtime_error("Double SFMLBackend instances.");

    initialized_ = true;

    display_width_ = pixel_source_->get_width();
    display_height_ = pixel_source_->get_height();

    window_ = std::make_unique<sf::RenderWindow>(
        sf::VideoMode(sf::Vector2u(display_width_, display_height_)),
        "Uotan RISCV Emulator (Next Generation)");

    if (!window_->isOpen())
        throw std::runtime_error("Failed to create window");

    texture_ = std::make_unique<sf::Texture>();

    if (!texture_->resize(sf::Vector2u(display_width_, display_height_)))
        throw std::runtime_error("Failed to create texture");

    sprite_ = std::make_unique<sf::Sprite>(*texture_);
}

SFML3Backend::~SFML3Backend() { initialized_ = false; }

void SFML3Backend::update() {
    if (!initialized_) [[unlikely]]
        throw std::runtime_error("SFML3Backend should have been initialized.");

    while (const std::optional<sf::Event> event = window_->pollEvent()) {
        if (event->is<sf::Event::Closed>()) [[unlikely]] {
            request_exit();
            return;
        }

        // TODO: Handle keyboard
    }

    using clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    static auto last_update = clock::now();
    const auto now = clock::now();
    constexpr auto frame_interval = 16ms + 648us;

    if (now - last_update < frame_interval)
        return;

    const size_t size = pixel_source_->get_size();
    auto buffer = std::make_unique<uint8_t[]>(size);

    {
        std::unique_lock<std::mutex> lock = pixel_source_->acquire_lock();
        const uint8_t* pixels = pixel_source_->get_pixels();
        std::memcpy(buffer.get(), pixels, sizeof(uint8_t) * size);
    }

    uint32_t* p = reinterpret_cast<uint32_t*>(buffer.get());
    const size_t pixel_count = size / 4;

    // xrgb->rgba
    for (size_t i = 0; i < pixel_count; i++) {
        uint32_t pixel = p[i];
        uint8_t r = (pixel >> 16) & 0xFF;
        uint8_t g = (pixel >> 8) & 0xFF;
        uint8_t b = pixel & 0xFF;
        p[i] = (0xFF << 24) | (b << 16) | (g << 8) | r;
    }

    texture_->update(buffer.get());

    window_->clear();
    window_->draw(*sprite_);
    window_->display();

    last_update = now;
}

} // namespace uemu::ui
