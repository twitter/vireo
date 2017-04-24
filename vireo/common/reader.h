//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/data.h"

namespace vireo {
namespace common {

using namespace std;

class PUBLIC Reader final {
  std::shared_ptr<struct _Reader> _this = nullptr;
public:
  Reader(common::Data32&& data);
  Reader(int file_descriptor, std::function<void(int file_descriptor)> deleter = NULL);  // Memory mapped
  Reader(const std::string& path);
  Reader(Reader&& reader);
  Reader(const uint32_t size, std::function<common::Data32(const uint32_t offset, const uint32_t size)> read_func);
  auto read(uint32_t offset, uint32_t size) -> common::Data32;
  auto size() const -> uint32_t;
  DISALLOW_COPY_AND_ASSIGN(Reader);
  const void* opaque;
  int(*const read_callback)(void*, uint8_t*, int);
  int64_t(*const seek_callback)(void*, int64_t, int);
};

}}
