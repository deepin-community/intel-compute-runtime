/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/device/device_info.h"
#include "shared/source/helpers/engine_control.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/options.h"
#include "shared/source/os_interface/performance_counters.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/reference_tracked_object.h"

#include <array>

namespace NEO {
class BindlessHeapsHelper;
class BuiltIns;
class CompilerInterface;
class ExecutionEnvironment;
class Debugger;
class GmmClientContext;
class GmmHelper;
class SyncBufferHandler;
enum class EngineGroupType : uint32_t;
class DebuggerL0;
class OSTime;
class SubDevice;
struct PhysicalDevicePciBusInfo;
class GfxCoreHelper;
class ProductHelper;
class CompilerProductHelper;
class ReleaseHelper;

struct SelectorCopyEngine : NonCopyableOrMovableClass {
    std::atomic<bool> isMainUsed = false;
    std::atomic<uint32_t> selector = 0;
};

using EnginesT = std::vector<EngineControl>;
struct EngineGroupT {
    EngineGroupType engineGroupType;
    EnginesT engines;
};
using EngineGroupsT = std::vector<EngineGroupT>;

struct SecondaryContexts {
    SecondaryContexts() = default;
    SecondaryContexts(SecondaryContexts &&in) {
        this->engines = std::move(in.engines);
        this->regularCounter = in.regularCounter.load();
        this->highPriorityCounter = in.highPriorityCounter.load();
        this->regularEnginesTotal = in.regularEnginesTotal;
        this->highPriorityEnginesTotal = in.highPriorityEnginesTotal;
    }
    SecondaryContexts(const SecondaryContexts &in) = delete;
    SecondaryContexts &operator=(const SecondaryContexts &) = delete;

    EnginesT engines;                             // vector of secondary EngineControls
    std::atomic<uint8_t> regularCounter = 0;      // Counter used to assign next regular EngineControl
    std::atomic<uint8_t> highPriorityCounter = 0; // Counter used to assign next highPriority EngineControl
    uint32_t regularEnginesTotal;
    uint32_t highPriorityEnginesTotal;
};

struct RTDispatchGlobalsInfo {
    GraphicsAllocation *rtDispatchGlobalsArray = nullptr;
    std::vector<GraphicsAllocation *> rtStacks; // per tile
};

class Device : public ReferenceTrackedObject<Device> {
  public:
    Device &operator=(const Device &) = delete;
    Device(const Device &) = delete;
    ~Device() override;

    template <typename DeviceT, typename... ArgsT>
    static DeviceT *create(ArgsT &&...args) {
        DeviceT *device = new DeviceT(std::forward<ArgsT>(args)...);
        return createDeviceInternals(device);
    }

    virtual void incRefInternal() {
        ReferenceTrackedObject<Device>::incRefInternal();
    }
    virtual unique_ptr_if_unused<Device> decRefInternal() {
        return ReferenceTrackedObject<Device>::decRefInternal();
    }

    bool getDeviceAndHostTimer(uint64_t *deviceTimestamp, uint64_t *hostTimestamp) const;
    bool getHostTimer(uint64_t *hostTimestamp) const;
    const HardwareInfo &getHardwareInfo() const;
    const DeviceInfo &getDeviceInfo() const;
    EngineControl *tryGetEngine(aub_stream::EngineType engineType, EngineUsage engineUsage);
    EngineControl &getEngine(aub_stream::EngineType engineType, EngineUsage engineUsage);
    EngineGroupsT &getRegularEngineGroups() {
        return this->regularEngineGroups;
    }
    size_t getEngineGroupIndexFromEngineGroupType(EngineGroupType engineGroupType) const;
    EngineControl &getEngine(uint32_t index);
    EngineControl &getDefaultEngine();
    EngineControl &getNextEngineForCommandQueue();
    EngineControl &getNextEngineForMultiRegularContextMode(aub_stream::EngineType engineType);
    EngineControl &getInternalEngine();
    EngineControl *getInternalCopyEngine();
    SelectorCopyEngine &getSelectorCopyEngine();
    MemoryManager *getMemoryManager() const;
    GmmHelper *getGmmHelper() const;
    GmmClientContext *getGmmClientContext() const;
    OSTime *getOSTime() const;
    double getProfilingTimerResolution();
    uint64_t getProfilingTimerClock();
    double getPlatformHostTimerResolution() const;
    GFXCORE_FAMILY getRenderCoreFamily() const;
    PerformanceCounters *getPerformanceCounters() { return performanceCounters.get(); }
    PreemptionMode getPreemptionMode() const { return preemptionMode; }
    Debugger *getDebugger() const;
    DebuggerL0 *getL0Debugger();
    const EnginesT &getAllEngines() const;
    const std::string getDeviceName() const;

