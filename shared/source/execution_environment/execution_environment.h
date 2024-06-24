/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debugger/debugger.h"
#include "shared/source/utilities/reference_tracked_object.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace NEO {
class DirectSubmissionController;
class GfxCoreHelper;
class MemoryManager;
struct OsEnvironment;
struct RootDeviceEnvironment;

class ExecutionEnvironment : public ReferenceTrackedObject<ExecutionEnvironment> {

  public:
    ExecutionEnvironment();
    ~ExecutionEnvironment() override;

    MOCKABLE_VIRTUAL bool initializeMemoryManager();
    void calculateMaxOsContextCount();
    virtual void prepareRootDeviceEnvironments(uint32_t numRootDevices);
    void prepareRootDeviceEnvironment(const uint32_t rootDeviceIndexForReInit);
    void parseAffinityMask();
    void adjustCcsCount();
    void adjustCcsCount(const uint32_t rootDeviceIndex) const;
    void sortNeoDevices();
    void setDeviceHierarchy(const GfxCoreHelper &gfxCoreHelper);
    void adjustRootDeviceEnvironments();
    void prepareForCleanup() const;
    void configureCcsMode();
    void setDebuggingMode(DebuggingMode debuggingMode) {
        debuggingEnabledMode = debuggingMode;
    }
    DebuggingMode getDebuggingMode() const { return debuggingEnabledMode; }
    bool isDebuggingEnabled() const { return debuggingEnabledMode != DebuggingMode::disabled; }
    void setMetricsEnabled(bool value) {
        this->metricsEnabled = value;
    }
    void setExposeSubDevicesAsDevices(bool value) {
        this->subDevicesAsDevices = value;
    }
    void setCombinedDeviceHierarchy(bool value) {
        this->combinedDeviceHierarchy = value;
    }
    bool isExposingSubDevicesAsDevices() const { return this->subDevicesAsDevices; }
    bool isCombinedDeviceHierarchy() const { return this->combinedDeviceHierarchy; }
    bool getSubDeviceHierarchy(uint32_t index, std::tuple<uint32_t, uint32_t, uint32_t> *subDeviceMap);
    bool areMetricsEnabled() { return this->metricsEnabled; }
    void setFP64EmulationEnabled() {
        fp64EmulationEnabled = true;
    }
    bool isFP64EmulationEnabled() const { return fp64EmulationEnabled; }

    DirectSubmissionController *initializeDirectSubmissionController();

    std::unique_ptr<MemoryManager> memoryManager;
    std::unique_ptr<DirectSubmissionController> directSubmissionController;
    std::unique_ptr<OsEnvironment> osEnvironment;
    std::vector<std::unique_ptr<RootDeviceEnvironment>> rootDeviceEnvironments;
    void releaseRootDeviceEnvironmentResources(RootDeviceEnvironment *rootDeviceEnvironment);
    // Map of Sub Device Indicies set during Affinity Mask in the form of:
    // <RootDeviceIndex, SubDeviceIndex, SubDeviceCount>
    // Primarily used by the Metrics Library to communicate the actual Sub Device Index being used in queries.
    std::unordered_map<uint32_t, std::tuple<uint32_t, uint32_t, uint32_t>> mapOfSubDeviceIndices;

  protected:
    static bool comparePciIdBusNumber(std::unique_ptr<RootDeviceEnvironment> &rootDeviceEnvironment1, std::unique_ptr<RootDeviceEnvironment> &rootDeviceEnvironment2);
    void parseCcsCountLimitations();
    void adjustCcsCountImpl(RootDeviceEnvironment *rootDeviceEnvironment) const;
    void configureNeoEnvironment();
    void restoreCcsMode();
    bool metricsEnabled = false;
    bool fp64EmulationEnabled = false;
    bool subDevicesAsDevices = false;
    bool combinedDeviceHierarchy = false;

    DebuggingMode debuggingEnabledMode = DebuggingMode::disabled;
    std::unordered_map<uint32_t, uint32_t> rootDeviceNumCcsMap;
    std::mutex initializeDirectSubmissionControllerMutex;
    std::vector<std::tuple<std::string, uint32_t>> deviceCcsModeVec;
};
} // namespace NEO
