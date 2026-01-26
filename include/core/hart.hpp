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

#pragma once

#include <array>
#include <atomic>
#include <cassert>
#include <exception>
#include <limits>
#include <memory>

#include "common/float.hpp"
#include "core/dram.hpp"

namespace uemu::core {

enum class PrivilegeLevel : uint8_t {
    U = 0, // User mode
    S = 1, // Supervisor mode
    M = 3  // Machine mode
};

enum class TrapCause : reg_t {
    InstructionAddressMisaligned = 0, // Instruction address misaligned
    InstructionAccessFault = 1,       // Instruction access fault
    IllegalInstruction = 2,           // Illegal instruction
    Breakpoint = 3,                   // Breakpoint (EBREAK)
    LoadAddressMisaligned = 4,        // Load address misaligned
    LoadAccessFault = 5,              // Load access fault
    StoreAMOAddressMisaligned = 6,    // Store/AMO address misaligned
    StoreAMOAccessFault = 7,          // Store/AMO access fault

    EnvironmentCallFromU = 8, // ECALL from U-mode
    EnvironmentCallFromS = 9, // ECALL from S-mode
    // 10 reserved for future
    EnvironmentCallFromM = 11, // ECALL from M-mode

    InstructionPageFault = 12, // Instruction page fault
    LoadPageFault = 13,        // Load page fault
    // 14 reserved
    StoreAMOPageFault = 15, // Store/AMO page fault

    SupervisorSoftwareInterrupt =
        (1ULL << 63) | 1, // Supervisor software interrupt
    MachineSoftwareInterrupt = (1ULL << 63) | 3, // Machine software interrupt
    SupervisorTimerInterrupt = (1ULL << 63) | 5, // Supervisor timer interrupt
    MachineTimerInterrupt = (1ULL << 63) | 7,    // Machine timer interrupt
    SupervisorExternalInterrupt =
        (1ULL << 63) | 9, // Supervisor external interrupt
    MachineExternalInterrupt = (1ULL << 63) | 11, // Machine external interrupt

    // Special sentinel meaning "no exception / not set".
    None = std::numeric_limits<reg_t>::max()
};

class Trap : public std::exception {
public:
    explicit Trap(addr_t pc, TrapCause cause, uint64_t tval)
        : pc(pc), cause(cause), tval(tval) {}

    [[noreturn]] __attribute__((noinline)) static void
    raise_exception(addr_t pc, TrapCause cause, uint64_t tval) {
        throw Trap(pc, cause, tval);
    }

    const char* what() const noexcept override { return "RISC-V Trap"; }

    const addr_t pc;
    const TrapCause cause;
    const uint64_t tval;
};

class DecodedInsn;
class CSR;

class FPR final {
public:
    FPR() : value_(float64_t{0}) {}

    float32_t read_32() const noexcept {
        if (is_boxed_f32(value_))
            return unbox_f32(value_);

        return float32_t{F32_DEFAULT_NAN};
    }

    float64_t read_64() const noexcept { return value_; }

    void write_32(float32_t x) noexcept { value_ = box_f32(x); }

    void write_64(float64_t x) noexcept { value_ = x; }

    void operator=(float64_t x) noexcept { write_64(x); }

    void operator=(float32_t x) noexcept { write_32(x); }

private:
    float64_t value_;
};

class RegisterFile {
public:
    RegisterFile() noexcept { gprs_.fill(0); }

    inline reg_t read(size_t idx) const noexcept {
        return idx ? gprs_[idx] : 0;
    }

    inline void write(size_t idx, reg_t value) noexcept {
        if (idx != 0) [[likely]]
            gprs_[idx] = value;
    }

    inline reg_t operator[](size_t idx) const noexcept { return read(idx); }

private:
    std::array<reg_t, 32> gprs_;
};

class MMU;

class Hart {
public:
    static constexpr size_t GPR_COUNT = 32;
    static constexpr size_t FPR_COUNT = 32;
    static constexpr size_t CSR_COUNT = 4096;

    explicit Hart(addr_t reset_pc = Dram::DRAM_BASE);

    void handle_trap(const Trap& trap) noexcept;
    void check_interrupts() const;
    void set_interrupt_pending(reg_t mip_mask, bool pending) noexcept;

    void connect_mmu(MMU* mmu) noexcept { this->mmu = mmu; }

    addr_t pc;
    RegisterFile gprs;
    std::array<FPR, FPR_COUNT> fprs;
    std::array<std::shared_ptr<CSR>, CSR_COUNT> csrs;
    PrivilegeLevel priv;
    MMU* mmu;

private:
    template <typename T>
    void add_csr() {
        csrs[T::ADDRESS] = std::make_shared<T>(this);
    }

    template <typename T>
    void add_csr(reg_t value) {
        csrs[T::ADDRESS] = std::make_shared<T>(this, value);
    }

