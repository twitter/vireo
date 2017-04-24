//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
