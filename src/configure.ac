AC_INIT([Recoll],[m4_esyscmd_s(cat RECOLL-VERSION.txt)])
AC_CONFIG_HEADERS([common/autoconfig.h])
AH_BOTTOM([#include "conf_post.h"])
AC_PREREQ([2.69])
AC_CONFIG_SRCDIR(index/recollindex.cpp)

AM_INIT_AUTOMAKE([1.10 no-define subdir-objects foreign])
AC_DISABLE_STATIC
LT_INIT
AC_CONFIG_MACRO_DIR([m4])

m4_include([m4/iconv.m4])
AM_ICONV
INCICONV=$CPPFLAGS
LIBICONV=$LTLIBICONV

AC_PROG_CXX
# AC_PROG_CXX used to set CXX to C when no compiler was found, but now it's
# g++. So actually try to build a program to verify the compiler.
if test C$CXX = C ; then
   AC_MSG_ERROR([C++ compiler needed. Please install one (ie: gnu g++)])
fi
AC_LANG_PUSH([C++])
AC_LINK_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[rcl_link_ok=yes],[rcl_link_ok=no])
if test "$rcl_link_ok" = "no" ; then
   AC_MSG_ERROR([No working C++ compiler was found])
fi
AC_LANG_POP([C++])

AC_CANONICAL_HOST

# We want librecoll.so to have no undefined symbols because it makes our
# life easier when building the loadable modules for, e.g. Python Can't
# make sense of the libtool -no-undefined flag. It seems to do nothing on
# Linux. Otoh, -Wl,--no-undefined or -z,defs is not portable (e.g. does not
# work on the mac). So set this link flag as a system-dependant value here
case $host_os in
  linux*)
    NO_UNDEF_LINK_FLAG=-Wl,--no-undefined
    ;;
esac

AC_PROG_YACC

LT_INIT
AC_C_BIGENDIAN

AC_SYS_LARGEFILE

# OpenBSD needs sys/param.h for mount.h to compile
AC_CHECK_HEADERS([sys/param.h, spawn.h])


if test "x$ac_cv_func_posix_spawn" = xyes; then :
   AC_ARG_ENABLE(posix_spawn,
    AS_HELP_STRING([--enable-posix_spawn],[Enable the use of posix_spawn().]),
        posixSpawnEnabled=$enableval, posixSpawnEnabled=no)
fi
if test X$posixSpawnEnabled = Xyes ; then
  AC_DEFINE(USE_POSIX_SPAWN, 1, [Use posix_spawn()])
fi

AC_CHECK_HEADERS([sys/mount.h sys/statfs.h sys/statvfs.h sys/vfs.h malloc.h malloc/malloc.h], [], [],
[#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif
])

AC_CHECK_FUNCS([posix_spawn setrlimit kqueue vsnprintf malloc_trim posix_fadvise])

# Use specific 'file' command ? (Useful on solaris to specify
# /usr/local/bin/file instead of the system's which doesn't understand '-i'
AC_ARG_WITH(file-command, 
    AS_HELP_STRING([--with-file-command],[Specify version of 'file' command (ie: --with-file-command=/usr/local/bin/file)]),
        withFileCommand=$withval, withFileCommand=file)
case $withFileCommand in
  file)
    AC_PATH_PROG(fileProg, file);;
  *) 
    fileProg=$withFileCommand;;
esac

if test ! -x "$fileProg"; then
   AC_MSG_ERROR([$fileProg does not exist or is not executable])
fi
AC_DEFINE_UNQUOTED(FILE_PROG, "$fileProg", [Path to the file program])

# Can't use Solaris standard 'file' command, it doesn't support -i
AC_DEFINE(USE_SYSTEM_FILE_COMMAND, 1,
    [Enable using the system's 'file' command to id mime if we fail internally])

# Use aspell to provide spelling expansions ?
# The default is yes. If we do find an aspell installation, we use it. Else
# we do compile the aspell module using an internal copy of aspell.h
# Only --with-aspell=no will completely disable aspell support
AC_ARG_WITH(aspell, 
    AS_HELP_STRING([--without-aspell],[Disable use of aspell spelling package to provide term expansion to other spellings]),
        withAspell=$withval, withAspell=yes)
case $withAspell in
     no);;
     yes)
     AC_PATH_PROG(aspellProg, aspell)
     ;;
     *) # The argument should be the path to the aspell program
     aspellProg=$withAspell
     ;;
