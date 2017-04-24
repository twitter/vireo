package com.twitter.vireo.encode

import com.twitter.vireo.common.Data
import com.twitter.vireo.frame.RGB
import com.twitter.vireo.functional.types._

class PNG(rgbs: Video[() => RGB])
  extends jni.PNG
  with Video[() => Data[Byte]]
  with AutoCloseable {

  def apply(index: Int) = () => encode(index)
  def close(): Unit = jniClose()

  jniInit(rgbs)
}

object PNG {
  def apply(rgbs: Video[() => RGB]) = {
    new PNG(rgbs)
  }
}
