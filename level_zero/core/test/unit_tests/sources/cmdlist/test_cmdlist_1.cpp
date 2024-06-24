/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/wait_status.h"
#include "shared/source/direct_submission/relaxed_ordering_helper.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/internal_allocation_storage.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/relaxed_ordering_commands_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_cpu_page_fault_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/builtin/builtin_functions_lib.h"
#include "level_zero/core/source/cmdqueue/cmdqueue_imp.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_event.h"
#include "level_zero/core/test/unit_tests/mocks/mock_image.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/sources/helper/ze_object_utils.h"

namespace L0 {
namespace ult {

using ContextCommandListCreate = Test<DeviceFixture>;

TEST_F(ContextCommandListCreate, whenCreatingCommandListFromContextThenSuccessIsReturned) {
    ze_command_list_desc_t desc = {};
    ze_command_list_handle_t hCommandList = {};

    ze_result_t result = context->createCommandList(device, &desc, &hCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(Context::fromHandle(CommandList::fromHandle(hCommandList)->getCmdListContext()), context);

    L0::CommandList *commandList = L0::CommandList::fromHandle(hCommandList);
    ze_context_handle_t hContext;
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->getContextHandle(&hContext));
    EXPECT_EQ(context, hContext);

    commandList->destroy();
}

TEST_F(ContextCommandListCreate, givenInvalidDescWhenCreatingCommandListFromContextThenErrorIsReturned) {
    ze_command_list_desc_t desc = {};
    desc.commandQueueGroupOrdinal = 0xffff;
    ze_command_list_handle_t hCommandList = {};

    ze_result_t result = context->createCommandList(device, &desc, &hCommandList);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(CommandList::fromHandle(hCommandList), nullptr);
}

TEST_F(ContextCommandListCreate, whenCreatingCommandListImmediateFromContextThenSuccessIsReturned) {
    ze_command_queue_desc_t desc = {};
    ze_command_list_handle_t hCommandList = {};

    ze_result_t result = context->createCommandListImmediate(device, &desc, &hCommandList);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(Context::fromHandle(CommandList::fromHandle(hCommandList)->getCmdListContext()), context);

    L0::CommandList *commandList = L0::CommandList::fromHandle(hCommandList);
    commandList->destroy();
}

TEST_F(ContextCommandListCreate, givenInvalidDescWhenCreatingCommandListImmediateFromContextThenErrorIsReturned) {
    ze_command_queue_desc_t desc = {};
    desc.ordinal = 0xffff;
    ze_command_list_handle_t hCommandList = {};

    ze_result_t result = context->createCommandListImmediate(device, &desc, &hCommandList);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, result);
    EXPECT_EQ(CommandList::fromHandle(hCommandList), nullptr);
}

using CommandListCreate = Test<DeviceFixture>;

TEST_F(CommandListCreate, whenCommandListIsCreatedWithInvalidProductFamilyThenFailureIsReturned) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(PRODUCT_FAMILY::IGFX_MAX_PRODUCT, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, returnValue);
    ASSERT_EQ(nullptr, commandList);
}

TEST_F(CommandListCreate, whenCommandListImmediateIsCreatedWithInvalidProductFamilyThenFailureIsReturned) {
    ze_result_t returnValue;
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(PRODUCT_FAMILY::IGFX_MAX_PRODUCT,
                                                                              device,
                                                                              &desc,
                                                                              internalEngine,
                                                                              NEO::EngineGroupType::renderCompute,
                                                                              returnValue));
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, returnValue);
    ASSERT_EQ(nullptr, commandList);
}

TEST_F(CommandListCreate, whenCommandListIsCreatedThenItIsInitialized) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    ze_device_handle_t hDevice;
    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_EQ(ZE_RESULT_SUCCESS, commandList->getDeviceHandle(&hDevice));
    EXPECT_EQ(device->toHandle(), hDevice);
    ASSERT_GT(commandList->getCmdContainer().getCmdBufferAllocations().size(), 0u);

    auto numAllocations = 0u;
    auto allocation = whiteboxCast(commandList->getCmdContainer().getCmdBufferAllocations()[0]);
    ASSERT_NE(allocation, nullptr);

    ++numAllocations;

    ASSERT_NE(nullptr, commandList->getCmdContainer().getCommandStream());
    for (uint32_t i = 0; i < NEO::HeapType::numTypes; i++) {
        auto heapType = static_cast<NEO::HeapType>(i);
        if (NEO::HeapType::dynamicState == heapType && !device->getHwInfo().capabilityTable.supportsImages) {
            ASSERT_EQ(commandList->getCmdContainer().getIndirectHeap(heapType), nullptr);
        } else {
            ASSERT_NE(commandList->getCmdContainer().getIndirectHeap(heapType), nullptr);
            ++numAllocations;
            ASSERT_NE(commandList->getCmdContainer().getIndirectHeapAllocation(heapType), nullptr);
        }
    }

    EXPECT_LT(0u, commandList->getCmdContainer().getCommandStream()->getAvailableSpace());
    ASSERT_EQ(commandList->getCmdContainer().getResidencyContainer().size(), numAllocations);
    EXPECT_EQ(commandList->getCmdContainer().getResidencyContainer().front(), allocation);
}

TEST_F(CommandListCreate, givenRegularCommandListThenDefaultNumIddPerBlockIsUsed) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    const uint32_t defaultNumIdds = CommandList::defaultNumIddsPerBlock;
    EXPECT_EQ(defaultNumIdds, commandList->getCmdContainer().getNumIddPerBlock());
}

TEST_F(CommandListCreate, givenNonExistingPtrThenAppendMemAdviseReturnsError) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    auto res = commandList->appendMemAdvise(device, nullptr, 0, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
}

TEST_F(CommandListCreate, givenNonExistingPtrThenAppendMemoryPrefetchReturnsError) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    auto res = commandList->appendMemoryPrefetch(nullptr, 0);
    EXPECT_EQ(ZE_RESULT_ERROR_INVALID_ARGUMENT, res);
}

TEST_F(CommandListCreate, givenValidPtrWhenAppendMemAdviseFailsThenReturnSuccess) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    memoryManager->failSetMemAdvise = true;

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrWhenAppendMemAdviseSucceedsThenReturnSuccess) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrThenAppendMemAdviseSetWithMaxHintThenSuccessReturned) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_FORCE_UINT32);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrThenAppendMemAdviseSetAndClearReadMostlyThenMemAdviseReadOnlySet) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.readOnly);
    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_CLEAR_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.readOnly);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrThenAppendMemAdviseSetAndClearPreferredLocationThenMemAdvisePreferredDeviceSet) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.devicePreferredLocation);
    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_CLEAR_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.devicePreferredLocation);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrWhenAppendMemAdviseIsCalledWithSetAndClearSystemMemoryPreferredLocationThenMemAdviseSetPreferredSystemMemoryFlagIsSetCorrectly) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto res = context->allocSharedMem(device->toHandle(),
                                       &deviceDesc,
                                       &hostDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_SYSTEM_MEMORY_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.systemPreferredLocation);
    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_CLEAR_SYSTEM_MEMORY_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.systemPreferredLocation);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrWhenAppendMemAdviseSetAndClearNonAtomicMostlyThenMemAdviseNonAtomicIgnored) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_NON_ATOMIC_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.nonAtomic);
    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_CLEAR_NON_ATOMIC_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.nonAtomic);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrThenAppendMemAdviseSetAndClearCachingThenMemAdviseCachingSet) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_BIAS_CACHED);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.cachedMemory);
    auto memoryManager = static_cast<MockMemoryManager *>(device->getDriverHandle()->getMemoryManager());
    EXPECT_EQ(1, memoryManager->memAdviseFlags.cachedMemory);
    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_BIAS_UNCACHED);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.cachedMemory);
    EXPECT_EQ(0, memoryManager->memAdviseFlags.cachedMemory);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

using CommandListMemAdvisePageFault = Test<PageFaultDeviceFixture>;

