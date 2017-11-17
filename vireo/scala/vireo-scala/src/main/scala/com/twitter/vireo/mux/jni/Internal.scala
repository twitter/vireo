package com.twitter.vireo.mux.jni

import com.twitter.vireo.common
import com.twitter.vireo.common.{Data, EditBox}
import com.twitter.vireo.encode.Sample
import com.twitter.vireo.FileFormat._
import com.twitter.vireo.functional.types._

class MP2TS {
  protected[this] val jni: Long = 0

  protected[this] def encode(): Data[Byte] = encode(jni)

  @native protected[this] def jniInit(audio: Audio[() => Sample], video: Video[() => Sample], caption: Caption[() => Sample]): Unit
  @native protected[this] def jniClose(): Unit
  @native protected[this] def encode(jni: Long): Data[Byte]

  common.Load()
}

class MP4 {
  protected[this] val jni: Long = 0

  protected[this] def encode(fileFormat: FileFormat): Data[Byte] = encode(jni, fileFormat)

  @native protected[this] def jniInit(audio: Audio[() => Sample], video: Video[() => Sample], caption: Caption[() => Sample], editBoxes: Seq[EditBox], fileFormat: FileFormat): Unit
  @native protected[this] def jniClose(): Unit
  @native protected[this] def encode(jni: Long, fileFormat: FileFormat): Data[Byte]

  common.Load()
}

class WebM {
  protected[this] val jni: Long = 0

  protected[this] def encode(): Data[Byte] = encode(jni)

  @native protected[this] def jniInit(audio: Audio[() => Sample], video: Video[() => Sample]): Unit
  @native protected[this] def jniClose(): Unit
  @native protected[this] def encode(jni: Long): Data[Byte]

  common.Load()
}
