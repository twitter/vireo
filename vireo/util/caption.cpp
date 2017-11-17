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

#include "vireo/common/util.h"
#include "vireo/util/caption.h"
#include "vireo/internal/decode/types.h"

#define EMULATION_PREVENTION_BYTE 0x03
#define USER_DATA_REGISTERED_ITU_T_T35 0x04
#define RBSP_TRAILING_BITS 0x80

namespace vireo {
namespace util {

enum SEIPayloadType {
  Unknown = 0,
  Caption = 1,
};

struct SEIPayloadInfo {
  SEIPayloadType type;
  decode::ByteRange byte_range;
  SEIPayloadInfo() : type(SEIPayloadType::Unknown), byte_range(decode::ByteRange()) {};
  SEIPayloadInfo(const common::Data32& sei_data) {
    uint32_t index = 0;

    // get payload type
    uint32_t payload_type = 0;
    while (sei_data(sei_data.a() + index) == 0xFF) { // in case the value is over 1 byte long
      payload_type += 255;
      index++;
    }
    payload_type += sei_data(sei_data.a() + index);
    index++;

    // get payload size
    int32_t payload_size = 0;
    while (sei_data(sei_data.a() + index) == 0xFF) {
      payload_size += 255;
      index++;
    }
    payload_size += sei_data(sei_data.a() + index);
    index++;

    // include emulation prevention byte(s) in byte range
    for (int32_t i = 0; i < payload_size - 2; i++) {
      if (sei_data.count() <= index + i + 2) {
        return;
      } else if ((sei_data(sei_data.a() + index + i) == 0x00) &&
                 (sei_data(sei_data.a() + index + i + 1) == 0x00) &&
                 (sei_data(sei_data.a() + index + i + 2) == EMULATION_PREVENTION_BYTE)) {
        payload_size += 1;
      }
    }

    if (payload_type == USER_DATA_REGISTERED_ITU_T_T35) {
      type = SEIPayloadType::Caption;
    }
    byte_range = decode::ByteRange(sei_data.a(), index + payload_size);
  }
};

CaptionPayloadInfo CaptionHandler::ParsePayloadInfo(const common::Data32& sei_data) {
  CaptionPayloadInfo caption_info;
  caption_info.valid = true;
  auto nal_copy = common::Data32(sei_data.data() + sei_data.a(), sei_data.count(), nullptr);
  nal_copy.set_bounds(nal_copy.a() + 1, nal_copy.b()); // skip the nalu type byte

  // scan through the nal
  while (nal_copy.count() > 1) {  // last byte is RBSP_TRAILING_BITS
    SEIPayloadInfo info = SEIPayloadInfo(nal_copy);
    if (info.byte_range.available) {
      if (info.type == SEIPayloadType::Caption) {
        caption_info.byte_ranges.push_back(info.byte_range);
      }
      nal_copy.set_bounds(info.byte_range.pos + info.byte_range.size, nal_copy.b());
    } else {
      caption_info.valid = false;
      break;
    }
  }
  return caption_info;
}

uint32_t CaptionHandler::CopyPayloadsIntoData(const common::Data32& sei_data,
                                              const CaptionPayloadInfo& info,
                                              const uint8_t& nalu_length_size,
                                              common::Data32& out_data) {
  uint32_t caption_size = 0;
  common::Data32 out_data_copy = common::Data32(out_data.data() + out_data.a(), out_data.capacity() - out_data.a(), nullptr);
  out_data_copy.set_bounds(nalu_length_size + 1, nalu_length_size + 1); // +1 to skip the nalu type byte
  for (auto range: info.byte_ranges) {
    common::Data32 caption = common::Data32(sei_data.data() + range.pos, range.size, nullptr);
    out_data_copy.copy(caption);
    out_data_copy.set_bounds(out_data_copy.b(), out_data_copy.b());
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
    write_byte(out_data_copy, out_data_copy.b(), RBSP_TRAILING_BITS);
    // write nalu type byte
    write_byte(out_data_copy, nalu_length_size, (uint8_t)internal::decode::H264NalType::SEI);
    caption_size += 2;
    // write nalu size
    out_data_copy.set_bounds(0, nalu_length_size);
    common::util::WriteNalSize(out_data_copy, caption_size, nalu_length_size);
    caption_size += nalu_length_size;
  }

  return caption_size;
}

}}
