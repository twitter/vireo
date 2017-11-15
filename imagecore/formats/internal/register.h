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
