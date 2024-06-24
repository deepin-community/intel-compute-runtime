/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gtest_helpers.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using CompilerProductHelperFixture = Test<DeviceFixture>;

HWTEST_F(CompilerProductHelperFixture, WhenIsMidThreadPreemptionIsSupportedIsCalledThenCorrectResultIsReturned) {
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    UnitTestHelper<FamilyType>::setExtraMidThreadPreemptionFlag(hwInfo, false);
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    EXPECT_FALSE(compilerProductHelper.isMidThreadPreemptionSupported(hwInfo));
    UnitTestHelper<FamilyType>::setExtraMidThreadPreemptionFlag(hwInfo, true);
    EXPECT_TRUE(compilerProductHelper.isMidThreadPreemptionSupported(hwInfo));
}

TEST(CompilerProductHelperTest, WhenCompilerProductHelperCreateIsCalledWithUnknownProductThenNullptrIsReturned) {
    EXPECT_EQ(nullptr, CompilerProductHelper::create(IGFX_UNKNOWN));
}

using IsBeforeXeHpc = IsBeforeGfxCore<IGFX_XE_HPC_CORE>;

HWTEST2_F(CompilerProductHelperFixture, GivenProductBeforeXeHpcWhenIsForceToStatelessRequiredThenFalseIsReturned, IsBeforeXeHpc) {
    auto &compilerProductHelper = getHelper<CompilerProductHelper>();
    EXPECT_FALSE(compilerProductHelper.isForceToStatelessRequired());
}

using IsAtLeastXeHpc = IsAtLeastGfxCore<IGFX_XE_HPC_CORE>;

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpcAndLaterWhenIsForceToStatelessRequiredThenCorrectResultIsReturned, IsAtLeastXeHpc) {
    DebugManagerStateRestore restorer;
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    EXPECT_TRUE(compilerProductHelper.isForceToStatelessRequired());

    debugManager.flags.DisableForceToStateless.set(false);
    EXPECT_TRUE(compilerProductHelper.isForceToStatelessRequired());

    debugManager.flags.DisableForceToStateless.set(true);
    EXPECT_FALSE(compilerProductHelper.isForceToStatelessRequired());
}

HWTEST2_F(CompilerProductHelperFixture, GivenGen11AndLaterThenSubgroupLocalBlockIoIsSupported, IsAtLeastGen11) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isSubgroupLocalBlockIoSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenGen9OrBeforeThenSubgroupLocalBlockIoIsNotSupported, IsAtMostGen9) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isSubgroupLocalBlockIoSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpAndLaterThenDotAccumulateIsSupported, IsAtLeastXeHpCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isDotAccumulateSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpThenDotAccumulateIsNotSupported, IsAtMostGen12lp) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isDotAccumulateSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpAndLaterThenCreateBufferWithPropertiesIsSupported, IsAtLeastXeHpCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isCreateBufferWithPropertiesSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpThenCreateBufferWithPropertiesIsNotSupported, IsAtMostGen12lp) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isCreateBufferWithPropertiesSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpcAndLaterThenSubgroupNamedBarrierIsSupported, IsAtLeastXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isSubgroupNamedBarrierSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpcThenSubgroupNamedBarrierIsNotSupported, IsAtMostXeHpgCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isSubgroupNamedBarrierSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenXeHpcAndLaterThenSubgroupExtendedBlockReadIsSupported, IsAtLeastXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isSubgroupExtendedBlockReadSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenPreXeHpcThenSubgroupExtendedBlockReadIsNotSupported, IsAtMostXeHpgCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isSubgroupExtendedBlockReadSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenCompilerProductHelperThenBFloat16ConversionIsSupportedBasedOnReleaseHelper, IsNotXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    if (releaseHelper) {

        EXPECT_EQ(releaseHelper->isBFloat16ConversionSupported(), compilerProductHelper.isBFloat16ConversionSupported(releaseHelper));
    } else {
        EXPECT_FALSE(compilerProductHelper.isBFloat16ConversionSupported(releaseHelper));
    }
}

HWTEST2_F(CompilerProductHelperFixture, GivenReleaseHelperThenBFloat16ConversionIsSupported, IsXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();
    EXPECT_TRUE(compilerProductHelper.isBFloat16ConversionSupported(releaseHelper));
}