TEST_F(CommandListMemAdvisePageFault, givenValidPtrAndPageFaultHandlerThenAppendMemAdviseWithReadOnlyAndDevicePreferredClearsMigrationBlocked) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    flags.cpuMigrationBlocked = 1;
    deviceImp->memAdviseSharedAllocations[allocData] = flags;

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.readOnly);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.devicePreferredLocation);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_CLEAR_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_CLEAR_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.readOnly);
    EXPECT_EQ(0, flags.devicePreferredLocation);
    EXPECT_EQ(0, flags.cpuMigrationBlocked);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListMemAdvisePageFault, givenValidPtrAndPageFaultHandlerThenGpuDomainHanlderWithHintsIsSet) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    deviceImp->memAdviseSharedAllocations[allocData] = flags;

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.readOnly);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.devicePreferredLocation);

    auto handlerWithHints = L0::transferAndUnprotectMemoryWithHints;

    EXPECT_EQ(handlerWithHints, reinterpret_cast<void *>(mockPageFaultManager->gpuDomainHandler));

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListMemAdvisePageFault, givenValidPtrAndPageFaultHandlerAndGpuDomainHandlerWithHintsSetThenHandlerBlocksCpuMigration) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.readOnly);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.devicePreferredLocation);

    auto handlerWithHints = L0::transferAndUnprotectMemoryWithHints;

    EXPECT_EQ(handlerWithHints, reinterpret_cast<void *>(mockPageFaultManager->gpuDomainHandler));

    NEO::PageFaultManager::PageFaultData pageData;
    pageData.cmdQ = deviceImp;
    pageData.domain = NEO::PageFaultManager::AllocationDomain::gpu;
    mockPageFaultManager->gpuDomainHandler(mockPageFaultManager, ptr, pageData);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.cpuMigrationBlocked);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListMemAdvisePageFault, givenValidPtrAndPageFaultHandlerAndGpuDomainHandlerWithHintsSetAndOnlyReadOnlyOrDevicePreferredHintThenHandlerAllowsCpuMigration) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.readOnly);

    auto handlerWithHints = L0::transferAndUnprotectMemoryWithHints;

    EXPECT_EQ(handlerWithHints, reinterpret_cast<void *>(mockPageFaultManager->gpuDomainHandler));

    NEO::PageFaultManager::PageFaultData pageData;
    pageData.cmdQ = deviceImp;
    pageData.domain = NEO::PageFaultManager::AllocationDomain::gpu;
    pageData.unifiedMemoryManager = device->getDriverHandle()->getSvmAllocsManager();
    EXPECT_EQ(0u, device->getDriverHandle()->getSvmAllocsManager()->nonGpuDomainAllocs.size());
    mockPageFaultManager->gpuDomainHandler(mockPageFaultManager, ptr, pageData);
    EXPECT_EQ(1u, device->getDriverHandle()->getSvmAllocsManager()->nonGpuDomainAllocs.size());

    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.cpuMigrationBlocked);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_CLEAR_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.readOnly);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.devicePreferredLocation);

    mockPageFaultManager->gpuDomainHandler(mockPageFaultManager, ptr, pageData);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.cpuMigrationBlocked);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_CLEAR_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.devicePreferredLocation);

    mockPageFaultManager->gpuDomainHandler(mockPageFaultManager, ptr, pageData);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.cpuMigrationBlocked);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListMemAdvisePageFault, givenValidPtrAndPageFaultHandlerAndGpuDomainHandlerWithHintsSetAndWithPrintUsmSharedMigrationDebugKeyThenMessageIsPrinted) {
    DebugManagerStateRestore restorer;
    debugManager.flags.PrintUmdSharedMigration.set(1);

    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto res = context->allocSharedMem(device->toHandle(),
                                       &deviceDesc,
                                       &hostDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.readOnly);

    auto handlerWithHints = L0::transferAndUnprotectMemoryWithHints;

    EXPECT_EQ(handlerWithHints, reinterpret_cast<void *>(mockPageFaultManager->gpuDomainHandler));

    testing::internal::CaptureStdout(); // start capturing

    NEO::PageFaultManager::PageFaultData pageData;
    pageData.cmdQ = deviceImp;
    pageData.domain = NEO::PageFaultManager::AllocationDomain::gpu;
    pageData.unifiedMemoryManager = device->getDriverHandle()->getSvmAllocsManager();
    mockPageFaultManager->gpuDomainHandler(mockPageFaultManager, ptr, pageData);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.cpuMigrationBlocked);

    std::string output = testing::internal::GetCapturedStdout(); // stop capturing

    std::string expectedString = "UMD transferred shared allocation";
    uint32_t occurrences = 0u;
    uint32_t expectedOccurrences = 1u;
    size_t idx = output.find(expectedString);
    while (idx != std::string::npos) {
        occurrences++;
        idx = output.find(expectedString, idx + 1);
    }
    EXPECT_EQ(expectedOccurrences, occurrences);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListMemAdvisePageFault, givenValidPtrAndPageFaultHandlerAndGpuDomainHandlerWithHintsSetAndInvalidHintsThenHandlerAllowsCpuMigration) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_BIAS_CACHED);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.cachedMemory);

    auto handlerWithHints = L0::transferAndUnprotectMemoryWithHints;

    EXPECT_EQ(handlerWithHints, reinterpret_cast<void *>(mockPageFaultManager->gpuDomainHandler));

    NEO::PageFaultManager::PageFaultData pageData;
    pageData.cmdQ = deviceImp;
    pageData.domain = NEO::PageFaultManager::AllocationDomain::gpu;
    pageData.unifiedMemoryManager = device->getDriverHandle()->getSvmAllocsManager();
    mockPageFaultManager->gpuDomainHandler(mockPageFaultManager, ptr, pageData);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.cpuMigrationBlocked);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListMemAdvisePageFault, givenValidPtrAndPageFaultHandlerAndGpuDomainHandlerWithHintsSetAndCpuDomainThenHandlerAllowsCpuMigration) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.readOnly);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.devicePreferredLocation);

    auto handlerWithHints = L0::transferAndUnprotectMemoryWithHints;

    EXPECT_EQ(handlerWithHints, reinterpret_cast<void *>(mockPageFaultManager->gpuDomainHandler));

    NEO::PageFaultManager::PageFaultData pageData;
    pageData.cmdQ = deviceImp;
    pageData.domain = NEO::PageFaultManager::AllocationDomain::cpu;
    pageData.unifiedMemoryManager = device->getDriverHandle()->getSvmAllocsManager();
    mockPageFaultManager->gpuDomainHandler(mockPageFaultManager, ptr, pageData);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.cpuMigrationBlocked);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListMemAdvisePageFault, givenInvalidPtrAndPageFaultHandlerAndGpuDomainHandlerWithHintsSetThenHandlerAllowsCpuMigration) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    NEO::MemAdviseFlags flags;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    auto allocData = device->getDriverHandle()->getSvmAllocsManager()->getSVMAlloc(ptr);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_READ_MOSTLY);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.readOnly);

    res = commandList->appendMemAdvise(device, ptr, size, ZE_MEMORY_ADVICE_SET_PREFERRED_LOCATION);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(1, flags.devicePreferredLocation);

    auto handlerWithHints = L0::transferAndUnprotectMemoryWithHints;

    EXPECT_EQ(handlerWithHints, reinterpret_cast<void *>(mockPageFaultManager->gpuDomainHandler));

    NEO::PageFaultManager::PageFaultData pageData;
    pageData.cmdQ = deviceImp;
    pageData.domain = NEO::PageFaultManager::AllocationDomain::gpu;
    pageData.unifiedMemoryManager = device->getDriverHandle()->getSvmAllocsManager();
    void *alloc = reinterpret_cast<void *>(0x1);
    mockPageFaultManager->gpuDomainHandler(mockPageFaultManager, alloc, pageData);
    flags = deviceImp->memAdviseSharedAllocations[allocData];
    EXPECT_EQ(0, flags.cpuMigrationBlocked);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListMemAdvisePageFault, givenUnifiedMemoryAllocWhenAllowCPUMemoryEvictionIsCalledThenSelectCorrectCsrWithOsContextForEviction) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    ze_host_mem_alloc_desc_t hostDesc = {};
    auto res = context->allocSharedMem(device->toHandle(),
                                       &deviceDesc,
                                       &hostDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    L0::DeviceImp *deviceImp = static_cast<L0::DeviceImp *>((L0::Device::fromHandle(device)));

    NEO::PageFaultManager::PageFaultData pageData;
    pageData.cmdQ = deviceImp;

    mockPageFaultManager->baseAllowCPUMemoryEviction(ptr, pageData);
    EXPECT_EQ(mockPageFaultManager->allowCPUMemoryEvictionImplCalled, 1);

    CommandStreamReceiver *csr = nullptr;
    if (deviceImp->getActiveDevice()->getInternalCopyEngine()) {
        csr = deviceImp->getActiveDevice()->getInternalCopyEngine()->commandStreamReceiver;
    } else {
        csr = deviceImp->getActiveDevice()->getInternalEngine().commandStreamReceiver;
    }
    ASSERT_NE(csr, nullptr);

    EXPECT_EQ(mockPageFaultManager->engineType, csr->getOsContext().getEngineType());
    EXPECT_EQ(mockPageFaultManager->engineUsage, csr->getOsContext().getEngineUsage());

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenValidPtrThenAppendMemoryPrefetchReturnsSuccess) {
    size_t size = 10;
    size_t alignment = 1u;
    void *ptr = nullptr;

    ze_device_mem_alloc_desc_t deviceDesc = {};
    auto res = context->allocDeviceMem(device->toHandle(),
                                       &deviceDesc,
                                       size, alignment, &ptr);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);
    EXPECT_NE(nullptr, ptr);

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    ASSERT_NE(nullptr, commandList);

    res = commandList->appendMemoryPrefetch(ptr, size);
    EXPECT_EQ(ZE_RESULT_SUCCESS, res);

    res = context->freeMem(ptr);
    ASSERT_EQ(res, ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, givenImmediateCommandListThenInternalEngineIsUsedIfRequested) {
    const ze_command_queue_desc_t desc = {};
    bool internalEngine = true;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::renderCompute,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList0);

    CommandQueueImp *cmdQueue = reinterpret_cast<CommandQueueImp *>(static_cast<CommandList *>(commandList0.get())->cmdQImmediate);
    EXPECT_EQ(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);

    internalEngine = false;

    std::unique_ptr<L0::CommandList> commandList1(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               internalEngine,
                                                                               NEO::EngineGroupType::renderCompute,
                                                                               returnValue));
    ASSERT_NE(nullptr, commandList1);

    cmdQueue = reinterpret_cast<CommandQueueImp *>(static_cast<CommandList *>(commandList1.get())->cmdQImmediate);
    EXPECT_NE(cmdQueue->getCsr(), neoDevice->getInternalEngine().commandStreamReceiver);
}

TEST_F(CommandListCreate, givenInternalUsageCommandListThenIsInternalReturnsTrue) {
    const ze_command_queue_desc_t desc = {};

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               true,
                                                                               NEO::EngineGroupType::renderCompute,
                                                                               returnValue));

    EXPECT_TRUE(commandList0->isInternal());
}

TEST_F(CommandListCreate, givenNonInternalUsageCommandListThenIsInternalReturnsFalse) {
    const ze_command_queue_desc_t desc = {};

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList0(CommandList::createImmediate(productFamily,
                                                                               device,
                                                                               &desc,
                                                                               false,
                                                                               NEO::EngineGroupType::renderCompute,
                                                                               returnValue));

    EXPECT_FALSE(commandList0->isInternal());
}

TEST_F(CommandListCreate, givenImmediateCommandListThenCustomNumIddPerBlockUsed) {
    const ze_command_queue_desc_t desc = {};

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    const uint32_t cmdListImmediateIdds = CommandList::commandListimmediateIddsPerBlock;
    EXPECT_EQ(cmdListImmediateIdds, commandList->getCmdContainer().getNumIddPerBlock());
}

TEST_F(CommandListCreate, whenCreatingImmediateCommandListThenItHasImmediateCommandQueueCreated) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, static_cast<CommandList *>(commandList.get())->cmdQImmediate);
}

TEST_F(CommandListCreate, whenCreatingImmediateCommandListWithSyncModeThenItHasImmediateCommandQueueCreated) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, static_cast<CommandList *>(commandList.get())->cmdQImmediate);
}

TEST_F(CommandListCreate, whenCreatingImmediateCommandListWithASyncModeThenItHasImmediateCommandQueueCreated) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, static_cast<CommandList *>(commandList.get())->cmdQImmediate);
}

TEST_F(CommandListCreate, givenAsynchronousOverrideWhenCreatingImmediateCommandListWithSyncModeThenAynchronousCommandQueueIsCreated) {
    DebugManagerStateRestore restore;
    debugManager.flags.OverrideImmediateCmdListSynchronousMode.set(2);
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);
    EXPECT_FALSE(whiteBoxCmdList->isSyncModeQueue);
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS, static_cast<CommandQueueImp *>(whiteBoxCmdList->cmdQImmediate)->getCommandQueueMode());
}

TEST_F(CommandListCreate, givenSynchronousOverrideWhenCreatingImmediateCommandListWithAsyncModeThenSynchronousCommandQueueIsCreated) {
    DebugManagerStateRestore restore;
    debugManager.flags.OverrideImmediateCmdListSynchronousMode.set(1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);
    EXPECT_TRUE(whiteBoxCmdList->isSyncModeQueue);
    EXPECT_EQ(ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS, static_cast<CommandQueueImp *>(whiteBoxCmdList->cmdQImmediate)->getCommandQueueMode());
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithSyncModeAndAppendSignalEventThenUpdateTaskCountNeededFlagIsDisabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, static_cast<CommandList *>(commandList.get())->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<Event> eventObject(static_cast<Event *>(L0::Event::fromHandle(event)));
    ASSERT_NE(nullptr, eventObject->csrs[0]);
    ASSERT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csrs[0]);

    commandList->appendSignalEvent(event);

    auto result = eventObject->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(eventObject->queryStatus(), ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithSyncModeAndAppendBarrierThenUpdateTaskCountNeededFlagIsDisabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, static_cast<CommandList *>(commandList.get())->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<Event> eventObject(static_cast<Event *>(L0::Event::fromHandle(event)));
    ASSERT_NE(nullptr, eventObject->csrs[0]);
    ASSERT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csrs[0]);

    commandList->appendBarrier(nullptr, 1, &event, false);

    auto result = eventObject->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(eventObject->queryStatus(), ZE_RESULT_SUCCESS);

    commandList->appendBarrier(nullptr, 0, nullptr, false);
}

