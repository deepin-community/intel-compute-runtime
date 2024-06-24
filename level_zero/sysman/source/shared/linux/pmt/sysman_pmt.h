/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/sys_calls.h"

#include "level_zero/zes_api.h"

#include "igfxfmid.h"

#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#include <sys/types.h>

namespace L0 {
namespace Sysman {
class LinuxSysmanImp;
class FsAccessInterface;

class PlatformMonitoringTech : NEO::NonCopyableOrMovableClass {
  public:
    PlatformMonitoringTech() = default;
    PlatformMonitoringTech(FsAccessInterface *pFsAccess, ze_bool_t onSubdevice, uint32_t subdeviceId);
    virtual ~PlatformMonitoringTech();

    virtual ze_result_t readValue(const std::string key, uint32_t &value);
    virtual ze_result_t readValue(const std::string key, uint64_t &value);
    std::string getGuid();
    static ze_result_t enumerateRootTelemIndex(FsAccessInterface *pFsAccess, std::string &gpuUpstreamPortPath);
    static void create(LinuxSysmanImp *pLinuxSysmanImp, std::string &gpuUpstreamPortPath,
                       std::map<uint32_t, L0::Sysman::PlatformMonitoringTech *> &mapOfSubDeviceIdToPmtObject);
    static ze_result_t getKeyOffsetMap(std::string guid, std::map<std::string, uint64_t> &keyOffsetMap);

  protected:
    static uint32_t rootDeviceTelemNodeIndex;
    std::string telemetryDeviceEntry{};
    std::map<std::string, uint64_t> keyOffsetMap;
    std::string guid;
    ze_result_t init(FsAccessInterface *pFsAccess, const std::string &gpuUpstreamPortPath, PRODUCT_FAMILY productFamily);
    static void doInitPmtObject(FsAccessInterface *pFsAccess, uint32_t subdeviceId, PlatformMonitoringTech *pPmt, const std::string &gpuUpstreamPortPath,
                                std::map<uint32_t, L0::Sysman::PlatformMonitoringTech *> &mapOfSubDeviceIdToPmtObject, PRODUCT_FAMILY productFamily);
    decltype(&NEO::SysCalls::pread) preadFunction = NEO::SysCalls::pread;

  private:
    static const std::string baseTelemSysFS;
    static const std::string telem;
    uint64_t baseOffset = 0;
    uint32_t subdeviceId = 0;
    ze_bool_t isSubdevice = 0;
};

} // namespace Sysman
} // namespace L0
