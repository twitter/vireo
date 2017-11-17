package com.twitter.vireo.common

import java.nio.ByteBuffer

trait Reader {
  def size: Int
  def read(offset: Int, size: Int): Data[Byte]
}

class DataReader(data: Data[Byte]) extends Reader {
  def size: Int = data.length
  def read(offset: Int, size: Int): Data[Byte] = {
    val byteBuffer = ByteBuffer.wrap(data.array())
    byteBuffer.position(offset)
    byteBuffer.limit(offset + size)
    new ByteData(byteBuffer)
  }
}

object DataReader {
  def apply(name: String) = {
    new DataReader(Data(name))
  }
  def apply(buffer: Array[Byte]) = {
    new DataReader(Data(buffer))
  }
  def apply(data: Data[Byte]) = {
    new DataReader(data)
  }
}