esac

if test X$withAspell != Xno ; then
   AC_DEFINE(RCL_USE_ASPELL, 1, [Compile the aspell interface])
   if test X$aspellProg != X ; then
      aspellBase=`dirname $aspellProg`
      aspellBase=`dirname $aspellBase`
      AC_DEFINE_UNQUOTED(ASPELL_PROG, "$aspellProg", [Path to the aspell program])
   fi   
fi

if test -f /usr/include/sys/inotify.h -o -f /usr/include/linux/inotify.h; then
   inot_default=yes
else
   inot_default=no
fi

# Real time monitoring with inotify
AC_ARG_WITH(inotify, 
    AS_HELP_STRING([--with-inotify],[Use inotify for almost real time indexing of modified files (the default
    is yes on Linux).]),
        withInotify=$withval, withInotify=$inot_default)

if test X$withInotify != Xno ; then
   AC_MSG_NOTICE([enabled support for inotify monitoring])
   AC_DEFINE(RCL_MONITOR, 1, [Real time monitoring option])
   AC_DEFINE(RCL_USE_INOTIFY, 1, [Compile the inotify interface])
else
   AC_MSG_NOTICE([inotify not found, inotify monitoring disabled])
fi

# Real time monitoring with FAM
AC_ARG_WITH(fam, 
    AS_HELP_STRING([--with-fam],[Use File Alteration Monitor for almost real time indexing of modified files. Give the fam/gamin library as argument (ie: /usr/lib/libfam.so) if configure does not find the right one.]),
        withFam=$withval, withFam=yes)

if test X$withFam != Xno -a X$withInotify != Xno ; then
   AC_MSG_NOTICE([FAM support enabled but inotify support also enabled. Disabling FAM support and using inotify])
   withFam=no
fi

famLib=""
case $withFam in
     no);;
     yes)
	for dir in /usr/local/lib ${libdir};do 
	 if test -f $dir/libfam.so ; then famLib=$dir/libfam.so;break;fi
	done
        if test X$famLib = X ; then
	  AC_MSG_NOTICE([FAM library not found, disabling FAM and real time indexing support])
	  withFam=no
        fi
     ;;
     *) # The argument should be the path to the fam library
     famLib=$withFam
     ;;
esac

if test X$withFam != Xno ; then
   AC_DEFINE(RCL_MONITOR, 1, [Real time monitoring option])
   AC_DEFINE(RCL_USE_FAM, 1, [Compile the fam interface])
   if test X$famLib != X ; then
      famLibDir=`dirname $famLib`
      famBase=`dirname $famLibDir`
      famBLib=`basename $famLib .so | sed -e s/lib//`
      if test ! -f $famBase/include/fam.h ; then
	 AC_MSG_ERROR([fam.h not found in $famBase/include. Specify --with-fam=no to disable fam support])
      fi
      LIBFAM="-L$famLibDir -l$famBLib"
      AC_MSG_NOTICE([fam library directive: $LIBFAM])
      AC_DEFINE_UNQUOTED(FAM_INCLUDE, "$famBase/include/fam.h",
	[Path to the fam api include file])
   else
	AC_MSG_ERROR([fam library not found])
   fi
fi

# Enable use of threads in the indexing pipeline.  
# Disabled by default on OS X as this actually hurts performance.
# Also disabled on Windows (which does not use configure, see autoconfig-win.h)
case ${host_os} in
    darwin*)
    AC_ARG_ENABLE(idxthreads,
        [--enable-idxthreads Enable multithread indexing.],
        idxthreadsEnabled=$enableval, idxthreadsEnabled=no)
      ;;
    *)
    AC_ARG_ENABLE(idxthreads,
       [--disable-idxthreads  Disable multithread indexing.],
        idxthreadsEnabled=$enableval, idxthreadsEnabled=yes)
      ;;
esac
AM_CONDITIONAL(NOTHREADS, [test X$idxthreadsEnabled = Xno])
if test X$idxthreadsEnabled = Xyes ; then
  AC_DEFINE(IDX_THREADS, 1, [Use multiple threads for indexing])
fi

AC_ARG_ENABLE(testmains,
    AS_HELP_STRING([--enable-testmains],[Enable building small test drivers. These are not unit tests.]),
        buildtestmains=$enableval, buildtestmains=no)
AM_CONDITIONAL([COND_TESTMAINS], [test "$buildtestmains" = yes])

