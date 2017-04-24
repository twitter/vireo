package com.twitter.vireo

import org.junit.Test
import org.junit.Assert._

case class ExpectedValues(what: String, function: String, line: Int, condition: String, reason: String)

object ExceptionTestHelper {
  def test(message: String, expectedValues: ExpectedValues, expectedExceptionType: ExceptionType) = {
    try {
      throw VireoException(message)
    } catch {
      case e: VireoException =>
        assertEquals(expectedValues.what, e.what)
        assertEquals(expectedValues.function, e.function)
        assertEquals(expectedValues.line, e.line)
        assertEquals(expectedValues.condition, e.condition)
        assertEquals(expectedValues.reason, e.reason)
        assertEquals(expectedExceptionType, e.exceptionType)
    }
  }
}

class ExceptionsTest {
 @Test
  def testInternalErrorException() = {
   val tests = Seq[(String, ExpectedValues)](
     ("[encode/h264.cpp: H264 (43)] invalid arguments: \"crf < kH264MinCRF || crf > kH264MaxCRF\" condition failed; reason: unexpected error, please report back",
       ExpectedValues("invalid arguments", "encode/h264.cpp: H264", 43, "crf < kH264MinCRF || crf > kH264MaxCRF", "unexpected error, please report back")),
     ("[decode/h264.cpp: operator() (185)] internal inconsistency: \"!(got_picture)\" condition failed; reason: unexpected error, please report back",
       ExpectedValues("internal inconsistency", "decode/h264.cpp: operator()", 185, "!(got_picture)", "unexpected error, please report back")),
     ("[imagecore/formats/reader.cpp: error code 12 (44)] imagecore: \"0\" condition failed; reason: unexpected error, please report back",
       ExpectedValues("imagecore", "imagecore/formats/reader.cpp: error code 12", 44, "0", "unexpected error, please report back")),
     ("[decode/mp2ts.cpp: MP2TS (416)] out of memory: \"format_context->pb == NULL\" condition failed; reason: unexpected error, please report back",
       ExpectedValues("out of memory", "decode/mp2ts.cpp: MP2TS", 416, "format_context->pb == NULL", "unexpected error, please report back")),
     ("[decode/aac.cpp: operator() (87)] out of range: \"index >= count()\" condition failed; reason: unexpected error, please report back",
       ExpectedValues("out of range", "decode/aac.cpp: operator()", 87, "index >= count()", "unexpected error, please report back")),
     ("[common/math.h: safe_umul (39)] overflow: \"y && x > numeric_limits<T>::max() / y\" condition failed; reason: unexpected error, please report back",
       ExpectedValues("overflow", "common/math.h: safe_umul", 39, "y && x > numeric_limits<T>::max() / y", "unexpected error, please report back")),
     ("[decode/annexb.cpp: ANNEXB (97)] uninitialized: \"!_this->initialized\" condition failed; reason: unexpected error, please report back",
       ExpectedValues("uninitialized", "decode/annexb.cpp: ANNEXB", 97, "!_this->initialized", "unexpected error, please report back"))
   )
   tests foreach { case (message, expected) =>
     ExceptionTestHelper.test(message, expected, ExceptionType.InternalError)
   }
 }

  @Test
  def testInvalidFileException() = {
    val tests = Seq[(String, ExpectedValues)](
      ("[decode/mp4.cpp: parse_video_codec_info (200)] invalid: \"!sps_size\" condition failed; reason: file is invalid",
        ExpectedValues("invalid", "decode/mp4.cpp: parse_video_codec_info", 200, "!sps_size", "file is invalid"))
    )
    tests foreach { case (message, expected) =>
      ExceptionTestHelper.test(message, expected, ExceptionType.InvalidFile)
    }
  }

  @Test
  def testReaderErrorException() = {
    val tests = Seq[(String, ExpectedValues)](
      ("[scala/jni/vireo/com_twitter_vireo_decode.cpp: operator() (660)] reader error: \"!byte_data_obj\" condition failed; reason: unexpected error, please report back",
        ExpectedValues("reader error", "scala/jni/vireo/com_twitter_vireo_decode.cpp: operator()", 660, "!byte_data_obj", "unexpected error, please report back"))
    )
    tests foreach { case (message, expected) =>
      ExceptionTestHelper.test(message, expected, ExceptionType.ReaderError)
    }
  }

  @Test
  def testUnsupportedFileException() = {
    val tests = Seq[(String, ExpectedValues)](
      ("[encode/mp4.cpp: mux (232)] unsafe: \"audio.count() >= security::kMaxSampleCount\" condition failed; reason: file is currently unsupported",
        ExpectedValues("unsafe", "encode/mp4.cpp: mux", 232, "audio.count() >= security::kMaxSampleCount", "file is currently unsupported")),
      ("[main.cpp: main (109)] unsupported: \"video_settings.codec != settings::Video::Codec::H264\" condition failed; reason: Video codec ProRes not supported",
        ExpectedValues("unsupported", "main.cpp: main", 109, "video_settings.codec != settings::Video::Codec::H264", "Video codec ProRes not supported")),
      ("com.twitter.vireo.VireoException: [main.cpp: main (109)] unsupported: \"video_settings.codec != settings::Video::Codec::H264\" condition failed; reason: Video codec ProRes not supported",
        ExpectedValues("unsupported", "main.cpp: main", 109, "video_settings.codec != settings::Video::Codec::H264", "Video codec ProRes not supported"))
    )
    tests foreach { case (message, expected) =>
      ExceptionTestHelper.test(message, expected, ExceptionType.UnsupportedFile)
    }
  }

  @Test
  def testUnknownErrorException() = {
    val expectedValues = ExpectedValues("unknown", "", -1, "", "")
    val tests = Seq[(String, ExpectedValues)](
      ("unknown exception in native code", expectedValues),
      ("[ (1)] unsupported: \"some condition != true\" condition failed; reason: no file", expectedValues),
      ("[file ()] unsupported: \"some condition != true\" condition failed; reason: no line number", expectedValues),
      ("[file (string)] unsupported: \"some condition != true\" condition failed; reason: line number is string", expectedValues),
      ("[file (1)] : \"some condition != true\" condition failed; reason: what is missing", expectedValues),
      ("[file (1)] unsupported: \"\" condition failed; reason: no condition", expectedValues),
      ("[file (1)] unsupported: \"some condition != true\" not matching expected pattern", expectedValues),
      ("[file (1)] unsupported: \"some condition != true\" condition failed; reason: ", expectedValues),  // reason not available
      ("file (1)] unsupported: \"some condition != true\" condition failed; reason: pattern single [ char off", expectedValues),
      ("[file (1)] blah: \"some condition != true\" condition failed; reason: what is unknown",
        ExpectedValues("blah", "file", 1, "some condition != true", "what is unknown"))
    )
    tests foreach { case (message, expected) =>
      ExceptionTestHelper.test(message, expected, ExceptionType.UnknownError)
    }
  }
}
