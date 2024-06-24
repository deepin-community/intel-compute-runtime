/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/mt_helpers.h"
#include "shared/source/utilities/reference_tracked_object.h"

#include <mutex>

namespace NEO {
class OSInterface;

struct DirectSubmissionProperties;
struct HardwareInfo;

class OsContext : public ReferenceTrackedObject<OsContext> {
  public:
    OsContext(uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor);
    static OsContext *create(OSInterface *osInterface, uint32_t rootDeviceIndex, uint32_t contextId, const EngineDescriptor &engineDescriptor);

    bool isImmediateContextInitializationEnabled(bool isDefaultEngine) const;
    bool isInitialized() const { return contextInitialized; }
    bool ensureContextInitialized();

    uint32_t getContextId() const { return contextId; }
    virtual uint64_t getOfflineDumpContextId(uint32_t deviceIndex) const { return 0; };
    uint32_t getNumSupportedDevices() const { return numSupportedDevices; }
    DeviceBitfield getDeviceBitfield() const { return deviceBitfield; }
    PreemptionMode getPreemptionMode() const { return preemptionMode; }
    const aub_stream::EngineType &getEngineType() const { return engineType; }
    EngineUsage getEngineUsage() { return engineUsage; }
    bool isRegular() const { return engineUsage == EngineUsage::regular; }
    bool isLowPriority() const { return engineUsage == EngineUsage::lowPriority; }
    bool isHighPriority() const { return engineUsage == EngineUsage::highPriority; }
    bool isInternalEngine() const { return engineUsage == EngineUsage::internal; }
    bool isCooperativeEngine() const { return engineUsage == EngineUsage::cooperative; }
    bool isRootDevice() const { return rootDevice; }
    bool isEngineInstanced() const { return engineInstancedDevice; }
    virtual bool isDirectSubmissionSupported() const { return false; }
    bool isDefaultContext() const { return defaultContext; }
    void setDefaultContext(bool value) { defaultContext = value; }
    bool isDirectSubmissionActive() { return directSubmissionActive; }
    bool isDebuggableContext() { return debuggableContext; }
    void setDirectSubmissionActive() { directSubmissionActive = true; }

    bool isDirectSubmissionAvailable(const HardwareInfo &hwInfo, bool &submitOnInit);
    bool checkDirectSubmissionSupportsEngine(const DirectSubmissionProperties &directSubmissionProperty,
                                             aub_stream::EngineType contextEngineType,
                                             bool &startOnInit,
                                             bool &startInContext);
    virtual void reInitializeContext() {}
    uint8_t getUmdPowerHintValue() { return powerHintValue; }
    void setUmdPowerHintValue(uint8_t powerHintValue) { this->powerHintValue = powerHintValue; }

    uint32_t getRootDeviceIndex() { return rootDeviceIndex; }

    void setNewResourceBound() {
        tlbFlushCounter++;
    };

    uint32_t peekTlbFlushCounter() const { return tlbFlushCounter.load(); }

    void setTlbFlushed(uint32_t newCounter) {
        NEO::MultiThreadHelpers::interlockedMax(lastFlushedTlbFlushCounter, newCounter);
    };
    bool isTlbFlushRequired() const {
        return (tlbFlushCounter.load() > lastFlushedTlbFlushCounter.load());
    };

    void setPrimaryContext(const OsContext *primary) {
        primaryContext = primary;
        isContextGroup = true;
    }
    const OsContext *getPrimaryContext() const {
        return primaryContext;
    }
    void setContextGroup(bool value) {
        isContextGroup = value;
    }
    bool isPartOfContextGroup() {
        return isContextGroup;
    }

  protected:
    virtual bool initializeContext() { return true; }

    std::atomic<uint32_t> tlbFlushCounter{0};
    std::atomic<uint32_t> lastFlushedTlbFlushCounter{0};

    const uint32_t rootDeviceIndex;
    const uint32_t contextId;
    const DeviceBitfield deviceBitfield;
    const PreemptionMode preemptionMode;
    const uint32_t numSupportedDevices;
    aub_stream::EngineType engineType = aub_stream::ENGINE_RCS;
    const EngineUsage engineUsage;
    const bool rootDevice = false;
    bool defaultContext = false;
    bool directSubmissionActive = false;
    std::once_flag contextInitializedFlag = {};
    bool contextInitialized = false;
    bool debuggableContext = false;
    bool engineInstancedDevice = false;
    uint8_t powerHintValue = 0;

    bool isContextGroup = false;
    const OsContext *primaryContext = nullptr;
};
} // namespace NEO
