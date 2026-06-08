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

#ifdef UEMU_UI_GTK4

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

#include <vte/vte.h>

#include "ui/gtk4_backend.hpp"

#include "linux/input-event-codes.h"

namespace uemu::ui {

bool GTK4Backend::gtk_initialized_ = false;

GTK4Backend::GTK4Backend(Endpoints endpoints, ExitCallback exit_callback)
    : UIBackend(std::move(endpoints), std::move(exit_callback)) {
    if (gtk_initialized_)
        throw std::runtime_error("Double GTK4Backend instances.");

    gtk_init();
    gtk_initialized_ = true;

    // ---- Window ----
    window_ = new Gtk::Window();
    window_->set_title("Uotan RISC-V Emulator (Next Generation)");
    window_->set_default_size(1024, 768);
    window_->signal_close_request().connect(
        [this]() -> bool {
            // Match SFML3 behavior: just request exit. GTK events MUST
            // keep being processed so GTK can finalize internal state
            // (the close-request signal and any cleanup callbacks).
            request_exit();
            window_->set_visible(false);
            return true;
        },
        false);

    // ---- Notebook (tab bar) ----
    notebook_ = Gtk::make_managed<Gtk::Notebook>();
    notebook_->set_show_tabs(true);
    notebook_->set_show_border(false);
    notebook_->set_scrollable(true);
    notebook_->property_page().signal_changed().connect(
        [this]() {
            active_tab_ = notebook_->get_current_page();
            // When switching to the SimpleFB tab, grab focus on the
            // Box so GTK4 has a valid focus target for keyboard events
            // (Gtk::Picture cannot receive focus; without a focus
            // target GTK4 may discard key events entirely).
            if (active_tab_ == 0 && fb_box_)
                fb_box_->grab_focus();
        });
    window_->set_child(*notebook_);

    // ---- Add tabs based on available endpoints ----
    if (endpoints_.pixel_source) {
        const std::string label = endpoints_.pixel_source->tag_name();
        setup_simplefb_tab();
        Gtk::Label tab_label(label);
        notebook_->append_page(*fb_box_, tab_label);
    }

    if (endpoints_.byte_source || endpoints_.byte_sink) {
        const std::string label = endpoints_.byte_source
                                      ? endpoints_.byte_source->tag_name()
                                      : "Console";
        setup_ns16550_tab();
        gtk_notebook_append_page(
            GTK_NOTEBOOK(notebook_->gobj()),
            GTK_WIDGET(vte_terminal_),
            gtk_label_new(label.c_str()));
    }

    // ---- Single window-level key controller (CAPTURE phase) ----
    // Gtk::Picture cannot receive keyboard focus, so per-widget TARGET-phase
    // controllers never fire. Instead, capture all keys at the window level
    // and route based on the active tab.
    window_key_controller_ = Gtk::EventControllerKey::create();
    window_key_controller_->set_propagation_phase(
        Gtk::PropagationPhase::CAPTURE);
    window_key_controller_->signal_key_pressed().connect(
        sigc::mem_fun(*this, &GTK4Backend::on_window_key_pressed), false);
    window_key_controller_->signal_key_released().connect(
        sigc::mem_fun(*this, &GTK4Backend::on_window_key_released), false);
    window_->add_controller(window_key_controller_);

    window_->set_visible(true);

    // Grab focus on the initial tab's widget so key events have a valid
    // target from the start. The tab-switch signal is not emitted for the
    // initial tab, so we must do this explicitly.
    if (active_tab_ == 0 && fb_box_)
        fb_box_->grab_focus();
}

GTK4Backend::~GTK4Backend() {
    if (fb_texture_) {
        g_clear_object(&fb_texture_);
        fb_texture_ = nullptr;
    }
    // Window destroyed by GTK close-request default handler; only destroy
    // if it wasn't closed by the user.
    if (window_) {
        gtk_window_destroy(window_->gobj());
        window_ = nullptr;
    }
    gtk_initialized_ = false;
}

// --------------- Keyboard routing ---------------

bool GTK4Backend::on_window_key_pressed(guint keyval, guint keycode,
                                        Gdk::ModifierType state) {
    // active_tab_ == 0 → SimpleFB (if present), otherwise NS16550
    if (active_tab_ == 0 && endpoints_.input_sink) {
        return on_simplefb_key_pressed(keyval, keycode, state);
    }
    if (endpoints_.byte_sink) {
        return on_vte_key_pressed(keyval, keycode, state);
    }
    return false;
}

void GTK4Backend::on_window_key_released(guint keyval, guint keycode,
                                         Gdk::ModifierType state) {
    if (active_tab_ == 0 && endpoints_.input_sink)
        on_simplefb_key_released(keyval, keycode, state);
}

// --------------- SimpleFB tab ---------------

void GTK4Backend::setup_simplefb_tab() {
    // Wrap Picture in a focusable Box — Gtk::Picture cannot receive
    // keyboard focus, and without a valid focus target GTK4 may
    // discard key events before they even enter the propagation
    // system. The Box acts as the focus anchor for this tab.
    fb_box_ = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    fb_box_->set_hexpand(true);
    fb_box_->set_vexpand(true);
    fb_box_->set_focusable(true);

    fb_picture_ = Gtk::make_managed<Gtk::Picture>();
    fb_picture_->set_hexpand(true);
    fb_picture_->set_vexpand(true);
    fb_picture_->set_can_shrink(false);

    fb_box_->append(*fb_picture_);
}

bool GTK4Backend::on_simplefb_key_pressed(guint keyval, guint /* keycode */,
                                          Gdk::ModifierType /* state */) {
    if (!endpoints_.input_sink) [[unlikely]]
        return false;

    auto code = gdk_keyval_to_linux(keyval);
    if (code == KEY_RESERVED)
        return false;

    endpoints_.input_sink->push_key_event({code, InputSink::KeyAction::Press});
    return true;
}

void GTK4Backend::on_simplefb_key_released(guint keyval, guint /* keycode */,
                                           Gdk::ModifierType /* state */) {
    if (!endpoints_.input_sink) [[unlikely]]
        return;

    auto code = gdk_keyval_to_linux(keyval);
    if (code == KEY_RESERVED)
        return;

    endpoints_.input_sink->push_key_event(
        {code, InputSink::KeyAction::Release});
}

InputSink::linux_event_code_t
GTK4Backend::gdk_keyval_to_linux(guint keyval) noexcept {
    switch (keyval) {
        case GDK_KEY_a:
        case GDK_KEY_A: return KEY_A;
        case GDK_KEY_b:
        case GDK_KEY_B: return KEY_B;
        case GDK_KEY_c:
        case GDK_KEY_C: return KEY_C;
        case GDK_KEY_d:
        case GDK_KEY_D: return KEY_D;
        case GDK_KEY_e:
        case GDK_KEY_E: return KEY_E;
        case GDK_KEY_f:
        case GDK_KEY_F: return KEY_F;
        case GDK_KEY_g:
        case GDK_KEY_G: return KEY_G;
        case GDK_KEY_h:
        case GDK_KEY_H: return KEY_H;
        case GDK_KEY_i:
        case GDK_KEY_I: return KEY_I;
        case GDK_KEY_j:
        case GDK_KEY_J: return KEY_J;
        case GDK_KEY_k:
        case GDK_KEY_K: return KEY_K;
        case GDK_KEY_l:
        case GDK_KEY_L: return KEY_L;
        case GDK_KEY_m:
        case GDK_KEY_M: return KEY_M;
        case GDK_KEY_n:
        case GDK_KEY_N: return KEY_N;
        case GDK_KEY_o:
        case GDK_KEY_O: return KEY_O;
        case GDK_KEY_p:
        case GDK_KEY_P: return KEY_P;
        case GDK_KEY_q:
        case GDK_KEY_Q: return KEY_Q;
        case GDK_KEY_r:
        case GDK_KEY_R: return KEY_R;
        case GDK_KEY_s:
        case GDK_KEY_S: return KEY_S;
        case GDK_KEY_t:
        case GDK_KEY_T: return KEY_T;
        case GDK_KEY_u:
        case GDK_KEY_U: return KEY_U;
        case GDK_KEY_v:
        case GDK_KEY_V: return KEY_V;
        case GDK_KEY_w:
        case GDK_KEY_W: return KEY_W;
        case GDK_KEY_x:
        case GDK_KEY_X: return KEY_X;
        case GDK_KEY_y:
        case GDK_KEY_Y: return KEY_Y;
        case GDK_KEY_z:
        case GDK_KEY_Z: return KEY_Z;

        case GDK_KEY_1:
        case GDK_KEY_exclam: return KEY_1;
        case GDK_KEY_2:
        case GDK_KEY_at: return KEY_2;
        case GDK_KEY_3:
        case GDK_KEY_numbersign: return KEY_3;
        case GDK_KEY_4:
        case GDK_KEY_dollar: return KEY_4;
        case GDK_KEY_5:
        case GDK_KEY_percent: return KEY_5;
        case GDK_KEY_6:
        case GDK_KEY_asciicircum: return KEY_6;
        case GDK_KEY_7:
        case GDK_KEY_ampersand: return KEY_7;
        case GDK_KEY_8:
        case GDK_KEY_asterisk: return KEY_8;
        case GDK_KEY_9:
        case GDK_KEY_parenleft: return KEY_9;
        case GDK_KEY_0:
        case GDK_KEY_parenright: return KEY_0;

        case GDK_KEY_Return: return KEY_ENTER;
        case GDK_KEY_Escape: return KEY_ESC;
        case GDK_KEY_BackSpace: return KEY_BACKSPACE;
        case GDK_KEY_Tab:
        case GDK_KEY_ISO_Left_Tab: return KEY_TAB;
        case GDK_KEY_space: return KEY_SPACE;
        case GDK_KEY_minus:
        case GDK_KEY_underscore: return KEY_MINUS;
        case GDK_KEY_equal:
        case GDK_KEY_plus: return KEY_EQUAL;
        case GDK_KEY_bracketleft:
        case GDK_KEY_braceleft: return KEY_LEFTBRACE;
        case GDK_KEY_bracketright:
        case GDK_KEY_braceright: return KEY_RIGHTBRACE;
        case GDK_KEY_backslash:
        case GDK_KEY_bar: return KEY_BACKSLASH;
        case GDK_KEY_semicolon:
        case GDK_KEY_colon: return KEY_SEMICOLON;
        case GDK_KEY_apostrophe:
        case GDK_KEY_quotedbl: return KEY_APOSTROPHE;
        case GDK_KEY_grave:
        case GDK_KEY_asciitilde: return KEY_GRAVE;
        case GDK_KEY_comma:
        case GDK_KEY_less: return KEY_COMMA;
        case GDK_KEY_period:
        case GDK_KEY_greater: return KEY_DOT;
        case GDK_KEY_slash:
        case GDK_KEY_question: return KEY_SLASH;

        case GDK_KEY_F1: return KEY_F1;
        case GDK_KEY_F2: return KEY_F2;
        case GDK_KEY_F3: return KEY_F3;
        case GDK_KEY_F4: return KEY_F4;
        case GDK_KEY_F5: return KEY_F5;
        case GDK_KEY_F6: return KEY_F6;
        case GDK_KEY_F7: return KEY_F7;
        case GDK_KEY_F8: return KEY_F8;
        case GDK_KEY_F9: return KEY_F9;
        case GDK_KEY_F10: return KEY_F10;
        case GDK_KEY_F11: return KEY_F11;
        case GDK_KEY_F12: return KEY_F12;
        case GDK_KEY_F13: return KEY_F13;
        case GDK_KEY_F14: return KEY_F14;
        case GDK_KEY_F15: return KEY_F15;
        case GDK_KEY_F16: return KEY_F16;
        case GDK_KEY_F17: return KEY_F17;
        case GDK_KEY_F18: return KEY_F18;
        case GDK_KEY_F19: return KEY_F19;
        case GDK_KEY_F20: return KEY_F20;
        case GDK_KEY_F21: return KEY_F21;
        case GDK_KEY_F22: return KEY_F22;
        case GDK_KEY_F23: return KEY_F23;
        case GDK_KEY_F24: return KEY_F24;

        case GDK_KEY_Caps_Lock: return KEY_CAPSLOCK;
        case GDK_KEY_Print: return KEY_SYSRQ;
        case GDK_KEY_Scroll_Lock: return KEY_SCROLLLOCK;
        case GDK_KEY_Pause: return KEY_PAUSE;
        case GDK_KEY_Insert: return KEY_INSERT;
        case GDK_KEY_Home: return KEY_HOME;
        case GDK_KEY_Page_Up: return KEY_PAGEUP;
        case GDK_KEY_Delete: return KEY_DELETE;
        case GDK_KEY_End: return KEY_END;
        case GDK_KEY_Page_Down: return KEY_PAGEDOWN;
        case GDK_KEY_Right: return KEY_RIGHT;
        case GDK_KEY_Left: return KEY_LEFT;
        case GDK_KEY_Down: return KEY_DOWN;
        case GDK_KEY_Up: return KEY_UP;

        case GDK_KEY_Num_Lock: return KEY_NUMLOCK;
        case GDK_KEY_KP_Divide: return KEY_KPSLASH;
        case GDK_KEY_KP_Multiply: return KEY_KPASTERISK;
        case GDK_KEY_KP_Subtract: return KEY_KPMINUS;
        case GDK_KEY_KP_Add: return KEY_KPPLUS;
        case GDK_KEY_KP_Equal: return KEY_KPEQUAL;
        case GDK_KEY_KP_Enter: return KEY_KPENTER;
        case GDK_KEY_KP_Decimal:
        case GDK_KEY_KP_Delete: return KEY_KPDOT;
        case GDK_KEY_KP_1:
        case GDK_KEY_KP_End: return KEY_KP1;
        case GDK_KEY_KP_2:
        case GDK_KEY_KP_Down: return KEY_KP2;
        case GDK_KEY_KP_3:
        case GDK_KEY_KP_Page_Down: return KEY_KP3;
        case GDK_KEY_KP_4:
        case GDK_KEY_KP_Left: return KEY_KP4;
        case GDK_KEY_KP_5:
        case GDK_KEY_KP_Begin: return KEY_KP5;
        case GDK_KEY_KP_6:
        case GDK_KEY_KP_Right: return KEY_KP6;
        case GDK_KEY_KP_7:
        case GDK_KEY_KP_Home: return KEY_KP7;
        case GDK_KEY_KP_8:
        case GDK_KEY_KP_Up: return KEY_KP8;
        case GDK_KEY_KP_9:
        case GDK_KEY_KP_Page_Up: return KEY_KP9;
        case GDK_KEY_KP_0:
        case GDK_KEY_KP_Insert: return KEY_KP0;

        case GDK_KEY_Control_L: return KEY_LEFTCTRL;
        case GDK_KEY_Shift_L: return KEY_LEFTSHIFT;
        case GDK_KEY_Alt_L: return KEY_LEFTALT;
        case GDK_KEY_Super_L:
        case GDK_KEY_Meta_L: return KEY_LEFTMETA;
        case GDK_KEY_Control_R: return KEY_RIGHTCTRL;
        case GDK_KEY_Shift_R: return KEY_RIGHTSHIFT;
        case GDK_KEY_Alt_R:
        case GDK_KEY_ISO_Level3_Shift: return KEY_RIGHTALT;
        case GDK_KEY_Super_R:
        case GDK_KEY_Meta_R: return KEY_RIGHTMETA;
        case GDK_KEY_Menu: return KEY_MENU;

        case GDK_KEY_AudioMute: return KEY_MUTE;
        case GDK_KEY_AudioRaiseVolume: return KEY_VOLUMEUP;
        case GDK_KEY_AudioLowerVolume: return KEY_VOLUMEDOWN;
        case GDK_KEY_AudioPlay: return KEY_PLAYPAUSE;
        case GDK_KEY_AudioStop: return KEY_STOPCD;
        case GDK_KEY_AudioNext: return KEY_NEXTSONG;
        case GDK_KEY_AudioPrev: return KEY_PREVIOUSSONG;

        default: return KEY_RESERVED;
    }
}

// --------------- NS16550 tab ---------------

void GTK4Backend::setup_ns16550_tab() {
    vte_terminal_ = VTE_TERMINAL(vte_terminal_new());
    if (!vte_terminal_) [[unlikely]]
        throw std::runtime_error("Failed to create VteTerminal");

    GtkWidget* w = GTK_WIDGET(vte_terminal_);
    gtk_widget_set_hexpand(w, TRUE);
    gtk_widget_set_vexpand(w, TRUE);

    vte_terminal_set_scrollback_lines(vte_terminal_, 10000);
    vte_terminal_set_scroll_on_output(vte_terminal_, TRUE);
    vte_terminal_set_scroll_on_keystroke(vte_terminal_, TRUE);
    vte_terminal_set_audible_bell(vte_terminal_, FALSE);
    vte_terminal_set_allow_hyperlink(vte_terminal_, FALSE);
    vte_terminal_set_mouse_autohide(vte_terminal_, TRUE);
}

bool GTK4Backend::on_vte_key_pressed(guint keyval, guint /* keycode */,
                                     Gdk::ModifierType state) {
    // Route all key events to ByteSink (serial console input).
    // Return true to consume the event (prevent VTE's default handling).
    if (!endpoints_.byte_sink) [[unlikely]]
        return false;

    uint8_t byte = 0;
    bool has_byte = false;

    if ((state & Gdk::ModifierType::CONTROL_MASK) != Gdk::ModifierType(0) &&
        ((keyval >= GDK_KEY_a && keyval <= GDK_KEY_z) ||
         (keyval >= GDK_KEY_A && keyval <= GDK_KEY_Z))) {
        byte = static_cast<uint8_t>(
            (keyval >= GDK_KEY_a ? keyval - GDK_KEY_a : keyval - GDK_KEY_A) +
            1);
        has_byte = true;
    } else if ((state & Gdk::ModifierType::CONTROL_MASK) !=
                   Gdk::ModifierType(0) &&
               keyval == GDK_KEY_bracketleft) {
        byte = 0x1b; // Ctrl+[ = ESC
        has_byte = true;
    } else if (keyval >= 0x20 && keyval <= 0x7E) {
        byte = static_cast<uint8_t>(keyval);
        has_byte = true;
    } else {
        switch (keyval) {
            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
                byte = '\r';
                has_byte = true;
                break;
            case GDK_KEY_BackSpace:
                byte = 0x7f;
                has_byte = true;
                break;
            case GDK_KEY_Tab:
            case GDK_KEY_ISO_Left_Tab:
                byte = '\t';
                has_byte = true;
                break;
            case GDK_KEY_Escape:
                byte = 0x1b;
                has_byte = true;
                break;
            case GDK_KEY_Up:
                pending_output_.insert(pending_output_.end(),
                                       {'\x1b', '[', 'A'});
                break;
            case GDK_KEY_Down:
                pending_output_.insert(pending_output_.end(),
                                       {'\x1b', '[', 'B'});
                break;
            case GDK_KEY_Right:
                pending_output_.insert(pending_output_.end(),
                                       {'\x1b', '[', 'C'});
                break;
            case GDK_KEY_Left:
                pending_output_.insert(pending_output_.end(),
                                       {'\x1b', '[', 'D'});
                break;
            case GDK_KEY_Home:
                pending_output_.insert(pending_output_.end(),
                                       {'\x1b', '[', 'H'});
                break;
            case GDK_KEY_End:
                pending_output_.insert(pending_output_.end(),
                                       {'\x1b', '[', 'F'});
                break;
            case GDK_KEY_Delete:
                pending_output_.insert(pending_output_.end(),
                                       {'\x1b', '[', '3', '~'});
                break;
            default: break;
        }
    }

    if (has_byte) {
        if (pending_output_.size() < MAX_PENDING_OUTPUT)
            pending_output_.push_back(byte);
    }

    return true; // consume all key events on the VTE tab
}

// --------------- Update / I/O pump ---------------

void GTK4Backend::update() {
    // Process pending GTK events non-blocking.
    // Must always run — even during shutdown — so GTK can finalize
    // internal signal emissions and cleanup callbacks.
    while (g_main_context_pending(nullptr))
        g_main_context_iteration(nullptr, FALSE);

    // Update framebuffer
    if (endpoints_.pixel_source)
        update_framebuffer();

    // Pump VTE terminal I/O
    if (endpoints_.byte_source || endpoints_.byte_sink)
        pump_vte_io();
}

void GTK4Backend::update_framebuffer() {
    using clock = std::chrono::steady_clock;
    using namespace std::chrono_literals;

    static auto last_update = clock::now();
    const auto now = clock::now();
    constexpr auto frame_interval = 16ms + 648us; // ~60 Hz

    if (now - last_update < frame_interval)
        return;

    const int width = static_cast<int>(endpoints_.pixel_source->get_width());
    const int height = static_cast<int>(endpoints_.pixel_source->get_height());
    const size_t size = endpoints_.pixel_source->get_size();

    // Copy pixel data from SimpleFB (hold lock only during copy)
    pixel_buffer_.resize(size);
    {
        std::unique_lock<std::mutex> lock =
            endpoints_.pixel_source->acquire_lock();
        const uint8_t* pixels = endpoints_.pixel_source->get_pixels();
        std::memcpy(pixel_buffer_.data(), pixels, size);
    }

    // Build texture via C API to avoid gtkmm's GdkMemoryTexture wrapping
    // failure (same class of issue as VteTerminal — the C++ wrapper class
    // may not be registered at runtime).
    auto bytes = Glib::Bytes::create(pixel_buffer_.data(), size);

    GdkTexture* new_texture = gdk_memory_texture_new(
        width, height, GDK_MEMORY_B8G8R8X8,
        bytes->gobj(),       // GBytes* — gdk_memory_texture_new refs it
        width * 4);

    // Set via C API to avoid gtkmm trying to wrap the GdkMemoryTexture
    gtk_picture_set_paintable(GTK_PICTURE(fb_picture_->gobj()),
                              GDK_PAINTABLE(new_texture));

    // Replace old texture
    if (fb_texture_)
        g_clear_object(&fb_texture_);
    fb_texture_ = new_texture;

    last_update = now;
}

void GTK4Backend::pump_vte_io() {
    auto* vte = vte_terminal_;

    // Guest → host: pop bytes from ByteSource and feed to VTE display
    if (endpoints_.byte_source) {
        std::array<uint8_t, 256> buffer{};
        size_t count = 0;
        do {
            count = endpoints_.byte_source->pop_bytes(buffer);
            if (count > 0)
                vte_terminal_feed(vte, reinterpret_cast<char*>(buffer.data()),
                                  count);
        } while (count == buffer.size());
    }

    // Host → guest: drain pending output queue to ByteSink
    if (endpoints_.byte_sink && !pending_output_.empty()) {
        std::array<uint8_t, 64> batch{};
        size_t batch_size = 0;

        while (!pending_output_.empty() && batch_size < batch.size()) {
            batch[batch_size] = pending_output_.front();
            pending_output_.pop_front();
            batch_size++;
        }

        size_t accepted = endpoints_.byte_sink->push_bytes(
            std::span<const uint8_t>(batch.data(), batch_size));

        // Put back any bytes that weren't accepted
        for (size_t i = accepted; i < batch_size; i++)
            pending_output_.push_front(batch[batch_size - 1 - (i - accepted)]);
    }
}

} // namespace uemu::ui

#endif // UEMU_UI_GTK4
