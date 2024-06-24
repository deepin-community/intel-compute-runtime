/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/libult/linux/drm_mock.h"

#include "level_zero/tools/test/unit_tests/sources/sysman/linux/mock_sysman_fixture.h"

#include "mock_engine_prelim.h"

extern bool sysmanUltsEnable;

class OsEngine;
namespace L0 {
namespace ult {
constexpr uint32_t handleComponentCount = 13u;
constexpr uint32_t handleCountForMultiDeviceFixture = 7u;
class ZesEngineFixture : public SysmanDeviceFixture {
  protected:
    std::vector<ze_device_handle_t> deviceHandles;
    std::unique_ptr<MockEngineNeoDrm> pDrm;
    std::unique_ptr<MockEnginePmuInterfaceImp> pPmuInterface;
    Drm *pOriginalDrm = nullptr;
    PmuInterface *pOriginalPmuInterface = nullptr;
    MemoryManager *pMemoryManagerOriginal = nullptr;
    std::unique_ptr<MockMemoryManagerInEngineSysman> pMemoryManager;
    std::unique_ptr<MockEngineSysfsAccess> pSysfsAccess;
    SysfsAccess *pSysfsAccessOriginal = nullptr;
    std::unique_ptr<MockEngineFsAccess> pFsAccess;
    FsAccess *pFsAccessOriginal = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanDeviceFixture::SetUp();
        pMemoryManagerOriginal = device->getDriverHandle()->getMemoryManager();
        pMemoryManager = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
        pMemoryManager->localMemorySupported[0] = false;
        device->getDriverHandle()->setMemoryManager(pMemoryManager.get());

        pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockEngineSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<MockEngineFsAccess>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

        pDrm = std::make_unique<MockEngineNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(neoDevice->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
        pPmuInterface = std::make_unique<MockEnginePmuInterfaceImp>(pLinuxSysmanImp);
        pOriginalDrm = pLinuxSysmanImp->pDrm;
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pLinuxSysmanImp->pDrm = pDrm.get();
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

        pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        // We received a device handle. Check for subdevices in this device
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        getEngineHandles(0);
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        device->getDriverHandle()->setMemoryManager(pMemoryManagerOriginal);
        pLinuxSysmanImp->pDrm = pOriginalDrm;
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;

        SysmanDeviceFixture::TearDown();
    }

    std::vector<zes_engine_handle_t> getEngineHandles(uint32_t count) {
        std::vector<zes_engine_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesEngineFixture, GivenComponentCountZeroWhenCallingzesDeviceEnumEngineGroupsThenNonZeroCountIsReturnedAndVerifyCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, handleComponentCount);

    uint32_t testcount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &testcount, NULL));
    EXPECT_EQ(testcount, count);

    count = 0;
    std::vector<zes_engine_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, handleComponentCount);
}

TEST_F(ZesEngineFixture, GivenPmuOpenFailsWhenCallingzesDeviceEnumEngineGroupsThenNoHandlesAreEnumerated) {
    auto pMemoryManagerTest = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
    pMemoryManagerTest->localMemorySupported[0] = true;
    device->getDriverHandle()->setMemoryManager(pMemoryManagerTest.get());
    pSysfsAccess->mockReadVal = 1;
    pSysfsAccess->mockReadSymLinkSuccess = true;
    pPmuInterface->mockPerfEventOpenRead = true;
    pPmuInterface->mockPerfEventOpenFailAtCount = 3;
    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(deviceHandles);

    uint32_t handleCount = 0;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &handleCount, nullptr), ZE_RESULT_SUCCESS);
    EXPECT_EQ(handleCount, 0u);
}

TEST_F(ZesEngineFixture, GivenPmuOpenFailsDueToTooManyOpenFilesWhenCallingzesDeviceEnumEngineGroupsThenErrorIsObserved) {
    auto pMemoryManagerTest = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
    pMemoryManagerTest->localMemorySupported[0] = true;
    device->getDriverHandle()->setMemoryManager(pMemoryManagerTest.get());
    pSysfsAccess->mockReadVal = 1;
    pSysfsAccess->mockReadSymLinkSuccess = true;
    pPmuInterface->mockPerfEventOpenRead = true;
    pPmuInterface->mockPerfEventOpenFailAtCount = 3;
    pPmuInterface->mockErrorNumber = EMFILE;
    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(deviceHandles);

    uint32_t handleCount = 0;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &handleCount, nullptr), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
    EXPECT_EQ(handleCount, 0u);
}

