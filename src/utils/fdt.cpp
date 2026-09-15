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
#include <cstdint>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <libfdt.h>

#include "utils/fdt.hpp"

namespace uemu::utils {

namespace {

// libfdt wants NUL terminated names and paths; string_view does not carry the
// terminator.
std::string terminated(std::string_view text) { return std::string(text); }

std::span<const uint8_t> as_bytes(const fdt32_t& cell) {
    return {reinterpret_cast<const uint8_t*>(&cell), sizeof(cell)};
}

} // namespace

FdtBuilder::FdtBuilder() : blob_(INITIAL_CAPACITY) {
    check(fdt_create_empty_tree(blob_.data(), static_cast<int>(blob_.size())),
          "Failed to create FDT");
}

int FdtBuilder::node(std::string_view path) const {
    const std::string name = terminated(path);
    return check(fdt_path_offset(blob_.data(), name.c_str()),
                 "Failed to find FDT node " + name);
}

int FdtBuilder::check(int result, std::string_view operation) {
    if (result < 0)
        throw std::runtime_error(std::string(operation) + ": " +
                                 fdt_strerror(result));

    return result;
}

bool FdtBuilder::out_of_space(int result) { return result == -FDT_ERR_NOSPACE; }

void FdtBuilder::grow() {
    const size_t capacity = blob_.size();
    if (capacity >= MAX_CAPACITY)
        throw std::runtime_error("Device tree is too large: it exceeds " +
                                 std::to_string(MAX_CAPACITY) + " bytes");

    const size_t grown = std::min(capacity * 2, MAX_CAPACITY);
    std::vector<uint8_t> bigger(grown);
    // libfdt cannot grow a blob in place, but it can move the tree into a
    // larger one.  Every node offset would move with it, which is why this
    // class only ever holds paths.
    check(fdt_open_into(blob_.data(), bigger.data(),
                        static_cast<int>(bigger.size())),
          "Failed to grow the device tree");
    blob_.swap(bigger);
}

void FdtBuilder::add_node(std::string_view path) {
    const std::string name = terminated(path);
    const size_t slash = name.rfind('/');
    if (slash == std::string::npos || slash + 1 == name.size())
        throw std::invalid_argument(
            "FDT node path must be absolute and named: " + name);

    const std::string parent =
        slash == 0 ? std::string("/") : name.substr(0, slash);
    const std::string child = name.substr(slash + 1);
    with_space(parent, "Failed to add FDT node " + name,
               [&child](void* blob, int parent_node) {
                   return fdt_add_subnode(blob, parent_node, child.c_str());
               });
}

void FdtBuilder::set_property(std::string_view path, std::string_view name,
                              std::span<const uint8_t> value) {
    const std::string property = terminated(name);
    const void* data = value.empty() ? nullptr : value.data();
    with_space(path, "Failed to set FDT property " + property,
               [&property, data, size = value.size()](void* blob, int node) {
                   return fdt_setprop(blob, node, property.c_str(), data,
                                      static_cast<int>(size));
               });
}

void FdtBuilder::set_empty(std::string_view path, std::string_view name) {
    set_property(path, name, {});
}

void FdtBuilder::set_u32(std::string_view path, std::string_view name,
                         uint32_t value) {
    const fdt32_t cell = cpu_to_fdt32(value);
    set_property(path, name, as_bytes(cell));
}

void FdtBuilder::set_cells(std::string_view path, std::string_view name,
                           std::initializer_list<uint32_t> values) {
    std::vector<uint8_t> cells;
    cells.reserve(values.size() * sizeof(fdt32_t));
    for (uint32_t value : values) {
        const fdt32_t cell = cpu_to_fdt32(value);
        const std::span<const uint8_t> bytes = as_bytes(cell);
        cells.insert(cells.end(), bytes.begin(), bytes.end());
    }

    set_property(path, name, cells);
}

void FdtBuilder::set_string(std::string_view path, std::string_view name,
                            std::string_view value) {
    std::vector<uint8_t> bytes(value.begin(), value.end());
    bytes.push_back('\0');
    set_property(path, name, bytes);
}

void FdtBuilder::set_string_list(
    std::string_view path, std::string_view name,
    std::initializer_list<std::string_view> values) {
    std::vector<uint8_t> bytes;
    for (std::string_view value : values) {
        bytes.insert(bytes.end(), value.begin(), value.end());
        bytes.push_back('\0');
    }
    set_property(path, name, bytes);
}

void FdtBuilder::set_phandle(std::string_view path, uint32_t phandle) {
    set_u32(path, "phandle", phandle);
}

std::vector<uint8_t> FdtBuilder::finish() {
    if (finished_)
        throw std::logic_error("FDT builder has already been finished");

    check(fdt_pack(blob_.data()), "Failed to pack FDT");
    const int size = fdt_totalsize(blob_.data());
    if (size <= 0)
        throw std::runtime_error("Packed FDT has an invalid size");

    blob_.resize(static_cast<size_t>(size));
    finished_ = true;
    return std::move(blob_);
}

} // namespace uemu::utils
