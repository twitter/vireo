/*
 * MIT License
 *
 * Copyright (c) 2017 Twitter
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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
