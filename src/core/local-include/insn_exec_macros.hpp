// === FP Helper Macros (copy-paste from execute.cpp lines 47-88) ===

#define FP_INST_PREP()                                                         \
    do {                                                                       \
        assert(softfloat_exceptionFlags == 0);                                 \
        if (!(csrs[MSTATUS::ADDRESS]->read_unchecked() &                       \
              MSTATUS::Field::FS)) [[unlikely]]                                \
            Trap::raise_exception(pc, TrapCause::IllegalInstruction, insn);    \
    } while (0)

#define FP_SETUP_RM()                                                          \
    do {                                                                       \
        auto rm = bits(insn, 14, 12);                                          \
        if (rm == FRM::RoundingMode::DYN)                                      \
            rm = csrs[FRM::ADDRESS]->read_unchecked();                         \
        if (rm > FRM::RoundingMode::RMM) [[unlikely]]                          \
            Trap::raise_exception(pc, TrapCause::IllegalInstruction, insn);    \
        else                                                                   \
            softfloat_roundingMode = rm;                                       \
    } while (0)

#define FP_SET_DIRTY()                                                         \
    do {                                                                       \
        CSR* mstatus = csrs[MSTATUS::ADDRESS].get();                           \
        reg_t v = mstatus->read_unchecked();                                   \
        mstatus->write_unchecked(v | MSTATUS::Field::FS);                      \
    } while (0)

#define FP_UPDATE_FLAGS()                                                      \
    do {                                                                       \
        if (softfloat_exceptionFlags) {                                        \
            FP_SET_DIRTY();                                                    \
            CSR* fflags = csrs[FFLAGS::ADDRESS].get();                         \
            reg_t v = fflags->read_unchecked();                                \
            fflags->write_unchecked(v | softfloat_exceptionFlags);             \
            softfloat_exceptionFlags = 0;                                      \
        }                                                                      \
    } while (0)

#define FP_INST_END()                                                          \
    do {                                                                       \
        FP_SET_DIRTY();                                                        \
        FP_UPDATE_FLAGS();                                                     \
    } while (0)

// === RV64I Base Integer Instructions ===
// (copy-paste from execute.cpp lines 96-201, applying 7 substitutions)

// ALU register-register
#define EXEC_ADD()   R.write(rd, R[rs1] + R[rs2])
#define EXEC_SUB()   R.write(rd, R[rs1] - R[rs2])
#define EXEC_AND()   R.write(rd, R[rs1] & R[rs2])
#define EXEC_OR()    R.write(rd, R[rs1] | R[rs2])
#define EXEC_XOR()   R.write(rd, R[rs1] ^ R[rs2])
#define EXEC_SLL()   R.write(rd, R[rs1] << bits(R[rs2], 5, 0))
#define EXEC_SRL()   R.write(rd, R[rs1] >> bits(R[rs2], 5, 0))
#define EXEC_SRA()   R.write(rd, static_cast<int64_t>(R[rs1]) >> bits(R[rs2], 5, 0))
#define EXEC_SLT()   R.write(rd, static_cast<int64_t>(R[rs1]) < static_cast<int64_t>(R[rs2]))
#define EXEC_SLTU()  R.write(rd, R[rs1] < R[rs2])

// ALU register-immediate
#define EXEC_ADDI()  R.write(rd, R[rs1] + imm)
#define EXEC_ANDI()  R.write(rd, R[rs1] & imm)
#define EXEC_ORI()   R.write(rd, R[rs1] | imm)
#define EXEC_XORI()  R.write(rd, R[rs1] ^ imm)
#define EXEC_SLLI()  R.write(rd, R[rs1] << bits(imm, 5, 0))
#define EXEC_SRLI()  R.write(rd, R[rs1] >> bits(imm, 5, 0))
#define EXEC_SRAI()  R.write(rd, static_cast<int64_t>(R[rs1]) >> bits(imm, 5, 0))
#define EXEC_SLTI()  R.write(rd, static_cast<int64_t>(R[rs1]) < static_cast<int64_t>(imm))
#define EXEC_SLTIU() R.write(rd, R[rs1] < static_cast<reg_t>(imm))

// RV64I 32-bit word ALU
#define EXEC_ADDW()  R.write(rd, sext(bits(R[rs1] + R[rs2], 31, 0), 32))
#define EXEC_SUBW()  R.write(rd, sext(bits(R[rs1] - R[rs2], 31, 0), 32))
#define EXEC_ADDIW() R.write(rd, sext(bits(R[rs1] + imm, 31, 0), 32))
#define EXEC_SLLW()                                                            \
    R.write(rd, sext(static_cast<uint32_t>(bits(R[rs1], 31, 0))                \
                         << bits(R[rs2], 4, 0),                                \
                     32))
#define EXEC_SRLW()                                                            \
    R.write(rd, sext(bits(R[rs1], 31, 0) >> bits(R[rs2], 4, 0), 32))
#define EXEC_SRAW()                                                            \
    R.write(rd, sext(static_cast<int32_t>(bits(R[rs1], 31, 0)) >>              \
                         bits(R[rs2], 4, 0),                                   \
                     32))
#define EXEC_SLLIW() R.write(rd, sext(bits(R[rs1], 31, 0) << bits(imm, 4, 0), 32))
#define EXEC_SRLIW() R.write(rd, sext(bits(R[rs1], 31, 0) >> bits(imm, 4, 0), 32))
#define EXEC_SRAIW()                                                           \
    R.write(rd, sext(static_cast<int32_t>(bits(R[rs1], 31, 0)) >>              \
                         bits(imm, 4, 0),                                      \
                     32))

// Load
#define EXEC_LB()                                                              \
    do {                                                                       \
        uint8_t v = mmu->read<uint8_t>(pc, R[rs1] + imm);                      \
        R.write(rd, sext(v, 8));                                               \
    } while (0)

#define EXEC_LBU()                                                             \
    do {                                                                       \
        uint8_t v = mmu->read<uint8_t>(pc, R[rs1] + imm);                      \
        R.write(rd, v);                                                        \
    } while (0)

#define EXEC_LH()                                                              \
    do {                                                                       \
        uint16_t v = mmu->read<uint16_t>(pc, R[rs1] + imm);                    \
        R.write(rd, sext(v, 16));                                              \
    } while (0)

#define EXEC_LHU()                                                             \
    do {                                                                       \
        uint16_t v = mmu->read<uint16_t>(pc, R[rs1] + imm);                    \
        R.write(rd, v);                                                        \
    } while (0)

#define EXEC_LW()                                                              \
    do {                                                                       \
        uint32_t v = mmu->read<uint32_t>(pc, R[rs1] + imm);                    \
        R.write(rd, sext(v, 32));                                              \
    } while (0)

