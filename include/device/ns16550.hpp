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
 *
 * ---
 * This file is part of uemu-ng and contains code derived from the Spike
 * (riscv-isa-sim) project.
 *
 * Spike is Copyright (c) 2010-2017, The Regents of the University of
 * California. Spike's original code is licensed under the BSD 3-Clause License.
 * Full Spike license can be found at:
 * https://opensource.org/licenses/BSD-3-Clause
 */

#include <memory>
#include <mutex>
#include <queue>

#include "device/device.hpp"
#include "host/console.hpp"

#pragma once

namespace uemu::device {

class NS16550 : public IrqDevice {
public:
    static constexpr addr_t DEFAULT_BASE = 0x10000000;
    static constexpr size_t SIZE = 0x100;
    static constexpr uint32_t DEFAULT_INTERRUPT_ID = 10;

    static constexpr uint32_t DEFAULT_REG_SHIFT = 0;
    static constexpr uint32_t DEFAULT_REG_IO_WIDTH = 1;

    static constexpr size_t QUEUE_SIZE = 64;

    static constexpr uint8_t RX = 0;  // Receive buffer (R)
    static constexpr uint8_t TX = 0;  // Transmit buffer (W)
    static constexpr uint8_t IER = 1; // Interrupt Enable Register
    static constexpr uint8_t IIR = 2; // Interrupt ID Register (R)
    static constexpr uint8_t FCR = 2; // FIFO Control Register (W)
    static constexpr uint8_t LCR = 3; // Line Control Register
    static constexpr uint8_t MCR = 4; // Modem Control Register
    static constexpr uint8_t LSR = 5; // Line Status Register
    static constexpr uint8_t MSR = 6; // Modem Status Register
    static constexpr uint8_t SCR = 7; // Scratch Register

    // IER bits
    static constexpr uint8_t IER_MSI = 0x08;  // Modem status interrupt
    static constexpr uint8_t IER_RLSI = 0x04; // Receiver line status interrupt
    static constexpr uint8_t IER_THRI =
        0x02; // Transmitter holding register interrupt
    static constexpr uint8_t IER_RDI = 0x01; // Receiver data interrupt

    // IIR bits
    static constexpr uint8_t IIR_NO_INT = 0x01; // No interrupts pending
    static constexpr uint8_t IIR_ID = 0x0e;     // Interrupt ID mask
    static constexpr uint8_t IIR_MSI = 0x00;    // Modem status interrupt
    static constexpr uint8_t IIR_THRI =
        0x02; // Transmitter holding register empty
    static constexpr uint8_t IIR_RDI = 0x04;  // Receiver data interrupt
    static constexpr uint8_t IIR_RLSI = 0x06; // Receiver line status interrupt
    static constexpr uint8_t IIR_TYPE_BITS = 0xc0;

    // FCR bits
    static constexpr uint8_t FCR_ENABLE_FIFO = 0x01; // Enable FIFO
    static constexpr uint8_t FCR_CLEAR_RCVR = 0x02;  // Clear receive FIFO
    static constexpr uint8_t FCR_CLEAR_XMIT = 0x04;  // Clear transmit FIFO
    static constexpr uint8_t FCR_DMA_SELECT = 0x08;  // DMA mode select

    // LCR bits
    static constexpr uint8_t LCR_DLAB = 0x80;   // Divisor latch access bit
    static constexpr uint8_t LCR_SBC = 0x40;    // Set break control
    static constexpr uint8_t LCR_SPAR = 0x20;   // Stick parity
    static constexpr uint8_t LCR_EPAR = 0x10;   // Even parity select
    static constexpr uint8_t LCR_PARITY = 0x08; // Parity enable
    static constexpr uint8_t LCR_STOP = 0x04;   // Stop bits

    // MCR bits
    static constexpr uint8_t MCR_LOOP = 0x10; // Enable loopback test mode
    static constexpr uint8_t MCR_OUT2 = 0x08; // Out2 complement
    static constexpr uint8_t MCR_OUT1 = 0x04; // Out1 complement
    static constexpr uint8_t MCR_RTS = 0x02;  // RTS complement
    static constexpr uint8_t MCR_DTR = 0x01;  // DTR complement

    // LSR bits
    static constexpr uint8_t LSR_FIFOE = 0x80; // FIFO error
    static constexpr uint8_t LSR_TEMT = 0x40;  // Transmitter empty
    static constexpr uint8_t LSR_THRE = 0x20;  // Transmit-hold-register empty
    static constexpr uint8_t LSR_BI = 0x10;    // Break interrupt indicator
    static constexpr uint8_t LSR_FE = 0x08;    // Frame error indicator
    static constexpr uint8_t LSR_PE = 0x04;    // Parity error indicator
    static constexpr uint8_t LSR_OE = 0x02;    // Overrun error indicator
    static constexpr uint8_t LSR_DR = 0x01;    // Receiver data ready
    static constexpr uint8_t LSR_BRK_ERROR_BITS = 0x1E;

    // MSR bits
    static constexpr uint8_t MSR_DCD = 0x80;  // Data Carrier Detect
    static constexpr uint8_t MSR_RI = 0x40;   // Ring Indicator
    static constexpr uint8_t MSR_DSR = 0x20;  // Data Set Ready
    static constexpr uint8_t MSR_CTS = 0x10;  // Clear to Send
    static constexpr uint8_t MSR_DDCD = 0x08; // Delta DCD
    static constexpr uint8_t MSR_TERI = 0x04; // Trailing edge ring indicator
    static constexpr uint8_t MSR_DDSR = 0x02; // Delta DSR
    static constexpr uint8_t MSR_DCTS = 0x01; // Delta CTS
    static constexpr uint8_t MSR_ANY_DELTA = 0x0F;

    explicit NS16550(std::shared_ptr<host::HostConsole> console,
                     IrqCallback irq_callback,
                     uint32_t interrupt_id = DEFAULT_INTERRUPT_ID,
                     uint32_t reg_shift = DEFAULT_REG_SHIFT,
                     uint32_t reg_io_width = DEFAULT_REG_IO_WIDTH);

    void tick() override;

private:
    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override;
    bool write_internal(addr_t offset, size_t size, uint64_t value) override;

    void update_interrupt();

    uint8_t rx_byte();
    void tx_byte(uint8_t val);

    std::shared_ptr<host::HostConsole> console_;

    uint32_t reg_shift_;
    uint32_t reg_io_width_;

    std::mutex ns16550_mutex_;

    std::queue<uint8_t> rx_queue_;
    uint8_t dll_;
    uint8_t dlm_;
    uint8_t iir_;
    uint8_t ier_;
    uint8_t fcr_;
    uint8_t lcr_;
    uint8_t mcr_;
    uint8_t lsr_;
    uint8_t msr_;
    uint8_t scr_;
};

} // namespace uemu::device
