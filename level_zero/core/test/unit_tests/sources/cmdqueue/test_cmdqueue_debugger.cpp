/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_bindless_heaps_helper.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "level_zero/core/source/cmdqueue/cmdqueue_hw.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdlist.h"
#include "level_zero/core/test/unit_tests/mocks/mock_cmdqueue.h"
#include "level_zero/core/test/unit_tests/mocks/mock_kernel.h"
#include "level_zero/core/test/unit_tests/sources/debugger/l0_debugger_fixture.h"

#include "test_traits_common.h"

namespace L0 {
namespace ult {

using L0CmdQueueDebuggerTest = Test<L0DebuggerPerContextAddressSpaceFixture>;
HWTEST_F(L0CmdQueueDebuggerTest, givenDebuggingEnabledWhenCmdListRequiringSbaProgrammingExecutedThenProgramSbaWritesToSbaTrackingBufferForNonInternalQueues) {
    DebugManagerStateRestore restorer;
    debugManager.flags.EnableStateBaseAddressTracking.set(1);

    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    bool internalQueueMode[] = {false, true};

    for (auto internalQueue : internalQueueMode) {
        ze_command_queue_desc_t queueDesc = {};
        ze_result_t returnValue;
        auto cmdQ = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, internalQueue, false, returnValue);
        ASSERT_NE(nullptr, cmdQ);

        auto commandQueue = whiteboxCast(cmdQ);

        Mock<KernelImp> kernel;
        std::unique_ptr<L0::CommandList> commandList(CommandList::create(NEO::defaultHwInfo->platform.eProductFamily, device, NEO::EngineGroupType::renderCompute, 0u, returnValue, false));
        ze_group_count_t groupCount{1, 1, 1};
        NEO::LinearStream &cmdStream = commandQueue->commandStream;

        auto usedSpaceBefore = cmdStream.getUsed();

        CmdListKernelLaunchParams launchParams = {};
        auto result = commandList->appendLaunchKernel(kernel.toHandle(), groupCount, nullptr, 0, nullptr, launchParams, false);
        EXPECT_EQ(ZE_RESULT_SUCCESS, result);
        commandList->close();

        ze_command_list_handle_t commandListHandle = commandList->toHandle();
        const uint32_t numCommandLists = 1u;

        result = cmdQ->executeCommandLists(numCommandLists, &commandListHandle, nullptr, true, nullptr, 0, nullptr);
        ASSERT_EQ(ZE_RESULT_SUCCESS, result);

        auto usedSpaceAfter = cmdStream.getUsed();
        ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

        GenCmdList cmdList;
        ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
            cmdList, ptrOffset(cmdStream.getCpuBase(), 0), usedSpaceAfter));

        auto pcItor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
        ASSERT_NE(cmdList.end(), pcItor);

        auto sbaItor = find<STATE_BASE_ADDRESS *>(pcItor, cmdList.end());
        ASSERT_NE(cmdList.end(), sbaItor);
        auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*sbaItor);

        auto sdiItor = find<MI_STORE_DATA_IMM *>(sbaItor, cmdList.end());

        if (!internalQueue) {
            ASSERT_NE(cmdList.end(), sdiItor);
            auto cmdSdi = genCmdCast<MI_STORE_DATA_IMM *>(*sdiItor);

            auto gmmHelper = neoDevice->getGmmHelper();

            std::vector<uint64_t> baseAddresses;
            baseAddresses.push_back(gmmHelper->canonize(cmdSba->getGeneralStateBaseAddress()));
            baseAddresses.push_back(gmmHelper->canonize(cmdSba->getSurfaceStateBaseAddress()));
            baseAddresses.push_back(gmmHelper->canonize(cmdSba->getDynamicStateBaseAddress()));
            baseAddresses.push_back(gmmHelper->canonize(cmdSba->getInstructionBaseAddress()));

            std::vector<size_t> offsets;
            offsets.push_back(offsetof(NEO::SbaTrackedAddresses, generalStateBaseAddress));
            offsets.push_back(offsetof(NEO::SbaTrackedAddresses, surfaceStateBaseAddress));
            offsets.push_back(offsetof(NEO::SbaTrackedAddresses, dynamicStateBaseAddress));
            offsets.push_back(offsetof(NEO::SbaTrackedAddresses, instructionBaseAddress));

            bool addressFound = false;
            for (size_t i = 0; i < baseAddresses.size(); i++) {

                if (baseAddresses[i] != 0) {
                    EXPECT_EQ(static_cast<uint32_t>(baseAddresses[i] & 0x0000FFFFFFFFULL), cmdSdi->getDataDword0());
                    EXPECT_EQ(static_cast<uint32_t>(baseAddresses[i] >> 32), cmdSdi->getDataDword1());

                    auto expectedGpuVa = gmmHelper->decanonize(device->getL0Debugger()->getSbaTrackingGpuVa()) + offsets[i];
                    EXPECT_EQ(expectedGpuVa, cmdSdi->getAddress());
                    addressFound = true;
                    break;
                }
            }
            EXPECT_TRUE(addressFound);
        } else {
            EXPECT_EQ(cmdList.end(), sdiItor);
        }
        cmdQ->destroy();

        neoDevice->getDefaultEngine().commandStreamReceiver->getStreamProperties().stateBaseAddress.resetState();
    }
}

using IsBetweenGen9AndGen12lp = IsWithinGfxCore<IGFX_GEN9_CORE, IGFX_GEN12LP_CORE>;
HWTEST2_F(L0CmdQueueDebuggerTest, givenDebuggingEnabledAndRequiredGsbaWhenInternalCommandQueueThenProgramGsbaDoesNotWritesToSbaTrackingBuffer, IsBetweenGen9AndGen12lp) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    ze_command_queue_desc_t queueDesc = {};
    ze_result_t returnValue;
    auto cmdQ = CommandQueue::create(productFamily, device, neoDevice->getDefaultEngine().commandStreamReceiver, &queueDesc, false, true, false, returnValue);
    ASSERT_NE(nullptr, cmdQ);

    auto cmdQHw = static_cast<CommandQueueHw<gfxCoreFamily> *>(cmdQ);
    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    auto usedSpaceBefore = cmdStream.getUsed();

    cmdQHw->programStateBaseAddress(0u, false, cmdStream, true, nullptr);

    auto usedSpaceAfter = cmdStream.getUsed();
    ASSERT_GT(usedSpaceAfter, usedSpaceBefore);

    GenCmdList cmdList;
    ASSERT_TRUE(FamilyType::Parse::parseCommandBuffer(
        cmdList, ptrOffset(cmdStream.getCpuBase(), 0), usedSpaceAfter));

    auto pcItor = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), pcItor);

    auto sbaItor = find<STATE_BASE_ADDRESS *>(pcItor, cmdList.end());
    ASSERT_NE(cmdList.end(), sbaItor);

    auto sdiItor = find<MI_STORE_DATA_IMM *>(sbaItor, cmdList.end());
    EXPECT_EQ(cmdList.end(), sdiItor);

    cmdQ->destroy();
}

} // namespace ult

} // namespace L0
