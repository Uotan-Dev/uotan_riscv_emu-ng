# Copyright 2025-2026 Nuo Shen, Nanjing University
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

.section .text
    .align 2
    .global _start
    .extern main

_start:
    // Disable all interrupts in M-mode
    csrw mie, zero
    csrw mip, zero
   
    // Initialize stack pointer for M-mode
    la sp, __stack_top
   
    // Delegate all exceptions and interrupts to S-mode
    li t0, 0xFFFFFFFFFFFFFFFF
    csrw medeleg, t0
    csrw mideleg, t0
   
    // Set up PMP to allow S-mode access to all memory
    li t0, 0x1F
    csrw pmpcfg0, t0
    li t0, -1
    csrw pmpaddr0, t0
   
    // Enable Svadu extension
    li t0, (1 << 61)
    csrw menvcfg, t0

    // Set mstatus for return to S-mode
    li t0, (3 << 11)
    csrc mstatus, t0 
    li t0, (1 << 11)
    csrs mstatus, t0
   
    li t0, (1 << 8)
    csrc mstatus, t0
   
    // Set mepc to s_mode_entry
    la t0, s_mode_entry
    csrw mepc, t0
   
    mret

s_mode_entry:
    // Disable interrupts in S-mode
    csrw sie, zero
    csrw sip, zero
   
    // Re-initialize stack pointer
    la sp, __stack_top
   
    // Clear BSS section
    la t0, __bss_start
    la t1, __bss_end
clear_bss:
    bge t0, t1, bss_done
    sd zero, 0(t0)
    addi t0, t0, 8
    j clear_bss

bss_done:
    // Initialize global pointer
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
   
    // Clear thread pointer
    mv tp, zero
   
    // Set up sstatus
    // Enable SUM bit to allow supervisor to access user pages
    li t0, (1 << 18) // SSTATUS_SUM
    csrs sstatus, t0
   
    // Clear SPP bit (we came from U-mode, not S-mode)
    li t0, (1 << 8) // SSTATUS_SPP
    csrc sstatus, t0
   
    // Ensure interrupts are disabled
    li t0, (1 << 1) // SSTATUS_SIE
    csrc sstatus, t0
   
    // Call main function
    call main
   
    // If main returns, halt
halt:
    wfi
    j halt
