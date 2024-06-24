/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/helpers/register_offsets.h"
#include "shared/source/os_interface/linux/engine_info.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/memory_info.h"
#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/libult/linux/drm_mock.h"
#include "shared/test/common/mocks/linux/mock_os_time_linux.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

#include "drm/xe_drm.h"

using namespace NEO;

struct MockIoctlHelperXe : IoctlHelperXe {
    using IoctlHelperXe::bindInfo;
    using IoctlHelperXe::contextParamEngine;
    using IoctlHelperXe::debugMetadata;
    using IoctlHelperXe::defaultEngine;
    using IoctlHelperXe::getFdFromVmExport;
    using IoctlHelperXe::IoctlHelperXe;
    using IoctlHelperXe::maxContextSetProperties;
    using IoctlHelperXe::maxExecQueuePriority;
    using IoctlHelperXe::setContextProperties;
    using IoctlHelperXe::setDefaultEngine;
    using IoctlHelperXe::UserFenceExtension;
    using IoctlHelperXe::xeGetBindFlagsName;
    using IoctlHelperXe::xeGetBindOperationName;
    using IoctlHelperXe::xeGetClassName;
    using IoctlHelperXe::xeGetengineClassName;
    using IoctlHelperXe::xeShowBindTable;
    using IoctlHelperXe::xeTimestampFrequency;
};

inline constexpr int testValueVmId = 0x5764;
inline constexpr int testValueMapOff = 0x7788;
inline constexpr int testValuePrime = 0x4321;
inline constexpr uint32_t testValueGemCreate = 0x8273;
class DrmMockXe : public DrmMockCustom {
  public:
    DrmMockXe(RootDeviceEnvironment &rootDeviceEnvironment) : DrmMockCustom(rootDeviceEnvironment) {

        auto xeQueryConfig = reinterpret_cast<drm_xe_query_config *>(queryConfig);
        xeQueryConfig->num_params = 5;
        xeQueryConfig->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] = (revId << 16) | devId;
        xeQueryConfig->info[DRM_XE_QUERY_CONFIG_VA_BITS] = 48;
        xeQueryConfig->info[DRM_XE_QUERY_CONFIG_MAX_EXEC_QUEUE_PRIORITY] = mockMaxExecQueuePriority;