    ExecutionEnvironment *getExecutionEnvironment() const { return executionEnvironment; }
    const RootDeviceEnvironment &getRootDeviceEnvironment() const;
    RootDeviceEnvironment &getRootDeviceEnvironmentRef() const;
    bool isFullRangeSvm() const;
    static bool isBlitSplitEnabled();
    static bool isInitDeviceWithFirstSubmissionEnabled(CommandStreamReceiverType csrType);
    bool isBcsSplitSupported();
    bool isInitDeviceWithFirstSubmissionSupported(CommandStreamReceiverType csrType);
    bool areSharedSystemAllocationsAllowed() const;
    template <typename SpecializedDeviceT>
    void setSpecializedDevice(SpecializedDeviceT *specializedDevice) {
        this->specializedDevice = reinterpret_cast<uintptr_t>(specializedDevice);
    }
    template <typename SpecializedDeviceT>
    SpecializedDeviceT *getSpecializedDevice() const {
        return reinterpret_cast<SpecializedDeviceT *>(specializedDevice);
    }
    MOCKABLE_VIRTUAL CompilerInterface *getCompilerInterface() const;
    BuiltIns *getBuiltIns() const;
    void allocateSyncBufferHandler();

    uint32_t getRootDeviceIndex() const {
        return this->rootDeviceIndex;
    }
    uint32_t getNumGenericSubDevices() const;
    Device *getSubDevice(uint32_t deviceId) const;
    Device *getNearestGenericSubDevice(uint32_t deviceId);
    virtual Device *getRootDevice() const = 0;
    DeviceBitfield getDeviceBitfield() const { return deviceBitfield; };
    uint32_t getNumSubDevices() const { return numSubDevices; }
    virtual bool isSubDevice() const = 0;
    bool hasRootCsr() const { return rootCsrCreated; }
    bool isEngineInstanced() const { return engineInstanced; }

    BindlessHeapsHelper *getBindlessHeapsHelper() const;

    static decltype(&PerformanceCounters::create) createPerformanceCountersFunc;
    std::unique_ptr<SyncBufferHandler> syncBufferHandler;
    GraphicsAllocation *getRTMemoryBackedBuffer() { return rtMemoryBackedBuffer; }
    RTDispatchGlobalsInfo *getRTDispatchGlobals(uint32_t maxBvhLevels);
    bool rayTracingIsInitialized() const { return rtMemoryBackedBuffer != nullptr; }
    void initializeRayTracing(uint32_t maxBvhLevels);
    void allocateRTDispatchGlobals(uint32_t maxBvhLevels);

    uint64_t getGlobalMemorySize(uint32_t deviceBitfield) const;
    const std::vector<SubDevice *> getSubDevices() const { return subdevices; }
    bool getUuid(std::array<uint8_t, ProductHelper::uuidSize> &uuid);
    void generateUuid(std::array<uint8_t, ProductHelper::uuidSize> &uuid);
    void getAdapterLuid(std::array<uint8_t, ProductHelper::luidSize> &luid);
    MOCKABLE_VIRTUAL bool verifyAdapterLuid();
    void getAdapterMask(uint32_t &nodeMask);
    const GfxCoreHelper &getGfxCoreHelper() const;
    const ProductHelper &getProductHelper() const;
    const CompilerProductHelper &getCompilerProductHelper() const;
    ReleaseHelper *getReleaseHelper() const;

    uint32_t getNumberOfRegularContextsPerEngine() const { return numberOfRegularContextsPerEngine; }
    bool isMultiRegularContextSelectionAllowed(aub_stream::EngineType engineType, EngineUsage engineUsage) const;
    MOCKABLE_VIRTUAL void stopDirectSubmissionAndWaitForCompletion();
    bool isAnyDirectSubmissionEnabled();
    bool isStateSipRequired() const {
        return getPreemptionMode() == PreemptionMode::MidThread || getDebugger() != nullptr;
    }

    MOCKABLE_VIRTUAL EngineControl *getSecondaryEngineCsr(uint32_t engineIndex, EngineTypeUsage engineTypeUsage);
    bool isSecondaryContextEngineType(aub_stream::EngineType type) {
        return EngineHelpers::isCcs(type);
    }

