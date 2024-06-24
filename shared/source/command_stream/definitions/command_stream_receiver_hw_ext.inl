/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/linear_stream.h"

namespace NEO {

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programEngineModeCommands(LinearStream &csr, const DispatchFlags &dispatchFlags) {
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programEngineModeEpliogue(LinearStream &csr, const DispatchFlags &dispatchFlags) {
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForEngineMode(const DispatchFlags &dispatchFlags) const {
    return 0u;
}

} // namespace NEO
