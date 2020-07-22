#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
#also adapt version number in codemeta.json
AC_INIT([frog], [0.22], [lamasoftware@science.ru.nl]) #adapt version number in codemeta.json as well
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

# when running tests, use CXX
AC_LANG([C++])

# Checks for libraries.

# Checks for typedefs, structures, and compiler characteristics.
# check OpenMP support
AC_OPENMP
if test "x$ac_cv_prog_cxx_openmp" != "x"; then
  if test "x$ac_cv_prog_cxx_openmp" != "xunsupported"; then
    CXXFLAGS="$CXXFLAGS $OPENMP_CXXFLAGS"
    AC_DEFINE([HAVE_OPENMP], [1] , [Define to 1 if you have OpenMP] )
  else
    AC_MSG_NOTICE([We don't have OpenMP. Multithreaded operation is disabled])
  fi
fi

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

if test $prefix = "NONE"; then
   prefix="$ac_default_prefix"
fi

PKG_PROG_PKG_CONFIG
if test "x$PKG_CONFIG_PATH" = x; then
    export PKG_CONFIG_PATH="$prefix/lib/pkgconfig"
else
    export PKG_CONFIG_PATH="$prefix/lib/pkgconfig:$PKG_CONFIG_PATH"
fi

AC_OSX_PKG( [icu4c] )

PKG_CHECK_MODULES( [TEXTCAT],
  [libexttextcat],
  [TEXTCAT_FOUND=1
   CXXFLAGS="$CXXFLAGS $TEXTCAT_CFLAGS"
   LIBS="$TEXTCAT_LIBS $LIBS"],
  [PKG_CHECK_MODULES( [TEXTCAT],
   [libtextcat],
   [TEXTCAT_FOUND=1
    CXXFLAGS="$CXXFLAGS $TEXTCAT_CFLAGS"
    LIBS="$TEXTCAT_LIBS $LIBS"],
   [TEXTCAT_FOUND=0]
   )]
 )

if test $TEXTCAT_FOUND = 0; then
# So, no pkg-config for textcat found.
#     Hopefully an old style version can be found???
  AC_CHECK_OLD_TEXTCAT
  if test $TEXTCAT_FOUND = 1; then
    AC_DEFINE([HAVE_OLD_TEXTCAT], [1], [textcat needs C linkage])
  fi
fi

if test $TEXTCAT_FOUND = 1; then
  AC_SEARCH_LM
fi

if test $TEXTCAT_FOUND = 0; then
  AC_MSG_ERROR( [no working libtextcat or libexttextcat found!] )
else
  AC_DEFINE([HAVE_TEXTCAT], [1], [textcat])
fi

PKG_CHECK_MODULES([XML2], [libxml-2.0 >= 2.6.16] )
CXXFLAGS="$CXXFLAGS $XML2_CFLAGS"
LIBS="$XML2_LIBS $LIBS"

PKG_CHECK_MODULES([ICU], [icu-uc >= 50 icu-io] )
CXXFLAGS="$CXXFLAGS $ICU_CFLAGS"
LIBS="$ICU_LIBS $LIBS"

AX_LIB_READLINE

PKG_CHECK_MODULES([ticcutils], [ticcutils >= 0.24] )

CXXFLAGS="$CXXFLAGS $ticcutils_CFLAGS"
LIBS="$ticcutils_LIBS $LIBS"

PKG_CHECK_MODULES([timbl], [timbl >= 6.4.14] )
CXXFLAGS="$CXXFLAGS $timbl_CFLAGS"
LIBS="$timbl_LIBS $LIBS"

PKG_CHECK_MODULES([mbt], [mbt >= 3.5] )
CXXFLAGS="$CXXFLAGS $mbt_CFLAGS"
LIBS="$mbt_LIBS $LIBS"

PKG_CHECK_MODULES([folia],[folia >= 2.4])
CXXFLAGS="$CXXFLAGS $folia_CFLAGS"
LIBS="$folia_LIBS $LIBS"

PKG_CHECK_MODULES([ucto], [ucto >= 0.15] )
CXXFLAGS="$CXXFLAGS $ucto_CFLAGS"
LIBS="$ucto_LIBS $LIBS"

UCTO_VERSION=`pkg-config --modversion ucto`
UCTO_INT="${UCTO_VERSION//.}" # no dots
UCTO_INT=$((10#$UCTO_INT))    # no leading 0 (that's octal)
AC_DEFINE_UNQUOTED( [UCTO_INT_VERSION],
	   	    [${UCTO_INT}],
		    [The integer representation of the ucto version])

PKG_CHECK_MODULES([frogdata], [frogdata >= 0.15] )

AC_CHECK_PROGS([DOXYGEN], [doxygen])
if test -z "$DOXYGEN"; then
   AC_MSG_WARN([Doxygen not found - continue without Doxygen support])
else
   AC_CHECK_PROGS([DOT], [dot])
   if test -z "$DOT"; then
      AC_MSG_ERROR([Doxygen needs dot, please install dot first])
   fi
fi
AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$DOXYGEN"])

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
