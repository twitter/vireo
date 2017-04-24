package com.twitter.vireo

import com.twitter.vireo.common.EditBox
import com.twitter.vireo.TestCommon._
import org.junit.Assert._
import org.junit.Test
import resource._

object Mp4Remuxer {
  def passThrough(input: String, output: String) = {
    val srcPath = TestSrcPath + "/" + input
    val dstPath = TestDstPath + "/" + output
    for {
      movie <- managed(demux.Movie(srcPath))
      mp4Encoder <- managed(mux.MP4(
        movie.audioTrack.map(encode.Sample(_: decode.Sample)),
        movie.videoTrack.map(encode.Sample(_: decode.Sample)),
        movie.videoTrack.editBoxes ++ movie.audioTrack.editBoxes))
      output <- managed(new java.io.FileOutputStream(dstPath))
    } {
      output.write(mp4Encoder().array())
    }
  }
}

class RemuxPassThroughTest {
  @Test
  def testPassThrough(): Unit = {
    Mp4Remuxer.passThrough("1080.mov", "1080.mp4")
  }

  @Test
  def testPassThroughLandscapeReverse(): Unit = {
    Mp4Remuxer.passThrough("1080-landscape-reverse.mov", "1080-landscape-reverse.mp4")
  }

  @Test
  def testPassThroughPortrait(): Unit = {
    Mp4Remuxer.passThrough("1080-portrait.mov", "1080-portrait.mp4")
  }

  @Test
  def testPassThroughPortraitReverse(): Unit = {
    Mp4Remuxer.passThrough("1080-portrait-reverse.mov", "1080-portrait-reverse.mp4")
  }

  @Test
  def testPassThroughInMemory(): Unit = {
    val file = new java.io.File(TestSrcPath + "/1080.mov")
    for { input <- managed(new java.io.FileInputStream(file)) } {
      val data = new Array[Byte](file.length().toInt)
      input.read(data)
      for {
        movieIntermediate <- managed(demux.Movie(data))
        mp4EncoderIntermediate <- managed(mux.MP4(
          movieIntermediate.audioTrack.map(encode.Sample(_: decode.Sample)),
          movieIntermediate.videoTrack.map(encode.Sample(_: decode.Sample)),
          movieIntermediate.audioTrack.editBoxes ++ movieIntermediate.videoTrack.editBoxes))
        movie <- managed(demux.Movie(mp4EncoderIntermediate()))
        mp4Encoder <- managed(mux.MP4(
          movie.audioTrack.map(encode.Sample(_: decode.Sample)),
          movie.videoTrack.map(encode.Sample(_: decode.Sample)),
          movie.audioTrack.editBoxes ++ movie.videoTrack.editBoxes))
        output <- managed(new java.io.FileOutputStream(TestDstPath + "/1080.mp4"))
      } {
        assertEquals(movie.audioTrack.editBoxes().length, movieIntermediate.audioTrack.editBoxes().length)
        assertEquals(movie.videoTrack.editBoxes().length, movieIntermediate.videoTrack.editBoxes().length)
        output.write(mp4Encoder().array())
      }
    }
  }
}

class RemuxAACTest {
  @Test
  def testAAC(): Unit = {
    Mp4Remuxer.passThrough("paris.m4a", "paris.m4a")
  }
}

class RemuxPCMTest {
  @Test
  def testPCM_S16LE(): Unit = {
    Mp4Remuxer.passThrough("pcm_s16le.mov", "pcm_s16le.mov")
  }

  @Test
  def testPCM_S16LE_LPCM(): Unit = {
    Mp4Remuxer.passThrough("pcm_s16le-lpcm.mov", "pcm_s16le-sowt.mov")
  }

  @Test
  def testPCM_S16BE(): Unit = {
    Mp4Remuxer.passThrough("pcm_s16be.mov", "pcm_s16be.mov")
  }

  @Test
  def testPCM_S16BE_LPCM(): Unit = {
    Mp4Remuxer.passThrough("pcm_s16be-lpcm.mov", "pcm_s16be-twos.mov")
  }

  @Test
  def testPCM_S24LE(): Unit = {
    Mp4Remuxer.passThrough("pcm_s24le.mov", "pcm_s24le.mov")
  }

  @Test
  def testPCM_S24BE(): Unit = {
    Mp4Remuxer.passThrough("pcm_s24be.mov", "pcm_s24be.mov")
  }
}

class RemuxSoundTrackTest {
  @Test
  def testSoundTrack(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/rosebud.mp4"))
      mp4Encoder <- managed(mux.MP4(
        movie.audioTrack.map(encode.Sample(_: decode.Sample)), functional.Video[() => encode.Sample]())
      )
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/rosebud.m4a"))
    } {
      output.write(mp4Encoder().array())
    }
  }
}

