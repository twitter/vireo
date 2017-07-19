//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#pragma once

#include "vireo/base_h.h"

class Profile;
std::ostream& operator<<(std::ostream& os, const Profile& profile);

class Profile {
  struct _Profile* _this;
  Profile();
public:
  ~Profile();
  Profile(Profile&& profile) noexcept;
  DISALLOW_COPY_AND_ASSIGN(Profile);
  static Profile Function(const std::string& name, std::function<void(void)> f, uint32_t iterations = 1);
  friend std::ostream& operator<<(std::ostream& os, const Profile& profile);
};

extern "C" {
void setTestPath(const char* path);
const char* getTestPath();
const char* getTestSrcPath();
const char* getTestDstPath();
}
