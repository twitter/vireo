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

#include <fstream>
#include <vector>

#include "vireo/base_cpp.h"
#include "vireo/common/path.h"
#include "vireo/demux/movie.h"
#include "vireo/error/error.h"
#include "vireo/mux/mp4.h"
#include "vireo/transform/trim.h"
#include "vireo/util/util.h"

using namespace vireo;

using std::ifstream;
using std::ofstream;
using std::vector;

int main(int argc, const char* argv[]) {
  if (argc < 5) {
    const string name = common::Path::Filename(argv[0]);
    cout << "Usage: " << name << " start_in_ms duration_in_ms input output" << endl;
    return 1;
  }
  uint64_t start_ms = atoi(argv[1]);
  uint64_t duration_ms = atoi(argv[2]);
  string input = vireo::common::Path::MakeAbsolute(argv[3]);
  string output = vireo::common::Path::MakeAbsolute(argv[4]);
  __try {
    THROW_IF(duration_ms == 0, InvalidArguments);

    // Demux movie
    demux::Movie demuxer(input);

    // Trim video track
    auto trimmed_video = transform::Trim<SampleType::Video>(demuxer.video_track, demuxer.video_track.edit_boxes(), start_ms, duration_ms);

    // Trim audio track
    auto trimmed_audio = transform::Trim<SampleType::Audio>(demuxer.audio_track, demuxer.audio_track.edit_boxes(), start_ms, duration_ms);

    // Trim caption track
    auto trimmed_caption = transform::Trim<SampleType::Caption>(demuxer.caption_track, demuxer.caption_track.edit_boxes(), start_ms, duration_ms);

    // Convert samples
    auto video_track = functional::Video<encode::Sample>(trimmed_video.track, encode::Sample::Convert);
    auto audio_track = functional::Audio<encode::Sample>(trimmed_audio.track, encode::Sample::Convert);
    auto caption_track = functional::Caption<encode::Sample>(trimmed_caption.track, encode::Sample::Convert);

    // Collect output edit boxes
    vector<common::EditBox> edit_boxes;
    edit_boxes.insert(edit_boxes.end(), trimmed_video.track.edit_boxes().begin(), trimmed_video.track.edit_boxes().end());
    edit_boxes.insert(edit_boxes.end(), trimmed_audio.track.edit_boxes().begin(), trimmed_audio.track.edit_boxes().end());

    // Prepare encoder
    mux::MP4 mp4_encoder(audio_track, video_track, caption_track, edit_boxes);

    // Mux
    util::save(output, mp4_encoder());
  } __catch (std::exception& e) {
#if __EXCEPTIONS
    cerr << "Error trimming movie: " << e.what() << endl;
#else
    cerr << "Error trimming movie" << endl;
#endif
    return 1;
  }
  return 0;
}
