/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/experimental_command_buffer.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/submission_status.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/debugger/debugger_l0.h"
#include "shared/source/device/sub_device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/ray_tracing_helper.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/program/sync_buffer_handler.h"
#include "shared/source/utilities/software_tags_manager.h"

namespace NEO {

decltype(&PerformanceCounters::create) Device::createPerformanceCountersFunc = PerformanceCounters::create;
extern CommandStreamReceiver *createCommandStream(ExecutionEnvironment &executionEnvironment,
                                                  uint32_t rootDeviceIndex,
                                                  const DeviceBitfield deviceBitfield);

Device::Device(ExecutionEnvironment *executionEnvironment, const uint32_t rootDeviceIndex)
    : executionEnvironment(executionEnvironment), rootDeviceIndex(rootDeviceIndex) {
    this->executionEnvironment->incRefInternal();
    this->executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setDummyBlitProperties(rootDeviceIndex);

    if (debugManager.flags.NumberOfRegularContextsPerEngine.get() > 1) {
        this->numberOfRegularContextsPerEngine = static_cast<uint32_t>(debugManager.flags.NumberOfRegularContextsPerEngine.get());
    }
}

Device::~Device() {
    finalizeRayTracing();

    DEBUG_BREAK_IF(nullptr == executionEnvironment->memoryManager.get());

    if (performanceCounters) {
        performanceCounters->shutdown();
    }

    for (auto &engine : allEngines) {
        engine.commandStreamReceiver->flushBatchedSubmissions();
    }
    allEngines.clear();

    for (auto subdevice : subdevices) {
        if (subdevice) {
            delete subdevice;
        }
    }
    subdevices.clear();

    syncBufferHandler.reset();
    commandStreamReceivers.clear();
    executionEnvironment->memoryManager->waitForDeletions();

    executionEnvironment->decRefInternal();
}

SubDevice *Device::createSubDevice(uint32_t subDeviceIndex) {
    return Device::create<SubDevice>(executionEnvironment, subDeviceIndex, *getRootDevice());
}

SubDevice *Device::createEngineInstancedSubDevice(uint32_t subDeviceIndex, aub_stream::EngineType engineType) {
    return Device::create<SubDevice>(executionEnvironment, subDeviceIndex, *getRootDevice(), engineType);
}

bool Device::genericSubDevicesAllowed() {
    auto deviceMask = executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->deviceAffinityMask.getGenericSubDevicesMask();
    uint32_t subDeviceCount = GfxCoreHelper::getSubDevicesCount(&getHardwareInfo());
    deviceBitfield = maxNBitValue(subDeviceCount);
    deviceBitfield &= deviceMask;
    numSubDevices = static_cast<uint32_t>(deviceBitfield.count());
    if (numSubDevices == 1) {
        numSubDevices = 0;
    }

    return (numSubDevices > 0);
}

bool Device::engineInstancedSubDevicesAllowed() {
    bool notAllowed = !debugManager.flags.EngineInstancedSubDevices.get();
    notAllowed |= engineInstanced;
    notAllowed |= (getHardwareInfo().gtSystemInfo.CCSInfo.NumberOfCCSEnabled < 2);
    notAllowed |= ((GfxCoreHelper::getSubDevicesCount(&getHardwareInfo()) < 2) && (!debugManager.flags.AllowSingleTileEngineInstancedSubDevices.get()));

    if (notAllowed) {
        return false;
    }

    UNRECOVERABLE_IF(deviceBitfield.count() != 1);
    uint32_t subDeviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));

    auto enginesMask = getRootDeviceEnvironment().deviceAffinityMask.getEnginesMask(subDeviceIndex);
    auto ccsCount = getHardwareInfo().gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

    numSubDevices = std::min(ccsCount, static_cast<uint32_t>(enginesMask.count()));

    if (numSubDevices == 1) {
        numSubDevices = 0;
    }

    return (numSubDevices > 0);
}

bool Device::createEngineInstancedSubDevices() {
    UNRECOVERABLE_IF(deviceBitfield.count() != 1);
    UNRECOVERABLE_IF(!subdevices.empty());

    uint32_t subDeviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));

    auto enginesMask = getRootDeviceEnvironment().deviceAffinityMask.getEnginesMask(subDeviceIndex);
    auto ccsCount = getHardwareInfo().gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

    subdevices.resize(ccsCount, nullptr);

    for (uint32_t i = 0; i < ccsCount; i++) {
        if (!enginesMask.test(i)) {
            continue;
        }

        auto engineType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + i);
        auto subDevice = createEngineInstancedSubDevice(subDeviceIndex, engineType);
        UNRECOVERABLE_IF(!subDevice);
        subdevices[i] = subDevice;
    }

    return true;
}

bool Device::createGenericSubDevices() {
    UNRECOVERABLE_IF(!subdevices.empty());
    uint32_t subDeviceCount = GfxCoreHelper::getSubDevicesCount(&getHardwareInfo());

    subdevices.resize(subDeviceCount, nullptr);

    for (auto i = 0u; i < subDeviceCount; i++) {
        if (!deviceBitfield.test(i)) {
            continue;
        }
        auto subDevice = createSubDevice(i);
        if (!subDevice) {
            return false;
        }
        subdevices[i] = subDevice;
    }

    hasGenericSubDevices = true;
    return true;
}

bool Device::createSubDevices() {
    if (genericSubDevicesAllowed()) {
        return createGenericSubDevices();
    }

    if (engineInstancedSubDevicesAllowed()) {
        return createEngineInstancedSubDevices();
    }

    return true;
}