AC_ARG_ENABLE(rclgrep,
    AS_HELP_STRING([--enable-rclgrep],[Enable building the index-less search tool.]),
        buildrclgrep=$enableval, buildrclgrep=no)
AM_CONDITIONAL([COND_RCLGREP], [test "$buildrclgrep" = yes])

# Enable CamelCase word splitting. This is optional because it causes 
# problems with phrases: with camelcase enabled, "MySQL manual"
# will be matched by "MySQL manual" and "my sql manual" but not 
# "mysql manual" (which would need increased slack as manual is now at pos
# 2 instead of 1
AC_ARG_ENABLE(camelcase,
    AS_HELP_STRING([--enable-camelcase],[Enable splitting camelCase words. This is not enabled by default as
   this makes phrase matches more difficult: you need to use matching
   case in the phrase query to get a match. Ie querying for 
   "MySQL manual" and "my sql manual" are the same, but not the same as
   "mysql manual" (in phrases only and you could raise the phrase slack to
   get a match).]),
        camelcaseEnabled=$enableval, camelcaseEnabled=no)
if test X$camelcaseEnabled = Xyes ; then
  AC_DEFINE(RCL_SPLIT_CAMELCASE, 1, [Split camelCase words])
fi

# Disable building the python module.
AC_ARG_ENABLE(python-module,
    AS_HELP_STRING([--disable-python-module],[Do not build the Python module.]),
        pythonEnabled=$enableval, pythonEnabled=yes)

AM_CONDITIONAL(MAKEPYTHON, [test X$pythonEnabled = Xyes])

# Disable building the libchm python wrapper
AC_ARG_ENABLE(python-chm, AS_HELP_STRING([--disable-python-chm],[Do not build the libchm Python wrapper.]),
    pythonChmEnabled=$enableval, pythonChmEnabled=yes)

if test X$pythonChmEnabled = Xyes; then
   AC_CHECK_LIB([chm], [chm_resolve_object], [],
   [AC_MSG_ERROR([--enable-python-chm is set but libchm is not found])])
fi

AM_CONDITIONAL(MAKEPYTHONCHM, [test X$pythonChmEnabled = Xyes])


AC_CHECK_FUNCS(mkdtemp)
AC_CHECK_LIB([pthread], [pthread_create], [], [])
AC_SEARCH_LIBS([dlopen], [dl], [], [])
if test X$ac_cv_search_function != Xno ; then
   AC_DEFINE(HAVE_DLOPEN, 1, [dlopen function is available])
fi   
AC_CHECK_LIB([z], [zlibVersion], [], [])

############# Putenv
AC_MSG_CHECKING(for type of string parameter to putenv)
AC_LANG_PUSH([C++])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <stdlib.h>
  ]], [[
    putenv((const char *)0);
  ]])],[rcl_putenv_string_const="1"],[rcl_putenv_string_const="0"])
if test X$rcl_putenv_string_const = X1 ; then
  AC_DEFINE(PUTENV_ARG_CONST, 1, [putenv parameter is const])
fi
AC_LANG_POP([C++])


#### Look for Xapian. Done in a strange way to work around autoconf
# cache
XAPIAN_CONFIG=${XAPIAN_CONFIG:-no}
if test "$XAPIAN_CONFIG" = "no"; then 
    AC_PATH_PROG(XAPIAN_CONFIG0, [xapian-config], no)
    XAPIAN_CONFIG=$XAPIAN_CONFIG0
fi
if test "$XAPIAN_CONFIG" = "no"; then 
   AC_PATH_PROG(XAPIAN_CONFIG1, [xapian-config-1.3], no)
   XAPIAN_CONFIG=$XAPIAN_CONFIG1
fi
if test "$XAPIAN_CONFIG" = "no"; then 
   AC_PATH_PROG(XAPIAN_CONFIG2, [xapian-config-1.1], no)
   XAPIAN_CONFIG=$XAPIAN_CONFIG2
fi

if test "$XAPIAN_CONFIG" = "no" ; then
   AC_MSG_ERROR([Cannot find xapian-config command in $PATH. Is
xapian-core installed ?])
   exit 1
