package com.twitter.vireo.decode

import com.twitter.vireo.frame
import com.twitter.vireo.functional

final class Video(samples: functional.types.Video[Sample], threadCount: Int)
  extends jni.Video
  with functional.types.Video[frame.Frame]
  with AutoCloseable {

  def apply(index: Int): frame.Frame = decode(index)
  def close(): Unit = jniClose()

  jniInit(samples, threadCount)
}

object Video {
  def apply(video: functional.types.Video[Sample], threadCount: Int = 0) = {
    new Video(video, threadCount)
  }
}