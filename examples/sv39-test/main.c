/*
 * Copyright 2025-2026 Nuo Shen, Nanjing University
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

#include <stdint.h>

// Device memory regions
#define CLINT_BASE 0x02000000
#define CLINT_SIZE 0x10000
#define TEST_BASE 0x00100000
#define TEST_SIZE 0x1000
#define CONSOLE_BASE 0x10008000
#define CONSOLE_SIZE 8

// RISC-V CSR definitions
#define CSR_SATP 0x180
#define CSR_SSTATUS 0x100

// SV39 constants
#define SATP_MODE_SV39 (8ULL << 60)
#define PAGE_SIZE 4096
#define PTE_V (1 << 0)
#define PTE_R (1 << 1)
#define PTE_W (1 << 2)
#define PTE_X (1 << 3)
#define PTE_U (1 << 4)
#define PTE_G (1 << 5)
#define PTE_A (1 << 6)
#define PTE_D (1 << 7)

// sstatus bits
#define SSTATUS_SIE (1ULL << 1)
#define SSTATUS_SPIE (1ULL << 5)
#define SSTATUS_SPP (1ULL << 8)
#define SSTATUS_SUM (1ULL << 18)

typedef uint64_t pte_t;

#define read_csr(reg)                                                          \
    ({                                                                         \
        uint64_t __tmp;                                                        \
        asm volatile("csrr %0, " #reg : "=r"(__tmp));                          \
        __tmp;                                                                 \
    })

#define write_csr(reg, val) ({ asm volatile("csrw " #reg ", %0" ::"r"(val)); })

#define sfence_vma() asm volatile("sfence.vma" ::: "memory")

// Page tables (aligned to 4KB)
__attribute__((aligned(4096))) static pte_t root_page_table[512];
__attribute__((aligned(4096))) static pte_t mid_page_table_ram[512];
__attribute__((aligned(4096))) static pte_t leaf_page_table_ram[512];
__attribute__((aligned(4096))) static pte_t mid_page_table_nonid[512];
__attribute__((aligned(4096))) static pte_t leaf_page_table_nonid[512];

// Test data areas
static volatile uint64_t test_data = 0xDEADBEEF;

void putchar(char c) { *(volatile char*)CONSOLE_BASE = c; }

void puts(const char* s) {
    while (*s)
        putchar(*s++);
}

void print_hex(uint64_t val) {
    const char hex[] = "0123456789ABCDEF";
    puts("0x");
    for (int i = 60; i >= 0; i -= 4)
        putchar(hex[(val >> i) & 0xF]);
}

static inline pte_t make_pte(uint64_t pa, uint64_t flags) {
    return ((pa >> 12) << 10) | flags;
}

void setup_sv39() {
    // Clear page tables
    for (int i = 0; i < 512; i++) {
        root_page_table[i] = 0;
        mid_page_table_ram[i] = 0;
        leaf_page_table_ram[i] = 0;
        mid_page_table_nonid[i] = 0;
        leaf_page_table_nonid[i] = 0;
    }

    puts("Setting up page tables...\r\n");

    // Identity map RAM: 0x80000000 - 0x80200000 (2MB with 4KB pages)
    uint64_t mid_ram_pa = (uint64_t)mid_page_table_ram;
    root_page_table[2] = make_pte(mid_ram_pa, PTE_V);
    uint64_t leaf_ram_pa = (uint64_t)leaf_page_table_ram;
    mid_page_table_ram[0] = make_pte(leaf_ram_pa, PTE_V);
    for (int i = 0; i < 512; i++) {
        uint64_t pa = 0x80000000 + (i * PAGE_SIZE);
        leaf_page_table_ram[i] = make_pte(pa, PTE_V | PTE_R | PTE_W | PTE_X);
    }
    puts(" RAM identity mapped (0x80000000-0x80200000)\r\n");

    // Identity map devices using 1GB huge page
    root_page_table[0] = make_pte(0x00000000, PTE_V | PTE_R | PTE_W);
    puts(" All devices identity mapped (0x00000000-0x3FFFFFFF)\r\n");

    // Non-identity mapping: Map physical 0x80100000 to virtual 0xC0000000
    // (4KB page)
    uint64_t mid_nonid_pa = (uint64_t)mid_page_table_nonid;
    root_page_table[3] = make_pte(mid_nonid_pa, PTE_V);
    uint64_t leaf_nonid_pa = (uint64_t)leaf_page_table_nonid;
    mid_page_table_nonid[0] = make_pte(leaf_nonid_pa, PTE_V);
    leaf_page_table_nonid[0] =
        make_pte(0x80100000, PTE_V | PTE_R | PTE_W | PTE_X);
    puts(" Non-identity mapping: VA 0xC0000000 -> PA 0x80100000\r\n");

    puts("Page tables setup complete\r\n\r\n");
}

void enable_sv39() {
    uint64_t root_ppn = ((uint64_t)root_page_table) >> 12;
    uint64_t satp_val = SATP_MODE_SV39 | root_ppn;

    puts("Enabling SV39...\r\n");
    puts("SATP value: ");
    print_hex(satp_val);
    puts("\r\n");

    write_csr(satp, satp_val);
    sfence_vma();

    puts("SV39 enabled!\r\n\r\n");
}

static inline void shutdown(int code) {
    volatile uint32_t* const power_off_reg = (volatile uint32_t*)TEST_BASE;
    *power_off_reg = ((uint32_t)code << 16) | 0x5555;
}

void test_virtual_memory() {
    puts("=== Virtual Memory Tests ===\r\n\r\n");

    // Test 1: Identity mapped R/W
    puts("Test 1: Identity-mapped R/W\r\n");
    test_data = 0x12345678ABCDEF00ULL;
    puts(" Written: ");
    print_hex(test_data);
    puts("\r\n");

    uint64_t read_val = test_data;
    puts(" Read: ");
    print_hex(read_val);
    puts("\r\n");

    if (read_val == 0x12345678ABCDEF00ULL) {
        puts(" PASS\r\n\r\n");
    } else {
        puts(" FAIL\r\n\r\n");
        shutdown(-1);
    }

    // Test 2: Non-identity mapping
    puts("Test 2: Non-identity mapping\r\n");
    volatile uint64_t* virtual_addr = (volatile uint64_t*)0xC0000000;
    volatile uint64_t* physical_addr = (volatile uint64_t*)0x80100000;

    // Write through physical address
    *physical_addr = 0xCAFEBABEDEADC0DEULL;
    puts(" Written 0xCAFEBABEDEADC0DE to PA 0x80100000\r\n");

    // Read through virtual address
    uint64_t virt_read = *virtual_addr;
    puts(" Read from VA 0xC0000000: ");
    print_hex(virt_read);
    puts("\r\n");

    if (virt_read == 0xCAFEBABEDEADC0DEULL) {
        puts(" PASS: Non-identity mapping works!\r\n\r\n");
    } else {
        puts(" FAIL: Non-identity mapping broken\r\n\r\n");
        shutdown(-1);
    }

    // Test 3: Write through virtual, read through physical
    puts("Test 3: Reverse non-identity test\r\n");
    *virtual_addr = 0x0123456789ABCDEFULL;
    puts(" Written 0x0123456789ABCDEF to VA 0xC0000000\r\n");

    uint64_t phys_read = *physical_addr;
    puts(" Read from PA 0x80100000: ");
    print_hex(phys_read);
    puts("\r\n");

    if (phys_read == 0x0123456789ABCDEFULL) {
        puts(" PASS\r\n\r\n");
    } else {
        puts(" FAIL\r\n\r\n");
        shutdown(-1);
    }

    // Test 4: Verify SATP
    puts("Test 4: Verify SATP register\r\n");
    uint64_t satp_val = read_csr(satp);
    puts(" SATP: ");
    print_hex(satp_val);
    puts("\r\n");

    uint64_t mode = satp_val >> 60;
    if (mode == 8) {
        puts(" PASS: SV39 mode active\r\n\r\n");
    } else {
        puts(" FAIL: SV39 mode not active\r\n\r\n");
        shutdown(-1);
    }

    puts("=== All Tests Complete ===\r\n");
}

int main() {
    puts("\r\n========================================\r\n");
    puts(" RISC-V SV39 Virtual Memory Test\r\n");
    puts(" (Fixed with device mappings)\r\n");
    puts("========================================\r\n\r\n");

    // Check initial mode
    uint64_t satp = read_csr(satp);
    puts("Initial SATP: ");
    print_hex(satp);
    puts("\r\n");

    uint64_t sstatus = read_csr(sstatus);
    puts("Initial sstatus: ");
    print_hex(sstatus);
    puts("\r\n\r\n");

    setup_sv39();
    enable_sv39();
    test_virtual_memory();

    puts("\r\nTest completed successfully!\r\n");

    shutdown(0);
    while (1)
        ;

    return 0;
}
