//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <mutex>

#include "reader.h"

namespace vireo {
namespace common {

using namespace std;

struct _Reader {
  mutex lock;
  uint32_t offset = 0;
  const uint32_t size;
  common::Data32 data;
  std::function<common::Data32(const uint32_t offset, const uint32_t size)> read_func;

  // opaque, read_func, seek_func used to interface with l-smash and ffmpeg
  const void* opaque = (void*)this;
  int (*const read_callback)(void*, uint8_t*, int) = [](void* opaque, uint8_t* buffer, int size) -> int {
    _Reader& reader = *(_Reader*)opaque;
    lock_guard<mutex>(reader.lock);
    if (reader.offset >= reader.size) {
      return 0;
    }
    const uint32_t read_size = std::min((uint32_t)size, reader.size - reader.offset);
    if (read_size) {
      auto data = reader.read_func(reader.offset, read_size);
      CHECK(data.count() == read_size);
      memcpy(buffer, data.data(), read_size);
      reader.offset += read_size;
    }
    return (int)read_size;
  };
  int64_t (*const seek_callback)(void*, int64_t, int) = [](void* opaque, int64_t offset, int whence) -> int64_t {
    _Reader& reader = *(_Reader*)opaque;
    lock_guard<mutex>(reader.lock);
    if (whence == SEEK_SET) {
      CHECK(offset >= 0);
      reader.offset = (uint32_t)offset;
    } else if (whence == SEEK_CUR) {
      CHECK((int64_t)reader.offset + offset >= 0);
      reader.offset = (uint32_t)((int64_t)reader.offset + offset);
    } else if (whence == SEEK_END) {
      CHECK((int64_t)reader.size + offset >= 0);
      reader.offset = (uint32_t)((int64_t)reader.size + offset);
    }
    return std::min(reader.offset, reader.size);
  };

  _Reader(common::Data32&& data) : data(move(data)), size(data.count()), read_func(
    [data = &this->data](const uint32_t offset, const uint32_t size) -> common::Data32 {
      THROW_IF(offset + size > data->count(), OutOfRange);
      return common::Data32(data->data() + data->a() + offset, size, nullptr);
    }) {}

  _Reader(const uint32_t size, std::function<common::Data32(const uint32_t offset, const uint32_t size)> read_func) : size(size), read_func(read_func) {}
};

Reader::Reader(common::Data32&& data) : _this(make_shared<_Reader>(move(data))), opaque(_this->opaque), read_callback(_this->read_callback), seek_callback(_this->seek_callback) {
  CHECK(_this->data.count());
}

Reader::Reader(int file_descriptor, std::function<void(int file_descriptor)> deleter) : Reader(common::Data32(file_descriptor, deleter)) {}

Reader::Reader(const std::string& path) : Reader(common::Data32(path)) {}

Reader::Reader(Reader&& reader) : _this(reader._this), opaque(_this->opaque), read_callback(_this->read_callback), seek_callback(_this->seek_callback) {
  reader._this = nullptr;
}

Reader::Reader(const uint32_t size, std::function<common::Data32(const uint32_t offset, const uint32_t size)> read_func)
  : _this(make_shared<_Reader>(size, read_func)), opaque(_this->opaque), read_callback(_this->read_callback), seek_callback(_this->seek_callback) {}

auto Reader::read(uint32_t offset, uint32_t size) -> common::Data32 {
  return _this->read_func(offset, size);
}

auto Reader::size() const -> uint32_t {
  return _this->size;
}

}}