#define EXEC_LWU()                                                             \
    do {                                                                       \
        uint32_t v = mmu->read<uint32_t>(pc, R[rs1] + imm);                    \
        R.write(rd, v);                                                        \
    } while (0)

#define EXEC_LD()                                                              \
    do {                                                                       \
        uint64_t v = mmu->read<uint64_t>(pc, R[rs1] + imm);                    \
        R.write(rd, v);                                                        \
    } while (0)

// Store
#define EXEC_SB()  mmu->write<uint8_t>(pc, R[rs1] + imm, R[rs2])
#define EXEC_SH()  mmu->write<uint16_t>(pc, R[rs1] + imm, R[rs2])
#define EXEC_SW()  mmu->write<uint32_t>(pc, R[rs1] + imm, bits(R[rs2], 31, 0))
#define EXEC_SD()  mmu->write<uint64_t>(pc, R[rs1] + imm, R[rs2])

// Control: upper-immediate
#define EXEC_LUI()   R.write(rd, sext(bits(imm, 31, 12) << 12, 32))
#define EXEC_AUIPC() R.write(rd, pc + imm)

// Control: jump
#define EXEC_JAL()                                                             \
    do {                                                                       \
        addr_t npc = pc + imm;                                                 \
        R.write(rd, pc + 4);                                                   \
        hart->pc = npc;                                                        \
        goto L_insn_done;                                                          \
    } while (0)

#define EXEC_JALR()                                                            \
    do {                                                                       \
        uint64_t t = pc + 4;                                                   \
        hart->pc = (R[rs1] + imm) & ~1ULL;                                     \
        R.write(rd, t);                                                        \
        goto L_insn_done;                                                          \
    } while (0)

// Control: branch
#define EXEC_BEQ()                                                             \
    do {                                                                       \
        if (R[rs1] == R[rs2])                                                  \
            hart->pc = pc + imm;                                               \
    } while (0)

#define EXEC_BNE()                                                             \
    do {                                                                       \
        if (R[rs1] != R[rs2])                                                  \
            hart->pc = pc + imm;                                               \
    } while (0)

#define EXEC_BLT()                                                             \
    do {                                                                       \
        if (static_cast<int64_t>(R[rs1]) < static_cast<int64_t>(R[rs2]))       \
            hart->pc = pc + imm;                                               \
    } while (0)

#define EXEC_BGE()                                                             \
    do {                                                                       \
        if (static_cast<int64_t>(R[rs1]) >= static_cast<int64_t>(R[rs2]))      \
            hart->pc = pc + imm;                                               \
    } while (0)

#define EXEC_BLTU()                                                            \
    do {                                                                       \
        if (R[rs1] < R[rs2])                                                   \
            hart->pc = pc + imm;                                               \
    } while (0)

#define EXEC_BGEU()                                                            \
    do {                                                                       \
        if (R[rs1] >= R[rs2])                                                  \
            hart->pc = pc + imm;                                               \
    } while (0)

// Misc
#define EXEC_FENCE()   /* nop */
#define EXEC_FENCE_I() /* nop */

// === Zicsr Extension (CSR Instructions) ===
// (copy-paste from execute.cpp lines 204-243)

#define EXEC_CSRRC()                                                           \
    do {                                                                       \
        reg_t t = csrs[csr]->read_checked(pc, insn);                           \
        if (rs1)                                                               \
            csrs[csr]->write_checked(pc, insn, t & ~R[rs1]);                   \
        R.write(rd, t);                                                        \
    } while (0)

#define EXEC_CSRRCI()                                                          \
    do {                                                                       \
        uint64_t zimm = bits(insn, 19, 15);                                    \
        reg_t t = csrs[csr]->read_checked(pc, insn);                           \
        if (zimm)                                                              \
            csrs[csr]->write_checked(pc, insn, t & ~zimm);                     \
        R.write(rd, t);                                                        \
    } while (0)

#define EXEC_CSRRS()                                                           \
    do {                                                                       \
        uint64_t t = csrs[csr]->read_checked(pc, insn);                        \
        if (rs1)                                                               \
            csrs[csr]->write_checked(pc, insn, t | R[rs1]);                    \
        R.write(rd, t);                                                        \
    } while (0)

#define EXEC_CSRRSI()                                                          \
    do {                                                                       \
        uint64_t zimm = bits(insn, 19, 15);                                    \
        uint64_t t = csrs[csr]->read_checked(pc, insn);                        \
        if (zimm)                                                              \
            csrs[csr]->write_checked(pc, insn, t | zimm);                      \
        R.write(rd, t);                                                        \
    } while (0)

#define EXEC_CSRRW()                                                           \
    do {                                                                       \
        if (rd) {                                                              \
            uint64_t t = csrs[csr]->read_checked(pc, insn);                    \
            csrs[csr]->write_checked(pc, insn, R[rs1]);                        \
            R.write(rd, t);                                                    \
        } else {                                                               \
            csrs[csr]->write_checked(pc, insn, R[rs1]);                        \
        }                                                                      \
    } while (0)

#define EXEC_CSRRWI()                                                          \
    do {                                                                       \
        uint64_t zimm = bits(insn, 19, 15);                                    \
        if (rd)                                                                \
            R.write(rd, csrs[csr]->read_checked(pc, insn));                    \
        csrs[csr]->write_checked(pc, insn, zimm);                              \
    } while (0)

// === Privileged Instructions ===
// (copy-paste from execute.cpp lines 245-340)

#define EXEC_EBREAK() Trap::raise_exception(pc, TrapCause::Breakpoint, pc)

#define EXEC_ECALL()                                                           \
    do {                                                                       \
        switch (hart->priv) {                                                  \
            case PrivilegeLevel::M:                                            \
                Trap::raise_exception(pc, TrapCause::EnvironmentCallFromM, 0); \
                break;                                                         \
            case PrivilegeLevel::S:                                            \
                Trap::raise_exception(pc, TrapCause::EnvironmentCallFromS, 0); \
                break;                                                         \
            case PrivilegeLevel::U:                                            \
                Trap::raise_exception(pc, TrapCause::EnvironmentCallFromU, 0); \
                break;                                                         \
            default: std::unreachable();                                       \
        }                                                                      \
    } while (0)

