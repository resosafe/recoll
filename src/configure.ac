AC_INIT(Recoll, 1.6)
AC_CONFIG_HEADERS([common/autoconfig.h])
AC_PREREQ(2.53)
AC_CONFIG_SRCDIR(index/recollindex.cpp)

sys=`uname | tr / _`

if test ! -f mk/$sys ; then
   AC_MSG_NOTICE([
    No system configuration file found in mk/ for uname = '$sys'. 
    Trying with Default file. 
    If the build fails, you'll need to write a configuration file, starting 
    from one of the existing ones.])

    sys=Default   
fi
(cd mk; rm -f sysconf; ln -s $sys sysconf)

AC_CHECK_HEADERS(sys/mount.h sys/statfs.h sys/statvfs.h sys/vfs.h)

# Use specific 'file' command ? (Useful on solaris to specify
# /usr/local/bin/file instead of the system's which doesn't understand '-i'
AC_ARG_WITH(file-command, 
    AC_HELP_STRING([--with-file-command],
   [Specify version of 'file' command (ie: --with-file-command=/usr/local/bin/file)]),
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
if test X$sys != XSunOS -o X$fileProg != X/usr/bin/file; then
   AC_DEFINE(USE_SYSTEM_FILE_COMMAND, 1, [Enable using the system's 'file' command to id mime if we fail internally])
fi


# Use aspell to provide spelling expansions ?
# The default is yes. If we do find an aspell installation, we use it. Else
# we do compile the aspell module using an internal copy of aspell.h
# Only --with-aspell=no will completely disable aspell support
AC_ARG_WITH(aspell, 
    AC_HELP_STRING([--without-aspell],
   [Disable use of aspell spelling package to provide term expansion to other spellings]),
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
      AC_DEFINE_UNQUOTED(ASPELL_PROG, "$aspellProg",
	[Path to the aspell program])
      if test -f $aspellBase/include/aspell.h ; then	
          AC_DEFINE_UNQUOTED(ASPELL_INCLUDE, "$aspellBase/include/aspell.h",
	    [Path to the aspell api include file])
      else
	AC_MSG_NOTICE([aspell support enabled but aspell package not found. Compiling with internal aspell interface file])
	 AC_DEFINE(ASPELL_INCLUDE, ["aspell-local.h"])
      fi
   else
     # aspell support enabled but no aspell install yet
	AC_MSG_NOTICE([aspell support enabled but aspell package not found. Compiling with internal aspell interface file])
	AC_DEFINE(ASPELL_INCLUDE, ["aspell-local.h"])
   fi
fi

if test -f /usr/include/sys/inotify.h ; then
   inot_default=yes
else
   inot_default=no
fi

# Real time monitoring with inotify
AC_ARG_WITH(inotify, 
    AC_HELP_STRING([--with-inotify],
   [Use inotify for almost real time indexing of modified files.]),
        withInotify=$withval, withInotify=$inot_default)

if test X$withInotify != Xno ; then
   AC_MSG_NOTICE([enabled support for inotify monitoring])
   AC_DEFINE(RCL_MONITOR, 1, [Real time monitoring option])
   AC_DEFINE(RCL_USE_INOTIFY, 1, [Compile the inotify interface])
else
   AC_MSG_NOTICE([inotify monitoring disabled])
fi

# Real time monitoring with FAM
AC_ARG_WITH(fam, 
    AC_HELP_STRING([--with-fam],
   [Use File Alteration Monitor for almost real time indexing of modified files. Give the fam/gamin library as argument (ie: /usr/lib/libfam.so) if configure does not find the right one.]),
        withFam=$withval, withFam=no)
if test X$withFam != Xno -a X$withInotify != Xno ; then
   AC_MSG_NOTICE([FAM support enabled but inotify support also enabled. Disabling FAM support and using inotify])
   withFam=no
fi

case $withFam in
     no);;
     yes)
	for dir in /usr/local/lib ${libdir};do 
	 if test -f $dir/libfam.so ; then famLib=$dir/libfam.so;break;fi
	done
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

# Enable use of file extended attributes. 
# Not by default as these are little used for now.
AC_ARG_ENABLE(xattr,
    AC_HELP_STRING([--enable-xattr],
   [Enable fetching metadata from file extended attributes. This is only
   useful if some application creates them on (part of) your data set. You also 
   need to set up appropriate mappings in the configuration.]),
        xattrEnabled=$enableval, xattrEnabled=no)

if test X$xattrEnabled = Xyes ; then
  AC_DEFINE(RCL_USE_XATTR, 1, [Use file extended attributes])
fi

# Enable CamelCase word splitting. This is optional because it causes 
# problems with phrases: with camelcase enabled, "MySQL manual"
# will be matched by "MySQL manual" and "my sql manual" but not 
# "mysql manual" (which would need increased slack as manual is now at pos
# 2 instead of 1
AC_ARG_ENABLE(camelcase,
    AC_HELP_STRING([--enable-camelcase],
   [Enable splitting camelCase words. This is not enabled by default as
   this makes phrase matches more difficult: you need to use matching
   case in the phrase query to get a match. Ie querying for 
   "MySQL manual" and "my sql manual" are the same, but not the same as
   "mysql manual" (in phrases only and you could raise the phrase slack to
   get a match).]),
        camelcaseEnabled=$enableval, camelcaseEnabled=no)

if test X$camelcaseEnabled = Xyes ; then
  AC_DEFINE(RCL_SPLIT_CAMELCASE, 1, [Split camelCase words])
fi


AC_CHECK_FUNCS(mkdtemp)

##### Look for iconv. We first look for libiconv in /usr/local/lib:/usr/lib
##    then in libc (Linux, solaris)
AC_LANG(C++)
LIBICONV=""
S_LDFLAGS=$LDFLAGS
dir=/usr/local/lib
LDFLAGS="$S_LDFLAGS -L$dir"
unset ac_cv_lib_iconv_iconv_open
AC_CHECK_LIB(iconv, iconv_open, LIBICONV="-L$dir -liconv";INCICONV=-I/usr/local/include)
if test A"$LIBICONV" = A ; then
  dir=${libdir}
  LDFLAGS="$S_LDFLAGS -L$dir"
  unset ac_cv_lib_iconv_iconv_open
  AC_CHECK_LIB(iconv, iconv_open, LIBICONV="-L$dir -liconv";INCICONV=-I/usr/include)
  if test A"$LIBICONV" = A; then
    dir=${libdir}
    LDFLAGS="$S_LDFLAGS -L$dir"
    unset ac_cv_lib_iconv_iconv_open
    AC_CHECK_LIB(c, iconv_open, LIBICONV=NONE;INCICONV=-I/usr/include)
  fi
fi
LDFLAGS=$S_LDFLAGS
if test A"$LIBICONV" = A ; then
   AC_MSG_ERROR([Cannot find iconv_open anywhere. Please install iconv])
   exit 1
fi
if test A"$LIBICONV" = ANONE ; then
   LIBICONV=""
fi
#echo LIBICONV $LIBICONV
#echo INCICONV $INCICONV

CPPFLAGS="$CPPFLAGS $INCICONV"
AC_MSG_CHECKING(for type of inbuf parameter to iconv)
AC_TRY_COMPILE([
    #include <stddef.h>
    #include <iconv.h>
  ],[
    iconv(0,(const char **)0,(size_t *)0,(char **)0,(size_t *)0);
  ], rcl_iconv_inbuf_const="1", rcl_iconv_inbuf_const="0")
if test X$rcl_iconv_inbuf_const = X1 ; then
  AC_DEFINE(RCL_ICONV_INBUF_CONST, 1, [iconv parameter 2 is const char**])
fi


AC_MSG_CHECKING(for type of string parameter to putenv)
AC_TRY_COMPILE([
    #include <stdlib.h>
  ],[
    putenv((const char *)0);
  ], rcl_putenv_string_const="1", rcl_putenv_string_const="0")
if test X$rcl_putenv_string_const = X1 ; then
  AC_DEFINE(PUTENV_ARG_CONST, 1, [putenv parameter is const])
fi

#### Look for Xapian
AC_PATH_PROG(XAPIAN_CONFIG, xapian-config, no)
if test "$XAPIAN_CONFIG" = "no" ; then
   AC_MSG_ERROR([Cannot find xapian-config command in $PATH. Is
xapian-core installed ?])
   exit 1
fi
LIBXAPIAN=`$XAPIAN_CONFIG --libs`
# Workaround for problem in xapian-config in some versions: wrongly lists
# libstdc++.la in the lib list
for i in $LIBXAPIAN ; do
    case $i in
    *stdc++*|-lm|-lgcc_s|-lc);;
    *) tmpxaplib="$tmpxaplib $i";;
    esac
