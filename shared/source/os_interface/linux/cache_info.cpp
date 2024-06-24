/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/cache_info.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/debug_helpers.h"

namespace NEO {

CacheInfo::~CacheInfo() {
    for (auto const &cacheRegion : cacheRegionsReserved) {
        cacheReserve.freeCache(CacheLevel::level3, cacheRegion.first);
    }
    cacheRegionsReserved.clear();
}

CacheRegion CacheInfo::reserveRegion(size_t cacheReservationSize) {
    uint16_t numWays = (maxReservationNumWays * cacheReservationSize) / maxReservationCacheSize;
    if (debugManager.flags.ClosNumCacheWays.get() != -1) {
        numWays = debugManager.flags.ClosNumCacheWays.get();
        cacheReservationSize = (numWays * maxReservationCacheSize) / maxReservationNumWays;
    }
    auto regionIndex = cacheReserve.reserveCache(CacheLevel::level3, numWays);
    if (regionIndex == CacheRegion::none) {
        return CacheRegion::none;
    }
    cacheRegionsReserved.insert({regionIndex, cacheReservationSize});

    return regionIndex;
}

CacheRegion CacheInfo::freeRegion(CacheRegion regionIndex) {
    auto search = cacheRegionsReserved.find(regionIndex);
    if (search != cacheRegionsReserved.end()) {
        cacheRegionsReserved.erase(search);
        return cacheReserve.freeCache(CacheLevel::level3, regionIndex);
    }
    return CacheRegion::none;
}

bool CacheInfo::isRegionReserved(CacheRegion regionIndex, [[maybe_unused]] size_t regionSize) const {
    auto search = cacheRegionsReserved.find(regionIndex);
    if (search != cacheRegionsReserved.end()) {
        if (debugManager.flags.ClosNumCacheWays.get() != -1) {
            auto numWays = debugManager.flags.ClosNumCacheWays.get();
            regionSize = (numWays * maxReservationCacheSize) / maxReservationNumWays;
        }
        DEBUG_BREAK_IF(search->second != regionSize);
        return true;
    }
    return false;
}

bool CacheInfo::getRegion(size_t regionSize, CacheRegion regionIndex) {
    if (regionIndex == CacheRegion::defaultRegion) {
        return true;
    }
    if (!isRegionReserved(regionIndex, regionSize)) {
        auto regionIdx = reserveRegion(regionSize);
        if (regionIdx == CacheRegion::none) {
            return false;
        }
        DEBUG_BREAK_IF(regionIdx != regionIndex);
    }
    return true;
}

} // namespace NEO
