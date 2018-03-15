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

#include <iostream>

#include "vireo/config.h"
#include "vireo/demux/movie.h"
#include "vireo/decode/video.h"
#include "vireo/encode/h264.h"
#include "vireo/mux/mp4.h"
#include "vireo/util/util.h"

using namespace std;
using namespace vireo;

/*
 * Simple example for remuxing an input file to mp4
 * This function works without GPL dependencies
 */
void remux(string in, string out) {
  cout << "Remuxing " << in << " to " << out << endl;
  // setup the demux -> mux pipeline
  demux::Movie movie(in);
  mux::MP4 muxer(movie.video_track);
  // nothing is executed until muxer() is called
  auto binary = muxer();
  // save to file
  util::save(out, binary);
  cout << "Done" << endl;
}

/*
 * Simple example for remuxing keyframes of an input file to mp4
 * This function works without GPL dependencies
 */
void keyframes(string in, string out) {
  cout << "Extracting keyframes from " << in << " to " << out << endl;
  demux::Movie movie(in);
  // extract keyframes using .filter operator
  auto keyframes = movie.video_track.filter([](decode::Sample& sample) {
    return sample.keyframe;
  });
  mux::MP4 muxer(keyframes);
  // nothing is executed until muxer() is called
  auto binary = muxer();
  util::save(out, binary);
  cout << "Done" << endl;
}

#ifdef HAVE_LIBAVCODEC
#ifdef HAVE_LIBX264
#define TRANSCODE_SUPPORTED
/*
 * Simple example for transcoding an input file to mp4
 * This function requires vireo to be built with --enable-gpl flag
 */
void transcode(string in, string out) {
  cout << "Transcoding " << in << " to " << out << endl;
  // setup the demux -> decode -> encode -> mux pipeline
  demux::Movie movie(in);
  decode::Video decoder(movie.video_track);
  encode::H264 encoder(decoder, 30.0f, 3, movie.video_track.fps());
  mux::MP4 muxer(encoder);
  // nothing is executed until muxer() is called
  auto binary = muxer();
  // save to file
  util::save(out, binary);
  cout << "Done" << endl;
}
#endif
#endif

int main() {
  remux("helloworld.mp4", "helloworld-remuxed.mp4");
  keyframes("helloworld.mp4", "helloworld-keyframes.mp4");
#ifdef TRANSCODE_SUPPORTED
  transcode("helloworld.mp4", "helloworld-transcoded.mp4");
#endif
  return 0;
}
