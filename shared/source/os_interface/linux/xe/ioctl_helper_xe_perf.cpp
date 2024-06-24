/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

#include "drm/xe_drm.h"

namespace NEO {
bool IoctlHelperXe::perfOpenEuStallStream(uint32_t euStallFdParameter, std::array<uint64_t, 12u> &properties, int32_t *stream) {
    return false;
}

bool IoctlHelperXe::perfDisableEuStallStream(int32_t *stream) {
    return false;
}
bool IoctlHelperXe::getEuStallProperties(std::array<uint64_t, 12u> &properties, uint64_t dssBufferSize, uint64_t samplingRate,
                                         uint64_t pollPeriod, uint64_t engineInstance, uint64_t notifyNReports) {
    return false;
}
} // namespace NEO