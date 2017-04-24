//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/reader.h"
#include "vireo/decode/types.h"

namespace vireo {
namespace internal {
namespace demux {

using namespace vireo::decode;

const vector<uint8_t> kJPGFtyp = { 0xFF, 0xD8 };
const vector<uint8_t> kPNGFtyp = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
const vector<uint8_t> kGIFFtyp = { 0x47, 0x49, 0x46 };
const vector<uint8_t> kBMPFtyp = { 0x42, 0x4D };
const vector<uint8_t> kWebPFtyp = { 0x52, 0x49, 0x46, 0x46 };
const vector<vector<uint8_t>> kTIFFFtyps = { { 0x49, 0x49 }, { 0x4D, 0x4D } };

const vector<vector<uint8_t>> kImageFtyps = { kJPGFtyp, kPNGFtyp, kGIFFtyp, kBMPFtyp, kWebPFtyp, kTIFFFtyps[0], kTIFFFtyps[1] };

class Image final {
  std::shared_ptr<struct _Image> _this = nullptr;

public:
  Image(common::Reader&& reader);
  Image(Image&& image);
  DISALLOW_COPY_AND_ASSIGN(Image);

  class Track final : public functional::DirectVideo<Track, Sample> {
    std::shared_ptr<_Image> _this;
    Track(const std::shared_ptr<_Image>& _this);
    friend class Image;
  public:
    Track(const Track& track);
    DISALLOW_ASSIGN(Track);
    auto duration() const -> uint64_t;
    auto fps() const -> float;
    auto operator()(const uint32_t index) const -> Sample;
  } track;
};

}}}