    template <typename T>
    void add_csr_ranged() {
        for (size_t i = T::MIN_ADDRESS; i <= T::MAX_ADDRESS;
             i += T::DELTA_ADDRESS)
            csrs[i] = std::make_shared<T>(this);
    }
};

class CSR {
public:
    CSR(Hart* hart, PrivilegeLevel min_priv, reg_t value)
        : hart_(hart), min_priv_(min_priv), value_(value) {
        if (!hart_)
            std::terminate();
    }

    virtual ~CSR() = default;

    virtual reg_t read_unchecked() const noexcept { return value_; }

    virtual void write_unchecked(reg_t v) noexcept { value_ = v; }

    virtual reg_t read_checked(const DecodedInsn& insn) const;

    virtual void write_checked(const DecodedInsn& insn, reg_t v);

protected:
    virtual bool check_permissions() const noexcept {
        return hart_->priv >= min_priv_;
    }

    Hart* hart_;
    PrivilegeLevel min_priv_;
    reg_t value_;
};

// CSR that is not implemented. Any access (read or write) to this CSR via
// checked path will raise an exception (typically Illegal Instruction).
class UnimplementedCSR final : public CSR {
public:
    UnimplementedCSR(Hart* hart, size_t address, bool trace)
        : CSR(hart, PrivilegeLevel::M, 0), address_(address), trace_(trace) {}

    reg_t read_unchecked() const noexcept override { return 0; }

    void write_unchecked([[maybe_unused]] reg_t v) noexcept override {}

    [[noreturn]] reg_t read_checked(const DecodedInsn& insn) const override;

    [[noreturn]] void write_checked(const DecodedInsn& insn, reg_t v) override;

private:
    size_t address_;
    bool trace_;
};

// Read-only CSR. Writes to this CSR via the checked path will raise an
// exception.
class ConstCSR : public CSR {
public:
    ConstCSR(Hart* hart, PrivilegeLevel min_priv, reg_t value)
        : CSR(hart, min_priv, value) {}

    void write_unchecked([[maybe_unused]] reg_t v) noexcept override {}

    [[noreturn]] void write_checked(const DecodedInsn& insn, reg_t v) override;
};

// Hardwired CSR.
class HardwiredCSR : public CSR {
public:
    HardwiredCSR(Hart* hart, PrivilegeLevel min_priv, reg_t value)
        : CSR(hart, min_priv, value) {}

    void write_unchecked([[maybe_unused]] reg_t v) noexcept override {}
};

class MISA final : public HardwiredCSR {
public:
    static constexpr size_t ADDRESS = 0x301;

    enum Shift : uint32_t {
        A_SHIFT = 'A' - 'A',
        C_SHIFT = 'C' - 'A',
        D_SHIFT = 'D' - 'A',
        F_SHIFT = 'F' - 'A',
        I_SHIFT = 'I' - 'A',
        M_SHIFT = 'M' - 'A',

        S_SHIFT = 'S' - 'A',
        U_SHIFT = 'U' - 'A',

        MXL_SHIFT = 62,
    };

    enum Field : reg_t {
        A = 1ULL << A_SHIFT, // Atomic extension
        C = 1ULL << C_SHIFT, // Compressed extension
        D = 1ULL << D_SHIFT, // Double-precision floating-point extension
        F = 1ULL << F_SHIFT, // Single-precision floating-point extension
        I = 1ULL << I_SHIFT, // RV32I/64I base ISA
        M = 1ULL << M_SHIFT, // Integer Multiply/Divide extension

        S = 1ULL << S_SHIFT, // Supervisor mode implemented
        U = 1ULL << U_SHIFT, // User mode implemented

        MXL = 3ULL << MXL_SHIFT,
    };

    enum Mxl : reg_t {
        xlen_32 = 1,
        xlen_64 = 2,
    };

    MISA(Hart* hart, reg_t value)
        : HardwiredCSR(hart, PrivilegeLevel::M, value) {}
};

class MVENDORID final : public ConstCSR {
public:
    static constexpr size_t ADDRESS = 0xF11;

    MVENDORID(Hart* hart, reg_t value)
        : ConstCSR(hart, PrivilegeLevel::M, value) {}
};

class MARCHID final : public ConstCSR {
public:
    static constexpr size_t ADDRESS = 0xF12;

    MARCHID(Hart* hart, reg_t value)
        : ConstCSR(hart, PrivilegeLevel::M, value) {}
};

class MIMPID final : public ConstCSR {
public:
    static constexpr size_t ADDRESS = 0xF13;

    MIMPID(Hart* hart, reg_t value)
        : ConstCSR(hart, PrivilegeLevel::M, value) {}
};

class MHARTID final : public ConstCSR {
public:
    static constexpr size_t ADDRESS = 0xF14;

    MHARTID(Hart* hart, reg_t value)
        : ConstCSR(hart, PrivilegeLevel::M, value) {}
};

class MENVCFG final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x30A;

    enum Shift {
        FIOM_SHIFT = 0,
        LPE_SHIFT = 2,
        SSE_SHIFT = 3,
        CBIE_SHIFT = 4,
        CBCFE_SHIFT = 6,
        CBZE_SHIFT = 7,
        PMM_SHIFT = 32,
        DTE_SHIFT = 59,
        CDE_SHIFT = 60,
        ADUE_SHIFT = 61,
        PBMTE_SHIFT = 62,
        STCE_SHIFT = 63,
    };

