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

#include "core/decoder.hpp"

namespace uemu::core {

#define RV_EXEC_IMPL(inst_name)                                                \
    void exec_##inst_name(Hart* hart, MMU* mmu, const DecodedInsn* d);

RISCV_INSTRUCTIONS(RV_EXEC_IMPL)

#undef RV_EXEC_IMPL

} // namespace uemu::core
