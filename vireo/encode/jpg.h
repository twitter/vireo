//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/data.h"
#include "vireo/domain/interval.hpp"
#include "vireo/encode/types.h"

namespace vireo {
namespace encode {

class PUBLIC JPG final : public functional::DirectVideo<JPG, common::Data32> {
  std::shared_ptr<struct _JPG> _this;
public:
  JPG(const functional::Video<frame::YUV>& frames, int quality, int optimization);
  DISALLOW_COPY_AND_ASSIGN(JPG);
  auto operator()(uint32_t sample) const -> common::Data32;
};

}}
