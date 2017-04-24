package com.twitter.vireo.decode

import com.twitter.vireo.functional
import com.twitter.vireo.sound.Sound

final class Audio(samples: functional.types.Audio[Sample])
  extends jni.Audio
  with functional.types.Audio[Sound]
  with AutoCloseable {

  def apply(index: Int): Sound = decode(index)
  def close(): Unit = jniClose()

  jniInit(samples)
}

object Audio {
  def apply(samples: functional.types.Audio[Sample]) = {
    new Audio(samples)
  }
}