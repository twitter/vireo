package com.twitter.vireo.functional

import com.twitter.vireo.domain._
import com.twitter.vireo.settings.{Audio => AudioSettings, Video => VideoSettings, Data => DataSettings, Caption => CaptionSettings}
import scala.collection.GenTraversableOnce

package object types {
  trait Media[Y, S]
    extends Interval[Y] {

    var settings: S = null.asInstanceOf[S]
    override def filter(f: Y => Boolean): functional.Media[Y, S] = {
      new functional.Media[Y, S](super.filter(f), settings)
    }
    def flatMap[Z](f: Y => GenTraversableOnce[Z]): functional.Media[Z, S] = {
      new functional.Media[Z, S](super.flatMap(f), settings)
    }
    def flatMap[Z](itemFunc: Y => GenTraversableOnce[Z], settingsFunc: S => S): functional.Media[Z, S] = {
      new functional.Media[Z, S](super.flatMap(itemFunc), settingsFunc(settings))
    }
    override def flatten[Z](implicit asTraversable: Y ⇒ GenTraversableOnce[Z]): functional.Media[Z, S] = {
      new functional.Media[Z, S](super.flatten(asTraversable), settings)
    }
    def map[Z](f: Y => Z): functional.Media[Z, S] = {
      new functional.Media[Z, S](super.map(f), settings)
    }
    def map[Z](itemFunc: Y => Z, settingsFunc: S => S): functional.Media[Z, S] = {
      new functional.Media[Z, S](super.map(itemFunc), settingsFunc(settings))
    }
    override def slice(from: Int, until: Int): functional.Media[Y, S] = {
      new functional.Media[Y, S](super.slice(from, until), settings)
    }
    def zipWithIndex: functional.Media[(Y, Int), S] = {
      new functional.Media[(Y, Int), S](super.zipWithIndex, settings)
    }
  }

  type Audio[Y] = Media[Y, AudioSettings]
  type Video[Y] = Media[Y, VideoSettings]
  type Data[Y] = Media[Y, DataSettings]
  type Caption[Y] = Media[Y, CaptionSettings]
}

object functional {
  class Media[Y, S](sequence: Seq[Y], s: S)
    extends types.Media[Y, S] {
    settings = s
    b = sequence.length

    def this(settings: S) = this(Seq[Y](), settings)
    def apply(index: Int): Y = sequence(index)
  }
}

class Audio[Y](audio: Seq[Y], settings: AudioSettings)
  extends functional.Media[Y, AudioSettings](audio, settings)

object Audio {
  def apply[Y]() = new Audio[Y](Seq[Y](), AudioSettings.None)
  def apply[Y](settings: AudioSettings) = new Audio[Y](Seq[Y](), settings)
  def apply[Y](audio: Seq[Y], settings: AudioSettings) = new Audio[Y](audio, settings)
}

class Video[Y](video: Seq[Y], settings: VideoSettings)
  extends functional.Media[Y, VideoSettings](video, settings)

object Video {
  def apply[Y]() = new Video[Y](Seq[Y](), VideoSettings.None)
  def apply[Y](settings: VideoSettings) = new Video[Y](Seq[Y](), settings)
  def apply[Y](video: Seq[Y], settings: VideoSettings) = new Video[Y](video, settings)
}

class Caption[Y](caption: Seq[Y], settings: CaptionSettings)
  extends functional.Media[Y, CaptionSettings](caption, settings)

object Caption {
  def apply[Y]() = new Caption[Y](Seq[Y](), CaptionSettings.None)
  def apply[Y](settings: CaptionSettings) = new Caption[Y](Seq[Y](), settings)
  def apply[Y](caption: Seq[Y], settings: CaptionSettings) = new Caption[Y](caption, settings)
}