/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/sysman/zes_handles_struct.h"
#include "level_zero/sysman/source/device/sysman_device.h"
#include <level_zero/zes_api.h>

#include <mutex>
#include <vector>

namespace L0 {
namespace Sysman {

struct OsSysman;

class Ras : _zes_ras_handle_t {
  public:
    virtual ze_result_t rasGetProperties(zes_ras_properties_t *pProperties) = 0;
    virtual ze_result_t rasGetConfig(zes_ras_config_t *pConfig) = 0;
    virtual ze_result_t rasSetConfig(const zes_ras_config_t *pConfig) = 0;
    virtual ze_result_t rasGetState(zes_ras_state_t *pState, ze_bool_t clear) = 0;
    virtual ze_result_t rasGetStateExp(uint32_t *pCount, zes_ras_state_exp_t *pState) = 0;
    virtual ze_result_t rasClearStateExp(zes_ras_error_category_exp_t category) = 0;

    static Ras *fromHandle(zes_ras_handle_t handle) {
        return static_cast<Ras *>(handle);
    }

    inline zes_ras_handle_t toHandle() { return this; }
    bool isRasErrorSupported = false;
    zes_ras_error_type_t rasErrorType{};
};

struct RasHandleContext {
    RasHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    MOCKABLE_VIRTUAL ~RasHandleContext();

    MOCKABLE_VIRTUAL void init(uint32_t subDeviceCount);
    void releaseRasHandles();

    ze_result_t rasGet(uint32_t *pCount, zes_ras_handle_t *phRas);

    OsSysman *pOsSysman = nullptr;
    std::vector<Ras *> handleList = {};
    bool isRasInitDone() {
        return rasInitDone;
    }

  private:
    void createHandle(zes_ras_error_type_t type, ze_bool_t isSubDevice, uint32_t subDeviceId);
    std::once_flag initRasOnce;
    bool rasInitDone = false;
};

} // namespace Sysman
} // namespace L0
