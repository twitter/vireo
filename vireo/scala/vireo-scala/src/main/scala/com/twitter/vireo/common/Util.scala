package com.twitter.vireo.common

import java.nio.file.{Paths, Files}
import java.util.logging.Logger
import java.util.Properties

class Loader(val name: String) {
  private val logger = Logger.getLogger("Loader")
  private var loaded = false
  private var loadedVersion: String = ""
  def apply() {
    if (!loaded) {
      this.synchronized {
        if (!loaded) {
          try {
            val os = System.getProperty("os.name")
            val ext = {
              if (os.startsWith("Mac OS X")) {
                "dylib"
              } else {
                "so"
              }
            }
            val prop = new Properties()
            val classLoader = getClass.getClassLoader
            val propStream = classLoader.getResourceAsStream(s"$name-scala.properties")
            if (propStream == null) {
              throw new RuntimeException(s"$name-scala: No property file")
            }
            try {
              prop.load(propStream)
            } catch {
              case e: Throwable => throw e
            } finally {
              propStream.close()
            }
            loadedVersion = prop.getProperty("version")
            val dstPath = {
              val localPath = Paths.get(new java.io.File(".").getCanonicalPath, s"lib$name-scala.$ext")
              if (Files.exists(localPath)) {
                localPath
              } else {
                val newLibPath = Paths.get(new java.io.File(".").getCanonicalPath,
                                           s"native-libs/$name-scala/$loadedVersion/lib$name-scala.$ext")
                if (!Files.exists(newLibPath)) {
                  if (!Files.exists(newLibPath.getParent)) {
                    Files.createDirectories(newLibPath.getParent)
                  }
                  val libraryStream = classLoader.getResourceAsStream(s"lib$name-scala.$ext")
                  if (libraryStream == null) {
                    throw new RuntimeException(s"lib$name-scala.$ext not found (likely not built)")
                  }
                  try {
                    logger.info(s"$name-scala: Copying lib$name-scala.$ext to ${newLibPath.getParent}/")
                    Files.copy(libraryStream, newLibPath)
                  } catch {
                    case e: Throwable => throw e
                  } finally {
                    libraryStream.close()
                  }
                }
                newLibPath
              }
            }
            logger.info(s"$name-scala: Loading $dstPath")
            System.load(dstPath.toString)
            loaded = true
          } catch {
            case e: Throwable => {
              loadedVersion = "system"
              System.loadLibrary(name)
              loaded = true
            }
          }
        }
      }
    }
  }
  def version(): String = {
    if (loaded) loadedVersion else "not loaded"
  }
}

object Load {
  val loader = new Loader("vireo")
  def apply() = loader()
  def version(): String = loader.version()
}
