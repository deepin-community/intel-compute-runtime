/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/linux/system_info.h"

static constexpr uint32_t dummyDeviceBlobData[] = {
    NEO::DeviceBlobConstants::maxSlicesSupported,
    1,
    0x01,
    NEO::DeviceBlobConstants::maxDualSubSlicesSupported,
    1,
    0x04,
    NEO::DeviceBlobConstants::maxEuPerDualSubSlice,
    1,
    0x03,
    NEO::DeviceBlobConstants::maxMemoryChannels,
    1,
    0x0A,
    NEO::DeviceBlobConstants::memoryType,
    1,
    0x0B,
    NEO::DeviceBlobConstants::numThreadsPerEu,
    1,
    0x0F,
    NEO::DeviceBlobConstants::maxRcs,
    1,
    0x17,
    NEO::DeviceBlobConstants::maxCcs,
    1,
    0x18,
    NEO::DeviceBlobConstants::l3BankSizeInKb,
    1,
    0x2D,
};

const std::vector<uint32_t> inputBlobData(reinterpret_cast<const uint32_t *>(dummyDeviceBlobData),
                                          reinterpret_cast<const uint32_t *>(ptrOffset(dummyDeviceBlobData, sizeof(dummyDeviceBlobData))));
