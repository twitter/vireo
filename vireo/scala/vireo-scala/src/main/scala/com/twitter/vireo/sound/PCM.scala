package com.twitter.vireo.sound

import com.twitter.vireo.common.Data

case class PCM(size: Short, channels: Byte, samples: Data[Short])
