package com.twitter.vireo.mux

import com.twitter.vireo.common.{Data, EditBox}
import com.twitter.vireo.encode.Sample
import com.twitter.vireo.FileFormat._
import com.twitter.vireo.functional
import com.twitter.vireo.functional.types._

class MP4(
  audio: Audio[() => Sample],
  video: Video[() => Sample],
  caption: Caption[() => Sample],
  editBoxes: Seq[EditBox],
  fileFormat: FileFormat)
  extends jni.MP4
  with Function0[Data[Byte]]
  with AutoCloseable {

  def apply(): Data[Byte] = encode(fileFormat)
  def apply(fileFormat: FileFormat): Data[Byte] = encode(fileFormat)
  def close(): Unit = jniClose()

  jniInit(audio, video, caption, editBoxes, fileFormat)
}

object MP4 {
  def apply(audio: Audio[() => Sample], video: Video[() => Sample], editBoxes: Seq[EditBox], fileFormat: FileFormat) = {
    new MP4(audio, video, functional.Caption[() => Sample](), editBoxes, fileFormat)
  }
  def apply(audio: Audio[() => Sample], video: Video[() => Sample], fileFormat: FileFormat) = {
    new MP4(audio, video, functional.Caption[() => Sample](), Seq[EditBox](), fileFormat)
  }
  def apply(audio: Audio[() => Sample], video: Video[() => Sample], editBoxes: Seq[EditBox]) = {
    new MP4(audio, video, functional.Caption[() => Sample](), editBoxes, Regular)
  }
  def apply(audio: Audio[() => Sample], video: Video[() => Sample]) = {
    new MP4(audio, video, functional.Caption[() => Sample](), Seq[EditBox](), Regular)
  }
  def apply(video: Video[() => Sample], editBoxes: Seq[EditBox]) = {
    new MP4(functional.Audio[() => Sample](), video, functional.Caption[() => Sample](), editBoxes, Regular)
  }
  def apply(video: Video[() => Sample]) = {
    new MP4(functional.Audio[() => Sample](), video, functional.Caption[() => Sample](), Seq[EditBox](), Regular)
  }
  def apply(audio: Audio[() => Sample], video: Video[() => Sample], caption: Caption[() => Sample], editBoxes: Seq[EditBox] = Seq[EditBox](), fileFormat: FileFormat = Regular) = {
    new MP4(audio, video, caption, editBoxes, fileFormat)
  }
}
