AUTOMAKE_OPTIONS = foreign
libimagecore_la_CPPFLAGS = -I./ -I../ -I../thirdparty/ -I../thirdparty/libjpeg -DIMAGECORE_WITH_BMP=1 -DIMAGECORE_WITH_GIF=1
lib_LTLIBRARIES = libimagecore.la
libimagecore_la_SOURCES = imagecore.cpp formats/reader.cpp formats/writer.cpp formats/exif/exifreader.cpp formats/exif/exifcommon.cpp formats/exif/exifwriter.cpp formats/internal/raw.cpp formats/internal/register.cpp \
		image/image.cpp image/kernel.cpp image/internal/filters.cpp image/internal/filters_intrinsics.cpp image/internal/conversions.cpp image/internal/platform_support.cpp image/internal/sse.cpp image/resizecrop.cpp image/tiledresize.cpp image/colorspace.cpp image/rgba.cpp image/yuv.cpp image/yuv_semiplanar.cpp image/grayscale.cpp image/colorpalette.cpp formats/internal/bmp.cpp formats/internal/gif.cpp

libimagecore_la_SOURCES += ../thirdparty/giflib/dgif_lib.c ../thirdparty/giflib/gif_err.c ../thirdparty/giflib/gif_hash.c ../thirdparty/giflib/gifalloc.c

if HAVE_LIBJPEG
libimagecore_la_SOURCES += formats/internal/jpeg.cpp
libimagecore_la_SOURCES += ../thirdparty/libjpeg/iccjpeg.c ../thirdparty/libjpeg/transupp.c
libimagecore_la_CPPFLAGS += -DIMAGECORE_WITH_JPEG=1
endif
if HAVE_LIBPNG
libimagecore_la_SOURCES += formats/internal/png.cpp
libimagecore_la_CPPFLAGS += -DIMAGECORE_WITH_PNG=1
endif
if HAVE_LIBWEBP
libimagecore_la_SOURCES += formats/internal/webp.cpp
libimagecore_la_CPPFLAGS += -DIMAGECORE_WITH_WEBP=1
endif
if HAVE_LIBLCMS2
libimagecore_la_CPPFLAGS += -DIMAGECORE_WITH_LCMS=1
endif

nobase_pkginclude_HEADERS = imagecore.h
nobase_pkginclude_HEADERS += formats/exif/exifcommon.h formats/exif/exifreader.h formats/exif/exifwriter.h formats/format.h formats/reader.h formats/writer.h image/colorpalette.h
nobase_pkginclude_HEADERS +=  image/colorspace.h image/grayscale.h image/image.h image/interleaved.h image/kernel.h image/resizecrop.h image/rgba.h image/tiledresize.h image/yuv.h image/yuv_semiplanar.h
nobase_pkginclude_HEADERS += utils/endianutils.h utils/mathtypes.h utils/mathutils.h utils/memorystream.h utils/securemath.h
