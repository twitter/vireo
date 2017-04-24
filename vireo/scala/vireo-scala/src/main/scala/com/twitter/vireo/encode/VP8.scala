package com.twitter.vireo.encode

import com.twitter.vireo.frame.Frame
import com.twitter.vireo.functional.types._

class VP8(
  frames: Video[Frame],
  quantizer: Int,
  optimization: Int,
  fps: Float,
  bitrate: Int)
  extends jni.VP8
  with Video[() => Sample]
  with AutoCloseable {

  def apply(index: Int) = encode(index)
  def close(): Unit = jniClose()

  jniInit(frames, quantizer, optimization, fps, bitrate)
}

object VP8 {
  def apply(frames: Video[Frame], quantizer: Int, optimization: Int, fps: Float) = {
    new VP8(frames, quantizer, optimization, fps, 0)
  }
  def apply(frames: Video[Frame], quantizer: Int, optimization: Int, fps: Float, bitrate: Int) = {
    new VP8(frames, quantizer, optimization, fps, bitrate)
  }
}
