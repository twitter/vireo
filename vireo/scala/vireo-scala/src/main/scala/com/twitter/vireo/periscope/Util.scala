package com.twitter.vireo.periscope

import com.twitter.vireo.common.Data
import com.twitter.vireo.settings

case class ID3Info(orientation: settings.Video.Orientation, ntpTimestamp: Double)

object Util extends jni.Util {
  def parseID3Info(data: Data[Byte]): ID3Info = jniParseID3Info(data)
}
