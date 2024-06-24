/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/os_interface/linux/product_helper_linux_tests.h"

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/linux/i915.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/helpers/mock_product_helper_hw.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <cstring>

using namespace NEO;

struct MockProductHelperTestLinux : ProductHelperTestLinux {
    void SetUp() override {
        ProductHelperTestLinux::SetUp();

        testPlatform->eRenderCoreFamily = defaultHwInfo->platform.eRenderCoreFamily;

        raii = new RAIIProductHelperFactory<MockProductHelperHw<IGFX_UNKNOWN>>(*this->executionEnvironment->rootDeviceEnvironments[0]);
        mockProductHelper = raii->mockProductHelper;
    }

    void TearDown() override {
        delete raii;
        ProductHelperTestLinux::TearDown();
    }

    RAIIProductHelperFactory<MockProductHelperHw<IGFX_UNKNOWN>> *raii;

    MockProductHelperHw<IGFX_UNKNOWN> *mockProductHelper;
};

TEST_F(MockProductHelperTestLinux, GivenDummyConfigWhenConfiguringHwInfoThenSucceeds) {
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
}

HWTEST2_F(MockProductHelperTestLinux, givenDebugFlagSetWhenEnablingBlitterOperationsSupportThenIgnore, IsAtMostGen11) {
    DebugManagerStateRestore restore{};
    HardwareInfo hardwareInfo = *defaultHwInfo;

    debugManager.flags.EnableBlitterOperationsSupport.set(1);
    mockProductHelper->configureHardwareCustom(&hardwareInfo, nullptr);
    EXPECT_FALSE(hardwareInfo.capabilityTable.blitterOperationsSupported);
}

HWTEST2_F(MockProductHelperTestLinux, givenUnsupportedChipsetUniqueUUIDWhenGettingUuidThenReturnFalse, IsAtMostGen11) {
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    std::array<uint8_t, ProductHelper::uuidSize> id;
    EXPECT_FALSE(productHelper.getUuid(nullptr, 0u, 0u, id));
}

TEST_F(MockProductHelperTestLinux, GivenDummyConfigThenEdramIsDetected) {
    mockProductHelper->use128MbEdram = true;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(1u, outHwInfo.featureTable.flags.ftrEDram);
}

TEST_F(MockProductHelperTestLinux, givenEnabledPlatformCoherencyWhenConfiguringHwInfoThenIgnoreAndSetAsDisabled) {
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSupportsCoherency);
}

TEST_F(MockProductHelperTestLinux, givenDisabledPlatformCoherencyWhenConfiguringHwInfoThenSetValidCapability) {
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSupportsCoherency);
}

TEST_F(MockProductHelperTestLinux, GivenFailGetEuCountWhenConfiguringHwInfoThenFails) {
    drm->storedRetValForEUVal = -4;
    drm->failRetTopology = true;

    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(-4, ret);
}

TEST_F(MockProductHelperTestLinux, GivenFailGetSsCountWhenConfiguringHwInfoThenFails) {
    drm->storedRetValForSSVal = -5;
    drm->failRetTopology = true;

    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(-5, ret);
}

TEST_F(MockProductHelperTestLinux, whenFailGettingTopologyThenFallbackToEuCountIoctl) {
    drm->failRetTopology = true;

    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_NE(-1, ret);
}

TEST_F(MockProductHelperTestLinux, givenInvalidTopologyDataWhenConfiguringThenReturnError) {
    auto storedSVal = drm->storedSVal;
    auto storedSSVal = drm->storedSSVal;
    auto storedEUVal = drm->storedEUVal;

    {
        // 0 euCount
        drm->storedSVal = storedSVal;
        drm->storedSSVal = storedSSVal;
        drm->storedEUVal = 0;

        DrmQueryTopologyData topologyData = {};
        EXPECT_FALSE(drm->queryTopology(outHwInfo, topologyData));
    }

    {
        // 0 subSliceCount
        drm->storedSVal = storedSVal;
        drm->storedSSVal = 0;
        drm->storedEUVal = storedEUVal;

        DrmQueryTopologyData topologyData = {};
        EXPECT_FALSE(drm->queryTopology(outHwInfo, topologyData));
    }

    {
        // 0 sliceCount
        drm->storedSVal = 0;
        drm->storedSSVal = storedSSVal;
        drm->storedEUVal = storedEUVal;

        DrmQueryTopologyData topologyData = {};
        EXPECT_FALSE(drm->queryTopology(outHwInfo, topologyData));
    }
}

