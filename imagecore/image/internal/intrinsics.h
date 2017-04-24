//
//  intrinsics.h
//  ImageCore
//
//  Created by Paul Melamed on 3/24/15.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

const int kHalf16 = (1 << 15) - 1;
const int kHalf22 = (1 << 21) - 1;

#define V128_SHUFFLE(z, y, x, w) (((z) << 6) | ((y) << 4) | ((x) << 2) | (w))
#define ZMASK 128

#if __SSE4_1__

#pragma GCC diagnostic ignored "-Wconversion"
#include "intrinsics_sse.h"

#elif __ARM_NEON__

#include "intrinsics_neon.h"

#endif
