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

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string_view>
#include <vector>

namespace uemu::utils {

// Device tree cells are big-endian, and a 64-bit address or size does not fit
// one cell: with #address-cells = #size-cells = 2 it is written as two.
constexpr uint32_t upper_cell(uint64_t value) {
    return static_cast<uint32_t>(value >> 32);
}

constexpr uint32_t lower_cell(uint64_t value) {
    return static_cast<uint32_t>(value);
}

// Builds a flattened device tree by calling libfdt, without exposing it: this
// header stays free of <libfdt.h>, and every libfdt failure becomes an
// exception whose message names the operation and libfdt's reason.
//
// Nodes are addressed by their path, e.g. "/soc/uart@10000000", never by a
// libfdt node offset.  An offset is a byte position inside the structure
// block, so inserting anything earlier in the tree - which every add_node()
// and set_*() does - moves it; a path is looked up again on each call, and so
// also survives the buffer growth that a set_*() may trigger.
//
// It deliberately knows nothing about uemu: no board, no devices, no topology.
// See fdt_generator.hpp for the BoardConfig -> device tree mapping.
class FdtBuilder {
public:
    // An empty tree holding only the root node, ready for add_node().
    FdtBuilder();

    // Creates the node at `path`; the parent node must already exist.
    void add_node(std::string_view path);

    // A property with no value at all, such as "interrupt-controller".
    void set_empty(std::string_view path, std::string_view name);
    void set_u32(std::string_view path, std::string_view name, uint32_t value);
    // A <...> list, such as "reg" or "interrupts-extended".
    void set_cells(std::string_view path, std::string_view name,
                   std::initializer_list<uint32_t> values);
    void set_string(std::string_view path, std::string_view name,
                    std::string_view value);
    // A NUL separated list of strings, such as "compatible".
    void set_string_list(std::string_view path, std::string_view name,
                         std::initializer_list<std::string_view> values);
    // The "phandle" other nodes refer to as <&label>.
    void set_phandle(std::string_view path, uint32_t phandle);

    // Packs the tree (dropping the room left by building it) and returns the
    // blob to hand to the guest.  The builder is spent afterwards.
    [[nodiscard]] std::vector<uint8_t> finish();

private:
    // An empty tree needs a few hundred bytes; the buffer is doubled whenever
    // libfdt reports that it ran out of room, up to a size no machine
    // description should ever need.
    static constexpr size_t INITIAL_CAPACITY = 1024;
    static constexpr size_t MAX_CAPACITY = 1024 * 1024;

    // The offset of the node at `path`, or an exception when it is missing.
    [[nodiscard]] int node(std::string_view path) const;

    // Throws when libfdt reported a failure, otherwise returns `result`.
    static int check(int result, std::string_view operation);

    // True when `result` is libfdt's "the buffer is too small".
    [[nodiscard]] static bool out_of_space(int result);

    // Resolves the path and applies `op`; on -FDT_ERR_NOSPACE it grows the
    // buffer and retries, which is safe because a failed libfdt call leaves the
    // tree untouched.
    template <typename Op>
    void with_space(std::string_view path, std::string_view operation,
                    Op&& op) {
        for (;;) {
            const int result = op(blob_.data(), node(path));
            if (!out_of_space(result)) {
                check(result, operation);
                return;
            }

            grow();
        }
    }

    void grow();

    void set_property(std::string_view path, std::string_view name,
                      std::span<const uint8_t> value);

    std::vector<uint8_t> blob_;
    bool finished_ = false;
};

} // namespace uemu::utils
