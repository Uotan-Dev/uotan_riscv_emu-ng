# Copyright 2026 Nuo Shen, Nanjing University
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(SOFTFLOAT_ROOT ${CMAKE_CURRENT_LIST_DIR}/berkeley-softfloat-3)
set(SOFTFLOAT_SOURCE_DIR ${SOFTFLOAT_ROOT}/source)
set(SPECIALIZE_TYPE RISCV)

# SoftFloat compilation options
add_definitions(
    -DSOFTFLOAT_ROUND_ODD
    -DINLINE_LEVEL=5
    -DSOFTFLOAT_FAST_DIV32TO16
    -DSOFTFLOAT_FAST_DIV64TO32
    -DSOFTFLOAT_FAST_INT64
)

# Include directories
set(SOFTFLOAT_INCLUDE_DIRS
    ${SOFTFLOAT_ROOT}/build/Linux-x86_64-GCC
    ${SOFTFLOAT_SOURCE_DIR}/include
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}
)

# Primitive operations source files
set(SOFTFLOAT_PRIMITIVES_SRCS
    ${SOFTFLOAT_SOURCE_DIR}/s_eq128.c
    ${SOFTFLOAT_SOURCE_DIR}/s_le128.c
    ${SOFTFLOAT_SOURCE_DIR}/s_lt128.c
    ${SOFTFLOAT_SOURCE_DIR}/s_shortShiftLeft128.c
    ${SOFTFLOAT_SOURCE_DIR}/s_shortShiftRight128.c
    ${SOFTFLOAT_SOURCE_DIR}/s_shortShiftRightJam64.c
    ${SOFTFLOAT_SOURCE_DIR}/s_shortShiftRightJam64Extra.c
    ${SOFTFLOAT_SOURCE_DIR}/s_shortShiftRightJam128.c
    ${SOFTFLOAT_SOURCE_DIR}/s_shortShiftRightJam128Extra.c
    ${SOFTFLOAT_SOURCE_DIR}/s_shiftRightJam32.c
    ${SOFTFLOAT_SOURCE_DIR}/s_shiftRightJam64.c
    ${SOFTFLOAT_SOURCE_DIR}/s_shiftRightJam64Extra.c
    ${SOFTFLOAT_SOURCE_DIR}/s_shiftRightJam128.c
    ${SOFTFLOAT_SOURCE_DIR}/s_shiftRightJam128Extra.c
    ${SOFTFLOAT_SOURCE_DIR}/s_shiftRightJam256M.c
    ${SOFTFLOAT_SOURCE_DIR}/s_countLeadingZeros8.c
    ${SOFTFLOAT_SOURCE_DIR}/s_countLeadingZeros16.c
    ${SOFTFLOAT_SOURCE_DIR}/s_countLeadingZeros32.c
    ${SOFTFLOAT_SOURCE_DIR}/s_countLeadingZeros64.c
    ${SOFTFLOAT_SOURCE_DIR}/s_add128.c
    ${SOFTFLOAT_SOURCE_DIR}/s_add256M.c
    ${SOFTFLOAT_SOURCE_DIR}/s_sub128.c
    ${SOFTFLOAT_SOURCE_DIR}/s_sub256M.c
    ${SOFTFLOAT_SOURCE_DIR}/s_mul64ByShifted32To128.c
    ${SOFTFLOAT_SOURCE_DIR}/s_mul64To128.c
    ${SOFTFLOAT_SOURCE_DIR}/s_mul128By32.c
    ${SOFTFLOAT_SOURCE_DIR}/s_mul128To256M.c
    ${SOFTFLOAT_SOURCE_DIR}/s_approxRecip_1Ks.c
    ${SOFTFLOAT_SOURCE_DIR}/s_approxRecip32_1.c
    ${SOFTFLOAT_SOURCE_DIR}/s_approxRecipSqrt_1Ks.c
    ${SOFTFLOAT_SOURCE_DIR}/s_approxRecipSqrt32_1.c
)

# Specialization source files (RISCV-specific)
set(SOFTFLOAT_SPECIALIZE_SRCS
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/softfloat_raiseFlags.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_f16UIToCommonNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_commonNaNToF16UI.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_propagateNaNF16UI.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_bf16UIToCommonNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_commonNaNToBF16UI.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_f32UIToCommonNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_commonNaNToF32UI.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_propagateNaNF32UI.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_f64UIToCommonNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_commonNaNToF64UI.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_propagateNaNF64UI.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/extF80M_isSignalingNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_extF80UIToCommonNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_commonNaNToExtF80UI.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_propagateNaNExtF80UI.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/f128M_isSignalingNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_f128UIToCommonNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_commonNaNToF128UI.c
    ${SOFTFLOAT_SOURCE_DIR}/${SPECIALIZE_TYPE}/s_propagateNaNF128UI.c
)