#define EXEC_MRET()                                                            \
    do {                                                                       \
        if (hart->priv != PrivilegeLevel::M) [[unlikely]]                      \
            Trap::raise_exception(pc, TrapCause::IllegalInstruction, insn);    \
        reg_t mstatus = csrs[MSTATUS::ADDRESS]->read_unchecked();              \
        hart->pc = csrs[MEPC::ADDRESS]->read_unchecked();                      \
        hart->priv =                                                           \
            static_cast<PrivilegeLevel>((mstatus & MSTATUS::Field::MPP) >>     \
                                        MSTATUS::Shift::MPP_SHIFT);            \
        if (hart->priv != PrivilegeLevel::M)                                   \
            mstatus &= ~MSTATUS::Field::MPRV;                                  \
        if (mstatus & MSTATUS::Field::MPIE)                                    \
            mstatus |= MSTATUS::Field::MIE;                                    \
        else                                                                   \
            mstatus &= ~MSTATUS::Field::MIE;                                   \
        mstatus |= MSTATUS::Field::MPIE;                                       \
        mstatus &= ~MSTATUS::Field::MPP;                                       \
        csrs[MSTATUS::ADDRESS]->write_unchecked(mstatus);                      \
        goto L_insn_done;                                                          \
    } while (0)

#define EXEC_SRET()                                                            \
    do {                                                                       \
        if (hart->priv == PrivilegeLevel::U ||                                 \
            (hart->priv == PrivilegeLevel::S &&                                \
             (csrs[MSTATUS::ADDRESS]->read_unchecked() & MSTATUS::TSR)))       \
            [[unlikely]]                                                       \
            Trap::raise_exception(pc, TrapCause::IllegalInstruction, insn);    \
        reg_t sstatus = csrs[SSTATUS::ADDRESS]->read_unchecked();              \
        hart->pc = csrs[SEPC::ADDRESS]->read_unchecked();                      \
        hart->priv =                                                           \
            static_cast<PrivilegeLevel>((sstatus & SSTATUS::Field::SPP) >>     \
                                        SSTATUS::Shift::SPP_SHIFT);            \
        if (sstatus & SSTATUS::Field::SPIE)                                    \
            sstatus |= SSTATUS::Field::SIE;                                    \
        else                                                                   \
            sstatus &= ~SSTATUS::Field::SIE;                                   \
        sstatus |= SSTATUS::Field::SPIE;                                       \
        sstatus &= ~SSTATUS::Field::SPP;                                       \
        csrs[SSTATUS::ADDRESS]->write_unchecked(sstatus);                      \
        if (hart->priv != PrivilegeLevel::M)                                   \
            csrs[MSTATUS::ADDRESS]->write_unchecked(                           \
                csrs[MSTATUS::ADDRESS]->read_unchecked() &                     \
                ~MSTATUS::Field::MPRV);                                        \
        goto L_insn_done;                                                          \
    } while (0)

#define EXEC_SFENCE_VMA()                                                      \
    do {                                                                       \
        if (hart->priv == PrivilegeLevel::U ||                                 \
            (hart->priv == PrivilegeLevel::S &&                                \
             (csrs[MSTATUS::ADDRESS]->read_unchecked() & MSTATUS::TVM)))       \
            [[unlikely]]                                                       \
            Trap::raise_exception(pc, TrapCause::IllegalInstruction, insn);    \
        if (rs1 == 0)                                                          \
            mmu->tlb_flush_all();                                              \
        else                                                                   \
            mmu->tlb_flush_vaddr(R[rs1]);                                      \
    } while (0)

#define EXEC_WFI()                                                             \
    do {                                                                       \
        if (hart->priv == PrivilegeLevel::U ||                                 \
            (hart->priv < PrivilegeLevel::M &&                                 \
             (csrs[MSTATUS::ADDRESS]->read_unchecked() & MSTATUS::TW)))        \
            Trap::raise_exception(pc, TrapCause::IllegalInstruction, insn);    \
        if (hart->has_pending_enabled_interrupt())                             \
            break;                                                             \
        const reg_t mie = csrs[MIE::ADDRESS]->read_unchecked();                \
        if (mie == 0)                                                          \
            break;                                                             \
        throw WfiWait{};                                                       \
    } while (0)

// === RV64M Extension ===
// (copy-paste from execute.cpp lines 343-420)

#define EXEC_MUL()    R.write(rd, R[rs1] * R[rs2])
#define EXEC_MULH()   R.write(rd, (int64_t)(((__int128_t)(int64_t)R[rs1] * (__int128_t)(int64_t)R[rs2]) >> 64))
#define EXEC_MULHSU() R.write(rd, (int64_t)(((__int128_t)(int64_t)R[rs1] * (__uint128_t)R[rs2]) >> 64))
#define EXEC_MULHU()  R.write(rd, (uint64_t)((__uint128_t)R[rs1] * (__uint128_t)R[rs2] >> 64))
#define EXEC_MULW()   R.write(rd, sext(bits(R[rs1] * R[rs2], 31, 0), 32))

#define EXEC_DIV()                                                             \
    do {                                                                       \
        if (static_cast<int64_t>(R[rs2]) == 0) [[unlikely]]                    \
            R.write(rd, ~0ULL);                                                \
        else if (static_cast<int64_t>(R[rs1]) == INT64_MIN &&                  \
                 static_cast<int64_t>(R[rs2]) == -1) [[unlikely]]              \
            R.write(rd, static_cast<int64_t>(R[rs1]));                         \
        else                                                                   \
            R.write(rd, static_cast<int64_t>(R[rs1]) /                         \
                            static_cast<int64_t>(R[rs2]));                     \
    } while (0)

#define EXEC_DIVU()                                                            \
    do {                                                                       \
        if (R[rs2] == 0) [[unlikely]]                                          \
            R.write(rd, ~0ULL);                                                \
        else                                                                   \
            R.write(rd, R[rs1] / R[rs2]);                                      \
    } while (0)

#define EXEC_DIVW()                                                            \
    do {                                                                       \
        int32_t v1 = static_cast<int32_t>(bits(R[rs1], 31, 0));                \
        int32_t v2 = static_cast<int32_t>(bits(R[rs2], 31, 0));                \
        if (v2 == 0) [[unlikely]]                                              \
            R.write(rd, ~0ULL);                                                \
        else if (v1 == INT32_MIN && v2 == -1) [[unlikely]]                     \
            R.write(rd, sext(v1, 32));                                         \
        else                                                                   \
            R.write(rd, sext(v1 / v2, 32));                                    \
    } while (0)

#define EXEC_DIVUW()                                                           \
    do {                                                                       \
        uint32_t v1 = bits(R[rs1], 31, 0);                                     \
        uint32_t v2 = bits(R[rs2], 31, 0);                                     \
        if (v2 == 0) [[unlikely]]                                              \
            R.write(rd, ~0ULL);                                                \
        else                                                                   \
            R.write(rd, sext(v1 / v2, 32));                                    \
    } while (0)