HWTEST2_F(CompilerProductHelperFixture, GivenReleaseHelperThenMatrixMultiplyAccumulateIsSupportedBasedOnReleaseHelper, IsNotXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    if (releaseHelper) {

        EXPECT_EQ(releaseHelper->isMatrixMultiplyAccumulateSupported(), compilerProductHelper.isMatrixMultiplyAccumulateSupported(releaseHelper));
    } else {
        EXPECT_FALSE(compilerProductHelper.isMatrixMultiplyAccumulateSupported(releaseHelper));
    }
}

HWTEST2_F(CompilerProductHelperFixture, GivenReleaseHelperThenDotProductAccumulateSystolicIsSupportedBasedOnReleaseHelper, IsNotXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    if (releaseHelper) {

        EXPECT_EQ(releaseHelper->isDotProductAccumulateSystolicSupported(), compilerProductHelper.isDotProductAccumulateSystolicSupported(releaseHelper));
    } else {
        EXPECT_FALSE(compilerProductHelper.isDotProductAccumulateSystolicSupported(releaseHelper));
    }
}

HWTEST2_F(CompilerProductHelperFixture, GivenReleaseHelperThenMatrixMultiplyAccumulateIsSupported, IsXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    EXPECT_TRUE(compilerProductHelper.isMatrixMultiplyAccumulateSupported(releaseHelper));
}

HWTEST2_F(CompilerProductHelperFixture, GivenReleaseHelperThenDotProductAccumulateSystolicIsSupported, IsXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    EXPECT_TRUE(compilerProductHelper.isDotProductAccumulateSystolicSupported(releaseHelper));
}

HWTEST2_F(CompilerProductHelperFixture, GivenReleaseHelperThenSplitMatrixMultiplyAccumulateIsSupportedBasedOnReleaseHelper, IsNotXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    if (releaseHelper) {

        EXPECT_EQ(releaseHelper->isSplitMatrixMultiplyAccumulateSupported(), compilerProductHelper.isSplitMatrixMultiplyAccumulateSupported(releaseHelper));
    } else {
        EXPECT_FALSE(compilerProductHelper.isSplitMatrixMultiplyAccumulateSupported(releaseHelper));
    }
}

HWTEST2_F(CompilerProductHelperFixture, GivenReleaseHelperThenSplitMatrixMultiplyAccumulateIsNotSupported, IsXeHpcCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto releaseHelper = pDevice->getReleaseHelper();

    EXPECT_FALSE(compilerProductHelper.isSplitMatrixMultiplyAccumulateSupported(releaseHelper));
}

HWTEST2_F(CompilerProductHelperFixture, givenAotConfigWhenSetHwInfoRevisionIdThenCorrectValueIsSet, IsAtMostDg2) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto hwInfo = *defaultHwInfo;
    auto productConfig = compilerProductHelper.getHwIpVersion(*defaultHwInfo);
    HardwareIpVersion aotConfig = {0};
    aotConfig.value = productConfig;
    compilerProductHelper.setProductConfigForHwInfo(hwInfo, aotConfig);
    EXPECT_EQ(hwInfo.platform.usRevId, aotConfig.revision);
    EXPECT_EQ(hwInfo.ipVersion.value, aotConfig.value);
}

HWTEST2_F(CompilerProductHelperFixture, givenAtMostXeHPWhenGetCachingPolicyOptionsThenReturnNullptr, IsAtMostXeHpCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    EXPECT_EQ(compilerProductHelper.getCachingPolicyOptions(false), nullptr);
    EXPECT_EQ(compilerProductHelper.getCachingPolicyOptions(true), nullptr);
}

HWTEST2_F(CompilerProductHelperFixture, givenAtLeastXeHpgCoreWhenGetCachingPolicyOptionsThenReturnWriteByPassPolicyOption, IsAtLeastXeHpgCore) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    const char *expectedStr = "-cl-store-cache-default=2 -cl-load-cache-default=4";
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(CompilerProductHelperFixture, givenAtLeastXeHpgCoreWhenGetCachingPolicyOptionsThenReturnWriteBackPolicyOption, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);

    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    const char *expectedStr = "-cl-store-cache-default=7 -cl-load-cache-default=4";
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(CompilerProductHelperFixture, givenAtLeastXeHpgCoreAndDebugFlagSetForceAllResourcesUncachedWhenGetCachingPolicyOptionsThenReturnUncachedPolicyOption, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(2);
    debugManager.flags.ForceAllResourcesUncached.set(true);

    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    const char *expectedStr = "-cl-store-cache-default=2 -cl-load-cache-default=2";
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(false), expectedStr, strlen(expectedStr)));
    EXPECT_EQ(0, memcmp(compilerProductHelper.getCachingPolicyOptions(true), expectedStr, strlen(expectedStr)));
}

