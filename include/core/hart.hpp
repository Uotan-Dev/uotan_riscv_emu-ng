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

#include <array>
#include <limits>
#include <memory>

#include "core/dram.hpp"

namespace uemu::core {

enum class PrivilegeLevel : uint8_t {
    U = 0, // User mode
    S = 1, // Supervisor mode
    M = 3  // Machine mode
};

enum class Exception : uint64_t {
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

    // Special sentinel meaning "no exception / not set".
    None = std::numeric_limits<uint64_t>::max()
};

class Trap : public std::exception {
public:
    explicit Trap(addr_t pc, Exception cause, uint64_t tval)
        : pc(pc), cause(cause), tval(tval) {}

    [[noreturn]] __attribute__((noinline)) static void
    raise_exception(addr_t pc, Exception cause, uint64_t tval) {
        throw Trap(pc, cause, tval);
    }

    const char* what() const noexcept override { return "RISC-V Trap"; }

    const addr_t pc;
    const Exception cause;
    const uint64_t tval;
};

class DecodedInsn;
class CSR;

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

class Hart {
public:
    static constexpr size_t GPR_COUNT = 32;
    static constexpr size_t CSR_COUNT = 4096;

    explicit Hart(addr_t reset_pc = Dram::DRAM_BASE);

    void handle_exception(const Trap& trap) noexcept;

    RegisterFile gprs;
    addr_t pc;
    std::array<std::shared_ptr<CSR>, CSR_COUNT> csrs;
    PrivilegeLevel priv;
};

class CSR {
public:
    explicit CSR(Hart* hart, PrivilegeLevel min_priv, reg_t value)
        : hart_(hart), min_priv_(min_priv), value_(value) {
        if (!hart_)
            std::terminate();
    }

    virtual ~CSR() = default;

    virtual reg_t read_unchecked() const noexcept { return value_; }

    virtual void write_unchecked(reg_t v) noexcept { value_ = v; }

    virtual reg_t read_checked(const DecodedInsn& insn,
                               PrivilegeLevel priv) const;

    virtual void write_checked(const DecodedInsn& insn, PrivilegeLevel priv,
                               reg_t v);

    void write_checked(const DecodedInsn& insn, reg_t v) {
        write_checked(insn, hart_->priv, v);
    }

    reg_t read_checked(const DecodedInsn& insn) const {
        return read_checked(insn, hart_->priv);
    }

protected:
    virtual bool check_permissions(PrivilegeLevel priv) const noexcept {
        return priv >= min_priv_;
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

    [[noreturn]] reg_t read_checked(const DecodedInsn& insn,
                                    PrivilegeLevel priv) const override;

    [[noreturn]] void write_checked(const DecodedInsn& insn,
                                    PrivilegeLevel priv, reg_t v) override;

private:
    size_t address_;
    bool trace_;
};

// Read-only CSR. Writes to this CSR via the checked path will raise an
// exception.
class ConstCSR : public CSR {
public:
    explicit ConstCSR(Hart* hart, PrivilegeLevel min_priv, reg_t value)
        : CSR(hart, min_priv, value) {}

    void write_unchecked([[maybe_unused]] reg_t v) noexcept override {}

    [[noreturn]] void write_checked(const DecodedInsn& insn,
                                    PrivilegeLevel priv, reg_t v) override;
};

// Hardwired CSR.
class HardwiredCSR : public CSR {
public:
    explicit HardwiredCSR(Hart* hart, PrivilegeLevel min_priv, reg_t value)
        : CSR(hart, min_priv, value) {}

    void write_unchecked([[maybe_unused]] reg_t v) noexcept override {}
};

class MISA final : public HardwiredCSR {
public:
    static constexpr size_t ADDRESS = 0x301;

    enum shift : uint32_t {
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

    enum field : reg_t {
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

    explicit MISA(Hart* hart, reg_t value)
        : HardwiredCSR(hart, PrivilegeLevel::M, value) {}
};

class MVENDORID final : public ConstCSR {
public:
    static constexpr size_t ADDRESS = 0xF11;

    explicit MVENDORID(Hart* hart, reg_t value)
        : ConstCSR(hart, PrivilegeLevel::M, value) {}
};

class MARCHID final : public ConstCSR {
public:
    static constexpr size_t ADDRESS = 0xF12;

    explicit MARCHID(Hart* hart, reg_t value)
        : ConstCSR(hart, PrivilegeLevel::M, value) {}
};

class MIMPID final : public ConstCSR {
public:
    static constexpr size_t ADDRESS = 0xF13;

    explicit MIMPID(Hart* hart, reg_t value)
        : ConstCSR(hart, PrivilegeLevel::M, value) {}
};

class MHARTID final : public ConstCSR {
public:
    static constexpr size_t ADDRESS = 0xF14;

    explicit MHARTID(Hart* hart, reg_t value)
        : ConstCSR(hart, PrivilegeLevel::M, value) {}
};

class MSTATUS final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x300;

    enum shift : uint32_t {
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

    enum field : reg_t {
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

    explicit MSTATUS(Hart* hart);

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

class MTVEC final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x305;

    enum shift : uint32_t {
        MODE_SHIFT = 0,
        BASE_SHIFT = 2,
    };

