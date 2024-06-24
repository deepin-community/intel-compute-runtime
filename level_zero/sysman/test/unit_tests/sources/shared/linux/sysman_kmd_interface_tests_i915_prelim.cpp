/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"

#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_hw_device_id.h"
#include "level_zero/sysman/test/unit_tests/sources/shared/linux/sysman_kmd_interface_tests.h"

#include "gtest/gtest.h"

namespace L0 {
namespace Sysman {
namespace ult {

using namespace NEO;

static const uint32_t mockReadVal = 23;

static int mockReadLinkSuccess(const char *path, char *buf, size_t bufsize) {
    constexpr size_t sizeofPath = sizeof("/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
    strcpy_s(buf, sizeofPath, "/sys/devices/pci0000:00/0000:00:01.0/0000:01:00.0/0000:02:01.0/0000:03:00.0");
    return sizeofPath;
}

static int mockReadLinkFailure(const char *path, char *buf, size_t bufsize) {
    errno = ENOENT;
    return -1;
}

static ssize_t mockReadSuccess(int fd, void *buf, size_t count, off_t offset) {
    std::ostringstream oStream;
    oStream << mockReadVal;
    std::string value = oStream.str();
    memcpy(buf, value.data(), count);
    return count;
}

static ssize_t mockReadFailure(int fd, void *buf, size_t count, off_t offset) {
    errno = ENOENT;
    return -1;
}

class SysmanFixtureDeviceI915Prelim : public SysmanDeviceFixture {
  protected:
    L0::Sysman::SysmanDevice *device = nullptr;

    void SetUp() override {
        SysmanDeviceFixture::SetUp();
        device = pSysmanDevice;
        pLinuxSysmanImp->pSysmanKmdInterface.reset(new SysmanKmdInterfaceI915Prelim(pLinuxSysmanImp->getProductFamily()));
        mockInitFsAccess();
    }

    void mockInitFsAccess() {
        VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
        pLinuxSysmanImp->pSysmanKmdInterface->initFsAccessInterface(*pLinuxSysmanImp->getDrm());
    }

    void TearDown() override {
        SysmanDeviceFixture::TearDown();
    }
};

TEST_F(SysmanFixtureDeviceI915Prelim, GivenI915PrelimVersionWhenSysmanKmdInterfaceInstanceIsCreatedThenValidPtrIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_NE(nullptr, pSysmanKmdInterface);
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetHwmonNameThenEmptyNameIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_STREQ("i915_gt0", pSysmanKmdInterface->getHwmonName(0, true).c_str());
    EXPECT_STREQ("i915", pSysmanKmdInterface->getHwmonName(0, false).c_str());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetEngineBasePathThenEmptyPathIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_STREQ("", pSysmanKmdInterface->getEngineBasePath(0).c_str());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceWhenGettingSysfsFileNamesThenProperPathsAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    bool baseDirectoryExists = true;
    EXPECT_STREQ("gt/gt0/rps_min_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/rps_max_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/.defaults/rps_min_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinDefaultFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/.defaults/rps_max_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxDefaultFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/rps_boost_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameBoostFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/punit_req_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameCurrentFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/rapl_PL1_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameTdpFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/rps_act_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameActualFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/rps_RP1_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameEfficientFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/rps_RP0_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxValueFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/rps_RPn_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinValueFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/throttle_reason_status", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonStatus, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/throttle_reason_pl1", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL1, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/throttle_reason_pl2", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL2, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/throttle_reason_pl4", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonPL4, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/throttle_reason_thermal", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameThrottleReasonThermal, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/addr_range", pSysmanKmdInterface->getSysfsFilePathForPhysicalMemorySize(0).c_str());
    EXPECT_STREQ("gt/gt0/mem_RP0_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMaxMemoryFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/mem_RPn_freq_mhz", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameMinMemoryFrequency, 0, baseDirectoryExists).c_str());
    EXPECT_STREQ("gt/gt0/rc6_enable", pSysmanKmdInterface->getSysfsFilePath(SysfsName::sysfsNameStandbyModeControl, 0, baseDirectoryExists).c_str());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetNativeUnitWithProperSysfsNameThenValidValuesAreReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_EQ(SysmanKmdInterface::SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerTimeout));
    EXPECT_EQ(SysmanKmdInterface::SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerTimeslice));
    EXPECT_EQ(SysmanKmdInterface::SysfsValueUnit::milli, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSchedulerWatchDogTimeout));
    EXPECT_EQ(SysmanKmdInterface::SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameSustainedPowerLimit));
    EXPECT_EQ(SysmanKmdInterface::SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameCriticalPowerLimit));
    EXPECT_EQ(SysmanKmdInterface::SysfsValueUnit::micro, pSysmanKmdInterface->getNativeUnit(SysfsName::sysfsNameDefaultPowerLimit));
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetEngineActivityFdThenInvalidFdisReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    auto pPmuInterface = std::make_unique<MockPmuInterfaceImp>(pLinuxSysmanImp);
    EXPECT_EQ(-1, pSysmanKmdInterface->getEngineActivityFd(ZES_ENGINE_GROUP_COMPUTE_SINGLE, 0, 0, pPmuInterface.get()));
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCheckingSupportForI915DriverThenProperStatusIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_FALSE(pSysmanKmdInterface->clientInfoAvailableInFdInfo());
    EXPECT_FALSE(pSysmanKmdInterface->isGroupEngineInterfaceAvailable());
    EXPECT_FALSE(pSysmanKmdInterface->useDefaultMaximumWatchdogTimeoutForExclusiveMode());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCheckingSupportForStandbyModeThenProperStatusIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_TRUE(pSysmanKmdInterface->isStandbyModeControlAvailable());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceAndIsIntegratedDeviceWhenGetEventsIsCalledThenValidEventTypeIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool isIntegratedDevice = true;
    EXPECT_EQ(mockReadVal, pSysmanKmdInterface->getEventType(isIntegratedDevice));
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceAndIsNotIntegratedDeviceWhenGetEventsIsCalledThenValidEventTypeIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkSuccess);
    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadSuccess);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool isIntegratedDevice = false;
    EXPECT_EQ(mockReadVal, pSysmanKmdInterface->getEventType(isIntegratedDevice));
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceAndIsNotIntegratedDeviceAndReadSymLinkFailsWhenGetEventsIsCalledThenFailureIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsReadlink)> mockReadLink(&NEO::SysCalls::sysCallsReadlink, &mockReadLinkFailure);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool isIntegratedDevice = false;
    EXPECT_EQ(0u, pSysmanKmdInterface->getEventType(isIntegratedDevice));
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceAndIsNotIntegratedDeviceAndFsReadFailsWhenGetEventsIsCalledThenFailureIsReturned) {

    VariableBackup<decltype(NEO::SysCalls::sysCallsPread)> mockPread(&NEO::SysCalls::sysCallsPread, &mockReadFailure);

    auto pSysmanKmdInterface = pLinuxSysmanImp->pSysmanKmdInterface.get();
    bool isIntegratedDevice = false;
    EXPECT_EQ(0u, pSysmanKmdInterface->getEventType(isIntegratedDevice));
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCheckingAvailabilityOfFrequencyFilesThenTrueValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_TRUE(pSysmanKmdInterface->isDefaultFrequencyAvailable());
    EXPECT_TRUE(pSysmanKmdInterface->isBoostFrequencyAvailable());
    EXPECT_TRUE(pSysmanKmdInterface->isTdpFrequencyAvailable());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCheckingPhysicalMemorySizeAvailabilityThenTrueValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_TRUE(pSysmanKmdInterface->isPhysicalMemorySizeSupported());
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetEngineClassStringThenInvalidValueIsReturned) {
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    EXPECT_EQ(std::nullopt, pSysmanKmdInterface->getEngineClassString(EngineClass::ENGINE_CLASS_COMPUTE));
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetNumEngineTypeAndInstancesThenErrorIsReturned) {
    std::vector<std::string> mockVecString = {"rcs"};
    std::map<zes_engine_type_flag_t, std::vector<std::string>> mockMapofEngine = {{ZES_ENGINE_TYPE_FLAG_RENDER, mockVecString}};
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, pSysmanKmdInterface->getNumEngineTypeAndInstances(
                                                       mockMapofEngine, pLinuxSysmanImp, nullptr, true, 0));
}

