package com.twitter.vireo

import com.twitter.vireo.common.EditBox
import com.twitter.vireo.encode._
import com.twitter.vireo.frame._
import com.twitter.vireo.TestCommon._
import org.junit.Test
import org.junit.Assert._
import resource._

object AudioEncoder {
  def aac(input: String, output: String) = {
    val srcPath = TestSrcPath + "/" + input
    val dstPath = TestDstPath + "/" + output
    for {
      movie <- managed(demux.Movie(srcPath))
      decoder <- managed(decode.Audio(movie.audioTrack))
      aacEncoder <- managed(encode.AAC(decoder, 1.toByte, 48 * 1024))
      mp4Encoder <- managed(mux.MP4(aacEncoder, functional.Video[() => encode.Sample]()))
      output <- managed(new java.io.FileOutputStream(dstPath))
    } {
      assertEquals(decoder.count, aacEncoder.count)
      output.write(mp4Encoder().array())
    }
  }
}

class EncodeAACTest {
  @Test
  def testEncodeAAC(): Unit = {
    AudioEncoder.aac("paris.m4a", "paris_reencoded-mono.m4a")
  }
}

class EncodeVorbisTest {
  @Test
  def testEncodeVorbis(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/paris.m4a"))
      decoder <- managed(decode.Audio(movie.audioTrack))
      vorbisEncoder <- managed(encode.Vorbis(decoder, 1.toByte, 48 * 1024))
      webMEncoder <- managed(mux.WebM(vorbisEncoder, functional.Video[() => encode.Sample]()))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/paris-mono.webm"))
    } {
      assertEquals(decoder.count, movie.audioTrack.count)
      output.write(webMEncoder().array())
    }
  }
}

class EncodePCMTest {
  @Test
  def testEncodePCM_S16LE(): Unit = {
    AudioEncoder.aac("pcm_s16le.mov", "pcm_s16le-aac.mp4")
  }

  @Test
  def testEncodePCM_S16LE_LPCM(): Unit = {
    AudioEncoder.aac("pcm_s16le-lpcm.mov", "pcm_s16le-lpcm-aac.mp4")
  }

  @Test
  def testEncodePCM_S16BE(): Unit = {
    AudioEncoder.aac("pcm_s16be.mov", "pcm_s16be-aac.mp4")
  }

  @Test
  def testEncodePCM_S16BE_LPCM(): Unit = {
    AudioEncoder.aac("pcm_s16be-lpcm.mov", "pcm_s16be-lpcm-aac.mp4")
  }

  @Test
  def testEncodePCM_S24LE(): Unit = {
    AudioEncoder.aac("pcm_s24le.mov", "pcm_s24le-aac.mp4")
  }

  @Test
  def testEncodePCM_S24BE(): Unit = {
    AudioEncoder.aac("pcm_s24be.mov", "pcm_s24be-aac.mp4")
  }
}

class EncodeJPGTest {
  @Test
  def testEncodeJPG(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/1080.mov"))
      decoder <- managed(decode.Video(movie.videoTrack))
      jpgEncoder <- managed(encode.JPG(decoder map { frame: Frame => () => frame.yuv() scale(1, 4) }, 95, 1))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/image.jpg"))
    } {
      output.write(jpgEncoder(0)().array())
    }
  }
}

class EncodePNGTest {
  @Test
  def testEncodePNG(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/1080.mov"))
      decoder <- managed(decode.Video(movie.videoTrack))
      pngEncoder <- managed(encode.PNG(decoder map { frame: Frame => () => frame.rgb() scale(1, 4) }))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/image.png"))
    } {
      output.write(pngEncoder(0)().array())
    }
  }
}

class EncodeVideoTest {
  private[this] def h264(bitrate: Int, threadCount: Int): Unit = {
    val srcPath = TestSrcPath + "/sochi.mp4"
    val dstPath = if (bitrate == 0) {
      TestDstPath + "/sochi.mp4"
    } else {
      TestDstPath + "/sochi_" + bitrate + "kbps.mp4"
    }
    for {
      movie <- managed(demux.Movie(srcPath))
      decoder <- managed(decode.Video(movie.videoTrack))
      h264Encoder <- managed(encode.H264(decoder, 25.0f, 3, movie.videoTrack.fps(), bitrate))
      mp4Encoder <- managed(mux.MP4(functional.Audio[() => encode.Sample](), h264Encoder, Seq[EditBox]()))
      output <- managed(new java.io.FileOutputStream(dstPath))
    } {
      assertEquals(decoder.count, h264Encoder.count)
      output.write(mp4Encoder().array())
    }
  }

