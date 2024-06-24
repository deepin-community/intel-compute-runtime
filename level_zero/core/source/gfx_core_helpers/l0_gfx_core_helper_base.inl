/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/gfx_core_helper.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/event/event.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"

namespace L0 {

template <typename Family>
L0::Event *L0GfxCoreHelperHw<Family>::createEvent(L0::EventPool *eventPool, const ze_event_desc_t *desc, L0::Device *device) const {
    if (NEO::debugManager.flags.OverrideTimestampPacketSize.get() != -1) {
        if (NEO::debugManager.flags.OverrideTimestampPacketSize.get() == 4) {
            return Event::create<uint32_t>(eventPool, desc, device);
        } else if (NEO::debugManager.flags.OverrideTimestampPacketSize.get() == 8) {
            return Event::create<uint64_t>(eventPool, desc, device);
        } else {
            UNRECOVERABLE_IF(true);
        }
    }

    return Event::create<typename Family::TimestampPacketType>(eventPool, desc, device);
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::alwaysAllocateEventInLocalMem() const {
    return false;
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::multiTileCapablePlatform() const {
    return false;
}

template <typename Family>
zet_debug_regset_type_intel_gpu_t L0GfxCoreHelperHw<Family>::getRegsetTypeForLargeGrfDetection() const {
    return ZET_DEBUG_REGSET_TYPE_INVALID_INTEL_GPU;
}

template <typename Family>
uint32_t L0GfxCoreHelperHw<Family>::getCmdListWaitOnMemoryDataSize() const {
    if constexpr (Family::isQwordInOrderCounter) {
        return sizeof(uint64_t);
    } else {
        return sizeof(uint32_t);
    }
}

template <typename Family>
bool L0GfxCoreHelperHw<Family>::hasUnifiedPostSyncAllocationLayout() const {
    return (getImmediateWritePostSyncOffset() == NEO::ImplicitScalingDispatch<Family>::getTimeStampPostSyncOffset());
}

template <typename Family>
uint32_t L0GfxCoreHelperHw<Family>::getImmediateWritePostSyncOffset() const {
    return NEO::ImplicitScalingDispatch<Family>::getImmediateWritePostSyncOffset();
}

template <typename Family>
void L0GfxCoreHelperHw<Family>::appendPlatformSpecificExtensions(std::vector<std::pair<std::string, uint32_t>> &extensions, const NEO::ProductHelper &productHelper) const {
}

} // namespace L0
