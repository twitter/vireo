//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include "vireo/base_cpp.h"
#include "vireo/error/error.h"

namespace vireo {
namespace error {

void ImageCoreHandler(int code, const char* message, const char* file, int line) {
  THROW(file, (string("error code ") + std::to_string(code)).c_str(), line, message, ImageCore, kErrorCategoryToGenericReason[ImageCore]);
}

}}