fi
LIBXAPIAN=`$XAPIAN_CONFIG --libs`
# The --static thing fails with older Xapians. Happily enough they don't
# need it either (because there are no needed libraries (no uuid and we
# deal explicitly with libz)
LIBXAPIANSTATICEXTRA=`$XAPIAN_CONFIG --static --libs 2> /dev/null`
# Workaround for problem in xapian-config in some versions: wrongly lists
# libstdc++.la in the lib list
for i in $LIBXAPIAN ; do
    case $i in
    *stdc++*|-lm|-lgcc_s|-lc);;
    *) tmpxaplib="$tmpxaplib $i";;
    esac
done
LIBXAPIAN=$tmpxaplib
LIBXAPIANDIR=`$XAPIAN_CONFIG --libs | awk '{print $1}'`
case A"$LIBXAPIANDIR" in
  A-L*) LIBXAPIANDIR=`echo $LIBXAPIANDIR | sed -e 's/-L//'`;;
  *) LIBXAPIANDIR="";;
esac
XAPIANCXXFLAGS=`$XAPIAN_CONFIG --cxxflags`

#echo XAPIAN_CONFIG: $XAPIAN_CONFIG 
#echo LIBXAPIAN: $LIBXAPIAN
#echo LIBXAPIANDIR: $LIBXAPIANDIR
#echo LIBXAPIANSTATICEXTRA: $LIBXAPIANSTATICEXTRA
#echo XAPIANCXXFLAGS: $XAPIANCXXFLAGS


PKG_CHECK_MODULES([XSLT], [libxslt], [], AC_MSG_ERROR([libxslt]))

AC_ARG_ENABLE(xadump, 
    AS_HELP_STRING([--enable-xadump],[Enable building the xadump low level Xapian access program.]),
        enableXADUMP=$enableval, enableXADUMP="no")
AM_CONDITIONAL(MAKEXADUMP, [test X$enableXADUMP = Xyes])

