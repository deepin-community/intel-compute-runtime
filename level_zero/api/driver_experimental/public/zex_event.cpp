/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/api/driver_experimental/public/zex_event.h"

#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/core/source/event/event.h"

namespace L0 {

ZE_APIEXPORT ze_result_t ZE_APICALL
zexEventGetDeviceAddress(ze_event_handle_t event, uint64_t *completionValue, uint64_t *address) {
    auto eventObj = Event::fromHandle(event);

    if (!eventObj || !eventObj->isCounterBased() || !eventObj->getInOrderExecInfo() || !completionValue || !address) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    *completionValue = eventObj->getInOrderExecSignalValueWithSubmissionCounter();
    *address = eventObj->getInOrderExecInfo()->getBaseDeviceAddress() + eventObj->getInOrderAllocationOffset();

    return ZE_RESULT_SUCCESS;
}

ZE_APIEXPORT ze_result_t ZE_APICALL
zexCounterBasedEventCreate(ze_context_handle_t hContext, ze_device_handle_t hDevice, uint64_t *deviceAddress, uint64_t *hostAddress, uint64_t completionValue, const ze_event_desc_t *desc, ze_event_handle_t *phEvent) {
    constexpr uint32_t counterBasedFlags = (ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_IMMEDIATE | ZE_EVENT_POOL_COUNTER_BASED_EXP_FLAG_NON_IMMEDIATE);

    constexpr EventDescriptor eventDescriptor = {
        nullptr,                           // eventPoolAllocation
        0,                                 // totalEventSize
        EventPacketsCount::maxKernelSplit, // maxKernelCount
        0,                                 // maxPacketsCount
        counterBasedFlags,                 // counterBasedFlags
        false,                             // timestampPool
        false,                             // kerneMappedTsPoolFlag
        false,                             // importedIpcPool
        false,                             // ipcPool
    };

    auto device = Device::fromHandle(hDevice);

    if (!hDevice || !deviceAddress || !hostAddress || !desc || !phEvent) {
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    }

    auto inOrderExecInfo = NEO::InOrderExecInfo::createFromExternalAllocation(*device->getNEODevice(), castToUint64(deviceAddress), hostAddress, completionValue);

    *phEvent = Event::create<uint64_t>(eventDescriptor, desc, device);
    Event::fromHandle(*phEvent)->updateInOrderExecState(inOrderExecInfo, completionValue, 0);

    return ZE_RESULT_SUCCESS;
}

} // namespace L0