HWTEST2_F(CommandListCreate, givenDirectSubmissionAndImmCmdListWhenDispatchingThenPassStallingCmdsInfo, IsAtLeastXeHpcCore) {
    bool useImmediateFlushTask = getHelper<L0GfxCoreHelper>().platformSupportsImmediateComputeFlushTask();

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &event));
    std::unique_ptr<L0::Event> eventObject(L0::Event::fromHandle(event));

    Mock<::L0::KernelImp> kernel;
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    uint8_t srcPtr[64] = {};
    uint8_t dstPtr[64] = {};
    const ze_copy_region_t region = {0U, 0U, 0U, 1, 1, 0U};

    driverHandle->importExternalPointer(dstPtr, MemoryConstants::pageSize);

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->csr);
    ultCsr->recordFlusheBatchBuffer = true;

    auto verifyFlags = [&ultCsr, useImmediateFlushTask](ze_result_t result, bool dispatchFlag, bool bbFlag) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        if (useImmediateFlushTask) {
            EXPECT_EQ(ultCsr->recordedImmediateDispatchFlags.hasStallingCmds, dispatchFlag);
        } else {
            EXPECT_EQ(ultCsr->recordedDispatchFlags.hasStallingCmds, dispatchFlag);
        }
        EXPECT_EQ(ultCsr->latestFlushedBatchBuffer.hasStallingCmds, bbFlag);
    };
    // non-pipelined state
    verifyFlags(commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false), false, true);

    // non-pipelined state already programmed
    verifyFlags(commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false), false, false);

    verifyFlags(commandList->appendLaunchKernelIndirect(kernel.toHandle(), groupCount, nullptr, 0, nullptr, false), false, false);

    verifyFlags(commandList->appendBarrier(nullptr, 0, nullptr, false), true, true);

    verifyFlags(commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, false, false), false, false);

    verifyFlags(commandList->appendMemoryCopyRegion(dstPtr, &region, 0, 0, srcPtr, &region, 0, 0, nullptr, 0, nullptr, false, false), false, false);

    verifyFlags(commandList->appendMemoryFill(dstPtr, srcPtr, 8, 1, nullptr, 0, nullptr, false), false, false);

    verifyFlags(commandList->appendEventReset(event), true, true);

    verifyFlags(commandList->appendSignalEvent(event), true, true);

    verifyFlags(commandList->appendPageFaultCopy(kernel.getIsaAllocation(), kernel.getIsaAllocation(), 1, false), false, false);

    verifyFlags(commandList->appendWaitOnEvents(1, &event, nullptr, false, true, false, false), true, true);

    verifyFlags(commandList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(dstPtr), nullptr, 0, nullptr), true, true);

    if constexpr (FamilyType::supportsSampler) {
        auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion);
        auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
        mockBuiltinKernel->setArgRedescribedImageCallBase = false;

        auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
        ze_image_region_t imgRegion = {1, 1, 1, 1, 1, 1};
        ze_image_desc_t zeDesc = {};
        zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
        image->initialize(device, &zeDesc);
        auto bytesPerPixel = static_cast<uint32_t>(image->getImageInfo().surfaceFormat->imageElementSizeInBytes);

        verifyFlags(commandList->appendImageCopyRegion(image->toHandle(), image->toHandle(), &imgRegion, &imgRegion, nullptr, 0, nullptr, false), false, false);

        verifyFlags(commandList->appendImageCopyFromMemory(image->toHandle(), dstPtr, &imgRegion, nullptr, 0, nullptr, false), false, false);

        verifyFlags(commandList->appendImageCopyToMemory(dstPtr, image->toHandle(), &imgRegion, nullptr, 0, nullptr, false), false, false);

        verifyFlags(commandList->appendImageCopyFromMemoryExt(image->toHandle(), dstPtr, &imgRegion, bytesPerPixel, bytesPerPixel, nullptr, 0, nullptr, false), false, false);

        verifyFlags(commandList->appendImageCopyToMemoryExt(dstPtr, image->toHandle(), &imgRegion, bytesPerPixel, bytesPerPixel, nullptr, 0, nullptr, false), false, false);
    }

    size_t rangeSizes = 1;
    const void **ranges = reinterpret_cast<const void **>(&dstPtr[0]);
    verifyFlags(commandList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, nullptr, 0, nullptr), true, true);

    const auto &productHelper = device->getProductHelper();

    bool stallingCmdRequired = productHelper.isComputeDispatchAllWalkerEnableInCfeStateRequired(device->getHwInfo());

    verifyFlags(commandList->appendLaunchCooperativeKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, false), false, stallingCmdRequired);

    verifyFlags(commandList->appendLaunchCooperativeKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, false), false, false);

    driverHandle->releaseImportedPointer(dstPtr);
}

HWTEST2_F(CommandListCreate, givenDirectSubmissionAndImmCmdListWhenDispatchingDisabledRelaxedOrderingThenPassStallingCmdsInfo, IsAtLeastXeHpcCore) {
    bool useImmediateFlushTask = getHelper<L0GfxCoreHelper>().platformSupportsImmediateComputeFlushTask();

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    auto commandList = zeUniquePtr(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &event));
    std::unique_ptr<L0::Event> eventObject(L0::Event::fromHandle(event));

    Mock<::L0::KernelImp> kernel;
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    uint8_t srcPtr[64] = {};
    uint8_t dstPtr[64] = {};
    const ze_copy_region_t region = {0U, 0U, 0U, 1, 1, 0U};

    driverHandle->importExternalPointer(dstPtr, MemoryConstants::pageSize);

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->csr);
    ultCsr->recordFlusheBatchBuffer = true;

    EXPECT_FALSE(NEO::RelaxedOrderingHelper::isRelaxedOrderingDispatchAllowed(*ultCsr, 1));

    auto verifyFlags = [&ultCsr, useImmediateFlushTask](ze_result_t result) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        if (useImmediateFlushTask) {
            EXPECT_TRUE(ultCsr->recordedImmediateDispatchFlags.hasStallingCmds);
        } else {
            EXPECT_TRUE(ultCsr->recordedDispatchFlags.hasStallingCmds);
        }
        EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.hasStallingCmds);
    };

    auto resetFlags = [&ultCsr, useImmediateFlushTask]() {
        if (useImmediateFlushTask) {
            ultCsr->recordedImmediateDispatchFlags.hasStallingCmds = false;
        } else {
            ultCsr->recordedDispatchFlags.hasStallingCmds = false;
        }
        ultCsr->latestFlushedBatchBuffer.hasStallingCmds = false;
    };

    bool inOrderExecAlreadyEnabled = false;

    for (bool inOrderExecution : {false, true}) {
        if (inOrderExecution && !inOrderExecAlreadyEnabled) {
            whiteBoxCmdList->enableInOrderExecution();
            inOrderExecAlreadyEnabled = true;
        }

        EXPECT_EQ(inOrderExecAlreadyEnabled, inOrderExecution);

        uint32_t numWaitEvents = inOrderExecution ? 0 : 1;
        ze_event_handle_t *waitlist = inOrderExecution ? nullptr : &event;

        // non-pipelined state or first in-order exec
        resetFlags();
        verifyFlags(commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 1, &event, launchParams, false));

        // non-pipelined state already programmed
        resetFlags();
        verifyFlags(commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, numWaitEvents, waitlist, launchParams, false));

        resetFlags();
        verifyFlags(commandList->appendLaunchKernelIndirect(kernel.toHandle(), groupCount, nullptr, numWaitEvents, waitlist, false));

        resetFlags();
        verifyFlags(commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, numWaitEvents, waitlist, false, false));

        resetFlags();
        verifyFlags(commandList->appendMemoryCopyRegion(dstPtr, &region, 0, 0, srcPtr, &region, 0, 0, nullptr, numWaitEvents, waitlist, false, false));

        resetFlags();
        verifyFlags(commandList->appendMemoryFill(dstPtr, srcPtr, 8, 1, nullptr, numWaitEvents, waitlist, false));

        if constexpr (FamilyType::supportsSampler) {
            auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion);
            auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
            mockBuiltinKernel->setArgRedescribedImageCallBase = false;

            auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
            ze_image_region_t imgRegion = {1, 1, 1, 1, 1, 1};
            ze_image_desc_t zeDesc = {};
            zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
            image->initialize(device, &zeDesc);
            auto bytesPerPixel = static_cast<uint32_t>(image->getImageInfo().surfaceFormat->imageElementSizeInBytes);

            resetFlags();
            verifyFlags(commandList->appendImageCopyRegion(image->toHandle(), image->toHandle(), &imgRegion, &imgRegion, nullptr, numWaitEvents, waitlist, false));

            resetFlags();
            verifyFlags(commandList->appendImageCopyFromMemory(image->toHandle(), dstPtr, &imgRegion, nullptr, numWaitEvents, waitlist, false));

            resetFlags();
            verifyFlags(commandList->appendImageCopyToMemory(dstPtr, image->toHandle(), &imgRegion, nullptr, numWaitEvents, waitlist, false));

            resetFlags();
            verifyFlags(commandList->appendImageCopyFromMemoryExt(image->toHandle(), dstPtr, &imgRegion, bytesPerPixel, bytesPerPixel, nullptr, numWaitEvents, waitlist, false));

            resetFlags();
            verifyFlags(commandList->appendImageCopyToMemoryExt(dstPtr, image->toHandle(), &imgRegion, bytesPerPixel, bytesPerPixel, nullptr, numWaitEvents, waitlist, false));
        }

        resetFlags();
        verifyFlags(commandList->appendLaunchCooperativeKernel(kernel.toHandle(), groupCount, nullptr, numWaitEvents, waitlist, false));
    }

    driverHandle->releaseImportedPointer(dstPtr);
}

HWTEST2_F(CommandListCreate, whenDispatchingThenPassNumCsrClients, IsAtLeastXeHpcCore) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    Mock<::L0::KernelImp> kernel;
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->csr);
    ultCsr->recordFlusheBatchBuffer = true;

    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(ultCsr->latestFlushedBatchBuffer.numCsrClients, ultCsr->getNumClients());
}

