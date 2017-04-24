//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include "vireo/error/error.h"
#include "vireo/periscope/util.h"

namespace vireo {
namespace periscope {

const static uint8_t kID3FrameHeaderSize = 10;
const static vector<uint8_t> kTIT3 = { 'T', 'I', 'T', '3' };
const static vector<uint8_t> kTKEY = { 'T', 'K', 'E', 'Y' };

struct Helper {
  // checks if next bytes match the id
  static auto StartsWith(const common::Data32& data, const vector<uint8_t> bytes) -> bool {
    if (data.count() >= bytes.size()) {
      for (auto i = 0; i < bytes.size(); ++i) {
        if (data(data.a() + i) != bytes[i]) {
          return false;
        }
      }
      return true;
    }
    return false;
  };

  // checks if current byte is a digit in ASCII
  static auto IsDigit(const common::Data32& data) -> bool {
    const uint32_t offset = data.a();
    if (data.count()) {
      if (data(offset) >= '0' && data(offset) <= '9') {
        return true;
      }
    }
    return false;
  };

  // checks if text encoding is UTF-8 and parses the double using atof
  static auto ParseUTF8Double(const common::Data32& _data) -> double {
    // check text encoding description (first byte) is UTF-8 (0x03)
    THROW_IF(_data(_data.a()) != 0x03, Unsupported, "Only UTF-8 text encoding is supported");
    auto data = common::Data32(_data.data() + _data.a() + 1, _data.count() - 1, nullptr);
    // check if string data conforms with expectations of atof
    THROW_IF(!IsDigit(data), Invalid, "Text in ID3 frame is not parseable to double");
    THROW_IF(data(data.b() - 1) != '\0', Invalid, "Text in ID3 frame is not parseable to double")
    return atof((char*)data.data());
  };

  // parses ntp timestamp from TIT3 frame
  static auto ParseTIT3(const common::Data32& _data) -> double {
    THROW_IF(!StartsWith(_data, kTIT3), InvalidArguments);
    CHECK(_data.count() > kID3FrameHeaderSize + 1);  // header + last '\0'
    auto data = common::Data32(_data.data() + _data.a(), _data.count(), nullptr);
    data.set_bounds(data.a() + kID3FrameHeaderSize, data.b());
    double ntp_timestamp = ParseUTF8Double(data);
    CHECK(ntp_timestamp >= 0);
    return ntp_timestamp;
  };

  // parses orientation from TKEY frame
  static auto ParseTKEY(const common::Data32& _data) -> settings::Video::Orientation {
    THROW_IF(!StartsWith(_data, kTKEY), InvalidArguments);
    CHECK(_data.count() > kID3FrameHeaderSize + 1);  // header + last '\0'
    auto data = common::Data32(_data.data() + _data.a(), _data.count(), nullptr);
    data.set_bounds(data.a() + kID3FrameHeaderSize, data.b());
    double degree = ParseUTF8Double(data);
    // make sure returned value is between [0, 360)
    while (degree < 0) {
      degree += 360;
    }
    degree = (uint32_t)degree % 360;
    return (settings::Video::Orientation)((uint32_t)round(degree / 90) % 4);
  };

  // skips 4-bytes for frame id + parses a next 4-bytes as 32 bit synchsafe integer
  static auto FrameSize(const common::Data32& data) -> uint32_t {
    CHECK(data.count() >= 8);
    uint32_t a = data.a();
    return data(a + 4) << 21 | data(a + 5) << 14 | data(a + 6) << 7 | data(a + 7);
  }
};

auto Util::ParseID3Info(const common::Data32& id3_data) -> ID3Info {
  ID3Info info;
  auto data = common::Data32(id3_data.data() + id3_data.a(), id3_data.count(), nullptr);
  while (data.count()) {
    if (Helper::StartsWith(data, kTIT3)) {
      uint32_t size = Helper::FrameSize(data) + kID3FrameHeaderSize;
      auto tit3_data = common::Data32(data.data() + data.a(), size, nullptr);
      info.ntp_timestamp = Helper::ParseTIT3(tit3_data);
      data.set_bounds(data.a() + size, data.b());
    }
    if (Helper::StartsWith(data, kTKEY)) {
      uint32_t size = Helper::FrameSize(data) + kID3FrameHeaderSize;
      auto tkey_data = common::Data32(data.data() + data.a(), size, nullptr);
      info.orientation = Helper::ParseTKEY(tkey_data);
      data.set_bounds(data.a() + size, data.b());
    }
    data.set_bounds(data.a() + 1, data.b());
  }
  return info;
}

}}