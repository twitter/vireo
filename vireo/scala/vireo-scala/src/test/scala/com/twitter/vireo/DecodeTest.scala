package com.twitter.vireo

import com.twitter.vireo.FileType._
import com.twitter.vireo.TestCommon._
import org.junit.Test
import org.junit.Assert._
import resource._

case class SoundInfo(index: Int, offset: Int, value: Int)
case class AudioTestCase(
  filename: String,
  numSounds: Int,
  channels: Byte,
  soundInfos: Seq[SoundInfo]
)

case class RGBA(r: Int, g: Int, b: Int, a: Int)
case class YUV(y: Int, u: Int, v: Int)
case class Pixel(column: Int, row: Int, rgb: RGBA, yuv: YUV)
case class FrameInfo(index: Int, pts: Long, pixels: Seq[Pixel])

case class VideoTestCase(
  filename: String,
  width: Int,
  height: Int,
  numFrames: Int,
  fileType: FileType,
  codec: settings.Video.Codec,
  frameInfos: Seq[FrameInfo],
  threadCount: Int = 1
)

object AudioDecodeTester {
  def test(testCase: AudioTestCase) = {
    val srcPath = TestSrcPath + "/" + testCase.filename
    for {
      movie <- managed(demux.Movie(srcPath))
      decoder <- managed(decode.Audio(movie.audioTrack))
    } {
      assertEquals(decoder.count, testCase.numSounds)

      val channels = movie.audioTrack.settings.channels
      assertEquals(channels, testCase.channels)
      assertEquals(channels, movie.audioTrack.settings.channels)
      for (sound <- decoder) {
        val pcm = sound.pcm()
        assertEquals(pcm.channels, channels)
        assertEquals(pcm.size, 1024)
        assertEquals(pcm.samples.length, pcm.size * channels)
      }
      for (soundInfo <- testCase.soundInfos) {
        assertEquals(decoder(soundInfo.index).pcm().samples(soundInfo.offset), soundInfo.value)
      }
    }
  }
}

object VideoDecodeTester {
  def test(testCase: VideoTestCase) = {
    val srcPath = TestSrcPath + "/" + testCase.filename
    for {
      movie <- managed(demux.Movie(srcPath))
      decoder <- managed(decode.Video(movie.videoTrack, testCase.threadCount))
    } {
      assertEquals(movie.fileType(), testCase.fileType)
      assertEquals(movie.videoTrack.settings.width, testCase.width)
      assertEquals(movie.videoTrack.settings.height, testCase.height)
      assertEquals(movie.videoTrack.settings.codec, testCase.codec)
      assertEquals(decoder.count, testCase.numFrames)
      assertEquals(testCase.width, decoder.settings.width)
      assertEquals(testCase.height, decoder.settings.height)

      for (frameInfo <- testCase.frameInfos) {
        val frame = decoder(frameInfo.index)
        assertEquals(frame.pts, frameInfo.pts)

        for (pixel <- frameInfo.pixels) {
          val rgb = frame.rgb()
          assertEquals(rgb.width, testCase.width)
          assertEquals(rgb.height, testCase.height)
          assertEquals(rgb.plane(pixel.row)(pixel.column * rgb.component_count), pixel.rgb.r.toByte)
          assertEquals(rgb.plane(pixel.row)(pixel.column * rgb.component_count + 1), pixel.rgb.g.toByte)
          assertEquals(rgb.plane(pixel.row)(pixel.column * rgb.component_count + 2), pixel.rgb.b.toByte)
          assertEquals(rgb.plane(pixel.row)(pixel.column * rgb.component_count + 3), pixel.rgb.a.toByte)

          val yuv = frame.yuv()
          assertEquals(yuv.width, testCase.width)
          assertEquals(yuv.height, testCase.height)
          assertEquals(yuv.y(pixel.row)(pixel.column), pixel.yuv.y.toByte)
          assertEquals(yuv.u(pixel.row / 2)(pixel.column / 2), pixel.yuv.u.toByte)
          assertEquals(yuv.v(pixel.row / 2)(pixel.column / 2), pixel.yuv.v.toByte)
        }
      }
    }
  }
}

class DecodeAACTest {
  @Test
  def testDecodeAAC() = {
    AudioDecodeTester.test(AudioTestCase("paris.m4a", 649, 2, Seq(SoundInfo(0, 50, 0), SoundInfo(10, 50, 29), SoundInfo(15, 50, -252), SoundInfo(20, 50, -1203))))
  }
}

