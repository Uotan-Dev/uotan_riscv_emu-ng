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

#include <cstdlib>
#include <filesystem>
#include <print>

#include <CLI/CLI.hpp>

#include "emulator.hpp"

int main(int argc, char* argv[]) {
    CLI::App app{"uemu-ng: RISC-V Emulator"};
    app.set_version_flag("-v,--version", "1.0.0");

    std::filesystem::path elf_file;
    std::filesystem::path signature_file;
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
    app.add_option("-s,--signature", signature_file,
                   "Dump signature to file (for riscv-arch-test)");
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

        uemu::Emulator emulator(dram_size, headless);

        emulator.loadelf(elf_file);
        emulator.run(std::chrono::milliseconds(timeout_ms));

        // Dump signature if requested
        if (!signature_file.empty())
            emulator.dump_signature(elf_file, signature_file);
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
