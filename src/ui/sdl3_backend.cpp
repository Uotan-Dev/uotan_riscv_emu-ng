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
#include <cstring>
#include <stdexcept>

#include <SDL3/SDL_init.h>

#include "ui/sdl3_backend.hpp"

#include "local-include/uotan_rgba.hpp"

namespace uemu::ui {

SDL3Backend::SDL3Backend(Endpoints endpoints)
    : UIBackend(std::move(endpoints)) {
    const auto& pixel_source = endpoints_.pixel_source;

    display_width_ = pixel_source->get_width();
    display_height_ = pixel_source->get_height();
    pixel_buffer_.resize(pixel_source->get_size());

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
        goto fail;

    if (!(window_ = SDL_CreateWindow("Uotan RISC-V Emulator (Next Generation)",
                                     display_width_, display_height_,
                                     SDL_WINDOW_RESIZABLE)))
        goto fail;

    if (!(renderer_ = SDL_CreateRenderer(window_, nullptr)))
        goto fail;

    if (!(texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_XRGB8888,
                                       SDL_TEXTUREACCESS_STREAMING,
                                       display_width_, display_height_)))
        goto fail;

    {
        // Set window icon from embedded RGBA data
        static constexpr int ICON_SIZE = 64;
        static_assert(uotan_rgba_len == 16384);

        SDL_Surface* icon = SDL_CreateSurfaceFrom(ICON_SIZE, ICON_SIZE,
                                                  SDL_PIXELFORMAT_RGBA8888,
                                                  uotan_rgba, ICON_SIZE * 4);

        if (icon) {
            SDL_SetWindowIcon(window_, icon);
            SDL_DestroySurface(icon);
        }
    }

    host_console_.apply_to_endpoint(*endpoints_.console_endpoint);

    return;

fail:
    std::string sdl_error = SDL_GetError();

    if (texture_)
        SDL_DestroyTexture(texture_);
    if (renderer_)
        SDL_DestroyRenderer(renderer_);
    if (window_)
        SDL_DestroyWindow(window_);

    SDL_Quit();

    throw std::runtime_error(
        std::string("SDL3 backend initialization failed: ") + sdl_error);
}

SDL3Backend::~SDL3Backend() {
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
}

void SDL3Backend::update() {
    const auto& input_sink = endpoints_.input_sink;
    const auto& pixel_source = endpoints_.pixel_source;

    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT: request_exit(); return;

            case SDL_EVENT_WINDOW_RESIZED: update_view(); break;

            case SDL_EVENT_KEY_DOWN: {
                auto linux_code = sdl_scancode_to_linux(event.key.scancode);
                if (linux_code != KEY_RESERVED && input_sink)
                    input_sink->push_key_event(
                        {linux_code, InputSink::KeyAction::Press});
                break;
            }

            case SDL_EVENT_KEY_UP: {
                auto linux_code = sdl_scancode_to_linux(event.key.scancode);
                if (linux_code != KEY_RESERVED && input_sink)
                    input_sink->push_key_event(
                        {linux_code, InputSink::KeyAction::Release});
                break;
            }

            default: break;
        }
    }

    using clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    if (!pixel_source) [[unlikely]]
        return;

    static auto last_update = clock::now();
    const auto now = clock::now();
    constexpr auto frame_interval = 16ms + 648us;

    if (now - last_update < frame_interval)
        return;

    const size_t size = pixel_source->get_size();
    uint8_t* buffer = pixel_buffer_.data();

    {
        std::unique_lock<std::mutex> lock = pixel_source->acquire_lock();
        const uint8_t* pixels = pixel_source->get_pixels();
        std::memcpy(buffer, pixels, sizeof(uint8_t) * size);
    }

    SDL_UpdateTexture(texture_, nullptr, buffer, display_width_ * 4);
    SDL_SetRenderDrawColor(renderer_, 64, 64, 64, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);

    last_update = now;
}

