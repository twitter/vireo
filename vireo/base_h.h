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

#include <functional>
#include <iostream>
#include <istream>
#include <memory>
#include <string>
#include <sstream>
#include <sys/param.h>

#if defined(CONSTRUCTOR)
#undef CONSTRUCTOR
#endif
#if defined(_WINDOWS)
#define CONSTRUCTOR __cdecl
#else
#define CONSTRUCTOR __attribute__((constructor))
#endif

#if defined(DESTRUCTOR)
#undef DESTRUCTOR
#endif
#if defined(_WINDOWS)
#define DESTRUCTOR __cdecl
#else
#define DESTRUCTOR __attribute__((destructor))
#endif

#if defined _WIN32 || defined __CYGWIN__
#ifdef BUILDING_DLL
#ifdef __GNUC__
#define PUBLIC __attribute__ ((dllexport))
#else
#define PUBLIC __declspec(dllexport) // Note: actually gcc seems to also supports this syntax.
#endif
#else
#ifdef __GNUC__
#define PUBLIC __attribute__ ((dllimport))
#else
#define PUBLIC __declspec(dllimport) // Note: actually gcc seems to also supports this syntax.
#endif
#endif
#define LOCAL
#else
#if __GNUC__ >= 4
#ifndef ANDROID
#define PUBLIC __attribute__ ((visibility ("default")))
#else
#define PUBLIC __attribute__ ((visibility ("hidden")))
#endif
#define LOCAL  __attribute__ ((visibility ("hidden")))
#else
#define PUBLIC
#define LOCAL
#endif
#endif

#if __llvm__ == 1
#define TT(__CLASS__) __CLASS__::template __CLASS__
#else
#define TT(__CLASS__) __CLASS__
#endif

#ifdef __EXCEPTIONS
# define __try      try
# define __catch(X) catch(X)
# define __throw_exception_again throw
#else
# define __try      if (true)
# define __catch(X) if (false)
# define __throw_exception_again
#endif

#define DISALLOW_COPY_AND_ASSIGN(C)\
C& operator=(const C&) = delete;\
C(const C&) = delete;

#define DISALLOW_COPY(C)\
C(const C&) = delete;

#define DISALLOW_ASSIGN(C)\
C& operator=(const C&) = delete;
