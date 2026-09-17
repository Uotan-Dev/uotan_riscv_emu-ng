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

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <adwaita.h>
#include <vte/vte.h>

#include "common/log.hpp"
#include "core/framebuffer.hpp"
#include "core/input.hpp"
#include "emulator.hpp"
#include "frontend/gtk4_frontend.hpp"

extern "C" {
#include "linux/input-event-codes.h" // IWYU pragma: keep
}

// The embedded PNG icon, shared with the SDL3 frontend.
extern "C" {
extern const uint8_t uotan_icon_data[];
extern const ptrdiff_t uotan_icon_size;
}

namespace uemu::frontend {

class Gtk4Frontend::Impl {
public:
    explicit Impl(Emulator& emulator) : emulator_(emulator) {}

    ~Impl() {
        if (application_) {
            destroy_windows();
            g_clear_object(&application_);
        }

        g_clear_object(&window_icon_);
    }

    void run(std::chrono::milliseconds timeout) {
        timeout_ = timeout;
        application_ =
            adw_application_new("dev.uotan.uemu", G_APPLICATION_NON_UNIQUE);
        g_signal_connect(application_, "activate", G_CALLBACK(on_activate),
                         this);

        const int status =
            g_application_run(G_APPLICATION(application_), 0, nullptr);

        if (tick_source_ != 0) {
            g_source_remove(tick_source_);
            tick_source_ = 0;
        }

        std::exception_ptr worker_error;

        if (started_ && !workers_joined_) {
            emulator_.request_shutdown();

            try {
                wait_for_workers();
            } catch (...) { worker_error = std::current_exception(); }
        }

        if (started_) {
            // Drain bytes written between the last UI tick and worker exit.
            try {
                present();
            } catch (...) {
                if (!callback_error_)
                    callback_error_ = std::current_exception();
            }
        }

        quitting_ = true;
        destroy_windows();
        g_clear_object(&application_);

        if (callback_error_)
            std::rethrow_exception(callback_error_);
        if (worker_error)
            std::rethrow_exception(worker_error);
        if (status != 0)
            throw std::runtime_error("GTK4 application exited with status " +
                                     std::to_string(status));

        if (started_)
            log::info("Emulator shutdown with code 0x{:x} and status 0x{:x}",
                      emulator_.shutdown_code(), emulator_.shutdown_status());
    }

private:
    struct ConsoleView {
        Impl* frontend;
        size_t console;
        VteTerminal* terminal;
    };

    struct WindowView {
        AdwApplicationWindow* window;
        AdwTabView* tabs;
        AdwBanner* banner;
        GtkLabel* title;
    };

    struct PasteRequest {
        Impl* frontend;
        size_t console;
    };

    static constexpr guint UI_INTERVAL_MS = 16;
    static constexpr glong SCROLLBACK_LINES = 10000;
    // Matches the size GTK uses for the window-control icon.
    static constexpr int HEADER_ICON_SIZE = 16;

    static void on_activate(GApplication*, gpointer data) noexcept {
        auto* self = static_cast<Impl*>(data);

        try {
            self->activate();
        } catch (...) { self->fail(std::current_exception()); }
    }

    static gboolean on_tick(gpointer data) noexcept {
        auto* self = static_cast<Impl*>(data);

        try {
            if (self->tick())
                return G_SOURCE_CONTINUE;
        } catch (...) { self->fail(std::current_exception()); }

        self->tick_source_ = 0;
        return G_SOURCE_REMOVE;
    }

    static gboolean on_main_close(GtkWindow*, gpointer data) noexcept {
        auto* self = static_cast<Impl*>(data);

        try {
            if (self->guest_stopped_) {
                self->quit_application();
            } else if (self->emulator_.finished()) {
                self->finish_guest();
                self->quit_application();
            } else {
                self->confirm_main_close();
            }
        } catch (...) { self->fail(std::current_exception()); }

        return GDK_EVENT_STOP;
    }

