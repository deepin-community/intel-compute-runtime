/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <memory>
#include <vector>

namespace NEO {
class ExecutionEnvironment;
class ClExecutionEnvironment;
class ClDevice;
class MemoryManager;
class MockMemoryManager;
class MockClDevice;
struct UltDeviceFactory;

struct UltClDeviceFactory {
    UltClDeviceFactory(uint32_t rootDevicesCount, uint32_t subDevicesCount);
    UltClDeviceFactory(uint32_t rootDevicesCount, uint32_t subDevicesCount, ClExecutionEnvironment *clExecutionEnvironment);
    ~UltClDeviceFactory();

    std::unique_ptr<UltDeviceFactory> pUltDeviceFactory;
    std::vector<MockClDevice *> rootDevices;
    std::vector<ClDevice *> subDevices;
};

} // namespace NEO
