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
#include <iostream>
#include <string>

#include <CLI/CLI.hpp>

#include "emulator.hpp"

int main(int argc, char* argv[]) {
    CLI::App app{"uemu-ng: RISC-V Emulator"};
    app.set_version_flag("-v,--version", "1.0.0");

    std::string elf_file;
    size_t dram_size_mb = 512;

    // Configure command line options
    app.add_option("-f,--file", elf_file, "ELF file to load")
        ->required()
        ->check(CLI::ExistingFile);
    app.add_option("-m,--memory", dram_size_mb, "DRAM size in MB")
        ->default_val(512)
        ->check(CLI::Range(64, 16384));

    // Parse command line
    CLI11_PARSE(app, argc, argv);

    try {
        size_t dram_size = dram_size_mb * 1024 * 1024;

        std::cout << "Initializing emulator...\n"
                  << "  DRAM size: " << dram_size_mb << " MB (" << dram_size
                  << " bytes)\n"
                  << "  ELF file: " << elf_file << std::endl;

        uemu::Emulator emulator(dram_size);

        emulator.loadelf(elf_file);
        emulator.run();
    } catch (const std::runtime_error& e) {
        std::cerr << "Runtime error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