void SDL3Backend::update_view() {
    // Re-apply logical presentation on resize so SDL re-computes the
    // letterbox layout for the new window dimensions.
    SDL_SetRenderLogicalPresentation(renderer_, display_width_, display_height_,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

constexpr InputSink::linux_event_code_t
SDL3Backend::sdl_scancode_to_linux(SDL_Scancode code) noexcept {
    switch (code) {
        // Letters
        case SDL_SCANCODE_A: return KEY_A;
        case SDL_SCANCODE_B: return KEY_B;
        case SDL_SCANCODE_C: return KEY_C;
        case SDL_SCANCODE_D: return KEY_D;
        case SDL_SCANCODE_E: return KEY_E;
        case SDL_SCANCODE_F: return KEY_F;
        case SDL_SCANCODE_G: return KEY_G;
        case SDL_SCANCODE_H: return KEY_H;
        case SDL_SCANCODE_I: return KEY_I;
        case SDL_SCANCODE_J: return KEY_J;
        case SDL_SCANCODE_K: return KEY_K;
        case SDL_SCANCODE_L: return KEY_L;
        case SDL_SCANCODE_M: return KEY_M;
        case SDL_SCANCODE_N: return KEY_N;
        case SDL_SCANCODE_O: return KEY_O;
        case SDL_SCANCODE_P: return KEY_P;
        case SDL_SCANCODE_Q: return KEY_Q;
        case SDL_SCANCODE_R: return KEY_R;
        case SDL_SCANCODE_S: return KEY_S;
        case SDL_SCANCODE_T: return KEY_T;
        case SDL_SCANCODE_U: return KEY_U;
        case SDL_SCANCODE_V: return KEY_V;
        case SDL_SCANCODE_W: return KEY_W;
        case SDL_SCANCODE_X: return KEY_X;
        case SDL_SCANCODE_Y: return KEY_Y;
        case SDL_SCANCODE_Z: return KEY_Z;

        // Numbers
        case SDL_SCANCODE_1: return KEY_1;
        case SDL_SCANCODE_2: return KEY_2;
        case SDL_SCANCODE_3: return KEY_3;
        case SDL_SCANCODE_4: return KEY_4;
        case SDL_SCANCODE_5: return KEY_5;
        case SDL_SCANCODE_6: return KEY_6;
        case SDL_SCANCODE_7: return KEY_7;
        case SDL_SCANCODE_8: return KEY_8;
        case SDL_SCANCODE_9: return KEY_9;
        case SDL_SCANCODE_0: return KEY_0;

        // Whitespace / basic
        case SDL_SCANCODE_RETURN: return KEY_ENTER;
        case SDL_SCANCODE_ESCAPE: return KEY_ESC;
        case SDL_SCANCODE_BACKSPACE: return KEY_BACKSPACE;
        case SDL_SCANCODE_TAB: return KEY_TAB;
        case SDL_SCANCODE_SPACE: return KEY_SPACE;

        // Punctuation
        case SDL_SCANCODE_MINUS: return KEY_MINUS;
        case SDL_SCANCODE_EQUALS: return KEY_EQUAL;
        case SDL_SCANCODE_LEFTBRACKET: return KEY_LEFTBRACE;
        case SDL_SCANCODE_RIGHTBRACKET: return KEY_RIGHTBRACE;
        case SDL_SCANCODE_BACKSLASH: return KEY_BACKSLASH;
        case SDL_SCANCODE_SEMICOLON: return KEY_SEMICOLON;
        case SDL_SCANCODE_APOSTROPHE: return KEY_APOSTROPHE;
        case SDL_SCANCODE_GRAVE: return KEY_GRAVE;
        case SDL_SCANCODE_COMMA: return KEY_COMMA;
        case SDL_SCANCODE_PERIOD: return KEY_DOT;
        case SDL_SCANCODE_SLASH: return KEY_SLASH;

        // Function keys
        case SDL_SCANCODE_F1: return KEY_F1;
        case SDL_SCANCODE_F2: return KEY_F2;
        case SDL_SCANCODE_F3: return KEY_F3;
        case SDL_SCANCODE_F4: return KEY_F4;
        case SDL_SCANCODE_F5: return KEY_F5;
        case SDL_SCANCODE_F6: return KEY_F6;
        case SDL_SCANCODE_F7: return KEY_F7;
        case SDL_SCANCODE_F8: return KEY_F8;
        case SDL_SCANCODE_F9: return KEY_F9;
        case SDL_SCANCODE_F10: return KEY_F10;
        case SDL_SCANCODE_F11: return KEY_F11;
        case SDL_SCANCODE_F12: return KEY_F12;
        case SDL_SCANCODE_F13: return KEY_F13;
        case SDL_SCANCODE_F14: return KEY_F14;
        case SDL_SCANCODE_F15: return KEY_F15;
        case SDL_SCANCODE_F16: return KEY_F16;
        case SDL_SCANCODE_F17: return KEY_F17;
        case SDL_SCANCODE_F18: return KEY_F18;
        case SDL_SCANCODE_F19: return KEY_F19;
        case SDL_SCANCODE_F20: return KEY_F20;
        case SDL_SCANCODE_F21: return KEY_F21;
        case SDL_SCANCODE_F22: return KEY_F22;
        case SDL_SCANCODE_F23: return KEY_F23;
        case SDL_SCANCODE_F24: return KEY_F24;

        // Caps / print / scroll / pause
        case SDL_SCANCODE_CAPSLOCK: return KEY_CAPSLOCK;
        case SDL_SCANCODE_PRINTSCREEN: return KEY_SYSRQ;
        case SDL_SCANCODE_SCROLLLOCK: return KEY_SCROLLLOCK;
        case SDL_SCANCODE_PAUSE: return KEY_PAUSE;

        // Navigation
        case SDL_SCANCODE_INSERT: return KEY_INSERT;
        case SDL_SCANCODE_HOME: return KEY_HOME;
        case SDL_SCANCODE_PAGEUP: return KEY_PAGEUP;
        case SDL_SCANCODE_DELETE: return KEY_DELETE;
        case SDL_SCANCODE_END: return KEY_END;
        case SDL_SCANCODE_PAGEDOWN: return KEY_PAGEDOWN;
        case SDL_SCANCODE_RIGHT: return KEY_RIGHT;
        case SDL_SCANCODE_LEFT: return KEY_LEFT;
        case SDL_SCANCODE_DOWN: return KEY_DOWN;
        case SDL_SCANCODE_UP: return KEY_UP;

        // Numpad
        case SDL_SCANCODE_NUMLOCKCLEAR: return KEY_NUMLOCK;
        case SDL_SCANCODE_KP_DIVIDE: return KEY_KPSLASH;
        case SDL_SCANCODE_KP_MULTIPLY: return KEY_KPASTERISK;
        case SDL_SCANCODE_KP_MINUS: return KEY_KPMINUS;
        case SDL_SCANCODE_KP_PLUS: return KEY_KPPLUS;
        case SDL_SCANCODE_KP_EQUALS: return KEY_KPEQUAL;
        case SDL_SCANCODE_KP_ENTER: return KEY_KPENTER;
        case SDL_SCANCODE_KP_PERIOD: return KEY_KPDOT;
        case SDL_SCANCODE_KP_1: return KEY_KP1;
        case SDL_SCANCODE_KP_2: return KEY_KP2;
        case SDL_SCANCODE_KP_3: return KEY_KP3;
        case SDL_SCANCODE_KP_4: return KEY_KP4;
        case SDL_SCANCODE_KP_5: return KEY_KP5;
        case SDL_SCANCODE_KP_6: return KEY_KP6;
        case SDL_SCANCODE_KP_7: return KEY_KP7;
        case SDL_SCANCODE_KP_8: return KEY_KP8;
        case SDL_SCANCODE_KP_9: return KEY_KP9;
        case SDL_SCANCODE_KP_0: return KEY_KP0;

        // Modifiers
        case SDL_SCANCODE_LCTRL: return KEY_LEFTCTRL;
        case SDL_SCANCODE_LSHIFT: return KEY_LEFTSHIFT;
        case SDL_SCANCODE_LALT: return KEY_LEFTALT;
        case SDL_SCANCODE_LGUI: return KEY_LEFTMETA;
        case SDL_SCANCODE_RCTRL: return KEY_RIGHTCTRL;
        case SDL_SCANCODE_RSHIFT: return KEY_RIGHTSHIFT;
        case SDL_SCANCODE_RALT: return KEY_RIGHTALT;
        case SDL_SCANCODE_RGUI: return KEY_RIGHTMETA;

        // Misc keys
        case SDL_SCANCODE_NONUSBACKSLASH: return KEY_102ND;
        case SDL_SCANCODE_APPLICATION: return KEY_COMPOSE;
        case SDL_SCANCODE_EXECUTE: return KEY_OPEN;
        case SDL_SCANCODE_HELP: return KEY_HELP;
        case SDL_SCANCODE_MENU: return KEY_MENU;
        case SDL_SCANCODE_SELECT: return KEY_SELECT;
        case SDL_SCANCODE_STOP: return KEY_STOP;
        case SDL_SCANCODE_AGAIN: return KEY_REDO;
        case SDL_SCANCODE_UNDO: return KEY_UNDO;
        case SDL_SCANCODE_CUT: return KEY_CUT;
        case SDL_SCANCODE_COPY: return KEY_COPY;
        case SDL_SCANCODE_PASTE: return KEY_PASTE;
        case SDL_SCANCODE_FIND: return KEY_SEARCH;
        case SDL_SCANCODE_ALTERASE: return KEY_ALTERASE;

        // Media
        case SDL_SCANCODE_MUTE: return KEY_MUTE;
        case SDL_SCANCODE_VOLUMEUP: return KEY_VOLUMEUP;
        case SDL_SCANCODE_VOLUMEDOWN: return KEY_VOLUMEDOWN;
        case SDL_SCANCODE_MEDIA_PLAY_PAUSE: return KEY_PLAYPAUSE;
        case SDL_SCANCODE_MEDIA_STOP: return KEY_STOPCD;
        case SDL_SCANCODE_MEDIA_NEXT_TRACK: return KEY_NEXTSONG;
        case SDL_SCANCODE_MEDIA_PREVIOUS_TRACK: return KEY_PREVIOUSSONG;

        // Numpad extended
        case SDL_SCANCODE_KP_COMMA: return KEY_KPCOMMA;
        case SDL_SCANCODE_KP_EQUALSAS400: return KEY_KPEQUAL;
        case SDL_SCANCODE_KP_00: return KEY_KP0;
        case SDL_SCANCODE_KP_000: return KEY_KP0;
        case SDL_SCANCODE_KP_LEFTPAREN: return KEY_KPLEFTPAREN;
        case SDL_SCANCODE_KP_RIGHTPAREN: return KEY_KPRIGHTPAREN;
        case SDL_SCANCODE_KP_PLUSMINUS: return KEY_KPPLUSMINUS;
        case SDL_SCANCODE_KP_POWER: return KEY_POWER;
        case SDL_SCANCODE_KP_SPACE: return KEY_SPACE;
        case SDL_SCANCODE_KP_MEMCLEAR: return KEY_CLEAR;
        case SDL_SCANCODE_KP_CLEAR: return KEY_CLEAR;
        case SDL_SCANCODE_KP_CLEARENTRY: return KEY_CLEAR;

        // System
        case SDL_SCANCODE_NONUSHASH: return KEY_102ND;
        case SDL_SCANCODE_SYSREQ: return KEY_SYSRQ;
        case SDL_SCANCODE_CANCEL: return KEY_CANCEL;
        case SDL_SCANCODE_CLEAR: return KEY_CLEAR;
        case SDL_SCANCODE_CLEARAGAIN: return KEY_CLEAR;
        case SDL_SCANCODE_CRSEL: return KEY_SELECT;
        case SDL_SCANCODE_EXSEL: return KEY_SELECT;
        case SDL_SCANCODE_MODE: return KEY_MODE;

        // AC (Application Control)
        case SDL_SCANCODE_AC_BACK: return KEY_BACK;
        case SDL_SCANCODE_AC_FORWARD: return KEY_FORWARD;
        case SDL_SCANCODE_AC_REFRESH: return KEY_REFRESH;
        case SDL_SCANCODE_AC_STOP: return KEY_STOP;
        case SDL_SCANCODE_AC_SEARCH: return KEY_SEARCH;
        case SDL_SCANCODE_AC_BOOKMARKS: return KEY_FAVORITES;
        case SDL_SCANCODE_AC_HOME: return KEY_HOMEPAGE;
        case SDL_SCANCODE_AC_NEW: return KEY_NEW;
        case SDL_SCANCODE_AC_OPEN: return KEY_OPEN;
        case SDL_SCANCODE_AC_CLOSE: return KEY_CLOSE;
        case SDL_SCANCODE_AC_EXIT: return KEY_EXIT;
        case SDL_SCANCODE_AC_SAVE: return KEY_SAVE;
        case SDL_SCANCODE_AC_PRINT: return KEY_PRINT;
        case SDL_SCANCODE_AC_PROPERTIES: return KEY_PROPS;

        // Media extended
        case SDL_SCANCODE_MEDIA_PLAY: return KEY_PLAY;
        case SDL_SCANCODE_MEDIA_PAUSE: return KEY_PAUSECD;
        case SDL_SCANCODE_MEDIA_RECORD: return KEY_RECORD;
        case SDL_SCANCODE_MEDIA_FAST_FORWARD: return KEY_FASTFORWARD;
        case SDL_SCANCODE_MEDIA_REWIND: return KEY_REWIND;
        case SDL_SCANCODE_MEDIA_EJECT: return KEY_EJECTCD;
        case SDL_SCANCODE_MEDIA_SELECT: return KEY_MEDIA;
        case SDL_SCANCODE_CHANNEL_INCREMENT: return KEY_CHANNELUP;
        case SDL_SCANCODE_CHANNEL_DECREMENT: return KEY_CHANNELDOWN;

        // Power / phone
        case SDL_SCANCODE_SLEEP: return KEY_SLEEP;
        case SDL_SCANCODE_WAKE: return KEY_WAKEUP;
        case SDL_SCANCODE_POWER: return KEY_POWER;
        case SDL_SCANCODE_CALL: return KEY_PHONE;
        case SDL_SCANCODE_ENDCALL: return KEY_PHONE;

        default: return KEY_RESERVED;
    }
}

} // namespace uemu::ui
