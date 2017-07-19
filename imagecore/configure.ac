AC_INIT([imagecore], [1.0], [media-platform@twitter.com])
AM_INIT_AUTOMAKE([subdir-objects])
AM_SILENT_RULES([yes])
AC_CONFIG_MACRO_DIRS([../m4])
AC_PROG_CC
AC_PROG_CXX
AC_LANG([C++])
AC_CHECK_LIB([jpeg], [jpeg_mem_src])
AM_CONDITIONAL([HAVE_LIBJPEG], [test "$ac_cv_lib_jpeg_jpeg_mem_src" = yes])
AC_CHECK_LIB(png16, png_create_read_struct)
AM_CONDITIONAL([HAVE_LIBPNG], [test "$ac_cv_lib_png_png_create_read_struct" = yes])
AC_CHECK_LIB(webp, WebPDecode)
AM_CONDITIONAL([HAVE_LIBWEBP], [test "$ac_cv_lib_webp_WebPDecode" = yes])
AC_CHECK_LIB(lcms2, cmsCloseProfile)
AM_CONDITIONAL([HAVE_LIBLCMS2], [test "$ac_cv_lib_lcms2_cmsCloseProfile" = yes])
AX_CHECK_COMPILE_FLAG([-msse4.1], [CPPFLAGS="$CPPFLAGS -msse4.1"])
AX_CXX_COMPILE_STDCXX_11([ext],[mandatory])
LT_INIT
AC_ENABLE_SHARED
AC_ENABLE_STATIC
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
