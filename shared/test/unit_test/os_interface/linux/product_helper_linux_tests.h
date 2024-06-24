/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/utilities/cpu_info.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/unit_test/os_interface/product_helper_tests.h"

using namespace NEO;

struct ProductHelperTestLinux : public ProductHelperTest {
    static void mockCpuidex(int *cpuInfo, int functionId, int subfunctionId) {
        if (subfunctionId == 0) {
            cpuInfo[0] = 0x7F;
        }
        if (subfunctionId == 1) {
            cpuInfo[0] = 0x1F;
        }
        if (subfunctionId == 2) {
            cpuInfo[0] = 0;
        }
    }

    void SetUp() override {
        ProductHelperTest::SetUp();
        executionEnvironment = std::make_unique<ExecutionEnvironment>();
        executionEnvironment->prepareRootDeviceEnvironments(1);
        executionEnvironment->rootDeviceEnvironments[0]->setHwInfoAndInitHelpers(defaultHwInfo.get());

        drm = new DrmMock(*executionEnvironment->rootDeviceEnvironments[0]);
        executionEnvironment->rootDeviceEnvironments[0]->osInterface.reset(new OSInterface());
        osInterface = executionEnvironment->rootDeviceEnvironments[0]->osInterface.get();
        osInterface->setDriverModel(std::unique_ptr<DriverModel>(drm));

        drm->storedEUVal = pInHwInfo.gtSystemInfo.EUCount;
        drm->storedSSVal = pInHwInfo.gtSystemInfo.SubSliceCount;

        rtCpuidexFunc = CpuInfo::cpuidexFunc;
        CpuInfo::cpuidexFunc = mockCpuidex;
    }

    void TearDown() override {
        CpuInfo::cpuidexFunc = rtCpuidexFunc;

        ProductHelperTest::TearDown();
    }

    template <typename HelperType>
    HelperType &getHelper() const {
        auto &helper = executionEnvironment->rootDeviceEnvironments[0]->getHelper<HelperType>();
        return helper;
    }

    RootDeviceEnvironment &getRootDeviceEnvironment() {
        return *executionEnvironment->rootDeviceEnvironments[0].get();
    }

    OSInterface *osInterface;
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    DrmMock *drm;

    void (*rtCpuidexFunc)(int *, int, int);
};