void Device::setAsEngineInstanced() {
    if (subdevices.size() > 0) {
        return;
    }

    UNRECOVERABLE_IF(deviceBitfield.count() != 1);

    uint32_t subDeviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));
    auto enginesMask = getRootDeviceEnvironment().deviceAffinityMask.getEnginesMask(subDeviceIndex);

    if (enginesMask.count() != 1) {
        return;
    }

    auto ccsCount = getHardwareInfo().gtSystemInfo.CCSInfo.NumberOfCCSEnabled;

    for (uint32_t i = 0; i < ccsCount; i++) {
        if (!enginesMask.test(i)) {
            continue;
        }

        UNRECOVERABLE_IF(engineInstanced);
        engineInstanced = true;
        engineInstancedType = static_cast<aub_stream::EngineType>(aub_stream::EngineType::ENGINE_CCS + i);
    }

    UNRECOVERABLE_IF(!engineInstanced);
}

bool Device::createDeviceImpl() {
    if (!createSubDevices()) {
        return false;
    }

    setAsEngineInstanced();

    auto &hwInfo = getHardwareInfo();
    preemptionMode = PreemptionHelper::getDefaultPreemptionMode(hwInfo);

    auto &productHelper = getProductHelper();
    if (getDebugger() && productHelper.disableL3CacheForDebug(hwInfo)) {
        getGmmHelper()->forceAllResourcesUncached();
    }

    getRootDeviceEnvironmentRef().initOsTime();

    initializeCaps();

    if (!createEngines()) {
        return false;
    }

    getDefaultEngine().osContext->setDefaultContext(true);

    for (auto &engine : allEngines) {
        auto commandStreamReceiver = engine.commandStreamReceiver;
        commandStreamReceiver->postInitFlagsSetup();
    }

    auto &registeredEngines = executionEnvironment->memoryManager->getRegisteredEngines(rootDeviceIndex);
    uint32_t defaultEngineIndexWithinMemoryManager = 0;
    for (auto engineIndex = 0u; engineIndex < registeredEngines.size(); engineIndex++) {
        OsContext *engine = registeredEngines[engineIndex].osContext;
        if (engine == getDefaultEngine().osContext) {
            defaultEngineIndexWithinMemoryManager = engineIndex;
            break;
        }
    }
    executionEnvironment->memoryManager->setDefaultEngineIndex(getRootDeviceIndex(), defaultEngineIndexWithinMemoryManager);

    if (getRootDeviceEnvironment().osInterface) {
        if (hwInfo.capabilityTable.instrumentationEnabled) {
            performanceCounters = createPerformanceCountersFunc(this);
        }
    }

    executionEnvironment->memoryManager->setForce32BitAllocations(getDeviceInfo().force32BitAddressess);

    if (debugManager.flags.EnableExperimentalCommandBuffer.get() > 0) {
        for (auto &engine : allEngines) {
            auto csr = engine.commandStreamReceiver;
            csr->setExperimentalCmdBuffer(std::make_unique<ExperimentalCommandBuffer>(csr, getDeviceInfo().profilingTimerResolution));
        }
    }

    if (debugManager.flags.EnableSWTags.get() && !getRootDeviceEnvironment().tagsManager->isInitialized()) {
        getRootDeviceEnvironment().tagsManager->initialize(*this);
    }

    createBindlessHeapsHelper();
    auto &gfxCoreHelper = getGfxCoreHelper();
    if (!isEngineInstanced()) {
        uuid.isValid = false;

        if (getRootDeviceEnvironment().osInterface == nullptr) {
            return true;
        }

        if (debugManager.flags.EnableChipsetUniqueUUID.get() != 0) {
            if (gfxCoreHelper.isChipsetUniqueUUIDSupported()) {

                auto deviceIndex = isSubDevice() ? static_cast<SubDevice *>(this)->getSubDeviceIndex() + 1 : 0;
                uuid.isValid = productHelper.getUuid(getRootDeviceEnvironment().osInterface->getDriverModel(), getRootDevice()->getNumSubDevices(), deviceIndex, uuid.id);
            }
        }

        if (!uuid.isValid) {
            PhysicalDevicePciBusInfo pciBusInfo = getRootDeviceEnvironment().osInterface->getDriverModel()->getPciBusInfo();
            uuid.isValid = generateUuidFromPciBusInfo(pciBusInfo, uuid.id);
        }
    }

    return true;
}

bool Device::createEngines() {
    if (engineInstanced) {
        return createEngine(0, {engineInstancedType, EngineUsage::regular});
    }

    auto &gfxCoreHelper = getGfxCoreHelper();
    auto gpgpuEngines = gfxCoreHelper.getGpgpuEngineInstances(getRootDeviceEnvironment());

    uint32_t deviceCsrIndex = 0;
    for (auto &engine : gpgpuEngines) {
        if (!createEngine(deviceCsrIndex++, engine)) {
            return false;
        }
    }

    if (gfxCoreHelper.areSecondaryContextsSupported()) {

        auto &hardwareInfo = this->getHardwareInfo();
        auto engineType = aub_stream::EngineType::ENGINE_CCS;

        if (tryGetEngine(engineType, EngineUsage::regular)) {
            auto contextCount = gfxCoreHelper.getContextGroupContextsCount();
            auto highPriorityContextCount = std::min(contextCount / 2, 4u);

            const EngineGroupType engineGroupType = gfxCoreHelper.getEngineGroupType(engineType, EngineUsage::regular, hardwareInfo);
            const auto engineGroupIndex = this->getEngineGroupIndexFromEngineGroupType(engineGroupType);
            auto &engineGroup = this->getRegularEngineGroups()[engineGroupIndex];

            secondaryEngines.resize(engineGroup.engines.size());

            for (uint32_t engineIndex = 0; engineIndex < static_cast<uint32_t>(engineGroup.engines.size()); engineIndex++) {
                auto primaryEngine = engineGroup.engines[engineIndex];

                secondaryEngines[engineIndex].regularEnginesTotal = contextCount - highPriorityContextCount;
                secondaryEngines[engineIndex].highPriorityEnginesTotal = highPriorityContextCount;
                secondaryEngines[engineIndex].regularCounter = 0;
                secondaryEngines[engineIndex].highPriorityCounter = 0;

                NEO::EngineTypeUsage engineTypeUsage;
                engineTypeUsage.first = primaryEngine.getEngineType();

                secondaryEngines[engineIndex].engines.push_back(primaryEngine);

                for (uint32_t i = 1; i < contextCount; i++) {
                    engineTypeUsage.second = EngineUsage::regular;

                    if (i >= contextCount - highPriorityContextCount) {
                        engineTypeUsage.second = EngineUsage::highPriority;
                    }
                    createSecondaryEngine(primaryEngine.commandStreamReceiver, engineIndex, engineTypeUsage);
                }

                primaryEngine.osContext->setContextGroup(true);
            }
        }
    }
    return true;
}

