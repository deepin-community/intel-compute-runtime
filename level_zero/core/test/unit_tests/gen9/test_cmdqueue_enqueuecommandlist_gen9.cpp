/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_memory_manager.h"
#include <level_zero/ze_api.h>

#include "gtest/gtest.h"

#include <limits>

namespace L0 {
namespace ult {

using CommandQueueExecuteCommandListsGen9 = Test<DeviceFixture>;

GEN9TEST_F(CommandQueueExecuteCommandListsGen9, WhenExecutingCmdListsThenPipelineSelectAndVfeStateAreAddedToCmdBuffer) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(
        productFamily,
        device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);
    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    auto commandList = CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false);
    commandList->close();
    ze_command_list_handle_t commandLists[] = {
        commandList->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, 0, nullptr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));

    using MEDIA_VFE_STATE = typename FamilyType::MEDIA_VFE_STATE;
    auto itorVFE = find<MEDIA_VFE_STATE *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(itorVFE, cmdList.end());

    // Should have a PS before a VFE
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    auto itorPS = find<PIPELINE_SELECT *>(cmdList.begin(), itorVFE);
    ASSERT_NE(itorPS, itorVFE);
    {
        auto cmd = genCmdCast<PIPELINE_SELECT *>(*itorPS);
        EXPECT_EQ(cmd->getMaskBits() & 3u, 3u);
        EXPECT_EQ(cmd->getPipelineSelection(), PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU);
    }

    commandList->destroy();
    commandQueue->destroy();
}

GEN9TEST_F(CommandQueueExecuteCommandListsGen9, WhenExecutingCmdListsThenStateBaseAddressForGeneralStateBaseAddressIsAdded) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(
        productFamily,
        device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);
    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    auto commandList = CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false);
    commandList->close();
    ze_command_list_handle_t commandLists[] = {
        commandList->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, 0, nullptr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;

    auto itorSba = find<STATE_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(itorSba, cmdList.end());
    {
        auto cmd = genCmdCast<STATE_BASE_ADDRESS *>(*itorSba);
        EXPECT_TRUE(cmd->getGeneralStateBaseAddressModifyEnable());
        EXPECT_EQ(0u, cmd->getGeneralStateBaseAddress());
        EXPECT_TRUE(cmd->getGeneralStateBufferSizeModifyEnable());
        uint32_t expectedGsbaSize = std::numeric_limits<uint32_t>::max();
        expectedGsbaSize >>= 12;
        EXPECT_EQ(expectedGsbaSize, cmd->getGeneralStateBufferSize());

        EXPECT_TRUE(cmd->getInstructionBaseAddressModifyEnable());
        EXPECT_TRUE(cmd->getInstructionBufferSizeModifyEnable());
        EXPECT_EQ(MemoryConstants::sizeOf4GBinPageEntities, cmd->getInstructionBufferSize());
        EXPECT_EQ(device->getDriverHandle()->getMemoryManager()->getInternalHeapBaseAddress(0, false),
                  cmd->getInstructionBaseAddress());
        EXPECT_EQ(commandQueue->getDevice()->getNEODevice()->getGmmHelper()->getMOCS(GMM_RESOURCE_USAGE_OCL_STATE_HEAP_BUFFER),
                  cmd->getInstructionMemoryObjectControlState());
    }

    commandList->destroy();
    commandQueue->destroy();
}

GEN9TEST_F(CommandQueueExecuteCommandListsGen9, WhenExecutingCmdListsThenMidThreadPreemptionForFirstExecuteIsConfigured) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(
        productFamily,
        device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);
    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    auto commandList = CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    commandList->commandListPreemptionMode = NEO::PreemptionMode::MidThread;
    commandList->close();

    ze_command_list_handle_t commandLists[] = {commandList->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);

    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, 0, nullptr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(commandQueue->commandStream.getCpuBase(), 0), usedSpaceAfter));
    using STATE_SIP = typename FamilyType::STATE_SIP;
    using GPGPU_CSR_BASE_ADDRESS = typename FamilyType::GPGPU_CSR_BASE_ADDRESS;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    auto itorCsr = find<GPGPU_CSR_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(itorCsr, cmdList.end());

    auto itorStateSip = find<STATE_SIP *>(itorCsr, cmdList.end());
    EXPECT_NE(itorStateSip, cmdList.end());

    auto itorLri = find<MI_LOAD_REGISTER_IMM *>(itorStateSip, cmdList.end());
    EXPECT_NE(itorLri, cmdList.end());

    MI_LOAD_REGISTER_IMM *lriCmd = static_cast<MI_LOAD_REGISTER_IMM *>(*itorLri);
    EXPECT_EQ(0x2580u, lriCmd->getRegisterOffset());
    uint32_t data = ((1 << 1) | (1 << 2)) << 16;
    EXPECT_EQ(data, lriCmd->getDataDword());

    commandList->destroy();
    commandQueue->destroy();
}

