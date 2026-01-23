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

#pragma once

#include <filesystem>
#include <fstream>

#include "core/dram.hpp"
#include "device/device.hpp"

namespace uemu::device {

// VirtIO constants
constexpr uint32_t VIRTIO_VENDOR_ID = 0x12345678;
constexpr uint32_t VIRTIO_MAGIC_NUMBER = 0x74726976;
constexpr uint32_t VIRTIO_VERSION = 2;
constexpr uint32_t VIRTIO_CONFIG_GENERATE = 0;
constexpr uint32_t VIRTIO_STATUS_DRIVER_OK = 4;
constexpr uint32_t VIRTIO_STATUS_DEVICE_NEEDS_RESET = 64;
constexpr uint32_t VIRTIO_INT_USED_RING = 1;
constexpr uint32_t VIRTIO_INT_CONF_CHANGE = 2;
constexpr uint16_t VIRTIO_DESC_F_NEXT = 1;
constexpr uint16_t VIRTIO_DESC_F_WRITE = 2;

// VirtIO Block Device constants
constexpr uint32_t VIRTIO_BLK_DEV_ID = 2;
constexpr uint32_t VIRTIO_BLK_T_IN = 0;
constexpr uint32_t VIRTIO_BLK_T_OUT = 1;
constexpr uint32_t VIRTIO_BLK_T_FLUSH = 4;
constexpr uint32_t VIRTIO_BLK_T_GET_ID = 8;
constexpr uint8_t VIRTIO_BLK_S_OK = 0;
constexpr uint8_t VIRTIO_BLK_S_IOERR = 1;
constexpr uint8_t VIRTIO_BLK_S_UNSUPP = 2;
constexpr uint32_t VIRTIO_BLK_F_RO = (1 << 5);

constexpr uint32_t DISK_BLK_SIZE = 512;
constexpr uint32_t VBLK_FEATURES_0 = 0;
constexpr uint32_t VBLK_FEATURES_1 = 1; // VIRTIO_F_VERSION_1
constexpr uint32_t VBLK_QUEUE_NUM_MAX = 1024;
constexpr uint64_t DEFAULT_DISK_SIZE = 64 * 1024 * 1024; // 64MB

// VirtIO MMIO register offsets (byte offsets divided by 4 for word addressing)
enum VirtioReg : uint32_t {
    VIRTIO_MagicValue = 0x000,
    VIRTIO_Version = 0x004,
    VIRTIO_DeviceID = 0x008,
    VIRTIO_VendorID = 0x00c,
    VIRTIO_DeviceFeatures = 0x010,
    VIRTIO_DeviceFeaturesSel = 0x014,
    VIRTIO_DriverFeatures = 0x020,
    VIRTIO_DriverFeaturesSel = 0x024,
    VIRTIO_QueueSel = 0x030,
    VIRTIO_QueueNumMax = 0x034,
    VIRTIO_QueueNum = 0x038,
    VIRTIO_QueueReady = 0x044,
    VIRTIO_QueueNotify = 0x050,
    VIRTIO_InterruptStatus = 0x060,
    VIRTIO_InterruptACK = 0x064,
    VIRTIO_Status = 0x070,
    VIRTIO_QueueDescLow = 0x080,
    VIRTIO_QueueDescHigh = 0x084,
    VIRTIO_QueueDriverLow = 0x090,
    VIRTIO_QueueDriverHigh = 0x094,
    VIRTIO_QueueDeviceLow = 0x0a0,
    VIRTIO_QueueDeviceHigh = 0x0a4,
    VIRTIO_ConfigGeneration = 0x0fc,
    VIRTIO_Config = 0x100,
};

#pragma pack(push, 1)

struct VirtqDesc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct VirtioBlkConfig {
    uint64_t capacity;
    uint32_t size_max;
    uint32_t seg_max;

    struct {
        uint16_t cylinders;
        uint8_t heads;
        uint8_t sectors;
    } geometry;

    uint32_t blk_size;

    struct {
        uint8_t physical_block_exp;
        uint8_t alignment_offset;
        uint16_t min_io_size;
        uint32_t opt_io_size;
    } topology;

    uint8_t writeback;
    uint8_t unused0[3];
    uint32_t max_discard_sectors;
    uint32_t max_discard_seg;
    uint32_t discard_sector_alignment;
    uint32_t max_write_zeroes_sectors;
    uint32_t max_write_zeroes_seg;
    uint8_t write_zeroes_may_unmap;
    uint8_t unused1[3];
    uint64_t disk_size;
};

struct VblkReqHeader {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

#pragma pack(pop)

struct VirtioBlkQueue {
    uint32_t queue_num = 0;
    uint64_t queue_desc = 0;
    uint64_t queue_avail = 0;
    uint64_t queue_used = 0;
    uint16_t last_avail = 0;
    bool ready = false;
};

class VirtioBlk : public IrqDevice {
public:
    static constexpr addr_t DEFAULT_BASE = 0x10001000;
    static constexpr size_t SIZE = 0x1000;
    static constexpr uint32_t DEFAULT_INTERRUPT_ID = 12;

    VirtioBlk(std::shared_ptr<core::Dram> dram,
              const std::filesystem::path& disk_path, IrqCallback irq_callback,
              uint32_t interrupt_id = DEFAULT_INTERRUPT_ID);

    VirtioBlk(const VirtioBlk&) = delete;
    VirtioBlk& operator=(const VirtioBlk&) = delete;
    VirtioBlk(VirtioBlk&&) = delete;
    VirtioBlk& operator=(VirtioBlk&&) = delete;

    ~VirtioBlk() override;

private:
    std::optional<uint64_t> read_internal(addr_t offset, size_t size) override;
    bool write_internal(addr_t offset, size_t size, uint64_t value) override;

    void set_fail();
    void update_status(uint32_t status);
    void queue_notify_handler(uint32_t index);
    int desc_handler(const VirtioBlkQueue& queue, uint16_t desc_idx,
                     uint32_t* plen);
    void read_handler(uint64_t sector, uint64_t desc_addr, uint32_t len);
    void write_handler(uint64_t sector, uint64_t desc_addr, uint32_t len);

    bool open_disk(const std::filesystem::path& disk_path);
    void close_disk();

    std::shared_ptr<core::Dram> dram_;

    // Device state
    uint32_t device_features_ = 0;
    uint32_t device_features_sel_ = 0;
    uint32_t driver_features_ = 0;
    uint32_t driver_features_sel_ = 0;
    uint32_t queue_sel_ = 0;
    std::array<VirtioBlkQueue, 2> queues_;
    uint32_t status_ = 0;
    uint32_t interrupt_status_ = 0;

    // Disk storage
    std::fstream disk_file_;
    size_t disk_size_;
    VirtioBlkConfig config_;

    std::filesystem::path disk_path_;
};

}; // namespace uemu::device
