/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/device_time_drm.h"

#include "shared/source/helpers/register_offsets.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/os_interface.h"

#include <time.h>

namespace NEO {

DeviceTimeDrm::DeviceTimeDrm(OSInterface &osInterface) {
    pDrm = osInterface.getDriverModel()->as<Drm>();
}

bool DeviceTimeDrm::getGpuCpuTimeImpl(TimeStampData *pGpuCpuTime, OSTime *osTime) {
    return pDrm->getIoctlHelper()->setGpuCpuTimes(pGpuCpuTime, osTime);
}

double DeviceTimeDrm::getDynamicDeviceTimerResolution(HardwareInfo const &hwInfo) const {
    if (pDrm) {
        int frequency = 0;

        auto error = pDrm->getTimestampFrequency(frequency);
        if (!error) {
            return nanosecondsPerSecond / frequency;
        }
    }
    return OSTime::getDeviceTimerResolution(hwInfo);
}

uint64_t DeviceTimeDrm::getDynamicDeviceTimerClock(HardwareInfo const &hwInfo) const {

    if (pDrm) {
        int frequency = 0;

        auto error = pDrm->getTimestampFrequency(frequency);
        if (!error) {
            return static_cast<uint64_t>(frequency);
        }
    }
    return static_cast<uint64_t>(nanosecondsPerSecond / OSTime::getDeviceTimerResolution(hwInfo));
}

} // namespace NEO