    enum Field {
        FIOM = 1ULL << FIOM_SHIFT,
        LPE = 1ULL << LPE_SHIFT,
        SSE = 1ULL << SSE_SHIFT,
        CBIE = 3ULL << CBIE_SHIFT,
        CBCFE = 1ULL << CBCFE_SHIFT,
        CBZE = 1ULL << CBZE_SHIFT,
        PMM = 3ULL << PMM_SHIFT,
        DTE = 1ULL << DTE_SHIFT,
        CDE = 1ULL << CDE_SHIFT,
        ADUE = 1ULL << ADUE_SHIFT,
        PBMTE = 1ULL << PBMTE_SHIFT,
        STCE = 1ULL << STCE_SHIFT,
    };

    MENVCFG(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {
        value_atomic_.store(0, std::memory_order_relaxed);
    }

    reg_t read_unchecked() const noexcept override {
        return value_atomic_.load(std::memory_order_relaxed) & mask_;
    }

    void write_unchecked(reg_t v) noexcept override {
        value_atomic_.store(v & mask_, std::memory_order_relaxed);
    }

private:
    static constexpr reg_t mask_ = Field::FIOM | Field::ADUE | Field::STCE;

    std::atomic<reg_t> value_atomic_;
};

class MSTATUS final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x300;

    enum Shift : uint32_t {
        SIE_SHIFT = 1,
        MIE_SHIFT = 3,
        SPIE_SHIFT = 5,
        MPIE_SHIFT = 7,
        SPP_SHIFT = 8,
        MPP_SHIFT = 11,
        FS_SHIFT = 13,
        MPRV_SHIFT = 17,
        SUM_SHIFT = 18,
        MXR_SHIFT = 19,
        TVM_SHIFT = 20,
        TW_SHIFT = 21,
        TSR_SHIFT = 22,
        UXL_SHIFT = 32,
        SXL_SHIFT = 34,
        SD_SHIFT = 63
    };

    enum Field : reg_t {
        SIE = 1ULL << SIE_SHIFT,
        MIE = 1ULL << MIE_SHIFT,
        SPIE = 1ULL << SPIE_SHIFT,
        MPIE = 1ULL << MPIE_SHIFT,
        SPP = 1ULL << SPP_SHIFT,
        MPP = 3ULL << MPP_SHIFT,
        FS = 3ULL << FS_SHIFT,
        MPRV = 1ULL << MPRV_SHIFT,
        SUM = 1ULL << SUM_SHIFT,
        MXR = 1ULL << MXR_SHIFT,
        TVM = 1ULL << TVM_SHIFT,
        TW = 1ULL << TW_SHIFT,
        TSR = 1ULL << TSR_SHIFT,
        UXL = 3ULL << UXL_SHIFT,
        SXL = 3ULL << SXL_SHIFT,
        SD = 1ULL << SD_SHIFT
    };

    MSTATUS(Hart* hart);

    reg_t read_unchecked() const noexcept override {
        return value_ & read_mask_;
    }

    void write_unchecked(reg_t v) noexcept override {
        v = (value_ & ~write_mask_) | (v & write_mask_);

        if (((v & Field::FS) >> Shift::FS_SHIFT) == 3)
            v |= Field::SD;
        else
            v &= ~Field::SD;

        value_ = v;
    }

private:
    reg_t read_mask_;
    reg_t write_mask_;
};

class TVECCSR : public CSR {
public:
    enum Shift : uint32_t {
        MODE_SHIFT = 0,
        BASE_SHIFT = 2,
    };

    enum Field : reg_t {
        MODE = 3ULL << MODE_SHIFT,
        BASE = ~3ULL,
    };

    enum Mode : reg_t {
        DIRECT = 0,
        VECTORED = 1,
    };

    TVECCSR(Hart* hart, PrivilegeLevel min_priv) : CSR(hart, min_priv, 0) {}

    void write_unchecked(reg_t v) noexcept override { value_ = v & ~2ULL; }
};

class MTVEC final : public TVECCSR {
public:
    static constexpr size_t ADDRESS = 0x305;

    MTVEC(Hart* hart) : TVECCSR(hart, PrivilegeLevel::M) {}
};

class MEDELEG final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x302;

    MEDELEG(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {}

    reg_t read_unchecked() const noexcept override { return value_ & mask_; }

    void write_unchecked(reg_t v) noexcept override { value_ = v & mask_; }

private:
    // medeleg[11] (ECALL from M) and medeleg[16] (double trap) are read-only
    // zero
    static constexpr reg_t mask_ = ~((1ULL << 11) | (1ULL << 16));
};

class MIDELEG final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x303;

    MIDELEG(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {
        value_atomic_.store(0, std::memory_order_relaxed);
    }

    reg_t read_unchecked() const noexcept override {
        return value_atomic_.load(std::memory_order_relaxed);
    }

    void write_unchecked(reg_t v) noexcept override {
        value_atomic_.store(v, std::memory_order_relaxed);
    }

private:
    std::atomic<reg_t> value_atomic_;
};