void Device::addEngineToEngineGroup(EngineControl &engine) {
    auto &hardwareInfo = this->getHardwareInfo();
    auto &gfxCoreHelper = getGfxCoreHelper();
    auto &productHelper = getProductHelper();
    auto &rootDeviceEnvironment = this->getRootDeviceEnvironment();

    EngineGroupType engineGroupType = gfxCoreHelper.getEngineGroupType(engine.getEngineType(), engine.getEngineUsage(), hardwareInfo);
    productHelper.adjustEngineGroupType(engineGroupType);

    if (!gfxCoreHelper.isSubDeviceEngineSupported(rootDeviceEnvironment, getDeviceBitfield(), engine.getEngineType())) {
        return;
    }

    if (EngineHelper::isCopyOnlyEngineType(engineGroupType) && debugManager.flags.EnableBlitterOperationsSupport.get() == 0) {
        return;
    }

    if (this->regularEngineGroups.empty() || this->regularEngineGroups.back().engineGroupType != engineGroupType) {
        this->regularEngineGroups.push_back(EngineGroupT{});
        this->regularEngineGroups.back().engineGroupType = engineGroupType;
    }

    auto &engines = this->regularEngineGroups.back().engines;

    if (engines.size() > 0 && engines.back().getEngineType() == engine.getEngineType()) {
        return; // Type already added. Exposing multiple contexts for the same engine is disabled.
    }

    engines.push_back(engine);
}

std::unique_ptr<CommandStreamReceiver> Device::createCommandStreamReceiver() const {
    return std::unique_ptr<CommandStreamReceiver>(createCommandStream(*executionEnvironment, getRootDeviceIndex(), getDeviceBitfield()));
}

bool Device::createEngine(uint32_t deviceCsrIndex, EngineTypeUsage engineTypeUsage) {
    const auto &hwInfo = getHardwareInfo();
    auto &gfxCoreHelper = getGfxCoreHelper();
    const auto engineType = engineTypeUsage.first;
    const auto engineUsage = engineTypeUsage.second;
    const auto defaultEngineType = engineInstanced ? this->engineInstancedType : getChosenEngineType(hwInfo);
    const bool isDefaultEngine = defaultEngineType == engineType && engineUsage == EngineUsage::regular;
    const bool createAsEngineInstanced = engineInstanced && EngineHelpers::isCcs(engineType);

    const bool isPrimaryEngine = EngineHelpers::isCcs(engineType) && engineUsage == EngineUsage::regular;
    const bool useContextGroup = isPrimaryEngine && gfxCoreHelper.areSecondaryContextsSupported();

    UNRECOVERABLE_IF(EngineHelpers::isBcs(engineType) && !hwInfo.capabilityTable.blitterOperationsSupported);

    std::unique_ptr<CommandStreamReceiver> commandStreamReceiver = createCommandStreamReceiver();
    if (!commandStreamReceiver) {
        return false;
    }

    bool internalUsage = (engineUsage == EngineUsage::internal);
    if (internalUsage) {
        commandStreamReceiver->initializeDefaultsForInternalEngine();
    }

    if (commandStreamReceiver->needsPageTableManager()) {
        commandStreamReceiver->createPageTableManager();
    }

    EngineDescriptor engineDescriptor(engineTypeUsage, getDeviceBitfield(), preemptionMode, false, createAsEngineInstanced);

    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(commandStreamReceiver.get(), engineDescriptor);
    osContext->setContextGroup(useContextGroup);

    commandStreamReceiver->setupContext(*osContext);

    if (osContext->isImmediateContextInitializationEnabled(isDefaultEngine)) {
        if (!commandStreamReceiver->initializeResources()) {
            return false;
        }
    }

    if (!commandStreamReceiver->initializeTagAllocation()) {
        return false;
    }

    if (!commandStreamReceiver->createGlobalFenceAllocation()) {
        return false;
    }

    if (preemptionMode == PreemptionMode::MidThread && !commandStreamReceiver->createPreemptionAllocation()) {
        return false;
    }

    bool firstSubmissionDone = false;
    if (isDefaultEngine) {
        bool defaultEngineAlreadySet = (allEngines.size() > defaultEngineIndex) && (allEngines[defaultEngineIndex].getEngineType() == engineType);

        if (!defaultEngineAlreadySet) {
            defaultEngineIndex = deviceCsrIndex;

            if (osContext->isDebuggableContext() ||
                this->isInitDeviceWithFirstSubmissionSupported(commandStreamReceiver->getType())) {
                if (SubmissionStatus::success != commandStreamReceiver->initializeDeviceWithFirstSubmission(*this)) {
                    return false;
                }
                firstSubmissionDone = true;
            }
        }
    }

    auto &compilerProductHelper = this->getCompilerProductHelper();
    bool isHeaplessStateInit = isPrimaryEngine && compilerProductHelper.isHeaplessStateInitEnabled();
    bool initializeDevice = (useContextGroup || isHeaplessStateInit) && !firstSubmissionDone;

    if (initializeDevice) {
        commandStreamReceiver->initializeDeviceWithFirstSubmission(*this);
    }

    if (EngineHelpers::isBcs(engineType) && (defaultBcsEngineIndex == std::numeric_limits<uint32_t>::max()) && (engineUsage == EngineUsage::regular)) {
        defaultBcsEngineIndex = deviceCsrIndex;
    }

    EngineControl engine{commandStreamReceiver.get(), osContext};
    allEngines.push_back(engine);
    if (engineUsage == EngineUsage::regular) {
        addEngineToEngineGroup(engine);
    }

    commandStreamReceivers.push_back(std::move(commandStreamReceiver));

    return true;
}

