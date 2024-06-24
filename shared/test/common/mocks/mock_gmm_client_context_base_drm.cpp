/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_gmm_client_context_base.h"

namespace NEO {
uint64_t MockGmmClientContextBase::mapGpuVirtualAddress(MapGpuVirtualAddressGmm *pMapGpuVa) {
    mapGpuVirtualAddressCalled++;
    return 0;
}
} // namespace NEO