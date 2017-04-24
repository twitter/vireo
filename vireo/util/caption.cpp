//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include "vireo/util/caption.h"
#include "vireo/internal/decode/types.h"
#include "vireo/internal/decode/util.h"

#define EMULATION_PREVENTION_BYTE 0x03
#define USER_DATA_REGISTERED_ITU_T_T35 0x04
#define RBSP_TRAILING_BITS 0x80

namespace vireo {
namespace util {

struct SEIPayloadInfo {
  uint32_t payload_type;
  uint32_t payload_size;
  decode::ByteRange payload_range;
  SEIPayloadInfo(uint32_t type, uint32_t size, decode::ByteRange range) : payload_type(type), payload_size(size), payload_range(range) {};
};

vector<decode::ByteRange> get_caption_ranges(const common::Data32& sei_data) {
  vector<decode::ByteRange> caption_ranges;
  uint32_t index = 1; // skip the nalu type byte

  auto next_sei_info = [&sei_data, &index]() -> SEIPayloadInfo {
    uint32_t payload_type = 0;
    int32_t payload_size = 0;

    // get payload type
    uint32_t start = sei_data.a() + index;
    while (sei_data(sei_data.a() + index) == 0xFF) { // in case the value is over 1 byte long
      payload_type += 255;
      index++;
    }
    payload_type += sei_data(sei_data.a() + index);
    index++;

    // get payload size
    while (sei_data(sei_data.a() + index) == 0xFF) {
      payload_size += 255;
      index++;
    }
    payload_size += sei_data(sei_data.a() + index);
    index++;

    // skip emulation prevention byte
    for (int32_t i = 0; i < payload_size - 2; i++) {
      if ((sei_data(sei_data.a() + index + i) == 0x00) &&
          (sei_data(sei_data.a() + index + i + 1) == 0x00) &&
          (sei_data(sei_data.a() + index + i + 2) == EMULATION_PREVENTION_BYTE)) {
        payload_size += 1;
      }
    }

    uint32_t end = sei_data.a() + index + payload_size;

    return SEIPayloadInfo(payload_type, payload_size, decode::ByteRange(start, end - start));
  };

  // scan through the nal
  while (index < sei_data.count() - 1) {
    SEIPayloadInfo info = next_sei_info();
    // extract caption data
    if (info.payload_type == USER_DATA_REGISTERED_ITU_T_T35) { // caption data
      caption_ranges.push_back(info.payload_range);
    }
    index += info.payload_size;
  }
  return caption_ranges;
}

uint32_t copy_caption_payloads_to_caption_data(const common::Data32& data, common::Data32& caption_data, const vector<decode::ByteRange>& caption_ranges, const uint8_t& nalu_length_size) {
  uint32_t caption_size = 0;
  common::Data32 caption_data_copy = common::Data32(caption_data.data() + caption_data.a(), caption_data.capacity() - caption_data.a(), nullptr);
  caption_data_copy.set_bounds(caption_data_copy.a() + nalu_length_size + 1, caption_data_copy.b()); // +1 to skip the nalu type byte
  for (auto range: caption_ranges) {
    common::Data32 caption = common::Data32(data.data() + range.pos, range.size, nullptr);
    caption_data_copy.copy(caption);
    caption_data_copy.set_bounds(caption_data_copy.b(), caption_data_copy.b());
    caption_size += range.size;
  }

  auto write_byte = [](common::Data32& data, const uint32_t pos, const uint8_t val) {
    common::Data32 byte_to_write = common::Data32(&val, 1, nullptr);
    data.set_bounds(pos, pos + 1);
    data.copy(byte_to_write);
    data.set_bounds(data.b(), data.b());
  };

  if (caption_size) {
    // write nalu trailing byte
    write_byte(caption_data_copy, caption_data_copy.b(), RBSP_TRAILING_BITS);
    // write nalu type byte
    write_byte(caption_data_copy, nalu_length_size, (uint8_t)internal::decode::H264NalType::SEI);
    caption_size += 2;
    // write nalu size
    caption_data_copy.set_bounds(0, nalu_length_size);
    internal::decode::write_nal_size(caption_data_copy, caption_size, nalu_length_size);
    caption_size += nalu_length_size;
  }

  return caption_size;
}

}}
