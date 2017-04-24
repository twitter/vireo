package com.twitter.vireo

import com.twitter.vireo.TestCommon._
import org.junit.Test
import org.junit.Assert._
import resource._

class DemuxMP4Test {
  private[this] def mp4(useReader: Boolean): Unit = {
    val name = TestSrcPath + "/perpertual.mp4"
    var nal: common.Data[Byte] = null
    def MP4(name: String, useReader: Boolean): demux.Movie = {
      useReader match {
        case false => demux.Movie(name)
        case true => demux.Movie(common.DataReader(name))
      }
    }
    for { movie <- managed(MP4(name, useReader)) } {
      assertEquals(movie.fileType(), FileType.MP4)

      val videoTrack = movie.videoTrack

      assertEquals(videoTrack.duration(), 3830)
      assertEquals(videoTrack.settings.width, 450)
      assertEquals(videoTrack.settings.height, 252)
      assertEquals(videoTrack.settings.codec, settings.Video.Codec.H264)

      assertEquals(videoTrack.count, 383)
      assertEquals(videoTrack(10).dts, 100)
      assertEquals(videoTrack(10).dts, 100)
      assertEquals(videoTrack(0).keyframe, true)
      assertEquals(videoTrack(1).keyframe, false)
      assertEquals(videoTrack(10).byteRange.isDefined, true)
      assertEquals(videoTrack(10).byteRange.get.pos, 25875)
      assertEquals(videoTrack(10).byteRange.get.size, 423)
      assertEquals(videoTrack(10).nal().length, 423)
      assertEquals(videoTrack(10).nal()(50), -43)
      assertEquals(videoTrack(10).nal()(422), -9)
      try {
        videoTrack(383).pts
      } catch {
        case e: Throwable =>
          assertTrue(e.getMessage.contains("out of range"))
      }
      nal = videoTrack(10).nal()
    }
    try {
      assertEquals(nal.length, 423)
    } catch {
      case e: IllegalAccessException => assertTrue(e.getMessage.contains("reclaimed by managed owner"))
    }
  }

  private[this] def mp4(): Unit = mp4(false)

  @Test
  def testDemuxMP4(): Unit = {
    mp4()
  }

  @Test
  def testDemuxMP4_Reader(): Unit = {
    mp4(true)
  }
}

class DemuxMP2TSTest {
  @Test
  def testDemuxMP2TS(): Unit = {
    var nal: common.Data[Byte] = null
    for { movie <- managed(demux.Movie(TestSrcPath + "/periscope.ts")) } {
      assertEquals(movie.fileType(), FileType.MP2TS)

      val videoTrack = movie.videoTrack
      val audioTrack = movie.audioTrack

      assertEquals(videoTrack.duration(), 269730)
      assertEquals(videoTrack.settings.width, 320)
      assertEquals(videoTrack.settings.height, 568)
      assertEquals(videoTrack.settings.codec, settings.Video.Codec.H264)

      assertEquals(videoTrack.count, 72)

      assertEquals(videoTrack(0).keyframe, true)
      assertEquals(videoTrack(1).keyframe, false)
      assertEquals(videoTrack(37).keyframe, true)
      assertEquals(videoTrack(10).dts, 289314720)
      assertEquals(videoTrack(10).pts, 289311030)
      assertEquals(videoTrack(10).byteRange.isDefined, false)
      assertEquals(videoTrack(10).nal().length, 105)
      assertEquals(videoTrack(10).nal()(44), -87)
      assertEquals(videoTrack(10).nal()(104), 126)

      assertEquals(audioTrack.duration(), 269584)
      assertEquals(audioTrack.settings.channels, 1)
      assertEquals(audioTrack.settings.timescale, 90000)
      assertEquals(audioTrack.settings.sampleRate, 44100)
      assertEquals(audioTrack.settings.bitrate, 0)
      assertEquals(audioTrack.settings.codec, settings.Audio.Codec.AAC_LC)

      assertEquals(audioTrack.count, 129)

      assertEquals(audioTrack(10).dts, 289298029)
      assertEquals(audioTrack(10).pts, 289298029)
      assertEquals(audioTrack(10).byteRange.isDefined, false)
      assertEquals(audioTrack(10).nal().length, 114)
      assertEquals(audioTrack(10).nal()(50), 49)
      assertEquals(audioTrack(10).nal()(113), 7)
      try {
        videoTrack(100).pts
      } catch {
        case e: Throwable =>
          assertTrue(e.getMessage.contains("out of range"))
      }
      nal = videoTrack(10).nal()
    }
    try {
      assertEquals(nal.length, 105)
    } catch {
      case e: IllegalAccessException => assertTrue(e.getMessage.contains("reclaimed by managed owner"))
    }
  }

  @Test
  def testDemuxMP2TS_NonExistingFile(): Unit = {
    try {
      managed(demux.Movie(TestSrcPath + "/NonExisting.ts")) acquireAndGet(movie => Unit)
    } catch {
      case e: java.io.FileNotFoundException => assertTrue(e.getMessage.contains("No such file or directory"))
    }
  }
}

class DemuxInvalidVideoTest {
  @Test
  def testDemuxInvalidVideoBuffer(): Unit = {
    try {
      for {movie <- managed(demux.Movie(new Array[Byte](1024)))} {
        movie.videoTrack(0).nal()
        throw new RuntimeException("we should not hit this line")
      }
    } catch {
      // movie throws exception
      case e: VireoException =>
        assertEquals(e.exceptionType, ExceptionType.InvalidFile)
        assertEquals(e.what, "invalid")
        assertEquals(e.function.endsWith("internal/demux/mp4.cpp: MP4"), true)
        assertEquals(e.condition, "lsmash_read_file(file, _this->file.get()) < 0")
        assertEquals(e.reason, "file is invalid")
    }
  }
}

class DemuxNonExistingVideoTest {
  @Test
  def testDemuxNonExistingFile(): Unit = {
    try {
      managed(demux.Movie(TestSrcPath + "/NonExisting.mp4")) acquireAndGet(movie => Unit)
    } catch {
      case e: java.io.FileNotFoundException => assertTrue(e.getMessage.contains("No such file or directory"))
    }
  }
}