class DecodePCMTest {
  @Test
  def testDecodePCM() = {
    AudioDecodeTester.test(AudioTestCase("pcm_s16le.mov", 94, 2, Seq(SoundInfo(0, 50, 0), SoundInfo(10, 50, -312), SoundInfo(15, 50, -5598), SoundInfo(20, 50, -1590))))
    AudioDecodeTester.test(AudioTestCase("pcm_s16le-lpcm.mov", 87, 2, Seq(SoundInfo(0, 50, -444), SoundInfo(10, 50, -5902), SoundInfo(15, 50, 506), SoundInfo(20, 50, 6273))))
    AudioDecodeTester.test(AudioTestCase("pcm_s16be.mov", 94, 2, Seq(SoundInfo(0, 50, 1), SoundInfo(10, 50, 4409), SoundInfo(15, 50, -1867), SoundInfo(20, 50, 710))))
    AudioDecodeTester.test(AudioTestCase("pcm_s16be-lpcm.mov", 141, 2, Seq(SoundInfo(0, 50, 1), SoundInfo(10, 50, 4409), SoundInfo(15, 50, -1867), SoundInfo(20, 50, 710))))
    AudioDecodeTester.test(AudioTestCase("pcm_s24le.mov", 87, 2, Seq(SoundInfo(0, 50, 0), SoundInfo(10, 50, -3274), SoundInfo(15, 50, 2440), SoundInfo(20, 50, -2199))))
    AudioDecodeTester.test(AudioTestCase("pcm_s24be.mov", 94, 2, Seq(SoundInfo(0, 50, 0), SoundInfo(10, 50, -63), SoundInfo(15, 50, -3950), SoundInfo(20, 50, 6363))))
  }
}

class DecodeVideoTest {
  @Test
  def testDecodeH264(): Unit = {
    VideoDecodeTester.test(VideoTestCase("360-quicktime.mov", 640, 360, 61, FileType.MP4, settings.Video.Codec.H264, Seq(
      FrameInfo(0, 0, Seq(Pixel(320, 180, RGBA(115, 100, 80, 255), YUV(105, 117, 136)))),
      FrameInfo(1, 2, Seq(Pixel(320, 180, RGBA(114, 99, 79, 255), YUV(104, 117, 136)))),
      FrameInfo(2, 22, Seq(Pixel(320, 180, RGBA(113, 98, 78, 255), YUV(103, 117, 136)))),
      FrameInfo(3, 42, Seq(Pixel(320, 180, RGBA(113, 97, 80, 255), YUV(103, 118, 136)))),
      FrameInfo(4, 62, Seq(Pixel(320, 180, RGBA(109, 93, 78, 255), YUV(100, 119, 136)))),
      FrameInfo(5, 82, Seq(Pixel(320, 180, RGBA(113, 97, 82, 255), YUV(103, 119, 136)))),
      // random access test
      FrameInfo(0, 0, Seq(Pixel(320, 180, RGBA(115, 100, 80, 255), YUV(105, 117, 136)))),
      FrameInfo(20, 383, Seq(Pixel(320, 180, RGBA(107, 92, 72, 255), YUV(98, 117, 136)))),
      FrameInfo(25, 483, Seq(Pixel(320, 180, RGBA(109, 91, 72, 255), YUV(98, 117, 137)))),
      FrameInfo(30, 583, Seq(Pixel(320, 180, RGBA(106, 92, 76, 255), YUV(98, 119, 135)))),
      FrameInfo(60, 1183, Seq(Pixel(320, 180, RGBA(98, 81, 71, 255), YUV(90, 121, 136)))),
      FrameInfo(5, 82, Seq(Pixel(320, 180, RGBA(113, 97, 82, 255), YUV(103, 119, 136)))),
      FrameInfo(1, 2, Seq(Pixel(320, 180, RGBA(114, 99, 79, 255), YUV(104, 117, 136))))
    )))
  }

