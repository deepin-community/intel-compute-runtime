/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/device_bitfield.h"
#include "shared/source/helpers/vec.h"

namespace WalkerPartition {
struct WalkerPartitionArgs;
}

namespace NEO {
struct HardwareInfo;
class LinearStream;
struct PipeControlArgs;
struct RootDeviceEnvironment;

namespace ImplicitScaling {
extern bool apiSupport;
} // namespace ImplicitScaling

struct ImplicitScalingHelper {
    static bool isImplicitScalingEnabled(const DeviceBitfield &devices, bool preCondition);
    static bool isSemaphoreProgrammingRequired();
    static bool isCrossTileAtomicRequired(bool defaultCrossTileRequirement);
    static bool isSynchronizeBeforeExecutionRequired();
    static bool isAtomicsUsedForSelfCleanup();
    static bool isSelfCleanupRequired(const WalkerPartition::WalkerPartitionArgs &args, bool apiSelfCleanup);
    static bool isWparidRegisterInitializationRequired();
    static bool isPipeControlStallRequired(bool defaultEmitPipeControl);
    static bool pipeControlBeforeCleanupAtomicSyncRequired();
};

template <typename GfxFamily>
struct ImplicitScalingDispatch {
    using DefaultWalkerType = typename GfxFamily::DefaultWalkerType;

    template <typename WalkerType>
    static size_t getSize(bool apiSelfCleanup,
                          bool preferStaticPartitioning,
                          const DeviceBitfield &devices,
                          const Vec3<size_t> &groupStart,
                          const Vec3<size_t> &groupCount);

    template <typename WalkerType>
    static void dispatchCommands(LinearStream &commandStream,
                                 WalkerType &walkerCmd,
                                 void **outWalkerPtr,
                                 const DeviceBitfield &devices,
                                 RequiredPartitionDim requiredPartitionDim,
                                 uint32_t &partitionCount,
                                 bool useSecondaryBatchBuffer,
                                 bool apiSelfCleanup,
                                 bool dcFlush,
                                 bool forceExecutionOnSingleTile,
                                 uint64_t workPartitionAllocationGpuVa,
                                 const HardwareInfo &hwInfo);

    static bool &getPipeControlStallRequired();

    static size_t getBarrierSize(const RootDeviceEnvironment &rootDeviceEnvironment,
                                 bool apiSelfCleanup,
                                 bool usePostSync);
    static void dispatchBarrierCommands(LinearStream &commandStream,
                                        const DeviceBitfield &devices,
                                        PipeControlArgs &flushArgs,
                                        const RootDeviceEnvironment &rootDeviceEnvironment,
                                        uint64_t gpuAddress,
                                        uint64_t immediateData,
                                        bool apiSelfCleanup,
                                        bool useSecondaryBatchBuffer);

    static size_t getRegisterConfigurationSize();
    static void dispatchRegisterConfiguration(LinearStream &commandStream,
                                              uint64_t workPartitionSurfaceAddress,
                                              uint32_t addressOffset);

    static size_t getOffsetRegisterSize();
    static void dispatchOffsetRegister(LinearStream &commandStream,
                                       uint32_t addressOffset);

    static uint32_t getImmediateWritePostSyncOffset();
    static uint32_t getTimeStampPostSyncOffset();

    static bool platformSupportsImplicitScaling(const RootDeviceEnvironment &rootDeviceEnvironment);

  private:
    static bool pipeControlStallRequired;
};

template <typename GfxFamily>
struct PartitionRegisters {
    enum {
        wparidCCSOffset = 0x221C,
        addressOffsetCCSOffset = 0x23B4
    };
};

} // namespace NEO