    static void on_close_confirmation(GObject* source, GAsyncResult* result,
                                      gpointer data) noexcept {
        auto* self = static_cast<Impl*>(data);
        self->close_confirmation_pending_ = false;

        try {
            GError* error = nullptr;
            const int response = gtk_alert_dialog_choose_finish(
                GTK_ALERT_DIALOG(source), result, &error);

            if (error) {
                const std::string message(error->message);
                g_error_free(error);
                throw std::runtime_error("Failed to show close confirmation: " +
                                         message);
            }

            if (response == 1)
                self->request_exit();
        } catch (...) { self->fail(std::current_exception()); }
    }

    static gboolean on_detached_close(GtkWindow* window,
                                      gpointer data) noexcept {
        auto* self = static_cast<Impl*>(data);

        try {
            self->dock_window(ADW_APPLICATION_WINDOW(window));
        } catch (...) { self->fail(std::current_exception()); }

        return GDK_EVENT_STOP;
    }

    static void on_detached_destroy(GtkWidget* window, gpointer data) noexcept {
        auto* self = static_cast<Impl*>(data);

        std::erase_if(self->detached_windows_,
                      [window](const WindowView& view) {
                          return GTK_WIDGET(view.window) == window;
                      });
    }

    static void on_detached_page_selected(AdwTabView* tabs, GParamSpec*,
                                          gpointer data) noexcept {
        auto* self = static_cast<Impl*>(data);

        try {
            self->update_detached_title(tabs);
        } catch (...) { self->fail(std::current_exception()); }
    }

    static gboolean on_close_page(AdwTabView* view, AdwTabPage* page,
                                  gpointer) noexcept {
        adw_tab_view_close_page_finish(view, page, FALSE);
        return GDK_EVENT_STOP;
    }

    static AdwTabView* on_create_window(AdwTabView*, gpointer data) noexcept {
        auto* self = static_cast<Impl*>(data);

        try {
            return self->create_detached_window();
        } catch (...) {
            self->fail(std::current_exception());
            return nullptr;
        }
    }

    static void on_console_commit(VteTerminal*, const char* text, guint size,
                                  gpointer data) noexcept {
        auto* view = static_cast<ConsoleView*>(data);

        try {
            if (!view->frontend->guest_running())
                return;

            view->frontend->emulator_.console_input(
                view->console, std::string_view(text, size));
        } catch (...) { view->frontend->fail(std::current_exception()); }
    }

    static gboolean on_framebuffer_key_pressed(GtkEventControllerKey*,
                                               guint keyval, guint,
                                               GdkModifierType,
                                               gpointer data) noexcept {
        auto* self = static_cast<Impl*>(data);

        try {
            return self->push_key(keyval, core::KeyEvent::Action::Press)
                       ? GDK_EVENT_STOP
                       : GDK_EVENT_PROPAGATE;
        } catch (...) {
            self->fail(std::current_exception());
            return GDK_EVENT_STOP;
        }
    }

    static void on_framebuffer_key_released(GtkEventControllerKey*,
                                            guint keyval, guint,
                                            GdkModifierType,
                                            gpointer data) noexcept {
        auto* self = static_cast<Impl*>(data);

        try {
            static_cast<void>(
                self->push_key(keyval, core::KeyEvent::Action::Release));
        } catch (...) { self->fail(std::current_exception()); }
    }

    static void on_framebuffer_pressed(GtkGestureClick*, int, double, double,
                                       gpointer data) noexcept {
        static_cast<Impl*>(data)->focus_framebuffer();
    }

    static void on_page_attached(AdwTabView* tabs, AdwTabPage* page, int,
                                 gpointer data) noexcept {
        auto* self = static_cast<Impl*>(data);

        if (!self->framebuffer_picture_ ||
            adw_tab_page_get_child(page) !=
                GTK_WIDGET(self->framebuffer_picture_))
            return;

        // libadwaita selects the attached page right after emitting this
        // signal, so an empty selection also means this page.  Leave the focus
        // alone while another page of the view is still selected.
        AdwTabPage* selected = adw_tab_view_get_selected_page(tabs);

        if (!selected || selected == page)
            self->focus_framebuffer();
    }

