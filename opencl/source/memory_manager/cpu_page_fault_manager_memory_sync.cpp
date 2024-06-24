/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/memory_manager/unified_memory_manager.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/page_fault_manager/cpu_page_fault_manager.h"

#include "opencl/source/command_queue/command_queue.h"
#include "opencl/source/command_queue/csr_selection_args.h"

namespace NEO {
void PageFaultManager::transferToCpu(void *ptr, size_t size, void *cmdQ) {
    auto commandQueue = static_cast<CommandQueue *>(cmdQ);
    auto retVal = commandQueue->enqueueSVMMap(true, CL_MAP_WRITE, ptr, size, 0, nullptr, nullptr, false);
    UNRECOVERABLE_IF(retVal);
}
void PageFaultManager::transferToGpu(void *ptr, void *cmdQ) {
    auto commandQueue = static_cast<CommandQueue *>(cmdQ);
    memoryData[ptr].unifiedMemoryManager->insertSvmMapOperation(ptr, memoryData[ptr].size, ptr, 0, false);
    auto retVal = commandQueue->enqueueSVMUnmap(ptr, 0, nullptr, nullptr, false);
    UNRECOVERABLE_IF(retVal);
    retVal = commandQueue->finish();
    UNRECOVERABLE_IF(retVal);

    auto allocData = memoryData[ptr].unifiedMemoryManager->getSVMAlloc(ptr);
    UNRECOVERABLE_IF(allocData == nullptr);
    this->evictMemoryAfterImplCopy(allocData->cpuAllocation, &commandQueue->getDevice());
}
void PageFaultManager::allowCPUMemoryEviction(void *ptr, PageFaultData &pageFaultData) {
    auto commandQueue = static_cast<CommandQueue *>(pageFaultData.cmdQ);

    auto allocData = memoryData[ptr].unifiedMemoryManager->getSVMAlloc(ptr);
    UNRECOVERABLE_IF(allocData == nullptr);
    CsrSelectionArgs csrSelectionArgs{CL_COMMAND_READ_BUFFER, &allocData->gpuAllocations, {}, commandQueue->getDevice().getRootDeviceIndex(), nullptr};
    auto &csr = commandQueue->selectCsrForBuiltinOperation(csrSelectionArgs);
    auto osInterface = commandQueue->getDevice().getRootDeviceEnvironment().osInterface.get();

    allowCPUMemoryEvictionImpl(ptr, csr, osInterface);
}

} // namespace NEO