class RemuxMP2TSTest {
  @Test
  def testMP2TS(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/1080.mov"))
      mp2tsEncoder <- managed(mux.MP2TS(
        movie.audioTrack.map(encode.Sample(_: decode.Sample)),
        movie.videoTrack.map(encode.Sample(_: decode.Sample)))
      )
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/1080.ts"))
    } {
      output.write(mp2tsEncoder().array())
    }
  }
}

class RemuxDashTest {
  @Test
  def testDash(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/train.mp4"))
    } {
      // Video
      // Initializer segment
      for {
        mp4Encoder <- managed(mux.MP4(
          functional.Audio[() => encode.Sample](),
          functional.Video[() => encode.Sample](movie.videoTrack.settings),
          Seq[EditBox](),
          FileFormat.DashInitializer)
        )
        output <- managed(new java.io.FileOutputStream(TestDstPath + "/train_video.mp4"))
      } {
        output.write(mp4Encoder().array())
      }
      // Data segment
      val G = 2 // number of GOPs to contain in one data segment
      val keyFrameIdx = movie.videoTrack.zipWithIndex.filter((x: (decode.Sample, Int)) => x._1.keyframe).map((x: (decode.Sample, Int)) => x._2).zipWithIndex.filter((x: (Int, Int)) => x._2 % G == 0).map((x: (Int, Int)) => x._1) ++ List(movie.videoTrack.toList.length)
      keyFrameIdx.sliding(2).toList.zipWithIndex.foreach ({ case (Seq(from, until), idx) =>
        for {
          mp4Encoder <- managed(mux.MP4(
            functional.Audio[() => encode.Sample](),
            movie.videoTrack.slice(from, until).map(encode.Sample(_: decode.Sample)),
            Seq[EditBox](),
            FileFormat.DashData)
          )
          output <- managed(new java.io.FileOutputStream(TestDstPath + "/train_video_" + (idx + 1) + ".m4s"))
        } {
          output.write(mp4Encoder().array())
        }
      })

      // Audio
      // Initializer segment
      for {
        mp4Encoder <- managed(mux.MP4(
          functional.Audio[() => encode.Sample](movie.audioTrack.settings),
          functional.Video[() => encode.Sample](),
          Seq[EditBox](),
          FileFormat.DashInitializer)
        )
        output <- managed(new java.io.FileOutputStream(TestDstPath + "/train_audio.m4a"))
      } {
        output.write(mp4Encoder().array())
      }
      // Data segment
      for {
        mp4Encoder <- managed(mux.MP4(
          movie.audioTrack.map(encode.Sample(_: decode.Sample)),
          functional.Video[() => encode.Sample](),
          Seq[EditBox](),
          FileFormat.DashData)
        )
        output <- managed(new java.io.FileOutputStream(TestDstPath + "/train_audio_1.m4s"))
      } {
        output.write(mp4Encoder().array())
      }
    }
  }
}

class RemuxMP2TStoMP4Test {
  @Test
  def testMP2TStoMP4(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/periscope.ts"))
      mp4Encoder <- managed(mux.MP4(
        movie.audioTrack.map(encode.Sample(_: decode.Sample)),
        movie.videoTrack.map(encode.Sample(_: decode.Sample))))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/periscope.mp4"))
    } {
      output.write(mp4Encoder().array())
    }
  }
}

