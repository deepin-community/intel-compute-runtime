/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/client_context/gmm_client_context.h"
#include "shared/source/gmm_helper/client_context/map_gpu_va_gmm.h"
#include "shared/source/os_interface/windows/gdi_interface.h"

namespace NEO {
uint64_t GmmClientContext::mapGpuVirtualAddress(MapGpuVirtualAddressGmm *pMapGpuVa) {
    return pMapGpuVa->gdi->mapGpuVirtualAddress(pMapGpuVa->mapGpuVirtualAddressParams);
}
uint64_t GmmClientContext::freeGpuVirtualAddress(FreeGpuVirtualAddressGmm *pFreeGpuVa) {
    return 0;
}

} // namespace NEO
