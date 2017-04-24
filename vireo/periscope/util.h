//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/data.h"
#include "vireo/settings/settings.h"

namespace vireo {
namespace periscope {

struct ID3Info {
  double ntp_timestamp;
  settings::Video::Orientation orientation;
  ID3Info(settings::Video::Orientation orientation = settings::Video::Orientation::UnknownOrientation,
          double ntp_timestamp = 0.0)
    : orientation(orientation), ntp_timestamp(ntp_timestamp) {};
};

struct Util {
  static auto ParseID3Info(const common::Data32& id3_data) -> ID3Info;
};

}}
