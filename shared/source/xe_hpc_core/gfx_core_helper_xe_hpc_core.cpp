/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/aub_mapper.h"
#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"

using Family = NEO::XeHpcCoreFamily;

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.inl"
#include "shared/source/helpers/gfx_core_helper_base.inl"
#include "shared/source/helpers/gfx_core_helper_dg2_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_pvc_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_tgllp_and_later.inl"
#include "shared/source/helpers/gfx_core_helper_xehp_and_later.inl"

namespace NEO {

template <>
const AuxTranslationMode GfxCoreHelperHw<Family>::defaultAuxTranslationMode = AuxTranslationMode::blit;

template <>
uint8_t GfxCoreHelperHw<Family>::getBarriersCountFromHasBarriers(uint8_t hasBarriers) const {
    static constexpr uint8_t possibleBarriersCounts[] = {
        0u,  // 0
        1u,  // 1
        2u,  // 2
        4u,  // 3
        8u,  // 4
        16u, // 5
        24u, // 6
        32u, // 7
    };
    return possibleBarriersCounts[hasBarriers];
}

template <>
const EngineInstancesContainer GfxCoreHelperHw<Family>::getGpgpuEngineInstances(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto defaultEngine = getChosenEngineType(hwInfo);
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();

    EngineInstancesContainer engines;

    if (hwInfo.featureTable.flags.ftrCCSNode) {
        for (uint32_t i = 0; i < hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled; i++) {
            engines.push_back({static_cast<aub_stream::EngineType>(i + aub_stream::ENGINE_CCS), EngineUsage::regular});
            if (productHelper.isCooperativeEngineSupported(hwInfo)) {
                engines.push_back({static_cast<aub_stream::EngineType>(i + aub_stream::ENGINE_CCS), EngineUsage::cooperative});
            }
        }
    }

    if ((debugManager.flags.NodeOrdinal.get() == static_cast<int32_t>(aub_stream::EngineType::ENGINE_CCCS)) ||
        hwInfo.featureTable.flags.ftrRcsNode) {
        engines.push_back({aub_stream::ENGINE_CCCS, EngineUsage::regular});
    }

    engines.push_back({defaultEngine, EngineUsage::lowPriority});
    engines.push_back({defaultEngine, EngineUsage::internal});

    if (hwInfo.capabilityTable.blitterOperationsSupported) {
        if (hwInfo.featureTable.ftrBcsInfo.test(0)) {
            engines.push_back({aub_stream::EngineType::ENGINE_BCS, EngineUsage::regular});  // Main copy engine
            engines.push_back({aub_stream::EngineType::ENGINE_BCS, EngineUsage::internal}); // Internal usage
        }

        for (uint32_t i = 1; i < hwInfo.featureTable.ftrBcsInfo.size(); i++) {
            if (hwInfo.featureTable.ftrBcsInfo.test(i)) {
                auto engineType = static_cast<aub_stream::EngineType>((i - 1) + aub_stream::ENGINE_BCS1); // Link copy engine
                engines.push_back({engineType, EngineUsage::regular});
                uint32_t internalIndex = 3;
                if (debugManager.flags.ForceBCSForInternalCopyEngine.get() != -1) {
                    internalIndex = debugManager.flags.ForceBCSForInternalCopyEngine.get();
                }
                if (i == internalIndex) {
                    engines.push_back({engineType, EngineUsage::internal}); // BCS3 for internal usage
                }
            }
        }
    }

    return engines;
};

template <>
EngineGroupType GfxCoreHelperHw<Family>::getEngineGroupType(aub_stream::EngineType engineType, EngineUsage engineUsage, const HardwareInfo &hwInfo) const {
    if (engineType == aub_stream::ENGINE_CCCS) {
        return EngineGroupType::renderCompute;
    }
    if (engineType >= aub_stream::ENGINE_CCS && engineType < (aub_stream::ENGINE_CCS + hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled)) {
        if (engineUsage == EngineUsage::cooperative) {
            return EngineGroupType::cooperativeCompute;
        }
        return EngineGroupType::compute;
    }
    if (engineType == aub_stream::ENGINE_BCS) {
        return EngineGroupType::copy;
    }
    if (engineType >= aub_stream::ENGINE_BCS1 && engineType < aub_stream::ENGINE_BCS1 + hwInfo.featureTable.ftrBcsInfo.size() - 1) {
        return EngineGroupType::linkedCopy;
    }
    UNRECOVERABLE_IF(true);
}

template <>
void GfxCoreHelperHw<Family>::adjustDefaultEngineType(HardwareInfo *pHwInfo, const ProductHelper &productHelper, AILConfiguration *ailConfiguration) {
    if (!pHwInfo->featureTable.flags.ftrCCSNode) {
        pHwInfo->capabilityTable.defaultEngineType = aub_stream::EngineType::ENGINE_CCCS;
    }
}

template <>
uint32_t GfxCoreHelperHw<Family>::getMetricsLibraryGenId() const {
    return static_cast<uint32_t>(MetricsLibraryApi::ClientGen::XeHPC);
}

template <>
uint32_t GfxCoreHelperHw<Family>::getMinimalSIMDSize() const {
    return 16u;
}

template <>
uint32_t GfxCoreHelperHw<Family>::getMocsIndex(const GmmHelper &gmmHelper, bool l3enabled, bool l1enabled) const {
    if (l3enabled) {
        return gmmHelper.getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1;
    }
    return gmmHelper.getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1;
}

template <>
const StackVec<size_t, 3> GfxCoreHelperHw<Family>::getDeviceSubGroupSizes() const {
    return {16, 32};
}

template <>
size_t MemorySynchronizationCommands<Family>::getSizeForSingleAdditionalSynchronization(const RootDeviceEnvironment &rootDeviceEnvironment) {
    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto programGlobalFenceAsMiMemFenceCommandInCommandStream = productHelper.isGlobalFenceInCommandStreamRequired(hwInfo);
    if (debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get() != -1) {
        programGlobalFenceAsMiMemFenceCommandInCommandStream = !!debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get();
    }

    if (programGlobalFenceAsMiMemFenceCommandInCommandStream) {
        return sizeof(Family::MI_MEM_FENCE);
    } else {
        return EncodeSemaphore<Family>::getSizeMiSemaphoreWait();
    }
}

template <>
void MemorySynchronizationCommands<Family>::setAdditionalSynchronization(void *&commandsBuffer, uint64_t gpuAddress, bool acquire, const RootDeviceEnvironment &rootDeviceEnvironment) {
    using MI_MEM_FENCE = typename Family::MI_MEM_FENCE;
    using MI_SEMAPHORE_WAIT = typename Family::MI_SEMAPHORE_WAIT;

    const auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    auto programGlobalFenceAsMiMemFenceCommandInCommandStream = productHelper.isGlobalFenceInCommandStreamRequired(hwInfo);
    if (debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get() != -1) {
        programGlobalFenceAsMiMemFenceCommandInCommandStream = !!debugManager.flags.ProgramGlobalFenceAsMiMemFenceCommandInCommandStream.get();
    }
    if (programGlobalFenceAsMiMemFenceCommandInCommandStream) {
        MI_MEM_FENCE miMemFence = Family::cmdInitMemFence;
        if (acquire) {
            miMemFence.setFenceType(Family::MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_ACQUIRE);
        } else {
            miMemFence.setFenceType(Family::MI_MEM_FENCE::FENCE_TYPE::FENCE_TYPE_RELEASE);
        }
        *reinterpret_cast<MI_MEM_FENCE *>(commandsBuffer) = miMemFence;
        commandsBuffer = ptrOffset(commandsBuffer, sizeof(MI_MEM_FENCE));
    } else {
        EncodeSemaphore<Family>::programMiSemaphoreWait(reinterpret_cast<MI_SEMAPHORE_WAIT *>(commandsBuffer),
                                                        gpuAddress,
                                                        EncodeSemaphore<Family>::invalidHardwareTag,
                                                        MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                        false, true, false, false, false);
        commandsBuffer = ptrOffset(commandsBuffer, EncodeSemaphore<Family>::getSizeMiSemaphoreWait());
    }
}

template <>
bool MemorySynchronizationCommands<Family>::isBarrierWaRequired(const RootDeviceEnvironment &rootDeviceEnvironment) {
    if (debugManager.flags.DisablePipeControlPrecedingPostSyncCommand.get() == 1) {
        return true;
    }
    return false;
}

template <>
size_t MemorySynchronizationCommands<Family>::getSizeForAdditonalSynchronization(const RootDeviceEnvironment &rootDeviceEnvironment) {
    return (debugManager.flags.DisablePipeControlPrecedingPostSyncCommand.get() == 1 ? 2 : 1) * getSizeForSingleAdditionalSynchronization(rootDeviceEnvironment);
}

template <>
void GfxCoreHelperHw<Family>::setL1CachePolicy(bool useL1Cache, typename Family::RENDER_SURFACE_STATE *surfaceState, const HardwareInfo *hwInfo) const {
    if (useL1Cache) {
        surfaceState->setL1CachePolicyL1CacheControl(Family::RENDER_SURFACE_STATE::L1_CACHE_POLICY_WB);
        if (debugManager.flags.OverrideL1CacheControlInSurfaceStateForScratchSpace.get() != -1) {
            surfaceState->setL1CachePolicyL1CacheControl(static_cast<typename Family::RENDER_SURFACE_STATE::L1_CACHE_POLICY>(debugManager.flags.OverrideL1CacheControlInSurfaceStateForScratchSpace.get()));
        }
    }
}

template <>
void GfxCoreHelperHw<Family>::setExtraAllocationData(AllocationData &allocationData, const AllocationProperties &properties, const RootDeviceEnvironment &rootDeviceEnvironment) const {
    if (properties.allocationType == AllocationType::timestampPacketTagBuffer || properties.allocationType == AllocationType::commandBuffer) {
        allocationData.flags.useSystemMemory = false;
    }

    bool forceLocalMemoryForDirectSubmission = true;
    switch (debugManager.flags.DirectSubmissionForceLocalMemoryStorageMode.get()) {
    case 0:
        forceLocalMemoryForDirectSubmission = false;
        break;
    case 1:
        forceLocalMemoryForDirectSubmission = properties.flags.multiOsContextCapable;
        break;
    default:
        break;
    }

    if (forceLocalMemoryForDirectSubmission) {
        if (properties.allocationType == AllocationType::commandBuffer ||
            properties.allocationType == AllocationType::ringBuffer ||
            properties.allocationType == AllocationType::semaphoreBuffer) {
            allocationData.flags.useSystemMemory = false;
            allocationData.flags.requiresCpuAccess = true;
        }
    }

    allocationData.cacheRegion = properties.cacheRegion;

    if (allocationData.flags.requiresCpuAccess && !allocationData.flags.useSystemMemory &&
        (allocationData.storageInfo.getMemoryBanks() > 1)) {

        auto &productHeler = rootDeviceEnvironment.getHelper<ProductHelper>();
        auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
        bool applyWa = productHeler.isTilePlacementResourceWaRequired(hwInfo);

        if (applyWa) {
            allocationData.storageInfo.memoryBanks = 1; // force Tile0
        }
    }
}

template <>
uint32_t GfxCoreHelperHw<Family>::getNumCacheRegions() const {
    constexpr uint32_t numSharedCacheRegions = 1;
    constexpr uint32_t numReservedCacheRegions = 2;
    constexpr uint32_t numTotalCacheRegions = numSharedCacheRegions + numReservedCacheRegions;
    return numTotalCacheRegions;
}

template <>
uint32_t GfxCoreHelperHw<Family>::alignSlmSize(uint32_t slmSize) const {
    const uint32_t alignedSlmSizes[] = {
        0u,
        1u * MemoryConstants::kiloByte,
        2u * MemoryConstants::kiloByte,
        4u * MemoryConstants::kiloByte,
        8u * MemoryConstants::kiloByte,
        16u * MemoryConstants::kiloByte,
        24u * MemoryConstants::kiloByte,
        32u * MemoryConstants::kiloByte,
        48u * MemoryConstants::kiloByte,
        64u * MemoryConstants::kiloByte,
        96u * MemoryConstants::kiloByte,
        128u * MemoryConstants::kiloByte,
    };

    for (auto &alignedSlmSize : alignedSlmSizes) {
        if (slmSize <= alignedSlmSize) {
            return alignedSlmSize;
        }
    }

    UNRECOVERABLE_IF(true);
    return 0;
}

template <>
uint32_t GfxCoreHelperHw<Family>::computeSlmValues(const HardwareInfo &hwInfo, uint32_t slmSize) const {
    using SHARED_LOCAL_MEMORY_SIZE = typename Family::INTERFACE_DESCRIPTOR_DATA::SHARED_LOCAL_MEMORY_SIZE;
    if (slmSize == 0u) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_0K;
    }