TEST_F(ZesEngineFixture, GivenPmuOpenFailsDueToTooManyOpenFilesInSystemWhenEnumeratingEngineGroupsThenErrorIsObserved) {
    auto pMemoryManagerTest = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
    pMemoryManagerTest->localMemorySupported[0] = true;
    device->getDriverHandle()->setMemoryManager(pMemoryManagerTest.get());
    pSysfsAccess->mockReadVal = 1;
    pSysfsAccess->mockReadSymLinkSuccess = true;
    pPmuInterface->mockPerfEventOpenRead = true;
    pPmuInterface->mockPerfEventOpenFailAtCount = 3;
    pPmuInterface->mockErrorNumber = ENFILE;
    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(deviceHandles);

    uint32_t handleCount = 0;
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &handleCount, nullptr), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
    EXPECT_EQ(handleCount, 0u);
}

TEST_F(ZesEngineFixture, GivenValidEngineHandlesWhenCallingZesEngineGetPropertiesThenVerifyCallSucceeds) {
    zes_engine_properties_t properties;
    auto handles = getEngineHandles(handleComponentCount);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[0], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_ALL, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[1], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_COMPUTE_ALL, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[2], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ALL, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[3], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_COPY_ALL, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[4], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_COMPUTE_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[5], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_RENDER_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[6], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[7], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[8], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[9], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[10], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_COPY_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[11], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE, properties.type);
    EXPECT_FALSE(properties.onSubdevice);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[12], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_RENDER_ALL, properties.type);
    EXPECT_FALSE(properties.onSubdevice);
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleAndIntegratedDeviceWhenCallingZesEngineGetActivityExtThenUnsupportedFeatureErrorIsReturned) {
    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesEngineGetActivityExt(handle, nullptr, &stats));
    }
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleAndIntegratedDeviceWhenCallingZesEngineGetActivityThenVerifyCallReturnsSuccess) {
    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivity(handle, &stats));
        EXPECT_EQ(mockActiveTime, stats.activeTime);
        EXPECT_EQ(mockTimestamp, stats.timestamp);
    }
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleAndDiscreteDeviceWhenCallingZesEngineGetActivityThenVerifyCallReturnsSuccess) {
    auto pMemoryManagerTest = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
    pMemoryManagerTest->localMemorySupported[0] = true;
    device->getDriverHandle()->setMemoryManager(pMemoryManagerTest.get());
    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivity(handle, &stats));
        EXPECT_EQ(mockActiveTime, stats.activeTime);
        EXPECT_EQ(mockTimestamp, stats.timestamp);
    }
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleAndDiscreteDeviceWhenCallingZesEngineGetActivityExtThenVerifyCallReturnsSuccess) {
    auto pMemoryManagerTest = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
    pMemoryManagerTest->localMemorySupported[0] = true;
    device->getDriverHandle()->setMemoryManager(pMemoryManagerTest.get());
    pSysfsAccess->mockReadVal = 2;
    pSysfsAccess->mockReadSymLinkSuccess = true;
    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(deviceHandles);
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivityExt(handle, &count, nullptr));
        EXPECT_EQ(count, pSysfsAccess->mockReadVal + 1);
        std::vector<zes_engine_stats_t> engineStats(count);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivityExt(handle, &count, engineStats.data()));
        for (auto &stat : engineStats) {
            EXPECT_EQ(mockActiveTime, stat.activeTime);
            EXPECT_EQ(mockTimestamp, stat.timestamp);
        }
    }
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleAndDiscreteDeviceWhenCallingZesEngineGetActivityExtMultipleTimesThenVerifyCallReturnsSuccess) {
    auto pMemoryManagerTest = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
    pMemoryManagerTest->localMemorySupported[0] = true;
    device->getDriverHandle()->setMemoryManager(pMemoryManagerTest.get());
    pSysfsAccess->mockReadVal = 2;
    pSysfsAccess->mockReadSymLinkSuccess = true;
    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(deviceHandles);
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivityExt(handle, &count, nullptr));
        EXPECT_EQ(count, pSysfsAccess->mockReadVal + 1);
        std::vector<zes_engine_stats_t> engineStats(count);
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivityExt(handle, &count, engineStats.data()));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivityExt(handle, &count, engineStats.data()));
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivityExt(handle, &count, engineStats.data()));
        for (auto &stat : engineStats) {
            EXPECT_EQ(mockActiveTime, stat.activeTime);
            EXPECT_EQ(mockTimestamp, stat.timestamp);
        }
    }
}

