/*
 * Copyright (C) 2021-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/populate_factory.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

#include "opencl/source/command_queue/command_queue_hw.h"
#include "opencl/source/helpers/cl_gfx_core_helper.h"
#include "opencl/source/mem_obj/buffer.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/source/sampler/sampler.h"

namespace NEO {

using Family = XeHpcCoreFamily;

struct EnableOCLXeHpcCore {
    EnableOCLXeHpcCore() {
        populateFactoryTable<BufferHw<Family>>();
        populateFactoryTable<ClGfxCoreHelperHw<Family>>();
        populateFactoryTable<CommandQueueHw<Family>>();
        populateFactoryTable<ImageHw<Family>>();
        populateFactoryTable<SamplerHw<Family>>();
    }
};

static EnableOCLXeHpcCore enable;
} // namespace NEO
