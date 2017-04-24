//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
