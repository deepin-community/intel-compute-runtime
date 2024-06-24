/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

namespace NEO {
#if defined(_WIN32)
unsigned int ultIterationMaxTimeInS = 90;
#else
unsigned int ultIterationMaxTimeInS = 45;
#endif
const char *executionName = "OCLOC";
} // namespace NEO
