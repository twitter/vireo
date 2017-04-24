package com.twitter.vireo.periscope.jni

import com.twitter.vireo.common
import com.twitter.vireo.common.Data
import com.twitter.vireo.periscope.ID3Info

class Util {
  @native protected[this] def jniParseID3Info(data: Data[Byte]): ID3Info

  common.Load()
}