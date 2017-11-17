package com.twitter.vireo.common

import java.io.RandomAccessFile
import java.nio.{ByteBuffer, MappedByteBuffer, ShortBuffer}
import java.nio.channels.FileChannel
import com.twitter.vireo.domain.Interval

trait Native[T, S] {
  def apply(copy: Boolean): S
  def apply(): S = apply(true)
  def cleaner(data: Data[T]): Unit
}

trait Data[T]
  extends Interval[T] {
  def byteBuffer(): ByteBuffer
  def shortBuffer(): ShortBuffer
  def array(): Array[T]
}

trait DataImpl[T]
  extends Data[T] {
  protected[this] var empty: Boolean = false
  protected[this] val initialized: Boolean = true
  protected[this] def checkInitialized() = {
    if (!initialized) {
      throw new IllegalAccessException("Buffer reclaimed by managed owner")
    }
  }
  def apply(index: Int): T = {
    if (empty) {
      if (index < a || index >= b) throw new ArrayIndexOutOfBoundsException("%d".format(index))
      null.asInstanceOf[T]
    } else {
      array()(index)
    }
  }
}

final class ByteData(input: Either[ByteBuffer, Int])
  extends DataImpl[Byte] {
  private[this] val buffer: ByteBuffer = input match {
    case Left(inputBuffer) => {
      a = inputBuffer.position
      b = inputBuffer.limit
      inputBuffer.duplicate
    }
    case Right(size) => {
      a = 0
      b = size
      empty = true
      ByteBuffer.allocate(0)
    }
  }

  def this(byteBuffer: ByteBuffer) = this(Left(byteBuffer))
  def this(array: Array[Byte]) = this(ByteBuffer.wrap(array))
  def this(size: Int) = this(Right(size))
  def this() = this(ByteBuffer.wrap(Array[Byte]()))

  private[this] def safeBuffer(): ByteBuffer = {
    checkInitialized()
    buffer
  }
  def byteBuffer(): ByteBuffer = safeBuffer().duplicate
  def shortBuffer(): ShortBuffer = byteBuffer().asShortBuffer

  override def slice(from: Int, until: Int): Data[Byte] = {
    val buf = byteBuffer()
    buf.position(from)
    buf.limit(until)
    Data(buf)
  }

  lazy val array: Array[Byte] = {
    val buf = safeBuffer()
    if (buf.hasArray) {
      if (buf.remaining == buf.capacity) buf.array else buf.array.slice(buf.position, buf.limit)
    } else {
      val arr = new Array[Byte](buf.remaining)
      buf.duplicate.get(arr)
      arr
    }
  }
}

final class ShortData(input: Either[ShortBuffer, Int])
  extends DataImpl[Short] {
  private[this] val buffer: ShortBuffer = input match {
    case Left(inputBuffer) => {
      a = inputBuffer.position
      b = inputBuffer.limit
      inputBuffer.duplicate
    }
    case Right(size) => {
      a = 0
      b = size
      empty = true
      ShortBuffer.allocate(0)
    }
  }

  def this(shortBuffer: ShortBuffer) = this(Left(shortBuffer))
  def this(byteBuffer: ByteBuffer) = this(byteBuffer.order(java.nio.ByteOrder.nativeOrder()).asShortBuffer())
  def this(array: Array[Short]) = this(ShortBuffer.wrap(array))
  def this(size: Int) = this(Right(size))
  def this() = this(ShortBuffer.wrap(Array[Short]()))

  private[this] def safeBuffer(): ShortBuffer = {
    checkInitialized()
    buffer
  }
  def byteBuffer(): ByteBuffer = {
    throw new UnsupportedOperationException("Can't convert Data[Short] to byteBuffer; call shortBuffer() instead")
  }
  def shortBuffer(): ShortBuffer = safeBuffer().duplicate

  override def slice(from: Int, until: Int): Data[Short] = {
    val buf = shortBuffer()
    buf.position(from)
    buf.limit(until)
    Data(buf)
  }

  lazy val array: Array[Short] = {
    val buf = safeBuffer()
    if (buf.hasArray) {
      if (buf.remaining == buf.capacity) buf.array else buf.array.slice(buf.position, buf.limit)
    } else {
      val arr = new Array[Short](buf.remaining)
      buf.duplicate.get(arr)
      arr
    }
  }
}

object Data {
  def apply() = {
    new ByteData()
  }
  def apply(size: Int) = {
    new ByteData(size)
  }
  def apply(byteBuffer: ByteBuffer) = {
    new ByteData(byteBuffer)
  }
  def apply(bytes: Array[Byte]) = {
    new ByteData(ByteBuffer.wrap(bytes))
  }
  def apply(shortBuffer: ShortBuffer) = {
    new ShortData(shortBuffer)
  }
  def apply(bytes: Array[Short]) = {
    new ShortData(ShortBuffer.wrap(bytes))
  }
  def apply(name: String) = {
    val file: RandomAccessFile = new RandomAccessFile(name, "r")
    val channel: FileChannel = file.getChannel
    val buffer: MappedByteBuffer = channel.map(FileChannel.MapMode.READ_ONLY, 0, channel.size())
    channel.close()
    file.close()
    new ByteData(buffer)
  }
}