AC_ARG_ENABLE(userdoc,
    AS_HELP_STRING([--disable-userdoc],[Disable building the user manual. (Avoids the need for docbook xml/xsl files and TeX tools.]),
        enableUserdoc=$enableval, enableUserdoc="yes")
AM_CONDITIONAL(MAKEUSERDOC, [test X$enableUserdoc = Xyes])



#### QT
# The way qt and its tools (qmake especially) are installed is very
# different between systems (and maybe qt versions)
#
# In general we need QTDIR to be set, because it is used inside the
# qmake-generated makefiles. But there are exceptions: ie on debian3.1 (at
# least on the sourceforge compile farm), QTDIR is not needed because qmake
# generates hard paths (and is installed in /usr/bin). We don't want to
# force the user to set QTDIR if it is not needed.
#
# The logic is then to first look for qmake, possibly using QTDIR if it is
# set.
# 
# If QTDIR is not set, we then generate a bogus qt project and check if
# QTDIR is needed in the Makefile, in which case we complain.
#
# QMAKESPEC: on most Linux system, there is a 'default' link inside the
# mkspecs directory, so that QMAKESPEC is not needed.
# If QMAKESPEC is not set and needed, the qmake test at the previous test
# will have failed, and we tell the user to check his environment.
#
AC_ARG_ENABLE(qtgui, 
    AS_HELP_STRING([--disable-qtgui],[Disable the QT-based graphical user interface.]),
        enableQT=$enableval, enableQT="yes")
AM_CONDITIONAL(MAKEQT, [test X$enableQT = Xyes])

AC_ARG_ENABLE(recollq, 
    AS_HELP_STRING([--enable-recollq],[Enable building the recollq command line query tool (recoll -t without
   need for Qt). This is done by default if --disable-qtgui is set but this
   option enables forcing it.]),
        enableRECOLLQ=$enableval, enableRECOLLQ="no")
if test X"$enableRECOLLQ" != X ; then
    AM_CONDITIONAL(MAKECMDLINE, [test X$enableRECOLLQ = Xyes])
else
    AM_CONDITIONAL(MAKECMDLINE, [test X$enableQT = Xno])
fi


if test X$enableQT = Xyes ; then

  if test X$QTDIR != X ; then
     PATH=$PATH:$QTDIR/bin
     export PATH
  fi
  
  if test X$QMAKE = X ; then
     QMAKE=qmake
  fi
  case $QMAKE in
    */*) QMAKEPATH=$QMAKE;;
    *) AC_PATH_PROG([QMAKEPATH], $QMAKE, NOTFOUND);;
  esac

  if test X$QMAKEPATH = XNOTFOUND ; then 
     AC_MSG_ERROR([Cannot find the qmake program. Maybe you need to install
  qt development files and tools and/or set the QTDIR environment variable?])
  fi
  QMAKE=$QMAKEPATH
  QTGUI=qtgui

 ##### Using Qt webkit for reslist display? Else Qt textbrowser
  AC_ARG_ENABLE(webkit,
    AS_HELP_STRING([--disable-webkit],[Disable use of qt-webkit (only meaningful if qtgui is enabled).]),
        enableWebkit=$enableval, enableWebkit="yes")

  if test "$enableWebkit" = "yes" ; then
   QMAKE_ENABLE_WEBKIT=""
   QMAKE_DISABLE_WEBKIT="#"
  else
   QMAKE_ENABLE_WEBKIT="#"
   QMAKE_DISABLE_WEBKIT=""
  fi

  AC_ARG_ENABLE(webengine,
    AS_HELP_STRING([--enable-webengine],[Enable use of qt-webengine (only meaningful if qtgui is enabled), in
      place or qt-webkit.]),
        enableWebengine=$enableval, enableWebengine="no")

  if test "$enableWebengine" = "yes" ; then
   QMAKE_ENABLE_WEBENGINE=""
   QMAKE_DISABLE_WEBENGINE="#"
   QMAKE_ENABLE_WEBKIT="#"
   QMAKE_DISABLE_WEBKIT=""
  else
   QMAKE_ENABLE_WEBENGINE="#"
   QMAKE_DISABLE_WEBENGINE=""
  fi

 ##### Using QZeitGeist lib ? Default no for now
  AC_ARG_WITH(qzeitgeist,
    AS_HELP_STRING([--with-qzeitgeist],[Enable the use of the qzeitgeist library to send zeitgeist events.]),
        withQZeitgeist=$withval, withQZeitgeist="no")

  case "$withQZeitgeist" in 
    no)  LIBQZEITGEIST=;;
    yes) LIBQZEITGEIST=-lqzeitgeist;;
    *)   LIBQZEITGEIST=$withQZeitgeist;;
  esac	

  if test "$withQZeitgeist" != "no" ; then
   QMAKE_ENABLE_ZEITGEIST=""
   QMAKE_DISABLE_ZEITGEIST="#"
  else
   QMAKE_ENABLE_ZEITGEIST="#"
   QMAKE_DISABLE_ZEITGEIST=""
  fi

 # Retain debugging symbols in GUI recoll ? This makes it enormous (~50MB)
  AC_ARG_ENABLE(guidebug,
    AS_HELP_STRING([--enable-guidebug],[Generate and retain debug symbols in GUI program (makes the file very big).]),
        enableGuiDebug=$enableval, enableGuiDebug="no")

  if test "$enableGuiDebug" = "yes" ; then
   QMAKE_ENABLE_GUIDEBUG=""
  else
   QMAKE_ENABLE_GUIDEBUG="#"
  fi

  AC_CONFIG_FILES($QTGUI/recoll.pro)

  ##################### End QT stuff
fi

dnl Borrow a macro definition from pkg.config,
dnl for older installs that lack it.
m4_ifndef([PKG_CHECK_VAR], [
dnl PKG_CHECK_VAR(VARIABLE, MODULE, CONFIG-VARIABLE,
dnl [ACTION-IF-FOUND], [ACTION-IF-NOT-FOUND])
dnl -------------------------------------------
dnl Retrieves the value of the pkg-config variable for the given module.
AC_DEFUN([PKG_CHECK_VAR],
[AC_REQUIRE([PKG_PROG_PKG_CONFIG])dnl
AC_ARG_VAR([$1], [value of $3 for $2, overriding pkg-config])dnl

_PKG_CONFIG([$1], [variable="][$3]["], [$2])
AS_VAR_COPY([$1], [pkg_cv_][$1])

AS_VAR_IF([$1], [""], [$5], [$4])dnl
])dnl PKG_CHECK_VAR
])

### Systemd
AC_ARG_WITH([systemd],
    AS_HELP_STRING([--without-systemd],[Disable installation of the systemd unit files.]))
AC_ARG_WITH([system-unit-dir],
    AS_HELP_STRING([--with-system-unit-dir=DIR],[Install location for systemd system unit files]),
    [SYSTEMD_SYSTEM_UNIT_DIR="$withval"],
    [PKG_CHECK_VAR([SYSTEMD_SYSTEM_UNIT_DIR], [systemd], [systemdsystemunitdir])])
AC_ARG_WITH([user-unit-dir],
    AS_HELP_STRING([--with-user-unit-dir=DIR],[Install location for systemd user unit files]),
    [SYSTEMD_USER_UNIT_DIR="$withval"],
    [PKG_CHECK_VAR([SYSTEMD_USER_UNIT_DIR], [systemd], [systemduserunitdir])])

if test "x$SYSTEMD_SYSTEM_UNIT_DIR" = "x" -o \
        "x$SYSTEMD_USER_UNIT_DIR" = "x"; then
    with_systemd="no"
fi

AM_CONDITIONAL([INSTALL_SYSTEMD_UNITS], [test "X$with_systemd" != "Xno"])

### X11: this is needed for the session monitoring code (in recollindex -m)
AC_ARG_ENABLE(x11mon, 
    AS_HELP_STRING([--disable-x11mon],[Disable recollindex support for X11 session monitoring.]),
        enableX11mon=$enableval, enableX11mon="yes")

if test X$withInotify = Xno -a X$withFam = Xno ; then
  enableX11mon=no
fi

if test "$enableX11mon" = "yes" ; then
  AC_PATH_XTRA
  X_LIBX11=-lX11
else
  AC_DEFINE(DISABLE_X11MON, 1, [No X11 session monitoring support])
  X_LIBX11=""
fi
#echo X_CFLAGS "'$X_CFLAGS'" X_PRE_LIBS "'$X_PRE_LIBS'" X_LIBS \
#      "'$X_LIBS'" X_LIBX11 "'$X_LIBX11'" X_EXTRA_LIBS "'$X_EXTRA_LIBS'"

# For communicating the value of RECOLL_DATADIR to non-make-based
# subpackages like python-recoll, we have to expand prefix in here, because
# things like "datadir = ${prefix}/share" (which is what we'd get by
# expanding @datadir@) don't mean a thing in Python... I guess we could
# have a piece of shell-script text to be substituted into and executed by
# setup.py for getting the value of pkgdatadir, but really...
m_prefix=$prefix
test "X$m_prefix" = "XNONE" && m_prefix=/usr/local
m_datadir=${m_prefix}/share
RECOLL_DATADIR=${m_datadir}/recoll

RCLVERSION=$PACKAGE_VERSION
RCLLIBVERSION=$RCLVERSION

AC_SUBST(NO_UNDEF_LINK_FLAG)
AC_SUBST(RECOLL_DATADIR)
AC_SUBST(X_CFLAGS)
AC_SUBST(X_PRE_LIBS)
AC_SUBST(X_LIBS)
AC_SUBST(X_LIBX11)
AC_SUBST(X_EXTRA_LIBS)
AC_SUBST(INCICONV)
AC_SUBST(LIBICONV)
AC_SUBST(LIBXAPIAN)
AC_SUBST(LIBXAPIANDIR)
AC_SUBST(LIBXAPIANSTATICEXTRA)
AC_SUBST(LIBFAM)
AC_SUBST(QMAKE)
AC_SUBST(QTGUI)
AC_SUBST(XAPIANCXXFLAGS)
AC_SUBST(QMAKE_ENABLE_WEBKIT)
AC_SUBST(QMAKE_DISABLE_WEBKIT)
AC_SUBST(QMAKE_ENABLE_WEBENGINE)
AC_SUBST(QMAKE_DISABLE_WEBENGINE)
AC_SUBST(QMAKE_ENABLE_GUIDEBUG)
AC_SUBST(QMAKE_DISABLE_GUIDEBUG)
AC_SUBST(QMAKE_ENABLE_ZEITGEIST)
AC_SUBST(QMAKE_DISABLE_ZEITGEIST)
AC_SUBST(LIBQZEITGEIST)
AC_SUBST(RCLVERSION)
AC_SUBST(RCLLIBVERSION)
AC_SUBST(XSLT_CFLAGS)
AC_SUBST(XSLT_LIBS)
AC_SUBST([SYSTEMD_SYSTEM_UNIT_DIR])
AC_SUBST([SYSTEMD_USER_UNIT_DIR])

AC_CONFIG_FILES([Makefile python/recoll/setup.py
 python/pychm/setup.py])

if test X$buildtestmains = Xyes ; then
   AC_CONFIG_FILES([testmains/Makefile])
fi
if test X$buildrclgrep = Xyes ; then
   AC_CONFIG_FILES([rclgrep/Makefile])
fi

AC_OUTPUT
