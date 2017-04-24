AC_INIT([vireo], [1.0], [kquennesson@twitter.com])
AM_INIT_AUTOMAKE([subdir-objects])
AC_CANONICAL_HOST
AC_PROG_CC
AC_PROG_CXX
AC_LANG([C++])
AX_CXX_COMPILE_STDCXX_14([ext],[mandatory])
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
AC_CHECK_HEADERS([x264.h libswscale/swscale.h libavformat/avformat.h libavcodec/avcodec.h libavutil/avutil.h lsmash.h vpx/vp8.h mkvmuxer.hpp vorbis/codec.h fdk-aac/aacenc_lib.h ogg/ogg.h], [], AC_MSG_ERROR([Missing required dependency] ))

AC_SUBST(JNI_CPPFLAGS, $JNI_CPPFLAGS)
AC_SUBST([JAVA_HOME])
AM_CONDITIONAL([JAVA_HOME_SET], [test ! -z "$JAVA_HOME"])
AM_CONDITIONAL([BUILD_SCALA], [test x$scala = xtrue])
LT_INIT
AC_ENABLE_SHARED
AC_ENABLE_STATIC
AC_CONFIG_SUBDIRS([../imagecore])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
