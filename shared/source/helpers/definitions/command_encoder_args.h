/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstdint>
#include <limits>

namespace NEO {
struct RootDeviceEnvironment;

struct EncodeDummyBlitWaArgs {
    bool isWaRequired = false;
    RootDeviceEnvironment *rootDeviceEnvironment = nullptr;
};

struct MiFlushArgs {
    bool timeStampOperation = false;
    bool commandWithPostSync = false;
    bool notifyEnable = false;
    bool tlbFlush = false;

    EncodeDummyBlitWaArgs &waArgs;
    MiFlushArgs(EncodeDummyBlitWaArgs &args) : waArgs(args) {}
};

enum class RequiredPartitionDim : uint32_t {
    none = 0,
    x,
    y,
    z
};

enum class RequiredDispatchWalkOrder : uint32_t {
    none = 0,
    x,
    y,
    additional
};

static constexpr uint32_t additionalKernelLaunchSizeParamNotSet = 0;

} // namespace NEO
