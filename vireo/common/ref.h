//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include <atomic>

#include "vireo/base_h.h"
#include "vireo/error/error.h"

namespace vireo {
namespace common {

// For internal use.
template <typename C>
struct Ref {
  std::atomic<int> count = ATOMIC_VAR_INIT(1);
  virtual ~Ref() {};
  auto ref() -> C* {
    std::atomic_fetch_add(&count, 1);
    return reinterpret_cast<C*>(this);
  }
  auto unref() -> void {
    int ref_count = std::atomic_fetch_sub(&count, 1);
    CHECK(ref_count >= 1);
    if (ref_count == 1) {
      delete this;
    }
  }
};

}}