    enum field : reg_t {
        MODE = 3ULL << MODE_SHIFT,
        BASE = ~3ULL,
    };

    enum Mode : reg_t {
        DIRECT = 0,
        VECTORED = 1,
    };

    MTVEC(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {}

    void write_unchecked(reg_t v) noexcept override { value_ = v & ~2ULL; }
};

class MSCRATCH final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x340;

    MSCRATCH(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {}
};

class MEPC final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x341;

    MEPC(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {
        write_mask_ = ~1ULL;
        read_mask_ = (hart_->csrs[MISA::ADDRESS]->read_unchecked() & MISA::C)
                         ? ~1ULL
                         : ~3ULL;
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

class MCAUSE final : public CSR {
public:
    static constexpr size_t ADDRESS = 0x342;

    MCAUSE(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {}

    void write_unchecked(reg_t v) noexcept override {
        if (is_valid_cause_value(v)) [[likely]]
            value_ = v;
    }

    // FIXME: Add interruptions
    static constexpr bool is_valid_cause_value(uint64_t value) noexcept {
        switch (value) {
            case static_cast<uint64_t>(Exception::InstructionAddressMisaligned):
            case static_cast<uint64_t>(Exception::InstructionAccessFault):
            case static_cast<uint64_t>(Exception::IllegalInstruction):
            case static_cast<uint64_t>(Exception::Breakpoint):
            case static_cast<uint64_t>(Exception::LoadAddressMisaligned):
            case static_cast<uint64_t>(Exception::LoadAccessFault):
            case static_cast<uint64_t>(Exception::StoreAMOAddressMisaligned):
            case static_cast<uint64_t>(Exception::StoreAMOAccessFault):
            case static_cast<uint64_t>(Exception::EnvironmentCallFromU):
            case static_cast<uint64_t>(Exception::EnvironmentCallFromS):
            case static_cast<uint64_t>(Exception::EnvironmentCallFromM):
            case static_cast<uint64_t>(Exception::InstructionPageFault):
            case static_cast<uint64_t>(Exception::LoadPageFault):
            case static_cast<uint64_t>(Exception::StoreAMOPageFault):
                return true;

            case static_cast<uint64_t>(Exception::None):
            default: return false;
        }
    }
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

class MCYCLE final : public CSR {
public:
    static constexpr size_t ADDRESS = 0xB00;

    MCYCLE(Hart* hart) : CSR(hart, PrivilegeLevel::M, 0) {}

    void advance() noexcept { value_++; }
};

class MINSTRET final : public CSR {
public:
    static constexpr size_t ADDRESS = 0xB02;

    MINSTRET(Hart* hart)
        : CSR(hart, PrivilegeLevel::M, 0), increase_suppressed_(false) {}

    void write_checked(const DecodedInsn& insn, PrivilegeLevel priv,
                       reg_t v) override {
        CSR::write_checked(insn, priv, v);
        increase_suppressed_ = true;
    }

    void advance() noexcept {
        if (!increase_suppressed_) [[likely]]
            value_++;
        else
            increase_suppressed_ = false;
    }

private:
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

class CYCLE final : public ConstCSR {
public:
    static constexpr size_t ADDRESS = 0xC00;

    CYCLE(Hart* hart) : ConstCSR(hart, PrivilegeLevel::U, 0) {
        if (!hart_->csrs[MCYCLE::ADDRESS] ||
            dynamic_cast<UnimplementedCSR*>(hart_->csrs[MCYCLE::ADDRESS].get()))
            std::terminate();
    }

    reg_t read_unchecked() const noexcept override {
        return hart_->csrs[MCYCLE::ADDRESS]->read_unchecked();
    }
};

class INSTRET final : public ConstCSR {
public:
    static constexpr size_t ADDRESS = 0xC02;

    INSTRET(Hart* hart) : ConstCSR(hart, PrivilegeLevel::U, 0) {
        if (!hart_->csrs[MINSTRET::ADDRESS] ||
            dynamic_cast<UnimplementedCSR*>(
                hart_->csrs[MINSTRET::ADDRESS].get()))
            std::terminate();
    }

    reg_t read_unchecked() const noexcept override {
        return hart_->csrs[MINSTRET::ADDRESS]->read_unchecked();
    }
};

class HPMCOUNTERN final : public ConstCSR {
public:
    static constexpr size_t MIN_ADDRESS = 0xC03;
    static constexpr size_t MAX_ADDRESS = 0xC1F;
    static constexpr size_t DELTA_ADDRESS = 1;

    HPMCOUNTERN(Hart* hart, size_t address)
        : ConstCSR(hart, PrivilegeLevel::U, 0), address_(address) {
        size_t mhpmcounter_address =
            MHPMCOUNTERN::MIN_ADDRESS + address_ - HPMCOUNTERN::MIN_ADDRESS;
        if (!hart_->csrs[mhpmcounter_address] ||
            dynamic_cast<UnimplementedCSR*>(
                hart_->csrs[mhpmcounter_address].get()))
            std::terminate();
    }

    reg_t read_unchecked() const noexcept override {
        return hart_
            ->csrs[MHPMCOUNTERN::MIN_ADDRESS + address_ -
                   HPMCOUNTERN::MIN_ADDRESS]
            ->read_unchecked();
    }

private:
    size_t address_;
};

} // namespace uemu::core
