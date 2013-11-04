#!/bin/sh
# Packages needed
# sudo apt-get install g++ gnupg dput lintian mini-dinstall yaclc bzr devscripts
# For the kio: (and kdesdk?)
# sudo apt-get install pkg-kde-tools  cdbs

RCLVERS=1.19.8
LENSVERS=1.19.2.3328
PPAVERS=1

case $RCLVERS in
    [23]*) PPANAME=recollexp-ppa;;
    1.14*) PPANAME=recoll-ppa;;
    *)     PPANAME=recoll15-ppa;;
esac
#PPANAME=recollexp-ppa

echo "PPA: $PPANAME. Type CR if Ok, else ^C"
read rep

####### QT4
debdir=debianrclqt4
series4="lucid oneiric precise quantal raring saucy"
series4="lucid"

for series in $series4 ; do
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
seriesk="lucid oneiric precise quantal raring"
seriesk="lucid"

debdir=debiankio
topdir=kio-recoll-${RCLVERS}
test -d kio-recoll_${RCLVERS}.orig.tar.gz || \
    cp -p recoll_${RCLVERS}.orig.tar.gz kio-recoll_${RCLVERS}.orig.tar.gz || \
    exit 1
test -d $topdir || cp -rp recoll-${RCLVERS} $topdir

for series in $seriesk ; do

  rm -rf $topdir/debian
  cp -rp ${debdir}/ $topdir/debian || exit 1

  sed -e s/SERIES/$series/g \
      -e s/PPAVERS/${PPAVERS}/g \
          < ${debdir}/changelog > $topdir/debian/changelog ;

  (cd $topdir;debuild -S -sa) || break

  dput $PPANAME kio-recoll_${RCLVERS}-0~ppa${PPAVERS}~${series}1_source.changes

done

### Unity Lens
seriesl="oneiric precise quantal raring"
seriesl=
debdir=debianunitylens
topdir=recoll-lens-${LENSVERS}

for series in $seriesl ; do

   rm -rf $topdir/debian
   cp -rp ${debdir}/ $topdir/debian || exit 1

  sed -e s/SERIES/$series/g \
      -e s/PPAVERS/${PPAVERS}/g \
          < ${debdir}/changelog > $topdir/debian/changelog ;

  (cd $topdir;debuild -S -sa) || break

  dput $PPANAME recoll-lens_${LENSVERS}-1~ppa${PPAVERS}~${series}1_source.changes

done