done
LIBXAPIAN=$tmpxaplib
# Also recent xapian libs need lz even when they think they don't...
LIBXAPIAN="$LIBXAPIAN -lz"
XAPIANCXXFLAGS=`$XAPIAN_CONFIG --cxxflags`

#echo XAPIAN_CONFIG: $XAPIAN_CONFIG 
#echo LIBXAPIAN: $LIBXAPIAN
#echo XAPIANCXXFLAGS: $XAPIANCXXFLAGS

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
AC_ARG_WITH(gui, 
    AC_HELP_STRING([--without-gui],
   [Disable the QT user interface and auxiliary x11 uses.]),
        withQT=$withval, withQT="yes")

if test "$withQT" != "yes" ; then
   AC_DEFINE(WITHOUT_X11)
   NOQTMAKE="#"
   NOCMDLINE=""
else
  NOQTMAKE=""
  NOCMDLINE="#"

  if test X$QTDIR != X ; then
     PATH=$PATH:$QTDIR/bin
     export PATH
  fi
  
  if test X$QMAKE = X ; then
     QMAKE=qmake
  fi
  
  AC_PATH_PROG([QMAKEPATH], $QMAKE, NOTFOUND)
  if test X$QMAKEPATH = XNOTFOUND ; then 
     AC_MSG_ERROR([Cannot find the qmake program. Maybe you need to install
  qt development files and tools and/or set the QTDIR environment variable?])
  fi
  QMAKE=$QMAKEPATH
  
  # MAC OS X: we don't support this (the native search tool is more than
  # good enough), but we make things work just enough so that the program can
  # be compiled and roughly installed (not as a .app, but to /usr/local),
  # basically to enable using a Macbook for development
  if test X$sys = XDarwin ; then
     # The default is xcode
     QMAKE="${QMAKE} -spec macx-g++"
  fi
  
  # Discriminate qt3/4. Qt3 qmake prints its version on stderr but we don't
  # depend on this. We try to detect the qt 4 version string instead.
  qmakevers="`${QMAKE} --version 2>&1`"
  #echo "qmake version: $qmakevers"
  v4=`expr "$qmakevers" : '.*Qt[ ][ ]*version[ ][ ]*4.*'`
  if test X$v4 = X0 ; then 
     AC_MSG_NOTICE([using qt version 3 user interface])
     QTGUI=qtgui
  else
    AC_MSG_NOTICE([using qt version 4 user interface])
    QTGUI=qt4gui
    AC_PATH_PROG([UIC3], [uic3], NOTFOUND)
    if test X$UIC3 = XNOTFOUND ; then 
      AC_MSG_ERROR([Cannot find the uic3 program. Maybe you need to add
      the qt3 compatibility libraries and tools to your qt4 installation ?])
    fi
  fi
  
  cd $QTGUI
  # We just want a .pro file: no problem with unsubstituted variables at 
  # this point.
  test -f uifrom3 && make -f uifrom3
  test -f recoll.pro && chmod +w recoll.pro
  cp recoll.pro.in recoll.pro
  #echo QMAKE ${QMAKE}
  ${QMAKE} recoll.pro
  if test $? != 0 ; then
     AC_MSG_ERROR([Cannot use qmake to generate a Makefile. Maybe you need to
  check the QTDIR and QMAKESPEC environment variables?])
  fi
  # is QTDIR set and do we actually need it ?
  if test X$QTDIR = X ; then
     QTDIRNEEDED=`grep INCPATH Makefile | grep = | grep QTDIR`
     if test "X$QTDIRNEEDED" != "X" ; then
         AC_MSG_ERROR([You need to set the QTDIR variable to point to the QT
  installation. If there is no default mkspecs, you should also set QMAKESPEC])
     fi
  fi
  cd ..

  AC_CONFIG_FILES($QTGUI/recoll.pro)
  ##################### End QT detection
