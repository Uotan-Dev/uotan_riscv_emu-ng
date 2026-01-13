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
                           std::shared_ptr<ui::InputSink> input_sink,
                           ExitCallback exit_callback)
    : UIBackend(pixel_source, input_sink, exit_callback) {
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

        if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
            auto linux_event_code =
                sfml_scancode_to_linux(keyPressed->scancode);

            if (linux_event_code != KEY_RESERVED && input_sink_)
                input_sink_->push_key_event(
                    {linux_event_code, InputSink::KeyAction::Press});
        } else if (const auto* keyReleased =
                       event->getIf<sf::Event::KeyReleased>()) {
            auto linux_event_code =
                sfml_scancode_to_linux(keyReleased->scancode);

            if (linux_event_code != KEY_RESERVED && input_sink_)
                input_sink_->push_key_event(
                    {linux_event_code, InputSink::KeyAction::Release});
        }
    }

    using clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    if (!pixel_source_) [[unlikely]]
        return;

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

constexpr InputSink::linux_event_code_t
SFML3Backend::sfml_scancode_to_linux(sf::Keyboard::Scancode code) noexcept {
    using Scancode = sf::Keyboard::Scancode;
    switch (code) {
        case Scancode::A: return KEY_A;
        case Scancode::B: return KEY_B;
        case Scancode::C: return KEY_C;
        case Scancode::D: return KEY_D;
        case Scancode::E: return KEY_E;
        case Scancode::F: return KEY_F;
        case Scancode::G: return KEY_G;
        case Scancode::H: return KEY_H;
        case Scancode::I: return KEY_I;
        case Scancode::J: return KEY_J;
        case Scancode::K: return KEY_K;
        case Scancode::L: return KEY_L;
        case Scancode::M: return KEY_M;
        case Scancode::N: return KEY_N;
        case Scancode::O: return KEY_O;
        case Scancode::P: return KEY_P;
        case Scancode::Q: return KEY_Q;
        case Scancode::R: return KEY_R;
        case Scancode::S: return KEY_S;
        case Scancode::T: return KEY_T;
        case Scancode::U: return KEY_U;
        case Scancode::V: return KEY_V;
        case Scancode::W: return KEY_W;
        case Scancode::X: return KEY_X;
        case Scancode::Y: return KEY_Y;
        case Scancode::Z: return KEY_Z;

        case Scancode::Num1: return KEY_1;
        case Scancode::Num2: return KEY_2;
        case Scancode::Num3: return KEY_3;
        case Scancode::Num4: return KEY_4;
        case Scancode::Num5: return KEY_5;
        case Scancode::Num6: return KEY_6;
        case Scancode::Num7: return KEY_7;
        case Scancode::Num8: return KEY_8;
        case Scancode::Num9: return KEY_9;
        case Scancode::Num0: return KEY_0;

        case Scancode::Enter: return KEY_ENTER;
        case Scancode::Escape: return KEY_ESC;
        case Scancode::Backspace: return KEY_BACKSPACE;
        case Scancode::Tab: return KEY_TAB;
        case Scancode::Space: return KEY_SPACE;
        case Scancode::Hyphen: return KEY_MINUS;
        case Scancode::Equal: return KEY_EQUAL;
        case Scancode::LBracket: return KEY_LEFTBRACE;
        case Scancode::RBracket: return KEY_RIGHTBRACE;
        case Scancode::Backslash: return KEY_BACKSLASH;
        case Scancode::Semicolon: return KEY_SEMICOLON;
        case Scancode::Apostrophe: return KEY_APOSTROPHE;
        case Scancode::Grave: return KEY_GRAVE;
        case Scancode::Comma: return KEY_COMMA;
        case Scancode::Period: return KEY_DOT;
        case Scancode::Slash: return KEY_SLASH;

        case Scancode::F1: return KEY_F1;
        case Scancode::F2: return KEY_F2;
        case Scancode::F3: return KEY_F3;
        case Scancode::F4: return KEY_F4;
        case Scancode::F5: return KEY_F5;
        case Scancode::F6: return KEY_F6;
        case Scancode::F7: return KEY_F7;
        case Scancode::F8: return KEY_F8;
        case Scancode::F9: return KEY_F9;
        case Scancode::F10: return KEY_F10;
        case Scancode::F11: return KEY_F11;
        case Scancode::F12: return KEY_F12;
        case Scancode::F13: return KEY_F13;
        case Scancode::F14: return KEY_F14;
        case Scancode::F15: return KEY_F15;
        case Scancode::F16: return KEY_F16;
        case Scancode::F17: return KEY_F17;
        case Scancode::F18: return KEY_F18;
        case Scancode::F19: return KEY_F19;
        case Scancode::F20: return KEY_F20;
        case Scancode::F21: return KEY_F21;
        case Scancode::F22: return KEY_F22;
        case Scancode::F23: return KEY_F23;
        case Scancode::F24: return KEY_F24;

        case Scancode::CapsLock: return KEY_CAPSLOCK;
        case Scancode::PrintScreen: return KEY_SYSRQ;
        case Scancode::ScrollLock: return KEY_SCROLLLOCK;
        case Scancode::Pause: return KEY_PAUSE;
        case Scancode::Insert: return KEY_INSERT;
        case Scancode::Home: return KEY_HOME;
        case Scancode::PageUp: return KEY_PAGEUP;
        case Scancode::Delete: return KEY_DELETE;
        case Scancode::End: return KEY_END;
        case Scancode::PageDown: return KEY_PAGEDOWN;
        case Scancode::Right: return KEY_RIGHT;
        case Scancode::Left: return KEY_LEFT;
        case Scancode::Down: return KEY_DOWN;
        case Scancode::Up: return KEY_UP;

        case Scancode::NumLock: return KEY_NUMLOCK;
        case Scancode::NumpadDivide: return KEY_KPSLASH;
        case Scancode::NumpadMultiply: return KEY_KPASTERISK;
        case Scancode::NumpadMinus: return KEY_KPMINUS;
        case Scancode::NumpadPlus: return KEY_KPPLUS;
        case Scancode::NumpadEqual: return KEY_KPEQUAL;
        case Scancode::NumpadEnter: return KEY_KPENTER;
        case Scancode::NumpadDecimal: return KEY_KPDOT;
        case Scancode::Numpad1: return KEY_KP1;
        case Scancode::Numpad2: return KEY_KP2;
        case Scancode::Numpad3: return KEY_KP3;
        case Scancode::Numpad4: return KEY_KP4;
        case Scancode::Numpad5: return KEY_KP5;
        case Scancode::Numpad6: return KEY_KP6;
        case Scancode::Numpad7: return KEY_KP7;
        case Scancode::Numpad8: return KEY_KP8;
        case Scancode::Numpad9: return KEY_KP9;
        case Scancode::Numpad0: return KEY_KP0;

        case Scancode::NonUsBackslash: return KEY_102ND;
        case Scancode::Application: return KEY_COMPOSE;
        case Scancode::Execute: return KEY_OPEN;
        case Scancode::ModeChange: return KEY_ALTERASE;
        case Scancode::Help: return KEY_HELP;
        case Scancode::Menu: return KEY_MENU;
        case Scancode::Select: return KEY_SELECT;
        case Scancode::Redo: return KEY_REDO;
        case Scancode::Undo: return KEY_UNDO;
        case Scancode::Cut: return KEY_CUT;
        case Scancode::Copy: return KEY_COPY;
        case Scancode::Paste: return KEY_PASTE;

        case Scancode::VolumeMute: return KEY_MUTE;
        case Scancode::VolumeUp: return KEY_VOLUMEUP;
        case Scancode::VolumeDown: return KEY_VOLUMEDOWN;
        case Scancode::MediaPlayPause: return KEY_PLAYPAUSE;
        case Scancode::MediaStop: return KEY_STOPCD;
        case Scancode::MediaNextTrack: return KEY_NEXTSONG;
        case Scancode::MediaPreviousTrack: return KEY_PREVIOUSSONG;

        case Scancode::LControl: return KEY_LEFTCTRL;
        case Scancode::LShift: return KEY_LEFTSHIFT;
        case Scancode::LAlt: return KEY_LEFTALT;
        case Scancode::LSystem: return KEY_LEFTMETA;
        case Scancode::RControl: return KEY_RIGHTCTRL;
        case Scancode::RShift: return KEY_RIGHTSHIFT;
        case Scancode::RAlt: return KEY_RIGHTALT;
        case Scancode::RSystem: return KEY_RIGHTMETA;

        case Scancode::Back: return KEY_BACK;
        case Scancode::Forward: return KEY_FORWARD;
        case Scancode::Refresh: return KEY_REFRESH;
        case Scancode::Stop: return KEY_STOP;
        case Scancode::Search: return KEY_SEARCH;
        case Scancode::Favorites: return KEY_FAVORITES;
        case Scancode::HomePage: return KEY_HOMEPAGE;
        case Scancode::LaunchApplication1: return KEY_PROG1;
        case Scancode::LaunchApplication2: return KEY_PROG2;
        case Scancode::LaunchMail: return KEY_MAIL;
        case Scancode::LaunchMediaSelect: return KEY_MEDIA;

        default: return KEY_RESERVED;
    }
}

} // namespace uemu::ui
