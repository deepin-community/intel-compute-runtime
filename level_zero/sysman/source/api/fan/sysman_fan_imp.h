/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/fan/sysman_fan.h"
#include "level_zero/sysman/source/api/fan/sysman_os_fan.h"

namespace L0 {
namespace Sysman {
class FanImp : public Fan, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t fanGetProperties(zes_fan_properties_t *pProperties) override;
    ze_result_t fanGetConfig(zes_fan_config_t *pConfig) override;
    ze_result_t fanSetDefaultMode() override;
    ze_result_t fanSetFixedSpeedMode(const zes_fan_speed_t *pSpeed) override;
    ze_result_t fanSetSpeedTableMode(const zes_fan_speed_table_t *pSpeedTable) override;
    ze_result_t fanGetState(zes_fan_speed_units_t units, int32_t *pSpeed) override;
    FanImp() = default;
    FanImp(OsSysman *pOsSysman);
    ~FanImp() override;

    std::unique_ptr<OsFan> pOsFan;
    void init();
};
} // namespace Sysman
} // namespace L0
