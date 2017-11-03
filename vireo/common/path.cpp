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

#include <algorithm>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "vireo/base_cpp.h"
#include "vireo/common/path.h"
#include "vireo/error/error.h"

namespace vireo {
namespace common {

string Path::RemoveLastComponent(const string& path) {
  const char* filename = strrchr(path.c_str(), '/');
  if (!filename || filename == path) {
    return NULL;
  }
  size_t length = filename - path.c_str();
  return path.substr(0, length);
}

string Path::RemoveExtension(const string& path) {
  const char* dot = strrchr(path.c_str(), '.');
  if (!dot || dot == path) {
    return NULL;
  }
  size_t length = dot - path.c_str();
  return path.substr(0, length);
}

bool Path::Exists(const string& path) {
  struct stat st;
  return stat(path.c_str(), &st) == 0;
}

int Path::CreateFolder(const string& path) {
  if (Exists(path)) {
    return 1;
  }
  const auto containing_folder = RemoveLastComponent(path);
  if (!Path::Exists(containing_folder)) {
    if (CreateFolder(containing_folder) != 0) {
      return 1;
    }
  }
  if (mkdir(path.c_str(), 0777) != 0) {
    return 1;
  }
  return 0;
}

string Path::Extension(const string& path) {
  const string kString_Empty = string();
  const char* dot = strrchr(path.c_str(), '.');
  if (!dot) {
    return kString_Empty;
  }
  std::string extension = path.substr(dot - path.c_str() + 1, path.length());
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
  return extension;
}

string Path::Filename(const string& path) {
  const char* slash = strrchr(path.c_str(), '/');
  if (!slash) {
    return path;
  }
  std::string filename = path.substr(slash - path.c_str() + 1, path.length());
  return filename;
}

string Path::MakeAbsolute(const string& path) {
  stringstream s_src;
  char cwd[1024];
  getcwd(cwd, sizeof(cwd));
  if (path[0] != '/') {
    s_src << cwd << "/";
  }
  s_src << path;
  return s_src.str();
}

}}
