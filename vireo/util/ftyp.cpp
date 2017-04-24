//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include "vireo/base_cpp.h"
#include "vireo/error/error.h"
#include "vireo/util/ftyp.h"

namespace vireo {
namespace util {

auto FtypUtil::Matches(const vector<uint8_t>& ftyp, const common::Data32& data) -> bool {
  if (data.count() < ftyp.size()) {
    return false;
  }
  uint32_t index = 0;
  for (const auto byte: ftyp) {
    if (byte != data(index++)) {
      return false;
    }
  }
  return true;
};

auto FtypUtil::Matches(const vector<vector<uint8_t>>& ftyps, const common::Data32& data) -> bool {
  for (auto ftyp: ftyps) {
    if (Matches(ftyp, data)) {
      return true;
    }
  }
  return false;
};

}}