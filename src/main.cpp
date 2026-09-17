/*
 * Copyright 2025-2026 Nuo Shen, Nanjing University
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

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

#include <CLI/CLI.hpp>

#include "common/log.hpp"
#include "emulator.hpp"
#include "fdt_generator.hpp"
#include "frontend/gtk4_frontend.hpp"
#include "frontend/headless_frontend.hpp"
#include "frontend/sdl3_frontend.hpp"

int main(int argc, char* argv[]) {
    CLI::App app{"uemu-ng: RISC-V Emulator"};
    app.set_version_flag("-v,--version", UEMU_VERSION);

    // The board is described once (see board_config.hpp); the options below
    // only override the fields the user asked for.
    uemu::BoardConfig config;

    std::filesystem::path elf_file;
    std::filesystem::path dump_dtb_file;
    // The option default is the board's DRAM size, so it stays in one place.
    size_t dram_size_mb = config.dram.size / (1024 * 1024);
    int64_t timeout_ms = 0;
    std::string frontend_name = "sdl3";

    // Configure command line options
    app.add_option("-f,--file", elf_file,
                   "ELF file to load (required unless --dump-dtb is used)")
        ->check(CLI::ExistingFile);
    app.add_option("-m,--memory", dram_size_mb, "DRAM size in MB")
        ->capture_default_str()
        ->check(CLI::Range(64, 16384));
    app.add_option("-d,--disk", config.virtio_blk.image, "Disk file to use");
    app.add_option("--flash0", config.flash0.image, "Flash0 file to use");
    app.add_option("--flash1", config.flash1.image, "Flash1 file to use");
    app.add_option("--dump-dtb", dump_dtb_file,
                   "Write the generated DTB to a file");
    app.add_option("-t,--timeout", timeout_ms,
                   "Execution timeout in milliseconds (0 = no timeout)")
        ->default_val(0)
        ->check(CLI::NonNegativeNumber);
    app.add_option("--frontend", frontend_name, "Frontend to use")
        ->default_val("sdl3")
        ->check(CLI::IsMember({"headless", "sdl3", "gtk4"}));

    try {
        // Parse command line
        CLI11_PARSE(app, argc, argv);

        config.dram.size = dram_size_mb * 1024 * 1024;

        uemu::log::info("Initializing emulator...");
        uemu::log::info("  DRAM size: {} MB ({} bytes)", dram_size_mb,
                        config.dram.size);
        if (!elf_file.empty())
            uemu::log::info("  ELF file: {}", elf_file.string());

        if (timeout_ms > 0)
            uemu::log::info("  Timeout: {} ms", timeout_ms);

        std::vector<uint8_t> dtb = uemu::FdtGenerator(config).generate();
        if (!dump_dtb_file.empty()) {
            std::ofstream output(dump_dtb_file, std::ios::binary);
            if (!output ||
                !output.write(reinterpret_cast<const char*>(dtb.data()),
                              static_cast<std::streamsize>(dtb.size())))
                throw std::runtime_error("Failed to write DTB: " +
                                         dump_dtb_file.string());
            output.close();
            if (!output)
                throw std::runtime_error("Failed to write DTB: " +
                                         dump_dtb_file.string());
            uemu::log::info("  DTB dump: {}", dump_dtb_file.string());
        }

        // Dumping the tree the emulator would use is useful on its own, so an
        // ELF is only required when there is something to run.
        if (elf_file.empty()) {
            if (dump_dtb_file.empty())
                throw std::runtime_error(
                    "--file is required unless --dump-dtb is used");

            uemu::log::info("No ELF file given; exiting after the DTB dump");
            return EXIT_SUCCESS;
        }

        uemu::Emulator emulator(config);

        emulator.load_elf(elf_file);
        static_cast<void>(emulator.install_fdt(dtb));

        if (frontend_name == "headless") {
            uemu::frontend::HeadlessFrontend frontend(emulator);
            frontend.run(std::chrono::milliseconds(timeout_ms));
        } else if (frontend_name == "sdl3") {
            uemu::frontend::SDL3Frontend frontend(emulator);
            frontend.run(std::chrono::milliseconds(timeout_ms));
        } else {
            uemu::frontend::Gtk4Frontend frontend(emulator);
            frontend.run(std::chrono::milliseconds(timeout_ms));
        }
    } catch (const std::runtime_error& e) {
        uemu::log::error("Runtime error: {}", e.what());
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        uemu::log::error("Exception: {}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        uemu::log::error("Unknown Error");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