  private[this] def h264(bitrate: Int) {
    h264(bitrate, 0)
  }

  @Test
  def testEncodeH264(): Unit = {
    h264(0)
  }

  @Test
  def testEncodeH264_200kbps(): Unit = {
    h264(200)
  }

  @Test
  def testEncodeH264_MultiThreading(): Unit = {
    h264(0, 4)
  }

  @Test
  def testEncodeH264_Portrait(): Unit = {
    val srcPath = TestSrcPath + "/1080-portrait.mov"
    val dstPath = TestDstPath + "/1080-portrait-encode.mp4"
    for {
      movie <- managed(demux.Movie(srcPath))
      decoder <- managed(decode.Video(movie.videoTrack))
      h264Encoder <- managed(encode.H264(
        decoder map ({ frame: Frame =>
          Frame(frame.pts, () => frame.yuv().rotate(Rotation.Right))
        }, { unrotatedSettings =>
          settings.Video(
            unrotatedSettings.codec,
            unrotatedSettings.height,
            unrotatedSettings.width,
            unrotatedSettings.parWidth,
            unrotatedSettings.parHeight,
            unrotatedSettings.timescale,
            settings.Video.Orientation.Landscape,
            unrotatedSettings.spsPps)
        }),
        25.0f, 3, movie.videoTrack.fps()))
      mp4Encoder <- managed(mux.MP4(h264Encoder))
      output <- managed(new java.io.FileOutputStream(dstPath))
    } {
      assertEquals(decoder.count, h264Encoder.count)
      output.write(mp4Encoder().array())
    }
  }

  @Test
  def testEncodeH264_30to15fps(): Unit = {
    val srcPath = TestSrcPath + "/michael.mp4"
    val dstPath = TestDstPath + "/michael-15fps.mp4"
    for {
      movie <- managed(demux.Movie(srcPath))
      decoder <- managed(decode.Video(movie.videoTrack))
      h264Encoder <- managed(encode.H264(
        decoder.zipWithIndex filter { frameAndIndex: (frame.Frame, Int) =>
          frameAndIndex._2 % 2 == 0
        } map { frameAndIndex: (frame.Frame, Int) => frameAndIndex._1 },
        25.0f, 3, movie.videoTrack.fps / 2.toFloat))
      mp4Encoder <- managed(mux.MP4(h264Encoder))
      output <- managed(new java.io.FileOutputStream(dstPath))
    } {
      assertEquals(decoder.count / 2, h264Encoder.count)
      val array = mp4Encoder()
      output.write(mp4Encoder().array())
    }
  }

  @Test
  def testEncodeH264_FromGif(): Unit = {
    val srcPath = TestSrcPath + "/jack_animated.gif"
    val dstPath = TestDstPath + "/jack_animated.mp4"
    for {
      movie <- managed(demux.Movie(srcPath))
      decoder <- managed(decode.Video(movie.videoTrack))
    } {
      val fps = movie.videoTrack.count * movie.videoTrack.settings.timescale / movie.videoTrack.duration()
      for {
        h264Encoder <- managed(encode.H264(decoder, 25.0f, 3, fps, 0))
        mp4Encoder <- managed(mux.MP4(functional.Audio[() => encode.Sample](), h264Encoder, Seq[EditBox]()))
        output <- managed(new java.io.FileOutputStream(dstPath))
      } {
        assertEquals(decoder.count, h264Encoder.count)
        output.write(mp4Encoder().array())
      }
    }
  }

