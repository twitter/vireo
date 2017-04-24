package com.twitter.vireo.mux

import com.twitter.vireo.common.Data
import com.twitter.vireo.encode.Sample
import com.twitter.vireo.functional.types._
import com.twitter.vireo.functional

class MP2TS(
  audio: Audio[() => Sample],
  video: Video[() => Sample],
  caption: Caption[() => Sample])
  extends jni.MP2TS
  with Function0[Data[Byte]]
  with AutoCloseable {

  def apply(): Data[Byte] = encode()
  def close(): Unit = jniClose()

  jniInit(audio, video, caption)
}

object MP2TS {
  def apply(audio: Audio[() => Sample], video: Video[() => Sample]) = {
    new MP2TS(audio, video, functional.Caption[() => Sample]())
  }
  def apply(video: Video[() => Sample]) = {
    new MP2TS(functional.Audio[() => Sample](), video, functional.Caption[() => Sample]())
  }
  def apply(audio: Audio[() => Sample], video: Video[() => Sample], caption: Caption[() => Sample]) = {
    new MP2TS(audio, video, caption)
  }
}