#define EXEC_REM()                                                             \
    do {                                                                       \
        if (static_cast<int64_t>(R[rs2]) == 0) [[unlikely]]                    \
            R.write(rd, (int64_t)R[rs1]);                                      \
        else if (static_cast<int64_t>(R[rs1]) == INT64_MIN &&                  \
                 static_cast<int64_t>(R[rs2]) == -1) [[unlikely]]              \
            R.write(rd, 0);                                                    \
        else                                                                   \
            R.write(rd, static_cast<int64_t>(R[rs1]) %                         \
                            static_cast<int64_t>(R[rs2]));                     \
    } while (0)

#define EXEC_REMU()                                                            \
    do {                                                                       \
        if (R[rs2] == 0) [[unlikely]]                                          \
            R.write(rd, R[rs1]);                                               \
        else                                                                   \
            R.write(rd, R[rs1] % R[rs2]);                                      \
    } while (0)

#define EXEC_REMW()                                                            \
    do {                                                                       \
        int32_t v1 = (int32_t)bits(R[rs1], 31, 0);                             \
        int32_t v2 = (int32_t)bits(R[rs2], 31, 0);                             \
        if (v2 == 0) [[unlikely]]                                              \
            R.write(rd, sext(v1, 32));                                         \
        else if (v1 == INT32_MIN && v2 == -1) [[unlikely]]                     \
            R.write(rd, 0);                                                    \
        else                                                                   \
            R.write(rd, sext(v1 % v2, 32));                                    \
    } while (0)

#define EXEC_REMUW()                                                           \
    do {                                                                       \
        uint32_t v1 = bits(R[rs1], 31, 0);                                     \
        uint32_t v2 = bits(R[rs2], 31, 0);                                     \
        if (v2 == 0) [[unlikely]]                                              \
            R.write(rd, sext(v1, 32));                                         \
        else                                                                   \
            R.write(rd, sext(v1 % v2, 32));                                    \
    } while (0)

// === RV64A Extension ===
// (copy-paste from execute.cpp lines 428-638)

#define EXEC_LR_D()                                                            \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b111) != 0) [[unlikely]]                                  \
            Trap::raise_exception(pc, TrapCause::LoadAccessFault, addr);       \
        uint64_t v = mmu->read<uint64_t>(pc, addr);                            \
        R.write(rd, v);                                                        \
        mmu->reservation_address = addr;                                       \
        mmu->reservation_valid = true;                                         \
    } while (0)

#define EXEC_LR_W()                                                            \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b11) != 0) [[unlikely]]                                   \
            Trap::raise_exception(pc, TrapCause::LoadAccessFault, addr);       \
        uint32_t v = mmu->read<uint32_t>(pc, addr);                            \
        R.write(rd, sext(v, 32));                                              \
        mmu->reservation_address = addr;                                       \
        mmu->reservation_valid = true;                                         \
    } while (0)

#define EXEC_SC_D()                                                            \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b111) != 0) [[unlikely]]                                  \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        mmu->probe_store<uint64_t>(pc, addr);                                  \
        if (mmu->reservation_valid && mmu->reservation_address == addr) {      \
            mmu->write<uint64_t>(pc, addr, R[rs2]);                            \
            R.write(rd, 0);                                                    \
        } else {                                                               \
            R.write(rd, 1);                                                    \
        }                                                                      \
        mmu->reservation_valid = false;                                        \
    } while (0)

#define EXEC_SC_W()                                                            \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b11) != 0) [[unlikely]]                                   \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        mmu->probe_store<uint32_t>(pc, addr);                                  \
        if (mmu->reservation_valid && mmu->reservation_address == addr) {      \
            mmu->write<uint32_t>(pc, addr, R[rs2]);                            \
            R.write(rd, 0);                                                    \
        } else {                                                               \
            R.write(rd, 1);                                                    \
        }                                                                      \
        mmu->reservation_valid = false;                                        \
    } while (0)

// AMO double-word macros
#define EXEC_AMOADD_D()                                                        \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b111) != 0) [[unlikely]]                                  \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint64_t t = mmu->read<uint64_t>(pc, addr, true);                      \
        mmu->write<uint64_t>(                                                  \
            pc, addr,                                                          \
            static_cast<int64_t>(t) + static_cast<int64_t>(R[rs2]));           \
        R.write(rd, t);                                                        \
    } while (0)

#define EXEC_AMOAND_D()                                                        \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b111) != 0) [[unlikely]]                                  \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint64_t t = mmu->read<uint64_t>(pc, addr, true);                      \
        mmu->write<uint64_t>(                                                  \
            pc, addr,                                                          \
            static_cast<int64_t>(t) & static_cast<int64_t>(R[rs2]));           \
        R.write(rd, t);                                                        \
    } while (0)

#define EXEC_AMOOR_D()                                                         \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b111) != 0) [[unlikely]]                                  \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint64_t t = mmu->read<uint64_t>(pc, addr, true);                      \
        mmu->write<uint64_t>(                                                  \
            pc, addr,                                                          \
            static_cast<int64_t>(t) | static_cast<int64_t>(R[rs2]));           \
        R.write(rd, t);                                                        \
    } while (0)

#define EXEC_AMOXOR_D()                                                        \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b111) != 0) [[unlikely]]                                  \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint64_t t = mmu->read<uint64_t>(pc, addr, true);                      \
        mmu->write<uint64_t>(                                                  \
            pc, addr,                                                          \
            static_cast<int64_t>(t) ^ static_cast<int64_t>(R[rs2]));           \
        R.write(rd, t);                                                        \
    } while (0)

#define EXEC_AMOMAX_D()                                                        \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b111) != 0) [[unlikely]]                                  \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint64_t t = mmu->read<uint64_t>(pc, addr, true);                      \
        mmu->write<uint64_t>(                                                  \
            pc, addr,                                                          \
            std::max(static_cast<int64_t>(t), static_cast<int64_t>(R[rs2])));  \
        R.write(rd, t);                                                        \
    } while (0)

#define EXEC_AMOMAXU_D()                                                       \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b111) != 0) [[unlikely]]                                  \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint64_t t = mmu->read<uint64_t>(pc, addr, true);                      \
        mmu->write<uint64_t>(pc, addr, std::max(t, R[rs2]));                   \
        R.write(rd, t);                                                        \
    } while (0)

#define EXEC_AMOMIN_D()                                                        \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b111) != 0) [[unlikely]]                                  \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint64_t t = mmu->read<uint64_t>(pc, addr, true);                      \
        mmu->write<uint64_t>(                                                  \
            pc, addr,                                                          \
            std::min(static_cast<int64_t>(t), static_cast<int64_t>(R[rs2])));  \
        R.write(rd, t);                                                        \
    } while (0)

#define EXEC_AMOMINU_D()                                                       \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b111) != 0) [[unlikely]]                                  \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint64_t t = mmu->read<uint64_t>(pc, addr, true);                      \
        mmu->write<uint64_t>(pc, addr, std::min(t, R[rs2]));                   \
        R.write(rd, t);                                                        \
    } while (0)

