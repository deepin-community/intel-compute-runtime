/*
 * Copyright (C) 2022-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/os_interface/os_interface.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_sip.h"

#include "level_zero/core/test/unit_tests/fixtures/device_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_built_ins.h"
#include "level_zero/core/test/unit_tests/mocks/mock_device.h"

#include "common/StateSaveAreaHeader.h"

namespace L0 {
namespace ult {

struct DebugApiFixture : public DeviceFixture {
    void setUp() {
        DeviceFixture::setUp();
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new NEO::OSInterface);
        mockBuiltins = new MockBuiltins();
        mockBuiltins->stateSaveAreaHeader = MockSipData::createStateSaveAreaHeader(2);
        neoDevice->executionEnvironment->rootDeviceEnvironments[0]->builtins.reset(mockBuiltins);
    }

    void tearDown() {
        DeviceFixture::tearDown();
    }

    MockBuiltins *mockBuiltins = nullptr;
};

size_t threadSlotOffset(SIP::StateSaveAreaHeader *pStateSaveAreaHeader, int slice, int subslice, int eu, int thread);

size_t regOffsetInThreadSlot(const SIP::regset_desc *regdesc, uint32_t start);

void initStateSaveArea(std::vector<char> &stateSaveArea, SIP::version version, L0::Device *device);

} // namespace ult
} // namespace L0