  @Test
  def testDecodeH264_NonSquarePixel(): Unit = {
    VideoDecodeTester.test(VideoTestCase("par_3_4_in_sps.mp4", 960, 720, 29, FileType.MP4, settings.Video.Codec.H264, Seq(
      FrameInfo(0, 1001, Seq(Pixel(320, 180, RGBA(0, 1, 0, 255), YUV(16, 126, 126)))),
      FrameInfo(1, 2002, Seq(Pixel(320, 180, RGBA(0, 1, 0, 255), YUV(16, 126, 126)))),
      FrameInfo(2, 3003, Seq(Pixel(320, 180, RGBA(0, 5, 0, 255), YUV(19, 126, 125)))),
      FrameInfo(3, 4004, Seq(Pixel(320, 180, RGBA(11, 21, 13, 255), YUV(32, 126, 124)))),
      FrameInfo(4, 5005, Seq(Pixel(320, 180, RGBA(21, 33, 24, 255), YUV(41, 126, 123)))),
      FrameInfo(5, 6006, Seq(Pixel(320, 180, RGBA(28, 42, 33, 255), YUV(49, 126, 122)))),
      // random access test
      FrameInfo(0, 1001, Seq(Pixel(320, 180, RGBA(0, 1, 0, 255), YUV(16, 126, 126)))),
      FrameInfo(10, 11011, Seq(Pixel(320, 180, RGBA(71, 100, 88, 255), YUV(95, 127, 116)))),
      FrameInfo(20, 21021, Seq(Pixel(320, 180, RGBA(62, 88, 77, 255), YUV(85, 127, 117)))),
      FrameInfo(28, 30030, Seq(Pixel(320, 180, RGBA(60, 86, 75, 255), YUV(83, 127, 117)))),
      FrameInfo(5, 6006, Seq(Pixel(320, 180, RGBA(28, 42, 33, 255), YUV(49, 126, 122)))),
      FrameInfo(1, 2002, Seq(Pixel(320, 180, RGBA(0, 1, 0, 255), YUV(16, 126, 126))))
    )))
  }

  @Test
  def testDecodeH264_MultiThreading(): Unit = {
    VideoDecodeTester.test(VideoTestCase("360-quicktime.mov", 640, 360, 61, FileType.MP4, settings.Video.Codec.H264, Seq(
      FrameInfo(0, 0, Seq(Pixel(320, 180, RGBA(115, 100, 80, 255), YUV(105, 117, 136)))),
      FrameInfo(1, 2, Seq(Pixel(320, 180, RGBA(114, 99, 79, 255), YUV(104, 117, 136)))),
      FrameInfo(2, 22, Seq(Pixel(320, 180, RGBA(113, 98, 78, 255), YUV(103, 117, 136)))),
      FrameInfo(3, 42, Seq(Pixel(320, 180, RGBA(113, 97, 80, 255), YUV(103, 118, 136)))),
      FrameInfo(4, 62, Seq(Pixel(320, 180, RGBA(109, 93, 78, 255), YUV(100, 119, 136)))),
      FrameInfo(5, 82, Seq(Pixel(320, 180, RGBA(113, 97, 82, 255), YUV(103, 119, 136)))),
      // random access test
      FrameInfo(0, 0, Seq(Pixel(320, 180, RGBA(115, 100, 80, 255), YUV(105, 117, 136)))),
      FrameInfo(20, 383, Seq(Pixel(320, 180, RGBA(107, 92, 72, 255), YUV(98, 117, 136)))),
      FrameInfo(25, 483, Seq(Pixel(320, 180, RGBA(109, 91, 72, 255), YUV(98, 117, 137)))),
      FrameInfo(30, 583, Seq(Pixel(320, 180, RGBA(106, 92, 76, 255), YUV(98, 119, 135)))),
      FrameInfo(60, 1183, Seq(Pixel(320, 180, RGBA(98, 81, 71, 255), YUV(90, 121, 136)))),
      FrameInfo(5, 82, Seq(Pixel(320, 180, RGBA(113, 97, 82, 255), YUV(103, 119, 136)))),
      FrameInfo(1, 2, Seq(Pixel(320, 180, RGBA(114, 99, 79, 255), YUV(104, 117, 136))))
    ), 4))
  }
}

class DecodeImageTest {
  @Test
  def testDecodeBMP(): Unit = {
    VideoDecodeTester.test(VideoTestCase("500.bmp", 480, 384, 1, FileType.Image, settings.Video.Codec.BMP, Seq(
      FrameInfo(0, 0, Seq(Pixel(50, 50, RGBA(16, 23, 29, 255), YUV(22, 132, 124))))
    )))
  }

