/*
 * MIT License
 *
 * Copyright (c) 2017 Twitter
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <chrono>

#include "vireo/base_cpp.h"
#include "vireo/common/math.h"
#include "vireo/common/path.h"
#include "vireo/tests/test_common.h"

using std::chrono::duration;
using std::chrono::system_clock;
using std::chrono::time_point;

using namespace vireo;

static string sPath_Test = "tests";
static string sPath_TestSrc = sPath_Test + "/data";
static string sPath_TestDst = sPath_Test + "/results";

void setTestPath(const char* path) {
  sPath_Test = path;
  sPath_TestSrc = sPath_Test + "/data";
  sPath_TestDst = sPath_Test + "/results";
}

const char* getTestPath() {
  return sPath_Test.c_str();
}

const char* getTestSrcPath() {
  return sPath_TestSrc.c_str();
}

const char* getTestDstPath() {
  if (!common::Path::Exists(sPath_TestDst)) {
    common::Path::CreateFolder(sPath_TestDst);
  }
  return sPath_TestDst.c_str();
}

struct _Profile {
  string name;
  uint32_t iterations;
  vector<double> elapsed_ms;
};

std::ostream& operator<<(std::ostream& os, const Profile& profile) {
  os << profile._this->name << " time stats over " << profile._this->iterations << " iterations:" << endl;
  auto avg_time = common::mean(profile._this->elapsed_ms);
  if (profile._this->iterations > 1) {
    auto var_time = common::variance(profile._this->elapsed_ms);
    auto std_time = common::std_dev(profile._this->elapsed_ms);
    os << "[Mean     ] " << avg_time << " msecs" << endl;
    os << "[Variance ] " << var_time << " msecs" << endl;
    os << "[Std. dev.] " << std_time << " msecs";
  } else {
    os << "[Total] " << avg_time << " msecs";
  }
  return os;
}


Profile::Profile() {
  _this = new _Profile();
}

Profile::~Profile() {
  delete _this;
}

Profile::Profile(Profile&& profile) noexcept {
  _this = profile._this;
  profile._this = NULL;
}

Profile Profile::Function(const std::string& name, std::function<void(void)> f, uint32_t iterations) {
  Profile p;
  p._this->name = name;
  p._this->iterations = iterations;
  for (uint32_t i = 0; i < iterations; ++i) {
    time_point<system_clock> start = system_clock::now();;
    f();
    time_point<system_clock> end = system_clock::now();
    p._this->elapsed_ms.push_back((end - start).count() / 1000.0f);
  }
  return p;
}