HWTEST_F(CommandListCreate, givenSignalEventWhenCallingSynchronizeThenUnregisterClient) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    Mock<::L0::KernelImp> kernel;
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->csr);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 3;

    ze_event_desc_t eventDesc = {};

    ze_event_handle_t event1 = nullptr;
    ze_event_handle_t event2 = nullptr;
    ze_event_handle_t event3 = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &event1));
    ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &event2));
    ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &event3));

    EXPECT_EQ(ultCsr->getNumClients(), 0u);

    {
        commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event1, 0, nullptr, launchParams, false);
        EXPECT_EQ(ultCsr->getNumClients(), 1u);

        Event::fromHandle(event1)->setIsCompleted();

        zeEventHostSynchronize(event1, std::numeric_limits<uint64_t>::max());
        EXPECT_EQ(ultCsr->getNumClients(), 0u);
    }

    {
        commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event2, 0, nullptr, launchParams, false);
        EXPECT_EQ(ultCsr->getNumClients(), 1u);

        *reinterpret_cast<uint32_t *>(Event::fromHandle(event2)->getHostAddress()) = static_cast<uint32_t>(Event::STATE_SIGNALED);

        zeEventHostSynchronize(event2, std::numeric_limits<uint64_t>::max());
        EXPECT_EQ(ultCsr->getNumClients(), 0u);
    }

    {
        commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event3, 0, nullptr, launchParams, false);
        EXPECT_EQ(ultCsr->getNumClients(), 1u);

        zeEventHostReset(event3);

        zeEventHostSynchronize(event3, 1);
        EXPECT_EQ(ultCsr->getNumClients(), 0u);
    }

    zeEventDestroy(event1);
    zeEventDestroy(event2);
    zeEventDestroy(event3);
}

HWTEST_F(CommandListCreate, givenDebugFlagSetWhenCallingSynchronizeThenDontUnregister) {
    DebugManagerStateRestore restore;
    debugManager.flags.TrackNumCsrClientsOnSyncPoints.set(0);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    Mock<::L0::KernelImp> kernel;
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->csr);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;

    ze_event_desc_t eventDesc = {};

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));

    ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &event));

    EXPECT_EQ(ultCsr->getNumClients(), 0u);
    commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event, 0, nullptr, launchParams, false);
    EXPECT_EQ(ultCsr->getNumClients(), 1u);

    Event::fromHandle(event)->setIsCompleted();

    zeEventHostSynchronize(event, std::numeric_limits<uint64_t>::max());

    EXPECT_EQ(ultCsr->getNumClients(), 1u);

    zeEventDestroy(event);
}

HWTEST2_F(CommandListCreate, givenDirectSubmissionAndImmCmdListWhenDispatchingThenPassRelaxedOrderingDependenciesInfo, IsAtLeastXeHpcCore) {
    bool useImmediateFlushTask = getHelper<L0GfxCoreHelper>().platformSupportsImmediateComputeFlushTask();

    DebugManagerStateRestore restore;
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &event));
    std::unique_ptr<L0::Event> eventObject(L0::Event::fromHandle(event));

    Mock<::L0::KernelImp> kernel;
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    uint8_t srcPtr[64] = {};
    uint8_t dstPtr[64] = {};
    const ze_copy_region_t region = {0U, 0U, 0U, 1, 1, 0U};

    driverHandle->importExternalPointer(dstPtr, MemoryConstants::pageSize);

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->csr);
    ultCsr->recordFlusheBatchBuffer = true;

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto verifyFlags = [&ultCsr, useImmediateFlushTask](ze_result_t result, bool dispatchFlag, bool bbFlag) {
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        if (useImmediateFlushTask) {
            EXPECT_EQ(ultCsr->recordedImmediateDispatchFlags.hasRelaxedOrderingDependencies, dispatchFlag);
        } else {
            EXPECT_EQ(ultCsr->recordedDispatchFlags.hasRelaxedOrderingDependencies, dispatchFlag);
        }
        EXPECT_EQ(ultCsr->latestFlushedBatchBuffer.hasRelaxedOrderingDependencies, bbFlag);
    };

    for (bool hasEventDependencies : {true, false}) {
        ze_event_handle_t *waitlist = hasEventDependencies ? &event : nullptr;
        uint32_t numWaitlistEvents = hasEventDependencies ? 1 : 0;

        verifyFlags(commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, numWaitlistEvents, waitlist, launchParams, false),
                    hasEventDependencies, hasEventDependencies);

        verifyFlags(commandList->appendLaunchKernelIndirect(kernel.toHandle(), groupCount, nullptr, numWaitlistEvents, waitlist, false),
                    hasEventDependencies, hasEventDependencies);

        verifyFlags(commandList->appendBarrier(nullptr, numWaitlistEvents, waitlist, false),
                    false, false);

        verifyFlags(commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, numWaitlistEvents, waitlist, false, false),
                    hasEventDependencies, hasEventDependencies);

        verifyFlags(commandList->appendMemoryCopyRegion(dstPtr, &region, 0, 0, srcPtr, &region, 0, 0, nullptr, numWaitlistEvents, waitlist, false, false),
                    hasEventDependencies, hasEventDependencies);

        verifyFlags(commandList->appendMemoryFill(dstPtr, srcPtr, 8, 1, nullptr, numWaitlistEvents, waitlist, false),
                    hasEventDependencies, hasEventDependencies);

        verifyFlags(commandList->appendEventReset(event), false, false);

        verifyFlags(commandList->appendSignalEvent(event), false, false);

        verifyFlags(commandList->appendPageFaultCopy(kernel.getIsaAllocation(), kernel.getIsaAllocation(), 1, false),
                    false, false);

        verifyFlags(commandList->appendWaitOnEvents(1, &event, nullptr, false, true, false, false), false, false);

        verifyFlags(commandList->appendWriteGlobalTimestamp(reinterpret_cast<uint64_t *>(dstPtr), nullptr, numWaitlistEvents, waitlist),
                    false, false);

        if constexpr (FamilyType::supportsSampler) {
            auto kernel = device->getBuiltinFunctionsLib()->getImageFunction(ImageBuiltin::copyImageRegion);
            auto mockBuiltinKernel = static_cast<Mock<::L0::KernelImp> *>(kernel);
            mockBuiltinKernel->setArgRedescribedImageCallBase = false;

            auto image = std::make_unique<WhiteBox<::L0::ImageCoreFamily<gfxCoreFamily>>>();
            ze_image_region_t imgRegion = {1, 1, 1, 1, 1, 1};
            ze_image_desc_t zeDesc = {};
            zeDesc.stype = ZE_STRUCTURE_TYPE_IMAGE_DESC;
            image->initialize(device, &zeDesc);
            auto bytesPerPixel = static_cast<uint32_t>(image->getImageInfo().surfaceFormat->imageElementSizeInBytes);

            verifyFlags(commandList->appendImageCopyRegion(image->toHandle(), image->toHandle(), &imgRegion, &imgRegion, nullptr, numWaitlistEvents, waitlist, false),
                        hasEventDependencies, hasEventDependencies);

            verifyFlags(commandList->appendImageCopyFromMemory(image->toHandle(), dstPtr, &imgRegion, nullptr, numWaitlistEvents, waitlist, false),
                        hasEventDependencies, hasEventDependencies);

            verifyFlags(commandList->appendImageCopyToMemory(dstPtr, image->toHandle(), &imgRegion, nullptr, numWaitlistEvents, waitlist, false),
                        hasEventDependencies, hasEventDependencies);

            verifyFlags(commandList->appendImageCopyFromMemoryExt(image->toHandle(), dstPtr, &imgRegion, bytesPerPixel, bytesPerPixel, nullptr, numWaitlistEvents, waitlist, false),
                        hasEventDependencies, hasEventDependencies);

            verifyFlags(commandList->appendImageCopyToMemoryExt(dstPtr, image->toHandle(), &imgRegion, bytesPerPixel, bytesPerPixel, nullptr, numWaitlistEvents, waitlist, false),
                        hasEventDependencies, hasEventDependencies);
        }

        size_t rangeSizes = 1;
        const void **ranges = reinterpret_cast<const void **>(&dstPtr[0]);
        verifyFlags(commandList->appendMemoryRangesBarrier(1, &rangeSizes, ranges, nullptr, numWaitlistEvents, waitlist),
                    false, false);
    }

    for (bool hasEventDependencies : {true, false}) {
        ze_event_handle_t *waitlist = hasEventDependencies ? &event : nullptr;
        uint32_t numWaitlistEvents = hasEventDependencies ? 1 : 0;
        verifyFlags(commandList->appendLaunchCooperativeKernel(kernel.toHandle(), groupCount, nullptr, numWaitlistEvents, waitlist, false),
                    hasEventDependencies, hasEventDependencies);
    }

    driverHandle->releaseImportedPointer(dstPtr);
}

HWTEST2_F(CommandListCreate, givenInOrderExecutionWhenDispatchingRelaxedOrderingWithoutInputEventsThenCountPreviousEventAsWaitlist, IsAtLeastXeHpcCore) {
    bool useImmediateFlushTask = getHelper<L0GfxCoreHelper>().platformSupportsImmediateComputeFlushTask();

    DebugManagerStateRestore restore;
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    auto commandList = zeUniquePtr(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());
    whiteBoxCmdList->enableInOrderExecution();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &event));
    std::unique_ptr<L0::Event> eventObject(L0::Event::fromHandle(event));

    Mock<::L0::KernelImp> kernel;
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->csr);
    ultCsr->recordFlusheBatchBuffer = true;

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    commandList->appendLaunchKernel(kernel.toHandle(), groupCount, event, 0, nullptr, launchParams, false);

    commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    if (useImmediateFlushTask) {
        EXPECT_TRUE(ultCsr->recordedImmediateDispatchFlags.hasRelaxedOrderingDependencies);
    } else {
        EXPECT_TRUE(ultCsr->recordedDispatchFlags.hasRelaxedOrderingDependencies);
    }
    EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);
}

HWTEST2_F(CommandListCreate, givenInOrderExecutionWhenDispatchingBarrierThenAllowForRelaxedOrdering, IsAtLeastXeHpcCore) {
    bool useImmediateFlushTask = getHelper<L0GfxCoreHelper>().platformSupportsImmediateComputeFlushTask();

    DebugManagerStateRestore restore;
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    auto commandList = zeUniquePtr(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());
    whiteBoxCmdList->enableInOrderExecution();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &event));
    std::unique_ptr<L0::Event> eventObject(L0::Event::fromHandle(event));

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->csr);
    ultCsr->recordFlusheBatchBuffer = true;

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    // Initialize NP state
    commandList->appendBarrier(nullptr, 1, &event, false);

    if (useImmediateFlushTask) {
        EXPECT_TRUE(ultCsr->recordedImmediateDispatchFlags.hasRelaxedOrderingDependencies);
        EXPECT_FALSE(ultCsr->recordedImmediateDispatchFlags.hasStallingCmds);
    } else {
        EXPECT_TRUE(ultCsr->recordedDispatchFlags.hasRelaxedOrderingDependencies);
        EXPECT_FALSE(ultCsr->recordedDispatchFlags.hasStallingCmds);
    }
    EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);
    EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.hasStallingCmds);

    commandList->appendBarrier(nullptr, 1, &event, false);

    if (useImmediateFlushTask) {
        EXPECT_TRUE(ultCsr->recordedImmediateDispatchFlags.hasRelaxedOrderingDependencies);
        EXPECT_FALSE(ultCsr->recordedImmediateDispatchFlags.hasStallingCmds);
    } else {
        EXPECT_TRUE(ultCsr->recordedDispatchFlags.hasRelaxedOrderingDependencies);
        EXPECT_FALSE(ultCsr->recordedDispatchFlags.hasStallingCmds);
    }
    EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);
    EXPECT_FALSE(ultCsr->latestFlushedBatchBuffer.hasStallingCmds);
}