TEST_F(MockProductHelperTestLinux, GivenFailingCustomConfigWhenConfiguringHwInfoThenFails) {
    mockProductHelper->failOnConfigureHardwareCustom = true;

    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(-1, ret);
}

TEST_F(MockProductHelperTestLinux, whenConfigureHwInfoIsCalledThenAreNonPersistentContextsSupportedReturnsTrue) {
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(drm->areNonPersistentContextsSupported());
}

TEST_F(MockProductHelperTestLinux, whenConfigureHwInfoIsCalledAndPersitentContextIsUnsupportedThenAreNonPersistentContextsSupportedReturnsFalse) {
    drm->storedPersistentContextsSupport = 0;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(drm->areNonPersistentContextsSupported());
}

HWTEST_F(MockProductHelperTestLinux, GivenPreemptionDrmEnabledMidThreadOnWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;

    mockProductHelper->enableMidThreadPreemption = true;

    UnitTestHelper<FamilyType>::setExtraMidThreadPreemptionFlag(pInHwInfo, true);

    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidThread, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(MockProductHelperTestLinux, GivenPreemptionDrmEnabledThreadGroupOnWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    mockProductHelper->enableThreadGroupPreemption = true;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(MockProductHelperTestLinux, givenDebugFlagSetWhenConfiguringHwInfoThenPrintGetParamIoctlsOutput) {
    DebugManagerStateRestore restore;
    debugManager.flags.PrintIoctlEntries.set(true);

    testing::internal::CaptureStdout(); // start capturing
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);

    std::array<std::string, 1> expectedStrings = {{"DRM_IOCTL_I915_GETPARAM: param: I915_PARAM_HAS_SCHEDULER, output value: 7, retCode: 0"

    }};

    debugManager.flags.PrintIoctlEntries.set(false);
    std::string output = testing::internal::GetCapturedStdout(); // stop capturing
    for (const auto &expectedString : expectedStrings) {
        EXPECT_NE(std::string::npos, output.find(expectedString));
    }

    EXPECT_EQ(std::string::npos, output.find("UNKNOWN"));
}

TEST_F(MockProductHelperTestLinux, GivenPreemptionDrmEnabledMidBatchOnWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    mockProductHelper->enableMidBatchPreemption = true;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidBatch, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(MockProductHelperTestLinux, WhenConfiguringHwInfoThenPreemptionIsSupportedPreemptionDrmEnabledNoPreemptionWhenConfiguringHwInfoThenPreemptionIsNotSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(MockProductHelperTestLinux, GivenPreemptionDrmDisabledAllPreemptionWhenConfiguringHwInfoThenPreemptionIsNotSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidThread;
    drm->storedPreemptionSupport = 0;
    mockProductHelper->enableMidThreadPreemption = true;
    mockProductHelper->enableMidBatchPreemption = true;
    mockProductHelper->enableThreadGroupPreemption = true;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    mockProductHelper->enableMidThreadPreemption = true;
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_FALSE(drm->isPreemptionSupported());
}

TEST_F(MockProductHelperTestLinux, GivenPreemptionDrmEnabledAllPreemptionDriverThreadGroupWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::ThreadGroup;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    mockProductHelper->enableMidBatchPreemption = true;
    mockProductHelper->enableThreadGroupPreemption = true;
    mockProductHelper->enableMidThreadPreemption = true;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::ThreadGroup, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(MockProductHelperTestLinux, GivenPreemptionDrmEnabledAllPreemptionDriverMidBatchWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::MidBatch;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    mockProductHelper->enableMidBatchPreemption = true;
    mockProductHelper->enableThreadGroupPreemption = true;
    mockProductHelper->enableMidThreadPreemption = true;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::MidBatch, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(MockProductHelperTestLinux, GivenConfigPreemptionDrmEnabledAllPreemptionDriverDisabledWhenConfiguringHwInfoThenPreemptionIsSupported) {
    pInHwInfo.capabilityTable.defaultPreemptionMode = PreemptionMode::Disabled;
    drm->storedPreemptionSupport =
        I915_SCHEDULER_CAP_ENABLED |
        I915_SCHEDULER_CAP_PRIORITY |
        I915_SCHEDULER_CAP_PREEMPTION;
    mockProductHelper->enableMidBatchPreemption = true;
    mockProductHelper->enableThreadGroupPreemption = true;
    mockProductHelper->enableMidThreadPreemption = true;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(PreemptionMode::Disabled, outHwInfo.capabilityTable.defaultPreemptionMode);
    EXPECT_TRUE(drm->isPreemptionSupported());
}

TEST_F(MockProductHelperTestLinux, givenPlatformEnabledFtrCompressionWhenInitializingThenFlagsAreSet) {
    pInHwInfo.capabilityTable.ftrRenderCompressedImages = true;
    pInHwInfo.capabilityTable.ftrRenderCompressedBuffers = true;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedImages);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrRenderCompressedBuffers);
}

TEST_F(MockProductHelperTestLinux, givenPointerToHwInfoWhenConfigureHwInfoCalledThenRequiedSurfaceSizeIsSettedProperly) {
    EXPECT_EQ(MemoryConstants::pageSize, pInHwInfo.capabilityTable.requiredPreemptionSurfaceSize);
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    auto expectedSize = static_cast<size_t>(outHwInfo.gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte);
    auto &rootDeviceEnvironment = executionEnvironment->rootDeviceEnvironments[0];
    auto &gfxCoreHelper = rootDeviceEnvironment->getHelper<GfxCoreHelper>();
    gfxCoreHelper.adjustPreemptionSurfaceSize(expectedSize, *rootDeviceEnvironment);
    EXPECT_EQ(expectedSize, outHwInfo.capabilityTable.requiredPreemptionSurfaceSize);
}

TEST_F(MockProductHelperTestLinux, givenInstrumentationForHardwareIsEnabledOrDisabledWhenConfiguringHwInfoThenOverrideItUsingHaveInstrumentation) {
    int ret;

    pInHwInfo.capabilityTable.instrumentationEnabled = false;
    ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    ASSERT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.instrumentationEnabled);

    pInHwInfo.capabilityTable.instrumentationEnabled = true;
    ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    ASSERT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.capabilityTable.instrumentationEnabled);
}

TEST_F(MockProductHelperTestLinux, givenGttSizeReturnedWhenInitializingHwInfoThenSetSvmFtr) {
    drm->storedGTTSize = MemoryConstants::max64BitAppAddress;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_FALSE(outHwInfo.capabilityTable.ftrSvm);

    drm->storedGTTSize = MemoryConstants::max64BitAppAddress + 1;
    ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_TRUE(outHwInfo.capabilityTable.ftrSvm);
}

TEST_F(MockProductHelperTestLinux, givenGttSizeReturnedWhenInitializingHwInfoThenSetGpuAddressSpace) {
    drm->storedGTTSize = maxNBitValue(40) + 1;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(drm->storedGTTSize - 1, outHwInfo.capabilityTable.gpuAddressSpace);
}

TEST_F(MockProductHelperTestLinux, givenFailingGttSizeIoctlWhenInitializingHwInfoThenSetDefaultValues) {
    drm->storedRetValForGetGttSize = -1;
    int ret = mockProductHelper->configureHwInfoDrm(&pInHwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);

    EXPECT_TRUE(outHwInfo.capabilityTable.ftrSvm);
    EXPECT_NE(0u, outHwInfo.capabilityTable.gpuAddressSpace);
    EXPECT_EQ(pInHwInfo.capabilityTable.gpuAddressSpace, outHwInfo.capabilityTable.gpuAddressSpace);
}
using HwConfigLinux = ::testing::Test;

HWTEST2_F(HwConfigLinux, givenPlatformWithPlatformQuerySupportedWhenItIsCalledThenReturnTrue, IsAtLeastMtl) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    EXPECT_TRUE(productHelper.isPlatformQuerySupported());
}