class RemuxDistributedMuxTest {
  @Test
  def testDistributedMux(): Unit = {
    case class DtsPair(video: Long, audio: Long)
    var dtsAtGOPBoundaries = Seq[DtsPair]()

    def isDtsOrdered(file: String, dtsPair: DtsPair = DtsPair(0, 0)): Boolean = {
      for { movie <- managed(demux.Movie(file)) } {
        val videoSamples = movie.videoTrack map { (x: decode.Sample) =>
          new decode.Sample(x.pts, x.dts + dtsPair.video, x.keyframe, x.sampleType, x.nal, x.byteRange)
        }
        val audioSamples = movie.audioTrack map { (x: decode.Sample) =>
          new decode.Sample(x.pts, x.dts + dtsPair.audio, x.keyframe, x.sampleType, x.nal, x.byteRange)
        }
        val samples = (videoSamples ++ audioSamples).sortWith(_.byteRange.map(_.pos).get < _.byteRange.map(_.pos).get)
        val dtsList = samples.map { (x: decode.Sample) =>
          val timescale = if (x.sampleType == SampleType.Video) {
            movie.videoTrack.settings.timescale
          } else {
            movie.audioTrack.settings.timescale
          }
          x.dts.toFloat / timescale
        }
        dtsList.sliding(2).foreach { case Seq(x, y) => if (x > y) return false }
      }
      true
    }

    for { movie <- managed(demux.Movie(TestSrcPath + "/train.mp4")) } {
      val videoSettings = movie.videoTrack.settings
      val audioSettings = movie.audioTrack.settings

      dtsAtGOPBoundaries = movie.videoTrack.filter((x: decode.Sample) => x.keyframe).map { (x: decode.Sample) =>
        val videoDts = x.dts
        val adjustedVideoDts = common.Math.roundDivide(videoDts, audioSettings.timescale, videoSettings.timescale)
        val audioDts = movie.audioTrack.map((x: decode.Sample) => x.dts).find(_ >= adjustedVideoDts).get
        new DtsPair(videoDts, audioDts)
      } ++ Seq(new DtsPair(Long.MaxValue, Long.MaxValue))

      // Save samples and headers for each GOP separately
      dtsAtGOPBoundaries.sliding(2).zipWithIndex.foreach { case (Seq(from, until), idx) =>
        val audioTrack = movie.audioTrack.filter((x: decode.Sample) => x.dts >= from.audio && x.dts < until.audio)
        val videoTrack = movie.videoTrack.filter((x: decode.Sample) => x.dts >= from.video && x.dts < until.video)
        val samplesFile = TestDstPath + "/dist-mux-samples-" + idx + ".mp4"
        val headerFile = TestDstPath + "/dist-mux-header-" + idx + ".mp4"
        for {
          mp4Encoder <- managed(mux.MP4(
            audioTrack.map(encode.Sample(_: decode.Sample)),
            videoTrack.map(encode.Sample(_: decode.Sample)),
            Seq[EditBox]())
          )
          outputSamples <- managed(new java.io.FileOutputStream(samplesFile))
          outputHeaders <- managed(new java.io.FileOutputStream(headerFile))
        } {
          outputSamples.write(mp4Encoder(FileFormat.SamplesOnly).array())
          outputHeaders.write(mp4Encoder(FileFormat.HeaderOnly).array())
          assertEquals(isDtsOrdered(headerFile, from), true)
        }
      }
    }

    // Read individual GOP headers and create the final header
    var audioSamples = Seq[encode.Sample]()
    var videoSamples = Seq[encode.Sample]()
    var audioSettings = settings.Audio.None
    var videoSettings = settings.Video.None
    dtsAtGOPBoundaries.sliding(2).zipWithIndex.foreach { case (Seq(from, until), idx) =>
      for (movie <- managed(demux.Movie(TestDstPath + "/dist-mux-header-" + idx + ".mp4"))) {
        if (idx == 0) {
          audioSettings = movie.audioTrack.settings
          videoSettings = movie.videoTrack.settings
        }
        val gopAudioTrack = movie.audioTrack map { (x: decode.Sample) =>
          require(x.byteRange.isDefined)
          new encode.Sample(x.pts, x.dts + from.audio, x.keyframe, x.sampleType, common.Data(x.byteRange.get.size))
        }
        val gopVideoTrack = movie.videoTrack map { (x: decode.Sample) =>
          require(x.byteRange.isDefined)
          new encode.Sample(x.pts, x.dts + from.video, x.keyframe, x.sampleType, common.Data(x.byteRange.get.size))
        }
        audioSamples = audioSamples ++ gopAudioTrack
        videoSamples = videoSamples ++ gopVideoTrack
      }
    }
    val audioTrack = functional.Audio[() => encode.Sample](audioSamples.map(x => () => x), audioSettings)
    val videoTrack = functional.Video[() => encode.Sample](videoSamples.map(x => () => x), videoSettings)
    val headerFile = TestDstPath + "/dist-mux-header.mp4"
    for {
      mp4Encoder <- managed(mux.MP4(audioTrack, videoTrack, Seq[EditBox]()))
      output <- managed(new java.io.FileOutputStream(headerFile))
    } {
      output.write(mp4Encoder(FileFormat.HeaderOnly).array())
      assertEquals(isDtsOrdered(headerFile), true)
    }

    // Concatenate final header and samples of each GOP to get the final muxed file
    for (output <- managed(new java.io.FileOutputStream(TestDstPath + "/train-dist-muxed.mp4"))) {
      val header = common.Data(TestDstPath + "/dist-mux-header.mp4")
      output.write(header.array)
      dtsAtGOPBoundaries.sliding(2).zipWithIndex.map(_._2) foreach { idx =>
        val samples = common.Data(TestDstPath + "/dist-mux-samples-" + idx + ".mp4")
        output.write(samples.array)
      }
    }
  }
}