HWTEST2_F(CommandListCreate, givenInOrderExecutionWhenDispatchingBarrierWithFlushAndWithoutDependenciesThenDontMarkAsStalling, IsAtLeastXeHpcCore) {
    bool useImmediateFlushTask = getHelper<L0GfxCoreHelper>().platformSupportsImmediateComputeFlushTask();

    DebugManagerStateRestore restore;
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    auto commandList0 = zeUniquePtr(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto commandList = zeUniquePtr(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());
    whiteBoxCmdList->enableInOrderExecution();

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    ASSERT_EQ(ZE_RESULT_SUCCESS, eventPool->createEvent(&eventDesc, &event));
    std::unique_ptr<L0::Event> eventObject(L0::Event::fromHandle(event));

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(whiteBoxCmdList->csr);
    ultCsr->recordFlusheBatchBuffer = true;

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    // Initialize NP state
    commandList0->appendBarrier(nullptr, 1, &event, false);

    if (useImmediateFlushTask) {
        EXPECT_FALSE(ultCsr->recordedImmediateDispatchFlags.hasRelaxedOrderingDependencies);
        EXPECT_TRUE(ultCsr->recordedImmediateDispatchFlags.hasStallingCmds);
    } else {
        EXPECT_TRUE(ultCsr->recordedDispatchFlags.hasRelaxedOrderingDependencies);
        EXPECT_FALSE(ultCsr->recordedDispatchFlags.hasStallingCmds);
    }
    EXPECT_FALSE(ultCsr->latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);
    EXPECT_TRUE(ultCsr->latestFlushedBatchBuffer.hasStallingCmds);

    ultCsr->unregisterClient(&client1);
    ultCsr->unregisterClient(&client2);

    commandList->appendBarrier(event, 0, nullptr, false);

    if (useImmediateFlushTask) {
        EXPECT_FALSE(ultCsr->recordedImmediateDispatchFlags.hasRelaxedOrderingDependencies);
        EXPECT_FALSE(ultCsr->recordedImmediateDispatchFlags.hasStallingCmds);
    } else {
        EXPECT_FALSE(ultCsr->recordedDispatchFlags.hasRelaxedOrderingDependencies);
        EXPECT_FALSE(ultCsr->recordedDispatchFlags.hasStallingCmds);
    }
    EXPECT_FALSE(ultCsr->latestFlushedBatchBuffer.hasRelaxedOrderingDependencies);
    EXPECT_FALSE(ultCsr->latestFlushedBatchBuffer.hasStallingCmds);
}

HWTEST2_F(CommandListCreate, givenInOrderExecutionWhenDispatchingRelaxedOrderingThenProgramConditionalBbStart, IsAtLeastXeHpcCore) {
    using MI_LOAD_REGISTER_REG = typename FamilyType::MI_LOAD_REGISTER_REG;

    DebugManagerStateRestore restore;
    debugManager.flags.DirectSubmissionRelaxedOrdering.set(1);

    auto ultCsr = static_cast<NEO::UltCommandStreamReceiver<FamilyType> *>(device->getNEODevice()->getDefaultEngine().commandStreamReceiver);

    ze_command_queue_desc_t desc = {};

    auto mockCmdQ = std::make_unique<Mock<CommandQueue>>(device, ultCsr, &desc);

    auto cmdList = makeZeUniquePtr<WhiteBox<L0::CommandListCoreFamilyImmediate<gfxCoreFamily>>>();

    cmdList->cmdQImmediate = mockCmdQ.get();
    cmdList->isFlushTaskSubmissionEnabled = true;
    cmdList->cmdListType = CommandList::CommandListType::typeImmediate;
    cmdList->csr = ultCsr;
    cmdList->initialize(device, NEO::EngineGroupType::renderCompute, 0u);
    cmdList->commandContainer.setImmediateCmdListCsr(ultCsr);
    cmdList->enableInOrderExecution();

    Mock<::L0::KernelImp> kernel;
    ze_group_count_t groupCount{1, 1, 1};
    CmdListKernelLaunchParams launchParams = {};

    ultCsr->recordFlusheBatchBuffer = true;

    auto directSubmission = new MockDirectSubmissionHw<FamilyType, RenderDispatcher<FamilyType>>(*ultCsr);
    ultCsr->directSubmission.reset(directSubmission);
    int client1, client2;
    ultCsr->registerClient(&client1);
    ultCsr->registerClient(&client2);

    auto cmdStream = cmdList->getCmdContainer().getCommandStream();

    cmdList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
    cmdList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    size_t offset = cmdStream->getUsed();

    cmdList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);

    GenCmdList genCmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        genCmdList,
        ptrOffset(cmdStream->getCpuBase(), offset),
        cmdStream->getUsed() - offset));

    // init registers
    auto lrrCmd = genCmdCast<MI_LOAD_REGISTER_REG *>(*genCmdList.begin());
    ASSERT_NE(nullptr, lrrCmd);
    lrrCmd++;
    lrrCmd++;

    EXPECT_TRUE(RelaxedOrderingCommandsHelper::verifyConditionalDataMemBbStart<FamilyType>(lrrCmd, 0, cmdList->inOrderExecInfo->getBaseDeviceAddress(), 2,
                                                                                           NEO::CompareOperation::less, true, cmdList->isQwordInOrderCounter()));
}

TEST_F(CommandListCreate, GivenGpuHangWhenCreatingImmCmdListWithSyncModeAndAppendBarrierThenAppendBarrierReturnsDeviceLost) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    mockCommandStreamReceiver.waitForCompletionWithTimeoutReturnValue = WaitStatus::gpuHang;

    const auto oldCsr = whiteBoxCmdList->csr;
    whiteBoxCmdList->csr = &mockCommandStreamReceiver;
    static_cast<WhiteBox<::L0::CommandQueue> *>(whiteBoxCmdList->cmdQImmediate)->csr = &mockCommandStreamReceiver;

    const auto appendBarrierResult = commandList->appendBarrier(nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, appendBarrierResult);

    whiteBoxCmdList->csr = oldCsr;
    static_cast<WhiteBox<::L0::CommandQueue> *>(whiteBoxCmdList->cmdQImmediate)->csr = oldCsr;
}

TEST_F(CommandListCreate, givenSplitBcsSizeWhenCreateCommandListThenProperSizeSet) {
    DebugManagerStateRestore restorer;
    debugManager.flags.SplitBcsSize.set(120);

    ze_command_queue_desc_t desc = {};

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);
    EXPECT_EQ(whiteBoxCmdList->minimalSizeForBcsSplit, 120 * MemoryConstants::kiloByte);
}

HWTEST_F(CommandListCreate, GivenGpuHangWhenCreatingImmediateCommandListAndAppendingSignalEventsThenDeviceLostIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));

    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<Event> eventObject(static_cast<Event *>(L0::Event::fromHandle(event)));
    ASSERT_NE(nullptr, eventObject->csrs[0]);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csrs[0]);

    returnValue = commandList->appendWaitOnEvents(1, &event, nullptr, false, true, false, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    returnValue = commandList->appendBarrier(nullptr, 1, &event, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    mockCommandStreamReceiver.waitForCompletionWithTimeoutReturnValue = WaitStatus::gpuHang;

    const auto oldCsr = whiteBoxCmdList->csr;
    whiteBoxCmdList->csr = &mockCommandStreamReceiver;
    static_cast<WhiteBox<::L0::CommandQueue> *>(whiteBoxCmdList->cmdQImmediate)->csr = &mockCommandStreamReceiver;

    returnValue = commandList->appendSignalEvent(event);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, returnValue);

    whiteBoxCmdList->csr = oldCsr;
    static_cast<WhiteBox<::L0::CommandQueue> *>(whiteBoxCmdList->cmdQImmediate)->csr = oldCsr;
}

HWTEST2_F(CommandListCreate, GivenGpuHangOnExecutingCommandListsWhenCreatingImmediateCommandListAndWaitingOnEventsThenDeviceLostIsReturned, IsSKL) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));

    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<Event> eventObject(static_cast<Event *>(L0::Event::fromHandle(event)));
    ASSERT_NE(nullptr, eventObject->csrs[0]);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csrs[0]);

    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    mockCommandStreamReceiver.waitForCompletionWithTimeoutReturnValue = WaitStatus::gpuHang;

    const auto oldCsr = whiteBoxCmdList->csr;
    whiteBoxCmdList->csr = &mockCommandStreamReceiver;
    static_cast<WhiteBox<::L0::CommandQueue> *>(whiteBoxCmdList->cmdQImmediate)->csr = &mockCommandStreamReceiver;

    returnValue = commandList->appendWaitOnEvents(1, &event, nullptr, false, true, false, false);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, returnValue);

    whiteBoxCmdList->csr = oldCsr;
    static_cast<WhiteBox<::L0::CommandQueue> *>(whiteBoxCmdList->cmdQImmediate)->csr = oldCsr;
}

TEST_F(CommandListCreate, givenImmediateCommandListWhenThereIsNoEnoughSpaceForImmediateCommandThenNextCommandBufferIsUsed) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    whiteBoxCmdList->isFlushTaskSubmissionEnabled = true;

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);

    // reduce available cmd buffer size, so next command can't fit in 1st and we need to use 2nd cmd buffer
    size_t useSize = commandList->getCmdContainer().getCommandStream()->getMaxAvailableSpace() - commonImmediateCommandSize + 1;
    commandList->getCmdContainer().getCommandStream()->getSpace(useSize);
    EXPECT_EQ(1U, commandList->getCmdContainer().getCmdBufferAllocations().size());

    auto oldStreamPtr = commandList->getCmdContainer().getCommandStream()->getCpuBase();
    auto result = commandList->appendMemoryCopy(dstPtr, srcPtr, 8, nullptr, 0, nullptr, false, false);
    auto newStreamPtr = commandList->getCmdContainer().getCommandStream()->getCpuBase();

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(oldStreamPtr, newStreamPtr);
    EXPECT_EQ(1U, commandList->getCmdContainer().getCmdBufferAllocations().size());
    whiteBoxCmdList->csr->getInternalAllocationStorage()->getTemporaryAllocations().freeAllGraphicsAllocations(device->getNEODevice());
}

HWTEST2_F(CommandListCreate, GivenGpuHangOnSynchronizingWhenCreatingImmediateCommandListAndWaitingOnEventsThenDeviceLostIsReturned, IsSKL) {
    DebugManagerStateRestore restorer;

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));

    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<Event> eventObject(static_cast<Event *>(L0::Event::fromHandle(event)));
    ASSERT_NE(nullptr, eventObject->csrs[0]);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csrs[0]);

    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    mockCommandStreamReceiver.waitForCompletionWithTimeoutReturnValue = WaitStatus::gpuHang;
    const auto oldCsr = whiteBoxCmdList->csr;
    whiteBoxCmdList->csr = &mockCommandStreamReceiver;
    static_cast<WhiteBox<::L0::CommandQueue> *>(whiteBoxCmdList->cmdQImmediate)->csr = &mockCommandStreamReceiver;

    returnValue = commandList->appendWaitOnEvents(1, &event, nullptr, false, true, false, false);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, returnValue);
    whiteBoxCmdList->csr = oldCsr;
    static_cast<WhiteBox<::L0::CommandQueue> *>(whiteBoxCmdList->cmdQImmediate)->csr = oldCsr;
}

