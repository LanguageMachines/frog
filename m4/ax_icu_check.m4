dnl @synopsis AX_ICU_CHECK([version], [action-if], [action-if-not])
dnl
dnl Test for ICU support
dnl
dnl This will define ICU_LIBS, ICU_CFLAGS, ICU_CXXFLAGS, ICU_IOLIBS.
dnl
dnl Based on ac_check_icu (http://autoconf-archive.cryp.to/ac_check_icu.html)
dnl by Akos Maroy <darkeye@tyrell.hu>.
dnl
dnl Portions Copyright 2005 Akos Maroy <darkeye@tyrell.hu>
dnl Copying and distribution of this file, with or without modification,
dnl are permitted in any medium without royalty provided the copyright
dnl notice and this notice are preserved.
dnl
dnl @author Hunter Morris <huntermorris@gmail.com>
dnl @version 2008-03-18
AC_DEFUN([AX_ICU_CHECK], [
  succeeded=no

  if test -z "$ICU_CONFIG"; then
     AC_PATH_PROG(ICU_CONFIG, icu-config, no)
  fi

  if test "$ICU_CONFIG" = "no" ; then
     echo "*** The icu-config script could not be found. Make sure it is"
     echo "*** in your path, and that taglib is properly installed."
     echo "*** Or see http://www.icu-project.org/"
  else
	ICU_VERSION=`$ICU_CONFIG --version`
	AC_MSG_CHECKING(for ICU >= $1)
	VERSION_CHECK=`expr $ICU_VERSION \>\= $1`
	if test "$VERSION_CHECK" = "1" ; then
	   AC_MSG_RESULT(yes)
	   succeeded=yes

	   AC_MSG_CHECKING(ICU_CFLAGS)
	   ICU_CFLAGS=`$ICU_CONFIG --cflags`
	   AC_MSG_RESULT($ICU_CFLAGS)

	   AC_MSG_CHECKING(ICU_CPPSEARCHPATH)
	   ICU_CPPSEARCHPATH=`$ICU_CONFIG --cppflags-searchpath`
	   AC_MSG_RESULT($ICU_CPPSEARCHPATH)

	   AC_MSG_CHECKING(ICU_CXXFLAGS)
	   ICU_CXXFLAGS=`$ICU_CONFIG --cxxflags`
	   AC_MSG_RESULT($ICU_CXXFLAGS)
	   
	   AC_MSG_CHECKING(ICU_LIBS)
	   ICU_LIBS=`$ICU_CONFIG --ldflags-libsonly`
	   AC_MSG_RESULT($ICU_LIBS)

	   AC_MSG_CHECKING(ICU_IOLIBS)
	   ICU_IOLIBS=`$ICU_CONFIG --ldflags-icuio`
	   AC_MSG_RESULT($ICU_IOLIBS)
	else
	   ICU_CFLAGS=""
	   ICU_CXXFLAGS=""
	   ICU_CPPSEARCHPATH=""
	   ICU_LIBS=""
	   ICU_IOLIBS=""
	   ## If we have a custom action on failure, don't print errors, but
	   ## do set a variable so people can do so.
	   ifelse([$3], ,echo "can't find ICU >= $1",)
        fi

	AC_SUBST(ICU_CFLAGS)
	AC_SUBST(ICU_CXXFLAGS)
	AC_SUBST(ICU_CPPSEARCHPATH)
	AC_SUBST(ICU_LIBS)
	AC_SUBST(ICU_IOLIBS)
  fi

  if test $succeeded = yes; then
     ifelse([$2], , :, [$2])
  else
     ifelse([$3], , AC_MSG_ERROR([Library requirements (ICU) not met.]), [$3])
  fi
])

