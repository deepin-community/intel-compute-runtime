/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/string.h"
#include "shared/source/release_helper/release_helper.h"

namespace NEO {

template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isMatrixMultiplyAccumulateSupported() const {
    return true;
}
template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isDotProductAccumulateSystolicSupported() const {
    return true;
}
template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isAdjustWalkOrderAvailable() const {
    return false;
}

template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isPipeControlPriorToNonPipelinedStateCommandsWARequired() const {
    return false;
}

template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isPipeControlPriorToPipelineSelectWaRequired() const {
    return false;
}

template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isProgramAllStateComputeCommandFieldsWARequired() const {
    return false;
}

template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isPrefetchDisablingRequired() const {
    return false;
}

template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isSplitMatrixMultiplyAccumulateSupported() const {
    return false;
}

template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isBFloat16ConversionSupported() const {
    return false;
}

template <ReleaseType releaseType>
inline bool ReleaseHelperHw<releaseType>::isAuxSurfaceModeOverrideRequired() const {
    return false;
}

template <ReleaseType releaseType>
int ReleaseHelperHw<releaseType>::getProductMaxPreferredSlmSize(int preferredEnumValue) const {
    return preferredEnumValue;
}

template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::getMediaFrequencyTileIndex(uint32_t &tileIndex) const {
    return false;
}

template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isResolvingSubDeviceIDNeeded() const {
    return true;
}

template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::shouldAdjustDepth() const {
    return false;
}

template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isDirectSubmissionSupported() const {
    return false;
}

template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isRcsExposureDisabled() const {
    return false;
}

template <ReleaseType releaseType>
std::vector<uint32_t> ReleaseHelperHw<releaseType>::getSupportedNumGrfs() const {
    return {128u, 256u};
}

template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isBindlessAddressingDisabled() const {
    return true;
}

template <ReleaseType releaseType>
uint32_t ReleaseHelperHw<releaseType>::getNumThreadsPerEu() const {
    return 8u;
}

template <ReleaseType releaseType>
uint64_t ReleaseHelperHw<releaseType>::getTotalMemBankSize() const {
    return 32ull * MemoryConstants::gigaByte;
}

template <ReleaseType releaseType>
const ThreadsPerEUConfigs ReleaseHelperHw<releaseType>::getThreadsPerEUConfigs() const {
    return {4, 8};
}

template <ReleaseType releaseType>
const std::string ReleaseHelperHw<releaseType>::getDeviceConfigString(uint32_t tileCount, uint32_t sliceCount, uint32_t subSliceCount, uint32_t euPerSubSliceCount) const {
    char configString[16] = {0};
    if (tileCount > 1) {
        auto err = snprintf_s(configString, sizeof(configString), sizeof(configString), "%utx%ux%ux%u", tileCount, sliceCount, subSliceCount, euPerSubSliceCount);
        UNRECOVERABLE_IF(err < 0);
    } else {
        auto err = snprintf_s(configString, sizeof(configString), sizeof(configString), "%ux%ux%u", sliceCount, subSliceCount, euPerSubSliceCount);
        UNRECOVERABLE_IF(err < 0);
    }
    return configString;
}

template <ReleaseType releaseType>
bool ReleaseHelperHw<releaseType>::isRayTracingSupported() const {
    return true;
}

template <ReleaseType releaseType>
uint32_t ReleaseHelperHw<releaseType>::getL3BankCount() const {
    return 0;
}

template <ReleaseType releaseType>
uint64_t ReleaseHelperHw<releaseType>::getL3CacheBankSizeInKb() const {
    return 0;
}

} // namespace NEO
