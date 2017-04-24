//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/encode/types.h"
#include "vireo/functional/media.hpp"

namespace vireo {
namespace mux {

class PUBLIC MP2TS final : public functional::Function<common::Data32> {
  std::shared_ptr<struct _MP2TS> _this;
public:
  MP2TS(const functional::Video<encode::Sample>& video) : MP2TS(functional::Audio<encode::Sample>(), video, functional::Caption<encode::Sample>()) {};
  MP2TS(const functional::Audio<encode::Sample>& audio, const functional::Video<encode::Sample>& video) : MP2TS(audio, video, functional::Caption<encode::Sample>()) {};
  MP2TS(const functional::Audio<encode::Sample>& audio, const functional::Video<encode::Sample>& video, const functional::Caption<encode::Sample>& caption);
  MP2TS(MP2TS&& mp2ts);
  DISALLOW_COPY_AND_ASSIGN(MP2TS);
};

}}