    UNRECOVERABLE_IF(slmSize > 128u * MemoryConstants::kiloByte);

    if (slmSize > 96u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_128K;
    }
    if (slmSize > 64u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_96K;
    }
    if (slmSize > 48u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_64K;
    }
    if (slmSize > 32u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_48K;
    }
    if (slmSize > 24u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_32K;
    }
    if (slmSize > 16u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_24K;
    }
    if (slmSize > 8u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_16K;
    }
    if (slmSize > 4u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_8K;
    }
    if (slmSize > 2u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_4K;
    }
    if (slmSize > 1u * MemoryConstants::kiloByte) {
        return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_2K;
    }
    return SHARED_LOCAL_MEMORY_SIZE::SHARED_LOCAL_MEMORY_SIZE_ENCODES_1K;
}

template <>
int32_t GfxCoreHelperHw<Family>::getDefaultThreadArbitrationPolicy() const {
    return ThreadArbitrationPolicy::RoundRobinAfterDependency;
}

template <>
bool GfxCoreHelperHw<Family>::isSubDeviceEngineSupported(const RootDeviceEnvironment &rootDeviceEnvironment, const DeviceBitfield &deviceBitfield, aub_stream::EngineType engineType) const {
    constexpr uint64_t tile1Bitfield = 0b10;

    bool affectedEngine = (deviceBitfield.to_ulong() == tile1Bitfield) &&
                          (aub_stream::ENGINE_BCS == engineType ||
                           aub_stream::ENGINE_BCS1 == engineType ||
                           aub_stream::ENGINE_BCS3 == engineType);

    auto &productHelper = rootDeviceEnvironment.template getHelper<ProductHelper>();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    return affectedEngine ? !productHelper.isBcsReportWaRequired(hwInfo) : true;
}

