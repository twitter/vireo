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
#include "vireo/common/editbox.h"
#include "vireo/encode/types.h"
#include "vireo/functional/media.hpp"

namespace vireo {
namespace mux {

class PUBLIC MP4 final : public functional::Function<common::Data32> {
  std::shared_ptr<struct _MP4> _this = nullptr;
public:
  MP4(const vireo::functional::Video<encode::Sample>& video, const FileFormat file_format) : MP4(functional::Audio<encode::Sample>(), video, functional::Caption<encode::Sample>(), vector<common::EditBox>(), file_format) {};
  MP4(const vireo::functional::Video<encode::Sample>& video, const vector<common::EditBox> edit_boxes = vector<common::EditBox>(), const FileFormat file_format = FileFormat::Regular) : MP4(functional::Audio<encode::Sample>(), video, functional::Caption<encode::Sample>(), edit_boxes, file_format) {};
  MP4(const vireo::functional::Audio<encode::Sample>& audio, const vireo::functional::Video<encode::Sample>& video, const FileFormat file_format) : MP4(audio, video, functional::Caption<encode::Sample>(), vector<common::EditBox>(), file_format) {};
  MP4(const vireo::functional::Audio<encode::Sample>& audio, const vireo::functional::Video<encode::Sample>& video, const vector<common::EditBox> edit_boxes = vector<common::EditBox>(), const FileFormat file_format = FileFormat::Regular) : MP4(audio, video, functional::Caption<encode::Sample>(), edit_boxes, file_format) {};

  MP4(const vireo::functional::Video<encode::Sample>& video, const vireo::functional::Caption<encode::Sample>& caption, const FileFormat file_format) : MP4(functional::Audio<encode::Sample>(), video, caption, vector<common::EditBox>(), file_format) {};
  MP4(const vireo::functional::Video<encode::Sample>& video, const vireo::functional::Caption<encode::Sample>& caption, const vector<common::EditBox> edit_boxes = vector<common::EditBox>(), const FileFormat file_format = FileFormat::Regular) : MP4(functional::Audio<encode::Sample>(), video, caption, edit_boxes, file_format) {};
  MP4(const vireo::functional::Audio<encode::Sample>& audio, const vireo::functional::Video<encode::Sample>& video, const vireo::functional::Caption<encode::Sample>& caption, const FileFormat file_format) : MP4(audio, video, caption, vector<common::EditBox>(), file_format) {};
  MP4(const vireo::functional::Audio<encode::Sample>& audio, const vireo::functional::Video<encode::Sample>& video, const vireo::functional::Caption<encode::Sample>& caption, const vector<common::EditBox> edit_boxes = vector<common::EditBox>(), const FileFormat file_format = FileFormat::Regular);

  MP4(const MP4& mp4);
  MP4(MP4&& mp4);
  DISALLOW_ASSIGN(MP4);
  auto operator()() -> common::Data32;
  auto operator()(FileFormat file_format) -> common::Data32;
};

}}
