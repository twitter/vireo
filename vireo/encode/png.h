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

class PUBLIC PNG final : public functional::DirectVideo<PNG, common::Data32> {
  std::shared_ptr<struct _PNG> _this;
public:
  PNG(const functional::Video<frame::RGB>& frames);
  DISALLOW_COPY_AND_ASSIGN(PNG);
  auto operator()(uint32_t index) const -> common::Data32;
};

}}
