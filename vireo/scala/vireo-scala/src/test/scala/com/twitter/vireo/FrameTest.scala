package com.twitter.vireo

import com.twitter.vireo.frame._
import com.twitter.vireo.TestCommon._
import java.io.FileOutputStream
import org.junit.Test
import org.junit.Assert._
import resource._

class FrameConvertTest {
  @Test
  def testConvert(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/8mp.jpg"))
      decoder <- managed(decode.Video(movie.videoTrack))
      encoder <- managed(encode.JPG(decoder flatMap { frm: Frame =>
        Seq[() => frame.YUV](
          () => frm.rgb() yuv(2, 2) scale(400, 2448),
          () => frm.rgb() rgb(3) yuv(2, 2) scale(400, 2448),
          () => frm.rgb() rgb(3) rgb(4) yuv(2, 2) fullRange(false) scale(400, 2448),
          () => frm.rgb() rgb(3) rgb(4) yuv(2, 2) fullRange(false) scale(400, 2448),
          () => frm.rgb() rgb(3) rgb(4) yuv(2, 2) fullRange(false) scale(400, 2448) fullRange(true)
        )
      }, 95, 1))
    } {
      assertEquals(decoder.count, 1)
      assertEquals(encoder.count, 5)

      encoder.zipWithIndex foreach { case (jpg, index) =>
        managed(new java.io.FileOutputStream(TestDstPath + "/convert-" + index + ".jpg")) acquireAndGet { output =>
          output.write(jpg().array())
        }
      }
    }
  }
}

class FrameResizeCropYUVTest {
  @Test
  def testResizeCropYUV(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/8mp.jpg"))
      decoder <- managed(decode.Video(movie.videoTrack))
      encoder <- managed(encode.JPG(decoder flatMap { frm: Frame =>
        Seq[() => frame.YUV](
          () => frm.yuv().scale(400, 2448).crop(66, 0, 400, 400),
          () => frm.yuv().stretch(400, 3264, 400, 2448)
        )
      }, 95, 1))
    } {
      assertEquals(decoder.count, 1)
      assertEquals(encoder.count, 2)

      encoder.zipWithIndex foreach { case (jpg, index) =>
        managed(new java.io.FileOutputStream(TestDstPath + "/resize_crop_yuv-" + index + ".jpg")) acquireAndGet { output =>
          output.write(jpg().array())
        }
      }
    }
  }
}

class FrameRotateYUVTest {
  @Test
  def testRotateYUV(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/1mp.jpg"))
      decoder <- managed(decode.Video(movie.videoTrack))
      encoder <- managed(new encode.JPG(decoder flatMap { frm: Frame =>
        Seq[() => frame.YUV](
          () => frm.yuv().rotate(Rotation.Right),
          () => frm.yuv().rotate(Rotation.Down),
          () => frm.yuv().rotate(Rotation.Left)
        )
      }, 95, 1))
    } {
      assertEquals(decoder.count, 1)
      assertEquals(encoder.count, 3)

      encoder.zipWithIndex foreach { case (jpg, index) =>
        managed(new java.io.FileOutputStream(TestDstPath + "/rotate_yuv-" + index + ".jpg")) acquireAndGet { output =>
          output.write(jpg().array())
        }
      }
    }
  }
}

class FrameResizeCropRGBTest {
  @Test
  def testResizeCropRGB(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/8mp.jpg"))
      decoder <- managed(decode.Video(movie.videoTrack))
      encoder <- managed(encode.JPG(decoder flatMap { frm: Frame =>
        Seq[() => frame.YUV](
          () => frm.yuv().rgb(3).scale(400, 2448).crop(66, 0, 400, 400).yuv(2, 2),
          () => frm.yuv().rgb(4).scale(400, 2448).crop(66, 0, 400, 400).yuv(2, 2),
          () => frm.rgb().scale(400, 2448).crop(66, 0, 400, 400).yuv(2, 2),
          () => frm.rgb().stretch(400, 3264, 400, 2448).yuv(2, 2)
        )
      }, 95, 1))
    } {
      assertEquals(decoder.count, 1)
      assertEquals(encoder.count, 4)

      encoder.zipWithIndex foreach { case (jpg, index) =>
        managed(new FileOutputStream(TestDstPath + "/resize_crop_rgb-" + index + ".jpg")) acquireAndGet { output =>
          output.write(jpg().array())
        }
      }
    }
  }
}

class FrameRotateRGBTest {
  @Test
  def testRotateRGB(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/1mp.jpg"))
      decoder <- managed(decode.Video(movie.videoTrack))
      encoder <- managed(new encode.JPG(decoder flatMap { frm: Frame =>
        Seq[() => frame.YUV](
          () => frm.rgb().rotate(Rotation.Right).yuv(2, 2),
          () => frm.rgb().rotate(Rotation.Down).yuv(2, 2),
          () => frm.rgb().rotate(Rotation.Left).yuv(2, 2)
        )
      }, 95, 1))
    } {
      assertEquals(decoder.count, 1)
      assertEquals(encoder.count, 3)

      encoder.zipWithIndex foreach { case (jpg, index) =>
        managed(new java.io.FileOutputStream(TestDstPath + "/rotate_rgb-" + index + ".jpg")) acquireAndGet { output =>
          output.write(jpg().array())
        }
      }
    }
  }
}

class Mp4FullRangeConvertTest {
  @Test
  def testMp4FullRangeConvert(): Unit = {
    for {
      movie <- managed(demux.Movie(TestSrcPath + "/square_720.mp4"))
      decoder <- managed(decode.Video(movie.videoTrack))
      encoder <- managed(new encode.JPG(decoder.slice(0, 5) flatMap { frm: Frame =>
        Seq[() => frame.YUV](
          () => frm.yuv().fullRange(true)
        )
      }, 95, 1))
    } {
      assertEquals(decoder.count, 5)
      assertEquals(encoder.count, 5)

      encoder.zipWithIndex foreach { case (jpg, index) =>
        managed(new java.io.FileOutputStream(TestDstPath + "/mp4_fullrange_convert-" + index + ".jpg")) acquireAndGet { output =>
          output.write(jpg().array())
        }
      }
    }
  }
}