class MIP final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x344;

    enum Shift : uint32_t {
        SSIP_SHIFT = 1,
        MSIP_SHIFT = 3,
        STIP_SHIFT = 5,
        MTIP_SHIFT = 7,
        SEIP_SHIFT = 9,
        MEIP_SHIFT = 11
    };

    enum Field : reg_t {
        SSIP = 1ULL << SSIP_SHIFT,
        MSIP = 1ULL << MSIP_SHIFT,
        STIP = 1ULL << STIP_SHIFT,
        MTIP = 1ULL << MTIP_SHIFT,
        SEIP = 1ULL << SEIP_SHIFT,
        MEIP = 1ULL << MEIP_SHIFT
    };

    MIP(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {
        value_atomic_.store(0, std::memory_order_relaxed);
        menvcfg_ = dynamic_cast<MENVCFG*>(hart_->csrs[MENVCFG::ADDRESS].get());
        assert(menvcfg_);
    }

    reg_t read_unchecked() const noexcept override {
        return value_atomic_.load(std::memory_order_relaxed) & read_mask_;
    }

    // For csr instructions
    void write_unchecked(reg_t v) noexcept override {
        reg_t old_val = value_atomic_.load(std::memory_order_relaxed);
        reg_t new_val;
        do {
            reg_t write_mask = write_mask_;

            if (!(menvcfg_->read_unchecked() & MENVCFG::Field::STCE))
                write_mask |= Field::STIP;

            new_val = (old_val & ~write_mask) | (v & write_mask);
        } while (!value_atomic_.compare_exchange_weak(
            old_val, new_val, std::memory_order_relaxed));
    }

    void set_pending(reg_t mask) noexcept {
        value_atomic_.fetch_or(mask & read_mask_, std::memory_order_relaxed);
    }

    void clear_pending(reg_t mask) noexcept {
        value_atomic_.fetch_and(~mask, std::memory_order_relaxed);
    }

private:
    static constexpr reg_t read_mask_ = Field::SSIP | Field::MSIP |
                                        Field::STIP | Field::MTIP |
                                        Field::SEIP | Field::MEIP;

    static constexpr reg_t write_mask_ = Field::SSIP | Field::SEIP;

    std::atomic<reg_t> value_atomic_;
    MENVCFG* menvcfg_;
};

class MIE final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x304;

    enum Shift : uint32_t {
        SSIE_SHIFT = 1,
        MSIE_SHIFT = 3,
        STIE_SHIFT = 5,
        MTIE_SHIFT = 7,
        SEIE_SHIFT = 9,
        MEIE_SHIFT = 11
    };

    enum Field : reg_t {
        SSIE = 1ULL << SSIE_SHIFT,
        MSIE = 1ULL << MSIE_SHIFT,
        STIE = 1ULL << STIE_SHIFT,
        MTIE = 1ULL << MTIE_SHIFT,
        SEIE = 1ULL << SEIE_SHIFT,
        MEIE = 1ULL << MEIE_SHIFT
    };

    MIE(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {}

    reg_t read_unchecked() const noexcept override { return value_ & mask_; }

    void write_unchecked(reg_t v) noexcept override { value_ = v & mask_; }

private:
    static constexpr reg_t mask_ = Field::SSIE | Field::MSIE | Field::STIE |
                                   Field::MTIE | Field::SEIE | Field::MEIE;
};

class MCOUNTINHIBIT final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x320;

    enum Shift : uint32_t {
        CY_SHIFT = 0,
        IR_SHIFT = 2,
    };

    enum Field : reg_t {
        CY = 1ULL << CY_SHIFT,
        IR = 1ULL << IR_SHIFT,
    };

    MCOUNTINHIBIT(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {}

    reg_t read_unchecked() const noexcept override { return value_ & mask_; }

    void write_unchecked(reg_t v) noexcept override { value_ = v & mask_; }

private:
    static constexpr reg_t mask_ = ~2ULL;
};

class MCYCLE final : public CSR {
public:
    static constexpr size_t ADDRESS = 0xB00;

    MCYCLE(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {
        mcountinhibit_ = dynamic_cast<MCOUNTINHIBIT*>(
            hart_->csrs[MCOUNTINHIBIT::ADDRESS].get());
        assert(mcountinhibit_);
    }

    void advance() noexcept {
        if (mcountinhibit_->read_unchecked() & MCOUNTINHIBIT::Field::CY)
            return;

        value_++;
    }

private:
    MCOUNTINHIBIT* mcountinhibit_;
};

class MINSTRET final : public CSR {
public:
    static constexpr size_t ADDRESS = 0xB02;

    MINSTRET(Hart* hart)
        : CSR(hart, PrivilegeLevel::M, 0), increase_suppressed_(false) {
        mcountinhibit_ = dynamic_cast<MCOUNTINHIBIT*>(
            hart_->csrs[MCOUNTINHIBIT::ADDRESS].get());
        assert(mcountinhibit_);
    }

