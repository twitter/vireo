package com.twitter.vireo.encode

import com.twitter.vireo.common.Data
import com.twitter.vireo.frame.YUV
import com.twitter.vireo.functional.types._

class JPG(
  yuvs: Video[() => YUV],
  quality: Int,
  optimization: Int)
  extends jni.JPG
  with Video[() => Data[Byte]]
  with AutoCloseable {

  def apply(index: Int) = () => encode(index)
  def close(): Unit = jniClose()

  jniInit(yuvs, quality, optimization)
}

object JPG {
  def apply(yuvs: Video[() => YUV], quality: Int, optimization: Int) = {
    new JPG(yuvs, quality, optimization)
  }
}
