/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/encoders/test_encode_dispatch_kernel_dg2_and_later.h"

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

#include "test_traits_common.h"

using namespace NEO;

using CommandEncodeStatesTestDg2AndLater = Test<CommandEncodeStatesFixture>;

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, givenEventAddressWhenEncodeAndPVCAndDG2ThenSetDataportSubsliceCacheFlushIstSet, IsAtLeastXeHpgCore) {
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint64_t eventAddress = MemoryConstants::cacheLineSize * 123;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.eventAddress = eventAddress;
    dispatchArgs.isTimestampEvent = true;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(true, cmd->getPostSync().getDataportSubsliceCacheFlush());
}

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, givenEventAddressWhenEncodeThenMocsIndex2IsSet, IsXeHpgCore) {
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint64_t eventAddress = MemoryConstants::cacheLineSize * 123;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.eventAddress = eventAddress;
    dispatchArgs.isTimestampEvent = true;
    dispatchArgs.dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, pDevice->getRootDeviceEnvironment());

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);

    auto gmmHelper = pDevice->getGmmHelper();

    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), cmd->getPostSync().getMocs());
}

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, GivenVariousSlmTotalSizesAndSettingRevIDToDifferentValuesWhenSetAdditionalInfoIsCalledThenCorrectValuesAreSet, IsXeHpgCore) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename FamilyType::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    const std::vector<PreferredSlmTestValues<FamilyType>> valuesToTest = {
        {0, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K},
        {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_16K},
        {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32K},
        // since we can't set 48KB as SLM size for workgroup, we need to ask for 64KB here.
        {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64K},
    };

    const std::array<REVID, 5> revs{REVISION_A0, REVISION_B, REVISION_C, REVISION_D, REVISION_K};
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto &productHelper = pDevice->getRootDeviceEnvironment().getProductHelper();
    for (auto rev : revs) {
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(rev, hwInfo);
        verifyPreferredSlmValues<FamilyType>(valuesToTest, pDevice->getRootDeviceEnvironment());
    }
}

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, GivenDebugOverrideWhenSetAdditionalInfoIsCalledThenDebugValuesAreSet, IsAtLeastXeHpgCore) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename FamilyType::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    DebugManagerStateRestore stateRestore;
    PREFERRED_SLM_ALLOCATION_SIZE debugOverrideValues[] = {PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K,
                                                           PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32K,
                                                           PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_128K};

    for (auto debugOverrideValue : debugOverrideValues) {
        debugManager.flags.OverridePreferredSlmAllocationSizePerDss.set(debugOverrideValue);
        const std::vector<PreferredSlmTestValues<FamilyType>> valuesToTest = {
            {0, debugOverrideValue},
            {32 * MemoryConstants::kiloByte, debugOverrideValue},
            {64 * MemoryConstants::kiloByte, debugOverrideValue},
        };
        verifyPreferredSlmValues<FamilyType>(valuesToTest, pDevice->getRootDeviceEnvironment());
    }
}

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, givenOverridePreferredSlmAllocationSizePerDssWhenDispatchingKernelThenCorrectValueIsSet, IsAtLeastXeHpgCore) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    DebugManagerStateRestore restorer;
    debugManager.flags.OverridePreferredSlmAllocationSizePerDss.set(5);
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 1;

    dispatchInterface->getSlmTotalSizeResult = slmTotalSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &idd = cmd->getInterfaceDescriptor();

    EXPECT_EQ(5u, static_cast<uint32_t>(idd.getPreferredSlmAllocationSize()));
}
