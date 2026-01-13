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

#include "device/goldfish_events.hpp"

namespace uemu::device {

GoldfishEvents::GoldfishEvents(IrqCallback irq_callback, uint32_t interrupt_id,
                               const std::string& device_name)
    : IrqDevice("GoldfishEvents", DEFAULT_BASE, SIZE, irq_callback,
                interrupt_id),
      device_name_(device_name), page_(0), state_(STATE_INIT), first_(0),
      last_(0) {
    events_.fill(0);

    // Set up event capabilities
    // Enable EV_SYN and EV_KEY
    set_event_bit(EV_SYN, EV_KEY);
    // Enable key codes
    // We want to implement Unicode reverse-mapping, so allow any kind of key
    // Set bits [1..0xff] for standard keys
    set_event_bits(EV_KEY, 1, 0xff);
    // Set bits [0x160..0x1ff] for extended keys
    set_event_bits(EV_KEY, 0x160, 0x1ff);
}

void GoldfishEvents::push_key_event(KeyEvent event) {
    std::lock_guard<std::mutex> lock(goldfish_events_mutex_);

    const auto [keycode, action] = event;
    enqueue_event(EV_KEY, static_cast<uint32_t>(keycode),
                  action == KeyAction::Press ? 1 : 0);
}

std::optional<uint64_t>
GoldfishEvents::read_internal(addr_t offset, [[maybe_unused]] size_t size) {
    std::lock_guard<std::mutex> lock(goldfish_events_mutex_);

    // This hack ensures we only raise IRQ when kernel driver is ready
    if (offset == REG_LEN && page_ == PAGE_ABSDATA) {
        if (state_ == STATE_BUFFERED)
            update_irq(true);

        state_ = STATE_LIVE;
    }

    if (offset == REG_READ)
        return dequeue_event();
    else if (offset == REG_LEN)
        return get_page_len();
    else if (offset >= REG_DATA)
        return get_page_data(offset - REG_DATA);

    return 0; // Do not fail the read
}

bool GoldfishEvents::write_internal(addr_t offset, [[maybe_unused]] size_t size,
                                    uint64_t value) {
    std::lock_guard<std::mutex> lock(goldfish_events_mutex_);

    if (offset == REG_SET_PAGE)
        page_ = static_cast<uint32_t>(value);

    return true; // Do not fail the write
}

void GoldfishEvents::enqueue_event(uint32_t type, uint32_t code, int value) {
    size_t enqueued =
        (last_ >= first_) ? (last_ - first_) : (MAX_EVENTS + last_ - first_);

    // queue is full
    if (enqueued + 3 > MAX_EVENTS)
        return;

    if (first_ == last_) {
        if (state_ == STATE_LIVE)
            update_irq(true);
        else
            state_ = STATE_BUFFERED;
    }

    // Enqueue the event (type, code, value)
    events_[last_] = type;
    last_ = (last_ + 1) & (MAX_EVENTS - 1);
    events_[last_] = code;
    last_ = (last_ + 1) & (MAX_EVENTS - 1);
    events_[last_] = static_cast<uint32_t>(value);
    last_ = (last_ + 1) & (MAX_EVENTS - 1);
}

uint32_t GoldfishEvents::dequeue_event() {
    // Check if queue is empty
    if (first_ == last_)
        return 0;

    uint32_t event = events_[first_];
    first_ = (first_ + 1) & (MAX_EVENTS - 1);

    if (first_ == last_) {
        update_irq(false); // Clear IRQ if queue is now empty
    } else if (((first_ + 2) & (MAX_EVENTS - 1)) < last_ ||
               (first_ & (MAX_EVENTS - 1)) > last_) {
        // Re-trigger IRQ if there are still events
        update_irq(false);
        update_irq(true);
    }

    return event;
}

void GoldfishEvents::set_event_bits(uint32_t type, size_t bitl, size_t bith) {
    if (type > EV_MAX) [[unlikely]]
        return;

    uint32_t il = bitl / 8;
    uint32_t ih = bith / 8;

    // Resize bits array if needed
    if (ih >= ev_bits_[type].bits.size())
        ev_bits_[type].bits.resize(ih + 1, 0);

    uint8_t maskl = 0xFFU << (bitl & 7);
    uint8_t maskh = 0xFFU >> (7 - (bith & 7));

    if (il >= ih) {
        maskh &= maskl;
        ev_bits_[type].bits[ih] |= maskh;
    } else {
        ev_bits_[type].bits[il] |= maskl;

        for (uint32_t i = il + 1; i < ih; i++)
            ev_bits_[type].bits[i] = 0xFF;

        ev_bits_[type].bits[ih] |= maskh;
    }
}

void GoldfishEvents::set_event_bit(uint32_t type, size_t bit) {
    set_event_bits(type, bit, bit);
}

size_t GoldfishEvents::get_page_len() const {
    if (page_ == PAGE_NAME)
        return device_name_.length();

    if (page_ >= PAGE_EVBITS && page_ <= PAGE_EVBITS + EV_MAX)
        return ev_bits_[page_ - PAGE_EVBITS].bits.size();

    return 0;
}

uint8_t GoldfishEvents::get_page_data(size_t offset) const {
    size_t page_len = get_page_len();

    if (offset >= page_len)
        return 0;

    if (page_ == PAGE_NAME)
        return static_cast<uint8_t>(device_name_[offset]);

    if (page_ >= PAGE_EVBITS && page_ <= PAGE_EVBITS + EV_MAX) {
        uint32_t type = page_ - PAGE_EVBITS;

        if (offset < ev_bits_[type].bits.size())
            return ev_bits_[type].bits[offset];
    }

    return 0;
}

} // namespace uemu::device
