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

#include <iomanip>
#include <fstream>
#include <set>
#include <vector>

#include "vireo/base_cpp.h"
#include "vireo/common/math.h"
#include "vireo/common/path.h"
#include "vireo/decode/video.h"
#include "vireo/demux/movie.h"
#include "vireo/encode/jpg.h"
#include "vireo/error/error.h"

using namespace vireo;

using std::ifstream;
using std::ofstream;
using std::set;
using std::vector;

int main(int argc, const char* argv[]) {
  if (argc < 5) {
    const string name = common::Path::Filename(argv[0]);
    cout << "Usage: " << name << " size count input output" << endl;
    return 1;
  }
  const int size = atoi(argv[1]);
  if (size < 50 || size > 1024) {
    cout << "Invalid thumnail size: " << size << " (minimum 50, maximum 1024)" << endl;
    return 1;
  }
  const int count = atoi(argv[2]);
  if (count < 2 || count > 100) {
    cout << "Invalid requested thumbnail count: " << count << " (minimum 2, maximum 100)" << endl;
    return 1;
  }
  stringstream s_src;
  s_src << common::Path::MakeAbsolute(argv[3]);

  stringstream s_dst;
  s_dst << common::Path::MakeAbsolute(argv[4]);
  if (!common::Path::Exists(s_dst.str())) {
    if (common::Path::CreateFolder(s_dst.str())) {
      cout << "Error creating output folder: " << s_dst.str() << endl;
      return 1;
    }
  }

  __try {
    // Demux file
    vireo::demux::Movie movie(s_src.str());
    vireo::decode::Video decoder(movie.video_track);

    // Get indices at which to produce thumbnails
    set<uint32_t> indices;
    for (uint32_t i = 0; i < count; ++i) {
      indices.insert(common::round_divide(i * (decoder.count() - 1), (uint32_t)1, (uint32_t)count - 1));
    }
    // Stream resized frames to jpg encoder
    vireo::encode::JPG jpg_encoder(functional::Video<frame::YUV>(decoder.filter_index([&indices](uint32_t index) { return indices.find(index) != indices.end(); }), [size, width=decoder.settings().width](const frame::Frame& frame) -> frame::YUV { return frame.yuv().full_range(true).scale(size, width); }), 95, 0);
    // Create thumbnails
    uint32_t index = 0;
    uint32_t jpg_index = 0;
    for (auto jpg: jpg_encoder) {
      stringstream jpg_dst;
      jpg_dst << s_dst.str() << "/" << jpg_index++ << ".jpg";
      ofstream ostream(common::Path::MakeAbsolute(jpg_dst.str()).c_str(), ofstream::out | ofstream::binary);
      ostream << jpg;
      ++index;
    }
  } __catch (std::exception& e) {
    cerr << "Error reading movie" << endl;
    return 1;
  }
  return 0;
}