# Other operations source files
set(SOFTFLOAT_OTHERS_SRCS
    ${SOFTFLOAT_SOURCE_DIR}/s_roundToUI32.c
    ${SOFTFLOAT_SOURCE_DIR}/s_roundToUI64.c
    ${SOFTFLOAT_SOURCE_DIR}/s_roundToI32.c
    ${SOFTFLOAT_SOURCE_DIR}/s_roundToI64.c
    ${SOFTFLOAT_SOURCE_DIR}/s_normSubnormalBF16Sig.c
    ${SOFTFLOAT_SOURCE_DIR}/s_roundPackToBF16.c
    ${SOFTFLOAT_SOURCE_DIR}/s_normSubnormalF16Sig.c
    ${SOFTFLOAT_SOURCE_DIR}/s_roundPackToF16.c
    ${SOFTFLOAT_SOURCE_DIR}/s_normRoundPackToF16.c
    ${SOFTFLOAT_SOURCE_DIR}/s_addMagsF16.c
    ${SOFTFLOAT_SOURCE_DIR}/s_subMagsF16.c
    ${SOFTFLOAT_SOURCE_DIR}/s_mulAddF16.c
    ${SOFTFLOAT_SOURCE_DIR}/s_normSubnormalF32Sig.c
    ${SOFTFLOAT_SOURCE_DIR}/s_roundPackToF32.c
    ${SOFTFLOAT_SOURCE_DIR}/s_normRoundPackToF32.c
    ${SOFTFLOAT_SOURCE_DIR}/s_addMagsF32.c
    ${SOFTFLOAT_SOURCE_DIR}/s_subMagsF32.c
    ${SOFTFLOAT_SOURCE_DIR}/s_mulAddF32.c
    ${SOFTFLOAT_SOURCE_DIR}/s_normSubnormalF64Sig.c
    ${SOFTFLOAT_SOURCE_DIR}/s_roundPackToF64.c
    ${SOFTFLOAT_SOURCE_DIR}/s_normRoundPackToF64.c
    ${SOFTFLOAT_SOURCE_DIR}/s_addMagsF64.c
    ${SOFTFLOAT_SOURCE_DIR}/s_subMagsF64.c
    ${SOFTFLOAT_SOURCE_DIR}/s_mulAddF64.c
    ${SOFTFLOAT_SOURCE_DIR}/s_normSubnormalExtF80Sig.c
    ${SOFTFLOAT_SOURCE_DIR}/s_roundPackToExtF80.c
    ${SOFTFLOAT_SOURCE_DIR}/s_normRoundPackToExtF80.c
    ${SOFTFLOAT_SOURCE_DIR}/s_addMagsExtF80.c
    ${SOFTFLOAT_SOURCE_DIR}/s_subMagsExtF80.c
    ${SOFTFLOAT_SOURCE_DIR}/s_normSubnormalF128Sig.c
    ${SOFTFLOAT_SOURCE_DIR}/s_roundPackToF128.c
    ${SOFTFLOAT_SOURCE_DIR}/s_normRoundPackToF128.c
    ${SOFTFLOAT_SOURCE_DIR}/s_addMagsF128.c
    ${SOFTFLOAT_SOURCE_DIR}/s_subMagsF128.c
    ${SOFTFLOAT_SOURCE_DIR}/s_mulAddF128.c
    ${SOFTFLOAT_SOURCE_DIR}/softfloat_state.c
    ${SOFTFLOAT_SOURCE_DIR}/ui32_to_f16.c
    ${SOFTFLOAT_SOURCE_DIR}/ui32_to_f32.c
    ${SOFTFLOAT_SOURCE_DIR}/ui32_to_f64.c
    ${SOFTFLOAT_SOURCE_DIR}/ui32_to_extF80.c
    ${SOFTFLOAT_SOURCE_DIR}/ui32_to_extF80M.c
    ${SOFTFLOAT_SOURCE_DIR}/ui32_to_f128.c
    ${SOFTFLOAT_SOURCE_DIR}/ui32_to_f128M.c
    ${SOFTFLOAT_SOURCE_DIR}/ui64_to_f16.c
    ${SOFTFLOAT_SOURCE_DIR}/ui64_to_f32.c
    ${SOFTFLOAT_SOURCE_DIR}/ui64_to_f64.c
    ${SOFTFLOAT_SOURCE_DIR}/ui64_to_extF80.c
    ${SOFTFLOAT_SOURCE_DIR}/ui64_to_extF80M.c
    ${SOFTFLOAT_SOURCE_DIR}/ui64_to_f128.c
    ${SOFTFLOAT_SOURCE_DIR}/ui64_to_f128M.c
    ${SOFTFLOAT_SOURCE_DIR}/i32_to_f16.c
    ${SOFTFLOAT_SOURCE_DIR}/i32_to_f32.c
    ${SOFTFLOAT_SOURCE_DIR}/i32_to_f64.c
    ${SOFTFLOAT_SOURCE_DIR}/i32_to_extF80.c
    ${SOFTFLOAT_SOURCE_DIR}/i32_to_extF80M.c
    ${SOFTFLOAT_SOURCE_DIR}/i32_to_f128.c
    ${SOFTFLOAT_SOURCE_DIR}/i32_to_f128M.c
    ${SOFTFLOAT_SOURCE_DIR}/i64_to_f16.c
    ${SOFTFLOAT_SOURCE_DIR}/i64_to_f32.c
    ${SOFTFLOAT_SOURCE_DIR}/i64_to_f64.c
    ${SOFTFLOAT_SOURCE_DIR}/i64_to_extF80.c
    ${SOFTFLOAT_SOURCE_DIR}/i64_to_extF80M.c
    ${SOFTFLOAT_SOURCE_DIR}/i64_to_f128.c
    ${SOFTFLOAT_SOURCE_DIR}/i64_to_f128M.c
    ${SOFTFLOAT_SOURCE_DIR}/bf16_isSignalingNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/bf16_to_f32.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_to_ui32.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_to_ui64.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_to_i32.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_to_i64.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_to_ui32_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_to_ui64_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_to_i32_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_to_i64_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_to_f32.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_to_f64.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_to_extF80.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_to_extF80M.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_to_f128.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_to_f128M.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_roundToInt.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_add.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_sub.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_mul.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_mulAdd.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_div.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_rem.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_sqrt.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_eq.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_le.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_lt.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_eq_signaling.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_le_quiet.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_lt_quiet.c
    ${SOFTFLOAT_SOURCE_DIR}/f16_isSignalingNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_to_ui32.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_to_ui64.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_to_i32.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_to_i64.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_to_ui32_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_to_ui64_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_to_i32_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_to_i64_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_to_bf16.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_to_f16.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_to_f64.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_to_extF80.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_to_extF80M.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_to_f128.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_to_f128M.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_roundToInt.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_add.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_sub.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_mul.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_mulAdd.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_div.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_rem.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_sqrt.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_eq.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_le.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_lt.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_eq_signaling.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_le_quiet.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_lt_quiet.c
    ${SOFTFLOAT_SOURCE_DIR}/f32_isSignalingNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_to_ui32.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_to_ui64.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_to_i32.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_to_i64.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_to_ui32_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_to_ui64_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_to_i32_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_to_i64_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_to_f16.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_to_f32.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_to_extF80.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_to_extF80M.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_to_f128.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_to_f128M.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_roundToInt.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_add.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_sub.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_mul.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_mulAdd.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_div.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_rem.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_sqrt.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_eq.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_le.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_lt.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_eq_signaling.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_le_quiet.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_lt_quiet.c
    ${SOFTFLOAT_SOURCE_DIR}/f64_isSignalingNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_to_ui32.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_to_ui64.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_to_i32.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_to_i64.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_to_ui32_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_to_ui64_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_to_i32_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_to_i64_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_to_f16.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_to_f32.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_to_f64.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_to_f128.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_roundToInt.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_add.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_sub.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_mul.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_div.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_rem.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_sqrt.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_eq.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_le.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_lt.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_eq_signaling.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_le_quiet.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_lt_quiet.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80_isSignalingNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_to_ui32.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_to_ui64.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_to_i32.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_to_i64.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_to_ui32_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_to_ui64_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_to_i32_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_to_i64_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_to_f16.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_to_f32.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_to_f64.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_to_f128M.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_roundToInt.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_add.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_sub.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_mul.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_div.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_rem.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_sqrt.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_eq.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_le.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_lt.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_eq_signaling.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_le_quiet.c
    ${SOFTFLOAT_SOURCE_DIR}/extF80M_lt_quiet.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_to_ui32.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_to_ui64.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_to_i32.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_to_i64.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_to_ui32_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_to_ui64_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_to_i32_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_to_i64_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_to_f16.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_to_f32.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_to_extF80.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_to_f64.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_roundToInt.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_add.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_sub.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_mul.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_mulAdd.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_div.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_rem.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_sqrt.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_eq.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_le.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_lt.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_eq_signaling.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_le_quiet.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_lt_quiet.c
    ${SOFTFLOAT_SOURCE_DIR}/f128_isSignalingNaN.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_to_ui32.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_to_ui64.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_to_i32.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_to_i64.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_to_ui32_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_to_ui64_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_to_i32_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_to_i64_r_minMag.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_to_f16.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_to_f32.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_to_extF80M.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_to_f64.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_roundToInt.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_add.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_sub.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_mul.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_mulAdd.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_div.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_rem.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_sqrt.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_eq.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_le.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_lt.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_eq_signaling.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_le_quiet.c
    ${SOFTFLOAT_SOURCE_DIR}/f128M_lt_quiet.c
)

# Combine all source files
set(SOFTFLOAT_ALL_SRCS
    ${SOFTFLOAT_PRIMITIVES_SRCS}
    ${SOFTFLOAT_SPECIALIZE_SRCS}
    ${SOFTFLOAT_OTHERS_SRCS}
)

# Create the softfloat library
add_library(softfloat STATIC ${SOFTFLOAT_ALL_SRCS})

# Set include directories for the library
target_include_directories(softfloat
    PUBLIC
        ${SOFTFLOAT_INCLUDE_DIRS}
)

# Set compiler options for softfloat
target_compile_options(softfloat PRIVATE
    -O3
    -Werror=implicit-function-declaration
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-uninitialized-const-pointer
    -Wno-empty-translation-unit
    -Wno-sign-compare
)

# Set C standard for softfloat
set_target_properties(softfloat PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
)
