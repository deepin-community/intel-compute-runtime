/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/hw_ip_version.h"
#include "shared/source/utilities/stackvec.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace NEO {

class ReleaseHelper;
enum class ReleaseType;

inline constexpr uint32_t maxArchitecture = 64;
using createReleaseHelperFunctionType = std::unique_ptr<ReleaseHelper> (*)(HardwareIpVersion hardwareIpVersion);
inline createReleaseHelperFunctionType *releaseHelperFactory[maxArchitecture]{};

using ThreadsPerEUConfigs = StackVec<uint32_t, 6>;

class ReleaseHelper {
  public:
    static std::unique_ptr<ReleaseHelper> create(HardwareIpVersion hardwareIpVersion);
    virtual ~ReleaseHelper() = default;

    virtual bool isAdjustWalkOrderAvailable() const = 0;
    virtual bool isMatrixMultiplyAccumulateSupported() const = 0;
    virtual bool isDotProductAccumulateSystolicSupported() const = 0;
    virtual bool isPipeControlPriorToNonPipelinedStateCommandsWARequired() const = 0;
    virtual bool isPipeControlPriorToPipelineSelectWaRequired() const = 0;
    virtual bool isProgramAllStateComputeCommandFieldsWARequired() const = 0;
    virtual bool isPrefetchDisablingRequired() const = 0;
    virtual bool isSplitMatrixMultiplyAccumulateSupported() const = 0;
    virtual bool isBFloat16ConversionSupported() const = 0;
    virtual bool isAuxSurfaceModeOverrideRequired() const = 0;
    virtual int getProductMaxPreferredSlmSize(int preferredEnumValue) const = 0;
    virtual bool getMediaFrequencyTileIndex(uint32_t &tileIndex) const = 0;
    virtual bool isResolvingSubDeviceIDNeeded() const = 0;
    virtual bool shouldAdjustDepth() const = 0;
    virtual bool isDirectSubmissionSupported() const = 0;
    virtual bool isRcsExposureDisabled() const = 0;
    virtual std::vector<uint32_t> getSupportedNumGrfs() const = 0;
    virtual bool isBindlessAddressingDisabled() const = 0;
    virtual uint32_t getNumThreadsPerEu() const = 0;
    virtual uint64_t getTotalMemBankSize() const = 0;
    virtual const ThreadsPerEUConfigs getThreadsPerEUConfigs() const = 0;
    virtual const std::string getDeviceConfigString(uint32_t tileCount, uint32_t sliceCount, uint32_t subSliceCount, uint32_t euPerSubSliceCount) const = 0;
    virtual bool isRayTracingSupported() const = 0;
    virtual uint32_t getL3BankCount() const = 0;
    virtual uint64_t getL3CacheBankSizeInKb() const = 0;

  protected:
    ReleaseHelper(HardwareIpVersion hardwareIpVersion) : hardwareIpVersion(hardwareIpVersion) {}
    HardwareIpVersion hardwareIpVersion{};
};

template <ReleaseType releaseType>
class ReleaseHelperHw : public ReleaseHelper {
  public:
    static std::unique_ptr<ReleaseHelper> create(HardwareIpVersion hardwareIpVersion) {
        return std::unique_ptr<ReleaseHelper>(new ReleaseHelperHw<releaseType>{hardwareIpVersion});
    }

    bool isAdjustWalkOrderAvailable() const override;
    bool isMatrixMultiplyAccumulateSupported() const override;
    bool isDotProductAccumulateSystolicSupported() const override;
    bool isPipeControlPriorToNonPipelinedStateCommandsWARequired() const override;
    bool isPipeControlPriorToPipelineSelectWaRequired() const override;
    bool isProgramAllStateComputeCommandFieldsWARequired() const override;
    bool isPrefetchDisablingRequired() const override;
    bool isSplitMatrixMultiplyAccumulateSupported() const override;
    bool isBFloat16ConversionSupported() const override;
    bool isAuxSurfaceModeOverrideRequired() const override;
    int getProductMaxPreferredSlmSize(int preferredEnumValue) const override;
    bool getMediaFrequencyTileIndex(uint32_t &tileIndex) const override;
    bool isResolvingSubDeviceIDNeeded() const override;
    bool shouldAdjustDepth() const override;
    bool isDirectSubmissionSupported() const override;
    bool isRcsExposureDisabled() const override;
    std::vector<uint32_t> getSupportedNumGrfs() const override;
    bool isBindlessAddressingDisabled() const override;
    uint32_t getNumThreadsPerEu() const override;
    uint64_t getTotalMemBankSize() const override;
    const StackVec<uint32_t, 6> getThreadsPerEUConfigs() const override;
    const std::string getDeviceConfigString(uint32_t tileCount, uint32_t sliceCount, uint32_t subSliceCount, uint32_t euPerSubSliceCount) const override;
    bool isRayTracingSupported() const override;
    uint32_t getL3BankCount() const override;
    uint64_t getL3CacheBankSizeInKb() const override;

  protected:
    ReleaseHelperHw(HardwareIpVersion hardwareIpVersion) : ReleaseHelper(hardwareIpVersion) {}
};

template <uint32_t architecture>
struct EnableReleaseHelperArchitecture {
    EnableReleaseHelperArchitecture(createReleaseHelperFunctionType *releaseTable) {
        releaseHelperFactory[architecture] = releaseTable;
    }
};

template <ReleaseType releaseType>
struct EnableReleaseHelper {
    EnableReleaseHelper(createReleaseHelperFunctionType &releaseTableEntry) {
        using ReleaseHelperType = ReleaseHelperHw<releaseType>;
        releaseTableEntry = ReleaseHelperType::create;
    }
};

} // namespace NEO