HWTEST2_F(CommandListCreate, GivenGpuHangOnSynchronizingWhenCreatingImmediateCommandListWithoutFlushTaskAndWaitingOnEventsThenDeviceLostIsReturnedFromExecute, IsSKL) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableFlushTaskSubmission.set(0);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));

    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<Event> eventObject(static_cast<Event *>(L0::Event::fromHandle(event)));
    ASSERT_NE(nullptr, eventObject->csrs[0]);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csrs[0]);

    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    Mock<CommandQueue> mockCommandQueue(device, &mockCommandStreamReceiver, &desc);
    mockCommandQueue.executeCommandListsResult = ZE_RESULT_ERROR_DEVICE_LOST;
    auto oldCommandQueue = whiteBoxCmdList->cmdQImmediate;
    whiteBoxCmdList->cmdQImmediate = &mockCommandQueue;

    returnValue = commandList->appendWaitOnEvents(1, &event, nullptr, false, true, false, false);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, returnValue);
    whiteBoxCmdList->cmdQImmediate = oldCommandQueue;
}

HWTEST2_F(CommandListCreate, GivenGpuHangOnSynchronizingWhenCreatingImmediateCommandListWithoutFlushTaskAndWaitingOnEventsThenDeviceLostIsReturnedFromSynchronize, IsSKL) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableFlushTaskSubmission.set(0);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));

    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<Event> eventObject(static_cast<Event *>(L0::Event::fromHandle(event)));
    ASSERT_NE(nullptr, eventObject->csrs[0]);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csrs[0]);

    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    Mock<CommandQueue> mockCommandQueue(device, &mockCommandStreamReceiver, &desc);
    mockCommandQueue.synchronizeResult = ZE_RESULT_ERROR_DEVICE_LOST;
    auto oldCommandQueue = whiteBoxCmdList->cmdQImmediate;
    whiteBoxCmdList->cmdQImmediate = &mockCommandQueue;

    returnValue = commandList->appendWaitOnEvents(1, &event, nullptr, false, true, false, false);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, returnValue);
    whiteBoxCmdList->cmdQImmediate = oldCommandQueue;
}

HWTEST_F(CommandListCreate, GivenGpuHangWhenCreatingImmediateCommandListAndAppendingEventResetThenDeviceLostIsReturned) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));

    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<Event> eventObject(static_cast<Event *>(L0::Event::fromHandle(event)));
    ASSERT_NE(nullptr, eventObject->csrs[0]);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csrs[0]);

    returnValue = commandList->appendWaitOnEvents(1, &event, nullptr, false, true, false, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    returnValue = commandList->appendBarrier(nullptr, 1, &event, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    returnValue = commandList->appendSignalEvent(event);
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    returnValue = eventObject->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    EXPECT_EQ(ZE_RESULT_SUCCESS, eventObject->queryStatus());

    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    mockCommandStreamReceiver.waitForCompletionWithTimeoutReturnValue = WaitStatus::gpuHang;

    const auto oldCsr = whiteBoxCmdList->csr;
    whiteBoxCmdList->csr = &mockCommandStreamReceiver;
    static_cast<WhiteBox<::L0::CommandQueue> *>(whiteBoxCmdList->cmdQImmediate)->csr = &mockCommandStreamReceiver;

    returnValue = commandList->appendEventReset(event);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, returnValue);

    whiteBoxCmdList->csr = oldCsr;
    static_cast<WhiteBox<::L0::CommandQueue> *>(whiteBoxCmdList->cmdQImmediate)->csr = oldCsr;
}

HWTEST_F(CommandListCreate, GivenImmediateCommandListWithFlushTaskCreatedThenNumIddPerBlockIsOne) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableFlushTaskSubmission.set(1);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));

    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    auto &commandContainer = commandList->getCmdContainer();

    EXPECT_EQ(1u, commandContainer.getNumIddPerBlock());
}

HWTEST_F(CommandListCreate, GivenGpuHangAndEnabledFlushTaskSubmissionFlagWhenCreatingImmediateCommandListAndAppendingWaitOnEventsThenDeviceLostIsReturned) {
    DebugManagerStateRestore restorer;
    NEO::debugManager.flags.EnableFlushTaskSubmission.set(true);

    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    ASSERT_EQ(ZE_RESULT_SUCCESS, returnValue);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<Event> eventObject(static_cast<Event *>(L0::Event::fromHandle(event)));
    ASSERT_NE(nullptr, eventObject->csrs[0]);
    ASSERT_EQ(static_cast<DeviceImp *>(device)->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csrs[0]);

    MockCommandStreamReceiver mockCommandStreamReceiver(*neoDevice->executionEnvironment, neoDevice->getRootDeviceIndex(), neoDevice->getDeviceBitfield());
    mockCommandStreamReceiver.waitForCompletionWithTimeoutReturnValue = WaitStatus::gpuHang;

    const auto oldCsr = whiteBoxCmdList->csr;
    whiteBoxCmdList->csr = &mockCommandStreamReceiver;
    static_cast<WhiteBox<::L0::CommandQueue> *>(whiteBoxCmdList->cmdQImmediate)->csr = &mockCommandStreamReceiver;

    returnValue = commandList->appendWaitOnEvents(1, &event, nullptr, false, true, false, false);
    EXPECT_EQ(ZE_RESULT_ERROR_DEVICE_LOST, returnValue);

    whiteBoxCmdList->csr = oldCsr;
    static_cast<WhiteBox<::L0::CommandQueue> *>(whiteBoxCmdList->cmdQImmediate)->csr = oldCsr;
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithSyncModeAndAppendResetEventThenUpdateTaskCountNeededFlagIsDisabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE | ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<Event> eventObject(static_cast<Event *>(L0::Event::fromHandle(event)));
    ASSERT_NE(nullptr, eventObject->csrs[0]);
    ASSERT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csrs[0]);

    commandList->appendEventReset(event);

    auto result = eventObject->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(eventObject->queryStatus(), ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithASyncModeAndAppendSignalEventThenUpdateTaskCountNeededFlagIsEnabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = 0;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<Event> eventObject(static_cast<Event *>(L0::Event::fromHandle(event)));
    ASSERT_NE(nullptr, eventObject->csrs[0]);
    ASSERT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csrs[0]);

    commandList->appendSignalEvent(event);

    auto result = eventObject->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(eventObject->queryStatus(), ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithASyncModeAndAppendBarrierThenUpdateTaskCountNeededFlagIsEnabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<Event> eventObject(static_cast<Event *>(L0::Event::fromHandle(event)));
    ASSERT_NE(nullptr, eventObject->csrs[0]);
    ASSERT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csrs[0]);

    commandList->appendBarrier(event, 0, nullptr, false);

    auto result = eventObject->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(eventObject->queryStatus(), ZE_RESULT_SUCCESS);

    commandList->appendBarrier(nullptr, 0, nullptr, false);
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithASyncModeAndCopyEngineAndAppendBarrierThenUpdateTaskCountNeededFlagIsEnabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<Event> eventObject(static_cast<Event *>(L0::Event::fromHandle(event)));
    ASSERT_NE(nullptr, eventObject->csrs[0]);
    ASSERT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csrs[0]);

    commandList->appendBarrier(event, 0, nullptr, false);

    auto result = eventObject->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(eventObject->queryStatus(), ZE_RESULT_SUCCESS);

    commandList->appendBarrier(nullptr, 0, nullptr, false);
}

TEST_F(CommandListCreate, whenCreatingImmCmdListWithASyncModeAndAppendEventResetThenUpdateTaskCountNeededFlagIsEnabled) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_HOST_VISIBLE;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    eventDesc.signal = ZE_EVENT_SCOPE_FLAG_HOST;
    eventDesc.wait = ZE_EVENT_SCOPE_FLAG_HOST;

    ze_event_handle_t event = nullptr;

    std::unique_ptr<L0::EventPool> eventPool(EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, returnValue));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    ASSERT_NE(nullptr, eventPool);

    eventPool->createEvent(&eventDesc, &event);

    std::unique_ptr<Event> eventObject(static_cast<Event *>(L0::Event::fromHandle(event)));
    ASSERT_NE(nullptr, eventObject->csrs[0]);
    ASSERT_EQ(device->getNEODevice()->getDefaultEngine().commandStreamReceiver, eventObject->csrs[0]);

    commandList->appendEventReset(event);

    auto result = eventObject->hostSignal();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    EXPECT_EQ(eventObject->queryStatus(), ZE_RESULT_SUCCESS);
}

TEST_F(CommandListCreate, whenInvokingAppendMemoryCopyFromContextForImmediateCommandListWithSyncModeThenSuccessIsReturned) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    auto result = commandList->appendMemoryCopyFromContext(dstPtr, nullptr, srcPtr, 8, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

struct CommandListCreateWithDeferredOsContextInitialization : ContextCommandListCreate {
    void SetUp() override {
        debugManager.flags.DeferOsContextInitialization.set(1);
        ContextCommandListCreate::SetUp();
    }

    void TearDown() override {
        ContextCommandListCreate::TearDown();
    }

    DebugManagerStateRestore restore;
};
TEST_F(ContextCommandListCreate, givenDeferredEngineCreationWhenImmediateCommandListIsCreatedThenEngineIsInitialized) {
    uint32_t groupsCount{};
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getCommandQueueGroupProperties(&groupsCount, nullptr));
    auto groups = std::vector<ze_command_queue_group_properties_t>(groupsCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, device->getCommandQueueGroupProperties(&groupsCount, groups.data()));

    for (uint32_t groupIndex = 0u; groupIndex < groupsCount; groupIndex++) {
        const auto &group = groups[groupIndex];
        for (uint32_t queueIndex = 0; queueIndex < group.numQueues; queueIndex++) {
            CommandStreamReceiver *expectedCsr{};
            EXPECT_EQ(ZE_RESULT_SUCCESS, device->getCsrForOrdinalAndIndex(&expectedCsr, groupIndex, queueIndex));

            ze_command_queue_desc_t desc = {};
            desc.mode = ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS;
            desc.ordinal = groupIndex;
            desc.index = queueIndex;
            ze_command_list_handle_t cmdListHandle;
            ze_result_t result = context->createCommandListImmediate(device, &desc, &cmdListHandle);
            L0::CommandList *cmdList = L0::CommandList::fromHandle(cmdListHandle);

            EXPECT_EQ(device, cmdList->getDevice());
            EXPECT_TRUE(cmdList->isImmediateType());
            EXPECT_NE(nullptr, cmdList);
            EXPECT_EQ(ZE_RESULT_SUCCESS, result);
            EXPECT_TRUE(expectedCsr->getOsContext().isInitialized());
            EXPECT_EQ(ZE_RESULT_SUCCESS, cmdList->destroy());
        }
    }
}

