//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