#define EXEC_AMOSWAP_D()                                                       \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b111) != 0) [[unlikely]]                                  \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint64_t t = mmu->read<uint64_t>(pc, addr, true);                      \
        mmu->write<uint64_t>(pc, addr, R[rs2]);                                \
        R.write(rd, t);                                                        \
    } while (0)

// AMO word macros
#define EXEC_AMOADD_W()                                                        \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b11) != 0) [[unlikely]]                                   \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint32_t t = mmu->read<uint32_t>(pc, addr, true);                      \
        mmu->write<uint32_t>(                                                  \
            pc, addr,                                                          \
            static_cast<int32_t>(t) + static_cast<int32_t>(R[rs2]));           \
        R.write(rd, sext(t, 32));                                              \
    } while (0)

#define EXEC_AMOAND_W()                                                        \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b11) != 0) [[unlikely]]                                   \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint32_t t = mmu->read<uint32_t>(pc, addr, true);                      \
        mmu->write<uint32_t>(                                                  \
            pc, addr,                                                          \
            static_cast<int32_t>(t) & static_cast<int32_t>(R[rs2]));           \
        R.write(rd, sext(t, 32));                                              \
    } while (0)

#define EXEC_AMOOR_W()                                                         \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b11) != 0) [[unlikely]]                                   \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint32_t t = mmu->read<uint32_t>(pc, addr, true);                      \
        mmu->write<uint32_t>(                                                  \
            pc, addr,                                                          \
            static_cast<int32_t>(t) | static_cast<int32_t>(R[rs2]));           \
        R.write(rd, sext(t, 32));                                              \
    } while (0)

#define EXEC_AMOXOR_W()                                                        \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b11) != 0) [[unlikely]]                                   \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint32_t t = mmu->read<uint32_t>(pc, addr, true);                      \
        mmu->write<uint32_t>(                                                  \
            pc, addr,                                                          \
            static_cast<int32_t>(t) ^ static_cast<int32_t>(R[rs2]));           \
        R.write(rd, sext(t, 32));                                              \
    } while (0)

#define EXEC_AMOMAX_W()                                                        \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b11) != 0) [[unlikely]]                                   \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint32_t t = mmu->read<uint32_t>(pc, addr, true);                      \
        mmu->write<uint32_t>(                                                  \
            pc, addr,                                                          \
            std::max(static_cast<int32_t>(t), static_cast<int32_t>(R[rs2])));  \
        R.write(rd, sext(t, 32));                                              \
    } while (0)

#define EXEC_AMOMAXU_W()                                                       \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b11) != 0) [[unlikely]]                                   \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint32_t t = mmu->read<uint32_t>(pc, addr, true);                      \
        mmu->write<uint32_t>(                                                  \
            pc, addr,                                                          \
            std::max(static_cast<uint32_t>(t),                                 \
                     static_cast<uint32_t>(R[rs2])));                          \
        R.write(rd, sext(t, 32));                                              \
    } while (0)

#define EXEC_AMOMIN_W()                                                        \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b11) != 0) [[unlikely]]                                   \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint32_t t = mmu->read<uint32_t>(pc, addr, true);                      \
        mmu->write<uint32_t>(                                                  \
            pc, addr,                                                          \
            std::min(static_cast<int32_t>(t), static_cast<int32_t>(R[rs2])));  \
        R.write(rd, sext(t, 32));                                              \
    } while (0)

#define EXEC_AMOMINU_W()                                                       \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b11) != 0) [[unlikely]]                                   \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint32_t t = mmu->read<uint32_t>(pc, addr, true);                      \
        mmu->write<uint32_t>(                                                  \
            pc, addr,                                                          \
            std::min(static_cast<uint32_t>(t),                                 \
                     static_cast<uint32_t>(R[rs2])));                          \
        R.write(rd, sext(t, 32));                                              \
    } while (0)

#define EXEC_AMOSWAP_W()                                                       \
    do {                                                                       \
        const addr_t addr = R[rs1];                                            \
        if ((addr & 0b11) != 0) [[unlikely]]                                   \
            Trap::raise_exception(pc, TrapCause::StoreAMOAccessFault, addr);   \
        uint32_t t = mmu->read<uint32_t>(pc, addr, true);                      \
        mmu->write<uint32_t>(pc, addr, R[rs2]);                                \
        R.write(rd, sext(t, 32));                                              \
    } while (0)

// === RV64F Extension (Single-Precision FP) ===
// (copy-paste from execute.cpp lines 641-857)

#define EXEC_FLW()                                                             \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        uint32_t v = mmu->read<uint32_t>(pc, R[rs1] + imm);                    \
        F[rd] = float32_t{v};                                                  \
        FP_SET_DIRTY();                                                        \
    } while (0)

#define EXEC_FSW()                                                             \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        mmu->write<uint32_t>(pc, R[rs1] + imm,                                 \
                             static_cast<uint32_t>(F[rs2].read_64().v));       \
    } while (0)

#define EXEC_FADD_S()                                                          \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f32_add(F[rs1].read_32(), F[rs2].read_32());                   \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FSUB_S()                                                          \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f32_sub(F[rs1].read_32(), F[rs2].read_32());                   \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FMUL_S()                                                          \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f32_mul(F[rs1].read_32(), F[rs2].read_32());                   \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FDIV_S()                                                          \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f32_div(F[rs1].read_32(), F[rs2].read_32());                   \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FSQRT_S()                                                         \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f32_sqrt(F[rs1].read_32());                                    \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FSGNJ_S()                                                         \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        float32_t f1 = F[rs1].read_32();                                       \
        float32_t f2 = F[rs2].read_32();                                       \
        F[rd] = float32_t{(f1.v & ~F32_SIGN) | (f2.v & F32_SIGN)};            \
        FP_SET_DIRTY();                                                    \
    } while (0)

#define EXEC_FSGNJN_S()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        float32_t f1 = F[rs1].read_32();                                       \
        float32_t f2 = F[rs2].read_32();                                       \
        F[rd] = float32_t{(f1.v & ~F32_SIGN) | (~f2.v & F32_SIGN)};           \
        FP_SET_DIRTY();                                                    \
    } while (0)

#define EXEC_FSGNJX_S()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        float32_t f1 = F[rs1].read_32();                                       \
        float32_t f2 = F[rs2].read_32();                                       \
        F[rd] = float32_t{f1.v ^ (f2.v & F32_SIGN)};                           \
        FP_SET_DIRTY();                                                    \
    } while (0)