fi


### X11: this is needed for the session monitoring code (in recollindex -m)
# We disable it if without-gui has been specified, but it could be separated
if test "$withQT" = "yes" ; then
  AC_PATH_XTRA
  X_LIBX11=-lX11
else
  X_LIBX11=""
fi
#echo X_CFLAGS "'$X_CFLAGS'" X_PRE_LIBS "'$X_PRE_LIBS'" X_LIBS \
#      "'$X_LIBS'" X_LIBX11 "'$X_LIBX11'" X_EXTRA_LIBS "'$X_EXTRA_LIBS'"


# We have to expand prefix in here, couldn't find a way to do it inside 
# the qt gui .pro file or Makefile. This just means that you can't change
# prefix at build time. It works at install time because we dont' use the
# qtgui Makefile
m_prefix=$prefix
test "X$m_prefix" = "XNONE" && m_prefix=/usr/local
m_datadir=${m_prefix}/share
QTRECOLL_DATADIR=${m_datadir}/recoll

AC_SUBST(X_CFLAGS)
AC_SUBST(X_PRE_LIBS)
AC_SUBST(X_LIBS)
AC_SUBST(X_LIBX11)
AC_SUBST(X_EXTRA_LIBS)
AC_SUBST(INCICONV)
AC_SUBST(LIBICONV)
AC_SUBST(LIBXAPIAN)
AC_SUBST(LIBFAM)
AC_SUBST(QMAKE)
AC_SUBST(QTGUI)
AC_SUBST(QTRECOLL_DATADIR)
AC_SUBST(XAPIANCXXFLAGS)
AC_SUBST(HAVE_MKDTEMP)
AC_SUBST(NOQTMAKE)
AC_SUBST(NOCMDLINE)

AC_CONFIG_FILES(mk/localdefs)
AC_CONFIG_FILES(recollinstall)
AC_CONFIG_FILES(Makefile)
AC_CONFIG_FILES(sampleconf/recoll.conf)

for d in bincimapmime index lib query
do 
    rm -f $d/alldeps.stamp
    cp -f /dev/null $d/alldeps
done

(cd lib;test -f Makefile && chmod +w Makefile;sh mkMake)

AC_OUTPUT
