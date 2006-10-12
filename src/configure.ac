AC_INIT(Recoll, 1.5)
AC_PREREQ(2.53)
AC_CONFIG_SRCDIR(index/recollindex.cpp)

sys=`uname`
if test -f mk/$sys ; then
    (cd mk; test -f sysconf || ln -s $sys sysconf)
else
   cat <<EOF
    No system configuration file found in mk/ for '$sys'. Maybe you
    could try to write one, starting from one of the existing files, they
    are really simple.
EOF
   exit 1
fi

##### Look for iconv. We first look for libiconv in /usr/local/lib:/usr/lib
##    then in libc (Linux, solaris)
AC_LANG(C)
LIBICONV=""
S_LDFLAGS=$LDFLAGS
dir=/usr/local/lib
LDFLAGS="$S_LDFLAGS -L$dir"
unset ac_cv_lib_iconv_iconv_open
AC_CHECK_LIB(iconv, iconv_open, LIBICONV="-L$dir -liconv";INCICONV=-I/usr/local/include)
if test A"$LIBICONV" = A ; then
  dir=/usr/lib
  LDFLAGS="$S_LDFLAGS -L$dir"
  unset ac_cv_lib_iconv_iconv_open
  AC_CHECK_LIB(iconv, iconv_open, LIBICONV="-L$dir -liconv";INCICONV=-I/usr/include)
  if test A"$LIBICONV" = A; then
    dir=/usr/lib
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
# Note about qt4: if --enable-qt4 (enables the qt4gui directory), and the 
# QTDIR variable are inconsistent, too bad.
#
AC_ARG_ENABLE(qt4,
    AC_HELP_STRING([--enable-qt4],
        [Use qt version 4]),
        rcl_cv_qt4=$enableval, rcl_cv_qt4=no)
AC_MSG_NOTICE(rcl_cv_qt4 $rcl_cv_qt4)
if test X$rcl_cv_qt4 = Xyes ; then 
   QTGUI=qt4gui
else
   QTGUI=qtgui
fi

if test X$QTDIR != X ; then
   PATH=$QTDIR/bin:$PATH
   export PATH
fi

AC_PATH_PROG([QMAKE], [qmake], NOTFOUND)
if test X$QMAKE = XNOTFOUND ; then 
   AC_MSG_ERROR([Cannot find the qmake program. Maybe you need to install
it and/or set the QTDIR environment variable?])
fi

cd $QTGUI
# We just want a .pro file: no problem with unsubstituted variables at 
# this point.
test -f recoll.pro && chmod +w recoll.pro
cp recoll.pro.in recoll.pro
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

##################### End QT detection

# We have to expand prefix in here, couldn't find a way to do it inside 
# the qt gui .pro file or Makefile. This just means that you can't change
# prefix at build time. It works at install time because we dont' use the
# qtgui Makefile
m_prefix=$prefix
test "X$m_prefix" = "XNONE" && m_prefix=/usr/local
m_datadir=${m_prefix}/share
QTRECOLL_DATADIR=${m_datadir}/recoll

AC_SUBST(LIBXAPIAN)
AC_SUBST(XAPIANCXXFLAGS)
AC_SUBST(LIBICONV)
AC_SUBST(INCICONV)
AC_SUBST(QTRECOLL_DATADIR)
AC_SUBST(QMAKE)
AC_SUBST(QTGUI)

AC_CONFIG_FILES(mk/localdefs)
AC_CONFIG_FILES(recollinstall)
AC_CONFIG_FILES(Makefile)
AC_CONFIG_FILES(sampleconf/recoll.conf)
AC_CONFIG_FILES($QTGUI/recoll.pro)

for d in bincimapmime index lib query
do 
    rm -f $d/alldeps.stamp
    cp -f /dev/null $d/alldeps
done

(cd lib;test -f Makefile && chmod +w Makefile;sh mkMake)

AC_OUTPUT
chmod a+x recollinstall
