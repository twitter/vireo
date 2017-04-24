package com.twitter.vireo

import com.twitter.vireo.common.EditBox
import com.twitter.vireo.TestCommon._
import org.junit.Assert._
import org.junit.Test
import resource._

case class TransformTestData(
  numVideoSamples: Int,
  videoDuration: Long,
  numAudioSamples: Int,
  audioDuration: Long,
  editBoxes: Seq[EditBox])

class TransformStitchTest {
  private[this] def test(
    input: String,
    expected: TransformTestData,
    disableAudio: Boolean = false,
    disableEditBoxes: Boolean = false
  ) = {
    val source = TestSrcPath + "/" + input
    for {
      movie <- managed(demux.Movie(source))
    } {
      val audioTracks = if (!disableAudio) {
        Seq(movie.audioTrack, movie.audioTrack)
      } else {
        Seq(functional.Audio[decode.Sample](), functional.Audio[decode.Sample]())
      }
      val videoTracks = Seq(movie.videoTrack, movie.videoTrack)
      val editBoxesPerTrack = if (!disableAudio) {
        Seq(movie.videoTrack.editBoxes() ++ movie.audioTrack.editBoxes(),
            movie.videoTrack.editBoxes() ++ movie.audioTrack.editBoxes())
      } else {
        Seq(movie.videoTrack.editBoxes(), movie.videoTrack.editBoxes())
      }
      for (stitched <- managed(transform.Stitch(audioTracks, videoTracks, editBoxesPerTrack))) {
        assertEquals(stitched.videoTrack.length, expected.numVideoSamples)
        assertEquals(stitched.videoTrack.duration(), expected.videoDuration)
        assertEquals(stitched.audioTrack.length, expected.numAudioSamples)
        assertEquals(stitched.audioTrack.duration(), expected.audioDuration)

        val editBoxes = stitched.audioTrack.editBoxes ++ stitched.videoTrack.editBoxes
        expected.editBoxes.zipWithIndex foreach { case (editBox, index) =>
          assertEquals(editBox.startPts, editBoxes(index).startPts)
          assertEquals(editBox.durationPts, editBoxes(index).durationPts)
          assertEquals(editBox.sampleType, editBoxes(index).sampleType)
        }
      }
    }
  }

  @Test
  def testStitch(): Unit = {
    test("rosebud.mp4", TransformTestData(996, 1993992, 1410, 1443840, Seq(
      EditBox(1617, 717875, SampleType.Audio),
      EditBox(723537, 717875, SampleType.Audio),
      EditBox(18729, 976700, SampleType.Video),
      EditBox(1015725, 976700, SampleType.Video)
    )))
    test("rosebud.mp4", TransformTestData(996, 1993992, 0, 0, Seq(
      EditBox(18729, 976700, SampleType.Video),
      EditBox(1015725, 976700, SampleType.Video)
    )), disableAudio = true)
    test("rosebud.mp4", TransformTestData(996, 1993992, 1410, 1443840, Seq[EditBox]()),
      disableAudio = false,
      disableEditBoxes = true)
  }
}

class TransformTrimTest {
  @Test
  def testTrim(): Unit = {
    // input parameters
    val source = TestSrcPath + "/michael.mp4"
    val startMs = 300L
    val durationMs = 1500L

    // expected value
    val expected = TransformTestData(51, 51000L, 66, 67584L, Seq(
      EditBox(5861, 44945, SampleType.Video),
      EditBox(833, 66150, SampleType.Audio)
    ))

    for {
      movie <- managed(demux.Movie(source))
      trimmedVideo <- managed(transform.Trim(movie.videoTrack, movie.videoTrack.editBoxes(), startMs, durationMs))
      trimmedAudio <- managed(transform.Trim(movie.audioTrack, movie.audioTrack.editBoxes(), startMs, durationMs))
    } {
      assertEquals(trimmedVideo.track.length, expected.numVideoSamples)
      assertEquals(trimmedVideo.track.duration(), expected.videoDuration)
      assertEquals(trimmedAudio.track.length, expected.numAudioSamples)
      assertEquals(trimmedAudio.track.duration(), expected.audioDuration)

      val editBoxes = trimmedVideo.track.editBoxes ++ trimmedAudio.track.editBoxes
      expected.editBoxes.zipWithIndex foreach { case (editBox, index) =>
        assertEquals(editBox.startPts, editBoxes(index).startPts)
        assertEquals(editBox.durationPts, editBoxes(index).durationPts)
        assertEquals(editBox.sampleType, editBoxes(index).sampleType)
      }
    }
  }
}
