# Overview

Vireo is a lightweight and versatile video processing library, written in C++-11 and built with functional programming principles. It also optionally comes with light Scala wrappers that enable easy video processing within a high-level functional language.

Vireo is built on top of best of class open source libraries, and defines a unified and modular interface for these libraries to communicate easily and efficiently. With this unified interface, it is easy to write new modules (e.g. new codec) or swap out the existing ones in favor of others (e.g. proprietary or hardware H.264 decoder).

Vireo tries to cater towards developers that are not necessarily experts in all individual video technologies and want to focus on just building products. It is released under the MIT license and aims to make it easy for developers to do media processing and create both commercial and non-commercial (see [Legal Disclaimer](#legal-disclaimer) for details).

Vireo conveniently ships with a few media processing tools that are built on top of Vireo.

**List of Tools:**

- **frames**: displays the contents of video files (list of audio/video samples, edit boxes, track durations etc.)
- **chunk**: chunks GOPs of the input video as well as the audio track into separate mp4 files
- **psnr**: compares the video quality of a test video against a reference video
- **remux**: allows remuxing an input file into other compatible containers
- **stitch**: stitches a number of input video files into a single video file
- **thumbnails**: extracts keyframes from the input video and saves them as JPG images
- **transcode**: transcodes an input video into another video with different format (able to change resolution, crop, change bitrate, convert containers/codecs)
- **trim**: trims the input video at desired time boundaries without transcoding the input video
- **validate**: checks if the video is valid and if so, supported by vireo
- **viddiff**: checks if two video files are functionally identical or not (does not compare data that does not affect the playback behavior)

# How to Build Vireo and Tools

To build and use vireo simply execute:

```
    # within the main repository directory
    $ cd vireo
    $ export PREFIX=/path/to/install/dir
    $ ./configure --prefix=$PREFIX
    $ make
    $ make install
```

Once built, you can find

- the headers under `$PREFIX/include`
- the compiled libraries under `$PREFIX/lib`
- the tools under `$PREFIX/bin`

To run a tool and see their usage, simply execute:

```
$ $PREFIX/bin/<tool_name>
```

An exception to this rule is the `validate` tool which requires `validate -h` to show its usage as it uses STDIN for input.

Please note that some of the tools listed in [List of Tools](#list-of-tools) may not be built based on the availability of optional third-party libraries listed under [Dependencies](#dependencies).

When compiling you can also optionally turn on C++ to Scala bindings by passing `--enable-scala` flag to `configure`. After you successfully build Vireo with these bindings, you can build the Scala wrappers by executing:

```
    # within the main repository directory
    $ cd vireo
    $ ./build_scala.sh
```

This will place the built jar file under `$PREFIX/lib`.

## **Dependencies**

## Required tools (for building C++ library)
- [autotools](https://en.wikipedia.org/wiki/GNU_Build_System)
- [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)

## Optional tools (for building Scala wrappers)
- [maven](https://maven.apache.org/)

## Required libraries
- [L-SMASH](https://github.com/l-smash/l-smash)

## Optional libraries
The following libraries are automatically enabled if present in your system

- [libpng](http://www.libpng.org/pub/png/libpng.html)
- [libjpeg](http://libjpeg.sourceforge.net/)
- [libfdk-aac](https://github.com/mstorsjo/fdk-aac)
- [libvpx](https://github.com/webmproject/libvpx)
- [libogg](https://xiph.org/ogg/)
- [libvorbis](https://xiph.org/vorbis/)
- [libvorbisenc](https://xiph.org/vorbis/doc/vorbisenc/index.html)
- [libwebm](https://github.com/webmproject/libwebm)

The following libraries are disabled by default. To enable GPL licensed components, `--enable-gpl` flag have to be explicitly passed to `configure` while building Vireo

- [libavformat](https://github.com/FFmpeg/FFmpeg)
- [libavcodec](https://github.com/FFmpeg/FFmpeg)
- [libavutil](https://github.com/FFmpeg/FFmpeg)
- [libswscale](https://github.com/FFmpeg/FFmpeg)
- [libx264](https://www.videolan.org/developers/x264.html)

## **Legal Disclaimer**

This installer allows you to select various third-party, pre-installed components to build with the Vireo platform. Some of these components are provided under licenses that may be incompatible with each other, so it may not be compliant to build all of the optional components together. You are responsible to determine what components to use for your build, and for complying with the applicable licenses. In addition, the use of some of the components of the Vireo platform, including those that implement the H.264, AAC, or MPEG-4 formats, may require licenses from third party patent holders. You are responsible for determining whether your contemplated use requires such a license.

# Using Vireo in Your Own Project

## Code Examples

To give you an idea on what you can build using Vireo, we provide 3 simple code snippets below.

1. Remuxing an input file to mp4
```
    /*
     * This function works without GPL dependencies
     */
    void remux(string in, string out) {
      // setup the demux -> mux pipeline
      demux::Movie movie(in);
      mux::MP4 muxer(movie.video_track);
      // nothing is executed until muxer() is called
      auto binary = muxer();
      // save to file
      util::save(out, binary);
    }
```

2. Remuxing keyframes of an input file to mp4
```
    /*
     * This function works without GPL dependencies
     */
    void keyframes(string in, string out) {
      demux::Movie movie(in);
      // extract keyframes using .filter operator
      auto keyframes = movie.video_track.filter([](decode::Sample& sample) {
        return sample.keyframe;
      });
      mux::MP4 muxer(keyframes);
      // nothing is executed until muxer() is called
      auto binary = muxer();
      util::save(out, binary);
    }
```

3. Transcoding an input file to mp4
```
    /*
     * This function requires vireo to be built with --enable-gpl flag
     */
    void transcode(string in, string out) {
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
```

## Build a HelloWorld Application with Vireo

You can built your own application simply by using `pkg-config`. Just make sure you add the Vireo install directory to your `PKG_CONFIG_PATH` if you did not install Vireo to a path where `pkg-config` is already looking at.

To build the examples provided in [Code Examples](#code-examples) section, simply execute the following:

```
    # within the main repository directory
    $ export PKG_CONFIG_PATH=$PREFIX/lib/pkgconfig
    $ cd vireo/helloworld
    $ g++ `pkg-config --cflags --libs vireo` -std=c++14 -o helloworld helloworld.cpp
    $ ./helloworld
```

# Guidelines for Contributors

- Make sure you have a **detailed description** about the change in your commit.
- **Prefer code readability** over fast development and premature performance optimizations.
- **If you're making assumptions** (can happen due to lack of enough test data, confusing documentation in the standard, or you're simply not implementing a rare edge case in order to decrease code complexity and development cost), **make sure they are documented in code**. Both with a `THROW_IF(..., Unsupported, "reason")` (or `CHECK(...)`) and ideally in comments as well.
- If there is any API or functionality change, make sure the affected tools (`remux`, `transcode` etc.) are also updated.
