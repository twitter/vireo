package com.twitter.vireo

import com.twitter.vireo.TestCommon._
import com.twitter.vireo.frame.Frame
import org.junit.Test
import org.junit.Assert._
import resource._

class PeriscopeTest {
  private[this] def encodeWithOrientationFromID3(
    input: String,
    output: String,
    expectedInfos: Seq[periscope.ID3Info]
  ): Unit = {
    val srcPath = TestSrcPath + "/" + input
    val dstPath = TestDstPath + "/" + output
    for { movie <- managed(demux.Movie(srcPath)) } {
      // check ID3 info
      assertEquals(expectedInfos.length, movie.dataTrack.length)
      expectedInfos.zip(movie.dataTrack).foreach {
        case (expectedInfo: periscope.ID3Info, sample: decode.Sample) =>
          val info = periscope.Util.parseID3Info(sample.nal())
          assertEquals(expectedInfo.orientation, info.orientation)
          assertEquals(expectedInfo.ntpTimestamp, info.ntpTimestamp, .0000001)
      }
      for {decoder <- managed(decode.Video(movie.videoTrack))} {
        val firstPts = decoder.head.pts
        val videoSettings = decoder.settings
        val orientation = periscope.Util.parseID3Info(movie.dataTrack.head.nal()).orientation
        val outputWidth = if (orientation % 2 != 0) videoSettings.height else videoSettings.width
        val outputHeight = if (orientation % 2 != 0) videoSettings.width else videoSettings.height
        for {
          h264Encoder <- managed(encode.H264(
            decoder map( { frame: Frame =>
              if (orientation == settings.Video.Orientation.Landscape) {
                Frame(frame.pts - firstPts, frame.yuv)
              } else {
                Frame(frame.pts - firstPts, () => frame.yuv().rotate(orientation))
              }
            }, { inputSettings =>
              settings.Video(
                inputSettings.codec,
                outputWidth,
                outputHeight,
                inputSettings.parWidth,
                inputSettings.parHeight,
                inputSettings.timescale,
                settings.Video.Orientation.Landscape,
                inputSettings.spsPps)
            }), 25.0f, 3, movie.videoTrack.fps()))
          mp4Encoder <- managed(mux.MP4(h264Encoder))
          output <- managed(new java.io.FileOutputStream(dstPath))
        } {
          assertEquals(decoder.count, h264Encoder.count)
          output.write(mp4Encoder().array())
        }
      }
    }
  }

  @Test
  def testPeriscopeDontRotate(): Unit = {
    encodeWithOrientationFromID3("periscope-334deg.ts", "periscope-334deg.mp4",
      Seq(
        periscope.ID3Info(settings.Video.Orientation.Landscape, 3663515468.4840002),
        periscope.ID3Info(settings.Video.Orientation.Landscape, 3663515470.2309999)
      )
    )
  }

  @Test
  def testPeriscopeRotate90(): Unit = {
    encodeWithOrientationFromID3("periscope-92deg.ts", "periscope-92deg.mp4",
      Seq(
        periscope.ID3Info(settings.Video.Orientation.Portrait, 3663430149.546),
        periscope.ID3Info(settings.Video.Orientation.Portrait, 3663430151.178)
      )
    )
  }

  @Test
  def testPeriscopeRotate270(): Unit = {
    encodeWithOrientationFromID3("periscope-271deg.ts", "periscope-271deg.mp4",
      Seq(
        periscope.ID3Info(settings.Video.Orientation.PortraitReverse, 3663515731.5580001)
      )
    )
  }

  @Test
  def testPeriscopeNoTKEY(): Unit = {
    encodeWithOrientationFromID3("periscope2.ts", "periscope-noTKEY.mp4",
      Seq(
        periscope.ID3Info(settings.Video.Orientation.UnknownOrientation, 3644340920.9429998),
        periscope.ID3Info(settings.Video.Orientation.UnknownOrientation, 3644340920.9429998),
        periscope.ID3Info(settings.Video.Orientation.UnknownOrientation, 3644340922.4749999),
        periscope.ID3Info(settings.Video.Orientation.UnknownOrientation, 3644340924.1399999)
      )
    )
  }

}