    static gboolean on_copy(GtkWidget*, GVariant*, gpointer data) noexcept {
        auto* view = static_cast<ConsoleView*>(data);
        vte_terminal_copy_clipboard_format(view->terminal, VTE_FORMAT_TEXT);
        return TRUE;
    }

    static gboolean on_paste(GtkWidget* widget, GVariant*,
                             gpointer data) noexcept {
        auto* view = static_cast<ConsoleView*>(data);

        try {
            if (!view->frontend->guest_running())
                return TRUE;

            auto request =
                std::make_unique<PasteRequest>(view->frontend, view->console);
            gdk_clipboard_read_text_async(gtk_widget_get_clipboard(widget),
                                          nullptr, on_paste_ready,
                                          request.release());
            return TRUE;
        } catch (...) {
            view->frontend->fail(std::current_exception());
            return FALSE;
        }
    }

    static void on_paste_ready(GObject* source, GAsyncResult* result,
                               gpointer data) noexcept {
        std::unique_ptr<PasteRequest> request(static_cast<PasteRequest*>(data));
        GError* error = nullptr;
        char* text = gdk_clipboard_read_text_finish(GDK_CLIPBOARD(source),
                                                    result, &error);

        if (error) {
            log::warn("Failed to read terminal clipboard: {}", error->message);
            g_error_free(error);
            return;
        }

        if (!text)
            return;

        try {
            if (request->frontend->guest_running())
                request->frontend->emulator_.console_input(request->console,
                                                           text);
        } catch (...) { request->frontend->fail(std::current_exception()); }

        g_free(text);
    }

    void activate() {
        if (activated_)
            return;
        activated_ = true;

        load_window_icon();

        const WindowView main = create_window(true);
        main_window_ = main.window;
        main_tabs_ = main.tabs;
        main_banner_ = main.banner;

        create_pages();
        gtk_window_present(GTK_WINDOW(main_window_));

        emulator_.start();
        started_ = true;
        start_time_ = std::chrono::steady_clock::now();
        tick_source_ = g_timeout_add(UI_INTERVAL_MS, on_tick, this);
    }

    // Decodes the embedded PNG once; every window reuses this texture.
    void load_window_icon() {
        GBytes* bytes = g_bytes_new_static(
            uotan_icon_data, static_cast<size_t>(uotan_icon_size));
        GError* error = nullptr;
        window_icon_ = gdk_texture_new_from_bytes(bytes, &error);
        g_bytes_unref(bytes);

        if (error) {
            log::warn("Failed to load the window icon: {}", error->message);
            g_error_free(error);
            window_icon_ = nullptr;
        }
    }

    static void on_window_realize(GtkWidget* window, gpointer data) noexcept {
        auto* self = static_cast<Impl*>(data);
        GdkTexture* icon = self->window_icon_;

        if (!icon)
            return;

        // A realized GtkWindow is a GtkNative backed by a GdkToplevel.
        GdkSurface* surface = gtk_native_get_surface(GTK_NATIVE(window));

        if (!surface || !GDK_IS_TOPLEVEL(surface))
            return;

        // GDK consumes the list during the call, so only the nodes are ours.
        GList* icons = g_list_prepend(nullptr, icon);
        gdk_toplevel_set_icon_list(GDK_TOPLEVEL(surface), icons);
        g_list_free(icons);
    }

