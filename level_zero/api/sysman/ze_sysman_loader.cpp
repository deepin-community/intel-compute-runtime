/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include <level_zero/ze_api.h>
#include <level_zero/ze_ddi.h>
#include <level_zero/zes_api.h>
#include <level_zero/zes_ddi.h>
#include <level_zero/zet_api.h>
#include <level_zero/zet_ddi.h>

#include "ze_ddi_tables.h"
#include "zes_sysman_all_api_entrypoints.h"

extern ze_gpu_driver_dditable_t driverDdiTable;

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetDeviceProcAddrTable(
    ze_api_version_t version,
    zes_device_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = L0::zesDeviceGetProperties;
    pDdiTable->pfnGetState = L0::zesDeviceGetState;
    pDdiTable->pfnReset = L0::zesDeviceReset;
    pDdiTable->pfnProcessesGetState = L0::zesDeviceProcessesGetState;
    pDdiTable->pfnPciGetProperties = L0::zesDevicePciGetProperties;
    pDdiTable->pfnPciGetState = L0::zesDevicePciGetState;
    pDdiTable->pfnPciGetBars = L0::zesDevicePciGetBars;
    pDdiTable->pfnPciGetStats = L0::zesDevicePciGetStats;
    pDdiTable->pfnEnumDiagnosticTestSuites = L0::zesDeviceEnumDiagnosticTestSuites;
    pDdiTable->pfnEnumEngineGroups = L0::zesDeviceEnumEngineGroups;
    pDdiTable->pfnEventRegister = L0::zesDeviceEventRegister;
    pDdiTable->pfnEnumFabricPorts = L0::zesDeviceEnumFabricPorts;
    pDdiTable->pfnEnumFans = L0::zesDeviceEnumFans;
    pDdiTable->pfnEnumFirmwares = L0::zesDeviceEnumFirmwares;
    pDdiTable->pfnEnumFrequencyDomains = L0::zesDeviceEnumFrequencyDomains;
    pDdiTable->pfnEnumLeds = L0::zesDeviceEnumLeds;
    pDdiTable->pfnEnumMemoryModules = L0::zesDeviceEnumMemoryModules;
    pDdiTable->pfnEnumPerformanceFactorDomains = L0::zesDeviceEnumPerformanceFactorDomains;
    pDdiTable->pfnEnumPowerDomains = L0::zesDeviceEnumPowerDomains;
    pDdiTable->pfnGetCardPowerDomain = L0::zesDeviceGetCardPowerDomain;
    pDdiTable->pfnEnumPsus = L0::zesDeviceEnumPsus;
    pDdiTable->pfnEnumRasErrorSets = L0::zesDeviceEnumRasErrorSets;
    pDdiTable->pfnEnumSchedulers = L0::zesDeviceEnumSchedulers;
    pDdiTable->pfnEnumStandbyDomains = L0::zesDeviceEnumStandbyDomains;
    pDdiTable->pfnEnumTemperatureSensors = L0::zesDeviceEnumTemperatureSensors;
    pDdiTable->pfnEccAvailable = L0::zesDeviceEccAvailable;
    pDdiTable->pfnEccConfigurable = L0::zesDeviceEccConfigurable;
    pDdiTable->pfnGetEccState = L0::zesDeviceGetEccState;
    pDdiTable->pfnSetEccState = L0::zesDeviceSetEccState;
    pDdiTable->pfnGet = L0::zesDeviceGet;
    pDdiTable->pfnSetOverclockWaiver = L0::zesDeviceSetOverclockWaiver;
    pDdiTable->pfnGetOverclockDomains = L0::zesDeviceGetOverclockDomains;
    pDdiTable->pfnGetOverclockControls = L0::zesDeviceGetOverclockControls;
    pDdiTable->pfnResetOverclockSettings = L0::zesDeviceResetOverclockSettings;
    pDdiTable->pfnReadOverclockState = L0::zesDeviceReadOverclockState;
    pDdiTable->pfnEnumOverclockDomains = L0::zesDeviceEnumOverclockDomains;
    pDdiTable->pfnResetExt = L0::zesDeviceResetExt;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetGlobalProcAddrTable(
    ze_api_version_t version,
    zes_global_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnInit = L0::zesInit;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetDriverProcAddrTable(
    ze_api_version_t version,
    zes_driver_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnEventListen = L0::zesDriverEventListen;
    pDdiTable->pfnEventListenEx = L0::zesDriverEventListenEx;
    pDdiTable->pfnGet = L0::zesDriverGet;
    pDdiTable->pfnGetExtensionProperties = L0::zesDriverGetExtensionProperties;
    pDdiTable->pfnGetExtensionFunctionAddress = L0::zesDriverGetExtensionFunctionAddress;
    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetDiagnosticsProcAddrTable(
    ze_api_version_t version,
    zes_diagnostics_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = L0::zesDiagnosticsGetProperties;
    pDdiTable->pfnGetTests = L0::zesDiagnosticsGetTests;
    pDdiTable->pfnRunTests = L0::zesDiagnosticsRunTests;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetEngineProcAddrTable(
    ze_api_version_t version,
    zes_engine_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = L0::zesEngineGetProperties;
    pDdiTable->pfnGetActivity = L0::zesEngineGetActivity;
    pDdiTable->pfnGetActivityExt = L0::zesEngineGetActivityExt;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetFabricPortProcAddrTable(
    ze_api_version_t version,
    zes_fabric_port_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = L0::zesFabricPortGetProperties;
    pDdiTable->pfnGetLinkType = L0::zesFabricPortGetLinkType;
    pDdiTable->pfnGetConfig = L0::zesFabricPortGetConfig;
    pDdiTable->pfnSetConfig = L0::zesFabricPortSetConfig;
    pDdiTable->pfnGetState = L0::zesFabricPortGetState;
    pDdiTable->pfnGetThroughput = L0::zesFabricPortGetThroughput;
    pDdiTable->pfnGetFabricErrorCounters = L0::zesFabricPortGetFabricErrorCounters;
    pDdiTable->pfnGetMultiPortThroughput = L0::zesFabricPortGetMultiPortThroughput;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetFanProcAddrTable(
    ze_api_version_t version,
    zes_fan_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = L0::zesFanGetProperties;
    pDdiTable->pfnGetConfig = L0::zesFanGetConfig;
    pDdiTable->pfnSetDefaultMode = L0::zesFanSetDefaultMode;
    pDdiTable->pfnSetFixedSpeedMode = L0::zesFanSetFixedSpeedMode;
    pDdiTable->pfnSetSpeedTableMode = L0::zesFanSetSpeedTableMode;
    pDdiTable->pfnGetState = L0::zesFanGetState;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetFirmwareProcAddrTable(
    ze_api_version_t version,
    zes_firmware_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = L0::zesFirmwareGetProperties;
    pDdiTable->pfnFlash = L0::zesFirmwareFlash;
    pDdiTable->pfnGetFlashProgress = L0::zesFirmwareGetFlashProgress;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetFrequencyProcAddrTable(
    ze_api_version_t version,
    zes_frequency_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;

    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = L0::zesFrequencyGetProperties;
    pDdiTable->pfnGetAvailableClocks = L0::zesFrequencyGetAvailableClocks;
    pDdiTable->pfnGetRange = L0::zesFrequencyGetRange;
    pDdiTable->pfnSetRange = L0::zesFrequencySetRange;
    pDdiTable->pfnGetState = L0::zesFrequencyGetState;
    pDdiTable->pfnGetThrottleTime = L0::zesFrequencyGetThrottleTime;
    pDdiTable->pfnOcGetCapabilities = L0::zesFrequencyOcGetCapabilities;
    pDdiTable->pfnOcGetFrequencyTarget = L0::zesFrequencyOcGetFrequencyTarget;
    pDdiTable->pfnOcSetFrequencyTarget = L0::zesFrequencyOcSetFrequencyTarget;
    pDdiTable->pfnOcGetVoltageTarget = L0::zesFrequencyOcGetVoltageTarget;
    pDdiTable->pfnOcSetVoltageTarget = L0::zesFrequencyOcSetVoltageTarget;
    pDdiTable->pfnOcSetMode = L0::zesFrequencyOcSetMode;
    pDdiTable->pfnOcGetMode = L0::zesFrequencyOcGetMode;
    pDdiTable->pfnOcGetIccMax = L0::zesFrequencyOcGetIccMax;
    pDdiTable->pfnOcSetIccMax = L0::zesFrequencyOcSetIccMax;
    pDdiTable->pfnOcGetTjMax = L0::zesFrequencyOcGetTjMax;
    pDdiTable->pfnOcSetTjMax = L0::zesFrequencyOcSetTjMax;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetLedProcAddrTable(
    ze_api_version_t version,
    zes_led_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = L0::zesLedGetProperties;
    pDdiTable->pfnGetState = L0::zesLedGetState;
    pDdiTable->pfnSetState = L0::zesLedSetState;
    pDdiTable->pfnSetColor = L0::zesLedSetColor;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetMemoryProcAddrTable(
    ze_api_version_t version,
    zes_memory_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = L0::zesMemoryGetProperties;
    pDdiTable->pfnGetState = L0::zesMemoryGetState;
    pDdiTable->pfnGetBandwidth = L0::zesMemoryGetBandwidth;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetPerformanceFactorProcAddrTable(
    ze_api_version_t version,
    zes_performance_factor_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = L0::zesPerformanceFactorGetProperties;
    pDdiTable->pfnGetConfig = L0::zesPerformanceFactorGetConfig;
    pDdiTable->pfnSetConfig = L0::zesPerformanceFactorSetConfig;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetPowerProcAddrTable(
    ze_api_version_t version,
    zes_power_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = L0::zesPowerGetProperties;
    pDdiTable->pfnGetEnergyCounter = L0::zesPowerGetEnergyCounter;
    pDdiTable->pfnGetLimits = L0::zesPowerGetLimits;
    pDdiTable->pfnSetLimits = L0::zesPowerSetLimits;
    pDdiTable->pfnGetLimitsExt = L0::zesPowerGetLimitsExt;
    pDdiTable->pfnSetLimitsExt = L0::zesPowerSetLimitsExt;
    pDdiTable->pfnGetEnergyThreshold = L0::zesPowerGetEnergyThreshold;
    pDdiTable->pfnSetEnergyThreshold = L0::zesPowerSetEnergyThreshold;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetPsuProcAddrTable(
    ze_api_version_t version,
    zes_psu_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = L0::zesPsuGetProperties;
    pDdiTable->pfnGetState = L0::zesPsuGetState;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetRasProcAddrTable(
    ze_api_version_t version,
    zes_ras_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = L0::zesRasGetProperties;
    pDdiTable->pfnGetConfig = L0::zesRasGetConfig;
    pDdiTable->pfnSetConfig = L0::zesRasSetConfig;
    pDdiTable->pfnGetState = L0::zesRasGetState;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetRasExpProcAddrTable(
    ze_api_version_t version,
    zes_ras_exp_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetStateExp = L0::zesRasGetStateExp;
    pDdiTable->pfnClearStateExp = L0::zesRasClearStateExp;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetSchedulerProcAddrTable(
    ze_api_version_t version,
    zes_scheduler_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = L0::zesSchedulerGetProperties;
    pDdiTable->pfnGetCurrentMode = L0::zesSchedulerGetCurrentMode;
    pDdiTable->pfnGetTimeoutModeProperties = L0::zesSchedulerGetTimeoutModeProperties;
    pDdiTable->pfnGetTimesliceModeProperties = L0::zesSchedulerGetTimesliceModeProperties;
    pDdiTable->pfnSetTimeoutMode = L0::zesSchedulerSetTimeoutMode;
    pDdiTable->pfnSetTimesliceMode = L0::zesSchedulerSetTimesliceMode;
    pDdiTable->pfnSetExclusiveMode = L0::zesSchedulerSetExclusiveMode;
    pDdiTable->pfnSetComputeUnitDebugMode = L0::zesSchedulerSetComputeUnitDebugMode;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetStandbyProcAddrTable(
    ze_api_version_t version,
    zes_standby_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = L0::zesStandbyGetProperties;
    pDdiTable->pfnGetMode = L0::zesStandbyGetMode;
    pDdiTable->pfnSetMode = L0::zesStandbySetMode;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetTemperatureProcAddrTable(
    ze_api_version_t version,
    zes_temperature_dditable_t *pDdiTable) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetProperties = L0::zesTemperatureGetProperties;
    pDdiTable->pfnGetConfig = L0::zesTemperatureGetConfig;
    pDdiTable->pfnSetConfig = L0::zesTemperatureSetConfig;
    pDdiTable->pfnGetState = L0::zesTemperatureGetState;

    return result;
}

ZE_DLLEXPORT ze_result_t ZE_APICALL
zesGetOverclockProcAddrTable(
    ze_api_version_t version,           ///< [in] API version requested
    zes_overclock_dditable_t *pDdiTable ///< [in,out] pointer to table of DDI function pointers
) {
    if (nullptr == pDdiTable)
        return ZE_RESULT_ERROR_INVALID_ARGUMENT;
    if (ZE_MAJOR_VERSION(driverDdiTable.version) != ZE_MAJOR_VERSION(version) ||
        ZE_MINOR_VERSION(driverDdiTable.version) > ZE_MINOR_VERSION(version))
        return ZE_RESULT_ERROR_UNSUPPORTED_VERSION;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pDdiTable->pfnGetDomainProperties = L0::zesOverclockGetDomainProperties;
    pDdiTable->pfnGetDomainVFProperties = L0::zesOverclockGetDomainVFProperties;
    pDdiTable->pfnGetDomainControlProperties = L0::zesOverclockGetDomainControlProperties;
    pDdiTable->pfnGetControlCurrentValue = L0::zesOverclockGetControlCurrentValue;
    pDdiTable->pfnGetControlPendingValue = L0::zesOverclockGetControlPendingValue;
    pDdiTable->pfnSetControlUserValue = L0::zesOverclockSetControlUserValue;
    pDdiTable->pfnGetControlState = L0::zesOverclockGetControlState;
    pDdiTable->pfnGetVFPointValues = L0::zesOverclockGetVFPointValues;
    pDdiTable->pfnSetVFPointValues = L0::zesOverclockSetVFPointValues;

    return result;
}