        auto xeQueryEngines = reinterpret_cast<drm_xe_query_engines *>(queryEngines);
        xeQueryEngines->num_engines = 11;
        xeQueryEngines->engines[0] = {{DRM_XE_ENGINE_CLASS_RENDER, 0, 0}, {}};
        xeQueryEngines->engines[1] = {{DRM_XE_ENGINE_CLASS_COPY, 1, 0}, {}};
        xeQueryEngines->engines[2] = {{DRM_XE_ENGINE_CLASS_COPY, 2, 0}, {}};
        xeQueryEngines->engines[3] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 3, 0}, {}};
        xeQueryEngines->engines[4] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 4, 0}, {}};
        xeQueryEngines->engines[5] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 5, 1}, {}};
        xeQueryEngines->engines[6] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 6, 1}, {}};
        xeQueryEngines->engines[7] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 7, 1}, {}};
        xeQueryEngines->engines[8] = {{DRM_XE_ENGINE_CLASS_COMPUTE, 8, 1}, {}};
        xeQueryEngines->engines[9] = {{DRM_XE_ENGINE_CLASS_VIDEO_DECODE, 9, 1}, {}};
        xeQueryEngines->engines[10] = {{DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE, 10, 0}, {}};

        auto xeQueryMemUsage = reinterpret_cast<drm_xe_query_mem_regions *>(queryMemUsage);
        xeQueryMemUsage->num_mem_regions = 3;
        xeQueryMemUsage->mem_regions[0] = {
            DRM_XE_MEM_REGION_CLASS_VRAM,  // class
            1,                             // instance
            MemoryConstants::pageSize,     // min page size
            2 * MemoryConstants::gigaByte, // total size
            MemoryConstants::megaByte      // used size
        };
        xeQueryMemUsage->mem_regions[1] = {
            DRM_XE_MEM_REGION_CLASS_SYSMEM, // class
            0,                              // instance
            MemoryConstants::pageSize,      // min page size
            MemoryConstants::gigaByte,      // total size
            MemoryConstants::kiloByte       // used size
        };
        xeQueryMemUsage->mem_regions[2] = {
            DRM_XE_MEM_REGION_CLASS_VRAM,  // class
            2,                             // instance
            MemoryConstants::pageSize,     // min page size
            4 * MemoryConstants::gigaByte, // total size
            MemoryConstants::gigaByte      // used size
        };

        auto xeQueryGtList = reinterpret_cast<drm_xe_query_gt_list *>(queryGtList.begin());
        xeQueryGtList->num_gt = 3;
        xeQueryGtList->gt_list[0] = {
            DRM_XE_QUERY_GT_TYPE_MAIN, // type
            0,                         // tile_id
            0,                         // gt_id
            {0},                       // padding
            12500000,                  // reference_clock
            0b100,                     // native mem regions
            0x011,                     // slow mem regions
        };
        xeQueryGtList->gt_list[1] = {
            DRM_XE_QUERY_GT_TYPE_MEDIA, // type
            1,                          // tile_id
            1,                          // gt_id
            {0},                        // padding
            12500000,                   // reference_clock
            0b001,                      // native mem regions
            0x110,                      // slow mem regions
        };
        xeQueryGtList->gt_list[2] = {
            DRM_XE_QUERY_GT_TYPE_MAIN, // type
            0,                         // tile_id
            0,                         // gt_id
            {0},                       // padding
            12500000,                  // reference_clock
            0b010,                     // native mem regions
            0x101,                     // slow mem regions
        };
    }

    void testMode(int f, int a = 0) {
        forceIoctlAnswer = f;
        setIoctlAnswer = a;
    }
    int ioctl(DrmIoctl request, void *arg) override {
        int ret = -1;
        ioctlCalled = true;
        if (forceIoctlAnswer) {
            return setIoctlAnswer;
        }
        switch (request) {
        case DrmIoctl::gemVmCreate: {
            struct drm_xe_vm_create *v = static_cast<struct drm_xe_vm_create *>(arg);
            v->vm_id = testValueVmId;
            ret = 0;
        } break;
        case DrmIoctl::gemUserptr: {
            ret = 0;
        } break;
        case DrmIoctl::gemClose: {
            auto gemClose = reinterpret_cast<GemClose *>(arg);
            passedGemClose = *gemClose;
            gemCloseCalled++;
            ret = 0;
        } break;
        case DrmIoctl::gemVmDestroy: {
            struct drm_xe_vm_destroy *v = static_cast<struct drm_xe_vm_destroy *>(arg);
            if (v->vm_id == testValueVmId)
                ret = 0;
        } break;
        case DrmIoctl::gemMmapOffset: {
            struct drm_xe_gem_mmap_offset *v = static_cast<struct drm_xe_gem_mmap_offset *>(arg);
            if (v->handle == testValueMapOff) {
                v->offset = v->handle;
                ret = 0;
            }
        } break;
        case DrmIoctl::primeFdToHandle: {
            PrimeHandle *v = static_cast<PrimeHandle *>(arg);
            if (v->fileDescriptor == testValuePrime) {
                v->handle = testValuePrime;
                ret = 0;
            }
        } break;
        case DrmIoctl::primeHandleToFd: {
            PrimeHandle *v = static_cast<PrimeHandle *>(arg);
            if (v->handle == testValuePrime) {
                v->fileDescriptor = testValuePrime;
                ret = 0;
            }
        } break;
        case DrmIoctl::gemCreate: {
            ioctlCnt.gemCreate++;
            auto createParams = static_cast<drm_xe_gem_create *>(arg);
            this->createParamsSize = createParams->size;
            this->createParamsPlacement = createParams->placement;
            this->createParamsFlags = createParams->flags;
            this->createParamsHandle = createParams->handle = testValueGemCreate;
            this->createParamsCpuCaching = createParams->cpu_caching;
            if (0 == this->createParamsSize || 0 == this->createParamsPlacement || 0 == this->createParamsCpuCaching) {
                return EINVAL;
            }
            ret = 0;
        } break;
        case DrmIoctl::getparam:
        case DrmIoctl::getResetStats:
            ret = -2;
            break;
        case DrmIoctl::query: {
            struct drm_xe_device_query *deviceQuery = static_cast<struct drm_xe_device_query *>(arg);
            switch (deviceQuery->query) {
            case DRM_XE_DEVICE_QUERY_CONFIG:
                if (deviceQuery->data) {
                    memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryConfig, sizeof(queryConfig));
                }
                deviceQuery->size = sizeof(queryConfig);
                break;
            case DRM_XE_DEVICE_QUERY_ENGINES:
                if (deviceQuery->data) {
                    memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryEngines, sizeof(queryEngines));
                }
                deviceQuery->size = sizeof(queryEngines);
                break;
            case DRM_XE_DEVICE_QUERY_MEM_REGIONS:
                if (deviceQuery->data) {
                    memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryMemUsage, sizeof(queryMemUsage));
                }
                deviceQuery->size = sizeof(queryMemUsage);
                break;
            case DRM_XE_DEVICE_QUERY_GT_LIST:
                if (deviceQuery->data) {
                    memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryGtList.begin(), sizeof(queryGtList));
                }
                deviceQuery->size = sizeof(queryGtList);
                break;
            case DRM_XE_DEVICE_QUERY_GT_TOPOLOGY:
                if (deviceQuery->data) {
                    memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryTopology.data(), queryTopology.size());
                }
                deviceQuery->size = static_cast<unsigned int>(queryTopology.size());
                break;
            case DRM_XE_DEVICE_QUERY_ENGINE_CYCLES:
                if (deviceQuery->data) {
                    memcpy_s(reinterpret_cast<void *>(deviceQuery->data), deviceQuery->size, queryEngineCycles, sizeof(queryEngineCycles));
                }
                deviceQuery->size = sizeof(queryEngineCycles);
                break;
            };
            ret = 0;
        } break;
        case DrmIoctl::gemVmBind: {
            ret = gemVmBindReturn;
            auto vmBindInput = static_cast<drm_xe_vm_bind *>(arg);
            vmBindInputs.push_back(*vmBindInput);

            EXPECT_EQ(1u, vmBindInput->num_syncs);

            auto &syncInput = reinterpret_cast<drm_xe_sync *>(vmBindInput->syncs)[0];
            syncInputs.push_back(syncInput);
        } break;

        case DrmIoctl::gemWaitUserFence: {
            ret = waitUserFenceReturn;
            auto waitUserFenceInput = static_cast<drm_xe_wait_user_fence *>(arg);
            waitUserFenceInputs.push_back(*waitUserFenceInput);
        } break;

        case DrmIoctl::gemContextCreateExt: {
            auto queueCreate = static_cast<drm_xe_exec_queue_create *>(arg);

            auto extension = queueCreate->extensions;
            while (extension) {
                auto ext = reinterpret_cast<drm_xe_user_extension *>(extension);
                if (ext->name == DRM_XE_EXEC_QUEUE_EXTENSION_SET_PROPERTY) {
                    auto setProperty = reinterpret_cast<drm_xe_ext_set_property *>(ext);
                    execQueueProperties.push_back(*setProperty);
                }
                extension = ext->next_extension;
            }
            queueCreate->exec_queue_id = mockExecQueueId;
            ret = 0;
        } break;
        case DrmIoctl::gemContextDestroy: {
            auto queueDestroy = static_cast<drm_xe_exec_queue_destroy *>(arg);
            if (queueDestroy->exec_queue_id == mockExecQueueId) {
                ret = 0;
            }
        } break;
        case DrmIoctl::gemContextSetparam:
        case DrmIoctl::gemContextGetparam:

        default:
            break;
        }
        return ret;
    }

    void addMockedQueryTopologyData(uint16_t tileId, uint16_t maskType, uint32_t nBytes, const std::vector<uint8_t> &mask) {

        ASSERT_EQ(nBytes, mask.size());

        auto additionalSize = 8u + nBytes;
        auto oldSize = queryTopology.size();
        auto newSize = oldSize + additionalSize;
        queryTopology.resize(newSize, 0u);

        uint8_t *dataPtr = queryTopology.data() + oldSize;

        drm_xe_query_topology_mask *topo = reinterpret_cast<drm_xe_query_topology_mask *>(dataPtr);
        topo->gt_id = tileId;
        topo->type = maskType;
        topo->num_bytes = nBytes;

        memcpy_s(reinterpret_cast<void *>(topo->mask), nBytes, mask.data(), nBytes);
    }

    int forceIoctlAnswer = 0;
    int setIoctlAnswer = 0;
    int gemVmBindReturn = 0;
    GemClose passedGemClose{};
    int gemCloseCalled = 0;

    const uint16_t revId = 0x12;
    const uint16_t devId = 0xabc;

    uint64_t queryConfig[6]{}; // 1 qword for num params and 1 qwords per param
    static constexpr uint32_t mockExecQueueId = 1234;
    static constexpr int32_t mockMaxExecQueuePriority = 3;

    static_assert(sizeof(drm_xe_engine) == 4 * sizeof(uint64_t), "");
    uint64_t queryEngines[45]{}; // 1 qword for num engines and 4 qwords per engine
    static_assert(sizeof(drm_xe_mem_region) == 11 * sizeof(uint64_t), "");
    uint64_t queryMemUsage[34]{}; // 1 qword for num regions and 11 qwords per region
    static_assert(sizeof(drm_xe_gt) == 12 * sizeof(uint64_t), "");
    StackVec<uint64_t, 37> queryGtList{}; // 1 qword for num gts and 12 qwords per gt
    alignas(64) std::vector<uint8_t> queryTopology;
    static_assert(sizeof(drm_xe_query_engine_cycles) == 5 * sizeof(uint64_t), "");
    uint64_t queryEngineCycles[5]{}; // 1 qword for eci and 4 qwords
    StackVec<drm_xe_wait_user_fence, 1> waitUserFenceInputs;
    StackVec<drm_xe_vm_bind, 1> vmBindInputs;
    StackVec<drm_xe_sync, 1> syncInputs;
    StackVec<drm_xe_ext_set_property, 1> execQueueProperties;

    int waitUserFenceReturn = 0;
    uint32_t createParamsFlags = 0u;
    uint16_t createParamsCpuCaching = 0u;
    uint32_t createParamsPlacement = 0u;
    bool ioctlCalled = false;
};