    [[nodiscard]] WindowView create_window(bool main) {
        auto* window = ADW_APPLICATION_WINDOW(
            adw_application_window_new(GTK_APPLICATION(application_)));
        auto* toolbar = ADW_TOOLBAR_VIEW(adw_toolbar_view_new());
        auto* header = ADW_HEADER_BAR(adw_header_bar_new());
        auto* banner = ADW_BANNER(adw_banner_new("Guest has stopped"));
        auto* tab_bar = ADW_TAB_BAR(adw_tab_bar_new());
        auto* tabs = ADW_TAB_VIEW(adw_tab_view_new());
        auto* title = GTK_LABEL(gtk_label_new("uemu-ng"));

        gtk_window_set_title(GTK_WINDOW(window), "uemu-ng");
        gtk_window_set_default_size(GTK_WINDOW(window), 1024, 768);
        gtk_widget_add_css_class(GTK_WIDGET(title), "title-3");
        gtk_widget_add_css_class(GTK_WIDGET(title),
                                 main ? "accent" : "warning");

        adw_tab_bar_set_view(tab_bar, tabs);
        adw_tab_bar_set_autohide(tab_bar, FALSE);
        adw_header_bar_set_title_widget(header, GTK_WIDGET(title));

        if (window_icon_ != nullptr) {
            auto* icon =
                gtk_image_new_from_paintable(GDK_PAINTABLE(window_icon_));
            gtk_image_set_pixel_size(GTK_IMAGE(icon), HEADER_ICON_SIZE);
            adw_header_bar_pack_start(header, icon);
        }

        adw_banner_set_revealed(banner, guest_stopped_);
        adw_toolbar_view_add_top_bar(toolbar, GTK_WIDGET(header));
        adw_toolbar_view_add_top_bar(toolbar, GTK_WIDGET(banner));
        adw_toolbar_view_add_top_bar(toolbar, GTK_WIDGET(tab_bar));
        adw_toolbar_view_set_content(toolbar, GTK_WIDGET(tabs));
        adw_application_window_set_content(window, GTK_WIDGET(toolbar));

        g_signal_connect(tabs, "close-page", G_CALLBACK(on_close_page), this);
        g_signal_connect(tabs, "create-window", G_CALLBACK(on_create_window),
                         this);
        g_signal_connect(tabs, "page-attached", G_CALLBACK(on_page_attached),
                         this);

        g_signal_connect(window, "realize", G_CALLBACK(on_window_realize),
                         this);

        if (main) {
            g_signal_connect(window, "close-request", G_CALLBACK(on_main_close),
                             this);
        } else {
            g_signal_connect(window, "close-request",
                             G_CALLBACK(on_detached_close), this);
            g_signal_connect(window, "destroy", G_CALLBACK(on_detached_destroy),
                             this);
            g_signal_connect(tabs, "notify::selected-page",
                             G_CALLBACK(on_detached_page_selected), this);
        }

        return {window, tabs, banner, title};
    }

    void create_pages() {
        auto* picture = GTK_PICTURE(gtk_picture_new());
        gtk_picture_set_content_fit(picture, GTK_CONTENT_FIT_CONTAIN);
        gtk_widget_set_hexpand(GTK_WIDGET(picture), TRUE);
        gtk_widget_set_vexpand(GTK_WIDGET(picture), TRUE);
        gtk_widget_set_focusable(GTK_WIDGET(picture), TRUE);
        gtk_widget_set_focus_on_click(GTK_WIDGET(picture), TRUE);

        GtkEventController* keyboard = gtk_event_controller_key_new();
        g_signal_connect(keyboard, "key-pressed",
                         G_CALLBACK(on_framebuffer_key_pressed), this);
        g_signal_connect(keyboard, "key-released",
                         G_CALLBACK(on_framebuffer_key_released), this);
        gtk_widget_add_controller(GTK_WIDGET(picture), keyboard);

        GtkGesture* click = gtk_gesture_click_new();
        g_signal_connect(click, "pressed", G_CALLBACK(on_framebuffer_pressed),
                         this);
        gtk_widget_add_controller(GTK_WIDGET(picture),
                                  GTK_EVENT_CONTROLLER(click));
        framebuffer_picture_ = picture;

        AdwTabPage* framebuffer_page =
            adw_tab_view_append(main_tabs_, GTK_WIDGET(picture));
        const std::string_view display_name =
            emulator_.framebuffer().display_name();
        const std::string display_title(display_name);
        adw_tab_page_set_title(framebuffer_page, display_title.c_str());

        consoles_.reserve(emulator_.console_count());

        for (size_t console = 0; console < emulator_.console_count();
             console++) {
            auto* terminal = VTE_TERMINAL(vte_terminal_new());
            const bool accepts_input = emulator_.console_accepts_input(console);

            vte_terminal_set_scrollback_lines(terminal, SCROLLBACK_LINES);
            vte_terminal_set_input_enabled(terminal, accepts_input);
            gtk_widget_set_hexpand(GTK_WIDGET(terminal), TRUE);
            gtk_widget_set_vexpand(GTK_WIDGET(terminal), TRUE);

            consoles_.push_back({this, console, terminal});
            ConsoleView& view = consoles_.back();

            if (accepts_input) {
                g_signal_connect(terminal, "commit",
                                 G_CALLBACK(on_console_commit), &view);
                add_terminal_shortcuts(view);
            } else {
                add_copy_shortcut(view);
            }

            AdwTabPage* page =
                adw_tab_view_append(main_tabs_, GTK_WIDGET(terminal));
            const std::string title = emulator_.console_name(console);
            adw_tab_page_set_title(page, title.c_str());
        }

        present();
    }