HWTEST2_F(CompilerProductHelperFixture, givenCachePolicyWithoutCorrespondingBuildOptionWhenGetCachingPolicyOptionsThenReturnNullptr, IsAtLeastXeHpgCore) {
    DebugManagerStateRestore restorer;
    debugManager.flags.OverrideL1CachePolicyInSurfaceStateAndStateless.set(5);
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_EQ(nullptr, compilerProductHelper.getCachingPolicyOptions(false));
    EXPECT_EQ(nullptr, compilerProductHelper.getCachingPolicyOptions(true));
}

TEST_F(CompilerProductHelperFixture, givenHwInfoWithIndependentForwardProgressThenReportsClKhrSubgroupExtension) {

    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto *releaseHelper = getReleaseHelper();
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.supportsIndependentForwardProgress = true;
    auto extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_khr_subgroups")));

    hwInfo.capabilityTable.supportsIndependentForwardProgress = false;
    extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_FALSE(hasSubstr(extensions, std::string("cl_khr_subgroups")));
}

TEST_F(CompilerProductHelperFixture, givenHwInfoWithCLVersionAtLeast20ThenReportsClExtFloatAtomicsExtension) {

    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto *releaseHelper = getReleaseHelper();
    auto hwInfo = *defaultHwInfo;
    hwInfo.capabilityTable.clVersionSupport = 20;
    auto extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_ext_float_atomics")));

    hwInfo.capabilityTable.clVersionSupport = 21;
    extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_ext_float_atomics")));

    hwInfo.capabilityTable.clVersionSupport = 30;
    extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_ext_float_atomics")));

    hwInfo.capabilityTable.clVersionSupport = 12;
    extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_FALSE(hasSubstr(extensions, std::string("cl_ext_float_atomics")));
}

TEST_F(CompilerProductHelperFixture, givenHwInfoWithCLVersion30ThenReportsClKhrExternalMemoryExtension) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto *releaseHelper = getReleaseHelper();
    auto hwInfo = *defaultHwInfo;

    hwInfo.capabilityTable.clVersionSupport = 30;
    auto extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_TRUE(hasSubstr(extensions, std::string("cl_khr_external_memory")));

    hwInfo.capabilityTable.clVersionSupport = 21;
    extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_FALSE(hasSubstr(extensions, std::string("cl_khr_external_memory")));

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ClKhrExternalMemoryExtension.set(0);

    hwInfo.capabilityTable.clVersionSupport = 30;
    extensions = compilerProductHelper.getDeviceExtensions(hwInfo, releaseHelper);
    EXPECT_FALSE(hasSubstr(extensions, std::string("cl_khr_external_memory")));
}

HWTEST2_F(CompilerProductHelperFixture, GivenAtMostGen11DeviceWhenCheckingIfIntegerDotExtensionIsSupportedThenFalseReturned, IsAtMostGen11) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_FALSE(compilerProductHelper.isDotIntegerProductExtensionSupported());
}

HWTEST2_F(CompilerProductHelperFixture, GivenAtLeastGen12lpDeviceWhenCheckingIfIntegerDotExtensionIsSupportedThenTrueReturned, IsAtLeastGen12lp) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();

    EXPECT_TRUE(compilerProductHelper.isDotIntegerProductExtensionSupported());
}

HWTEST2_F(CompilerProductHelperFixture, givenConfigWhenMatchConfigWithRevIdThenProperConfigIsReturned, IsNotPvcOrDg2) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    auto config = hwInfo.ipVersion.value;
    EXPECT_EQ(compilerProductHelper.matchRevisionIdWithProductConfig(config, 0x0), config);
    EXPECT_EQ(compilerProductHelper.matchRevisionIdWithProductConfig(config, 0x1), config);
    EXPECT_EQ(compilerProductHelper.matchRevisionIdWithProductConfig(config, 0x4), config);
}