class RemuxMP4ToMP2TSCaptionTest {
  @Test
  def testMP4ToMP2TSCaption(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/caption.mp4"))
      mp2tsEncoder <- managed(mux.MP2TS(
        movie.audioTrack.map(encode.Sample(_: decode.Sample)),
        movie.videoTrack.map(encode.Sample(_: decode.Sample)),
        movie.captionTrack.map(encode.Sample(_: decode.Sample))))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/caption-remuxed-from-mp4.ts"))
    } {
      output.write(mp2tsEncoder().array())
    }
  }
}

class RemuxMP4ToMP4CaptionTest {
  @Test
  def testMP4ToMP4Caption(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/caption.mp4"))
      mp4Encoder <- managed(mux.MP4(
        movie.audioTrack.map(encode.Sample(_: decode.Sample)),
        movie.videoTrack.map(encode.Sample(_: decode.Sample)),
        movie.captionTrack.map(encode.Sample(_: decode.Sample))))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/caption-remuxed-from-mp4.mp4"))
    } {
      output.write(mp4Encoder().array())
    }
  }
}

class RemuxMP4ToMP2TSCaptionNotPresentTest {
  @Test
  def testMP4ToMP2TSCaptionNotPresent(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/bruce.mp4"))
      mp2tsEncoder <- managed(mux.MP2TS(
        movie.audioTrack.map(encode.Sample(_: decode.Sample)),
        movie.videoTrack.map(encode.Sample(_: decode.Sample)),
        movie.captionTrack.map(encode.Sample(_: decode.Sample))))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/bruce-remuxed-from-mp4.ts"))
    } {
      output.write(mp2tsEncoder().array())
    }
  }
}

class RemuxMP4ToMP4CaptionNotPresentTest {
  @Test
  def testMP4ToMP4CaptionNotPresent(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/bruce.mp4"))
      mp4Encoder <- managed(mux.MP4(
        movie.audioTrack.map(encode.Sample(_: decode.Sample)),
        movie.videoTrack.map(encode.Sample(_: decode.Sample)),
        movie.captionTrack.map(encode.Sample(_: decode.Sample))))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/bruce-remuxed-from-mp4.mp4"))
    } {
      output.write(mp4Encoder().array())
    }
  }
}

class RemuxMP2TSToMP4CaptionTest {
  @Test
  def testMP2TSToMP4Caption(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/caption.ts"))
      mp4Encoder <- managed(mux.MP4(
        movie.audioTrack.map(encode.Sample(_: decode.Sample)),
        movie.videoTrack.map(encode.Sample(_: decode.Sample)),
        movie.captionTrack.map(encode.Sample(_: decode.Sample))))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/caption-remuxed-from-ts.mp4"))
    } {
      output.write(mp4Encoder().array())
    }
  }
}

class RemuxMP2TSToMP2TSCaptionTest {
  @Test
  def testMP2TSToMP2TSCaption(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/caption.ts"))
      mp2tsEncoder <- managed(mux.MP2TS(
        movie.audioTrack.map(encode.Sample(_: decode.Sample)),
        movie.videoTrack.map(encode.Sample(_: decode.Sample)),
        movie.captionTrack.map(encode.Sample(_: decode.Sample))))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/caption-remuxed-from-ts.ts"))
    } {
      output.write(mp2tsEncoder().array())
    }
  }
}

class RemuxMP2TSToMP4CaptionNotPresentTest {
  @Test
  def testMP2TSToMP4CaptionNotPresent(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/periscope2.ts"))
      mp4Encoder <- managed(mux.MP4(
        movie.audioTrack.map(encode.Sample(_: decode.Sample)),
        movie.videoTrack.map(encode.Sample(_: decode.Sample)),
        movie.captionTrack.map(encode.Sample(_: decode.Sample))))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/periscope2-remuxed-from-ts.mp4"))
    } {
      output.write(mp4Encoder().array())
    }
  }
}

class RemuxMP2TSToMP2TSCaptionNotPresentTest {
  @Test
  def testMP2TSToMP2TSCaptionNotPresent(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/periscope2.ts"))
      mp2tsEncoder <- managed(mux.MP2TS(
        movie.audioTrack.map(encode.Sample(_: decode.Sample)),
        movie.videoTrack.map(encode.Sample(_: decode.Sample)),
        movie.captionTrack.map(encode.Sample(_: decode.Sample))))
      output <- managed(new java.io.FileOutputStream(TestDstPath + "/periscope2-remuxed-from-ts.ts"))
    } {
      output.write(mp2tsEncoder().array())
    }
  }
}
