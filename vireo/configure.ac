#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([FULL-PACKAGE-NAME], [VERSION], [BUG-REPORT-ADDRESS])
AM_INIT_AUTOMAKE([subdir-objects])
AM_SILENT_RULES([yes])
AC_CONFIG_MACRO_DIRS([../m4])
AC_CANONICAL_HOST
AC_CONFIG_SRCDIR([base_cpp.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB
AC_LANG([C++])
AX_CXX_COMPILE_STDCXX_14([ext],[mandatory])

# Handles java/scala
AX_CHECK_JAVA_HOME
AX_JNI_INCLUDE_DIR
AC_ARG_ENABLE([scala],
[  --disable-scala  Disable jni interface],
[case "${enableval}" in
  yes) scala=false ;;
  no)  scala=true ;;
  *) AC_MSG_ERROR([bad value ${enableval} for --disable-scala]) ;;
esac],[scala=true])
for JNI_INCLUDE_DIR in $JNI_INCLUDE_DIRS
do
  JNI_CPPFLAGS="$JNI_CPPFLAGS -I$JNI_INCLUDE_DIR"
done
AC_SUBST(JNI_CPPFLAGS, $JNI_CPPFLAGS)
AC_SUBST([JAVA_HOME])
AM_CONDITIONAL([JAVA_HOME_SET], [test ! -z "$JAVA_HOME"])
AM_CONDITIONAL([BUILD_SCALA], [test x$scala = xtrue])

# Checks for libraries.
## Required
AC_CHECK_LIB([lsmash], [lsmash_free], [], [AC_MSG_ERROR(L-Smash is required. Instructions at https://github.com/twitter/vireo)])
## Optional
AC_CHECK_LIB([avcodec], [av_init_packet])
AC_CHECK_LIB([avformat], [av_add_index_entry])
AC_CHECK_LIB([avutil], [av_asprintf])
AC_CHECK_LIB([fdk-aac], [aacDecoder_Open])
AC_CHECK_LIB([ogg], [ogg_packet_clear])
AC_CHECK_LIB([pthread], [pthread_create])
AC_CHECK_LIB([swscale], [swscale_version])
AC_CHECK_LIB([vorbis], [vorbis_block_init])
AC_CHECK_LIB([vorbisenc], [vorbis_encode_init])
AC_CHECK_LIB([vpx], [vpx_free])
AC_CHECK_LIB([webm], [_ZN8mkvmuxer10AudioTrackC1EPj])
AC_CHECK_LIB([x264], [x264_free])
AC_CHECK_LIB([z], [deflate])

# Conditionals for Makefile.am
AM_CONDITIONAL([USE_LIBWEBM], [test `grep -c HAVE_LIBWEBM confdefs.h` -eq 1])
AM_CONDITIONAL([USE_LIBAVCODEC], [test `grep -c HAVE_LIBAVCODEC confdefs.h` -eq 1])
AM_CONDITIONAL([USE_LIBAVFORMAT], [test `grep -c HAVE_LIBAVFORMAT confdefs.h` -eq 1])
AM_CONDITIONAL([USE_LIBAVUTIL], [test `grep -c HAVE_LIBAVUTIL confdefs.h` -eq 1])
AM_CONDITIONAL([USE_LIBFDK_AAC], [test `grep -c HAVE_LIBFDK_AAC confdefs.h` -eq 1])
AM_CONDITIONAL([USE_LIBLSMASH], [test `grep -c HAVE_LIBLSMASH confdefs.h` -eq 1])
AM_CONDITIONAL([USE_LIBVORBIS], [test `grep -c HAVE_LIBVORBIS confdefs.h` -eq 1])
AM_CONDITIONAL([USE_LIBVORBISENC], [test `grep -c HAVE_LIBVORBISENC confdefs.h` -eq 1])
AM_CONDITIONAL([USE_LIBVPX], [test `grep -c HAVE_LIBVPX confdefs.h` -eq 1])
AM_CONDITIONAL([USE_LIBX264], [test `grep -c HAVE_LIBX264 confdefs.h` -eq 1])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stddef.h stdlib.h string.h sys/param.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MMAP
AC_CHECK_FUNCS([getcwd memset mkdir munmap sqrt strrchr])

LT_INIT
AC_ENABLE_SHARED
AC_ENABLE_STATIC
AC_CONFIG_SUBDIRS([../imagecore])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