#define EXEC_FMIN_S()                                                          \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        float32_t f1 = F[rs1].read_32();                                       \
        float32_t f2 = F[rs2].read_32();                                       \
        if (f32_isSignalingNaN(f1) || f32_isSignalingNaN(f2)) {                \
            CSR* fflags = csrs[FFLAGS::ADDRESS].get();                         \
            reg_t v = fflags->read_unchecked();                                \
            fflags->write_unchecked(v | FFLAGS::Field::NV);                    \
        }                                                                      \
        bool smaller = f32_lt_quiet(f1, f2) ||                                 \
                       (f32_eq(f1, f2) && f32_isNegative(f1));                 \
        if (f32_isNaN(f1) && f32_isNaN(f2)) {                                  \
            F[rd] = float32_t{F32_DEFAULT_NAN};                                \
        } else {                                                               \
            if (smaller || f32_isNaN(f2))                                      \
                F[rd] = f1;                                                    \
            else                                                               \
                F[rd] = f2;                                                    \
        }                                                                      \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FMAX_S()                                                          \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        float32_t f1 = F[rs1].read_32();                                       \
        float32_t f2 = F[rs2].read_32();                                       \
        if (f32_isSignalingNaN(f1) || f32_isSignalingNaN(f2)) {                \
            CSR* fflags = csrs[FFLAGS::ADDRESS].get();                         \
            reg_t v = fflags->read_unchecked();                                \
            fflags->write_unchecked(v | FFLAGS::Field::NV);                    \
        }                                                                      \
        bool greater = f32_lt_quiet(f2, f1) ||                                 \
                       (f32_eq(f2, f1) && f32_isNegative(f2));                 \
        if (f32_isNaN(f1) && f32_isNaN(f2)) {                                  \
            F[rd] = float32_t{F32_DEFAULT_NAN};                                \
        } else {                                                               \
            if (greater || f32_isNaN(f2))                                      \
                F[rd] = f1;                                                    \
            else                                                               \
                F[rd] = f2;                                                    \
        }                                                                      \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FCVT_W_S()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        R.write(rd,                                                            \
                static_cast<int64_t>(f32_to_i32(F[rs1].read_32(),              \
                                                softfloat_roundingMode, true))); \
        FP_UPDATE_FLAGS();                                       \
    } while (0)

#define EXEC_FCVT_WU_S()                                                       \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        R.write(rd,                                                            \
                static_cast<int64_t>(static_cast<int32_t>(                     \
                    f32_to_ui32(F[rs1].read_32(),                              \
                                softfloat_roundingMode, true))));              \
        FP_UPDATE_FLAGS();                                       \
    } while (0)

#define EXEC_FCVT_L_S()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        R.write(rd,                                                            \
                f32_to_i64(F[rs1].read_32(), softfloat_roundingMode, true));   \
        FP_UPDATE_FLAGS();                                       \
    } while (0)

#define EXEC_FCVT_LU_S()                                                       \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        R.write(rd, f32_to_ui64(F[rs1].read_32(),                              \
                                softfloat_roundingMode, true));                \
        FP_UPDATE_FLAGS();                                       \
    } while (0)

#define EXEC_FCVT_S_W()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = i32_to_f32(static_cast<int32_t>(R[rs1]));                      \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FCVT_S_WU()                                                       \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = ui32_to_f32(static_cast<uint32_t>(R[rs1]));                    \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FCVT_S_L()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = i64_to_f32(R[rs1]);                                            \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FCVT_S_LU()                                                       \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = ui64_to_f32(R[rs1]);                                           \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FMV_X_W()                                                         \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        R.write(rd,                                                            \
                static_cast<int64_t>(static_cast<int32_t>(                     \
                    static_cast<uint32_t>(F[rs1].read_64().v))));              \
    } while (0)

#define EXEC_FMV_W_X()                                                         \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        F[rd] =                                                                \
            float32_t{static_cast<uint32_t>(static_cast<int32_t>(R[rs1]))};    \
        FP_SET_DIRTY();                                                    \
    } while (0)

#define EXEC_FCLASS_S()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        R.write(rd,                                                            \
                static_cast<int64_t>(                                          \
                    static_cast<int32_t>(f32_classify(F[rs1].read_32()))));    \
    } while (0)

#define EXEC_FEQ_S()                                                           \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        R.write(rd, f32_eq(F[rs1].read_32(), F[rs2].read_32()));               \
        FP_UPDATE_FLAGS();                                       \
    } while (0)

#define EXEC_FLT_S()                                                           \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        R.write(rd, f32_lt(F[rs1].read_32(), F[rs2].read_32()));               \
        FP_UPDATE_FLAGS();                                       \
    } while (0)

#define EXEC_FLE_S()                                                           \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        R.write(rd, f32_le(F[rs1].read_32(), F[rs2].read_32()));               \
        FP_UPDATE_FLAGS();                                       \
    } while (0)

#define EXEC_FMADD_S()                                                         \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f32_mulAdd(F[rs1].read_32(), F[rs2].read_32(),                 \
                           F[rs3].read_32());                                  \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FMSUB_S()                                                         \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f32_mulAdd(F[rs1].read_32(), F[rs2].read_32(),                 \
                           f32_neg(F[rs3].read_32()));                         \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FNMSUB_S()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f32_mulAdd(f32_neg(F[rs1].read_32()), F[rs2].read_32(),        \
                           F[rs3].read_32());                                  \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FNMADD_S()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f32_mulAdd(f32_neg(F[rs1].read_32()), F[rs2].read_32(),        \
                           f32_neg(F[rs3].read_32()));                         \
        FP_INST_END();                                                         \
    } while (0)

// === RV64D Extension (Double-Precision FP) ===
// (copy-paste from execute.cpp lines 860-1085)

#define EXEC_FLD()                                                             \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        uint64_t v = mmu->read<uint64_t>(pc, R[rs1] + imm);                    \
        F[rd] = float64_t{v};                                                  \
        FP_SET_DIRTY();                                                    \
    } while (0)

#define EXEC_FSD()                                                             \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        mmu->write<uint64_t>(pc, R[rs1] + imm, F[rs2].read_64().v);            \
    } while (0)

#define EXEC_FADD_D()                                                          \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f64_add(F[rs1].read_64(), F[rs2].read_64());                   \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FSUB_D()                                                          \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f64_sub(F[rs1].read_64(), F[rs2].read_64());                   \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FMUL_D()                                                          \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f64_mul(F[rs1].read_64(), F[rs2].read_64());                   \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FDIV_D()                                                          \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f64_div(F[rs1].read_64(), F[rs2].read_64());                   \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FSQRT_D()                                                         \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f64_sqrt(F[rs1].read_64());                                    \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FSGNJ_D()                                                         \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        float64_t f1 = F[rs1].read_64();                                       \
        float64_t f2 = F[rs2].read_64();                                       \
        F[rd] = float64_t{(f1.v & ~F64_SIGN) | (f2.v & F64_SIGN)};            \
        FP_SET_DIRTY();                                                    \
    } while (0)