    void write_checked(const DecodedInsn& insn, reg_t v) override {
        CSR::write_checked(insn, v);
        increase_suppressed_ = true;
    }

    void advance() noexcept {
        if (!increase_suppressed_ && !(mcountinhibit_->read_unchecked() &
                                       MCOUNTINHIBIT::Field::IR)) [[likely]]
            value_++;

        increase_suppressed_ = false;
    }

private:
    MCOUNTINHIBIT* mcountinhibit_;
    bool increase_suppressed_;
};

class MHPMCOUNTERN final : public HardwiredCSR {
public:
    static constexpr size_t MIN_ADDRESS = 0xB03;
    static constexpr size_t MAX_ADDRESS = 0xB1F;
    static constexpr size_t DELTA_ADDRESS = 1;

    MHPMCOUNTERN(Hart* hart) : HardwiredCSR(hart, PrivilegeLevel::M, 0) {}
};

class MHPMEVENTN final : public HardwiredCSR {
public:
    static constexpr size_t MIN_ADDRESS = 0x323;
    static constexpr size_t MAX_ADDRESS = 0x33F;
    static constexpr size_t DELTA_ADDRESS = 1;

    MHPMEVENTN(Hart* hart) : HardwiredCSR(hart, PrivilegeLevel::M, 0) {}
};

class MCOUNTEREN final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x306;

    enum Shift : uint32_t { CY_SHIFT = 0, TM_SHIFT = 1, IR_SHIFT = 2 };

    enum Field : reg_t {
        CY = 1ULL << CY_SHIFT,
        TM = 1ULL << TM_SHIFT,
        IR = 1ULL << IR_SHIFT
    };

    MCOUNTEREN(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {}

    // Check the availability of the hardware performance-monitoring counters
    // (0xC00 to 0xC1F)
    bool hpm_available_to_supervisor_and_user(size_t csr_addr) const noexcept {
        if (csr_addr == 0x14D) // stimecmp
            return value_ & TM;

        if (csr_addr < 0xC00 || csr_addr > 0xC1F) [[unlikely]]
            std::terminate();

        return value_ & (1ULL << (csr_addr - 0xC00));
    }
};

class MSCRATCH final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x340;

    MSCRATCH(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {}
};

class EPCCSR : public CSR {
public:
    EPCCSR(Hart* hart, PrivilegeLevel min_priv) : CSR(hart, min_priv, 0) {
        write_mask_ = ~1ULL;
        read_mask_ = ~1ULL;
    }

    reg_t read_unchecked() const noexcept override {
        return value_ & read_mask_;
    }

    void write_unchecked(reg_t v) noexcept override {
        value_ = (value_ & ~write_mask_) | (v & write_mask_);
    }

private:
    reg_t read_mask_;
    reg_t write_mask_;
};

class MEPC final : public EPCCSR {
public:
    static constexpr size_t ADDRESS = 0x341;

    MEPC(Hart* hart) : EPCCSR(hart, PrivilegeLevel::M) {}
};

class CAUSECSR : public CSR {
public:
    CAUSECSR(Hart* hart, PrivilegeLevel min_priv) : CSR(hart, min_priv, 0) {}

    void write_unchecked(reg_t v) noexcept override {
        if (is_valid_cause_value(v)) [[likely]]
            value_ = v;
    }

    bool is_valid_cause_value(uint64_t value) noexcept {
        switch (value) {
            case static_cast<uint64_t>(TrapCause::InstructionAddressMisaligned):
            case static_cast<uint64_t>(TrapCause::InstructionAccessFault):
            case static_cast<uint64_t>(TrapCause::IllegalInstruction):
            case static_cast<uint64_t>(TrapCause::Breakpoint):
            case static_cast<uint64_t>(TrapCause::LoadAddressMisaligned):
            case static_cast<uint64_t>(TrapCause::LoadAccessFault):
            case static_cast<uint64_t>(TrapCause::StoreAMOAddressMisaligned):
            case static_cast<uint64_t>(TrapCause::StoreAMOAccessFault):
            case static_cast<uint64_t>(TrapCause::EnvironmentCallFromU):
            case static_cast<uint64_t>(TrapCause::EnvironmentCallFromS):
            case static_cast<uint64_t>(TrapCause::InstructionPageFault):
            case static_cast<uint64_t>(TrapCause::LoadPageFault):
            case static_cast<uint64_t>(TrapCause::StoreAMOPageFault):
            case static_cast<uint64_t>(TrapCause::SupervisorSoftwareInterrupt):
            case static_cast<uint64_t>(TrapCause::SupervisorTimerInterrupt):
            case static_cast<uint64_t>(TrapCause::SupervisorExternalInterrupt):
                return true;

            // MCAUSE-only values
            case static_cast<uint64_t>(TrapCause::EnvironmentCallFromM):
            case static_cast<uint64_t>(TrapCause::MachineSoftwareInterrupt):
            case static_cast<uint64_t>(TrapCause::MachineTimerInterrupt):
            case static_cast<uint64_t>(TrapCause::MachineExternalInterrupt):
                return min_priv_ == PrivilegeLevel::M;

            default: return false;
        }
    }
};

class MCAUSE final : public CAUSECSR {
public:
    static constexpr size_t ADDRESS = 0x342;

