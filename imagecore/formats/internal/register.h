//
//  register.h
//  ImageCore
//
//  Created by Luke Alonso on 6/27/14.
//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0
//

#pragma once

#define REGISTER_IMAGE_READER(c) int __register##c = ImageReader::registerReader(new c::Factory());
#define REGISTER_IMAGE_WRITER(c) int __register##c = ImageWriter::registerWriter(new c::Factory());

#define DECLARE_IMAGE_READER(c) \
class Factory : public ImageReader::Factory \
{ \
public: \
virtual ImageReader* create() { return new c(); } \
virtual bool matchesSignature(const uint8_t* sig, unsigned int sigLen); \
}; \
private: \
c(); \
virtual ~c(); \
public:

#define DECLARE_IMAGE_WRITER(c) \
class Factory : public ImageWriter::Factory \
{ \
public: \
virtual ImageWriter* create() { return new c(); } \
virtual EImageFormat getFormat(); \
virtual bool appropriateForInputFormat(EImageFormat); \
virtual bool supportsInputColorModel(EImageColorModel); \
virtual bool matchesExtension(const char* extension); \
}; \
private: \
c(); \
virtual ~c(); \
public:

namespace imagecore {

int registerDefaultImageReaders();
int registerDefaultImageWriters();

}
