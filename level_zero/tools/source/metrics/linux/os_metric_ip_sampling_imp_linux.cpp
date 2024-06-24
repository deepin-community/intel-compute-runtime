/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/metrics/metric.h"
#include "level_zero/tools/source/metrics/os_interface_metric.h"

#include <algorithm>

namespace L0 {

constexpr uint32_t maxDssBufferSize = 512 * MemoryConstants::kiloByte;
constexpr uint32_t defaultPollPeriodNs = 10000000u;
constexpr uint32_t unitReportSize = 64u;

class MetricIpSamplingLinuxImp : public MetricIpSamplingOsInterface {
  public:
    MetricIpSamplingLinuxImp(Device &device);
    ~MetricIpSamplingLinuxImp() override = default;
    ze_result_t startMeasurement(uint32_t &notifyEveryNReports, uint32_t &samplingPeriodNs) override;
    ze_result_t stopMeasurement() override;
    ze_result_t readData(uint8_t *pRawData, size_t *pRawDataSize) override;
    uint32_t getRequiredBufferSize(const uint32_t maxReportCount) override;
    uint32_t getUnitReportSize() override;
    bool isNReportsAvailable() override;
    bool isDependencyAvailable() override;
    ze_result_t getMetricsTimerResolution(uint64_t &timerResolution) override;

  private:
    int32_t stream = -1;
    Device &device;

