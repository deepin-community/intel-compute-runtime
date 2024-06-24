/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"

namespace NEO {

template <typename GfxFamily>
SubmissionStatus CommandStreamReceiverHw<GfxFamily>::initializeDeviceWithFirstSubmission(Device &device) {
    return flushTagUpdate();
}

template <typename GfxFamily>
CompletionStamp CommandStreamReceiverHw<GfxFamily>::flushTaskStateless(
    LinearStream &commandStream, size_t commandStreamStart,
    const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh,
    TaskCountType taskLevel, DispatchFlags &dispatchFlags, Device &device) {

    UNRECOVERABLE_IF(true);
    return {};
}

template <typename GfxFamily>
SubmissionStatus CommandStreamReceiverHw<GfxFamily>::programHeaplessProlog(Device &device) {
    UNRECOVERABLE_IF(true);
    return SubmissionStatus::unsupported;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programStateBaseAddressHeapless(Device &device, LinearStream &commandStream) {
    UNRECOVERABLE_IF(true);
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programComputeModeHeapless(Device &device, LinearStream &commandStream) {
    UNRECOVERABLE_IF(true);
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::programHeaplessStateProlog(Device &device, LinearStream &commandStream) {
    UNRECOVERABLE_IF(true);
}

template <typename GfxFamily>
size_t CommandStreamReceiverHw<GfxFamily>::getCmdSizeForHeaplessPrologue(Device &device) const {
    UNRECOVERABLE_IF(true);
    return 0;
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::handleAllocationsResidencyForflushTaskStateless(const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh) {
    UNRECOVERABLE_IF(true);
}

template <typename GfxFamily>
void CommandStreamReceiverHw<GfxFamily>::handleAllocationsResidencyForHeaplessProlog(LinearStream &linearStream, Device &device) {
    UNRECOVERABLE_IF(true);
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getRequiredCmdStreamHeaplessSize(const DispatchFlags &dispatchFlags, Device &device) {
    UNRECOVERABLE_IF(true);
    return 0u;
}

template <typename GfxFamily>
inline size_t CommandStreamReceiverHw<GfxFamily>::getRequiredCmdStreamHeaplessSizeAligned(const DispatchFlags &dispatchFlags, Device &device) {
    UNRECOVERABLE_IF(true);
    return 0u;
}

} // namespace NEO