#define EXEC_FSGNJN_D()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        float64_t f1 = F[rs1].read_64();                                       \
        float64_t f2 = F[rs2].read_64();                                       \
        F[rd] = float64_t{(f1.v & ~F64_SIGN) | (~f2.v & F64_SIGN)};           \
        FP_SET_DIRTY();                                                    \
    } while (0)

#define EXEC_FSGNJX_D()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        float64_t f1 = F[rs1].read_64();                                       \
        float64_t f2 = F[rs2].read_64();                                       \
        F[rd] = float64_t{f1.v ^ (f2.v & F64_SIGN)};                           \
        FP_SET_DIRTY();                                                    \
    } while (0)

#define EXEC_FMIN_D()                                                          \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        float64_t f1 = F[rs1].read_64();                                       \
        float64_t f2 = F[rs2].read_64();                                       \
        if (f64_isSignalingNaN(f1) || f64_isSignalingNaN(f2)) {                \
            CSR* fflags = csrs[FFLAGS::ADDRESS].get();                         \
            reg_t v = fflags->read_unchecked();                                \
            fflags->write_unchecked(v | FFLAGS::Field::NV);                    \
        }                                                                      \
        bool smaller = f64_lt_quiet(f1, f2) ||                                 \
                       (f64_eq(f1, f2) && f64_isNegative(f1));                 \
        if (f64_isNaN(f1) && f64_isNaN(f2)) {                                  \
            F[rd] = float64_t{F64_DEFAULT_NAN};                                \
        } else {                                                               \
            if (smaller || f64_isNaN(f2))                                      \
                F[rd] = f1;                                                    \
            else                                                               \
                F[rd] = f2;                                                    \
        }                                                                      \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FMAX_D()                                                          \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        float64_t f1 = F[rs1].read_64();                                       \
        float64_t f2 = F[rs2].read_64();                                       \
        if (f64_isSignalingNaN(f1) || f64_isSignalingNaN(f2)) {                \
            CSR* fflags = csrs[FFLAGS::ADDRESS].get();                         \
            reg_t v = fflags->read_unchecked();                                \
            fflags->write_unchecked(v | FFLAGS::Field::NV);                    \
        }                                                                      \
        bool greater = f64_lt_quiet(f2, f1) ||                                 \
                       (f64_eq(f2, f1) && f64_isNegative(f2));                 \
        if (f64_isNaN(f1) && f64_isNaN(f2)) {                                  \
            F[rd] = float64_t{F64_DEFAULT_NAN};                                \
        } else {                                                               \
            if (greater || f64_isNaN(f2))                                      \
                F[rd] = f1;                                                    \
            else                                                               \
                F[rd] = f2;                                                    \
        }                                                                      \
        FP_INST_END();                                                         \
    } while (0)

// FCVT double
#define EXEC_FCVT_W_D()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        R.write(rd,                                                            \
                static_cast<int64_t>(f64_to_i32(F[rs1].read_64(),              \
                                                softfloat_roundingMode, true))); \
        FP_UPDATE_FLAGS();                                       \
    } while (0)

#define EXEC_FCVT_WU_D()                                                       \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        R.write(rd,                                                            \
                static_cast<int64_t>(static_cast<int32_t>(                     \
                    f64_to_ui32(F[rs1].read_64(),                              \
                                softfloat_roundingMode, true))));              \
        FP_UPDATE_FLAGS();                                       \
    } while (0)

#define EXEC_FCVT_L_D()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        R.write(rd,                                                            \
                f64_to_i64(F[rs1].read_64(), softfloat_roundingMode, true));   \
        FP_UPDATE_FLAGS();                                       \
    } while (0)

#define EXEC_FCVT_LU_D()                                                       \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        R.write(rd, f64_to_ui64(F[rs1].read_64(),                              \
                                softfloat_roundingMode, true));                \
        FP_UPDATE_FLAGS();                                       \
    } while (0)

#define EXEC_FCVT_D_W()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = i32_to_f64(static_cast<int32_t>(R[rs1]));                      \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FCVT_D_WU()                                                       \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = ui32_to_f64(static_cast<uint32_t>(R[rs1]));                    \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FCVT_D_L()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = i64_to_f64(R[rs1]);                                            \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FCVT_D_LU()                                                       \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = ui64_to_f64(R[rs1]);                                           \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FCVT_S_D()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f64_to_f32(F[rs1].read_64());                                  \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FCVT_D_S()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f32_to_f64(F[rs1].read_32());                                  \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FMV_X_D()                                                         \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        R.write(rd, F[rs1].read_64().v);                                       \
    } while (0)

#define EXEC_FMV_D_X()                                                         \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        F[rd] = float64_t{R[rs1]};                                             \
        FP_SET_DIRTY();                                                    \
    } while (0)

#define EXEC_FCLASS_D()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        R.write(rd,                                                            \
                static_cast<int64_t>(f64_classify(F[rs1].read_64())));         \
    } while (0)

#define EXEC_FEQ_D()                                                           \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        R.write(rd, f64_eq(F[rs1].read_64(), F[rs2].read_64()));               \
        FP_UPDATE_FLAGS();                                       \
    } while (0)

#define EXEC_FLT_D()                                                           \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        R.write(rd, f64_lt(F[rs1].read_64(), F[rs2].read_64()));               \
        FP_UPDATE_FLAGS();                                       \
    } while (0)

#define EXEC_FLE_D()                                                           \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        R.write(rd, f64_le(F[rs1].read_64(), F[rs2].read_64()));               \
        FP_UPDATE_FLAGS();                                       \
    } while (0)

#define EXEC_FMADD_D()                                                         \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f64_mulAdd(F[rs1].read_64(), F[rs2].read_64(),                 \
                           F[rs3].read_64());                                  \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FMSUB_D()                                                         \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f64_mulAdd(F[rs1].read_64(), F[rs2].read_64(),                 \
                           f64_neg(F[rs3].read_64()));                         \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FNMSUB_D()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f64_mulAdd(f64_neg(F[rs1].read_64()), F[rs2].read_64(),        \
                           F[rs3].read_64());                                  \
        FP_INST_END();                                                         \
    } while (0)

#define EXEC_FNMADD_D()                                                        \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        FP_SETUP_RM();                                                         \
        F[rd] = f64_mulAdd(f64_neg(F[rs1].read_64()), F[rs2].read_64(),        \
                           f64_neg(F[rs3].read_64()));                         \
        FP_INST_END();                                                         \
    } while (0)

// === RV64C Extension (Compressed Instructions) ===
// (copy-paste from execute.cpp lines 1088-1197)

