/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/ioctl_helper_xe.h"

namespace NEO {

unsigned int IoctlHelperXe::getIoctlRequestValueDebugger(DrmIoctl ioctlRequest) const {
    UNRECOVERABLE_IF(true);
    return 0;
}

int IoctlHelperXe::debuggerOpenIoctl(DrmIoctl request, void *arg) {
    UNRECOVERABLE_IF(true);
    return 0;
}

int IoctlHelperXe::debuggerMetadataCreateIoctl(DrmIoctl request, void *arg) {
    UNRECOVERABLE_IF(true);
    return 0;
}

int IoctlHelperXe::debuggerMetadataDestroyIoctl(DrmIoctl request, void *arg) {
    UNRECOVERABLE_IF(true);
    return 0;
}

void *IoctlHelperXe::allocateDebugMetadata() {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

void *IoctlHelperXe::freeDebugMetadata(void *metadata) {
    UNRECOVERABLE_IF(true);
    return nullptr;
}

void IoctlHelperXe::addDebugMetadataCookie(uint64_t cookie) {
}

void IoctlHelperXe::addDebugMetadata(DrmResourceClass type, uint64_t *offset, uint64_t size) {
}

int IoctlHelperXe::getRunaloneExtProperty() {
    UNRECOVERABLE_IF(true);
    return 0;
}

int IoctlHelperXe::getEuDebugSysFsEnable() {
    UNRECOVERABLE_IF(true);
    return 0;
}

uint32_t IoctlHelperXe::registerResource(DrmResourceClass classType, const void *data, size_t size) {
    UNRECOVERABLE_IF(true);
    return 0;
}

void IoctlHelperXe::unregisterResource(uint32_t handle) {
    UNRECOVERABLE_IF(true);
}

std::unique_ptr<uint8_t[]> IoctlHelperXe::prepareVmBindExt(const StackVec<uint32_t, 2> &bindExtHandles) {
    return {};
}

} // namespace NEO