  @Test
  def testDecodeGIF(): Unit = {
    VideoDecodeTester.test(VideoTestCase("jack_animated.gif", 422, 238, 45, FileType.Image, settings.Video.Codec.GIF, Seq(
      FrameInfo(0, 0, Seq(Pixel(200, 100, RGBA(155, 143, 137, 255), YUV(146, 121, 139)))),
      FrameInfo(1, 40, Seq(Pixel(200, 100, RGBA(116, 109, 112, 255), YUV(111, 123, 138)))),
      FrameInfo(2, 80, Seq(Pixel(200, 100, RGBA(137, 124, 123, 255), YUV(128, 125, 137)))),
      FrameInfo(3, 120, Seq(Pixel(200, 100, RGBA(132, 107, 111, 255), YUV(115, 125, 140)))),
      FrameInfo(4, 160, Seq(Pixel(200, 100, RGBA(92, 85, 88, 255), YUV(87, 125, 134)))),
      FrameInfo(5, 200, Seq(Pixel(200, 100, RGBA(77, 78, 85, 255), YUV(79, 127, 134)))),
      // random access test
      FrameInfo(0, 0, Seq(Pixel(200, 100, RGBA(155, 143, 137, 255), YUV(146, 121, 139)))),
      FrameInfo(10, 400, Seq(Pixel(200, 100, RGBA(35, 28, 34, 255), YUV(31, 129, 129)))),
      FrameInfo(20, 800, Seq(Pixel(200, 100, RGBA(42, 42, 48, 255), YUV(43, 131, 128)))),
      FrameInfo(30, 1200, Seq(Pixel(200, 100, RGBA(183, 174, 191, 255), YUV(179, 132, 128)))),
      FrameInfo(42, 1680, Seq(Pixel(200, 100, RGBA(174, 182, 190, 255), YUV(181, 133, 126)))),
      FrameInfo(43, 1718, Seq(Pixel(200, 100, RGBA(174, 182, 190, 255), YUV(181, 133, 126)))),
      FrameInfo(44, 1719, Seq(Pixel(200, 100, RGBA(174, 182, 190, 255), YUV(181, 133, 126)))),
      FrameInfo(5, 200, Seq(Pixel(200, 100, RGBA(77, 78, 85, 255), YUV(79, 127, 134)))),
      FrameInfo(1, 40, Seq(Pixel(200, 100, RGBA(116, 109, 112, 255), YUV(111, 123, 138))))
    )))
  }

  @Test
  def testDecodeJPG(): Unit = {
    VideoDecodeTester.test(VideoTestCase("8mp.jpg", 3264, 2448, 1, FileType.Image, settings.Video.Codec.JPG, Seq(
      FrameInfo(0, 0, Seq(Pixel(50, 50, RGBA(19, 15, 17, 255), YUV(17, 128, 130))))
    )))
    VideoDecodeTester.test(VideoTestCase("rot-270.jpg", 306, 408, 1, FileType.Image, settings.Video.Codec.JPG, Seq(
      FrameInfo(0, 0, Seq(Pixel(50, 50, RGBA(146, 146, 142, 255), YUV(146, 126, 128))))
    )))
  }

  @Test
  def testDecodePNG(): Unit = {
    VideoDecodeTester.test(VideoTestCase("1mp.png", 1280, 960, 1, FileType.Image, settings.Video.Codec.PNG, Seq(
      FrameInfo(0, 0, Seq(Pixel(50, 50, RGBA(52, 45, 44, 255), YUV(47, 126, 132))))
    )))
  }

  @Test
  def testDecodeWebP(): Unit = {
    VideoDecodeTester.test(VideoTestCase("lake.webp", 1024, 682, 1, FileType.Image, settings.Video.Codec.WebP, Seq(
      FrameInfo(0, 0, Seq(Pixel(50, 50, RGBA(16, 64, 122, 255), YUV(57, 165, 99))))
    )))
  }
}

class DecodeUnsupportedVideoTest {
  @Test
  def testDecodeUnsupportedVideo(): Unit = {
    try {
      for {
        movie <- managed(demux.Movie(TestSrcPath + "/mpeg4.mp4"))
        h264Decoder <- managed(decode.Video(movie.videoTrack))
      } {
        h264Decoder(0).yuv()
        throw new RuntimeException("we should not hit this line")
      }
    } catch {
      // video decoder throws exception
      case e: VireoException =>
        assertEquals(e.exceptionType, ExceptionType.UnsupportedFile)
        assertEquals(e.what, "unsupported")
        assertEquals(e.function.endsWith("decode/video.cpp: Video"), true)
        assertEquals(e.condition, "settings.codec != settings::Video::Codec::H264 && !settings::Video::IsImage(settings.codec)")
        assertEquals(e.reason, "file is currently unsupported")
    }
  }
}
