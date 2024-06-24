/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/cmdlist/cmdlist_imp.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/device/device.h"
#include "shared/source/helpers/engine_control.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/indirect_heap/indirect_heap.h"
#include "shared/source/memory_manager/allocation_properties.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/os_interface/os_time.h"
#include "shared/source/os_interface/sys_calls_common.h"

#include "level_zero/core/source/cmdqueue/cmdqueue.h"
#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/gfx_core_helpers/l0_gfx_core_helper.h"
#include "level_zero/tools/source/metrics/metric.h"

#include "igfxfmid.h"

#include <algorithm>

namespace L0 {

CommandList::CommandList(uint32_t numIddsPerBlock) : commandContainer(numIddsPerBlock) {
    if (NEO::debugManager.flags.SplitBcsSize.get() != -1) {
        this->minimalSizeForBcsSplit = NEO::debugManager.flags.SplitBcsSize.get() * MemoryConstants::kiloByte;
    }
}

CommandListAllocatorFn commandListFactory[IGFX_MAX_PRODUCT] = {};
CommandListAllocatorFn commandListFactoryImmediate[IGFX_MAX_PRODUCT] = {};

ze_result_t CommandListImp::destroy() {
    if (this->isBcsSplitNeeded) {
        static_cast<DeviceImp *>(this->device)->bcsSplit.releaseResources();
    }

    if (isImmediateType() && this->isFlushTaskSubmissionEnabled && !this->isSyncModeQueue) {
        auto timeoutMicroseconds = NEO::TimeoutControls::maxTimeout;
        this->csr->waitForCompletionWithTimeout(NEO::WaitParams{false, false, timeoutMicroseconds}, this->csr->peekTaskCount());
    }

    if (!isImmediateType() &&
        !isCopyOnly() &&
        this->stateBaseAddressTracking &&
        this->cmdListHeapAddressModel == NEO::HeapAddressModel::privateHeaps) {

        auto surfaceStateHeap = this->commandContainer.getIndirectHeap(NEO::HeapType::surfaceState);
        if (surfaceStateHeap) {
            auto heapAllocation = surfaceStateHeap->getGraphicsAllocation();

            auto rootDeviceIndex = device->getRootDeviceIndex();
            auto &deviceEngines = device->getNEODevice()->getMemoryManager()->getRegisteredEngines(rootDeviceIndex);
            for (auto &engine : deviceEngines) {
                if (NEO::EngineHelpers::isComputeEngine(engine.getEngineType())) {
                    auto contextId = engine.osContext->getContextId();
                    if (heapAllocation->isUsedByOsContext(contextId) && engine.osContext->isInitialized() && heapAllocation->getTaskCount(contextId) > 0) {
                        engine.commandStreamReceiver->sendRenderStateCacheFlush();
                    }
                }
            }
        }
    }

    delete this;
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandListImp::appendMetricMemoryBarrier() {

    return device->getMetricDeviceContext().appendMetricMemoryBarrier(*this);
}

ze_result_t CommandListImp::appendMetricStreamerMarker(zet_metric_streamer_handle_t hMetricStreamer,
                                                       uint32_t value) {
    return MetricStreamer::fromHandle(hMetricStreamer)->appendStreamerMarker(*this, value);
}

ze_result_t CommandListImp::appendMetricQueryBegin(zet_metric_query_handle_t hMetricQuery) {
    if (isImmediateType() && isFlushTaskSubmissionEnabled) {
        this->device->activateMetricGroups();
    }

    return MetricQuery::fromHandle(hMetricQuery)->appendBegin(*this);
}

ze_result_t CommandListImp::appendMetricQueryEnd(zet_metric_query_handle_t hMetricQuery, ze_event_handle_t hSignalEvent,
                                                 uint32_t numWaitEvents, ze_event_handle_t *phWaitEvents) {
    return MetricQuery::fromHandle(hMetricQuery)->appendEnd(*this, hSignalEvent, numWaitEvents, phWaitEvents);
}

CommandList *CommandList::create(uint32_t productFamily, Device *device, NEO::EngineGroupType engineGroupType,
                                 ze_command_list_flags_t flags, ze_result_t &returnValue,
                                 bool internalUsage) {
    CommandListAllocatorFn allocator = nullptr;
    if (productFamily < IGFX_MAX_PRODUCT) {
        allocator = commandListFactory[productFamily];
    }

    CommandListImp *commandList = nullptr;
    returnValue = ZE_RESULT_ERROR_UNINITIALIZED;

    if (allocator) {
        commandList = static_cast<CommandListImp *>((*allocator)(CommandList::defaultNumIddsPerBlock));
        commandList->internalUsage = internalUsage;
        returnValue = commandList->initialize(device, engineGroupType, flags);
        if (returnValue != ZE_RESULT_SUCCESS) {
            commandList->destroy();
            commandList = nullptr;
        }
    }

    return commandList;
}

ze_result_t CommandListImp::getDeviceHandle(ze_device_handle_t *phDevice) {
    *phDevice = getDevice()->toHandle();
    return ZE_RESULT_SUCCESS;
}
ze_result_t CommandListImp::getContextHandle(ze_context_handle_t *phContext) {
    *phContext = getCmdListContext();
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandListImp::getOrdinal(uint32_t *pOrdinal) {
    UNRECOVERABLE_IF(ordinal == std::nullopt);
    *pOrdinal = ordinal.value();
    return ZE_RESULT_SUCCESS;
}

ze_result_t CommandListImp::getImmediateIndex(uint32_t *pIndex) {
    if (isImmediateType()) {
        return cmdQImmediate->getIndex(pIndex);
    }
    return ZE_RESULT_ERROR_INVALID_ARGUMENT;
}

ze_result_t CommandListImp::isImmediate(ze_bool_t *pIsImmediate) {
    *pIsImmediate = isImmediateType();
    return ZE_RESULT_SUCCESS;
}

CommandList *CommandList::createImmediate(uint32_t productFamily, Device *device,
                                          const ze_command_queue_desc_t *desc,
                                          bool internalUsage, NEO::EngineGroupType engineGroupType,
                                          ze_result_t &returnValue) {

    ze_command_queue_desc_t cmdQdesc = *desc;

    int32_t overrideImmediateCmdListSyncMode = NEO::debugManager.flags.OverrideImmediateCmdListSynchronousMode.get();
    if (overrideImmediateCmdListSyncMode != -1) {
        cmdQdesc.mode = static_cast<ze_command_queue_mode_t>(overrideImmediateCmdListSyncMode);
    }
    CommandListAllocatorFn allocator = nullptr;
    if (productFamily < IGFX_MAX_PRODUCT) {
        allocator = commandListFactoryImmediate[productFamily];
    }

    CommandListImp *commandList = nullptr;
    returnValue = ZE_RESULT_ERROR_UNINITIALIZED;

    if (allocator) {
        NEO::CommandStreamReceiver *csr = nullptr;
        auto deviceImp = static_cast<DeviceImp *>(device);
        const auto &hwInfo = device->getHwInfo();
        auto &gfxCoreHelper = device->getGfxCoreHelper();
        if (internalUsage) {
            if (NEO::EngineHelper::isCopyOnlyEngineType(engineGroupType) && deviceImp->getActiveDevice()->getInternalCopyEngine()) {
                csr = deviceImp->getActiveDevice()->getInternalCopyEngine()->commandStreamReceiver;
            } else {
                auto internalEngine = deviceImp->getActiveDevice()->getInternalEngine();
                csr = internalEngine.commandStreamReceiver;
                engineGroupType = deviceImp->getInternalEngineGroupType();
            }
        } else {
            returnValue = device->getCsrForOrdinalAndIndexWithPriority(&csr, cmdQdesc.ordinal, cmdQdesc.index, cmdQdesc.priority);
            if (returnValue != ZE_RESULT_SUCCESS) {
                return commandList;
            }
        }

        UNRECOVERABLE_IF(nullptr == csr);

        commandList = static_cast<CommandListImp *>((*allocator)(CommandList::commandListimmediateIddsPerBlock));
        commandList->csr = csr;
        commandList->internalUsage = internalUsage;
        commandList->cmdListType = CommandListType::typeImmediate;
        commandList->isSyncModeQueue = (cmdQdesc.mode == ZE_COMMAND_QUEUE_MODE_SYNCHRONOUS);

        if (!internalUsage) {
            auto &productHelper = device->getProductHelper();
            commandList->isFlushTaskSubmissionEnabled = gfxCoreHelper.isPlatformFlushTaskEnabled(productHelper);
            if (NEO::debugManager.flags.EnableFlushTaskSubmission.get() != -1) {
                commandList->isFlushTaskSubmissionEnabled = !!NEO::debugManager.flags.EnableFlushTaskSubmission.get();
            }
            PRINT_DEBUG_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Flush Task for Immediate command list : %s\n", commandList->isFlushTaskSubmissionEnabled ? "Enabled" : "Disabled");

            auto &rootDeviceEnvironment = device->getNEODevice()->getRootDeviceEnvironment();
            bool enabledCmdListSharing = !NEO::EngineHelper::isCopyOnlyEngineType(engineGroupType) && commandList->isFlushTaskSubmissionEnabled;
            commandList->immediateCmdListHeapSharing = L0GfxCoreHelper::enableImmediateCmdListHeapSharing(rootDeviceEnvironment, enabledCmdListSharing);
        }
        csr->initializeResources();
        csr->initDirectSubmission();
        returnValue = commandList->initialize(device, engineGroupType, 0);

        if ((cmdQdesc.flags & ZE_COMMAND_QUEUE_FLAG_IN_ORDER) || (NEO::debugManager.flags.ForceInOrderImmediateCmdListExecution.get() == 1)) {
            commandList->enableInOrderExecution();
        }

        if (returnValue != ZE_RESULT_SUCCESS) {
            commandList->destroy();
            commandList = nullptr;
            return commandList;
        }

        auto commandQueue = CommandQueue::create(productFamily, device, csr, &cmdQdesc, commandList->isCopyOnly(), internalUsage, true, returnValue);
        if (!commandQueue) {
            commandList->destroy();
            commandList = nullptr;
            return commandList;
        }

        commandList->cmdQImmediate = commandQueue;
        commandList->isTbxMode = csr->isTbxMode();
        commandList->commandListPreemptionMode = device->getDevicePreemptionMode();

        commandList->isBcsSplitNeeded = deviceImp->bcsSplit.setupDevice(productFamily, internalUsage, &cmdQdesc, csr);

        commandList->copyThroughLockedPtrEnabled = gfxCoreHelper.copyThroughLockedPtrEnabled(hwInfo, device->getProductHelper());

        return commandList;
    }

    return commandList;
}

void CommandListImp::setStreamPropertiesDefaultSettings(NEO::StreamProperties &streamProperties) {
    if (this->stateComputeModeTracking) {
        streamProperties.stateComputeMode.setPropertiesCoherencyDevicePreemption(cmdListDefaultCoherency, this->commandListPreemptionMode, true);
    }

    streamProperties.frontEndState.setPropertiesDisableOverdispatchEngineInstanced(cmdListDefaultDisableOverdispatch, cmdListDefaultEngineInstancedDevice, true);
    streamProperties.pipelineSelect.setPropertiesModeSelectedMediaSamplerClockGate(cmdListDefaultPipelineSelectModeSelected, cmdListDefaultMediaSamplerClockGate, true);
}

void CommandListImp::enableInOrderExecution() {
    UNRECOVERABLE_IF(inOrderExecInfo.get());

    auto deviceCounterNode = this->device->getDeviceInOrderCounterAllocator()->getTag();
    NEO::TagNodeBase *hostCounterNode = nullptr;

    auto &gfxCoreHelper = device->getGfxCoreHelper();

    if (gfxCoreHelper.duplicatedInOrderCounterStorageEnabled(device->getNEODevice()->getRootDeviceEnvironment())) {
        hostCounterNode = this->device->getHostInOrderCounterAllocator()->getTag();
    }

    inOrderExecInfo = NEO::InOrderExecInfo::create(deviceCounterNode, hostCounterNode, *this->device->getNEODevice(), this->partitionCount, !isImmediateType());
}

void CommandListImp::storeReferenceTsToMappedEvents(bool isClearEnabled) {
    if (mappedTsEventList.size()) {
        uint64_t currentCpuTimeStamp = 0;
        device->getNEODevice()->getOSTime()->getCpuTime(&currentCpuTimeStamp);
        for (auto &event : mappedTsEventList) {
            event->setReferenceTs(currentCpuTimeStamp);
        }

        if (isClearEnabled) {
            mappedTsEventList.clear();
        }
    }
}

void CommandListImp::addToMappedEventList(Event *event) {
    if (event && event->hasKerneMappedTsCapability) {
        if (std::find(mappedTsEventList.begin(), mappedTsEventList.end(), event) == mappedTsEventList.end()) {
            mappedTsEventList.push_back(event);
        }
    }
}

void CommandListImp::addRegularCmdListSubmissionCounter() {
    if (isInOrderExecutionEnabled()) {
        inOrderExecInfo->addRegularCmdListSubmissionCounter(1);
    }
}

void CommandListImp::enableSynchronizedDispatch(NEO::SynchronizedDispatchMode mode) {
    if (!device->isImplicitScalingCapable()) {
        return;
    }

    this->synchronizedDispatchMode = mode;

    if (mode == NEO::SynchronizedDispatchMode::full) {
        this->syncDispatchQueueId = device->getNextSyncDispatchQueueId();
    } else {
        // Limited mode doesnt acquire new token during execution. It only checks if token is already acquired by full sync dispatch.
        device->ensureSyncDispatchTokenAllocation();
    }
}

} // namespace L0