TEST_F(ZesEngineFixture, GivenReadingNumberOfVfsFailWhenInitializingEnginesThenGetActivityExtReturnsError) {
    auto pMemoryManagerTest = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
    pMemoryManagerTest->localMemorySupported[0] = true;
    device->getDriverHandle()->setMemoryManager(pMemoryManagerTest.get());
    pSysfsAccess->mockReadStatus = ZE_RESULT_ERROR_INVALID_ARGUMENT;
    pSysfsAccess->mockReadSymLinkSuccess = true;
    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(deviceHandles);
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesEngineGetActivityExt(handle, &count, nullptr));
    }
}

TEST_F(ZesEngineFixture, GivenDiscreteDeviceWithNoVfsWhenCallingZesEngineGetActivityExtThenReturnFailure) {
    auto pMemoryManagerTest = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
    pMemoryManagerTest->localMemorySupported[0] = true;
    device->getDriverHandle()->setMemoryManager(pMemoryManagerTest.get());
    pSysfsAccess->mockReadVal = 0;
    pSysfsAccess->mockReadSymLinkSuccess = true;
    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(deviceHandles);
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesEngineGetActivityExt(handle, &count, nullptr));
    }
}

TEST_F(ZesEngineFixture, GivenDiscreteDeviceWithValidVfsWhenPmuReadingFailsWhenCallingZesEngineGetActivityExtThenReturnFailure) {
    auto pMemoryManagerTest = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
    pMemoryManagerTest->localMemorySupported[0] = true;
    device->getDriverHandle()->setMemoryManager(pMemoryManagerTest.get());
    pSysfsAccess->mockReadVal = 2;
    pSysfsAccess->mockReadSymLinkSuccess = true;
    pPmuInterface->mockPmuRead = true;
    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(deviceHandles);
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        uint32_t count = 0;
        EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivityExt(handle, &count, nullptr));
        EXPECT_EQ(count, pSysfsAccess->mockReadVal + 1);
        std::vector<zes_engine_stats_t> engineStats(count);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesEngineGetActivityExt(handle, &count, engineStats.data()));
    }
}

TEST_F(ZesEngineFixture, GivenDiscreteDeviceWithTotalTicksInvalidVfWhenCallingZesEngineGetActivityExtThenReturnFailure) {
    auto pMemoryManagerTest = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
    pMemoryManagerTest->localMemorySupported[0] = true;
    device->getDriverHandle()->setMemoryManager(pMemoryManagerTest.get());
    pSysfsAccess->mockReadVal = 1;
    pSysfsAccess->mockReadSymLinkSuccess = true;
    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(deviceHandles);
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());
    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    uint32_t count = 0;
    pPmuInterface->mockPerfEventOpenRead = true;
    pPmuInterface->mockPerfEventOpenFailAtCount = 2;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivityExt(handle, &count, nullptr));
    EXPECT_EQ(count, pSysfsAccess->mockReadVal + 1);
    std::vector<zes_engine_stats_t> engineStats(count);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesEngineGetActivityExt(handle, &count, engineStats.data()));
}

TEST_F(ZesEngineFixture, GivenDiscreteDeviceWithBusyTicksInvalidVfWhenCallingZesEngineGetActivityExtThenReturnFailure) {
    auto pMemoryManagerTest = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
    pMemoryManagerTest->localMemorySupported[0] = true;
    device->getDriverHandle()->setMemoryManager(pMemoryManagerTest.get());
    pSysfsAccess->mockReadVal = 1;
    pSysfsAccess->mockReadSymLinkSuccess = true;
    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(deviceHandles);
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());
    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    uint32_t count = 0;
    pPmuInterface->mockPerfEventOpenRead = true;
    pPmuInterface->mockPerfEventOpenFailAtCount = 3;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivityExt(handle, &count, nullptr));
    EXPECT_EQ(count, pSysfsAccess->mockReadVal + 1);
    std::vector<zes_engine_stats_t> engineStats(count);
    EXPECT_EQ(ZE_RESULT_ERROR_NOT_AVAILABLE, zesEngineGetActivityExt(handle, &count, engineStats.data()));
}

