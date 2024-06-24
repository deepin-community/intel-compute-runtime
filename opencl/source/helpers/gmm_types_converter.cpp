/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/helpers/gmm_types_converter.h"

#include "shared/source/helpers/get_info.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/graphics_allocation.h"

#include "GL/gl.h"
#include "GL/glext.h"

using namespace NEO;

void GmmTypesConverter::queryImgFromBufferParams(ImageInfo &imgInfo, GraphicsAllocation *gfxAlloc) {
    // 1D or 2D from buffer
    if (imgInfo.imgDesc.imageRowPitch > 0) {
        imgInfo.rowPitch = imgInfo.imgDesc.imageRowPitch;
    } else {
        imgInfo.rowPitch = getValidParam(imgInfo.imgDesc.imageWidth) * imgInfo.surfaceFormat->imageElementSizeInBytes;
    }
    imgInfo.slicePitch = imgInfo.rowPitch * getValidParam(imgInfo.imgDesc.imageHeight);
    imgInfo.size = gfxAlloc->getUnderlyingBufferSize();
    imgInfo.qPitch = 0;
}

uint32_t GmmTypesConverter::getRenderMultisamplesCount(uint32_t numSamples) {
    if (numSamples == 2) {
        return 1;
    } else if (numSamples == 4) {
        return 2;
    } else if (numSamples == 8) {
        return 3;
    } else if (numSamples == 16) {
        return 4;
    }
    return 0;
}

GMM_YUV_PLANE GmmTypesConverter::convertPlane(ImagePlane imagePlane) {
    if (imagePlane == ImagePlane::planeY) {
        return GMM_PLANE_Y;
    } else if (imagePlane == ImagePlane::planeU || imagePlane == ImagePlane::planeUV) {
        return GMM_PLANE_U;
    } else if (imagePlane == ImagePlane::planeV) {
        return GMM_PLANE_V;
    }

    return GMM_NO_PLANE;
}

GMM_CUBE_FACE_ENUM GmmTypesConverter::getCubeFaceIndex(uint32_t target) {
    switch (target) {
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
        return __GMM_CUBE_FACE_NEG_X;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_X:
        return __GMM_CUBE_FACE_POS_X;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
        return __GMM_CUBE_FACE_NEG_Y;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
        return __GMM_CUBE_FACE_POS_Y;
    case GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        return __GMM_CUBE_FACE_NEG_Z;
    case GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
        return __GMM_CUBE_FACE_POS_Z;
    }
    return __GMM_NO_CUBE_MAP;
}