    [[nodiscard]] bool push_key(guint keyval, core::KeyEvent::Action action) {
        if (!guest_running())
            return false;

        const uint32_t code = gdk_key_to_linux(keyval);

        if (code == KEY_RESERVED)
            return false;

        emulator_.push_key_event({.code = code, .action = action});
        return true;
    }

    [[nodiscard]] static constexpr uint32_t
    gdk_key_to_linux(guint keyval) noexcept {
        switch (keyval) {
            // Letters
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

            // Number row, including its shifted symbols.
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

            // Basic keys and punctuation.
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

            // Modifiers.
            case GDK_KEY_Shift_L: return KEY_LEFTSHIFT;
            case GDK_KEY_Shift_R: return KEY_RIGHTSHIFT;
            case GDK_KEY_Control_L: return KEY_LEFTCTRL;
            case GDK_KEY_Control_R: return KEY_RIGHTCTRL;
            case GDK_KEY_Alt_L: return KEY_LEFTALT;
            case GDK_KEY_Alt_R:
            case GDK_KEY_ISO_Level3_Shift: return KEY_RIGHTALT;
            case GDK_KEY_Meta_L:
            case GDK_KEY_Super_L: return KEY_LEFTMETA;
            case GDK_KEY_Meta_R:
            case GDK_KEY_Super_R: return KEY_RIGHTMETA;
            case GDK_KEY_Caps_Lock: return KEY_CAPSLOCK;

            // Navigation.
            case GDK_KEY_Up: return KEY_UP;
            case GDK_KEY_Down: return KEY_DOWN;
            case GDK_KEY_Left: return KEY_LEFT;
            case GDK_KEY_Right: return KEY_RIGHT;
            case GDK_KEY_Insert: return KEY_INSERT;
            case GDK_KEY_Delete: return KEY_DELETE;
            case GDK_KEY_Home: return KEY_HOME;
            case GDK_KEY_End: return KEY_END;
            case GDK_KEY_Page_Up: return KEY_PAGEUP;
            case GDK_KEY_Page_Down: return KEY_PAGEDOWN;

            // Function keys.
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

            // Numeric keypad.  GTK reports navigation keyvals when Num Lock
            // is off; those still identify the same physical keypad keys.
            case GDK_KEY_KP_0:
            case GDK_KEY_KP_Insert: return KEY_KP0;
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
            case GDK_KEY_KP_Enter: return KEY_KPENTER;
            case GDK_KEY_KP_Add: return KEY_KPPLUS;
            case GDK_KEY_KP_Subtract: return KEY_KPMINUS;
            case GDK_KEY_KP_Multiply: return KEY_KPASTERISK;
            case GDK_KEY_KP_Divide: return KEY_KPSLASH;
            case GDK_KEY_KP_Decimal:
            case GDK_KEY_KP_Delete: return KEY_KPDOT;
            case GDK_KEY_Num_Lock: return KEY_NUMLOCK;

            default: return KEY_RESERVED;
        }
    }

    static GtkShortcut* make_shortcut(const char* trigger,
                                      GtkShortcutFunc callback,
                                      ConsoleView& view) {
        return gtk_shortcut_new(
            gtk_shortcut_trigger_parse_string(trigger),
            gtk_callback_action_new(callback, &view, nullptr));
    }