TEST_F(CommandListCreate, whenInvokingAppendMemoryCopyFromContextForImmediateCommandListWithASyncModeThenSuccessIsReturned) {
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    auto result = commandList->appendMemoryCopyFromContext(dstPtr, nullptr, srcPtr, 8, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(CommandListCreate, whenInvokingAppendMemoryCopyFromContextForImmediateCommandListThenSuccessIsReturned) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    void *srcPtr = reinterpret_cast<void *>(0x1234);
    void *dstPtr = reinterpret_cast<void *>(0x2345);
    auto result = commandList->appendMemoryCopyFromContext(dstPtr, nullptr, srcPtr, 8, nullptr, 0, nullptr, false);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
}

TEST_F(CommandListCreate, givenQueueDescriptionwhenCreatingImmediateCommandListForEveryEnigneThenItHasImmediateCommandQueueCreated) {
    auto &engineGroups = neoDevice->getRegularEngineGroups();
    for (uint32_t ordinal = 0; ordinal < engineGroups.size(); ordinal++) {
        for (uint32_t index = 0; index < engineGroups[ordinal].engines.size(); index++) {
            ze_command_queue_desc_t desc = {};
            desc.ordinal = ordinal;
            desc.index = index;
            ze_result_t returnValue;
            std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::renderCompute, returnValue));
            ASSERT_NE(nullptr, commandList);
            auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

            EXPECT_EQ(device, commandList->getDevice());
            EXPECT_TRUE(commandList->isImmediateType());
            EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);
        }
    }
}

TEST_F(CommandListCreate, givenInvalidProductFamilyThenReturnsNullPointer) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(IGFX_UNKNOWN, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    EXPECT_EQ(nullptr, commandList);
}

HWCMDTEST_F(IGFX_GEN8_CORE, CommandListCreate, whenCommandListIsCreatedThenPCAndStateBaseAddressCmdsAreAddedAndCorrectlyProgrammed) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseBindlessMode.set(0);
    debugManager.flags.DispatchCmdlistCmdBufferPrimary.set(0);

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();
    auto gmmHelper = commandContainer.getDevice()->getGmmHelper();

    ASSERT_NE(nullptr, commandContainer.getCommandStream());
    auto usedSpaceBefore = commandContainer.getCommandStream()->getUsed();

    auto result = commandList->close();
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandContainer.getCommandStream()->getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), usedSpaceAfter));

    auto itorPc = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPc);
    auto cmdPc = genCmdCast<PIPE_CONTROL *>(*itorPc);
    EXPECT_TRUE(cmdPc->getDcFlushEnable());
    EXPECT_TRUE(cmdPc->getCommandStreamerStallEnable());
    EXPECT_TRUE(cmdPc->getTextureCacheInvalidationEnable());

    auto itor = find<STATE_BASE_ADDRESS *>(itorPc, cmdList.end());
    ASSERT_NE(cmdList.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);

    auto dsh = commandContainer.getIndirectHeap(NEO::HeapType::dynamicState);
    auto ioh = commandContainer.getIndirectHeap(NEO::HeapType::indirectObject);
    auto ssh = commandContainer.getIndirectHeap(NEO::HeapType::surfaceState);

    EXPECT_TRUE(cmdSba->getDynamicStateBaseAddressModifyEnable());
    EXPECT_TRUE(cmdSba->getDynamicStateBufferSizeModifyEnable());
    EXPECT_EQ(dsh->getHeapGpuBase(), cmdSba->getDynamicStateBaseAddress());
    EXPECT_EQ(dsh->getHeapSizeInPages(), cmdSba->getDynamicStateBufferSize());

    EXPECT_TRUE(cmdSba->getIndirectObjectBaseAddressModifyEnable());
    EXPECT_TRUE(cmdSba->getIndirectObjectBufferSizeModifyEnable());
    EXPECT_EQ(ioh->getHeapGpuBase(), cmdSba->getIndirectObjectBaseAddress());
    EXPECT_EQ(ioh->getHeapSizeInPages(), cmdSba->getIndirectObjectBufferSize());

    EXPECT_TRUE(cmdSba->getSurfaceStateBaseAddressModifyEnable());
    EXPECT_EQ(ssh->getHeapGpuBase(), cmdSba->getSurfaceStateBaseAddress());

    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER), cmdSba->getStatelessDataPortAccessMemoryObjectControlState());
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenCreatedThenStateBaseAddressCmdIsNotProgrammedAndHeapIsNotAllocated) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());

    EXPECT_EQ(cmdList.end(), itor);

    for (uint32_t i = 0; i < NEO::HeapType::numTypes; i++) {
        ASSERT_EQ(commandContainer.getIndirectHeap(static_cast<NEO::HeapType>(i)), nullptr);
        ASSERT_EQ(commandContainer.getIndirectHeapAllocation(static_cast<NEO::HeapType>(i)), nullptr);
    }
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenSetBarrierThenMiFlushDWIsProgrammed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();
    commandList->appendBarrier(nullptr, 0, nullptr, false);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenImmediateCommandListWithCopyOnlyWhenSetBarrierThenMiFlushCmdIsInsertedInTheCmdContainer) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    ze_command_queue_desc_t desc = {};
    desc.mode = ZE_COMMAND_QUEUE_MODE_ASYNCHRONOUS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::createImmediate(productFamily, device, &desc, false, NEO::EngineGroupType::copy, returnValue));
    ASSERT_NE(nullptr, commandList);
    auto whiteBoxCmdList = static_cast<CommandList *>(commandList.get());

    EXPECT_EQ(device, commandList->getDevice());
    EXPECT_TRUE(commandList->isImmediateType());
    EXPECT_NE(nullptr, whiteBoxCmdList->cmdQImmediate);

    auto &commandContainer = commandList->getCmdContainer();
    commandList->appendBarrier(nullptr, 0, nullptr, false);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, whenCommandListIsResetThenContainsStatelessUncachedResourceIsSetToFalse) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily,
                                                                     device,
                                                                     NEO::EngineGroupType::compute,
                                                                     0u,
                                                                     returnValue, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    returnValue = commandList->reset();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);

    EXPECT_FALSE(commandList->getContainsStatelessUncachedResource());
}

HWTEST_F(CommandListCreate, givenBindlessModeDisabledWhenCommandListsResetThenSbaReloaded) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.UseBindlessMode.set(0);
    debugManager.flags.EnableStateBaseAddressTracking.set(0);
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily,
                                                                     device,
                                                                     NEO::EngineGroupType::compute,
                                                                     0u,
                                                                     returnValue, false));
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    returnValue = commandList->reset();
    auto usedAfter = commandList->getCmdContainer().getCommandStream()->getUsed();
    EXPECT_EQ(ZE_RESULT_SUCCESS, returnValue);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), usedAfter));

    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWithCopyOnlyWhenResetThenStateBaseAddressNotProgrammed) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::copy, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();
    commandList->reset();

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());

    EXPECT_EQ(cmdList.end(), itor);
}

HWTEST_F(CommandListCreate, givenCommandListWhenSetBarrierThenPipeControlIsProgrammed) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();
    commandList->appendBarrier(nullptr, 0, nullptr, false);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());

    EXPECT_NE(cmdList.end(), itor);
}

HWTEST2_F(CommandListCreate, givenCommandListWhenAppendingBarrierThenPipeControlIsProgrammedAndHdcFlushIsSet, IsAtLeastXeHpCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();
    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    returnValue = commandList->appendBarrier(nullptr, 0, nullptr, false);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        commandContainer.getCommandStream()->getUsed() - usedBefore));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    auto pipeControlCmd = reinterpret_cast<typename FamilyType::PIPE_CONTROL *>(*itor);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControlCmd));
}

HWTEST2_F(CommandListCreate, givenCommandListWhenAppendingBarrierThenPipeControlIsProgrammedWithHdcAndUntypedFlushSet, IsAtLeastXeHpgCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    auto &commandContainer = commandList->getCmdContainer();
    size_t usedBefore = commandContainer.getCommandStream()->getUsed();
    returnValue = commandList->appendBarrier(nullptr, 0, nullptr, false);
    EXPECT_EQ(returnValue, ZE_RESULT_SUCCESS);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList,
        ptrOffset(commandContainer.getCommandStream()->getCpuBase(), usedBefore),
        commandContainer.getCommandStream()->getUsed() - usedBefore));
    auto itor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);

    auto pipeControlCmd = reinterpret_cast<typename FamilyType::PIPE_CONTROL *>(*itor);
    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControlCmd));
    EXPECT_TRUE(pipeControlCmd->getUnTypedDataPortCacheFlush());
}

HWTEST_F(CommandListCreate, givenCommandListWhenAppendingBarrierWithIncorrectWaitEventsThenInvalidArgumentIsReturned) {
    ze_result_t returnValue;
    std::unique_ptr<L0::CommandList> commandList(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    returnValue = commandList->appendBarrier(nullptr, 4, nullptr, false);
    EXPECT_EQ(returnValue, ZE_RESULT_ERROR_INVALID_ARGUMENT);
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenProfilingBeforeCommandForCopyOnlyThenCommandsHaveCorrectEventOffsets, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    auto baseAddr = event->getGpuAddress(device);
    auto contextOffset = event->getContextStartOffset();
    auto globalOffset = event->getGlobalStartOffset();
    EXPECT_EQ(baseAddr, event->getPacketAddress(device));

    commandList->appendEventForProfilingCopyCommand(event.get(), true);
    EXPECT_EQ(1u, event->getPacketsInUse());

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::globalTimestampLdw);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, globalOffset));
    EXPECT_NE(cmdList.end(), ++itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::gpThreadTimeRegAddressOffsetLow);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, contextOffset));
}

HWTEST2_F(CommandListCreate, givenCopyCommandListWhenProfilingAfterCommandForCopyOnlyThenCommandsHaveCorrectEventOffsets, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);
    ze_event_pool_desc_t eventPoolDesc = {};
    eventPoolDesc.count = 1;
    eventPoolDesc.flags = ZE_EVENT_POOL_FLAG_KERNEL_TIMESTAMP;

    ze_event_desc_t eventDesc = {};
    eventDesc.index = 0;
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto eventPool = std::unique_ptr<L0::EventPool>(L0::EventPool::create(driverHandle.get(), context, 0, nullptr, &eventPoolDesc, result));
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    auto event = std::unique_ptr<L0::Event>(L0::Event::create<typename FamilyType::TimestampPacketType>(eventPool.get(), &eventDesc, device));

    commandList->appendEventForProfilingCopyCommand(event.get(), false);

    auto contextOffset = event->getContextEndOffset();
    auto globalOffset = event->getGlobalEndOffset();
    auto baseAddr = event->getGpuAddress(device);
    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandList->getCmdContainer().getCommandStream()->getCpuBase(), 0), commandList->getCmdContainer().getCommandStream()->getUsed()));
    auto itor = find<MI_STORE_REGISTER_MEM *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(cmdList.end(), itor);
    auto cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::globalTimestampLdw);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, globalOffset));
    EXPECT_NE(cmdList.end(), ++itor);
    cmd = genCmdCast<MI_STORE_REGISTER_MEM *>(*itor);
    EXPECT_EQ(cmd->getRegisterAddress(), RegisterOffsets::gpThreadTimeRegAddressOffsetLow);
    EXPECT_EQ(cmd->getMemoryAddress(), ptrOffset(baseAddr, contextOffset));
}

