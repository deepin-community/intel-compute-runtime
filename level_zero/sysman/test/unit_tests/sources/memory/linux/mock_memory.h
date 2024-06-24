/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/system_info.h"

#include "level_zero/sysman/source/api/memory/linux/sysman_os_memory_imp.h"
#include "level_zero/sysman/source/api/memory/sysman_memory_imp.h"
#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_hw_device_id.h"

using namespace NEO;

constexpr uint32_t vF0HbmLRead = 16;
constexpr uint32_t vF0HbmHRead = 2;
constexpr uint32_t vF0HbmLWrite = 8;
constexpr uint32_t vF0HbmHWrite = 2;
constexpr uint32_t vF1HbmLRead = 16;
constexpr uint32_t vF1HbmHRead = 2;
constexpr uint32_t vF1HbmLWrite = 8;
constexpr uint32_t vF1HbmHWrite = 2;

constexpr uint8_t vF0Hbm0ReadValue = 92;
constexpr uint8_t vF0Hbm0WriteValue = 96;
constexpr uint8_t vF0Hbm1ReadValue = 104;
constexpr uint8_t vF0Hbm1WriteValue = 108;
constexpr uint8_t vF0TimestampLValue = 168;
constexpr uint8_t vF0TimestampHValue = 172;
constexpr uint8_t vF0Hbm2ReadValue = 113;
constexpr uint8_t vF0Hbm2WriteValue = 125;
constexpr uint8_t vF0Hbm3ReadValue = 135;
constexpr uint8_t vF0Hbm3WriteValue = 20;

constexpr uint8_t vF1Hbm0ReadValue = 92;
constexpr uint8_t vF1Hbm0WriteValue = 96;
constexpr uint8_t vF1Hbm1ReadValue = 104;
constexpr uint8_t vF1Hbm1WriteValue = 108;
constexpr uint8_t vF1TimestampLValue = 168;
constexpr uint8_t vF1TimestampHValue = 172;
constexpr uint8_t vF1Hbm2ReadValue = 113;
constexpr uint8_t vF1Hbm2WriteValue = 125;
constexpr uint8_t vF1Hbm3ReadValue = 135;
constexpr uint8_t vF1Hbm3WriteValue = 20;
constexpr uint64_t mockIdiReadVal = 8u;
constexpr uint64_t mockIdiWriteVal = 9u;
constexpr uint64_t mockDisplayVc1ReadVal = 10u;
constexpr uint64_t numberMcChannels = 16;
constexpr uint64_t transactionSize = 32;

namespace L0 {
namespace Sysman {
namespace ult {

uint32_t mockMemoryType = NEO::DeviceBlobConstants::MemoryType::hbm2e;
struct MockMemoryNeoDrm : public NEO::Drm {
    using Drm::ioctlHelper;
    const int mockFd = 33;
    std::vector<bool> mockQuerySystemInfoReturnValue{};
    bool isRepeated = false;
    bool mockReturnEmptyRegions = false;
    MockMemoryNeoDrm(RootDeviceEnvironment &rootDeviceEnvironment) : Drm(std::make_unique<MockSysmanHwDeviceIdDrm>(mockFd, ""), rootDeviceEnvironment) {}

    void setMemoryType(uint32_t memory) {
        mockMemoryType = memory;
    }

    std::vector<uint8_t> getMemoryRegionsReturnsEmpty() {
        return {};
    }

    void resetSystemInfo() {
        systemInfo.reset(nullptr);
    }

    bool querySystemInfo() override {
        bool returnValue = true;
        if (!mockQuerySystemInfoReturnValue.empty()) {
            returnValue = mockQuerySystemInfoReturnValue.front();
            if (isRepeated != true) {
                mockQuerySystemInfoReturnValue.erase(mockQuerySystemInfoReturnValue.begin());
            }
            resetSystemInfo();
            return returnValue;
        }

        uint32_t hwBlob[] = {NEO::DeviceBlobConstants::maxMemoryChannels, 1, 8, NEO::DeviceBlobConstants::memoryType, 0, mockMemoryType};
        std::vector<uint32_t> inputBlobData(reinterpret_cast<uint32_t *>(hwBlob), reinterpret_cast<uint32_t *>(ptrOffset(hwBlob, sizeof(hwBlob))));
        this->systemInfo.reset(new SystemInfo(inputBlobData));
        return returnValue;
    }
};
struct MockMemoryPmt : public L0::Sysman::PlatformMonitoringTech {
    using L0::Sysman::PlatformMonitoringTech::guid;
    using L0::Sysman::PlatformMonitoringTech::keyOffsetMap;
    std::vector<ze_result_t> mockReadValueReturnStatus{};
    std::vector<uint32_t> mockReadArgumentValue{};
    ze_result_t mockIdiReadValueFailureReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t mockIdiWriteFailureReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t mockDisplayVc1ReadFailureReturnStatus = ZE_RESULT_SUCCESS;
    ze_result_t mockReadTimeStampFailureReturnStatus = ZE_RESULT_SUCCESS;
    bool mockVfid0Status = false;
    bool mockVfid1Status = false;
    bool isRepeated = false;