  @Test
  def testEncodeH264_FromProvidedH264Params(): Unit = {
    val srcPath = TestSrcPath + "/train.mp4"
    val dstPath = TestDstPath + "/train_1Mbps.mp4"
    for {
      movie <- managed(demux.Movie(srcPath))
      decoder <- managed(decode.Video(movie.videoTrack))
    } {
      val params = H264Params(
        ComputationalParams(3, 4),
        RateControlParams(RCMethod.CBR, 0, 1000, 1000, 1000, 0.5f),
        GopParams(0),
        VideoProfile.Main,
        movie.videoTrack.fps())
      for {
        h264Encoder <- managed(encode.H264(decoder.slice(0, 10), params))
        mp4Encoder <- managed(mux.MP4(functional.Audio[() => encode.Sample](), h264Encoder, Seq[EditBox]()))
        output <- managed(new java.io.FileOutputStream(dstPath))
      } {
        assertEquals(10, h264Encoder.count)
        output.write(mp4Encoder().array())
      }
    }
  }

  @Test
  def testEncodeH264_FromProvidedProfile(): Unit = {
    val srcPath = TestSrcPath + "/facebook.mp4"
    val dstPath = TestDstPath + "/facebook_baseline.mp4"
    for {
      movie <- managed(demux.Movie(srcPath))
      decoder <- managed(decode.Video(movie.videoTrack))
    } {
      val params = H264Params(
        ComputationalParams(3, 4),
        RateControlParams(RCMethod.CBR, 0, 1000, 1000, 1000, 0.5f),
        GopParams(0),
        VideoProfile.Baseline,
        movie.videoTrack.fps())
      for {
        h264Encoder <- managed(encode.H264(decoder.slice(0, 10), params))
        mp4Encoder <- managed(mux.MP4(functional.Audio[() => encode.Sample](), h264Encoder, Seq[EditBox]()))
        output <- managed(new java.io.FileOutputStream(dstPath))
      } {
        assertEquals(10, h264Encoder.count)
        output.write(mp4Encoder().array())
      }
    }
  }

  @Test
  def testEncodeH264_Mp4ToMp4WithBFrames(): Unit = {
    val srcPath = TestSrcPath + "/train.mp4"
    val dstPath = TestDstPath + "/train_with_bframes.mp4"
    for {
      movie <- managed(demux.Movie(srcPath))
      decoder <- managed(decode.Video(movie.videoTrack))
    } {
      val params = H264Params(
        ComputationalParams(3, 4),
        RateControlParams(RCMethod.CBR, 0, 1000, 1000, 1000, 0.5f),
        GopParams(3),
        VideoProfile.Main,
        movie.videoTrack.fps())
      for {
        h264Encoder <- managed(encode.H264(decoder.slice(0, 10), params))
        mp4Encoder <- managed(mux.MP4(functional.Audio[() => encode.Sample](), h264Encoder, Seq[EditBox]()))
        output <- managed(new java.io.FileOutputStream(dstPath))
      } {
        assertEquals(10, h264Encoder.count)
        output.write(mp4Encoder().array())
      }
    }
  }

  @Test
  def testEncodeH264_Mp4ToMp2tsWithBFrames(): Unit = {
    val srcPath = TestSrcPath + "/train.mp4"
    val dstPath = TestDstPath + "/train_with_bframes.ts"
    for {
      movie <- managed(demux.Movie(srcPath))
      decoder <- managed(decode.Video(movie.videoTrack))
    } {
      val params = H264Params(
        ComputationalParams(3, 4),
        RateControlParams(RCMethod.CBR, 0, 1000, 1000, 1000, 0.5f),
        GopParams(3),
        VideoProfile.Main,
        movie.videoTrack.fps())
      for {
        h264Encoder <- managed(encode.H264(decoder.slice(0, 10), params))
        mp4Encoder <- managed(mux.MP2TS(functional.Audio[() => encode.Sample](), h264Encoder))
        output <- managed(new java.io.FileOutputStream(dstPath))
      } {
        assertEquals(10, h264Encoder.count)
        output.write(mp4Encoder().array())
      }
    }
  }
}