HWTEST2_F(CommandListCreate, givenNullEventWhenAppendEventAfterWalkerThenNothingAddedToStream, IsAtLeastSkl) {
    using GfxFamily = typename NEO::GfxFamilyMapper<gfxCoreFamily>::GfxFamily;
    using MI_STORE_REGISTER_MEM = typename GfxFamily::MI_STORE_REGISTER_MEM;
    auto commandList = std::make_unique<WhiteBox<::L0::CommandListCoreFamily<gfxCoreFamily>>>();
    commandList->initialize(device, NEO::EngineGroupType::copy, 0u);

    auto usedBefore = commandList->getCmdContainer().getCommandStream()->getUsed();

    commandList->appendSignalEventPostWalker(nullptr, nullptr, nullptr, false, false);

    EXPECT_EQ(commandList->getCmdContainer().getCommandStream()->getUsed(), usedBefore);
}

TEST_F(CommandListCreate, givenCreatedCommandListWhenGettingTrackingFlagsThenDefaultValuseIsHwSupported) {
    auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();

    auto &l0GfxCoreHelper = rootDeviceEnvironment.getHelper<L0GfxCoreHelper>();
    auto &productHelper = rootDeviceEnvironment.getHelper<NEO::ProductHelper>();

    ze_result_t returnValue;
    std::unique_ptr<L0::ult::CommandList> commandList(CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false)));
    ASSERT_NE(nullptr, commandList.get());

    bool expectedStateComputeModeTracking = l0GfxCoreHelper.platformSupportsStateComputeModeTracking();
    EXPECT_EQ(expectedStateComputeModeTracking, commandList->stateComputeModeTracking);

    bool expectedPipelineSelectTracking = l0GfxCoreHelper.platformSupportsPipelineSelectTracking();
    EXPECT_EQ(expectedPipelineSelectTracking, commandList->pipelineSelectStateTracking);

    bool expectedFrontEndTracking = l0GfxCoreHelper.platformSupportsFrontEndTracking();
    EXPECT_EQ(expectedFrontEndTracking, commandList->frontEndStateTracking);

    bool expectedStateBaseAddressTracking = l0GfxCoreHelper.platformSupportsStateBaseAddressTracking(device->getNEODevice()->getRootDeviceEnvironment());
    EXPECT_EQ(expectedStateBaseAddressTracking, commandList->getCmdListStateBaseAddressTracking());

    bool expectedDoubleSbaWa = productHelper.isAdditionalStateBaseAddressWARequired(device->getHwInfo());
    EXPECT_EQ(expectedDoubleSbaWa, commandList->doubleSbaWa);

    auto expectedHeapAddressModel = l0GfxCoreHelper.getPlatformHeapAddressModel(device->getNEODevice()->getRootDeviceEnvironment());
    EXPECT_EQ(expectedHeapAddressModel, commandList->getCmdListHeapAddressModel());
    EXPECT_EQ(expectedHeapAddressModel, commandList->getCmdContainer().getHeapAddressModel());

    auto expectedDispatchCmdListBatchBufferAsPrimary = L0GfxCoreHelper::dispatchCmdListBatchBufferAsPrimary(rootDeviceEnvironment, true);
    EXPECT_EQ(expectedDispatchCmdListBatchBufferAsPrimary, commandList->getCmdListBatchBufferFlag());
}

TEST(BuiltinTypeHelperTest, givenNonStatelessAndNonHeaplessWhenAdjustBuiltinTypeIsCalledThenCorrectBuiltinTypeIsReturned) {
    bool isStateless = false;
    bool isHeapless = false;

    EXPECT_EQ(Builtin::copyBufferBytes, BuiltinTypeHelper::adjustBuiltinType<Builtin::copyBufferBytes>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::copyBufferToBufferMiddle, BuiltinTypeHelper::adjustBuiltinType<Builtin::copyBufferToBufferMiddle>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::copyBufferToBufferSide, BuiltinTypeHelper::adjustBuiltinType<Builtin::copyBufferToBufferSide>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::fillBufferImmediate, BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferImmediate>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::fillBufferImmediateLeftOver, BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferImmediateLeftOver>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::fillBufferSSHOffset, BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferSSHOffset>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::fillBufferMiddle, BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferMiddle>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::fillBufferRightLeftover, BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferRightLeftover>(isStateless, isHeapless));
}

TEST(BuiltinTypeHelperTest, givenStatelessAndNonHeaplessWhenAdjustBuiltinTypeIsCalledThenCorrectBuiltinTypeIsReturned) {
    bool isStateless = true;
    bool isHeapless = false;

    EXPECT_EQ(Builtin::copyBufferBytesStateless, BuiltinTypeHelper::adjustBuiltinType<Builtin::copyBufferBytes>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::copyBufferToBufferMiddleStateless, BuiltinTypeHelper::adjustBuiltinType<Builtin::copyBufferToBufferMiddle>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::copyBufferToBufferSideStateless, BuiltinTypeHelper::adjustBuiltinType<Builtin::copyBufferToBufferSide>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::fillBufferImmediateStateless, BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferImmediate>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::fillBufferImmediateLeftOverStateless, BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferImmediateLeftOver>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::fillBufferSSHOffsetStateless, BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferSSHOffset>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::fillBufferMiddleStateless, BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferMiddle>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::fillBufferRightLeftoverStateless, BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferRightLeftover>(isStateless, isHeapless));
}

TEST(BuiltinTypeHelperTest, givenHeaplessWhenAdjustBuiltinTypeIsCalledThenCorrectBuiltinTypeIsReturned) {
    bool isStateless = false;
    bool isHeapless = true;

    EXPECT_EQ(Builtin::copyBufferBytesStatelessHeapless, BuiltinTypeHelper::adjustBuiltinType<Builtin::copyBufferBytes>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::copyBufferToBufferMiddleStatelessHeapless, BuiltinTypeHelper::adjustBuiltinType<Builtin::copyBufferToBufferMiddle>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::copyBufferToBufferSideStatelessHeapless, BuiltinTypeHelper::adjustBuiltinType<Builtin::copyBufferToBufferSide>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::fillBufferImmediateStatelessHeapless, BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferImmediate>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::fillBufferImmediateLeftOverStatelessHeapless, BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferImmediateLeftOver>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::fillBufferSSHOffsetStatelessHeapless, BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferSSHOffset>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::fillBufferMiddleStatelessHeapless, BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferMiddle>(isStateless, isHeapless));
    EXPECT_EQ(Builtin::fillBufferRightLeftoverStatelessHeapless, BuiltinTypeHelper::adjustBuiltinType<Builtin::fillBufferRightLeftover>(isStateless, isHeapless));
}
HWTEST2_F(CommandListCreate, givenDummyBlitRequiredWhenEncodeMiFlushWithPostSyncThenDummyBlitIsProgrammedPriorToMiFlushAndDummyAllocationIsAddedToResidencyContainer, IsAtLeastXeHpCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceDummyBlitWa.set(1);
    MockCommandListCoreFamily<gfxCoreFamily> cmdlist;
    cmdlist.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdlist.csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    auto &commandContainer = cmdlist.getCmdContainer();
    cmdlist.dummyBlitWa.isWaRequired = true;
    MiFlushArgs args{cmdlist.dummyBlitWa};
    args.commandWithPostSync = true;
    auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironmentRef();
    commandContainer.getResidencyContainer().clear();
    EXPECT_EQ(nullptr, rootDeviceEnvironment.getDummyAllocation());
    cmdlist.encodeMiFlush(0, 0, args);
    GenCmdList programmedCommands;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        programmedCommands, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(programmedCommands.begin(), programmedCommands.end());
    EXPECT_NE(programmedCommands.begin(), itor);
    EXPECT_NE(programmedCommands.end(), itor);
    auto firstCommand = programmedCommands.begin();
    UnitTestHelper<FamilyType>::verifyDummyBlitWa(&rootDeviceEnvironment, firstCommand);
    EXPECT_NE(nullptr, rootDeviceEnvironment.getDummyAllocation());
    EXPECT_EQ(commandContainer.getResidencyContainer().size(), 1u);
    EXPECT_EQ(commandContainer.getResidencyContainer()[0], rootDeviceEnvironment.getDummyAllocation());
}

HWTEST2_F(CommandListCreate, givenDummyBlitRequiredWhenEncodeMiFlushWithoutPostSyncThenDummyBlitIsNotProgrammedAndDummyAllocationIsNotAddedToResidencyContainer, IsAtLeastXeHpCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceDummyBlitWa.set(1);
    MockCommandListCoreFamily<gfxCoreFamily> cmdlist;
    cmdlist.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdlist.csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    auto &commandContainer = cmdlist.getCmdContainer();
    cmdlist.dummyBlitWa.isWaRequired = true;
    MiFlushArgs args{cmdlist.dummyBlitWa};
    args.commandWithPostSync = false;
    auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironmentRef();
    rootDeviceEnvironment.initDummyAllocation();
    EXPECT_NE(nullptr, rootDeviceEnvironment.getDummyAllocation());
    commandContainer.getResidencyContainer().clear();
    cmdlist.encodeMiFlush(0, 0, args);
    GenCmdList programmedCommands;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        programmedCommands, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(programmedCommands.begin(), programmedCommands.end());
    EXPECT_EQ(programmedCommands.begin(), itor);
    EXPECT_NE(programmedCommands.end(), itor);
    EXPECT_EQ(commandContainer.getResidencyContainer().size(), 0u);
}

HWTEST2_F(CommandListCreate, givenDummyBlitNotRequiredWhenEncodeMiFlushThenDummyBlitIsNotProgrammedAndDummyAllocationIsNotAddedToResidencyContainer, IsAtLeastXeHpCore) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceDummyBlitWa.set(0);
    MockCommandListCoreFamily<gfxCoreFamily> cmdlist;
    cmdlist.initialize(device, NEO::EngineGroupType::copy, 0u);
    cmdlist.csr = device->getNEODevice()->getDefaultEngine().commandStreamReceiver;
    auto &commandContainer = cmdlist.getCmdContainer();
    cmdlist.dummyBlitWa.isWaRequired = true;
    MiFlushArgs args{cmdlist.dummyBlitWa};
    auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironmentRef();
    rootDeviceEnvironment.initDummyAllocation();
    EXPECT_NE(nullptr, rootDeviceEnvironment.getDummyAllocation());
    commandContainer.getResidencyContainer().clear();
    cmdlist.encodeMiFlush(0, 0, args);
    GenCmdList programmedCommands;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        programmedCommands, ptrOffset(commandContainer.getCommandStream()->getCpuBase(), 0), commandContainer.getCommandStream()->getUsed()));
    auto itor = find<MI_FLUSH_DW *>(programmedCommands.begin(), programmedCommands.end());
    EXPECT_EQ(programmedCommands.begin(), itor);
    EXPECT_NE(programmedCommands.end(), itor);
    EXPECT_EQ(commandContainer.getResidencyContainer().size(), 0u);
}

} // namespace ult
} // namespace L0
