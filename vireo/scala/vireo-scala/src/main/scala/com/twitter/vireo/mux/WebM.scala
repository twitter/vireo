package com.twitter.vireo.mux

import com.twitter.vireo.common.Data
import com.twitter.vireo.encode.Sample
import com.twitter.vireo.functional.types._
import com.twitter.vireo.functional

class WebM(
  audio: Audio[() => Sample],
  video: Video[() => Sample])
  extends jni.WebM
  with AutoCloseable {

  def apply(): Data[Byte] = encode()
  def close(): Unit = jniClose()

  jniInit(audio, video)
}

object WebM {
  def apply(audio: Audio[() => Sample], video: Video[() => Sample]) = {
    new WebM(audio, video)
  }
  def apply(video: Video[() => Sample]) = {
    new WebM(functional.Audio[() => Sample](), video)
  }
}