    void setGuid(std::string guid) {
        this->guid = guid;
    }

    MockMemoryPmt() = default;
    ze_result_t readValue(const std::string key, uint32_t &val) override {
        ze_result_t result = ZE_RESULT_SUCCESS;

        if (mockVfid0Status == true) {
            return mockedReadValueWithVfid0True(key, val);
        }

        if (mockVfid1Status == true) {
            return mockedReadValueWithVfid1True(key, val);
        }

        if (!mockReadValueReturnStatus.empty()) {
            result = mockReadValueReturnStatus.front();
            if (!mockReadArgumentValue.empty()) {
                val = mockReadArgumentValue.front();
            }

            if (isRepeated != true) {
                mockReadValueReturnStatus.erase(mockReadValueReturnStatus.begin());
                if (!mockReadArgumentValue.empty()) {
                    mockReadArgumentValue.erase(mockReadArgumentValue.begin());
                }
            }
        }
        return result;
    }

    ze_result_t mockedReadValueWithVfid0True(const std::string key, uint32_t &val) {
        if (key.compare("VF0_VFID") == 0) {
            val = 1;
        } else if (key.compare("VF1_VFID") == 0) {
            val = 0;
        } else if (key.compare("VF0_HBM0_READ") == 0) {
            val = vF0Hbm0ReadValue;
        } else if (key.compare("VF0_HBM0_WRITE") == 0) {
            val = vF0Hbm0WriteValue;
        } else if (key.compare("VF0_HBM1_READ") == 0) {
            val = vF0Hbm1ReadValue;
        } else if (key.compare("VF0_HBM1_WRITE") == 0) {
            val = vF0Hbm1WriteValue;
        } else if (key.compare("VF0_TIMESTAMP_L") == 0) {
            val = vF0TimestampLValue;
        } else if (key.compare("VF0_TIMESTAMP_H") == 0) {
            val = vF0TimestampHValue;
        } else if (key.compare("VF0_HBM2_READ") == 0) {
            val = vF0Hbm2ReadValue;
        } else if (key.compare("VF0_HBM2_WRITE") == 0) {
            val = vF0Hbm2WriteValue;
        } else if (key.compare("VF0_HBM3_READ") == 0) {
            val = vF0Hbm3ReadValue;
        } else if (key.compare("VF0_HBM3_WRITE") == 0) {
            val = vF0Hbm3WriteValue;
        } else if (key.compare("VF0_HBM_READ_L") == 0) {
            val = vF0HbmLRead;
        } else if (key.compare("VF0_HBM_READ_H") == 0) {
            val = vF0HbmHRead;
        } else if (key.compare("VF0_HBM_WRITE_L") == 0) {
            val = vF0HbmLWrite;
        } else if (key.compare("VF0_HBM_WRITE_H") == 0) {
            val = vF0HbmHWrite;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t mockedReadValueWithVfid1True(const std::string key, uint32_t &val) {
        if (key.compare("VF0_VFID") == 0) {
            val = 0;
        } else if (key.compare("VF1_VFID") == 0) {
            val = 1;
        } else if (key.compare("VF1_HBM0_READ") == 0) {
            val = vF1Hbm0ReadValue;
        } else if (key.compare("VF1_HBM0_WRITE") == 0) {
            val = vF1Hbm0WriteValue;
        } else if (key.compare("VF1_HBM1_READ") == 0) {
            val = vF1Hbm1ReadValue;
        } else if (key.compare("VF1_HBM1_WRITE") == 0) {
            val = vF1Hbm1WriteValue;
        } else if (key.compare("VF1_TIMESTAMP_L") == 0) {
            val = vF1TimestampLValue;
        } else if (key.compare("VF1_TIMESTAMP_H") == 0) {
            val = vF1TimestampHValue;
        } else if (key.compare("VF1_HBM2_READ") == 0) {
            val = vF1Hbm2ReadValue;
        } else if (key.compare("VF1_HBM2_WRITE") == 0) {
            val = vF1Hbm2WriteValue;
        } else if (key.compare("VF1_HBM3_READ") == 0) {
            val = vF1Hbm3ReadValue;
        } else if (key.compare("VF1_HBM3_WRITE") == 0) {
            val = vF1Hbm3WriteValue;
        } else if (key.compare("VF1_HBM_READ_L") == 0) {
            val = vF1HbmLRead;
        } else if (key.compare("VF1_HBM_READ_H") == 0) {
            val = vF1HbmHRead;
        } else if (key.compare("VF1_HBM_WRITE_L") == 0) {
            val = vF1HbmLWrite;
        } else if (key.compare("VF1_HBM_WRITE_H") == 0) {
            val = vF1HbmHWrite;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t readValue(const std::string key, uint64_t &val) override {
        ze_result_t result = ZE_RESULT_SUCCESS;

        if (mockIdiReadValueFailureReturnStatus != ZE_RESULT_SUCCESS) {
            return mockIdiReadValueFailure(key, val);
        }

        if (mockIdiWriteFailureReturnStatus != ZE_RESULT_SUCCESS) {
            return mockIdiWriteFailure(key, val);
        }

        if (mockDisplayVc1ReadFailureReturnStatus != ZE_RESULT_SUCCESS) {
            return mockDisplayVc1ReadFailure(key, val);
        }

        if (mockReadTimeStampFailureReturnStatus != ZE_RESULT_SUCCESS) {
            return mockReadTimeStampFailure(key, val);
        }

        if (key.compare("IDI_READS[0]") == 0 || key.compare("IDI_READS[1]") == 0 || key.compare("IDI_READS[2]") == 0 || key.compare("IDI_READS[3]") == 0 || key.compare("IDI_READS[4]") == 0 || key.compare("IDI_READS[5]") == 0 || key.compare("IDI_READS[6]") == 0 || key.compare("IDI_READS[7]") == 0 || key.compare("IDI_READS[8]") == 0 || key.compare("IDI_READS[9]") == 0 || key.compare("IDI_READS[10]") == 0 || key.compare("IDI_READS[11]") == 0 || key.compare("IDI_READS[12]") == 0 || key.compare("IDI_READS[13]") == 0 || key.compare("IDI_READS[14]") == 0 || key.compare("IDI_READS[15]") == 0) {
            val = mockIdiReadVal;
        } else if (key.compare("IDI_WRITES[0]") == 0 || key.compare("IDI_WRITES[1]") == 0 || key.compare("IDI_WRITES[2]") == 0 || key.compare("IDI_WRITES[3]") == 0 || key.compare("IDI_WRITES[4]") == 0 || key.compare("IDI_WRITES[5]") == 0 || key.compare("IDI_WRITES[6]") == 0 || key.compare("IDI_WRITES[7]") == 0 || key.compare("IDI_WRITES[8]") == 0 || key.compare("IDI_WRITES[9]") == 0 || key.compare("IDI_WRITES[10]") == 0 || key.compare("IDI_WRITES[11]") == 0 || key.compare("IDI_WRITES[12]") == 0 || key.compare("IDI_WRITES[13]") == 0 || key.compare("IDI_WRITES[14]") == 0 || key.compare("IDI_WRITES[15]") == 0) {
            val = mockIdiWriteVal;
        } else if (key.compare("DISPLAY_VC1_READS[0]") == 0 || key.compare("DISPLAY_VC1_READS[1]") == 0 || key.compare("DISPLAY_VC1_READS[2]") == 0 || key.compare("DISPLAY_VC1_READS[3]") == 0 || key.compare("DISPLAY_VC1_READS[4]") == 0 || key.compare("DISPLAY_VC1_READS[5]") == 0 || key.compare("DISPLAY_VC1_READS[6]") == 0 || key.compare("DISPLAY_VC1_READS[7]") == 0 || key.compare("DISPLAY_VC1_READS[8]") == 0 || key.compare("DISPLAY_VC1_READS[9]") == 0 || key.compare("DISPLAY_VC1_READS[10]") == 0 || key.compare("DISPLAY_VC1_READS[11]") == 0 || key.compare("DISPLAY_VC1_READS[12]") == 0 || key.compare("DISPLAY_VC1_READS[13]") == 0 || key.compare("DISPLAY_VC1_READS[14]") == 0 || key.compare("DISPLAY_VC1_READS[15]") == 0) {
            val = mockDisplayVc1ReadVal;
        } else {
            result = ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return result;
    }

    ze_result_t mockIdiReadValueFailure(const std::string key, uint64_t &val) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    ze_result_t mockIdiWriteFailure(const std::string key, uint64_t &val) {
        if (key.compare("IDI_READS[0]") == 0 || key.compare("IDI_READS[1]") == 0 || key.compare("IDI_READS[2]") == 0 || key.compare("IDI_READS[3]") == 0 || key.compare("IDI_READS[4]") == 0 || key.compare("IDI_READS[5]") == 0 || key.compare("IDI_READS[6]") == 0 || key.compare("IDI_READS[7]") == 0 || key.compare("IDI_READS[8]") == 0 || key.compare("IDI_READS[9]") == 0 || key.compare("IDI_READS[10]") == 0 || key.compare("IDI_READS[11]") == 0 || key.compare("IDI_READS[12]") == 0 || key.compare("IDI_READS[13]") == 0 || key.compare("IDI_READS[14]") == 0 || key.compare("IDI_READS[15]") == 0) {
            val = mockIdiReadVal;
        } else if (key.compare("IDI_WRITES[0]") == 0 || key.compare("IDI_WRITES[1]") == 0 || key.compare("IDI_WRITES[2]") == 0 || key.compare("IDI_WRITES[3]") == 0 || key.compare("IDI_WRITES[4]") == 0 || key.compare("IDI_WRITES[5]") == 0 || key.compare("IDI_WRITES[6]") == 0 || key.compare("IDI_WRITES[7]") == 0 || key.compare("IDI_WRITES[8]") == 0 || key.compare("IDI_WRITES[9]") == 0 || key.compare("IDI_WRITES[10]") == 0 || key.compare("IDI_WRITES[11]") == 0 || key.compare("IDI_WRITES[12]") == 0 || key.compare("IDI_WRITES[13]") == 0 || key.compare("IDI_WRITES[14]") == 0 || key.compare("IDI_WRITES[15]") == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t mockDisplayVc1ReadFailure(const std::string key, uint64_t &val) {
        if (key.compare("IDI_READS[0]") == 0 || key.compare("IDI_READS[1]") == 0 || key.compare("IDI_READS[2]") == 0 || key.compare("IDI_READS[3]") == 0 || key.compare("IDI_READS[4]") == 0 || key.compare("IDI_READS[5]") == 0 || key.compare("IDI_READS[6]") == 0 || key.compare("IDI_READS[7]") == 0 || key.compare("IDI_READS[8]") == 0 || key.compare("IDI_READS[9]") == 0 || key.compare("IDI_READS[10]") == 0 || key.compare("IDI_READS[11]") == 0 || key.compare("IDI_READS[12]") == 0 || key.compare("IDI_READS[13]") == 0 || key.compare("IDI_READS[14]") == 0 || key.compare("IDI_READS[15]") == 0) {
            val = mockIdiReadVal;
        } else if (key.compare("IDI_WRITES[0]") == 0 || key.compare("IDI_WRITES[1]") == 0 || key.compare("IDI_WRITES[2]") == 0 || key.compare("IDI_WRITES[3]") == 0 || key.compare("IDI_WRITES[4]") == 0 || key.compare("IDI_WRITES[5]") == 0 || key.compare("IDI_WRITES[6]") == 0 || key.compare("IDI_WRITES[7]") == 0 || key.compare("IDI_WRITES[8]") == 0 || key.compare("IDI_WRITES[9]") == 0 || key.compare("IDI_WRITES[10]") == 0 || key.compare("IDI_WRITES[11]") == 0 || key.compare("IDI_WRITES[12]") == 0 || key.compare("IDI_WRITES[13]") == 0 || key.compare("IDI_WRITES[14]") == 0 || key.compare("IDI_WRITES[15]") == 0) {
            val = mockIdiWriteVal;
        } else if (key.compare("DISPLAY_VC1_READS[0]") == 0 || key.compare("DISPLAY_VC1_READS[1]") == 0 || key.compare("DISPLAY_VC1_READS[2]") == 0 || key.compare("DISPLAY_VC1_READS[3]") == 0 || key.compare("DISPLAY_VC1_READS[4]") == 0 || key.compare("DISPLAY_VC1_READS[5]") == 0 || key.compare("DISPLAY_VC1_READS[6]") == 0 || key.compare("DISPLAY_VC1_READS[7]") == 0 || key.compare("DISPLAY_VC1_READS[8]") == 0 || key.compare("DISPLAY_VC1_READS[9]") == 0 || key.compare("DISPLAY_VC1_READS[10]") == 0 || key.compare("DISPLAY_VC1_READS[11]") == 0 || key.compare("DISPLAY_VC1_READS[12]") == 0 || key.compare("DISPLAY_VC1_READS[13]") == 0 || key.compare("DISPLAY_VC1_READS[14]") == 0 || key.compare("DISPLAY_VC1_READS[15]") == 0) {
            return ZE_RESULT_ERROR_UNKNOWN;
        }
        return ZE_RESULT_SUCCESS;
    }

    ze_result_t mockReadTimeStampFailure(const std::string key, uint64_t &val) {
        if (key.compare("IDI_READS[0]") == 0 || key.compare("IDI_READS[1]") == 0 || key.compare("IDI_READS[2]") == 0 || key.compare("IDI_READS[3]") == 0 || key.compare("IDI_READS[4]") == 0 || key.compare("IDI_READS[5]") == 0 || key.compare("IDI_READS[6]") == 0 || key.compare("IDI_READS[7]") == 0 || key.compare("IDI_READS[8]") == 0 || key.compare("IDI_READS[9]") == 0 || key.compare("IDI_READS[10]") == 0 || key.compare("IDI_READS[11]") == 0 || key.compare("IDI_READS[12]") == 0 || key.compare("IDI_READS[13]") == 0 || key.compare("IDI_READS[14]") == 0 || key.compare("IDI_READS[15]") == 0) {
            val = mockIdiReadVal;
        } else if (key.compare("IDI_WRITES[0]") == 0 || key.compare("IDI_WRITES[1]") == 0 || key.compare("IDI_WRITES[2]") == 0 || key.compare("IDI_WRITES[3]") == 0 || key.compare("IDI_WRITES[4]") == 0 || key.compare("IDI_WRITES[5]") == 0 || key.compare("IDI_WRITES[6]") == 0 || key.compare("IDI_WRITES[7]") == 0 || key.compare("IDI_WRITES[8]") == 0 || key.compare("IDI_WRITES[9]") == 0 || key.compare("IDI_WRITES[10]") == 0 || key.compare("IDI_WRITES[11]") == 0 || key.compare("IDI_WRITES[12]") == 0 || key.compare("IDI_WRITES[13]") == 0 || key.compare("IDI_WRITES[14]") == 0 || key.compare("IDI_WRITES[15]") == 0) {
            val = mockIdiWriteVal;
        } else if (key.compare("DISPLAY_VC1_READS[0]") == 0 || key.compare("DISPLAY_VC1_READS[1]") == 0 || key.compare("DISPLAY_VC1_READS[2]") == 0 || key.compare("DISPLAY_VC1_READS[3]") == 0 || key.compare("DISPLAY_VC1_READS[4]") == 0 || key.compare("DISPLAY_VC1_READS[5]") == 0 || key.compare("DISPLAY_VC1_READS[6]") == 0 || key.compare("DISPLAY_VC1_READS[7]") == 0 || key.compare("DISPLAY_VC1_READS[8]") == 0 || key.compare("DISPLAY_VC1_READS[9]") == 0 || key.compare("DISPLAY_VC1_READS[10]") == 0 || key.compare("DISPLAY_VC1_READS[11]") == 0 || key.compare("DISPLAY_VC1_READS[12]") == 0 || key.compare("DISPLAY_VC1_READS[13]") == 0 || key.compare("DISPLAY_VC1_READS[14]") == 0 || key.compare("DISPLAY_VC1_READS[15]") == 0) {
            val = mockDisplayVc1ReadVal;
        } else {
            return ZE_RESULT_ERROR_NOT_AVAILABLE;
        }
        return ZE_RESULT_SUCCESS;
    }
};

class PublicLinuxMemoryImp : public L0::Sysman::LinuxMemoryImp {
  public:
    PublicLinuxMemoryImp(L0::Sysman::OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId) : LinuxMemoryImp(pOsSysman, onSubdevice, subdeviceId) {}
    PublicLinuxMemoryImp() = default;
    using L0::Sysman::LinuxMemoryImp::pSysmanKmdInterface;
};

class MockSysmanKmdInterfaceXe : public L0::Sysman::SysmanKmdInterfaceXe {
  public:
    using L0::Sysman::SysmanKmdInterface::pProcfsAccess;
    using L0::Sysman::SysmanKmdInterface::pSysfsAccess;
    MockSysmanKmdInterfaceXe(const PRODUCT_FAMILY productFamily) : SysmanKmdInterfaceXe(productFamily) {}
    ~MockSysmanKmdInterfaceXe() override = default;
};

class MockSysFsAccessInterface : public L0::Sysman::SysFsAccessInterface {
  public:
    MockSysFsAccessInterface() = default;
    ~MockSysFsAccessInterface() override = default;
};

class MockProcFsAccessInterface : public L0::Sysman::ProcFsAccessInterface {
  public:
    MockProcFsAccessInterface() = default;
    ~MockProcFsAccessInterface() override = default;
};

} // namespace ult
} // namespace Sysman
} // namespace L0