    MCAUSE(Hart* hart) : CAUSECSR(hart, PrivilegeLevel::M) {}
};

class MTVAL final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x343;

    MTVAL(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {}
};

class MCONFIGPTR final : public ConstCSR {
public:
    static constexpr size_t ADDRESS = 0xF15;

    MCONFIGPTR(Hart* hart) : ConstCSR(hart, PrivilegeLevel::M, 0) {}
};

class PMPCFGN final : public HardwiredCSR {
public:
    static constexpr size_t MIN_ADDRESS = 0x3A0;
    static constexpr size_t MAX_ADDRESS = 0x3AF;
    static constexpr size_t DELTA_ADDRESS = 2;

    PMPCFGN(Hart* hart) : HardwiredCSR(hart, PrivilegeLevel::M, 0) {}
};

class PMPADDRN final : public CSR {
public:
    static constexpr size_t MIN_ADDRESS = 0x3B0;
    static constexpr size_t MAX_ADDRESS = 0x3EF;
    static constexpr size_t DELTA_ADDRESS = 1;

    PMPADDRN(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {}
};

class TSELECT final : public HardwiredCSR {
public:
    static constexpr size_t ADDRESS = 0x7A0;

    TSELECT(Hart* hart) : HardwiredCSR(hart, PrivilegeLevel::M, 0) {}
};

class TDATAN final : public HardwiredCSR {
public:
    static constexpr size_t MIN_ADDRESS = 0x7A1;
    static constexpr size_t MAX_ADDRESS = 0x7A3;
    static constexpr size_t DELTA_ADDRESS = 1;

    TDATAN(Hart* hart) : HardwiredCSR(hart, PrivilegeLevel::M, 0) {}
};

class SSTATUS final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x100;

    using Shift = MSTATUS::Shift;
    using Field = MSTATUS::Field;
    using F = MSTATUS::Field;
    using S = MSTATUS::Shift;

    SSTATUS(Hart* hart) : CSR(hart, PrivilegeLevel::S, 0) {
        mstatus_ = dynamic_cast<MSTATUS*>(hart->csrs[MSTATUS::ADDRESS].get());
        assert(mstatus_);
    }

    reg_t read_unchecked() const noexcept override {
        return mstatus_->read_unchecked() & read_mask_;
    }

    void write_unchecked(reg_t v) noexcept override {
        reg_t old_value = mstatus_->read_unchecked();
        v = (old_value & ~write_mask_) | (v & write_mask_);
        mstatus_->write_unchecked(v);
    }

private:
    static constexpr reg_t read_mask_ =
        F::SIE | F::SPIE | F::SPP | F::FS | F::SUM | F::MXR | F::UXL | F::SD;

    static constexpr reg_t write_mask_ =
        F::SIE | F::SPIE | F::SPP | F::SUM | F::MXR;

    MSTATUS* mstatus_;
};

class STVEC final : public TVECCSR {
public:
    static constexpr size_t ADDRESS = 0x105;

    STVEC(Hart* hart) : TVECCSR(hart, PrivilegeLevel::S) {}
};

class SIP final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x144;

    using Shift = MIP::Shift;
    using Field = MIP::Field;

    SIP(Hart* hart) : CSR(hart, PrivilegeLevel::S, 0) {
        mip_ = dynamic_cast<MIP*>(hart->csrs[MIP::ADDRESS].get());
        mideleg_ = dynamic_cast<MIDELEG*>(hart->csrs[MIDELEG::ADDRESS].get());
        assert(mip_ && mideleg_);
    }

    reg_t read_unchecked() const noexcept override {
        return mip_->read_unchecked() & mask_ & mideleg_->read_unchecked();
    }

    void write_unchecked(reg_t v) noexcept override {
        mip_->write_unchecked(v & mask_ & mideleg_->read_unchecked());
    }

private:
    static constexpr reg_t mask_ = Field::SSIP | Field::STIP | Field::SEIP;

    MIP* mip_;
    MIDELEG* mideleg_;
};

class SIE final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x104;

    using Shift = MIE::Shift;
    using Field = MIE::Field;

    SIE(Hart* hart) : CSR(hart, PrivilegeLevel::S, 0) {
        mie_ = dynamic_cast<MIE*>(hart->csrs[MIE::ADDRESS].get());
        assert(mie_);
    }

    reg_t read_unchecked() const noexcept override {
        return mie_->read_unchecked() & mask_;
    }

    void write_unchecked(reg_t v) noexcept override {
        mie_->write_unchecked(v & mask_);
    }

private:
    static constexpr reg_t mask_ = Field::SSIE | Field::STIE | Field::SEIE;

    MIE* mie_;
};

