/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/gen12lp/hw_info.h"

#include "level_zero/core/source/image/image_hw.inl"

namespace L0 {

template <>
struct ImageProductFamily<IGFX_ALDERLAKE_N> : public ImageCoreFamily<IGFX_GEN12LP_CORE> {
    using ImageCoreFamily::ImageCoreFamily;

    ze_result_t initialize(Device *device, const ze_image_desc_t *desc) override {
        return ImageCoreFamily<IGFX_GEN12LP_CORE>::initialize(device, desc);
    };
};

static ImagePopulateFactory<IGFX_ALDERLAKE_N, ImageProductFamily<IGFX_ALDERLAKE_N>> populateADLN;

} // namespace L0
