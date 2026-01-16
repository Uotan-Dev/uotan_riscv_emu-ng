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

#include <iostream>
#include <print> // IWYU pragma: keep

#include "device/virtio_blk.hpp"

namespace uemu::device {

VirtioBlk::VirtioBlk(std::shared_ptr<core::Dram> dram,
                     const std::string& disk_path, IrqCallback irq_callback,
                     uint32_t interrupt_id)
    : IrqDevice("VirtIO-Block", DEFAULT_BASE, SIZE, irq_callback, interrupt_id),
      dram_(std::move(dram)), disk_path_(disk_path) {
    std::memset(&config_, 0, sizeof(config_));

    if (!open_disk(disk_path_))
        throw std::runtime_error("Failed to open disk: " + disk_path_);
}

VirtioBlk::~VirtioBlk() { close_disk(); }

bool VirtioBlk::open_disk(const std::string& disk_path) {
    std::ifstream test_file(disk_path, std::ios::binary);
    bool file_exists = test_file.good();
    test_file.close();

    if (!file_exists) {
        // Create a new 64MB disk file
        std::ofstream create_file(disk_path, std::ios::binary);

        if (!create_file)
            return false;

        create_file.seekp(DEFAULT_DISK_SIZE - 1);
        create_file.put(0);
        create_file.close();

        if (!create_file.good())
            return false;
    }

    disk_file_.open(disk_path, std::ios::binary | std::ios::in | std::ios::out);

    if (!disk_file_)
        return false;

    disk_file_.seekg(0, std::ios::end);
    std::streamsize file_size = disk_file_.tellg();
    disk_file_.seekg(0, std::ios::beg);

    if (file_size <= 0)
        return false;

    disk_size_ = static_cast<size_t>(file_size);

    config_.disk_size = disk_size_;
    config_.capacity = (disk_size_ - 1) / DISK_BLK_SIZE + 1;
    return true;
}

void VirtioBlk::close_disk() {
    if (disk_file_.is_open()) {
        disk_file_.flush();
        disk_file_.close();
    }
}

void VirtioBlk::set_fail() {
    status_ |= VIRTIO_STATUS_DEVICE_NEEDS_RESET;

    if (status_ & VIRTIO_STATUS_DRIVER_OK) {
        interrupt_status_ |= VIRTIO_INT_CONF_CHANGE;
        update_irq(true);
    }

    // std::println(std::cerr, "fail");
}

void VirtioBlk::update_status(uint32_t status) {
    status_ |= status;

    if (status == 0) {
        uint32_t saved_device_features = device_features_;
        uint64_t saved_capacity = config_.capacity;

        device_features_sel_ = 0;
        driver_features_ = 0;
        driver_features_sel_ = 0;
        queue_sel_ = 0;
        status_ = 0;
        interrupt_status_ = 0;

        for (auto& queue : queues_)
            queue = VirtioBlkQueue{};

        std::memset(&config_, 0, sizeof(config_));

        device_features_ = saved_device_features;
        config_.capacity = saved_capacity;
    }
}

void VirtioBlk::read_handler(uint64_t sector, uint64_t desc_addr,
                             uint32_t len) {
    uint64_t offset = sector * DISK_BLK_SIZE;

    if (offset >= disk_size_ || offset + len > disk_size_)
        return;

    // Read from file
    std::vector<uint8_t> buffer(len);
    disk_file_.seekg(offset);
    if (!disk_file_.read(reinterpret_cast<char*>(buffer.data()), len))
        return;

    // Copy to DRAM
    dram_->write_bytes(desc_addr, buffer.data(), len);
}

void VirtioBlk::write_handler(uint64_t sector, uint64_t desc_addr,
                              uint32_t len) {
    uint64_t offset = sector * DISK_BLK_SIZE;

    if (offset >= disk_size_ || offset + len > disk_size_)
        return;

    // Read from DRAM
    std::vector<uint8_t> buffer(len);
    dram_->read_bytes(desc_addr, buffer.data(), len);

    // Write to file
    disk_file_.seekp(offset);
    disk_file_.write(reinterpret_cast<const char*>(buffer.data()), len);
    // disk_file_.flush();
}

int VirtioBlk::desc_handler(const VirtioBlkQueue& queue, uint16_t desc_idx,
                            uint32_t* plen) {
    VirtqDesc vq_desc[3];

    for (int i = 0; i < 3; i++) {
        static_assert(sizeof(VirtqDesc) == sizeof(uint32_t) * 4);

        addr_t desc_addr = queue.queue_desc + desc_idx * 16;

        if (!dram_->is_valid_addr(desc_addr, sizeof(VirtqDesc))) {
            set_fail();
            return -1;
        }

        dram_->read_bytes(desc_addr, &vq_desc[i], sizeof(VirtqDesc));
        desc_idx = vq_desc[i].next;
    }

    if (!(vq_desc[0].flags & VIRTIO_DESC_F_NEXT) ||
        !(vq_desc[1].flags & VIRTIO_DESC_F_NEXT) ||
        (vq_desc[2].flags & VIRTIO_DESC_F_NEXT)) {
        set_fail();
        return -1;
    }

    // Read request header from first descriptor
    if (!dram_->is_valid_addr(vq_desc[0].addr, 16)) {
        set_fail();
        return -1;
    }

    uint32_t type = dram_->read<uint32_t>(vq_desc[0].addr + 0);
    uint64_t sector = dram_->read<uint64_t>(vq_desc[0].addr + 8);

    // Validate sector bounds
    if (sector > config_.capacity - 1) {
        dram_->write<uint8_t>(vq_desc[2].addr, VIRTIO_BLK_S_IOERR);
        return -1;
    }

    uint8_t status = VIRTIO_BLK_S_OK;

    switch (type) {
        case VIRTIO_BLK_T_IN:
            // Read from disk to buffer
            read_handler(sector, vq_desc[1].addr, vq_desc[1].len);
            break;
        case VIRTIO_BLK_T_OUT:
            // Write from buffer to disk
            if (device_features_ & VIRTIO_BLK_F_RO)
                status = VIRTIO_BLK_S_IOERR;
            else
                write_handler(sector, vq_desc[1].addr, vq_desc[1].len);
            break;
        case VIRTIO_BLK_T_FLUSH: disk_file_.flush(); break;
        case VIRTIO_BLK_T_GET_ID:
            dram_->write_bytes(vq_desc[1].addr, (const uint8_t*)"SERIAL0001",
                               10);
            break;
        default:
            // println(std::cerr, "FUCKED: {}", type);
            status = VIRTIO_BLK_S_UNSUPP;
            break;
    }

    // Write status to third descriptor
    dram_->write<uint8_t>(vq_desc[2].addr, status);
    *plen = vq_desc[1].len;

    return (status == VIRTIO_BLK_S_OK) ? 0 : -1;
}

void VirtioBlk::queue_notify_handler(uint32_t index) {
    if (index >= queues_.size()) {
        set_fail();
        return;
    }

    VirtioBlkQueue& queue = queues_[index];

    if (status_ & VIRTIO_STATUS_DEVICE_NEEDS_RESET)
        return;

    if (!((status_ & VIRTIO_STATUS_DRIVER_OK) && queue.ready)) {
        set_fail();
        return;
    }

    // virtq_avail structure:
    // u16 flags; (offset 0)
    // u16 idx;   (offset 2) <- We need this
    // u16 ring[];(offset 4)

    if (!dram_->is_valid_addr(queue.queue_avail, 4)) {
        set_fail();
        return;
    }

    // Read 'idx' field (byte offset 2)
    uint16_t new_avail_idx = dram_->read<uint16_t>(queue.queue_avail + 2);

    if (new_avail_idx - queue.last_avail >
        static_cast<uint16_t>(queue.queue_num)) {
        set_fail();
        return;
    }

    if (queue.last_avail == new_avail_idx)
        return;

    // virtq_used structure:
    // u16 flags; (offset 0)
    // u16 idx;   (offset 2)
    // virtq_used_elem ring[]; (offset 4)

    if (!dram_->is_valid_addr(queue.queue_used, 4)) {
        set_fail();
        return;
    }

    // Read current used 'idx' (byte offset 2) to increment it locally
    uint16_t new_used_idx = dram_->read<uint16_t>(queue.queue_used + 2);

    while (queue.last_avail != new_avail_idx) {
        uint16_t queue_idx = queue.last_avail % queue.queue_num;

        addr_t avail_elem_addr = queue.queue_avail + 4 + (queue_idx * 2);
        if (!dram_->is_valid_addr(avail_elem_addr, 2)) {
            set_fail();
            return;
        }

        uint16_t head_desc_idx = dram_->read<uint16_t>(avail_elem_addr);

        uint32_t len = 0;
        int result = desc_handler(queue, head_desc_idx, &len);
        if (result != 0) {
            set_fail();
            return;
        }

        addr_t used_elem_addr =
            queue.queue_used + 4 + ((new_used_idx % queue.queue_num) * 8);
        if (!dram_->is_valid_addr(used_elem_addr, 8)) {
            set_fail();
            return;
        }

        dram_->write<uint32_t>(used_elem_addr + 0, head_desc_idx);
        dram_->write<uint32_t>(used_elem_addr + 4, len);

        queue.last_avail++;
        new_used_idx++;
    }

    // Update Used Ring 'idx' (byte offset 2)
    dram_->write<uint16_t>(queue.queue_used + 2, new_used_idx);

    uint16_t avail_flags = dram_->read<uint16_t>(queue.queue_avail + 0);
    if (!(avail_flags & 1)) {
        interrupt_status_ |= VIRTIO_INT_USED_RING;
        update_irq(true);
    }
}

std::optional<uint64_t> VirtioBlk::read_internal(addr_t offset, size_t size) {
    if (size == 8) {
        std::optional<uint64_t> lo = read_internal(offset, 4);

        if (lo == std::nullopt) [[unlikely]]
            return std::nullopt;

        std::optional<uint64_t> hi = read_internal(offset + 4, 4);

        if (hi == std::nullopt) [[unlikely]]
            return std::nullopt;

        return *lo | (*hi << 32);
    }

    if (size != 4)
        return std::nullopt;

    uint32_t reg = offset >> 2;

    switch (reg) {
        case VIRTIO_MagicValue: return VIRTIO_MAGIC_NUMBER;
        case VIRTIO_Version: return VIRTIO_VERSION;
        case VIRTIO_DeviceID: return VIRTIO_BLK_DEV_ID;
        case VIRTIO_VendorID: return VIRTIO_VENDOR_ID;
        case VIRTIO_DeviceFeatures:
            if (device_features_sel_ == 0)
                return VBLK_FEATURES_0 | device_features_;
            else if (device_features_sel_ == 1)
                return VBLK_FEATURES_1;
            return 0;
        case VIRTIO_QueueNumMax: return VBLK_QUEUE_NUM_MAX;
        case VIRTIO_QueueReady: return queues_[queue_sel_].ready ? 1 : 0;
        case VIRTIO_InterruptStatus: return interrupt_status_;
        case VIRTIO_Status: return status_;
        case VIRTIO_ConfigGeneration: return VIRTIO_CONFIG_GENERATE;
        default:
            if (reg >= VIRTIO_Config) {
                uint32_t config_offset = (reg - VIRTIO_Config) * 4;
                if (config_offset + 4 <= sizeof(config_)) {
                    uint32_t value;
                    std::memcpy(&value,
                                reinterpret_cast<const uint8_t*>(&config_) +
                                    config_offset,
                                4);
                    return value;
                }
            }
            return 0;
    }

    return 0;
}

bool VirtioBlk::write_internal(addr_t offset, size_t size, uint64_t value) {
    if (size == 8)
        return write_internal(offset, 4, value & 0xFFFFFFFF) &&
               write_internal(offset + 4, 4, value >> 32);

    if (size != 4)
        return false;

    uint32_t reg = offset >> 2;
    uint32_t val = static_cast<uint32_t>(value);

    switch (reg) {
        case VIRTIO_DeviceFeaturesSel: device_features_sel_ = val; break;
        case VIRTIO_DriverFeatures:
            if (driver_features_sel_ == 0)
                driver_features_ = val;
            break;
        case VIRTIO_DriverFeaturesSel: driver_features_sel_ = val; break;
        case VIRTIO_QueueSel:
            if (val < queues_.size())
                queue_sel_ = val;
            else
                set_fail();
            break;
        case VIRTIO_QueueNum:
            if (val > 0 && val <= VBLK_QUEUE_NUM_MAX)
                queues_[queue_sel_].queue_num = val;
            else
                set_fail();
            break;
        case VIRTIO_QueueReady:
            queues_[queue_sel_].ready = (val & 1) != 0;
            if (val & 1) {
                addr_t avail_idx_addr = queues_[queue_sel_].queue_avail + 2;
                if (dram_->is_valid_addr(avail_idx_addr, 2))
                    queues_[queue_sel_].last_avail =
                        dram_->read<uint16_t>(avail_idx_addr);
            }
            break;
        case VIRTIO_QueueDescLow: queues_[queue_sel_].queue_desc = val; break;
        case VIRTIO_QueueDescHigh:
            if (val != 0)
                set_fail(); // 64-bit addresses not supported
            break;
        case VIRTIO_QueueDriverLow:
            queues_[queue_sel_].queue_avail = val;
            break;
        case VIRTIO_QueueDriverHigh:
            if (val != 0)
                set_fail();
            break;
        case VIRTIO_QueueDeviceLow: queues_[queue_sel_].queue_used = val; break;
        case VIRTIO_QueueDeviceHigh:
            if (val != 0)
                set_fail();
            break;
        case VIRTIO_QueueNotify: queue_notify_handler(val); break;
        case VIRTIO_InterruptACK:
            interrupt_status_ &= ~val;
            if (interrupt_status_ == 0)
                update_irq(false);
            break;
        case VIRTIO_Status: update_status(val); break;
        default:
            // Configuration space writes
            if (reg >= VIRTIO_Config) {
                uint32_t config_offset = (reg - VIRTIO_Config) * 4;
                if (config_offset + 4 <= sizeof(config_)) {
                    std::memcpy(reinterpret_cast<uint8_t*>(&config_) +
                                    config_offset,
                                &val, 4);
                }
            }
            break;
    }

    return true;
}

} // namespace uemu::device
