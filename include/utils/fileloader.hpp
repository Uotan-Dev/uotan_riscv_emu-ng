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

#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace uemu::utils {

class FileLoader {
public:
    FileLoader() = delete;
    ~FileLoader() = delete;
    FileLoader(const FileLoader&) = delete;
    FileLoader& operator=(const FileLoader&) = delete;

    static std::vector<uint8_t> read_file(const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file)
            throw std::runtime_error("Failed to open file: " + path.string());

        const auto size = file.tellg();
        file.seekg(0);

        std::vector<uint8_t> data(size);
        if (!file.read(reinterpret_cast<char*>(data.data()), size))
            throw std::runtime_error("Failed to read file: " + path.string());

        return data;
    }
};

} // namespace uemu::utils