GEN9TEST_F(CommandQueueExecuteCommandListsGen9, GivenCmdListsWithDifferentPreemptionModesWhenExecutingThenQueuePreemptionIsSwitchedFromMidThreadToThreadGroupAndMidThread) {
    const ze_command_queue_desc_t desc = {};
    ze_result_t returnValue;
    auto commandQueue = whiteboxCast(CommandQueue::create(
        productFamily,
        device, neoDevice->getDefaultEngine().commandStreamReceiver, &desc, false, false, false, returnValue));
    ASSERT_NE(nullptr, commandQueue);
    auto usedSpaceBefore = commandQueue->commandStream.getUsed();

    auto commandListMidThread = CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    commandListMidThread->commandListPreemptionMode = NEO::PreemptionMode::MidThread;
    commandListMidThread->close();

    auto commandListThreadGroup = CommandList::whiteboxCast(CommandList::create(productFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
    commandListThreadGroup->commandListPreemptionMode = NEO::PreemptionMode::ThreadGroup;
    commandListThreadGroup->close();

    ze_command_list_handle_t commandLists[] = {commandListMidThread->toHandle(),
                                               commandListThreadGroup->toHandle(),
                                               commandListMidThread->toHandle()};
    uint32_t numCommandLists = sizeof(commandLists) / sizeof(commandLists[0]);
    auto result = commandQueue->executeCommandLists(numCommandLists, commandLists, nullptr, true, nullptr, 0, nullptr);

    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto usedSpaceAfter = commandQueue->commandStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, commandQueue->commandStream.getCpuBase(), usedSpaceAfter));
    using STATE_SIP = typename FamilyType::STATE_SIP;
    using GPGPU_CSR_BASE_ADDRESS = typename FamilyType::GPGPU_CSR_BASE_ADDRESS;
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto itorCsr = find<GPGPU_CSR_BASE_ADDRESS *>(cmdList.begin(), cmdList.end());
    EXPECT_NE(itorCsr, cmdList.end());

    auto itorStateSip = find<STATE_SIP *>(itorCsr, cmdList.end());
    EXPECT_NE(itorStateSip, cmdList.end());

    auto itorLri = find<MI_LOAD_REGISTER_IMM *>(itorStateSip, cmdList.end());
    EXPECT_NE(itorLri, cmdList.end());

    MI_LOAD_REGISTER_IMM *lriCmd = static_cast<MI_LOAD_REGISTER_IMM *>(*itorLri);
    EXPECT_EQ(0x2580u, lriCmd->getRegisterOffset());
    uint32_t data = ((1 << 1) | (1 << 2)) << 16;
    EXPECT_EQ(data, lriCmd->getDataDword());

    // next should be BB_START to 1st Mid-Thread Cmd List
    auto itorBBStart = find<MI_BATCH_BUFFER_START *>(itorLri, cmdList.end());
    EXPECT_NE(itorBBStart, cmdList.end());

    // next should be PIPE_CONTROL and LRI switching to thread-group
    auto itorPipeControl = find<PIPE_CONTROL *>(itorBBStart, cmdList.end());
    EXPECT_NE(itorPipeControl, cmdList.end());

    itorLri = find<MI_LOAD_REGISTER_IMM *>(itorPipeControl, cmdList.end());
    EXPECT_NE(itorLri, cmdList.end());

    lriCmd = static_cast<MI_LOAD_REGISTER_IMM *>(*itorLri);
    EXPECT_EQ(0x2580u, lriCmd->getRegisterOffset());
    data = (1 << 1) | (((1 << 1) | (1 << 2)) << 16);
    EXPECT_EQ(data, lriCmd->getDataDword());
    // start of thread-group command list
    itorBBStart = find<MI_BATCH_BUFFER_START *>(itorLri, cmdList.end());
    EXPECT_NE(itorBBStart, cmdList.end());

    // next should be PIPE_CONTROL and LRI switching to mid-thread again
    itorPipeControl = find<PIPE_CONTROL *>(itorBBStart, cmdList.end());
    EXPECT_NE(itorPipeControl, cmdList.end());

    itorLri = find<MI_LOAD_REGISTER_IMM *>(itorPipeControl, cmdList.end());
    EXPECT_NE(itorLri, cmdList.end());

    lriCmd = static_cast<MI_LOAD_REGISTER_IMM *>(*itorLri);
    EXPECT_EQ(0x2580u, lriCmd->getRegisterOffset());
    data = ((1 << 1) | (1 << 2)) << 16;
    EXPECT_EQ(data, lriCmd->getDataDword());
    // start of thread-group command list
    itorBBStart = find<MI_BATCH_BUFFER_START *>(itorLri, cmdList.end());
    EXPECT_NE(itorBBStart, cmdList.end());

    commandListMidThread->destroy();
    commandListThreadGroup->destroy();
    commandQueue->destroy();
}

} // namespace ult
} // namespace L0
