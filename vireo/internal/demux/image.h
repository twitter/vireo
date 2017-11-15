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