HWTEST_F(CompilerProductHelperFixture, givenProductHelperWhenGetAndOverrideHwIpVersionThenCorrectMatchIsFound) {
    DebugManagerStateRestore stateRestore;
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    uint32_t config = 0x1234;
    debugManager.flags.OverrideHwIpVersion.set(config);
    hwInfo.ipVersion.value = 0x5678;
    EXPECT_EQ(compilerProductHelper.getHwIpVersion(hwInfo), config);
}

HWTEST_F(CompilerProductHelperFixture, givenCompilerProductHelperWhenIsHeaplessModeEnabledThenFalseIsReturned) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    EXPECT_FALSE(compilerProductHelper.isHeaplessModeEnabled());
}

HWTEST_F(CompilerProductHelperFixture, WhenFullListOfSupportedOpenCLCVersionsIsRequestedThenReturnsListOfAllSupportedVersionsByTheAssociatedDevice) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto versions = compilerProductHelper.getDeviceOpenCLCVersions(pDevice->getHardwareInfo(), NEO::OclCVersion{3, 0});
    ASSERT_LT(3U, versions.size());

    EXPECT_EQ(1, versions[0].major);
    EXPECT_EQ(0, versions[0].minor);

    EXPECT_EQ(1, versions[1].major);
    EXPECT_EQ(1, versions[1].minor);

    EXPECT_EQ(1, versions[2].major);
    EXPECT_EQ(2, versions[2].minor);

    if (pDevice->getHardwareInfo().capabilityTable.clVersionSupport == 30) {
        ASSERT_EQ(4U, versions.size());
        EXPECT_EQ(3, versions[3].major);
        EXPECT_EQ(0, versions[3].minor);
    } else {
        EXPECT_EQ(3U, versions.size());
    }
}

HWTEST_F(CompilerProductHelperFixture, WhenLimitedListOfSupportedOpenCLCVersionsIsRequestedThenReturnsListOfAllSupportedVersionsByTheAssociatedDeviceTrimmedToProvidedMax) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto versions = compilerProductHelper.getDeviceOpenCLCVersions(pDevice->getHardwareInfo(), NEO::OclCVersion{1, 1});
    ASSERT_EQ(2U, versions.size());

    EXPECT_EQ(1, versions[0].major);
    EXPECT_EQ(0, versions[0].minor);

    EXPECT_EQ(1, versions[1].major);
    EXPECT_EQ(1, versions[1].minor);
}

HWTEST_F(CompilerProductHelperFixture, GivenRequestForLimitedListOfSupportedOpenCLCVersionsWhenMaxVersionIsEmptyThenReturnsListOfAllSupportedVersionsByTheAssociatedDevice) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto versions = compilerProductHelper.getDeviceOpenCLCVersions(pDevice->getHardwareInfo(), NEO::OclCVersion{0, 0});
    ASSERT_LT(3U, versions.size());

    EXPECT_EQ(1, versions[0].major);
    EXPECT_EQ(0, versions[0].minor);

    EXPECT_EQ(1, versions[1].major);
    EXPECT_EQ(1, versions[1].minor);

    EXPECT_EQ(1, versions[2].major);
    EXPECT_EQ(2, versions[2].minor);

    if (pDevice->getHardwareInfo().capabilityTable.clVersionSupport == 30) {
        ASSERT_EQ(4U, versions.size());
        EXPECT_EQ(3, versions[3].major);
        EXPECT_EQ(0, versions[3].minor);
    } else {
        EXPECT_EQ(3U, versions.size());
    }
}

HWTEST_F(CompilerProductHelperFixture, GivenRequestForLimitedListOfSupportedOpenCLCVersionsWhenMaxVersionIsBelow10ThenReturnsListOfAllSupportedVersionsByTheAssociatedDeviceTrimmedToOclC12) {
    auto &compilerProductHelper = pDevice->getCompilerProductHelper();
    auto versions = compilerProductHelper.getDeviceOpenCLCVersions(pDevice->getHardwareInfo(), NEO::OclCVersion{0, 1});
    ASSERT_EQ(3U, versions.size());

    EXPECT_EQ(1, versions[0].major);
    EXPECT_EQ(0, versions[0].minor);

    EXPECT_EQ(1, versions[1].major);
    EXPECT_EQ(1, versions[1].minor);

    EXPECT_EQ(1, versions[2].major);
    EXPECT_EQ(2, versions[2].minor);
}
