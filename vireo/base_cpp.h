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