class SCOUNTEREN final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x106;

    using Shift = MCOUNTEREN::Shift;
    using Field = MCOUNTEREN::Field;

    SCOUNTEREN(Hart* hart) : CSR(hart, PrivilegeLevel::S, 0) {}

    // Check the availability of the hardware performance-monitoring counters
    // (0xC00 to 0xC1F)
    // Note: MCOUNTEREN::hpm_available() must also be called for U-mode.
    bool hpm_available_to_user(size_t csr_addr) const noexcept {
        if (csr_addr < 0xC00 || csr_addr > 0xC1F) [[unlikely]]
            std::terminate();

        return value_ & (1ULL << (csr_addr - 0xC00));
    }
};

class SSCRATCH final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x140;

    SSCRATCH(Hart* hart) : CSR(hart, PrivilegeLevel::S, 0) {}
};

class SEPC final : public EPCCSR {
public:
    static constexpr size_t ADDRESS = 0x141;

    SEPC(Hart* hart) : EPCCSR(hart, PrivilegeLevel::S) {}
};

class SCAUSE final : public CAUSECSR {
public:
    static constexpr size_t ADDRESS = 0x142;

    SCAUSE(Hart* hart) : CAUSECSR(hart, PrivilegeLevel::S) {}
};

class STVAL final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x143;

    STVAL(Hart* hart) : CSR(hart, PrivilegeLevel::S, 0) {}
};

class SENVCFG final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x10A;

    using Shift = MENVCFG::Shift;
    using Field = MENVCFG::Field;

    SENVCFG(Hart* hart) : CSR(hart, PrivilegeLevel::S, 0) {}

    reg_t read_unchecked() const noexcept override { return value_ & mask_; }

    void write_unchecked(reg_t v) noexcept override { value_ = v & mask_; }

private:
    static constexpr reg_t mask_ = Field::FIOM;
};

class SATP final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x180;

    enum Shift : uint32_t { PPN_SHIFT = 0, ASID_SHIFT = 44, MODE_SHIFT = 60 };

    enum Field : reg_t {
        PPN = ((1ULL << 44) - 1) << PPN_SHIFT,
        ASID = ((1ULL << 16) - 1) << ASID_SHIFT,
        MODE = 0xFULL << MODE_SHIFT
    };

    enum Mode : reg_t { Bare = 0, Sv39 = 8, Sv48 = 9, Sv57 = 10 };

    SATP(Hart* hart) : CSR(hart, PrivilegeLevel::S, 0) {
        mstatus_ = dynamic_cast<MSTATUS*>(hart->csrs[MSTATUS::ADDRESS].get());
        assert(mstatus_);
    }

    void write_unchecked(reg_t v) noexcept override;

    bool check_permissions() const noexcept override {
        if (hart_->priv == PrivilegeLevel::S &&
            (mstatus_->read_unchecked() & MSTATUS::TVM))
            return false;

        return CSR::check_permissions();
    }

private:
    MSTATUS* mstatus_;
};

class STIMECMP : public CSR {
public:
    static constexpr size_t ADDRESS = 0x14D;

    STIMECMP(Hart* hart) : CSR(hart, PrivilegeLevel::S, 0) {
        mcounteren_ =
            dynamic_cast<MCOUNTEREN*>(hart_->csrs[MCOUNTEREN::ADDRESS].get());
        menvcfg_ = dynamic_cast<MENVCFG*>(hart_->csrs[MENVCFG::ADDRESS].get());
        assert(mcounteren_ && menvcfg_);
    }

    reg_t read_unchecked() const noexcept override {
        return value_atomic_.load(std::memory_order_relaxed);
    }

    void write_unchecked(reg_t v) noexcept override {
        value_atomic_.store(v, std::memory_order_relaxed);
    }

protected:
    bool check_permissions() const noexcept override {
        if (hart_->priv == PrivilegeLevel::M)
            return true;

        if (hart_->priv == PrivilegeLevel::U)
            return false;

        if (!(mcounteren_->read_unchecked() & core::MCOUNTEREN::TM))
            return false;

        if (!(menvcfg_->read_unchecked() & core::MENVCFG::STCE))
            return false;

        return true;
    }

private:
    MCOUNTEREN* mcounteren_;
    MENVCFG* menvcfg_;

    std::atomic<reg_t> value_atomic_;
};

class UserCounterCSR : public ConstCSR {
public:
    UserCounterCSR(Hart* hart, size_t address, size_t mirrored_address)
        : ConstCSR(hart, PrivilegeLevel::U, 0), address_(address),
          mirrored_address_(mirrored_address) {
        mcounteren_ =
            dynamic_cast<MCOUNTEREN*>(hart_->csrs[MCOUNTEREN::ADDRESS].get());
        scounteren_ =
            dynamic_cast<SCOUNTEREN*>(hart_->csrs[SCOUNTEREN::ADDRESS].get());
        assert(mcounteren_ && scounteren_);
    }

