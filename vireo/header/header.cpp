//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

extern "C" {
#include "lsmash.h"
}
#include "vireo/common/security.h"
#include "vireo/common/util.h"
#include "vireo/base_cpp.h"
#include "vireo/constants.h"
#include "vireo/error/error.h"
#include "vireo/header/header.h"

namespace vireo {
namespace header {

SPS_PPS::SPS_PPS(const common::Data16& sps, const common::Data16& pps, const uint8_t nalu_length_size) noexcept
  : sps(sps), pps(pps), nalu_length_size(nalu_length_size) {
  THROW_IF(sps.count() >= security::kMaxHeaderSize, Unsafe);
  THROW_IF(pps.count() >= security::kMaxHeaderSize, Unsafe);
  THROW_IF(nalu_length_size != 4 && nalu_length_size != 2, InvalidArguments);
}

SPS_PPS::SPS_PPS(const SPS_PPS& sps_pps) noexcept
  : sps(sps_pps.sps), pps(sps_pps.pps), nalu_length_size(sps_pps.nalu_length_size) {
}

SPS_PPS::SPS_PPS(SPS_PPS&& sps_pps) noexcept
  : sps(move(sps_pps.sps)), pps(move(sps_pps.pps)), nalu_length_size(sps_pps.nalu_length_size) {
}

auto SPS_PPS::operator=(const SPS_PPS& sps_pps) -> void {
  sps = sps_pps.sps;
  pps = sps_pps.pps;
  nalu_length_size = sps_pps.nalu_length_size;
}

auto SPS_PPS::operator=(SPS_PPS&& sps_pps) -> void {
  sps = move(sps_pps.sps);
  pps = move(sps_pps.pps);
  nalu_length_size = sps_pps.nalu_length_size;
}

auto SPS_PPS::operator==(const SPS_PPS& sps_pps) const -> bool {
  return (nalu_length_size == sps_pps.nalu_length_size &&
          sps == sps_pps.sps &&
          pps == sps_pps.pps);
}

auto SPS_PPS::operator!=(const SPS_PPS& sps_pps) const -> bool {
  return !(*this == sps_pps);
}

auto SPS_PPS::as_extradata(ExtraDataType type) const -> common::Data16 {
  switch (type) {
    case iso: {
      lsmash_codec_specific_t* cs = lsmash_create_codec_specific_data(LSMASH_CODEC_SPECIFIC_DATA_TYPE_ISOM_VIDEO_H264,
                                                                      LSMASH_CODEC_SPECIFIC_FORMAT_STRUCTURED);
      CHECK(cs);
      lsmash_h264_specific_parameters_t* parameters = (lsmash_h264_specific_parameters_t*)cs->data.structured;
      parameters->lengthSizeMinusOne = nalu_length_size - 1;
      THROW_IF(lsmash_append_h264_parameter_set(parameters, H264_PARAMETER_SET_TYPE_SPS, (void*)sps.data(), sps.count()) != 0, InvalidArguments);
      THROW_IF(lsmash_append_h264_parameter_set(parameters, H264_PARAMETER_SET_TYPE_PPS, (void*)pps.data(), pps.count()) != 0, InvalidArguments);

      lsmash_codec_specific_t* cs_u = lsmash_convert_codec_specific_format(cs, LSMASH_CODEC_SPECIFIC_FORMAT_UNSTRUCTURED);
      lsmash_destroy_codec_specific_data(cs);
      CHECK(cs_u);

      return common::Data16(cs_u->data.unstructured + 8, cs_u->size - 8, [cs_u](uint8_t* p) {
        lsmash_destroy_codec_specific_data(cs_u);
      });
    }
    case annex_b: {
      const uint16_t length = sps.count() + pps.count() + 8;
      common::Data16 header(new uint8_t[length], length, [](uint8_t* p) { delete[] p; });
      uint8_t* data = (uint8_t*)header.data();
      uint16_t offset = 0;
      data[offset++] = 0;
      data[offset++] = 0;
      data[offset++] = 0;
      data[offset++] = 0x1;
      memcpy(data + offset, (void*)sps.data(), sps.count());
      offset += sps.count();
      data[offset++] = 0;
      data[offset++] = 0;
      data[offset++] = 0;
      data[offset++] = 0x1;
      memcpy(data + offset, (void*)pps.data(), pps.count());
      return header;
    }
    case avcc: {
      const uint16_t sps_size = sps.count();
      const uint16_t pps_size = pps.count();
      const uint16_t length = sps_size + pps_size + 2 * nalu_length_size;
      common::Data16 sps_pps_data(new uint8_t[length], length, [](uint8_t* p) { delete[] p; });
      common::util::WriteNalSize(sps_pps_data, sps_size, nalu_length_size);
      sps_pps_data.set_bounds(sps_pps_data.a() + nalu_length_size, length);
      sps_pps_data.copy(sps);
      sps_pps_data.set_bounds(sps_pps_data.a() + sps_size, length);
      common::util::WriteNalSize(sps_pps_data, pps_size, nalu_length_size);
      sps_pps_data.set_bounds(sps_pps_data.a() + nalu_length_size, length);
      sps_pps_data.copy(pps);
      sps_pps_data.set_bounds(0, length);
      return sps_pps_data;
    }
  }
}

}}
