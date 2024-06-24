/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/mocks/mock_module.h"

#include "level_zero/core/source/device/device.h"

namespace L0 {
namespace ult {

ze_result_t WhiteBox<::L0::Module>::initializeTranslationUnit(const ze_module_desc_t *desc, NEO::Device *neoDevice) {
    auto result = this->BaseClass::initializeTranslationUnit(desc, neoDevice);
    if (this->mockGlobalConstBuffer) {
        this->translationUnit->globalConstBuffer = this->mockGlobalConstBuffer;
    }
    if (this->mockGlobalVarBuffer) {
        this->translationUnit->globalVarBuffer = this->mockGlobalVarBuffer;
    }
    return result;
}
} // namespace ult
} // namespace L0