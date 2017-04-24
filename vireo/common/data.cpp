//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <fcntl.h>
#include <iomanip>
#include <sys/mman.h>
#include <sys/stat.h>

#include "vireo/base_cpp.h"
#include "vireo/common/data.h"
#include "vireo/error/error.h"

namespace vireo {
namespace common {

template <typename Y, typename X>
Data<Y, X> Data<Y, X>::None = common::Data<Y, X>(0);

template <typename Y, typename X>
struct _Data {
  const Y* bytes;
  X capacity;
  function<void(Y*)> deleter;
  void clear() {
    if (deleter) {
      deleter((Y*)bytes);
      bytes = nullptr;
    }
  }
};

template <typename Y, typename X>
Data<Y, X>::Data(const Y* bytes, X length, function<void(Y*)> deleter)
  : domain::Interval<Data<Y, X>, Y, X>(0, length) {
  _this = new _Data<Y, X>();
  _this->bytes = bytes;
  _this->capacity = length;
  _this->deleter = deleter;
}

template <typename Y, typename X>
Data<Y, X>::Data(int fd, std::function<void(int fd)> deleter)
  : domain::Interval<Data<Y, X>, Y, X>(0, 0) {
  struct stat sb;
  CHECK(fstat(fd, &sb) != -1);
  size_t size = (size_t)sb.st_size;
  CHECK(size != 0);
  THROW_IF(size > std::numeric_limits<X>::max(), Unsupported, "File size too large");
  int fd_copy = dup(fd);
  CHECK(fd_copy != 0);
  this->set_bounds(0, (X)size);
  _this = new _Data<Y, X>();
  auto bytes = (const Y*)mmap(nullptr, size, PROT_READ, MAP_SHARED, fd_copy, 0);
  CHECK(bytes != MAP_FAILED);
  _this->bytes = bytes;
  _this->capacity = (X)size;
  _this->deleter = [fd_copy, size, deleter, fd](Y* p) { munmap(p, size); close(fd_copy); if (deleter) { deleter(fd); } };
}

template <typename Y, typename X>
Data<Y, X>::Data(const string& path)
  : Data(open(path.c_str(), O_RDONLY), [](int file_descriptor) { close(file_descriptor); }) {}

template <typename Y, typename X>
Data<Y, X>::Data(Data&& data) noexcept
  : domain::Interval<Data<Y, X>, Y, X>(data.a(), data.b()) {
  delete _this;
  _this = data._this;
  data._this = nullptr;
}

template <typename Y, typename X>
Data<Y, X>::Data(const Data& data) noexcept
  : domain::Interval<Data<Y, X>, Y, X>(0, data.count()) {
  _this = new _Data<Y, X>();
  _this->capacity = data.count();
  if (data.data()) {
    _this->bytes = new Y[_this->capacity];
    _this->deleter = [](Y* p) { delete[] p; };
    memcpy((Y*)_this->bytes, data.data() + data.a(), data.count() * sizeof(Y));
  } else {
    _this->bytes = nullptr;
    _this->deleter = nullptr;
  }
}

template <typename Y, typename X>
Data<Y, X>::~Data() {
  if (_this && _this->deleter) {
    _this->deleter((Y*)_this->bytes);
  }
  delete _this;
}

template <typename Y, typename X>
auto Data<Y, X>::operator=(const Data& data) -> Data& {
  CHECK(_this && data._this);
  if (_this->deleter) {
    _this->deleter((Y*)_this->bytes);
  }
  _this->capacity = data.count();
  if (data.data()) {
    _this->bytes = new Y[_this->capacity];
    _this->deleter = [](Y* p) { delete[] p; };
    memcpy((Y*)_this->bytes, data.data() + data.a(), data.count() * sizeof(Y));
  } else {
    _this->bytes = nullptr;
    _this->deleter = nullptr;
  }
  this->set_bounds(0, _this->capacity);
  return *this;
}

template <typename Y, typename X>
auto Data<Y, X>::operator=(Data&& data) -> Data& {
  CHECK(_this && data._this);
  this->set_bounds(data.a(), data.b());
  if (_this->deleter) {
    _this->deleter((Y*)_this->bytes);
  }
  delete _this;
  _this = data._this;
  data._this = nullptr;
  return *this;
}

template <typename Y, typename X>
auto Data<Y, X>::operator==(const Data& data) const -> bool {
  CHECK(_this && data._this);
  if (this->count() != data.count()) {
    return false;
  }
  if (this->data() == nullptr && data.data() == nullptr) {
    return true;
  }
  return memcmp((Y*)_this->bytes + this->a(), (Y*)data.data() + data.a(), this->count()) == 0;
}

template <typename Y, typename X>
auto Data<Y, X>::operator!=(const Data& data) const -> bool {
  return !(*this == data);
}

template <typename Y, typename X>
auto Data<Y, X>::operator()(X x) const -> Y {
  CHECK(_this);
  CHECK(x < _this->capacity);
  return _this->bytes[x];
}

template <typename Y, typename X>
auto Data<Y, X>::data() const -> const Y*&& {
  CHECK(_this);
  return move(_this->bytes);
}

template <typename Y, typename X>
auto Data<Y, X>::capacity() const -> X&& {
  CHECK(_this);
  return move(_this->capacity);
}

template <typename Y, typename X>
auto Data<Y, X>::copy(const Data& data) -> void {
  CHECK(_this && _this->bytes && data.data());
  THROW_IF((data.count() + this->a()) > _this->capacity, OutOfRange);
  this->set_bounds(this->a(), data.count() + this->a());
  memcpy((void*)(_this->bytes + this->a()), data._this->bytes + data.a(), data.count() * sizeof(Y));
}

// We can't access private Data fields, as declaring the operators as 'friend' breaks
// explicit instantiation on clang (gcc ok).
template <typename Y, typename X>
void operator<<(std::ostream& os, const Data<Y, X>& obj) {
  CHECK(obj.data() && obj.b());
  os.write((const char*)obj.data(), obj.b());
  CHECK((bool)os);
}

template <typename Y, typename X>
void operator<<(FILE* out, const Data<Y, X>& obj) {
  CHECK(obj.data() && obj.b());
  CHECK(fwrite((const char*)obj.data(), 1, obj.b(), out) == obj.b());
}

template <typename Y, typename X>
void operator<<(int out_fd, const Data<Y, X>& obj) {
  CHECK(obj.data() && obj.b());
  CHECK(write(out_fd, obj.data(), obj.b()) == obj.b());
}

#define DATA_EXPLICIT_INSTANTIATION(Y, X)\
template class Data<Y, X>;\
template void operator<<(std::ostream& os, const Data<Y, X>& obj);\
template void operator<<(FILE* out, const Data<Y, X>& obj);\
template void operator<<(int out_fd, const Data<Y, X>& obj);

#ifndef ANDROID
DATA_EXPLICIT_INSTANTIATION(uint8_t, size_t); // Data64
#endif
DATA_EXPLICIT_INSTANTIATION(uint8_t, uint32_t); // Data32
DATA_EXPLICIT_INSTANTIATION(uint8_t, uint16_t); // Data16
DATA_EXPLICIT_INSTANTIATION(int16_t, uint32_t); // Sample16
DATA_EXPLICIT_INSTANTIATION(int32_t, uint16_t); // Pixel16
}}