TEST_F(ZesEngineFixture, GivenTooManyFilesErrorWhenCallingZesEngineGetActivityExtThenReturnFailure) {
    auto pMemoryManagerTest = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
    pMemoryManagerTest->localMemorySupported[0] = true;
    device->getDriverHandle()->setMemoryManager(pMemoryManagerTest.get());
    pSysfsAccess->mockReadVal = 1;
    pSysfsAccess->mockReadSymLinkSuccess = true;
    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(deviceHandles);
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());
    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    uint32_t count = 0;
    pPmuInterface->mockPerfEventOpenRead = true;
    pPmuInterface->mockPerfEventOpenFailAtCount = 3;
    pPmuInterface->mockErrorNumber = EMFILE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivityExt(handle, &count, nullptr));
    EXPECT_EQ(count, pSysfsAccess->mockReadVal + 1);
    std::vector<zes_engine_stats_t> engineStats(count);
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, zesEngineGetActivityExt(handle, &count, engineStats.data()));
}

TEST_F(ZesEngineFixture, GivenTooManyFilesInSystemErrorWhenCallingZesEngineGetActivityExtThenReturnFailure) {
    auto pMemoryManagerTest = std::make_unique<MockMemoryManagerInEngineSysman>(*neoDevice->getExecutionEnvironment());
    pMemoryManagerTest->localMemorySupported[0] = true;
    device->getDriverHandle()->setMemoryManager(pMemoryManagerTest.get());
    pSysfsAccess->mockReadVal = 1;
    pSysfsAccess->mockReadSymLinkSuccess = true;
    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(deviceHandles);
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());
    auto handle = handles[0];
    ASSERT_NE(nullptr, handle);
    uint32_t count = 0;
    pPmuInterface->mockPerfEventOpenRead = true;
    pPmuInterface->mockPerfEventOpenFailAtCount = 3;
    pPmuInterface->mockErrorNumber = ENFILE;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetActivityExt(handle, &count, nullptr));
    EXPECT_EQ(count, pSysfsAccess->mockReadVal + 1);
    std::vector<zes_engine_stats_t> engineStats(count);
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, zesEngineGetActivityExt(handle, &count, engineStats.data()));
}

TEST_F(ZesEngineFixture, GivenUnknownEngineTypeThengetEngineGroupFromTypeReturnsGroupAllEngineGroup) {
    auto group = LinuxEngineImp::getGroupFromEngineType(ZES_ENGINE_GROUP_3D_SINGLE);
    EXPECT_EQ(group, ZES_ENGINE_GROUP_ALL);
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleWhenCallingZesEngineGetActivityAndPmuReadFailsThenVerifyEngineGetActivityReturnsFailure) {

    pPmuInterface->mockPmuRead = true;

    zes_engine_stats_t stats = {};
    auto handles = getEngineHandles(handleComponentCount);
    EXPECT_EQ(handleComponentCount, handles.size());

    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
        EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, zesEngineGetActivity(handle, &stats));
    }
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleWhenCallingZesEngineGetActivityAndperfEventOpenFailsThenVerifyEngineGetActivityReturnsFailure) {

    pPmuInterface->mockPerfEventOpenRead = true;

    MockEnginePmuInterfaceImp pPmuInterfaceImp(pLinuxSysmanImp);
    EXPECT_EQ(-1, pPmuInterface->pmuInterfaceOpen(0, -1, 0));
}

TEST_F(ZesEngineFixture, GivenValidOsSysmanPointerWhenRetrievingEngineTypeAndInstancesAndIfEngineInfoQueryFailsThenErrorIsReturned) {
    std::set<std::pair<zes_engine_group_t, std::pair<uint32_t, uint32_t>>> engineGroupInstance;

    pDrm->mockReadSysmanQueryEngineInfo = true;

    EXPECT_EQ(ZE_RESULT_ERROR_UNSUPPORTED_FEATURE, OsEngine::getNumEngineTypeAndInstances(engineGroupInstance, pOsSysman));
}

