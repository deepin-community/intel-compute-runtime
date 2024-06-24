/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

using ClCreateFromGLBuffer_ = ApiTests;

namespace ULT {

TEST_F(ClCreateFromGLBuffer_, givenNullConxtextWhenCreateFromGLIsCalledThenErrorIsReturned) {
    int errCode = CL_SUCCESS;
    auto retVal = clCreateFromGLBuffer(nullptr,           // cl_context context
                                       CL_MEM_READ_WRITE, // cl_mem_flags flags
                                       0,                 // cl_GLuint bufobj
                                       &errCode           // cl_int * errcode_ret
    );
    EXPECT_EQ(nullptr, retVal);
    EXPECT_EQ(errCode, CL_INVALID_CONTEXT);
}
} // namespace ULT
