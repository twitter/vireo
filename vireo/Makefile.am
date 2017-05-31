AUTOMAKE_OPTIONS = foreign
SUBDIRS = ../imagecore

libvireo_la_CPPFLAGS = -I./ -I../ -I../thirdparty/l-smash_extra
bin_PROGRAMS = frames remux stitch thumbnails transcode trim unchunk validate viddiff

frames_CPPFLAGS = -I../
frames_SOURCES = tools/remux/main.cpp  tests/test_common.cpp
frames_LDADD = ./libvireo.la ../imagecore/libimagecore.la

remux_CPPFLAGS = -I../
remux_SOURCES = tools/remux/main.cpp  tests/test_common.cpp
remux_LDADD = ./libvireo.la ../imagecore/libimagecore.la

stitch_CPPFLAGS = -I../
stitch_SOURCES = tools/stitch/main.cpp tests/test_common.cpp
stitch_LDADD = ./libvireo.la ../imagecore/libimagecore.la

thumbnails_CPPFLAGS = -I../
thumbnails_SOURCES = tools/thumbnails/main.cpp tests/test_common.cpp
thumbnails_LDADD = ./libvireo.la ../imagecore/libimagecore.la

transcode_CPPFLAGS = -I../
transcode_SOURCES = tools/transcode/main.cpp tests/test_common.cpp
transcode_LDADD = ./libvireo.la ../imagecore/libimagecore.la

trim_CPPFLAGS = -I../
trim_SOURCES = tools/trim/main.cpp tests/test_common.cpp
trim_LDADD = ./libvireo.la ../imagecore/libimagecore.la

unchunk_CPPFLAGS = -I../
unchunk_SOURCES = tools/unchunk/main.cpp tests/test_common.cpp
unchunk_LDADD = ./libvireo.la ../imagecore/libimagecore.la

validate_CPPFLAGS = -I../
validate_SOURCES = tools/validate/main.cpp tests/test_common.cpp
validate_LDADD = ./libvireo.la ../imagecore/libimagecore.la

viddiff_CPPFLAGS = -I../
viddiff_SOURCES = tools/viddiff/main.cpp tests/test_common.cpp
viddiff_LDADD = ./libvireo.la ../imagecore/libimagecore.la

lib_LTLIBRARIES = libvireo.la
libvireo_la_SOURCES =
libvireo_la_SOURCES += common/bitreader.cpp common/data.cpp common/editbox.cpp common/path.cpp common/reader.cpp
libvireo_la_SOURCES += decode/audio.cpp decode/video.cpp
libvireo_la_SOURCES += demux/movie.cpp
libvireo_la_SOURCES += encode/aac.cpp encode/h264.cpp encode/jpg.cpp encode/png.cpp encode/vorbis.cpp encode/vp8.cpp
libvireo_la_SOURCES += error/error.cpp
libvireo_la_SOURCES += frame/frame.cpp frame/plane.cpp frame/rgb.cpp frame/util.cpp frame/yuv.cpp
libvireo_la_SOURCES += header/header.cpp
libvireo_la_SOURCES += internal/decode/aac.cpp internal/decode/annexb.cpp internal/decode/avcc.cpp internal/decode/h264.cpp internal/decode/h264_bytestream.cpp internal/decode/image.cpp internal/decode/pcm.cpp
libvireo_la_SOURCES += internal/demux/image.cpp internal/demux/mp2ts.cpp internal/demux/mp4.cpp internal/demux/webm.cpp
libvireo_la_SOURCES += mux/mp2ts.cpp mux/mp4.cpp mux/webm.cpp
libvireo_la_SOURCES += util/caption.cpp util/ftyp.cpp util/timer.cpp
libvireo_la_SOURCES += transform/stitch.cpp transform/trim.cpp
libvireo_la_SOURCES += settings/settings.cpp settings/settings-vorbis.cpp
libvireo_la_SOURCES += sound/pcm.cpp sound/sound.cpp
libvireo_la_LDFLAGS = -lswscale -lavformat -lavcodec -lavutil -llsmash -lz -lx264 -lvpx -lwebm -lvorbis -lvorbisenc -lfdk-aac -logg -lpthread
libvireo_la_LIBADD = ../imagecore/libimagecore.la

if BUILD_SCALA
if JAVA_HOME_SET
libvireo_la_SOURCES += scala/jni/common/jni.cpp
libvireo_la_SOURCES += scala/jni/vireo/decode.cpp scala/jni/vireo/encode.cpp scala/jni/vireo/demux.cpp scala/jni/vireo/frame.cpp scala/jni/vireo/mux.cpp scala/jni/vireo/sound.cpp scala/jni/vireo/transform.cpp scala/jni/vireo/util.cpp
libvireo_la_CPPFLAGS += $(JNI_CPPFLAGS)
endif
endif

nobase_pkginclude_HEADERS = base_cpp.h base_h.h version.h types.h constants.h
nobase_pkginclude_HEADERS += common/bitreader.h common/data.h common/editbox.h common/math.h common/path.h common/reader.h common/ref.h common/security.h
nobase_pkginclude_HEADERS += decode/audio.h decode/types.h decode/video.h
nobase_pkginclude_HEADERS += demux/movie.h
nobase_pkginclude_HEADERS += domain/util.h
nobase_pkginclude_HEADERS += encode/aac.h encode/h264.h encode/jpg.h encode/png.h encode/types.h encode/util.h encode/vorbis.h encode/vp8.h
nobase_pkginclude_HEADERS += error/error.h
nobase_pkginclude_HEADERS += frame/frame.h frame/plane.h frame/rgb.h frame/util.h frame/yuv.h
nobase_pkginclude_HEADERS += header/header.h
nobase_pkginclude_HEADERS += mux/mp2ts.h mux/mp4.h mux/webm.h
nobase_pkginclude_HEADERS += settings/settings.h
nobase_pkginclude_HEADERS += transform/stitch.h transform/trim.h
nobase_pkginclude_HEADERS += util/caption.h util/ftyp.h util/timer.h util/util.h
