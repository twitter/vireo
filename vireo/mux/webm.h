//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/encode/types.h"
#include "vireo/functional/media.hpp"
#include "vireo/settings/settings.h"

namespace vireo {
namespace mux {

class PUBLIC WebM final : public functional::Function<common::Data32> {
  std::shared_ptr<struct _WebM> _this;
public:
  WebM(const vireo::functional::Video<encode::Sample>& video) : WebM(functional::Audio<encode::Sample>(), video) {};
  WebM(const vireo::functional::Audio<encode::Sample>& audio, const vireo::functional::Video<encode::Sample>& video);
  WebM(const WebM& webm);
  WebM(WebM&& webm);
  DISALLOW_ASSIGN(WebM);
};

}}
