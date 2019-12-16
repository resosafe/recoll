#!/bin/sh
# Packages needed
# sudo apt-get install g++ gnupg dput lintian mini-dinstall yaclc bzr devscripts
# For the kio: (and kdesdk?)
# sudo apt-get install pkg-kde-tools  cdbs

PPA_KEYID=D38B9201

RCLVERS=1.26.3
SCOPEVERS=1.20.2.4
GSSPVERS=1.0.0
PPAVERS=1

# 
RCLSRC=/y/home/dockes/projets/fulltext/recoll/src
SCOPESRC=/y/home/dockes/projets/fulltext/unity-scope-recoll
GSSPSRC=/y/home/dockes/projets/fulltext/gssp-recoll
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
# No new releases for trusty either because of risk of kio compat (kio
# wont build)
# Active series:
# 16.04LTS xenial 2021-04
# 18.04LTS bionic 2023-04
# 19.04    disco  2020-01
# 19.10    eoan   2020-07
series="xenial bionic disco eoan"
series=

if test "X$series" != X ; then
    check_recoll_orig
    test -d recoll-${RCLVERS} || tar xzf recoll_${RCLVERS}.orig.tar.gz
fi

for series in $series ; do

  rm -rf recoll-${RCLVERS}/debian
  cp -rp ${debdir}/ recoll-${RCLVERS}/debian || exit 1

  if test -f $debdir/control-$series ; then
      cp -f -p $debdir/control-$series recoll-${RCLVERS}/debian/control
  else 
      cp -f -p $debdir/control recoll-${RCLVERS}/debian/control
  fi
  if test -f $debdir/rules-$series ; then
      cp -f -p $debdir/rules-$series recoll-${RCLVERS}/debian/rules
  else 
      cp -f -p $debdir/rules recoll-${RCLVERS}/debian/rules
  fi

  sed -e s/SERIES/${series}/g \
      -e s/PPAVERS/${PPAVERS}/g \
      < ${debdir}/changelog > recoll-${RCLVERS}/debian/changelog

  (cd recoll-${RCLVERS};debuild -k$PPA_KEYID -S -sa)  || break

  dput $PPANAME recoll_${RCLVERS}-1~ppa${PPAVERS}~${series}1_source.changes
done



### KIO. Does not build on trusty from recoll 1.23 because of the need
### for c++11
# Note: no new releases for lucid: no webkit. Or use old debianrclqt4 dir.
# No new releases for trusty either because of risk of kio compat (kio
# wont build)
# Active series:
# 16.04LTS xenial 2021-04
# 18.04LTS bionic 2023-04
# 19.04    disco  2020-01
# 19.10    eoan   2020-07
series="xenial bionic disco eoan"
#series=

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
        tar xzf ../recoll_${RCLVERS}.orig.tar.gz || exit 1
        mv recoll-${RCLVERS} ../$topdir || exit 1
        cd ..
    fi
fi
for svers in $series ; do

  rm -rf $topdir/debian
  cp -rp ${debdir}/ $topdir/debian || exit 1

  if test -f $debdir/control-$svers ; then
      cp -f -p $debdir/control-$svers $topdir/debian/control
  else 
      cp -f -p $debdir/control $topdir/debian/control
  fi

  sed -e s/SERIES/$svers/g \
      -e s/PPAVERS/${PPAVERS}/g \
          < ${debdir}/changelog > $topdir/debian/changelog ;
  (cd $topdir;debuild -k$PPA_KEYID -S -sa) || exit 1

  dput $PPANAME kio-recoll_${RCLVERS}-1~ppa${PPAVERS}~${svers}1_source.changes

done

### GSSP
# Note: no new releases for lucid: no webkit. Or use old debianrclqt4 dir.
# No new releases for trusty either because of risk of kio compat (kio
# wont build)
# Active series:
# 16.04LTS xenial 2021-04
# 18.04LTS bionic 2023-04
# 19.04    disco  2020-01
# 19.10    eoan   2020-07
series="xenial bionic disco eoan"
series=

debdir=debiangssp
if test ! -d ${debdir}/ ; then
    rm -f ${debdir}
    ln -s ${GSSPSRC}/debian $debdir
fi
topdir=gssp-recoll-${GSSPVERS}
dload=$RCLDOWNLOAD/downloads
if test "X$series" != X ; then
    if test ! -f gssp-recoll_${GSSPVERS}.orig.tar.gz ; then 
        if test -f gssp-recoll-${GSSPVERS}.tar.gz ; then
          mv gssp-recoll-${GSSPVERS}.tar.gz gssp-recoll_${GSSPVERS}.orig.tar.gz
        else
          if test -f $dload/gssp-recoll-${GSSPVERS}.tar.gz;then
                cp -p $dload/gssp-recoll-${GSSPVERS}.tar.gz \
                   gssp-recoll_${GSSPVERS}.orig.tar.gz || fatal copy
            else
                fatal "Can find neither " \
                      "gssp-recoll_${GSSPVERS}.orig.tar.gz nor " \
                      "$dload/gssp-recoll-${GSSPVERS}.tar.gz"
            fi
        fi
    fi
    test -d $topdir || tar xzf gssp-recoll_${GSSPVERS}.orig.tar.gz || exit 1
fi
for series in $series ; do

   rm -rf $topdir/debian
   cp -rp ${debdir}/ $topdir/debian || exit 1

  sed -e s/SERIES/$series/g \
      -e s/PPAVERS/${PPAVERS}/g \
          < ${debdir}/changelog > $topdir/debian/changelog ;

  (cd $topdir;debuild -k$PPA_KEYID -S -sa) || break

  dput $PPANAME \
      gssp-recoll_${GSSPVERS}-1~ppa${PPAVERS}~${series}1_source.changes

done


### Unity Scope
series="trusty xenial  zesty artful bionic"
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
    test -d $topdir ||  tar xzf unity-scope-recoll_${SCOPEVERS}.orig.tar.gz \
        || exit 1
fi
for series in $series ; do

   rm -rf $topdir/debian
   cp -rp ${debdir}/ $topdir/debian || exit 1

  sed -e s/SERIES/$series/g \
      -e s/PPAVERS/${PPAVERS}/g \
          < ${debdir}/changelog > $topdir/debian/changelog ;

  (cd $topdir;debuild -k$PPA_KEYID -S -sa) || break

  dput $PPANAME \
      unity-scope-recoll_${SCOPEVERS}-1~ppa${PPAVERS}~${series}1_source.changes

done