bool Device::createSecondaryEngine(CommandStreamReceiver *primaryCsr, uint32_t index, EngineTypeUsage engineTypeUsage) {
    auto engineUsage = engineTypeUsage.second;
    std::unique_ptr<CommandStreamReceiver> commandStreamReceiver = createCommandStreamReceiver();
    if (!commandStreamReceiver) {
        return false;
    }

    bool internalUsage = (engineUsage == EngineUsage::internal);
    if (internalUsage) {
        commandStreamReceiver->initializeDefaultsForInternalEngine();
    }

    EngineDescriptor engineDescriptor(engineTypeUsage, getDeviceBitfield(), preemptionMode, false, false);

    auto osContext = executionEnvironment->memoryManager->createAndRegisterSecondaryOsContext(&primaryCsr->getOsContext(), commandStreamReceiver.get(), engineDescriptor);
    commandStreamReceiver->setupContext(*osContext);

    EngineControl engine{commandStreamReceiver.get(), osContext};
    secondaryEngines[index].engines.push_back(engine);

    commandStreamReceivers.push_back(std::move(commandStreamReceiver));

    return true;
}

EngineControl *Device::getSecondaryEngineCsr(uint32_t engineIndex, EngineTypeUsage engineTypeUsage) {

    if (secondaryEngines.size() == 0 || !EngineHelpers::isCcs(engineTypeUsage.first) || engineIndex >= secondaryEngines.size()) {
        return nullptr;
    }

    auto secondaryEngineIndex = 0;
    if (engineTypeUsage.second == EngineUsage::highPriority) {
        secondaryEngineIndex = (secondaryEngines[engineIndex].highPriorityCounter.fetch_add(1)) % (secondaryEngines[engineIndex].highPriorityEnginesTotal);
        secondaryEngineIndex += secondaryEngines[engineIndex].regularEnginesTotal;
    } else if (engineTypeUsage.second == EngineUsage::regular) {
        secondaryEngineIndex = (secondaryEngines[engineIndex].regularCounter.fetch_add(1)) % (secondaryEngines[engineIndex].regularEnginesTotal);
    } else {
        DEBUG_BREAK_IF(true);
    }

    if (secondaryEngineIndex > 0) {
        auto commandStreamReceiver = secondaryEngines[engineIndex].engines[secondaryEngineIndex].commandStreamReceiver;

        auto lock = commandStreamReceiver->obtainUniqueOwnership();

        if (!commandStreamReceiver->isInitialized()) {

            if (commandStreamReceiver->needsPageTableManager()) {
                commandStreamReceiver->createPageTableManager();
            }

            EngineDescriptor engineDescriptor(engineTypeUsage, getDeviceBitfield(), preemptionMode, false, false);

            if (!commandStreamReceiver->initializeResources()) {
                return nullptr;
            }

            if (!commandStreamReceiver->initializeTagAllocation()) {
                return nullptr;
            }

            if (!commandStreamReceiver->createGlobalFenceAllocation()) {
                return nullptr;
            }

            if (preemptionMode == PreemptionMode::MidThread && !commandStreamReceiver->createPreemptionAllocation()) {
                return nullptr;
            }
        }
    }
    return &secondaryEngines[engineIndex].engines[secondaryEngineIndex];
}

const HardwareInfo &Device::getHardwareInfo() const { return *getRootDeviceEnvironment().getHardwareInfo(); }

const DeviceInfo &Device::getDeviceInfo() const {
    return deviceInfo;
}

double Device::getProfilingTimerResolution() {
    return getOSTime()->getDynamicDeviceTimerResolution(getHardwareInfo());
}

uint64_t Device::getProfilingTimerClock() {
    return getOSTime()->getDynamicDeviceTimerClock(getHardwareInfo());
}

bool Device::isBcsSplitSupported() {
    auto &productHelper = getProductHelper();
    auto bcsSplit = productHelper.isBlitSplitEnqueueWARequired(getHardwareInfo()) &&
                    Device::isBlitSplitEnabled();

    if (debugManager.flags.SplitBcsCopy.get() != -1) {
        bcsSplit = debugManager.flags.SplitBcsCopy.get();
    }

    return bcsSplit;
}

bool Device::isInitDeviceWithFirstSubmissionSupported(CommandStreamReceiverType csrType) {
    return !this->executionEnvironment->areMetricsEnabled() &&
           getProductHelper().isInitDeviceWithFirstSubmissionRequired(getHardwareInfo()) &&
           Device::isInitDeviceWithFirstSubmissionEnabled(csrType);
}

double Device::getPlatformHostTimerResolution() const {
    if (getOSTime()) {
        return getOSTime()->getHostTimerResolution();
    }

    return 0.0;
}

GFXCORE_FAMILY Device::getRenderCoreFamily() const {
    return this->getHardwareInfo().platform.eRenderCoreFamily;
}

Debugger *Device::getDebugger() const {
    return getRootDeviceEnvironment().debugger.get();
}

bool Device::areSharedSystemAllocationsAllowed() const {
    auto sharedSystemAllocationsSupport = static_cast<bool>(getHardwareInfo().capabilityTable.sharedSystemMemCapabilities);
    if (debugManager.flags.EnableSharedSystemUsmSupport.get() != -1) {
        sharedSystemAllocationsSupport = debugManager.flags.EnableSharedSystemUsmSupport.get();
    }
    return sharedSystemAllocationsSupport;
}

