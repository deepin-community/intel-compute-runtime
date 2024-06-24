/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device/device.h"

#include "shared/source/device/device.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"

namespace L0 {

uint32_t Device::getRootDeviceIndex() const {
    return neoDevice->getRootDeviceIndex();
}

NEO::DebuggerL0 *Device::getL0Debugger() {
    return getNEODevice()->getL0Debugger();
}

template <typename NodeT>
NEO::TagAllocatorBase *getInOrderCounterAllocator(std::unique_ptr<NEO::TagAllocatorBase> &allocator, std::mutex &inOrderAllocatorMutex, NEO::Device &neoDevice) {
    if (!allocator.get()) {
        std::unique_lock<std::mutex> lock(inOrderAllocatorMutex);

        if (!allocator.get()) {
            RootDeviceIndicesContainer rootDeviceIndices = {neoDevice.getRootDeviceIndex()};

            const size_t maxPartitionCount = neoDevice.getDeviceBitfield().count();

            const size_t nodeSize = sizeof(uint64_t) * maxPartitionCount * 2; // Multiplied by 2 to handle 32b overflow

            DEBUG_BREAK_IF(alignUp(nodeSize, MemoryConstants::cacheLineSize) * NodeT::defaultAllocatorTagCount > MemoryConstants::pageSize64k);

            allocator = std::make_unique<NEO::TagAllocator<NodeT>>(rootDeviceIndices, neoDevice.getMemoryManager(), NodeT::defaultAllocatorTagCount,
                                                                   MemoryConstants::cacheLineSize, nodeSize, false, neoDevice.getDeviceBitfield());
        }
    }

    return allocator.get();
}

NEO::TagAllocatorBase *Device::getDeviceInOrderCounterAllocator() {
    return getInOrderCounterAllocator<NEO::DeviceAllocNodeType<true>>(deviceInOrderCounterAllocator, inOrderAllocatorMutex, *getNEODevice());
}

NEO::TagAllocatorBase *Device::getHostInOrderCounterAllocator() {
    return getInOrderCounterAllocator<NEO::DeviceAllocNodeType<false>>(hostInOrderCounterAllocator, inOrderAllocatorMutex, *getNEODevice());
}

uint32_t Device::getNextSyncDispatchQueueId() {
    auto newValue = syncDispatchQueueIdAllocator.fetch_add(1);

    UNRECOVERABLE_IF(newValue == (std::numeric_limits<uint32_t>::max() - 1));

    ensureSyncDispatchTokenAllocation();

    return newValue;
}

void Device::ensureSyncDispatchTokenAllocation() {
    if (!syncDispatchTokenAllocation) {
        std::unique_lock<std::mutex> lock(syncDispatchTokenMutex);

        if (!syncDispatchTokenAllocation) {

            const NEO::AllocationProperties allocationProperties(getRootDeviceIndex(), true, MemoryConstants::pageSize,
                                                                 NEO::AllocationType::syncDispatchToken,
                                                                 true, false, getNEODevice()->getDeviceBitfield());

            syncDispatchTokenAllocation = getNEODevice()->getMemoryManager()->allocateGraphicsMemoryWithProperties(allocationProperties);
            UNRECOVERABLE_IF(syncDispatchTokenAllocation == nullptr);

            memset(syncDispatchTokenAllocation->getUnderlyingBuffer(), 0, syncDispatchTokenAllocation->getUnderlyingBufferSize());
        }
    }
}

} // namespace L0