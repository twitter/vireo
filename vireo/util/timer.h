//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

namespace vireo {
namespace util {

class PUBLIC Timer final {
  bool initialized = false;
  std::chrono::time_point<std::chrono::high_resolution_clock> start;
  uint64_t _total = 0;
public:
  auto mark() -> uint64_t;
  auto now() -> void;
  auto reset() -> void;
  auto total() -> void;
};

static Timer timer;

}}
