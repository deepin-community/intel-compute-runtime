/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/system_info.h"

#include "level_zero/sysman/source/api/ras/linux/ras_util/sysman_ras_util.h"
#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"
#include "level_zero/sysman/source/shared/linux/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper_hw.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"
#include "level_zero/sysman/source/sysman_const.h"

#include <algorithm>

namespace L0 {
namespace Sysman {

template <PRODUCT_FAMILY gfxProduct>
void SysmanProductHelperHw<gfxProduct>::getFrequencyStepSize(double *pStepSize) {
    *pStepSize = (50.0 / 3); // Step of 16.6666667 Mhz
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryProperties(zes_mem_properties_t *pProperties, LinuxSysmanImp *pLinuxSysmanImp, NEO::Drm *pDrm, SysmanKmdInterface *pSysmanKmdInterface, uint32_t subDeviceId, bool isSubdevice) {
    auto pSysFsAccess = pSysmanKmdInterface->getSysFsAccess();

    pProperties->location = ZES_MEM_LOC_DEVICE;
    pProperties->type = ZES_MEM_TYPE_DDR;
    pProperties->onSubdevice = isSubdevice;
    pProperties->subdeviceId = subDeviceId;
    pProperties->busWidth = -1;
    pProperties->numChannels = -1;
    pProperties->physicalSize = 0;

    auto hwDeviceId = pLinuxSysmanImp->getSysmanHwDeviceIdInstance();
    auto status = pDrm->querySystemInfo();

    if (status) {
        auto memSystemInfo = pDrm->getSystemInfo();
        if (memSystemInfo != nullptr) {
            pProperties->numChannels = memSystemInfo->getMaxMemoryChannels();
            auto memType = memSystemInfo->getMemoryType();
            switch (memType) {
            case NEO::DeviceBlobConstants::MemoryType::hbm2e:
            case NEO::DeviceBlobConstants::MemoryType::hbm2:
                pProperties->type = ZES_MEM_TYPE_HBM;
                break;
            case NEO::DeviceBlobConstants::MemoryType::lpddr4:
                pProperties->type = ZES_MEM_TYPE_LPDDR4;
                break;
            case NEO::DeviceBlobConstants::MemoryType::lpddr5:
                pProperties->type = ZES_MEM_TYPE_LPDDR5;
                break;
            default:
                pProperties->type = ZES_MEM_TYPE_DDR;
                break;
            }
        }
    }

    pProperties->busWidth = memoryBusWidth;
    pProperties->physicalSize = 0;

    if (pSysmanKmdInterface->isPhysicalMemorySizeSupported() == true) {
        if (isSubdevice) {
            std::string memval;
            std::string physicalSizeFile = pSysmanKmdInterface->getSysfsFilePathForPhysicalMemorySize(subDeviceId);
            ze_result_t result = pSysFsAccess->read(physicalSizeFile, memval);
            uint64_t intval = strtoull(memval.c_str(), nullptr, 16);
            if (ZE_RESULT_SUCCESS != result) {
                pProperties->physicalSize = 0u;
            } else {
                pProperties->physicalSize = intval;
            }
        }
    }

    return ZE_RESULT_SUCCESS;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryBandwidth(zes_mem_bandwidth_t *pBandwidth, PlatformMonitoringTech *pPmt, SysmanDeviceImp *pDevice, SysmanKmdInterface *pSysmanKmdInterface, uint32_t subdeviceId) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <PRODUCT_FAMILY gfxProduct>
void SysmanProductHelperHw<gfxProduct>::getMemoryHealthIndicator(FirmwareUtil *pFwInterface, zes_mem_health_t *health) {
    *health = ZES_MEM_HEALTH_UNKNOWN;
}

template <PRODUCT_FAMILY gfxProduct>
void SysmanProductHelperHw<gfxProduct>::getMediaPerformanceFactorMultiplier(const double performanceFactor, double *pMultiplier) {
    if (performanceFactor > halfOfMaxPerformanceFactor) {
        *pMultiplier = 1;
    } else if (performanceFactor > minPerformanceFactor) {
        *pMultiplier = 0.5;
    } else {
        *pMultiplier = 0;
    }
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isPerfFactorSupported() {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isMemoryMaxTemperatureSupported() {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isFrequencySetRangeSupported() {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getGlobalMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) {
    auto isValidTemperature = [](auto temperature) {
        if ((temperature > invalidMaxTemperature) || (temperature < invalidMinTemperature)) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): temperature:%f is not in valid limits \n", __FUNCTION__, temperature);
            return false;
        }
        return true;
    };

    auto getMaxTemperature = [&](auto temperature, auto numTemperatureEntries) {
        uint32_t maxTemperature = 0;
        for (uint32_t count = 0; count < numTemperatureEntries; count++) {
            uint32_t localTemperatureVal = (temperature >> (8 * count)) & 0xff;
            if (isValidTemperature(localTemperatureVal)) {
                if (localTemperatureVal > maxTemperature) {
                    maxTemperature = localTemperatureVal;
                }
            }
        }
        return maxTemperature;
    };

    ze_result_t result = ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    std::string key;

    // SOC_TEMPERATURES is present in all product families
    uint64_t socTemperature = 0;
    key = "SOC_TEMPERATURES";
    result = pPmt->readValue(key, socTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Pmt->readvalue() for SOC_TEMPERATURES is returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    // Check max temperature among possible sensors like PCH or GT_TEMP, DRAM, SA, PSF, DE, PCIE, TYPEC across SOC_TEMPERATURES
    uint32_t maxSocTemperature = getMaxTemperature(socTemperature, numSocTemperatureEntries);

    *pTemperature = static_cast<double>(maxSocTemperature);

    return result;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getGpuMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) {
    double gpuMaxTemperature = 0;
    uint64_t socTemperature = 0;
    // Gpu temperature is obtained from GT_TEMP in SOC_TEMPERATURE's bit 0 to 7.
    std::string key = "SOC_TEMPERATURES";
    auto result = pPmt->readValue(key, socTemperature);
    if (result != ZE_RESULT_SUCCESS) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Pmt->readvalue() for SOC_TEMPERATURES is returning error:0x%x \n", __FUNCTION__, result);
        return result;
    }
    gpuMaxTemperature = static_cast<double>(socTemperature & 0xff);

    *pTemperature = gpuMaxTemperature;
    return ZE_RESULT_SUCCESS;
}

template <PRODUCT_FAMILY gfxProduct>
ze_result_t SysmanProductHelperHw<gfxProduct>::getMemoryMaxTemperature(PlatformMonitoringTech *pPmt, double *pTemperature) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

template <PRODUCT_FAMILY gfxProduct>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getGtRasUtilInterface() {
    return RasInterfaceType::none;
}

template <PRODUCT_FAMILY gfxProduct>
RasInterfaceType SysmanProductHelperHw<gfxProduct>::getHbmRasUtilInterface() {
    return RasInterfaceType::none;
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isRepairStatusSupported() {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
void SysmanProductHelperHw<gfxProduct>::getCurrentVoltage(PlatformMonitoringTech *pPmt, double &voltage) {
    voltage = -1.0;
}

template <PRODUCT_FAMILY gfxProduct>
int32_t SysmanProductHelperHw<gfxProduct>::getPowerLimitValue(uint64_t value) {
    uint64_t val = value / milliFactor;
    return static_cast<int32_t>(val);
}

template <PRODUCT_FAMILY gfxProduct>
uint64_t SysmanProductHelperHw<gfxProduct>::setPowerLimitValue(int32_t value) {
    uint64_t val = static_cast<uint64_t>(value) * milliFactor;
    return val;
}

template <PRODUCT_FAMILY gfxProduct>
zes_limit_unit_t SysmanProductHelperHw<gfxProduct>::getPowerLimitUnit() {
    return ZES_LIMIT_UNIT_POWER;
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isPowerSetLimitSupported() {
    return true;
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isDiagnosticsSupported() {
    return false;
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isStandbySupported(SysmanKmdInterface *pSysmanKmdInterface) {
    return pSysmanKmdInterface->isStandbyModeControlAvailable();
}

template <PRODUCT_FAMILY gfxProduct>
void SysmanProductHelperHw<gfxProduct>::getDeviceSupportedFwTypes(FirmwareUtil *pFwInterface, std::vector<std::string> &fwTypes) {
    fwTypes.clear();
    pFwInterface->getDeviceSupportedFwTypes(fwTypes);
}

template <PRODUCT_FAMILY gfxProduct>
bool SysmanProductHelperHw<gfxProduct>::isEccConfigurationSupported() {
    return false;
}

} // namespace Sysman
} // namespace L0
