# Overview

**vireo** is a C++ library with wrappers for Scala and Java for **easy** media processing on the cloud, desktop and mobile.

vireo is built based on the following principles:

* **Ease of use**

  - high level building blocks allow coding just 'as easy' as describing behavior

* **Modular**

  - has use-case independent modular building blocks

  - easy to swap any 3rd-party library

* **Efficient**

  - executes only what is needed

  - enforces minimal computation / memory usage for all use cases

* **Doesn't reinvent the wheel**

  - layered on top of best of class open source libraries

* **'Multiresolution'**

  - allows programmers to quickly get to a functional state based on high level functional logic, then refine the behavior as optimization (caching, ...) or application (distribution, parallelization) according to the need

* **Portable**

  - can run on the cloud, locally on your desktop or on mobile

  - written in C++-11 and has 1:1 mapping with Scala


## Using vireo in your project

### Build requirements:
  - autotools
  - libswscale
  - libavformat
  - libavcodec
  - libavutil
  - liblsmash
  - libx264
  - libvpx
  - libwebm
  - libvorbis
  - libvorbisenc
  - libfdk-aac
  - libogg
  - libpng
  - libjpeg

### C++

To build and use vireo in your native code, simply execute

```
  cd vireo
  ./configure --prefix=/path/to/install/dir
  make
  make install
```

Once built, the headers will be copied under `/path/to/install/dir/include` and the compiled library will be under `/path/to/install/dir/lib/`.

If a valid JDK is found, the JNI bindings for scala will be built as well. To disable the JNI bindings, pass --disable-scala to configure.

### Scala

First build and install the C++ library, then

```
  cd vireo/scala/vireo-scala
  mvn install
```

This will produce a vireo-scala.jar in vireo-scala/target.

## Tools

We built a set of tools for various use cases. Please find the list of tools along with instructions on how to use them below.

### C++

List of tools:

* **chunk**: chunks GOPs of the input video as well as the audio track into separate mp4 files.

* **frames**: displays the contents of video files (list of audio/video samples, edit boxes, track durations etc.).

* **psnr**: compares the video quality of a test video against a reference video.

* **remux**: allows remuxing an input file into other compatible mp4 formats (used for testing DASH).

* **stitch**: stitches a number of input video files into a single video file.

* **thumbnails**: extracts keyframes from the input video and saves them as JPG images.

* **transcode**: transcodes an input video into another video with different format (able to change resolution, crop, change bitrate, convert to H.264/MP4/MPEG-TS or VP9/WebM).

* **trim**: trims the input video at desired time boundaries without transcoding the input video.

* **validate**: checks if the video is valid and if so, supported by vireo.

* **viddiff**: checks if two video files are functionally identical or not (does not compare data that does not affect the playback behavior)

.. note:: Every tool is built and used in a similar fashion. Therefore instructions will use `mytool` as the name of the tool; just replace `mytool` with the respective tool name when executing instructions.

To see the usage of the tool simply execute:

```
  ../../build/release/mytool
```

The exception is the `validate` tool which requires `validate -h` to show usage as it will just start listening STDIN for input otherwise.

# Code Examples

## Creating a video from only keyframes

This example shows how to extract keyframes from an original video and package them into an MP4 file without decoding or encoding frames.

.. image:: assets/keyframe-extraction.png
   :height: 300px
   :align: center

### C++

```
  demux::Movie movie("movie.mp4");
  auto keyframes = movie.video_track.filter([](decode::Sample& sample) { return sample.keyframe; });
  mux::MP4 muxer(functional::Video<encode::Sample>(keyframes, encode::Sample::Convert));
  util::save("movie_keyframes.mp4", muxer());
```

### Scala

```
  for (movie <- managed(demux.Movie("movie.mp4"))) {
    val keyframes = movie.videoTrack.filter[decode.Sample](_.keyframe)
    for {
      muxer <- managed(mux.MP4(keyframes.map(encode.Sample(_: decode.Sample))))
      output <- managed(new java.io.FileOutputStream("movie_keyframes.mp4"))
    } {
      output.write(muxer().array())
    }
  }
```

## H.264 Transcoding

### Scala

```
  for {
    movie <- managed(demux.Movie("movie.mp4"))
    decoder <- managed(decode.Video(movie.videoTrack))
    encoder <- managed(encode.H264(decoder, 25.0f, 3, movie.videoTrack.fps()))
    muxer <- managed(mux.MP4(encoder))
    output <- managed(new java.io.FileOutputStream("movie_transcoded.mp4"))
  } {
    output.write(muxer().array())
  }
```

The JPG encoder and H.264 encoder have a very similar interface, so a straightforward implementation is to add the YUV frame lambdas from the H.264 decoder both in H.264 and JPG encoder.

```
  for {
    movie <- managed(demux.Movie("movie.mp4"))
    decoder <- managed(decode.Video(movie.videoTrack))
  } {
    // Transcode video
    for {
      encoder <- managed(encode.H264(decoder, 25.0f, 3, movie.videoTrack.fps()))
      muxer <- managed(mux.MP4(encoder))
      output <- managed(new java.io.FileOutputStream("movie_transcoded.mp4"))
    } {
      output.write(muxer().array())
    }
    // Write JPG thumbnails
    for (jpgEncoder <- managed(encode.JPG(decoder map { frame: Frame => frame.yuv }, 95, 1))) {
      var index = 0
      jpgEncoder foreach { jpgData =>
        val filename = s"thumbnail_$index.jpg"
        for (output <- managed(new java.io.FileOutputStream(filename))) {
          output.write(jpgData().array())
          index += 1
        }
      }
    }
  }
```

.. warning:: Although this example is functional, it is not optimal. With this code each keyframe is decoded **twice**! Once by the H.264 encoder and also once by the JPG encoder.

As an optimization we can cache the raw frames as they are streamed into the H.264 encoder.

```
    // Cache frames
    val frames = decoder map { frame: Frame =>
      val yuv = frame.yuv()
      Frame(frame.pts, () => yuv)
    }
    // Transcode video
    for {
      encoder <- managed(encode.H264(frames, 25.0f, 3, movie.videoTrack.fps()))
      ...
    }
    // Write JPG thumbnails
    for (jpgEncoder <- managed(encode.JPG(frames map { frame: Frame => frame.yuv }, 95, 1))) {
      ...
    }
```

However this does not scale well when videos have too many keyframes as raw frames are very memory heavy and we would most likely run out of memory.

Instead we can embed the JPG encoder within the H.264 encoder lambda and create the thumbnail right after we decode a frame during video transcoding.

```
    // Transcode video and store thumbnails at the same time
    var index = 0
    for {
      encoder <- managed(encode.H264(decoder map { frame: Frame =>
        // Override frame.yuv function
        val yuv = () => {
          // Force decode frame
          val yuv = frame.yuv()
          // Write JPG thumbnail
          val thumbnail = functional.Video(Seq(() => yuv), decoder.settings)
          for (jpgEncoder <- managed(encode.JPG(thumbnail, 95, 1))) {
            val filename = s"thumbnail_$index.jpg"
            for (output <- managed(new java.io.FileOutputStream(filename))) {
              output.write(jpgEncoder(0)().array())
              index += 1
            }
          }
          yuv
        }
        Frame(frame.pts, yuv)
      }, 25.0f, 3, movie.videoTrack.fps()))
    }

```

This is now optimal both in terms of memory and CPU usage!

.. note:: This exercise is a good demonstration of the 'multiresolution' philoshophy: make a high level prototype and make further refinements as needs arise.