    reg_t read_unchecked() const noexcept override {
        return hart_->csrs[mirrored_address_]->read_unchecked();
    }

protected:
    bool check_permissions() const noexcept override {
        if (hart_->priv == PrivilegeLevel::M)
            return true;

        if (!mcounteren_->hpm_available_to_supervisor_and_user(address_))
            return false;

        if (hart_->priv == PrivilegeLevel::U &&
            !scounteren_->hpm_available_to_user(address_))
            return false;

        return true;
    }

private:
    size_t address_;
    size_t mirrored_address_;

    MCOUNTEREN* mcounteren_;
    SCOUNTEREN* scounteren_;
};

class CYCLE final : public UserCounterCSR {
public:
    static constexpr size_t ADDRESS = 0xC00;

    CYCLE(Hart* hart) : UserCounterCSR(hart, ADDRESS, MCYCLE::ADDRESS) {}
};

class TIME final : public ConstCSR {
public:
    static constexpr size_t ADDRESS = 0xC01;

    TIME(Hart* hart) : ConstCSR(hart, PrivilegeLevel::U, 0) {
        mcounteren_ =
            dynamic_cast<MCOUNTEREN*>(hart_->csrs[MCOUNTEREN::ADDRESS].get());
        scounteren_ =
            dynamic_cast<SCOUNTEREN*>(hart_->csrs[SCOUNTEREN::ADDRESS].get());
        assert(mcounteren_ && scounteren_);
    }

    reg_t read_unchecked() const noexcept override {
        return value_atomic_.load(std::memory_order_relaxed);
    }

    void write_unchecked(reg_t v) noexcept override {
        value_atomic_.store(v, std::memory_order_relaxed);
    }

    void mirror_from_mtime(reg_t mtime_value) noexcept {
        value_atomic_.store(mtime_value, std::memory_order_relaxed);
    }

protected:
    bool check_permissions() const noexcept override {
        if (hart_->priv == PrivilegeLevel::M)
            return true;

        if (!mcounteren_->hpm_available_to_supervisor_and_user(ADDRESS))
            return false;

        if (hart_->priv == PrivilegeLevel::U &&
            !scounteren_->hpm_available_to_user(ADDRESS))
            return false;

        return true;
    }

private:
    MCOUNTEREN* mcounteren_;
    SCOUNTEREN* scounteren_;

    std::atomic<reg_t> value_atomic_;
};

class INSTRET final : public UserCounterCSR {
public:
    static constexpr size_t ADDRESS = 0xC02;

    INSTRET(Hart* hart) : UserCounterCSR(hart, ADDRESS, MINSTRET::ADDRESS) {}
};

class HPMCOUNTERN final : public UserCounterCSR {
public:
    static constexpr size_t MIN_ADDRESS = 0xC03;
    static constexpr size_t MAX_ADDRESS = 0xC1F;
    static constexpr size_t DELTA_ADDRESS = 1;

    HPMCOUNTERN(Hart* hart, size_t address)
        : UserCounterCSR(hart, address,
                         MHPMCOUNTERN::MIN_ADDRESS + address -
                             HPMCOUNTERN::MIN_ADDRESS) {}
};

class FFLAGS final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x001;

    enum Shift : uint32_t {
        NX_SHIFT = 0,
        UF_SHIFT = 1,
        OF_SHIFT = 2,
        DZ_SHIFT = 3,
        NV_SHIFT = 4
    };

    enum Field : reg_t {
        NX = 1ULL << NX_SHIFT,
        UF = 1ULL << UF_SHIFT,
        OF = 1ULL << OF_SHIFT,
        DZ = 1ULL << DZ_SHIFT,
        NV = 1ULL << NV_SHIFT,
    };

    FFLAGS(Hart* hart) : CSR(hart, PrivilegeLevel::U, 0) {}

    reg_t read_unchecked() const noexcept override { return value_ & 0b11111; }

    void write_unchecked(reg_t v) noexcept override { value_ = v & 0b11111; }
};

class FRM final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x002;

    enum RoundingMode : uint8_t {
        RNE = 0b000,
        RTZ = 0b001,
        RDN = 0b010,
        RUP = 0b011,
        RMM = 0b100,
        DYN = 0b111, // In instruction’s rm field
    };

    FRM(Hart* hart) : CSR(hart, PrivilegeLevel::U, 0) {}

    reg_t read_unchecked() const noexcept override { return value_ & 0b111; }

    void write_unchecked(reg_t v) noexcept override { value_ = v & 0b111; }
};

class FCSR final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x003;

    FCSR(Hart* hart) : CSR(hart, PrivilegeLevel::U, 0) {
        fflags_ = dynamic_cast<FFLAGS*>(hart->csrs[FFLAGS::ADDRESS].get());
        frm_ = dynamic_cast<FRM*>(hart_->csrs[FRM::ADDRESS].get());
        assert(fflags_ && frm_);
    }

    reg_t read_unchecked() const noexcept override {
        return fflags_->read_unchecked() | (frm_->read_unchecked() << 5);
    }

    void write_unchecked(reg_t v) noexcept override {
        fflags_->write_unchecked(v & 0b11111);
        frm_->write_unchecked((v >> 5) & 0b111);
    }

private:
    FFLAGS* fflags_;
    FRM* frm_;
};

} // namespace uemu::core