HWTEST2_F(HwConfigLinux, GivenDifferentValuesFromTopologyQueryWhenConfiguringHwInfoThenMaxSlicesSupportedSetToAvailableCountInGtSystemInfo, MatchAny) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
    auto drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();
    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.get();
    osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    auto hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    HardwareInfo outHwInfo;
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    hwInfo.gtSystemInfo.MaxSubSlicesSupported = drm->storedSSVal * 2;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = drm->storedSSVal * 2;
    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 16;
    hwInfo.gtSystemInfo.MaxSlicesSupported = drm->storedSVal * 4;

    int ret = productHelper.configureHwInfoDrm(&hwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);

    EXPECT_EQ(static_cast<uint32_t>(drm->storedSSVal * 2), outHwInfo.gtSystemInfo.MaxSubSlicesSupported);
    EXPECT_EQ(static_cast<uint32_t>(drm->storedSSVal * 2), outHwInfo.gtSystemInfo.MaxDualSubSlicesSupported);
    EXPECT_EQ(16u, outHwInfo.gtSystemInfo.MaxEuPerSubSlice);
    EXPECT_EQ(static_cast<uint32_t>(drm->storedSVal), outHwInfo.gtSystemInfo.MaxSlicesSupported);

    drm->storedSVal = 3;
    drm->storedSSVal = 12;
    drm->storedEUVal = 12 * 8;

    hwInfo.gtSystemInfo.MaxSubSlicesSupported = drm->storedSSVal / 2;
    hwInfo.gtSystemInfo.MaxDualSubSlicesSupported = drm->storedSSVal / 2;
    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 6;
    hwInfo.gtSystemInfo.MaxSlicesSupported = drm->storedSVal / 2;

    ret = productHelper.configureHwInfoDrm(&hwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);

    EXPECT_EQ(12u, outHwInfo.gtSystemInfo.MaxSubSlicesSupported);
    EXPECT_EQ(6u, outHwInfo.gtSystemInfo.MaxEuPerSubSlice); // MaxEuPerSubslice is preserved
    EXPECT_EQ(static_cast<uint32_t>(drm->storedSVal), outHwInfo.gtSystemInfo.MaxSlicesSupported);

    EXPECT_EQ(outHwInfo.gtSystemInfo.MaxSubSlicesSupported, outHwInfo.gtSystemInfo.MaxDualSubSlicesSupported);

    hwInfo.gtSystemInfo.MaxEuPerSubSlice = 0;

    ret = productHelper.configureHwInfoDrm(&hwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);
    EXPECT_EQ(8u, outHwInfo.gtSystemInfo.MaxEuPerSubSlice);
}