size_t Device::getEngineGroupIndexFromEngineGroupType(EngineGroupType engineGroupType) const {
    for (size_t i = 0; i < regularEngineGroups.size(); i++) {
        if (regularEngineGroups[i].engineGroupType == engineGroupType) {
            return i;
        }
    }
    UNRECOVERABLE_IF(true);
    return 0;
}

EngineControl *Device::tryGetEngine(aub_stream::EngineType engineType, EngineUsage engineUsage) {
    for (auto &engine : allEngines) {
        if ((engine.getEngineType() == engineType) &&
            (engine.getEngineUsage() == engineUsage)) {
            return &engine;
        }
    }

    if (debugManager.flags.OverrideInvalidEngineWithDefault.get()) {
        return &allEngines[0];
    }
    return nullptr;
}

EngineControl &Device::getEngine(aub_stream::EngineType engineType, EngineUsage engineUsage) {
    auto engine = tryGetEngine(engineType, engineUsage);
    UNRECOVERABLE_IF(!engine);
    return *engine;
}

EngineControl &Device::getEngine(uint32_t index) {
    UNRECOVERABLE_IF(index >= allEngines.size());
    return allEngines[index];
}

bool Device::getDeviceAndHostTimer(uint64_t *deviceTimestamp, uint64_t *hostTimestamp) const {
    TimeStampData timeStamp;
    auto retVal = getOSTime()->getGpuCpuTime(&timeStamp);
    if (retVal) {
        *hostTimestamp = timeStamp.cpuTimeinNS;
        if (debugManager.flags.EnableDeviceBasedTimestamps.get()) {
            auto resolution = getOSTime()->getDynamicDeviceTimerResolution(getHardwareInfo());
            *deviceTimestamp = getGfxCoreHelper().getGpuTimeStampInNS(timeStamp.gpuTimeStamp, resolution);
        } else
            *deviceTimestamp = *hostTimestamp;
    }
    return retVal;
}

bool Device::getHostTimer(uint64_t *hostTimestamp) const {
    return getOSTime()->getCpuTime(hostTimestamp);
}

uint32_t Device::getNumGenericSubDevices() const {
    return (hasRootCsr() ? getNumSubDevices() : 0);
}

Device *Device::getSubDevice(uint32_t deviceId) const {
    UNRECOVERABLE_IF(deviceId >= subdevices.size());
    return subdevices[deviceId];
}

Device *Device::getNearestGenericSubDevice(uint32_t deviceId) {
    /*
     * EngineInstanced: Upper level
     * Generic SubDevice: 'this'
     * RootCsr Device: Next level SubDevice (generic)
     */

    if (engineInstanced) {
        return getRootDevice()->getNearestGenericSubDevice(Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong())));
    }

    if (subdevices.empty() || !hasRootCsr()) {
        return this;
    }
    UNRECOVERABLE_IF(deviceId >= subdevices.size());
    return subdevices[deviceId];
}

BindlessHeapsHelper *Device::getBindlessHeapsHelper() const {
    return getRootDeviceEnvironment().getBindlessHeapsHelper();
}

GmmClientContext *Device::getGmmClientContext() const {
    return getGmmHelper()->getClientContext();
}

void Device::allocateSyncBufferHandler() {
    static std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    if (syncBufferHandler.get() == nullptr) {
        syncBufferHandler = std::make_unique<SyncBufferHandler>(*this);
        UNRECOVERABLE_IF(syncBufferHandler.get() == nullptr);
    }
}

uint64_t Device::getGlobalMemorySize(uint32_t deviceBitfield) const {
    auto globalMemorySize = getMemoryManager()->isLocalMemorySupported(this->getRootDeviceIndex())
                                ? getMemoryManager()->getLocalMemorySize(this->getRootDeviceIndex(), deviceBitfield)
                                : getMemoryManager()->getSystemSharedMemory(this->getRootDeviceIndex());
    globalMemorySize = std::min(globalMemorySize, getMemoryManager()->getMaxApplicationAddress() + 1);
    double percentOfGlobalMemoryAvailable = getPercentOfGlobalMemoryAvailable();
    globalMemorySize = static_cast<uint64_t>(static_cast<double>(globalMemorySize) * percentOfGlobalMemoryAvailable);

    if (debugManager.flags.ClDeviceGlobalMemSizeAvailablePercent.get() == -1 &&
        !getMemoryManager()->isLocalMemorySupported(this->getRootDeviceIndex())) {
        const uint64_t internalResourcesSize = 450 * MemoryConstants::megaByte;
        globalMemorySize = std::max(static_cast<uint64_t>(0), globalMemorySize - internalResourcesSize);
    }

    return globalMemorySize;
}

double Device::getPercentOfGlobalMemoryAvailable() const {
    if (debugManager.flags.ClDeviceGlobalMemSizeAvailablePercent.get() != -1) {
        return 0.01 * static_cast<double>(debugManager.flags.ClDeviceGlobalMemSizeAvailablePercent.get());
    }
    return getMemoryManager()->getPercentOfGlobalMemoryAvailable(this->getRootDeviceIndex());
}

NEO::DebuggerL0 *Device::getL0Debugger() {
    auto debugger = getDebugger();
    return debugger ? static_cast<NEO::DebuggerL0 *>(debugger) : nullptr;
}

const std::vector<EngineControl> &Device::getAllEngines() const {
    return this->allEngines;
}

const RootDeviceEnvironment &Device::getRootDeviceEnvironment() const {
    return *executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()];
}

RootDeviceEnvironment &Device::getRootDeviceEnvironmentRef() const {
    return *executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()];
}

bool Device::isFullRangeSvm() const {
    return getRootDeviceEnvironment().isFullRangeSvm();
}

