/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace NEO {
enum PreemptionMode : uint32_t {
    // Keep in sync with ForcePreemptionMode debug variable
    Initial = 0,
    Disabled = 1,
    MidBatch,
    ThreadGroup,
    MidThread,
};
} // namespace NEO