TEST_F(ZesEngineFixture, GivenHandleQueryItemCalledWithInvalidEngineTypeThenzesDeviceEnumEngineGroupsSucceeds) {
    auto drm = std::make_unique<DrmMockEngine>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment()));
    pLinuxSysmanImp->pDrm = drm.get();
    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(deviceHandles);
    uint32_t count = 0;
    uint32_t mockHandleCount = 5u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, mockHandleCount);
}

TEST_F(ZesEngineFixture, GivenHandleQueryItemCalledWhenPmuInterfaceOpenFailsThenzesDeviceEnumEngineGroupsSucceedsAndHandleCountIsZero) {

    pFsAccess->mockReadVal = true;

    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(deviceHandles);
    uint32_t count = 0;
    uint32_t mockHandleCount = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, mockHandleCount);
}

TEST_F(ZesEngineFixture, GivenValidDrmObjectWhenCallingsysmanQueryEngineInfoMethodThenSuccessIsReturned) {
    auto drm = std::make_unique<DrmMockEngine>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment()));
    pLinuxSysmanImp->pDrm = drm.get();
    ASSERT_NE(nullptr, drm);
    EXPECT_TRUE(drm->sysmanQueryEngineInfo());
    auto engineInfo = drm->getEngineInfo();
    ASSERT_NE(nullptr, engineInfo);
}

TEST_F(ZesEngineFixture, GivenValidEngineHandleAndHandleCountZeroWhenCallingReInitThenValidCountIsReturnedAndVerifyzesDeviceEnumEngineGroupsSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, handleComponentCount);

    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();

    pLinuxSysmanImp->reInitSysmanDeviceResources();

    count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, handleComponentCount);
}

class ZesEngineMultiFixture : public SysmanMultiDeviceFixture {
  protected:
    std::vector<ze_device_handle_t> deviceHandles;
    std::unique_ptr<MockEnginePmuInterfaceImp> pPmuInterface;
    PmuInterface *pOriginalPmuInterface = nullptr;
    std::unique_ptr<MockEngineNeoDrm> pDrm;
    Drm *pOriginalDrm = nullptr;
    std::unique_ptr<MockEngineSysfsAccess> pSysfsAccess;
    SysfsAccess *pSysfsAccessOriginal = nullptr;
    std::unique_ptr<MockEngineFsAccess> pFsAccess;
    FsAccess *pFsAccessOriginal = nullptr;

    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanMultiDeviceFixture::SetUp();
        pSysfsAccessOriginal = pLinuxSysmanImp->pSysfsAccess;
        pSysfsAccess = std::make_unique<MockEngineSysfsAccess>();
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccess.get();

        pFsAccessOriginal = pLinuxSysmanImp->pFsAccess;
        pFsAccess = std::make_unique<MockEngineFsAccess>();
        pLinuxSysmanImp->pFsAccess = pFsAccess.get();

        pDrm = std::make_unique<MockEngineNeoDrm>(const_cast<NEO::RootDeviceEnvironment &>(neoDevice->getRootDeviceEnvironment()));
        pDrm->setupIoctlHelper(neoDevice->getRootDeviceEnvironment().getHardwareInfo()->platform.eProductFamily);
        pPmuInterface = std::make_unique<MockEnginePmuInterfaceImp>(pLinuxSysmanImp);
        pOriginalDrm = pLinuxSysmanImp->pDrm;
        pOriginalPmuInterface = pLinuxSysmanImp->pPmuInterface;
        pLinuxSysmanImp->pDrm = pDrm.get();
        pLinuxSysmanImp->pPmuInterface = pPmuInterface.get();

        pDrm->mockReadSysmanQueryEngineInfoMultiDevice = true;
        pSysfsAccess->mockReadSymLinkSuccess = true;

        pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
        uint32_t subDeviceCount = 0;
        // We received a device handle. Check for subdevices in this device
        Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, nullptr);
        if (subDeviceCount == 0) {
            deviceHandles.resize(1, device->toHandle());
        } else {
            deviceHandles.resize(subDeviceCount, nullptr);
            Device::fromHandle(device->toHandle())->getSubDevices(&subDeviceCount, deviceHandles.data());
        }
        getEngineHandles(0);
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        SysmanMultiDeviceFixture::TearDown();
        pLinuxSysmanImp->pDrm = pOriginalDrm;
        pLinuxSysmanImp->pPmuInterface = pOriginalPmuInterface;
        pLinuxSysmanImp->pSysfsAccess = pSysfsAccessOriginal;
        pLinuxSysmanImp->pFsAccess = pFsAccessOriginal;
    }

    std::vector<zes_engine_handle_t> getEngineHandles(uint32_t count) {
        std::vector<zes_engine_handle_t> handles(count, nullptr);
        EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
        return handles;
    }
};

TEST_F(ZesEngineMultiFixture, GivenComponentCountZeroWhenCallingzesDeviceEnumEngineGroupsThenNonZeroCountIsReturnedAndVerifyCallSucceeds) {
    uint32_t count = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, handleCountForMultiDeviceFixture);

    uint32_t testcount = count + 1;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &testcount, NULL));
    EXPECT_EQ(testcount, count);

    count = 0;
    std::vector<zes_engine_handle_t> handles(count, nullptr);
    EXPECT_EQ(zesDeviceEnumEngineGroups(device->toHandle(), &count, handles.data()), ZE_RESULT_SUCCESS);
    EXPECT_EQ(count, handleCountForMultiDeviceFixture);
}

TEST_F(ZesEngineMultiFixture, GivenValidEngineHandlesWhenCallingZesEngineGetPropertiesThenVerifyCallSucceeds) {
    zes_engine_properties_t properties;
    auto handles = getEngineHandles(handleCountForMultiDeviceFixture);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
    }
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[0], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_ALL, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[1], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_ALL, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 1u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[2], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ALL, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 1u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[3], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_RENDER_SINGLE, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[4], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 1u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[5], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 1u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[6], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_RENDER_ALL, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 0u);
}

TEST_F(ZesEngineMultiFixture, GivenHandleQueryItemCalledWhenPmuInterfaceOpenFailsThenzesDeviceEnumEngineGroupsSucceedsAndHandleCountIsZero) {

    pFsAccess->mockReadVal = true;

    pSysmanDeviceImp->pEngineHandleContext->handleList.clear();
    pSysmanDeviceImp->pEngineHandleContext->init(deviceHandles);
    uint32_t count = 0;
    uint32_t mockHandleCount = 0u;
    EXPECT_EQ(ZE_RESULT_SUCCESS, zesDeviceEnumEngineGroups(device->toHandle(), &count, NULL));
    EXPECT_EQ(count, mockHandleCount);
}

class ZesEngineAffinityMaskFixture : public ZesEngineMultiFixture {
    void SetUp() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        NEO::debugManager.flags.ZE_AFFINITY_MASK.set("0.1");
        ZesEngineMultiFixture::SetUp();
    }

    void TearDown() override {
        if (!sysmanUltsEnable) {
            GTEST_SKIP();
        }
        ZesEngineMultiFixture::TearDown();
    }
    DebugManagerStateRestore restorer;
};

TEST_F(ZesEngineAffinityMaskFixture, GivenValidEngineHandlesWhenCallingZesEngineGetPropertiesWhenAffinityMaskIsSetThenVerifyCallSucceeds) {
    zes_engine_properties_t properties;
    uint32_t handleCountForEngineAffinityMaskFixture = 4u;
    auto handles = getEngineHandles(handleCountForEngineAffinityMaskFixture);
    for (auto handle : handles) {
        ASSERT_NE(nullptr, handle);
    }

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[0], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_ALL, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 1u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[1], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ALL, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 1u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[2], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 1u);

    EXPECT_EQ(ZE_RESULT_SUCCESS, zesEngineGetProperties(handles[3], &properties));
    EXPECT_EQ(ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE, properties.type);
    EXPECT_TRUE(properties.onSubdevice);
    EXPECT_EQ(properties.subdeviceId, 1u);
}

} // namespace ult
} // namespace L0
