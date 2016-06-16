#!/bin/sh
# Packages needed
# sudo apt-get install g++ gnupg dput lintian mini-dinstall yaclc bzr devscripts
# For the kio: (and kdesdk?)
# sudo apt-get install pkg-kde-tools  cdbs

RCLVERS=1.22.2
LENSVERS=1.19.10.3543
SCOPEVERS=1.20.2.4
PPAVERS=1

# 
RCLSRC=/y/home/dockes/projets/fulltext/recoll/src
SCOPESRC=/y/home/dockes/projets/fulltext/unity-scope-recoll
RCLDOWNLOAD=/y/home/dockes/projets/lesbonscomptes/recoll

case $RCLVERS in
    [23]*) PPANAME=recollexp-ppa;;
    1.14*) PPANAME=recoll-ppa;;
    *)     PPANAME=recoll15-ppa;;
esac
#PPANAME=recollexp-ppa
echo "PPA: $PPANAME. Type CR if Ok, else ^C"
read rep

fatal()
{
    echo $*; exit 1
}

check_recoll_orig()
{
    if test ! -f recoll_${RCLVERS}.orig.tar.gz ; then 
        cp -p $RCLDOWNLOAD/recoll-${RCLVERS}.tar.gz \
            recoll_${RCLVERS}.orig.tar.gz || \
            fatal "Can find neither recoll_${RCLVERS}.orig.tar.gz nor " \
            "recoll-${RCLVERS}.tar.gz"
    fi
}

# Note: recoll 1.22+ builds on precise fail. precise stays at 1.21

####### QT
debdir=debian
# Note: no new releases for lucid: no webkit. Or use old debianrclqt4 dir.
series="trusty vivid wily xenial"
#series=trusty

if test "X$series" != X ; then
    check_recoll_orig
    test -d recoll-${RCLVERS} || tar xvzf recoll_${RCLVERS}.orig.tar.gz
fi

for series in $series ; do

  rm -rf recoll-${RCLVERS}/debian
  cp -rp ${debdir}/ recoll-${RCLVERS}/debian || exit 1

  if test -f $debdir/control-$series ; then
      cp -f -p $debdir/control-$series recoll-${RCLVERS}/debian/control
  else 
      cp -f -p $debdir/control recoll-${RCLVERS}/debian/control
  fi

  sed -e s/SERIES/${series}/g \
      -e s/PPAVERS/${PPAVERS}/g \
      < ${debdir}/changelog > recoll-${RCLVERS}/debian/changelog

  (cd recoll-${RCLVERS};debuild -S -sa)  || break

  dput $PPANAME recoll_${RCLVERS}-1~ppa${PPAVERS}~${series}1_source.changes
done

### KIO
series="trusty vivid wily xenial"
series=

debdir=debiankio
topdir=kio-recoll-${RCLVERS}
if test "X$series" != X ; then
    check_recoll_orig
    if test ! -f kio-recoll_${RCLVERS}.orig.tar.gz ; then
        cp -p recoll_${RCLVERS}.orig.tar.gz \
            kio-recoll_${RCLVERS}.orig.tar.gz || exit 1
    fi
    if test ! -d $topdir ; then 
        mkdir temp
        cd temp
        tar xvzf ../recoll_${RCLVERS}.orig.tar.gz || exit 1
        mv recoll-${RCLVERS} ../$topdir || exit 1
        cd ..
    fi
fi
for series in $series ; do

  rm -rf $topdir/debian
  cp -rp ${debdir}/ $topdir/debian || exit 1

  sed -e s/SERIES/$series/g \
      -e s/PPAVERS/${PPAVERS}/g \
          < ${debdir}/changelog > $topdir/debian/changelog ;

  (cd $topdir;debuild -S -sa) || exit 1

  dput $PPANAME kio-recoll_${RCLVERS}-0~ppa${PPAVERS}~${series}1_source.changes

done

### Unity Lens
series="precise"
series=

debdir=debianunitylens
topdir=recoll-lens-${LENSVERS}
if test "X$series" != X ; then
    if test ! -f recoll-lens_${LENSVERS}.orig.tar.gz ; then 
        if test -f recoll-lens-${LENSVERS}.tar.gz ; then
            mv recoll-lens-${LENSVERS}.tar.gz \
                recoll-lens_${LENSVERS}.orig.tar.gz
        else
            fatal "Can find neither recoll-lens_${LENSVERS}.orig.tar.gz nor " \
                "recoll-lens-${LENSVERS}.tar.gz"
        fi
    fi
    test -d $topdir ||  tar xvzf recoll-lens_${LENSVERS}.orig.tar.gz || exit 1
fi

for series in $series ; do

   rm -rf $topdir/debian
   cp -rp ${debdir}/ $topdir/debian || exit 1

  sed -e s/SERIES/$series/g \
      -e s/PPAVERS/${PPAVERS}/g \
          < ${debdir}/changelog > $topdir/debian/changelog ;

  (cd $topdir;debuild -S -sa) || break

  dput $PPANAME \
      recoll-lens_${LENSVERS}-1~ppa${PPAVERS}~${series}1_source.changes

done

### Unity Scope
series="trusty vivid wily xenial"
series=

debdir=debianunityscope
if test ! -d ${debdir}/ ; then
    rm -f ${debdir}
    ln -s ${SCOPESRC}/debian $debdir
fi
topdir=unity-scope-recoll-${SCOPEVERS}
if test "X$series" != X ; then
    if test ! -f unity-scope-recoll_${SCOPEVERS}.orig.tar.gz ; then 
        if test -f unity-scope-recoll-${SCOPEVERS}.tar.gz ; then
            mv unity-scope-recoll-${SCOPEVERS}.tar.gz \
                unity-scope-recoll_${SCOPEVERS}.orig.tar.gz
        else
            if test -f $RCLDOWNLOAD/unity-scope-recoll-${SCOPEVERS}.tar.gz;then
                cp -p $RCLDOWNLOAD/unity-scope-recoll-${SCOPEVERS}.tar.gz \
                   unity-scope-recoll_${SCOPEVERS}.orig.tar.gz || fatal copy
            else
                fatal "Can find neither " \
                      "unity-scope-recoll_${SCOPEVERS}.orig.tar.gz nor " \
                      "$RCLDOWNLOAD/unity-scope-recoll-${SCOPEVERS}.tar.gz"
            fi
        fi
    fi
    test -d $topdir ||  tar xvzf unity-scope-recoll_${SCOPEVERS}.orig.tar.gz \
        || exit 1
fi
for series in $series ; do

   rm -rf $topdir/debian
   cp -rp ${debdir}/ $topdir/debian || exit 1

  sed -e s/SERIES/$series/g \
      -e s/PPAVERS/${PPAVERS}/g \
          < ${debdir}/changelog > $topdir/debian/changelog ;

  (cd $topdir;debuild -S -sa) || break

  dput $PPANAME \
      unity-scope-recoll_${SCOPEVERS}-1~ppa${PPAVERS}~${series}1_source.changes

done