EngineControl &Device::getInternalEngine() {
    if (this->allEngines[0].commandStreamReceiver->getType() != CommandStreamReceiverType::CSR_HW) {
        return this->getDefaultEngine();
    }

    auto engineType = getChosenEngineType(getHardwareInfo());

    return this->getNearestGenericSubDevice(0)->getEngine(engineType, EngineUsage::internal);
}

EngineControl &Device::getNextEngineForCommandQueue() {
    this->initializeEngineRoundRobinControls();

    const auto &defaultEngine = this->getDefaultEngine();

    const auto &hardwareInfo = this->getHardwareInfo();
    const auto &gfxCoreHelper = getGfxCoreHelper();
    const auto engineGroupType = gfxCoreHelper.getEngineGroupType(defaultEngine.getEngineType(), defaultEngine.getEngineUsage(), hardwareInfo);

    const auto defaultEngineGroupIndex = this->getEngineGroupIndexFromEngineGroupType(engineGroupType);
    auto &engineGroup = this->getRegularEngineGroups()[defaultEngineGroupIndex];

    auto engineIndex = 0u;
    do {
        engineIndex = (this->regularCommandQueuesCreatedWithinDeviceCount++ / this->queuesPerEngineCount) % engineGroup.engines.size();
    } while (!this->availableEnginesForCommandQueueusRoundRobin.test(engineIndex));
    return engineGroup.engines[engineIndex];
}

EngineControl *Device::getInternalCopyEngine() {
    if (!getHardwareInfo().capabilityTable.blitterOperationsSupported) {
        return nullptr;
    }

    const auto &productHelper = this->getProductHelper();
    auto expectedEngine = productHelper.getDefaultCopyEngine();

    if (debugManager.flags.ForceBCSForInternalCopyEngine.get() != -1) {
        expectedEngine = EngineHelpers::mapBcsIndexToEngineType(debugManager.flags.ForceBCSForInternalCopyEngine.get(), true);
    }

    for (auto &engine : allEngines) {
        if (engine.osContext->getEngineType() == expectedEngine &&
            engine.osContext->isInternalEngine()) {
            return &engine;
        }
    }
    return nullptr;
}

RTDispatchGlobalsInfo *Device::getRTDispatchGlobals(uint32_t maxBvhLevels) {
    if (rtDispatchGlobalsInfos.size() == 0) {
        return nullptr;
    }

    size_t last = rtDispatchGlobalsInfos.size() - 1;
    if (maxBvhLevels > last) {
        return nullptr;
    }

    for (size_t i = last; i >= maxBvhLevels; i--) {
        if (rtDispatchGlobalsInfos[i] != nullptr) {
            return rtDispatchGlobalsInfos[i];
        }

        if (i == 0) {
            break;
        }
    }

    allocateRTDispatchGlobals(maxBvhLevels);
    return rtDispatchGlobalsInfos[maxBvhLevels];
}

void Device::initializeRayTracing(uint32_t maxBvhLevels) {
    if (rtMemoryBackedBuffer == nullptr) {
        auto size = RayTracingHelper::getTotalMemoryBackedFifoSize(*this);

        AllocationProperties allocProps(getRootDeviceIndex(), true, size, AllocationType::buffer, true, getDeviceBitfield());
        auto &productHelper = getProductHelper();
        allocProps.flags.resource48Bit = productHelper.is48bResourceNeededForRayTracing();
        allocProps.flags.isUSMDeviceAllocation = true;

        rtMemoryBackedBuffer = getMemoryManager()->allocateGraphicsMemoryWithProperties(allocProps);
    }

    while (rtDispatchGlobalsInfos.size() <= maxBvhLevels) {
        rtDispatchGlobalsInfos.push_back(nullptr);
    }
}

void Device::finalizeRayTracing() {
    getMemoryManager()->freeGraphicsMemory(rtMemoryBackedBuffer);
    rtMemoryBackedBuffer = nullptr;

    for (size_t i = 0; i < rtDispatchGlobalsInfos.size(); i++) {
        auto rtDispatchGlobalsInfo = rtDispatchGlobalsInfos[i];
        if (rtDispatchGlobalsInfo == nullptr) {
            continue;
        }
        for (size_t j = 0; j < rtDispatchGlobalsInfo->rtStacks.size(); j++) {
            getMemoryManager()->freeGraphicsMemory(rtDispatchGlobalsInfo->rtStacks[j]);
            rtDispatchGlobalsInfo->rtStacks[j] = nullptr;
        }

        getMemoryManager()->freeGraphicsMemory(rtDispatchGlobalsInfo->rtDispatchGlobalsArray);
        rtDispatchGlobalsInfo->rtDispatchGlobalsArray = nullptr;

        delete rtDispatchGlobalsInfos[i];
        rtDispatchGlobalsInfos[i] = nullptr;
    }
}

void Device::initializeEngineRoundRobinControls() {
    if (this->availableEnginesForCommandQueueusRoundRobin.any()) {
        return;
    }

    uint32_t queuesPerEngine = 1u;

    if (debugManager.flags.CmdQRoundRobindEngineAssignNTo1.get() != -1) {
        queuesPerEngine = debugManager.flags.CmdQRoundRobindEngineAssignNTo1.get();
    }

    this->queuesPerEngineCount = queuesPerEngine;

    std::bitset<8> availableEngines = std::numeric_limits<uint8_t>::max();

    if (debugManager.flags.CmdQRoundRobindEngineAssignBitfield.get() != -1) {
        availableEngines = debugManager.flags.CmdQRoundRobindEngineAssignBitfield.get();
    }

    this->availableEnginesForCommandQueueusRoundRobin = availableEngines;
}

OSTime *Device::getOSTime() const { return getRootDeviceEnvironment().osTime.get(); };