HWTEST2_F(HwConfigLinux, givenSliceCountWhenConfigureHwInfoDrmThenProperInitializationInSliceInfoEnabled, MatchAny) {
    auto executionEnvironment = std::make_unique<ExecutionEnvironment>();
    executionEnvironment->prepareRootDeviceEnvironments(1);

    executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(NEO::defaultHwInfo.get());
    auto drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
    executionEnvironment->rootDeviceEnvironments[0]->osInterface = std::make_unique<OSInterface>();

    auto osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.get();
    osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

    auto hwInfo = *executionEnvironment->rootDeviceEnvironments[0]->getHardwareInfo();
    HardwareInfo outHwInfo;
    auto &productHelper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<ProductHelper>();
    uint32_t sliceCount = 4;
    drm->storedSVal = sliceCount;
    hwInfo.gtSystemInfo.SliceCount = sliceCount;

    int ret = productHelper.configureHwInfoDrm(&hwInfo, &outHwInfo, *executionEnvironment->rootDeviceEnvironments[0].get());
    EXPECT_EQ(0, ret);

    for (uint32_t i = 0; i < sliceCount; i++) {
        EXPECT_TRUE(outHwInfo.gtSystemInfo.SliceInfo[i].Enabled);
    }
}

HWTEST2_F(ProductHelperTest, givenProductHelperWhenIsPlatformQueryNotSupportedThenReturnFalse, IsAtLeastMtl) {

    EXPECT_TRUE(productHelper->isPlatformQuerySupported());
}