TEST_F(SysmanFixtureDeviceI915Prelim, GivenSysmanKmdInterfaceInstanceWhenCallingGetDeviceWedgedStatusThenVerifyDeviceIsNotWedged) {
    class DrmMock : public Drm {
      public:
        DrmMock(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<MockSysmanHwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {}
        using Drm::setupIoctlHelper;
        int ioctlRetVal = 0;
        int ioctlErrno = 0;
        int mockFd = 33;
        int ioctl(DrmIoctl request, void *arg) override {
            return ioctlRetVal;
        }
        int getErrno() override { return ioctlErrno; }
    };
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    auto pDrm = new DrmMock(const_cast<NEO::RootDeviceEnvironment &>(pSysmanDeviceImp->getRootDeviceEnvironment()));
    pDrm->setupIoctlHelper(pSysmanDeviceImp->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
    auto &osInterface = pSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    osInterface->setDriverModel(std::unique_ptr<DrmMock>(pDrm));
    auto pSysmanKmdInterface = pLinuxSysmanImp->getSysmanKmdInterface();
    zes_device_state_t deviceState = {};
    pSysmanKmdInterface->getWedgedStatus(pLinuxSysmanImp, &deviceState);
    EXPECT_EQ(0u, deviceState.reset & ZES_RESET_REASON_FLAG_WEDGED);
}

} // namespace ult
} // namespace Sysman
} // namespace L0
