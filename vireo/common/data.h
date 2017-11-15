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
#include "vireo/domain/interval.hpp"

namespace vireo {
namespace common {

template <typename Y, typename X>
class Data;
template <typename Y, typename X>
struct _Data;

template <typename Y, typename X>
PUBLIC void operator<<(std::ostream& os, const Data<Y, X>& obj);
template <typename Y, typename X>
PUBLIC void operator<<(FILE* out, const Data<Y, X>& obj);
template <typename Y, typename X>
PUBLIC void operator<<(int out_fd, const Data<Y, X>& obj);

template <typename Y, typename X>
class PUBLIC Data : public domain::Interval<Data<Y, X>, Y, X> {
  _Data<Y, X>* _this = NULL;
public:
  Data() : Data(0) {};
  Data(X length) : Data(nullptr, length, nullptr) {};  // empty with length
  Data(const Y* bytes, X length, std::function<void(Y*)> deleter);
  Data(int file_descriptor, std::function<void(int file_descriptor)> deleter);  // Memory mapped
  Data(const std::string& path);
  Data(Data&& data) noexcept;
  Data(const Data& data) noexcept;
  virtual ~Data();
  auto operator=(Data&& x) -> Data&;
  auto operator=(const Data& x) -> Data&;
  auto operator==(const Data& x) const -> bool;
  auto operator!=(const Data& x) const -> bool;
  auto operator()(X x) const -> Y;
  auto data() const -> const Y*&&;
  auto capacity() const -> X&&;
  auto copy(const Data& x) -> void;
  static Data None;
};

#ifndef ANDROID
typedef Data<uint8_t, size_t> Data64;
#endif
typedef Data<uint8_t, uint32_t> Data32;
typedef Data<uint8_t, uint16_t> Data16;
typedef Data<int16_t, uint32_t> Sample16;
typedef Data<uint32_t, uint16_t> Pixel16; // used in ckoia
}}
