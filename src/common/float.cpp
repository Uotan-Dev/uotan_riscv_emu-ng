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

#include "common/float.hpp"

extern "C" {
#include "platform.h"

#include "internals.h"
#include "specialize.h"
}

namespace uemu {

uint_fast16_t f32_classify(float32_t a) {
    union ui32_f32 uA;
    uint_fast32_t uiA;

    uA.f = a;
    uiA = uA.ui;

    uint_fast16_t infOrNaN = expF32UI(uiA) == 0xFF;
    uint_fast16_t subnormalOrZero = expF32UI(uiA) == 0;
    bool sign = signF32UI(uiA);
    bool fracZero = fracF32UI(uiA) == 0;
    bool isNaN = isNaNF32UI(uiA);
    bool isSNaN = softfloat_isSigNaNF32UI(uiA);

    return (sign && infOrNaN && fracZero) << 0 |
           (sign && !infOrNaN && !subnormalOrZero) << 1 |
           (sign && subnormalOrZero && !fracZero) << 2 |
           (sign && subnormalOrZero && fracZero) << 3 |
           (!sign && infOrNaN && fracZero) << 7 |
           (!sign && !infOrNaN && !subnormalOrZero) << 6 |
           (!sign && subnormalOrZero && !fracZero) << 5 |
           (!sign && subnormalOrZero && fracZero) << 4 |
           (isNaN && isSNaN) << 8 | (isNaN && !isSNaN) << 9;
}

uint_fast16_t f64_classify(float64_t a) {
    union ui64_f64 uA;
    uint_fast64_t uiA;

    uA.f = a;
    uiA = uA.ui;

    uint_fast16_t infOrNaN = expF64UI(uiA) == 0x7FF;
    uint_fast16_t subnormalOrZero = expF64UI(uiA) == 0;
    bool sign = signF64UI(uiA);
    bool fracZero = fracF64UI(uiA) == 0;
    bool isNaN = isNaNF64UI(uiA);
    bool isSNaN = softfloat_isSigNaNF64UI(uiA);

    return (sign && infOrNaN && fracZero) << 0 |
           (sign && !infOrNaN && !subnormalOrZero) << 1 |
           (sign && subnormalOrZero && !fracZero) << 2 |
           (sign && subnormalOrZero && fracZero) << 3 |
           (!sign && infOrNaN && fracZero) << 7 |
           (!sign && !infOrNaN && !subnormalOrZero) << 6 |
           (!sign && subnormalOrZero && !fracZero) << 5 |
           (!sign && subnormalOrZero && fracZero) << 4 |
           (isNaN && isSNaN) << 8 | (isNaN && !isSNaN) << 9;
}

} // namespace uemu
