#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([frog], [0.13.3], [lamasoftware@science.ru.nl])
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER([config.h])

if test x"${CXXFLAGS+set}" = xset; then
  # the user set CXXFLAGS; don't override it.
  cxx_flags_were_set=true
else
  cxx_flags_were_set=false
fi

# Checks for programs.
AC_PROG_CXX( [g++ c++] )

if $cxx_flags_were_set; then
  CXXFLAGS=$CXXFLAGS
fi

# use libtool
AC_PROG_LIBTOOL
LT_INIT

SYSCONFDIR=$sysconfdir
AC_SUBST([SYSCONFDIR])

# when running tests, use CXX
AC_LANG([C++])

# Checks for libraries.

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_HEADER_DIRENT
AC_CHECK_FUNCS([localtime_r])

ACX_PTHREAD([],[AC_MSG_ERROR([We need pthread support!])])

if test x"$acx_pthread_ok" = xyes; then
   LIBS="$PTHREAD_LIBS $LIBS" \
   CXXFLAGS="$CXXFLAGS $PTHREAD_CFLAGS"
fi

# check OpenMP support
AC_OPENMP
if test "x$ac_cv_prog_cxx_openmp" != "xunsupported"; then
   CXXFLAGS="$CXXFLAGS $OPENMP_CXXFLAGS"
   AC_DEFINE(HAVE_OPENMP, 1 , Define to 1 if you have OpenMP )
else
   AC_MSG_NOTICE(We don't have OpenMP. Multithreaded operation is disabled)
fi

if test $prefix = "NONE"; then
   prefix="$ac_default_prefix"
fi

PKG_PROG_PKG_CONFIG
AC_PATH_PROG(pkgconfigpath, pkg-config, NONE)
if test "$pkgconfigpath" != "NONE"; then
# ugly hack when PKG_CONFIG_PATH isn't defined.
# couldn't get it to work otherwise
  if test "x$PKG_CONFIG_PATH" = x; then
     export PKG_CONFIG_PATH=""
  fi
fi
PKG_CHECK_MODULES([XML2], [libxml-2.0 >= 2.6.16] )
CXXFLAGS="$CXXFLAGS $XML2_CFLAGS"
LIBS="$XML2_LIBS $LIBS"

AX_LIB_READLINE


#check for needed dependencies
useICU=1;
# inspired by feh-1.3.4/configure.ac. Tnx Tom Gilbert and feh hackers.
AC_ARG_WITH(icu,
 [ --with-icu=DIR use ICU installed in <DIR>],
 [if test "$with_icu" = "no"; then
 useICU=0
 else
 CXXFLAGS="$CXXFLAGS -I$withval/include"
 LIBS="-L$withval/lib $LIBS"
 fi] )

if test "$useICU" = "1"; then
 AX_ICU_CHECK( [4.2],
 [CXXFLAGS="$CXXFLAGS $ICU_CPPSEARCHPATH"
 LIBS="$ICU_LIBPATH $ICU_LIBS $ICU_IOLIBS $LIBS"],
 [AC_MSG_FAILURE( "No ICU development environment found. Please check if libicu-ev or the like is installed" )] )
 AC_DEFINE(HAVE_ICU, 1, we want to use ICU )
else
 AC_MSG_ERROR("ICU support is required")
fi

AC_ARG_WITH(ticcutils,
       [  --with-ticcutils=DIR       use ticcutils installed in <DIR>],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
PKG_CHECK_MODULES([ticcutils], [ticcutils >= 0.9] )
CXXFLAGS="$CXXFLAGS $ticcutils_CFLAGS"
LIBS="$ticcutils_LIBS $LIBS"

AC_ARG_WITH(timbl,
       [  --with-timbl=DIR       use timbl installed in <DIR>],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
PKG_CHECK_MODULES([timbl], [timbl >= 6.4.4] )
CXXFLAGS="$CXXFLAGS $timbl_CFLAGS"
LIBS="$timbl_LIBS $LIBS"

AC_ARG_WITH(mbt,
       [  --with-mbt=DIR       use mbt installed in <DIR>],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
PKG_CHECK_MODULES([mbt], [mbt >= 3.2.14] )
CXXFLAGS="$CXXFLAGS $mbt_CFLAGS"
LIBS="$mbt_LIBS $LIBS"

AC_ARG_WITH(folia,
       [  --with-folia=DIR       use libfolia installed in <DIR>],
       [PKG_CONFIG_PATH="$withval/lib/pkgconfig:$PKG_CONFIG_PATH"],
       [PKG_CONFIG_PATH="$prefix/lib/pkgconfig:$PKG_CONFIG_PATH"])
AC_MSG_NOTICE( [pkg-config search path: $PKG_CONFIG_PATH] )
PKG_CHECK_MODULES([folia],[folia >= 1.0])
CXXFLAGS="$CXXFLAGS $folia_CFLAGS"
LIBS="$folia_LIBS $LIBS"

AC_ARG_WITH(ucto,
       [  --with-ucto=DIR       use ucto installed in <DIR>],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
PKG_CHECK_MODULES([ucto], [ucto >= 0.8.4] )
CXXFLAGS="$CXXFLAGS $ucto_CFLAGS"
LIBS="$ucto_LIBS $LIBS"

PKG_CHECK_MODULES([frogdata], [frogdata >= 0.9] )
#

AC_OUTPUT([
  Makefile
  frog.pc
  m4/Makefile
  docs/Makefile
  tests/Makefile
  src/Makefile
  include/Makefile
  include/frog/Makefile
])