bool Device::getUuid(std::array<uint8_t, ProductHelper::uuidSize> &uuid) {
    if (this->uuid.isValid) {
        uuid = this->uuid.id;

        auto hwInfo = getHardwareInfo();
        auto subDevicesCount = GfxCoreHelper::getSubDevicesCount(&hwInfo);

        if (subDevicesCount > 1 && deviceBitfield.count() == 1) {
            // In case of no sub devices created (bits set in affinity mask == 1), return the UUID of enabled sub-device.
            uint32_t subDeviceIndex = Math::log2(static_cast<uint32_t>(deviceBitfield.to_ulong()));
            uuid[ProductHelper::uuidSize - 1] = subDeviceIndex + 1;
        }
    }
    return this->uuid.isValid;
}

bool Device::generateUuidFromPciBusInfo(const PhysicalDevicePciBusInfo &pciBusInfo, std::array<uint8_t, ProductHelper::uuidSize> &uuid) {
    if (pciBusInfo.pciDomain != PhysicalDevicePciBusInfo::invalidValue) {
        generateUuid(uuid);

        /* Device UUID uniquely identifies a device within a system.
         * We generate it based on device information along with PCI information
         * This guarantees uniqueness of UUIDs on a system even when multiple
         * identical Intel GPUs are present.
         */

        /* We want to have UUID matching between different GPU APIs (including outside
         * of compute_runtime project - i.e. other than L0 or OCL). This structure definition
         * has been agreed upon by various Intel driver teams.
         *
         * Consult other driver teams before changing this.
         */

        struct DeviceUUID {
            uint16_t vendorID;
            uint16_t deviceID;
            uint16_t revisionID;
            uint16_t pciDomain;
            uint8_t pciBus;
            uint8_t pciDev;
            uint8_t pciFunc;
            uint8_t reserved[4];
            uint8_t subDeviceID;
        };
        static_assert(sizeof(DeviceUUID) == ProductHelper::uuidSize);

        DeviceUUID deviceUUID{};
        memcpy_s(&deviceUUID, sizeof(DeviceUUID), uuid.data(), uuid.size());

        deviceUUID.pciDomain = static_cast<uint16_t>(pciBusInfo.pciDomain);
        deviceUUID.pciBus = static_cast<uint8_t>(pciBusInfo.pciBus);
        deviceUUID.pciDev = static_cast<uint8_t>(pciBusInfo.pciDevice);
        deviceUUID.pciFunc = static_cast<uint8_t>(pciBusInfo.pciFunction);

        memcpy_s(uuid.data(), uuid.size(), &deviceUUID, sizeof(DeviceUUID));

        return true;
    }
    return false;
}

void Device::generateUuid(std::array<uint8_t, ProductHelper::uuidSize> &uuid) {
    const auto &deviceInfo = getDeviceInfo();
    const auto &hardwareInfo = getHardwareInfo();
    uint32_t rootDeviceIndex = getRootDeviceIndex();
    uint16_t vendorId = static_cast<uint16_t>(deviceInfo.vendorId);
    uint16_t deviceId = static_cast<uint16_t>(hardwareInfo.platform.usDeviceID);
    uint16_t revisionId = static_cast<uint16_t>(hardwareInfo.platform.usRevId);
    uint8_t subDeviceId = isSubDevice() ? static_cast<SubDevice *>(this)->getSubDeviceIndex() + 1 : 0;
    uuid.fill(0);
    memcpy_s(&uuid[0], sizeof(uint32_t), &vendorId, sizeof(vendorId));
    memcpy_s(&uuid[2], sizeof(uint32_t), &deviceId, sizeof(deviceId));
    memcpy_s(&uuid[4], sizeof(uint32_t), &revisionId, sizeof(revisionId));
    memcpy_s(&uuid[6], sizeof(uint32_t), &rootDeviceIndex, sizeof(rootDeviceIndex));
    uuid[15] = subDeviceId;
}

void Device::getAdapterMask(uint32_t &nodeMask) {
    if (verifyAdapterLuid()) {
        nodeMask = 1;
    }
}

const GfxCoreHelper &Device::getGfxCoreHelper() const {
    return getRootDeviceEnvironment().getHelper<GfxCoreHelper>();
}

const ProductHelper &Device::getProductHelper() const {
    return getRootDeviceEnvironment().getHelper<ProductHelper>();
}

const CompilerProductHelper &Device::getCompilerProductHelper() const {
    return getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
}

ReleaseHelper *Device::getReleaseHelper() const {
    return getRootDeviceEnvironment().getReleaseHelper();
}

void Device::stopDirectSubmissionAndWaitForCompletion() {
    for (auto &engine : allEngines) {
        auto csr = engine.commandStreamReceiver;
        if (csr->isAnyDirectSubmissionEnabled()) {
            auto lock = csr->obtainUniqueOwnership();
            csr->stopDirectSubmission(true);
        }
    }
}

bool Device::isAnyDirectSubmissionEnabled() {
    bool enabled = false;
    for (auto &engine : allEngines) {
        auto csr = engine.commandStreamReceiver;
        enabled |= csr->isAnyDirectSubmissionEnabled();
    }
    return enabled;
}

