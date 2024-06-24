/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/built_ins.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/api_specific_config.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/path.h"

#include "os_inc.h"

#include <cstdint>
#include <fstream>
#include <sstream>

namespace NEO {

const char *getBuiltinAsString(EBuiltInOps::Type builtin) {
    const char *builtinString = getAdditionalBuiltinAsString(builtin);
    if (builtinString) {
        return builtinString;
    }
    switch (builtin) {
    default:
        return "unknown";
    case EBuiltInOps::auxTranslation:
        return "aux_translation.builtin_kernel";
    case EBuiltInOps::copyBufferToBuffer:
        return "copy_buffer_to_buffer.builtin_kernel";
    case EBuiltInOps::copyBufferToBufferStateless:
    case EBuiltInOps::copyBufferToBufferStatelessHeapless:
        return "copy_buffer_to_buffer_stateless.builtin_kernel";
    case EBuiltInOps::copyBufferRect:
        return "copy_buffer_rect.builtin_kernel";
    case EBuiltInOps::copyBufferRectStateless:
    case EBuiltInOps::copyBufferRectStatelessHeapless:
        return "copy_buffer_rect_stateless.builtin_kernel";
    case EBuiltInOps::fillBuffer:
        return "fill_buffer.builtin_kernel";
    case EBuiltInOps::fillBufferStateless:
    case EBuiltInOps::fillBufferStatelessHeapless:
        return "fill_buffer_stateless.builtin_kernel";
    case EBuiltInOps::copyBufferToImage3d:
        return "copy_buffer_to_image3d.builtin_kernel";
    case EBuiltInOps::copyBufferToImage3dStateless:
        return "copy_buffer_to_image3d_stateless.builtin_kernel";
    case EBuiltInOps::copyImage3dToBuffer:
        return "copy_image3d_to_buffer.builtin_kernel";
    case EBuiltInOps::copyImage3dToBufferStateless:
        return "copy_image3d_to_buffer_stateless.builtin_kernel";
    case EBuiltInOps::copyImageToImage1d:
        return "copy_image_to_image1d.builtin_kernel";
    case EBuiltInOps::copyImageToImage2d:
        return "copy_image_to_image2d.builtin_kernel";
    case EBuiltInOps::copyImageToImage3d:
        return "copy_image_to_image3d.builtin_kernel";
    case EBuiltInOps::fillImage1d:
        return "fill_image1d.builtin_kernel";
    case EBuiltInOps::fillImage2d:
        return "fill_image2d.builtin_kernel";
    case EBuiltInOps::fillImage3d:
        return "fill_image3d.builtin_kernel";
    case EBuiltInOps::queryKernelTimestamps:
        return "copy_kernel_timestamps.builtin_kernel";
    };
}

BuiltinResourceT createBuiltinResource(const char *ptr, size_t size) {
    return BuiltinResourceT(ptr, ptr + size);
}

BuiltinResourceT createBuiltinResource(const BuiltinResourceT &r) {
    return BuiltinResourceT(r);
}

std::string createBuiltinResourceName(EBuiltInOps::Type builtin, const std::string &extension) {
    return getBuiltinAsString(builtin) + extension;
}

StackVec<std::string, 3> getBuiltinResourceNames(EBuiltInOps::Type builtin, BuiltinCode::ECodeType type, const Device &device) {
    auto &hwInfo = device.getHardwareInfo();
    auto &productHelper = device.getRootDeviceEnvironment().getHelper<ProductHelper>();
    auto releaseHelper = device.getReleaseHelper();

    auto createDeviceIdFilenameComponent = [](const NEO::HardwareIpVersion &hwIpVersion) {
        std::ostringstream deviceId;
        deviceId << hwIpVersion.architecture << "_" << hwIpVersion.release << "_" << hwIpVersion.revision;
        return deviceId.str();
    };
    const auto deviceIp = createDeviceIdFilenameComponent(hwInfo.ipVersion);
    const auto builtinFilename = getBuiltinAsString(builtin);
    const auto extension = BuiltinCode::getExtension(type);
    auto getAddressingModePrefix = [type, &productHelper, releaseHelper, builtin]() {
        if (type == BuiltinCode::ECodeType::binary) {
            const bool requiresStatelessAddressing = (false == productHelper.isStatefulAddressingModeSupported());
            const bool builtInUsesStatelessAddressing = EBuiltInOps::isStateless(builtin);
            const bool heaplessEnabled = EBuiltInOps::isHeapless(builtin);
            if (builtInUsesStatelessAddressing || requiresStatelessAddressing) {
                return heaplessEnabled ? "stateless_heapless_" : "stateless_";
            } else if (ApiSpecificConfig::getBindlessMode(releaseHelper)) {
                return "bindless_";
            } else {
                return "bindful_";
            }
        }
        return "";
    };
    const auto addressingModePrefix = getAddressingModePrefix();

    auto createBuiltinResourceName = [](ConstStringRef deviceIpPath, ConstStringRef addressingModePrefix, ConstStringRef builtinFilename, ConstStringRef extension) {
        std::ostringstream outResourceName;
        if (false == deviceIpPath.empty()) {
            outResourceName << deviceIpPath.str() << "_";
        }
        outResourceName << addressingModePrefix.str() << builtinFilename.str() << extension.str();
        return outResourceName.str();
    };
    StackVec<std::string, 3> resourcesToLookup = {};
    resourcesToLookup.push_back(createBuiltinResourceName(deviceIp, addressingModePrefix, builtinFilename, extension));

    if (BuiltinCode::ECodeType::binary != type) {
        resourcesToLookup.push_back(createBuiltinResourceName("", addressingModePrefix, builtinFilename, extension));
    }
    return resourcesToLookup;
}

std::string getDriverInstallationPath() {
    return "";
}

BuiltinResourceT Storage::load(const std::string &resourceName) {
    return loadImpl(joinPath(rootPath, resourceName));
}

BuiltinResourceT FileStorage::loadImpl(const std::string &fullResourceName) {
    BuiltinResourceT ret;

    std::ifstream f{fullResourceName, std::ios::in | std::ios::binary | std::ios::ate};
    auto end = f.tellg();
    f.seekg(0, std::ios::beg);
    auto beg = f.tellg();
    auto s = end - beg;
    ret.resize(static_cast<size_t>(s));
    f.read(ret.data(), s);
    return ret;
}

const BuiltinResourceT *EmbeddedStorageRegistry::get(const std::string &name) const {
    auto it = resources.find(name);
    if (resources.end() == it) {
        return nullptr;
    }

    return &it->second;
}

BuiltinResourceT EmbeddedStorage::loadImpl(const std::string &fullResourceName) {
    auto *constResource = EmbeddedStorageRegistry::getInstance().get(fullResourceName);
    if (constResource == nullptr) {
        BuiltinResourceT ret;
        return ret;
    }

    return createBuiltinResource(*constResource);
}

BuiltinsLib::BuiltinsLib() {
    allStorages.push_back(std::unique_ptr<Storage>(new EmbeddedStorage("")));
    allStorages.push_back(std::unique_ptr<Storage>(new FileStorage(getDriverInstallationPath())));
}

BuiltinCode BuiltinsLib::getBuiltinCode(EBuiltInOps::Type builtin, BuiltinCode::ECodeType requestedCodeType, Device &device) {
    std::lock_guard<std::mutex> lockRaii{mutex};

    BuiltinResourceT bc;
    BuiltinCode::ECodeType usedCodetType = BuiltinCode::ECodeType::invalid;

    if (requestedCodeType == BuiltinCode::ECodeType::any) {
        uint32_t codeType = static_cast<uint32_t>(BuiltinCode::ECodeType::binary);
        if (debugManager.flags.RebuildPrecompiledKernels.get()) {
            codeType = static_cast<uint32_t>(BuiltinCode::ECodeType::source);
        }
        for (uint32_t e = static_cast<uint32_t>(BuiltinCode::ECodeType::count);
             codeType != e; ++codeType) {
            bc = getBuiltinResource(builtin, static_cast<BuiltinCode::ECodeType>(codeType), device);
            if (bc.size() > 0) {
                usedCodetType = static_cast<BuiltinCode::ECodeType>(codeType);
                break;
            }
        }
    } else {
        bc = getBuiltinResource(builtin, requestedCodeType, device);
        usedCodetType = requestedCodeType;
    }

    BuiltinCode ret;
    std::swap(ret.resource, bc);
    ret.type = usedCodetType;
    ret.targetDevice = &device;

    return ret;
}

BuiltinResourceT BuiltinsLib::getBuiltinResource(EBuiltInOps::Type builtin, BuiltinCode::ECodeType requestedCodeType, Device &device) {
    BuiltinResourceT builtinResource;
    auto resourcesToLookup = getBuiltinResourceNames(builtin, requestedCodeType, device);
    for (auto &resourceName : resourcesToLookup) {
        for (auto &storage : allStorages) {
            builtinResource = storage->load(resourceName);
            if (builtinResource.size() != 0) {
                return builtinResource;
            }
        }
    }
    return builtinResource;
}

} // namespace NEO
