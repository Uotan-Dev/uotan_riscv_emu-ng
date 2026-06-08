/*
 * Copyright 2025 Nuo Shen, Nanjing University
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

#include <atomic>
#include <cstdlib>
#include <filesystem>
#include <print>
#include <thread>

#include <CLI/CLI.hpp>

#include "emulator.hpp"
#include "ui/headless_backend.hpp"
#include "ui/sfml3_backend.hpp"

int main(int argc, char* argv[]) {
    CLI::App app{"uemu-ng: RISC-V Emulator"};
    app.set_version_flag("-v,--version", "1.0.0");

    std::filesystem::path elf_file;
    std::filesystem::path disk_file;
    std::filesystem::path flash0_file, flash1_file;
    size_t dram_size_mb = 512;
    uint64_t timeout_ms = 0;
    bool headless = false;

    // Configure command line options
    app.add_option("-f,--file", elf_file, "ELF file to load")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("-m,--memory", dram_size_mb, "DRAM size in MB")
        ->default_val(512)
        ->check(CLI::Range(64, 16384));
    app.add_option("-d,--disk", disk_file, "Disk file to use");
    app.add_option("--flash0", flash0_file, "Flash0 file to use");
    app.add_option("--flash1", flash1_file, "Flash1 file to use");
    app.add_option("-t,--timeout", timeout_ms,
                   "Execution timeout in milliseconds (0 = no timeout)")
        ->default_val(0);
    app.add_flag("--headless", headless, "Run in headless mode (no UI window)");

    // Parse command line
    CLI11_PARSE(app, argc, argv);

    try {
        size_t dram_size = dram_size_mb * 1024 * 1024;

        std::println("Initializing emulator...");
        std::println("  DRAM size: {} MB ({} bytes)", dram_size_mb, dram_size);
        std::println("  ELF file: {}", elf_file.string());

        if (timeout_ms > 0)
            std::println("  Timeout: {} ms", timeout_ms);

        // 1. Create UI backend first (parameterless)
        std::unique_ptr<uemu::ui::UIBackend> ui_backend;
        if (headless)
            ui_backend = std::make_unique<uemu::ui::HeadlessBackend>();
        else
            ui_backend = std::make_unique<uemu::ui::SFML3Backend>();

        // 2. Create emulator (no more headless param)
        uemu::Emulator emulator(dram_size, disk_file, flash0_file,
                                flash1_file);
        emulator.loadelf(elf_file);

        // 3. Wire endpoints
        ui_backend->set_endpoints(emulator.get_ui_endpoints());

        // 4. Engine on worker thread
        std::atomic<bool> engine_alive = true;
        std::thread engine_thread([&]() -> void {
            emulator.run(std::chrono::milliseconds(timeout_ms));
            engine_alive.store(false, std::memory_order::release);
        });

        // 5. UI owns main thread
        ui_backend->run([&]() -> bool {
            return engine_alive.load(std::memory_order::acquire);
        });

        // 6. Cleanup
        if (engine_thread.joinable())
            engine_thread.join();
    } catch (const std::runtime_error& e) {
        std::println(stderr, "Runtime error: {}", e.what());
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::println(stderr, "Exception: {}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        std::println("Unknown Error");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
