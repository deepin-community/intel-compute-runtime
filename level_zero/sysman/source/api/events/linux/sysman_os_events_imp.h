/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/sysman/source/api/events/sysman_os_events.h"
#include "level_zero/sysman/source/shared/linux/udev/udev_lib.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

class LinuxSysmanDriverImp;

class LinuxEventsImp : public OsEvents, NEO::NonCopyableOrMovableClass {
  public:
    bool eventListen(zes_event_type_flags_t &pEvent, uint64_t timeout) override;
    ze_result_t eventRegister(zes_event_type_flags_t events) override;
    LinuxEventsImp() = delete;
    LinuxEventsImp(OsSysman *pOsSysman);
    ~LinuxEventsImp() override = default;

  protected:
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
};

class LinuxEventsUtil {

  public:
    LinuxEventsUtil() = delete;
    LinuxEventsUtil(LinuxSysmanDriverImp *pOsSysmanDriverImp);
    ~LinuxEventsUtil() = default;

    ze_result_t eventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents);
    void eventRegister(zes_event_type_flags_t events, SysmanDeviceImp *pSysmanDevice);

  protected:
    UdevLib *pUdevLib = nullptr;
    LinuxSysmanDriverImp *pLinuxSysmanDriverImp = nullptr;
    int pipeFd[2] = {-1, -1};
    std::map<SysmanDeviceImp *, zes_event_type_flags_t> deviceEventsMap;
    bool checkRasEvent(zes_event_type_flags_t &pEvent, SysmanDeviceImp *pSysmanDeviceImp, zes_event_type_flags_t registeredEvents);
    bool isResetRequired(void *dev, zes_event_type_flags_t &pEvent);
    bool checkDeviceDetachEvent(zes_event_type_flags_t &pEvent);
    bool checkDeviceAttachEvent(zes_event_type_flags_t &pEvent);
    bool checkIfMemHealthChanged(void *dev, zes_event_type_flags_t &pEvent);
    bool checkIfFabricPortStatusChanged(void *dev, zes_event_type_flags_t &pEvent);
    bool listenSystemEvents(zes_event_type_flags_t *pEvents, uint32_t count, std::vector<zes_event_type_flags_t> &registeredEvents, zes_device_handle_t *phDevices, uint64_t timeout);

  private:
    std::string action;
    static const std::string add;
    static const std::string remove;
    static const std::string change;
    static const std::string unbind;
    static const std::string bind;
    static bool checkRasEventOccured(Ras *rasHandle);
    void getDevIndexToDevPathMap(std::vector<zes_event_type_flags_t> &registeredEvents, uint32_t count, zes_device_handle_t *phDevices, std::map<uint32_t, std::string> &mapOfDevIndexToDevPath);
    bool checkDeviceEvents(std::vector<zes_event_type_flags_t> &registeredEvents, std::map<uint32_t, std::string> mapOfDevIndexToDevPath, zes_event_type_flags_t *pEvents, void *dev);
    std::once_flag initEventsOnce;
    std::mutex eventsMutex;
    void init();
};

} // namespace Sysman
} // namespace L0