class EncodeVP8Test {
  private[this] def vp8(bitrate: Int) {
    val srcPath = TestSrcPath + "/sochi.mp4"
    val dstPath = if (bitrate == 0) {
    TestDstPath + "/sochi.webm"
    } else {
      TestDstPath + "/sochi_" + bitrate + "kbps.webm"
    }
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/sochi.mp4"))
      decoder <- managed(decode.Video(movie.videoTrack))
      vp8Encoder <- managed(encode.VP8(decoder, 25, 0, movie.videoTrack.fps(), bitrate))
      webMEncoder <- managed(mux.WebM(vp8Encoder))
      output <- managed(new java.io.FileOutputStream(dstPath))
    } {
      assertEquals(movie.videoTrack.count, decoder.count)
      assertEquals(decoder.count, vp8Encoder.count)
      output.write(webMEncoder().array())
    }
  }

  @Test
  def testEncodeVP8(): Unit = {
    vp8(0)
  }

  @Test
  def testEncodeVP8_200kbps(): Unit = {
    vp8(200)
  }
}

// Demonstrates how an MP4 (H.264+AAC) file is transcoded to a WebM (VP8+Vorbis) file
class EncodeWebMTest {
  @Test
  def testEncodeWebM(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/360-quicktime.mov"))
      videoDecoder <- managed(decode.Video(movie.videoTrack))
      audioDecoder <- managed(decode.Audio(movie.audioTrack))
      vp8Encoder <- managed(encode.VP8(videoDecoder, 25, 0, movie.videoTrack.fps()))
      vorbisEncoder <- managed(encode.Vorbis(audioDecoder, movie.audioTrack.settings.channels, 48 * 1024))
      webMEncoder <- managed(mux.WebM(vorbisEncoder, vp8Encoder))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/360-quicktime.webm"))
    } {
      output.write(webMEncoder().array())
    }
  }
}

class EncodeMp4ToMp2tsCaptionTest {
  @Test
  def testEncodeMp4ToMp2tsCaption(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/caption.mp4"))
      videoDecoder <- managed(decode.Video(movie.videoTrack))
      h264Encoder <- managed(encode.H264(videoDecoder, 25, 0, movie.videoTrack.fps()))
      mp2tsEncoder <- managed(mux.MP2TS(movie.audioTrack.map(encode.Sample(_: decode.Sample)), h264Encoder, movie.captionTrack.map(encode.Sample(_: decode.Sample))))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/caption-transcoded-from-mp4.ts"))
    } {
      output.write(mp2tsEncoder().array())
    }
  }
}

class EncodeMp4ToMp4CaptionTest {
  @Test
  def testEncodeMp4ToMp4Caption(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/caption.mp4"))
      videoDecoder <- managed(decode.Video(movie.videoTrack))
      h264Encoder <- managed(encode.H264(videoDecoder, 25, 0, movie.videoTrack.fps()))
      mp4Encoder <- managed(mux.MP4(movie.audioTrack.map(encode.Sample(_: decode.Sample)), h264Encoder, movie.captionTrack.map(encode.Sample(_: decode.Sample))))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/caption-transcoded-from-mp4.mp4"))
    } {
      output.write(mp4Encoder().array())
    }
  }
}

class EncodeMp2tsToMp4CaptionTest {
  @Test
  def testEncodeMp2tsToMp4Caption(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/caption.ts"))
      videoDecoder <- managed(decode.Video(movie.videoTrack))
      h264Encoder <- managed(encode.H264(videoDecoder, 25, 0, movie.videoTrack.fps()))
      mp4Encoder <- managed(mux.MP4(movie.audioTrack.map(encode.Sample(_: decode.Sample)), h264Encoder, movie.captionTrack.map(encode.Sample(_: decode.Sample))))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/caption-transcoded-from-ts.mp4"))
    } {
      output.write(mp4Encoder().array())
    }
  }
}

class EncodeMp2tsToMp2tsCaptionTest {
  @Test
  def testEncodeMp2tsToMp2tsCaption(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/caption.ts"))
      videoDecoder <- managed(decode.Video(movie.videoTrack))
      h264Encoder <- managed(encode.H264(videoDecoder, 25, 0, movie.videoTrack.fps()))
      mp2tsEncoder <- managed(mux.MP2TS(movie.audioTrack.map(encode.Sample(_: decode.Sample)), h264Encoder, movie.captionTrack.map(encode.Sample(_: decode.Sample))))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/caption-transcoded-from-ts.ts"))
    } {
      output.write(mp2tsEncoder().array())
    }
  }
}