    std::atomic<uint32_t> debugExecutionCounter = 0;

  protected:
    Device() = delete;
    Device(ExecutionEnvironment *executionEnvironment, const uint32_t rootDeviceIndex);

    MOCKABLE_VIRTUAL void initializeCaps();

    template <typename T>
    static T *createDeviceInternals(T *device) {
        if (false == device->createDeviceImpl()) {
            delete device;
            return nullptr;
        }
        return device;
    }

    MOCKABLE_VIRTUAL bool createDeviceImpl();
    virtual bool createEngines();

    void addEngineToEngineGroup(EngineControl &engine);
    MOCKABLE_VIRTUAL bool createEngine(uint32_t deviceCsrIndex, EngineTypeUsage engineTypeUsage);
    MOCKABLE_VIRTUAL bool createSecondaryEngine(CommandStreamReceiver *primaryCsr, uint32_t index, EngineTypeUsage engineTypeUsage);

    MOCKABLE_VIRTUAL std::unique_ptr<CommandStreamReceiver> createCommandStreamReceiver() const;
    MOCKABLE_VIRTUAL SubDevice *createSubDevice(uint32_t subDeviceIndex);
    MOCKABLE_VIRTUAL SubDevice *createEngineInstancedSubDevice(uint32_t subDeviceIndex, aub_stream::EngineType engineType);
    MOCKABLE_VIRTUAL size_t getMaxParameterSizeFromIGC() const;
    double getPercentOfGlobalMemoryAvailable() const;
    virtual void createBindlessHeapsHelper() {}
    bool createSubDevices();
    bool createGenericSubDevices();
    bool createEngineInstancedSubDevices();
    virtual bool genericSubDevicesAllowed();
    bool engineInstancedSubDevicesAllowed();
    void setAsEngineInstanced();
    void finalizeRayTracing();

    DeviceInfo deviceInfo = {};

    std::unique_ptr<PerformanceCounters> performanceCounters;
    std::vector<std::unique_ptr<CommandStreamReceiver>> commandStreamReceivers;
    EnginesT allEngines;

    std::vector<SecondaryContexts> secondaryEngines;

    EngineGroupsT regularEngineGroups;
    std::vector<SubDevice *> subdevices;

    PreemptionMode preemptionMode = PreemptionMode::Disabled;
    ExecutionEnvironment *executionEnvironment = nullptr;
    aub_stream::EngineType engineInstancedType = aub_stream::EngineType::NUM_ENGINES;
    uint32_t defaultEngineIndex = 0;
    uint32_t defaultBcsEngineIndex = std::numeric_limits<uint32_t>::max();
    uint32_t numSubDevices = 0;
    std::atomic_uint32_t regularCommandQueuesCreatedWithinDeviceCount{0};
    std::atomic<uint8_t> regularContextPerCcsEngineAssignmentHelper = 0;
    std::atomic<uint8_t> regularContextPerBcsEngineAssignmentHelper = 0;
    std::bitset<8> availableEnginesForCommandQueueusRoundRobin = 0;
    uint32_t queuesPerEngineCount = 1;
    uint32_t numberOfRegularContextsPerEngine = 1;
    void initializeEngineRoundRobinControls();
    bool hasGenericSubDevices = false;
    bool engineInstanced = false;
    bool rootCsrCreated = false;
    const uint32_t rootDeviceIndex;

    SelectorCopyEngine selectorCopyEngine = {};

    DeviceBitfield deviceBitfield = 1;

    uintptr_t specializedDevice = reinterpret_cast<uintptr_t>(nullptr);

    GraphicsAllocation *rtMemoryBackedBuffer = nullptr;
    std::vector<RTDispatchGlobalsInfo *> rtDispatchGlobalsInfos;

    struct {
        bool isValid = false;
        std::array<uint8_t, ProductHelper::uuidSize> id;
    } uuid;
    bool generateUuidFromPciBusInfo(const PhysicalDevicePciBusInfo &pciBusInfo, std::array<uint8_t, ProductHelper::uuidSize> &uuid);
};

inline EngineControl &Device::getDefaultEngine() {
    return allEngines[defaultEngineIndex];
}

inline SelectorCopyEngine &Device::getSelectorCopyEngine() {
    return selectorCopyEngine;
}

} // namespace NEO
