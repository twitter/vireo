//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

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