    ze_result_t getNearestSupportedSamplingUnit(uint32_t &samplingPeriodNs, uint32_t &samplingRate);
};

MetricIpSamplingLinuxImp::MetricIpSamplingLinuxImp(Device &device) : device(device) {}

ze_result_t MetricIpSamplingLinuxImp::getNearestSupportedSamplingUnit(uint32_t &samplingPeriodNs, uint32_t &samplingUnit) {

    static constexpr uint32_t samplingClockGranularity = 251u;
    static constexpr uint32_t minSamplingUnit = 1u;
    static constexpr uint32_t maxSamplingUnit = 7u;

    uint64_t gpuTimeStampfrequency = 0;
    ze_result_t ret = getMetricsTimerResolution(gpuTimeStampfrequency);
    if (ret != ZE_RESULT_SUCCESS) {
        return ret;
    }

    uint64_t gpuClockPeriodNs = nsecPerSec / gpuTimeStampfrequency;
    uint64_t numberOfClocks = samplingPeriodNs / gpuClockPeriodNs;

    samplingUnit = std::clamp(static_cast<uint32_t>(numberOfClocks / samplingClockGranularity), minSamplingUnit, maxSamplingUnit);
    samplingPeriodNs = samplingUnit * samplingClockGranularity * static_cast<uint32_t>(gpuClockPeriodNs);
    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricIpSamplingLinuxImp::startMeasurement(uint32_t &notifyEveryNReports, uint32_t &samplingPeriodNs) {

    const auto drm = device.getOsInterface().getDriverModel()->as<NEO::Drm>();

    uint32_t samplingUnit = 0;
    if (getNearestSupportedSamplingUnit(samplingPeriodNs, samplingUnit) != ZE_RESULT_SUCCESS) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    DeviceImp &deviceImp = static_cast<DeviceImp &>(device);

    auto ioctlHelper = drm->getIoctlHelper();
    uint32_t euStallFdParameter = ioctlHelper->getEuStallFdParameter();
    std::array<uint64_t, 12u> properties;
    auto engineInfo = drm->getEngineInfo();
    if (engineInfo == nullptr) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    auto classInstance = engineInfo->getEngineInstance(deviceImp.getPhysicalSubDeviceId(), aub_stream::ENGINE_CCS);
    if (classInstance == nullptr) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    notifyEveryNReports = std::max(notifyEveryNReports, 1u);

    if (!ioctlHelper->getEuStallProperties(properties, maxDssBufferSize, samplingUnit, defaultPollPeriodNs,
                                           classInstance->engineInstance, notifyEveryNReports)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    if (!ioctlHelper->perfOpenEuStallStream(euStallFdParameter, properties, &stream)) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t MetricIpSamplingLinuxImp::stopMeasurement() {
    const auto drm = device.getOsInterface().getDriverModel()->as<NEO::Drm>();
    auto ioctlHelper = drm->getIoctlHelper();
    bool result = ioctlHelper->perfDisableEuStallStream(&stream);

    return result ? ZE_RESULT_SUCCESS : ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t MetricIpSamplingLinuxImp::readData(uint8_t *pRawData, size_t *pRawDataSize) {

    ssize_t ret = NEO::SysCalls::read(stream, pRawData, *pRawDataSize);
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get() && (ret < 0), stderr, "read() failed errno = %d | ret = %d \n",
                       errno, ret);

    if (ret >= 0) {
        *pRawDataSize = ret;
        return ZE_RESULT_SUCCESS;
    }

    *pRawDataSize = 0;

    // If read needs to try again, do not return error
    if (errno == EINTR || errno == EAGAIN || errno == EBUSY) {
        return ZE_RESULT_SUCCESS;
    }

    return ZE_RESULT_ERROR_UNKNOWN;
}

uint32_t MetricIpSamplingLinuxImp::getRequiredBufferSize(const uint32_t maxReportCount) {

    const auto hwInfo = device.getNEODevice()->getHardwareInfo();
    const auto maxSupportedReportCount = (maxDssBufferSize * hwInfo.gtSystemInfo.MaxDualSubSlicesSupported) / unitReportSize;
    return std::min(maxSupportedReportCount, maxReportCount) * getUnitReportSize();
}

uint32_t MetricIpSamplingLinuxImp::getUnitReportSize() {
    return unitReportSize;
}

bool MetricIpSamplingLinuxImp::isNReportsAvailable() {
    struct pollfd pollParams;
    memset(&pollParams, 0, sizeof(pollParams));

    DEBUG_BREAK_IF(stream == -1);

    pollParams.fd = stream;
    pollParams.revents = 0;
    pollParams.events = POLLIN;

    int32_t pollResult = NEO::SysCalls::poll(&pollParams, 1, 0u);
    PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get() && (pollResult < 0), stderr, "poll() failed errno = %d | pollResult = %d \n",
                       errno, pollResult);

    if (pollResult > 0) {
        return true;
    }
    return false;
}

bool MetricIpSamplingLinuxImp::isDependencyAvailable() {

    const auto &hardwareInfo = device.getNEODevice()->getHardwareInfo();
    const auto &productHelper = device.getNEODevice()->getProductHelper();

    if (!productHelper.isIpSamplingSupported(hardwareInfo)) {
        return false;
    }

    uint32_t notifyEveryNReports = 1u;
    uint32_t samplingPeriod = 100;

    ze_result_t status = startMeasurement(notifyEveryNReports, samplingPeriod);
    if (stream != -1) {
        stopMeasurement();
    }
    return status == ZE_RESULT_SUCCESS ? true : false;
}

ze_result_t MetricIpSamplingLinuxImp::getMetricsTimerResolution(uint64_t &timerResolution) {
    ze_result_t result = ZE_RESULT_SUCCESS;

    const auto drm = device.getOsInterface().getDriverModel()->as<NEO::Drm>();
    int32_t gpuTimeStampfrequency = 0;
    int32_t ret = drm->getTimestampFrequency(gpuTimeStampfrequency);
    if (ret < 0 || gpuTimeStampfrequency == 0) {
        timerResolution = 0;
        result = ZE_RESULT_ERROR_UNKNOWN;
        PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "getTimestampFrequency() failed errno = %d | ret = %d \n",
                           errno, ret);
    } else {
        timerResolution = static_cast<uint64_t>(gpuTimeStampfrequency);
    }

    return result;
}

std::unique_ptr<MetricIpSamplingOsInterface> MetricIpSamplingOsInterface::create(Device &device) {
    return std::make_unique<MetricIpSamplingLinuxImp>(device);
}

} // namespace L0
