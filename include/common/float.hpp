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

extern "C" {
#include "softfloat.h"  // IWYU pragma: keep
}

#define F32_DEFAULT_NAN 0x7FC00000
#define F64_DEFAULT_NAN 0x7FF8000000000000

#define F32_SIGN (1U << 31)
#define F64_SIGN (1ULL << 63)

namespace uemu {

constexpr bool is_boxed_f32(float64_t x) { return (x.v >> 32) == 0xFFFFFFFF; }

constexpr float32_t unbox_f32(float64_t x) {
    return float32_t{static_cast<uint32_t>(x.v)};
}

constexpr float64_t box_f32(float32_t x) {
    return float64_t{static_cast<uint64_t>(x.v) | UINT64_C(0xFFFFFFFF00000000)};
}

constexpr bool f32_isNegative(float32_t x) { return x.v & F32_SIGN; }

constexpr bool f64_isNegative(float64_t x) { return x.v & F64_SIGN; }

constexpr float32_t f32_neg(float32_t x) { return float32_t{x.v ^ F32_SIGN}; }

constexpr float64_t f64_neg(float64_t x) { return float64_t{x.v ^ F64_SIGN}; }

constexpr bool f32_isNaN(float32_t x) {
    return ((~x.v & 0x7F800000) == 0) && (x.v & 0x007FFFFF);
}

constexpr bool f64_isNaN(float64_t x) {
    return ((~x.v & 0x7FF0000000000000) == 0) && (x.v & 0x000FFFFFFFFFFFFF);
}

uint_fast16_t f32_classify(float32_t a);

uint_fast16_t f64_classify(float64_t a);

} // namespace uemu
