//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include <vector>

#include "vireo/base_h.h"
#include "vireo/common/data.h"
#include "vireo/types.h"

namespace vireo {
namespace util {

using namespace std;

struct FtypUtil {
  static auto Matches(const vector<uint8_t>& ftyp, const common::Data32& data) -> bool;
  static auto Matches(const vector<vector<uint8_t>>& ftyps, const common::Data32& data) -> bool;
};

}}
