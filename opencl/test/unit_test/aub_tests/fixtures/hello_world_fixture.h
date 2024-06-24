/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "opencl/test/unit_test/aub_tests/command_stream/aub_command_stream_fixture.h"
#include "opencl/test/unit_test/aub_tests/fixtures/simple_arg_fixture.h"
#include "opencl/test/unit_test/fixtures/hello_world_fixture.h"
#include "opencl/test/unit_test/fixtures/simple_arg_fixture.h"

namespace NEO {

////////////////////////////////////////////////////////////////////////////////
// Factory where all command stream traffic funnels to an AUB file
////////////////////////////////////////////////////////////////////////////////
struct AUBHelloWorldFixtureFactory {
    typedef NEO::IndirectHeapFixture IndirectHeapFixture;
    typedef NEO::HelloWorldKernelFixture KernelFixture;
};
} // namespace NEO
