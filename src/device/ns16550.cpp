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

#include "device/ns16550.hpp"
#include "core/mmu.hpp"

namespace uemu::device {

NS16550::NS16550(std::shared_ptr<host::HostConsole> console,
                 IrqCallback irq_callback, uint32_t interrupt_id,
                 uint32_t reg_shift, uint32_t reg_io_width)
    : IrqDevice("NS16550", DEFAULT_BASE, SIZE, irq_callback, interrupt_id),
      console_(std::move(console)), reg_shift_(reg_shift),
      reg_io_width_(reg_io_width), dll_(0x0C), dlm_(0), iir_(IIR_NO_INT),
      ier_(0), fcr_(0), lcr_(0), mcr_(MCR_OUT2), lsr_(LSR_TEMT | LSR_THRE),
      msr_(MSR_DCD | MSR_DSR | MSR_CTS), scr_(0) {}

void NS16550::tick() {
    std::lock_guard<std::mutex> lock(ns16550_mutex_);

    if (!(fcr_ & FCR_ENABLE_FIFO) || (mcr_ & MCR_LOOP) ||
        QUEUE_SIZE <= rx_queue_.size())
        return;

    std::optional<char> c = console_->read_char();

    if (c.has_value()) {
        rx_queue_.push(static_cast<uint8_t>(*c));
        lsr_ |= LSR_DR;
        update_interrupt();
    }
}

std::optional<uint64_t> NS16550::read_internal(addr_t offset, size_t size) {
    uint8_t val;

    if (reg_io_width_ != size) [[unlikely]]
        return std::nullopt;

    if (offset + size > core::MMU::PGSIZE) [[unlikely]]
        return std::nullopt;

    offset >>= reg_shift_;
    offset &= 7;

    {
        std::lock_guard<std::mutex> lock(ns16550_mutex_);

        switch (offset) {
            case RX:
                val = (lcr_ & LCR_DLAB) ? dll_ : rx_byte();
                update_interrupt();
                return val;
            case IER: return (lcr_ & LCR_DLAB) ? dlm_ : ier_;
            case IIR: return iir_ | IIR_TYPE_BITS;
            case LCR: return lcr_;
            case MCR: return mcr_;
            case LSR: return lsr_;
            case MSR: return msr_;
            case SCR: return scr_;
            default: return std::nullopt;
        };
    }

    std::unreachable();
}

bool NS16550::write_internal(addr_t offset, size_t size, uint64_t value) {
    if (reg_io_width_ != size) [[unlikely]]
        return false;

    if (offset + size > core::MMU::PGSIZE) [[unlikely]]
        return false;

    offset >>= reg_shift_;
    offset &= 7;
    value &= 0xFF;

    {
        std::lock_guard<std::mutex> lock(ns16550_mutex_);

        switch (offset) {
            case TX:
                if (lcr_ & LCR_DLAB) {
                    dll_ = value;
                    update_interrupt();
                    return true;
                }

                /* Loopback mode */
                if (mcr_ & MCR_LOOP) {
                    if (rx_queue_.size() < QUEUE_SIZE) {
                        rx_queue_.push(value);
                        lsr_ |= LSR_DR;
                    }
                    update_interrupt();
                    return true;
                }

                tx_byte(value);
                update_interrupt();
                return true;
            case IER:
                if (!(lcr_ & LCR_DLAB))
                    ier_ = value & 0x0f;
                else
                    dlm_ = value;

                update_interrupt();
                return true;
            case FCR:
                fcr_ = value;
                update_interrupt();
                return true;
            case LCR:
                lcr_ = value;
                update_interrupt();
                return true;
            case MCR:
                mcr_ = value;
                update_interrupt();
                return true;
            case LSR:
                /* Factory test */
                return true;
            case MSR:
                /* Not used */
                return true;
            case SCR: scr_ = value; return true;
            default: return false;
        };
    }

    std::unreachable();
}

void NS16550::update_interrupt() {
    uint8_t interrupts = 0;

    /* Handle clear rx */
    if (fcr_ & FCR_CLEAR_RCVR) {
        fcr_ &= ~FCR_CLEAR_RCVR;

        while (!rx_queue_.empty())
            rx_queue_.pop();

        lsr_ &= ~LSR_DR;
    }

    /* Handle clear tx */
    if (fcr_ & FCR_CLEAR_XMIT) {
        fcr_ &= ~FCR_CLEAR_XMIT;
        lsr_ |= LSR_TEMT | LSR_THRE;
    }

    /* Data ready and rcv interrupt enabled ? */
    if ((ier_ & IER_RDI) && (lsr_ & LSR_DR))
        interrupts |= IIR_RDI;

    /* Transmitter empty and interrupt enabled ? */
    if ((ier_ & IER_THRI) && (lsr_ & LSR_TEMT))
        interrupts |= IIR_THRI;

    /* Now update the interrup line, if necessary */
    if (!interrupts) {
        iir_ = IIR_NO_INT;
        update_irq(false);
    } else {
        iir_ = interrupts;
        update_irq(true);
    }

    /*
     * If the OS disabled the tx interrupt, we know that there is nothing
     * more to transmit.
     */
    if (!(ier_ & IER_THRI))
        lsr_ |= LSR_TEMT | LSR_THRE;
}

uint8_t NS16550::rx_byte() {
    if (rx_queue_.empty()) {
        lsr_ &= ~LSR_DR;
        return 0;
    }

    /* Break issued ? */
    if (lsr_ & LSR_BI) {
        lsr_ &= ~LSR_BI;
        return 0;
    }

    uint8_t ret = rx_queue_.front();
    rx_queue_.pop();
    if (rx_queue_.empty())
        lsr_ &= ~LSR_DR;

    return ret;
}

void NS16550::tx_byte(uint8_t val) {
    lsr_ |= LSR_TEMT | LSR_THRE;
    console_->write_char(val);
}

} // namespace uemu::device