    static GtkShortcutController& add_shortcut_controller(ConsoleView& view) {
        auto* controller =
            GTK_SHORTCUT_CONTROLLER(gtk_shortcut_controller_new());
        gtk_widget_add_controller(GTK_WIDGET(view.terminal),
                                  GTK_EVENT_CONTROLLER(controller));
        return *controller;
    }

    static void add_copy_shortcut(ConsoleView& view) {
        GtkShortcutController& controller = add_shortcut_controller(view);
        gtk_shortcut_controller_add_shortcut(
            &controller, make_shortcut("<Control><Shift>c", on_copy, view));
    }

    static void add_terminal_shortcuts(ConsoleView& view) {
        GtkShortcutController& controller = add_shortcut_controller(view);
        gtk_shortcut_controller_add_shortcut(
            &controller, make_shortcut("<Control><Shift>c", on_copy, view));
        gtk_shortcut_controller_add_shortcut(
            &controller, make_shortcut("<Control><Shift>v", on_paste, view));
    }

    [[nodiscard]] AdwTabView* create_detached_window() {
        const WindowView view = create_window(false);
        detached_windows_.push_back(view);
        update_detached_title(view.tabs);
        gtk_window_present(GTK_WINDOW(view.window));
        return view.tabs;
    }

    void update_detached_title(AdwTabView* tabs) {
        const auto it = std::ranges::find_if(
            detached_windows_,
            [tabs](const WindowView& view) { return view.tabs == tabs; });

        if (it == detached_windows_.end())
            return;

        AdwTabPage* page = adw_tab_view_get_selected_page(tabs);
        const char* title = page ? adw_tab_page_get_title(page) : nullptr;

        if (!title || *title == '\0')
            title = "uemu-ng";

        gtk_label_set_text(it->title, title);
        gtk_window_set_title(GTK_WINDOW(it->window), title);
    }

    void dock_window(AdwApplicationWindow* window) {
        if (quitting_)
            return;

        const auto it = std::ranges::find_if(
            detached_windows_,
            [window](const WindowView& view) { return view.window == window; });

        if (it == detached_windows_.end())
            return;

        while (adw_tab_view_get_n_pages(it->tabs) > 0) {
            AdwTabPage* page = adw_tab_view_get_nth_page(it->tabs, 0);
            adw_tab_view_transfer_page(it->tabs, page, main_tabs_,
                                       adw_tab_view_get_n_pages(main_tabs_));
        }

        gtk_window_destroy(GTK_WINDOW(window));
    }

    bool tick() {
        present();

        if (emulator_.finished()) {
            finish_guest();
            return false;
        }

        if (timeout_.count() > 0) {
            const auto elapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - start_time_);

            if (elapsed >= timeout_) {
                log::warn("Execution timeout reached ({} ms), shutting down...",
                          timeout_.count());
                request_exit();
                return false;
            }
        }

        return true;
    }

    void finish_guest() {
        wait_for_workers();

        // Capture output produced immediately before the workers exited.
        present();

        guest_stopped_ = true;
        for (const ConsoleView& view : consoles_)
            vte_terminal_set_input_enabled(view.terminal, FALSE);

        if (main_banner_)
            adw_banner_set_revealed(main_banner_, TRUE);
        for (const WindowView& view : detached_windows_)
            adw_banner_set_revealed(view.banner, TRUE);
    }

    void wait_for_workers() {
        if (!started_ || workers_joined_)
            return;

        try {
            emulator_.wait();
        } catch (...) {
            workers_joined_ = true;
            throw;
        }

        workers_joined_ = true;
    }

