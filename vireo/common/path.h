//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"
#include "vireo/common/data.h"
#include "vireo/domain/interval.hpp"

namespace vireo {
namespace common {

class PUBLIC Path {
public:
  static int CreateFolder(const std::string& path);
  static bool Exists(const std::string& path);
  static std::string MakeAbsolute(const std::string& path);
  static std::string RemoveLastComponent(const std::string& path);
  static std::string RemoveExtension(const std::string& path);
  static std::string Extension(const std::string& path);
  static std::string Filename(const std::string& path);
};

}}
