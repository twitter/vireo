package com.twitter.vireo.header

class SPS_PPS(val sps: Array[Byte], val pps: Array[Byte], val naluLengthSize: Byte) {
  override def toString: String = {
    "(sps = " + sps.length + ", pps = " + pps.length + ", nalu length size = " + naluLengthSize + ")";
  }
}