#define EXEC_C_NOP()    /* nop */

#define EXEC_C_ADDI()   R.write(rd, R[rd] + imm)

#define EXEC_C_ADDIW()  R.write(rd, sext(bits(R[rd] + imm, 31, 0), 32))

#define EXEC_C_LI()     R.write(rd, imm)

#define EXEC_C_ADDI16SP()                                                      \
    do {                                                                       \
        if (imm == 0) [[unlikely]]                                             \
            Trap::raise_exception(pc, TrapCause::IllegalInstruction, insn);    \
        else                                                                   \
            R.write(2, R[2] + imm);                                            \
    } while (0)

#define EXEC_C_LUI()                                                           \
    do {                                                                       \
        if (rd == 2 || imm == 0) [[unlikely]]                                  \
            Trap::raise_exception(pc, TrapCause::IllegalInstruction, insn);    \
        else                                                                   \
            R.write(rd, imm);                                                  \
    } while (0)

#define EXEC_C_SRLI()   R.write(rd, R[rd] >> imm)

#define EXEC_C_SRAI()   R.write(rd, static_cast<int64_t>(R[rd]) >> imm)

#define EXEC_C_ANDI()   R.write(rd, R[rd] & imm)

#define EXEC_C_SUB()    R.write(rd, R[rd] - R[rs2])

#define EXEC_C_XOR()    R.write(rd, R[rd] ^ R[rs2])

#define EXEC_C_OR()     R.write(rd, R[rd] | R[rs2])

#define EXEC_C_AND()    R.write(rd, R[rd] & R[rs2])

#define EXEC_C_SUBW()   R.write(rd, sext(bits(R[rd] - R[rs2], 31, 0), 32))

#define EXEC_C_ADDW()   R.write(rd, sext(bits(R[rd] + R[rs2], 31, 0), 32))

#define EXEC_C_J()                                                             \
    do {                                                                       \
        hart->pc = pc + imm;                                                   \
        goto L_insn_done;                                                          \
    } while (0)

#define EXEC_C_BEQZ()                                                          \
    do {                                                                       \
        if (R[rs1] == 0)                                                       \
            hart->pc = pc + imm;                                               \
    } while (0)

#define EXEC_C_BNEZ()                                                          \
    do {                                                                       \
        if (R[rs1] != 0)                                                       \
            hart->pc = pc + imm;                                               \
    } while (0)

#define EXEC_C_ADDI4SPN()                                                      \
    do {                                                                       \
        if (imm == 0) [[unlikely]]                                             \
            Trap::raise_exception(pc, TrapCause::IllegalInstruction, insn);    \
        else                                                                   \
            R.write(rd, R[2] + imm);                                           \
    } while (0)

#define EXEC_C_FLD()                                                           \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        uint64_t v = mmu->read<uint64_t>(pc, R[rs1] + imm);                    \
        F[rd] = float64_t{v};                                                  \
    } while (0)

#define EXEC_C_LW()                                                            \
    do {                                                                       \
        uint32_t v = mmu->read<uint32_t>(pc, R[rs1] + imm);                    \
        R.write(rd, sext(v, 32));                                              \
    } while (0)

#define EXEC_C_LD()                                                            \
    do {                                                                       \
        uint64_t v = mmu->read<uint64_t>(pc, R[rs1] + imm);                    \
        R.write(rd, v);                                                        \
    } while (0)

#define EXEC_C_FSD()                                                           \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        mmu->write<uint64_t>(pc, R[rs1] + imm, F[rs2].read_64().v);            \
    } while (0)

#define EXEC_C_SW()     mmu->write<uint32_t>(pc, R[rs1] + imm, bits(R[rs2], 31, 0))

#define EXEC_C_SD()     mmu->write<uint64_t>(pc, R[rs1] + imm, R[rs2])

#define EXEC_C_SLLI()   R.write(rd, R[rd] << imm)

#define EXEC_C_FLDSP()                                                         \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        uint64_t v = mmu->read<uint64_t>(pc, R[2] + imm);                      \
        F[rd] = float64_t{v};                                                  \
    } while (0)

#define EXEC_C_LWSP()                                                          \
    do {                                                                       \
        if (rd == 0) [[unlikely]]                                              \
            Trap::raise_exception(pc, TrapCause::IllegalInstruction, insn);    \
        else {                                                                 \
            uint32_t v = mmu->read<uint32_t>(pc, R[2] + imm);                  \
            R.write(rd, sext(v, 32));                                          \
        }                                                                      \
    } while (0)

#define EXEC_C_LDSP()                                                          \
    do {                                                                       \
        if (rd == 0) [[unlikely]]                                              \
            Trap::raise_exception(pc, TrapCause::IllegalInstruction, insn);    \
        else {                                                                 \
            uint64_t v = mmu->read<uint64_t>(pc, R[2] + imm);                  \
            R.write(rd, v);                                                    \
        }                                                                      \
    } while (0)

#define EXEC_C_JR()                                                            \
    do {                                                                       \
        if (rs1 == 0) [[unlikely]]                                             \
            Trap::raise_exception(pc, TrapCause::IllegalInstruction, insn);    \
        else                                                                   \
            hart->pc = R[rs1] & ~1ULL;                                         \
        goto L_insn_done;                                                          \
    } while (0)

#define EXEC_C_MV()                                                            \
    do {                                                                       \
        if (rs2 == 0) [[unlikely]]                                             \
            Trap::raise_exception(pc, TrapCause::IllegalInstruction, insn);    \
        else                                                                   \
            R.write(rd, R[rs2]);                                               \
    } while (0)

#define EXEC_C_EBREAK() Trap::raise_exception(pc, TrapCause::Breakpoint, pc)

#define EXEC_C_JALR()                                                          \
    do {                                                                       \
        if (rs1 == 0) [[unlikely]]                                             \
            Trap::raise_exception(pc, TrapCause::IllegalInstruction, insn);    \
        else {                                                                 \
            uint64_t t = pc + 2;                                               \
            hart->pc = R[rs1] & ~1ULL;                                         \
            R.write(1, t);                                                     \
            goto L_insn_done;                                                      \
        }                                                                      \
    } while (0)

#define EXEC_C_ADD()    R.write(rd, R[rd] + R[rs2])

#define EXEC_C_FSDSP()                                                         \
    do {                                                                       \
        FP_INST_PREP();                                                        \
        mmu->write<uint64_t>(pc, R[2] + imm, F[rs2].read_64().v);             \
    } while (0)

#define EXEC_C_SWSP()   mmu->write<uint32_t>(pc, R[2] + imm, bits(R[rs2], 31, 0))

#define EXEC_C_SDSP()   mmu->write<uint64_t>(pc, R[2] + imm, R[rs2])
