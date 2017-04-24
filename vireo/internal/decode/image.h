//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/decode/types.h"
#include "vireo/frame/frame.h"
#include "vireo/functional/media.hpp"

namespace vireo {
namespace internal {
namespace decode {

using namespace vireo::decode;

class PUBLIC Image final : public functional::DirectVideo<Image, frame::Frame> {
  std::shared_ptr<struct _Image> _this;

public:
  Image(const functional::Video<Sample>& track);
  Image(const Image& image);
  DISALLOW_ASSIGN(Image);
  auto operator()(uint32_t index) const -> frame::Frame;
};

}}}
