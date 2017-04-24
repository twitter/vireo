//  Copyright 2017 Twitter, Inc.
//  Licensed under the Apache License, Version 2.0
//  http://www.apache.org/licenses/LICENSE-2.0

#include <algorithm>
#include <array>
#include <limits>
#include <map>
#include <set>
#include <string.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include <queue>

#include "vireo/base_h.h"

using std::array;
using std::bind;
using std::cout;
using std::cerr;
using std::ios_base;
using std::istream;
using std::endl;
using std::function;
using std::get;
using std::map;
using std::make_shared;
using std::make_pair;
using std::make_tuple;
using std::max;
using std::min;
using std::move;
using std::numeric_limits;
using std::ostream;
using std::pair;
using std::runtime_error;
using std::set;
using std::shared_ptr;
using std::string;
using std::stringstream;
using std::tuple;
using std::unique_ptr;
using std::unordered_map;
using std::vector;
using std::queue;

#ifdef TESTING
#define PAUSE_FOR_LEAK_CHECKS() if (getenv("LEAK_CHECKS") != NULL) { sleep(atoi(getenv("LEAK_CHECKS"))); }
#else
#define PAUSE_FOR_LEAK_CHECKS()
#endif
