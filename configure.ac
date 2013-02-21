#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
# $Id$
# $URL$

AC_PREREQ(2.59)
AC_INIT([frog], [0.12.17], [timbl@uvt.nl])
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
AC_PROG_CXX( [g++] )

if $cxx_flags_were_set; then
  CXXFLAGS=$CXXFLAGS
fi

AC_PROG_RANLIB
# use libtool
AC_PROG_LIBTOOL
AC_SUBST([LIBTOOL_DEPS])

SYSCONFDIR=$sysconfdir
AC_SUBST([SYSCONFDIR])

# when running tests, use CXX
AC_LANG([C++])

# Checks for libraries.

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_HEADER_DIRENT
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK

ACX_PTHREAD([],[AC_MSG_ERROR([We need pthread support!])])

if test x"$acx_pthread_ok" = xyes; then
   LIBS="$PTHREAD_LIBS $LIBS" \
   CXXFLAGS="$CXXFLAGS $PTHREAD_CFLAGS"
fi

# check for OpenMP
OPENMP_CHECK( [CXXFLAGS="$CXXFLAGS $OPENMP_CXXFLAGS"
	      AC_DEFINE(HAVE_OPENMP, 1 , Define to 1 if you have OpenMP )],
	      [AC_MSG_NOTICE(We don't have OpenMP. Multithreaded testing is disabled)] )

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

#check for needed dependencies
useICU=1;
# inspired by feh-1.3.4/configure.ac.  Tnx Tom Gilbert and feh hackers.
AC_ARG_WITH(icu,
       [  --with-icu=DIR       use ICU installed in <DIR>],
       [if test "$with_icu" = "no"; then
           useICU=0
	else
	   CXXFLAGS="$CXXFLAGS -I$withval/include"
           LIBS="-L$withval/lib $LIBS"
	fi] )

if test "$useICU" = "1"; then
  AX_ICU_CHECK( [3.6],
		[CXXFLAGS="$CXXFLAGS $ICU_CPPSEARCHPATH"
		LIBS="$ICU_LIBPATH $ICU_LIBS $ICU_IOLIBS $LIBS"],
		[AC_MSG_FAILURE( "No ICU development environment found. Please check if libicu-dev or the like is installed" )] )
  AC_DEFINE(HAVE_ICU, 1, we want to use ICU )
else
  AC_MSG_ERROR("ICU support is required")
fi

AC_ARG_WITH(timbl,
       [  --with-timbl=DIR       use timbl installed in <DIR>],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
PKG_CHECK_MODULES([timbl], [timbl >= 6.4.2] )
CXXFLAGS="$CXXFLAGS $timbl_CFLAGS"
LIBS="$timbl_LIBS $LIBS"

AC_ARG_WITH(ticcutils,
       [  --with-ticcutils=DIR       use ticcutils installed in <DIR>],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
PKG_CHECK_MODULES([ticcutils], [ticcutils >= 0.1] )
CXXFLAGS="$CXXFLAGS $ticcutils_CFLAGS"
LIBS="$ticcutils_LIBS $LIBS"

AC_ARG_WITH(timblserver,
       [  --with-timblserver=DIR     use timblserver installed in <DIR>],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
PKG_CHECK_MODULES([timblserver], [timblserver >= 1.4] )
CXXFLAGS="$CXXFLAGS $timblserver_CFLAGS"
LIBS="$timblserver_LIBS $LIBS"

AC_ARG_WITH(mbt,
       [  --with-mbt=DIR       use mbt installed in <DIR>],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
PKG_CHECK_MODULES([mbt], [mbt >= 3.2.8] )
CXXFLAGS="$CXXFLAGS $mbt_CFLAGS"
LIBS="$mbt_LIBS $LIBS"

AC_ARG_WITH(folia,
       [  --with-folia=DIR       use libfolia installed in <DIR>],
       [PKG_CONFIG_PATH="$withval/lib/pkgconfig:$PKG_CONFIG_PATH"],
       [PKG_CONFIG_PATH="$prefix/lib/pkgconfig:$PKG_CONFIG_PATH"])
AC_MSG_NOTICE( [pkg-config search path: $PKG_CONFIG_PATH] )
PKG_CHECK_MODULES([folia],[folia >= 0.10])
CXXFLAGS="$CXXFLAGS $folia_CFLAGS"
LIBS="$folia_LIBS $LIBS"

AC_ARG_WITH(ucto,
       [  --with-ucto=DIR       use ucto installed in <DIR>],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$withval/lib/pkgconfig"],
       [PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"])
PKG_CHECK_MODULES([ucto], [ucto >= 0.5.3] )
CXXFLAGS="$CXXFLAGS $ucto_CFLAGS"
LIBS="$ucto_LIBS $LIBS"

#
# the ac_python_devel.m4 script tramples upon $LIBS, so we save it
save_LIBS="$LIBS"
save_FLAGS="$CXXFLAGS"

AC_ARG_WITH(python,
       [  --with-python=DIR       use python executable installed in <DIR>],
       [PYTHON="$withval/python"],
       [PYTHON="python"])

#we need Python > 2.5.x
AM_PATH_PYTHON([2.5],
	[PYTHONDIR=${pkgpythondir}
	 AC_SUBST([PYTHONDIR])],
	[:])

# set PYTHON_EXTRA_LIBS to a non-empty string
# this fools AC_PYTHON_DEVEL into NOT setting it.
# That fails on some systems with linking errors
# we don't need PYTHON_EXTRA_LIBS
PYTHON_EXTRA_LIBS=" "

AC_PYTHON_DEVEL([>= '2.5'])
if test "x$PYTHON_CPPFLAGS" != "x"; then
 CXXFLAGS="$save_FLAGS $PYTHON_CPPFLAGS"
 LIBS="$save_LIBS $PYTHON_LDFLAGS"
else
 CXXFLAGS="$save_FLAGS"
 LIBS="$save_LIBS"
fi

AC_CHECK_HEADERS( [Python.h], , AC_MSG_FAILURE("can't find Python C API") )

AC_OUTPUT([
  Makefile
  m4/Makefile
  docs/Makefile
  scripts/Makefile
  tests/Makefile
  src/Makefile
  include/Makefile
  include/frog/Makefile
])