template <>
uint32_t GfxCoreHelperHw<Family>::getComputeUnitsUsedForScratch(const RootDeviceEnvironment &rootDeviceEnvironment) const {
    if (debugManager.flags.OverrideNumComputeUnitsForScratch.get() != -1) {
        return static_cast<uint32_t>(debugManager.flags.OverrideNumComputeUnitsForScratch.get());
    }

    auto &helper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto hwInfo = rootDeviceEnvironment.getHardwareInfo();
    uint32_t threadEuRatio = helper.getThreadEuRatioForScratch(*hwInfo);

    return hwInfo->gtSystemInfo.MaxSubSlicesSupported * hwInfo->gtSystemInfo.MaxEuPerSubSlice * threadEuRatio;
}

template <>
size_t GfxCoreHelperHw<Family>::getSipKernelMaxDbgSurfaceSize(const HardwareInfo &hwInfo) const {
    return 40 * MemoryConstants::megaByte;
}

template <>
uint64_t GfxCoreHelperHw<Family>::getPatIndex(CacheRegion cacheRegion, CachePolicy cachePolicy) const {
    /*
    PAT Index  CLOS   MemType
    SHARED
    0          0      UC (00)
    1          0      WC (01)
    2          0      WT (10)
    3          0      WB (11)
    RESERVED 1
    4          1      WT (10)
    5          1      WB (11)
    RESERVED 2
    6          2      WT (10)
    7          2      WB (11)
    */

    if ((debugManager.flags.ForceAllResourcesUncached.get() == true)) {
        cacheRegion = CacheRegion::defaultRegion;
        cachePolicy = CachePolicy::uncached;
    }

    UNRECOVERABLE_IF((cacheRegion > CacheRegion::defaultRegion) && (cachePolicy < CachePolicy::writeThrough));
    return (static_cast<uint32_t>(cachePolicy) + (static_cast<uint16_t>(cacheRegion) * 2));
}

template <>
bool GfxCoreHelperHw<Family>::copyThroughLockedPtrEnabled(const HardwareInfo &hwInfo, const ProductHelper &productHelper) const {
    if (debugManager.flags.ExperimentalCopyThroughLock.get() != -1) {
        return debugManager.flags.ExperimentalCopyThroughLock.get() == 1;
    }
    return true;
}

template <>
bool GfxCoreHelperHw<Family>::isRelaxedOrderingSupported() const {
    return true;
}

template <>
char const *GfxCoreHelperHw<Family>::getDefaultDeviceHierarchy() const {
    return deviceHierarchyFlat;
}

} // namespace NEO

namespace NEO {
template class GfxCoreHelperHw<Family>;
template class FlatBatchBufferHelperHw<Family>;
template struct MemorySynchronizationCommands<Family>;
template struct LriHelper<Family>;
} // namespace NEO
