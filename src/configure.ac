AC_INIT(Recoll, 1.0)
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
XAPIANCXXFLAGS=`$XAPIAN_CONFIG --cxxflags`

#echo XAPIAN_CONFIG $XAPIAN_CONFIG 
#echo LIBXAPIAN $LIBXAPIAN
#echo XAPIANCXXFLAGS $XAPIANCXXFLAGS

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

if test X$QTDIR != X ; then
   PATH=$QTDIR/bin:$PATH
   export PATH
fi

AC_PATH_PROG([QMAKE], [qmake], NOTFOUND)
if test X$QMAKE = XNOTFOUND ; then 
   AC_MSG_ERROR([Cannot find the qmake program. Maybe you need to install
it and/or set the QTDIR environment variable?])
fi

cd qtgui
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

AC_SUBST(LIBXAPIAN)
AC_SUBST(XAPIANCXXFLAGS)
AC_SUBST(LIBICONV)
AC_SUBST(INCICONV)

AC_CONFIG_FILES(mk/localdefs)
AC_CONFIG_FILES(recollinstall)
AC_CONFIG_FILES(sampleconf/recoll.conf)

for d in bincimapmime index lib query
do 
    rm -f $d/alldeps.stamp
    cp -f /dev/null $d/alldeps
done

AC_OUTPUT
chmod a+x recollinstall
