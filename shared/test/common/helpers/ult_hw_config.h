/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
namespace NEO {
struct UltHwConfig {
    bool mockedPrepareDeviceEnvironmentsFuncResult = true;
    bool useHwCsr = false;
    bool useMockedPrepareDeviceEnvironmentsFunc = true;
    bool forceOsAgnosticMemoryManager = true;
    bool useinitBuiltinsAsyncEnabled = false;
    bool useWaitForTimestamps = false;
    bool useBlitSplit = false;
    bool useFirstSubmissionInitDevice = false;

    bool csrFailInitDirectSubmission = false;
    bool csrBaseCallDirectSubmissionAvailable = false;
    bool csrSuperBaseCallDirectSubmissionAvailable = false;

    bool csrBaseCallBlitterDirectSubmissionAvailable = false;
    bool csrSuperBaseCallBlitterDirectSubmissionAvailable = false;

    bool csrBaseCallCreatePreemption = true;
    bool csrCreatePreemptionReturnValue = true;
    bool reserved = false;

    const char *aubTestName = nullptr;
};

extern UltHwConfig ultHwConfig;
} // namespace NEO
