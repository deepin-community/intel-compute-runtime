/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_mapper.h"

namespace NEO {

struct XeHpcCoreFamily;

template <>
struct GfxFamilyMapper<IGFX_XE_HPC_CORE> {
    using GfxFamily = XeHpcCoreFamily;
    static const char *name;
};
} // namespace NEO
