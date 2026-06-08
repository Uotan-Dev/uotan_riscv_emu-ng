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

#ifdef UEMU_UI_GTK4

#include <deque>
#include <vector>

#include <gtkmm.h>

#include "ui/ui_backend.hpp"

// Forward-declare VteTerminal (C API; no gtkmm binding exists for VTE)
using VteTerminal = struct _VteTerminal;

namespace uemu::ui {

class GTK4Backend : public UIBackend {
public:
    GTK4Backend(Endpoints endpoints, ExitCallback exit_callback);
    ~GTK4Backend() override;

    void update() override;

private:
    void setup_simplefb_tab();
    void setup_ns16550_tab();
    void update_framebuffer();
    void pump_vte_io();

    bool on_window_key_pressed(guint keyval, guint keycode,
                               Gdk::ModifierType state);
    void on_window_key_released(guint keyval, guint keycode,
                                Gdk::ModifierType state);
    bool on_simplefb_key_pressed(guint keyval, guint keycode,
                                 Gdk::ModifierType state);
    void on_simplefb_key_released(guint keyval, guint keycode,
                                  Gdk::ModifierType state);
    bool on_vte_key_pressed(guint keyval, guint keycode,
                            Gdk::ModifierType state);
    static InputSink::linux_event_code_t
    gdk_keyval_to_linux(guint keyval) noexcept;

    // GTK widgets (owned by GTK widget hierarchy, not RefPtr)
    Gtk::Window* window_ = nullptr;
    Gtk::Notebook* notebook_ = nullptr;

    // SimpleFB tab widgets.
    // fb_picture_ is wrapped in fb_box_ because Gtk::Picture cannot receive
    // keyboard focus. The focusable Box ensures GTK4 delivers key events
    // (otherwise GTK may discard events when no widget has focus).
    Gtk::Box* fb_box_ = nullptr;
    Gtk::Picture* fb_picture_ = nullptr;
    GdkTexture* fb_texture_ = nullptr;
    std::vector<uint8_t> pixel_buffer_;

    // NS16550 tab widgets
    VteTerminal* vte_terminal_ = nullptr; // C API — no gtkmm wrapper exists

    // Window-level CAPTURE-phase key controller — fires before any child
    // widget, routing to SimpleFB or NS16550 handler based on active tab.
    Glib::RefPtr<Gtk::EventControllerKey> window_key_controller_;

    // Terminal I/O buffering
    std::deque<uint8_t> pending_output_;
    static constexpr size_t MAX_PENDING_OUTPUT = 4096;

    // Tab tracking: which tab index is currently active
    int active_tab_ = 0;

    static bool gtk_initialized_;
};

} // namespace uemu::ui

#endif // UEMU_UI_GTK4