    void present() {
        for (const ConsoleView& view : consoles_) {
            if (std::string bytes = emulator_.console_output(view.console);
                !bytes.empty())
                vte_terminal_feed(view.terminal, bytes.data(),
                                  static_cast<gssize>(bytes.size()));
        }

        if (!framebuffer_picture_)
            return;

        const core::Framebuffer& framebuffer = emulator_.framebuffer();
        const size_t size = framebuffer.byte_size();
        void* pixels = g_malloc(size);

        {
            std::unique_lock<std::mutex> lock = framebuffer.lock();
            std::memcpy(pixels, framebuffer.pixels(), size);
        }

        GBytes* bytes = g_bytes_new_take(pixels, size);
        GdkTexture* texture = gdk_memory_texture_new(
            static_cast<int>(framebuffer.width()),
            static_cast<int>(framebuffer.height()), GDK_MEMORY_B8G8R8X8, bytes,
            framebuffer.width() * 4);
        gtk_picture_set_paintable(framebuffer_picture_, GDK_PAINTABLE(texture));
        g_object_unref(texture);
        g_bytes_unref(bytes);
    }

    void focus_framebuffer() noexcept {
        if (framebuffer_picture_)
            gtk_widget_grab_focus(GTK_WIDGET(framebuffer_picture_));
    }

    [[nodiscard]] bool guest_running() const noexcept {
        return started_ && !guest_stopped_ && !quitting_ &&
               !emulator_.finished();
    }

    void confirm_main_close() {
        if (close_confirmation_pending_ || quitting_)
            return;

        auto* dialog = gtk_alert_dialog_new("Stop the virtual machine?");
        constexpr const char* BUTTONS[] = {"Cancel", "Stop", nullptr};

        gtk_alert_dialog_set_detail(
            dialog,
            "The guest is still running. Closing the main window will stop "
            "it.");
        gtk_alert_dialog_set_buttons(dialog, BUTTONS);
        gtk_alert_dialog_set_cancel_button(dialog, 0);
        gtk_alert_dialog_set_default_button(dialog, 0);

        close_confirmation_pending_ = true;
        gtk_alert_dialog_choose(dialog, GTK_WINDOW(main_window_), nullptr,
                                on_close_confirmation, this);
        g_object_unref(dialog);
    }

    void quit_application() noexcept {
        if (quitting_)
            return;

        quitting_ = true;
        if (application_)
            g_application_quit(G_APPLICATION(application_));
    }

    void request_exit() noexcept {
        if (quitting_)
            return;

        if (started_ && !workers_joined_)
            emulator_.request_shutdown();
        quit_application();
    }

    void fail(std::exception_ptr error) noexcept {
        if (!callback_error_)
            callback_error_ = std::move(error);
        request_exit();
    }

    void destroy_windows() noexcept {
        while (!detached_windows_.empty()) {
            GtkWindow* window = GTK_WINDOW(detached_windows_.back().window);
            gtk_window_destroy(window);
        }

        if (main_window_) {
            gtk_window_destroy(GTK_WINDOW(main_window_));
            main_window_ = nullptr;
            main_tabs_ = nullptr;
            main_banner_ = nullptr;
            framebuffer_picture_ = nullptr;
        }

        consoles_.clear();
    }

    Emulator& emulator_;
    AdwApplication* application_ = nullptr;
    AdwApplicationWindow* main_window_ = nullptr;
    AdwTabView* main_tabs_ = nullptr;
    AdwBanner* main_banner_ = nullptr;
    GtkPicture* framebuffer_picture_ = nullptr;
    GdkTexture* window_icon_ = nullptr;
    std::vector<ConsoleView> consoles_;
    std::vector<WindowView> detached_windows_;
    guint tick_source_ = 0;
    std::chrono::milliseconds timeout_{};
    std::chrono::steady_clock::time_point start_time_{};
    std::exception_ptr callback_error_;
    bool activated_ = false;
    bool started_ = false;
    bool workers_joined_ = false;
    bool guest_stopped_ = false;
    bool close_confirmation_pending_ = false;
    bool quitting_ = false;
};

Gtk4Frontend::Gtk4Frontend(Emulator& emulator)
    : Frontend(emulator), impl_(std::make_unique<Impl>(emulator)) {}

Gtk4Frontend::~Gtk4Frontend() = default;

void Gtk4Frontend::run(std::chrono::milliseconds timeout) {
    impl_->run(timeout);
}

void Gtk4Frontend::poll_input() {}

void Gtk4Frontend::present() {}

} // namespace uemu::frontend