void Device::allocateRTDispatchGlobals(uint32_t maxBvhLevels) {
    UNRECOVERABLE_IF(rtDispatchGlobalsInfos.size() < maxBvhLevels + 1);
    UNRECOVERABLE_IF(rtDispatchGlobalsInfos[maxBvhLevels] != nullptr);

    uint32_t extraBytesLocal = 0;
    uint32_t extraBytesGlobal = 0;
    uint32_t dispatchGlobalsStride = MemoryConstants::pageSize64k;
    UNRECOVERABLE_IF(RayTracingHelper::getDispatchGlobalSize() > dispatchGlobalsStride);

    bool allocFailed = false;

    uint32_t tileCount = 1;
    if (this->getNumSubDevices() > 1) {
        // If device encompasses multiple tiles, allocate RTDispatchGlobals for each tile
        tileCount = this->getNumSubDevices();
    }

    auto dispatchGlobalsSize = tileCount * dispatchGlobalsStride;
    auto rtStackSize = RayTracingHelper::getRTStackSizePerTile(*this, tileCount, maxBvhLevels, extraBytesLocal, extraBytesGlobal);

    std::unique_ptr<RTDispatchGlobalsInfo> dispatchGlobalsInfo = std::make_unique<RTDispatchGlobalsInfo>();

    auto &productHelper = getProductHelper();

    GraphicsAllocation *dispatchGlobalsArrayAllocation = nullptr;

    AllocationProperties arrayAllocProps(getRootDeviceIndex(), true, dispatchGlobalsSize,
                                         AllocationType::globalSurface, true, getDeviceBitfield());
    arrayAllocProps.flags.resource48Bit = productHelper.is48bResourceNeededForRayTracing();
    arrayAllocProps.flags.isUSMDeviceAllocation = true;
    dispatchGlobalsArrayAllocation = getMemoryManager()->allocateGraphicsMemoryWithProperties(arrayAllocProps);

    if (dispatchGlobalsArrayAllocation == nullptr) {
        return;
    }

    for (unsigned int tile = 0; tile < tileCount; tile++) {
        DeviceBitfield deviceBitfield =
            (tileCount == 1)
                ? this->getDeviceBitfield()
                : subdevices[tile]->getDeviceBitfield();

        AllocationProperties allocProps(getRootDeviceIndex(), true, rtStackSize, AllocationType::buffer, true, deviceBitfield);
        allocProps.flags.resource48Bit = productHelper.is48bResourceNeededForRayTracing();
        allocProps.flags.isUSMDeviceAllocation = true;

        auto rtStackAllocation = getMemoryManager()->allocateGraphicsMemoryWithProperties(allocProps);

        if (rtStackAllocation == nullptr) {
            allocFailed = true;
            break;
        }

        struct RTDispatchGlobals dispatchGlobals = {0};

        dispatchGlobals.rtMemBasePtr = rtStackAllocation->getGpuAddress() + rtStackSize;
        dispatchGlobals.callStackHandlerKSP = reinterpret_cast<uint64_t>(nullptr);
        dispatchGlobals.stackSizePerRay = 0;
        dispatchGlobals.numDSSRTStacks = RayTracingHelper::stackDssMultiplier;
        dispatchGlobals.maxBVHLevels = maxBvhLevels;

        uint32_t *dispatchGlobalsAsArray = reinterpret_cast<uint32_t *>(&dispatchGlobals);
        dispatchGlobalsAsArray[7] = 1;

        MemoryTransferHelper::transferMemoryToAllocation(productHelper.isBlitCopyRequiredForLocalMemory(this->getRootDeviceEnvironment(), *dispatchGlobalsArrayAllocation),
                                                         *this,
                                                         dispatchGlobalsArrayAllocation,
                                                         tile * dispatchGlobalsStride,
                                                         &dispatchGlobals,
                                                         sizeof(RTDispatchGlobals));

        dispatchGlobalsInfo->rtStacks.push_back(rtStackAllocation);
    }

    if (allocFailed) {
        for (auto allocation : dispatchGlobalsInfo->rtStacks) {
            getMemoryManager()->freeGraphicsMemory(allocation);
        }

        getMemoryManager()->freeGraphicsMemory(dispatchGlobalsArrayAllocation);
        return;
    }

    dispatchGlobalsInfo->rtDispatchGlobalsArray = dispatchGlobalsArrayAllocation;
    rtDispatchGlobalsInfos[maxBvhLevels] = dispatchGlobalsInfo.release();
}

MemoryManager *Device::getMemoryManager() const {
    return executionEnvironment->memoryManager.get();
}

GmmHelper *Device::getGmmHelper() const {
    return getRootDeviceEnvironment().getGmmHelper();
}

CompilerInterface *Device::getCompilerInterface() const {
    return executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->getCompilerInterface();
}

BuiltIns *Device::getBuiltIns() const {
    return executionEnvironment->rootDeviceEnvironments[getRootDeviceIndex()]->getBuiltIns();
}

EngineControl &Device::getNextEngineForMultiRegularContextMode(aub_stream::EngineType engineType) {
    UNRECOVERABLE_IF(defaultEngineIndex != 0);
    UNRECOVERABLE_IF((engineType != aub_stream::EngineType::ENGINE_BCS) && (engineType != aub_stream::EngineType::ENGINE_CCS));

    const auto maxIndex = numberOfRegularContextsPerEngine - 1; // 1 for internal engine
    uint32_t atomicOutValue = 0;
    uint32_t indexOffset = 0;

    if (engineType == aub_stream::EngineType::ENGINE_CCS) {
        atomicOutValue = regularContextPerCcsEngineAssignmentHelper.fetch_add(1);
        indexOffset = defaultEngineIndex;
    } else {
        atomicOutValue = regularContextPerBcsEngineAssignmentHelper.fetch_add(1);
        indexOffset = defaultBcsEngineIndex;
    }

    auto indexToAssign = (atomicOutValue % maxIndex) + indexOffset;

    return allEngines[indexToAssign];
}

bool Device::isMultiRegularContextSelectionAllowed(aub_stream::EngineType engineType, EngineUsage engineUsage) const {
    if (this->numberOfRegularContextsPerEngine <= 1 || getNumGenericSubDevices() > 1 || engineUsage != EngineUsage::regular) {
        return false;
    }

    if (engineType == aub_stream::EngineType::ENGINE_BCS && debugManager.flags.EnableMultipleRegularContextForBcs.get() == 1) {
        return true;
    }

    return EngineHelpers::isCcs(engineType);
}
} // namespace NEO
