#!/bin/sh

fatal()
{
    echo $*
    exit 1
}
usage()
{
    fatal make-recoll-dmg.sh
}

# Adjustable things
top=~/Recoll
# The possibly bogus version we have in paths (may be harcoded in the .pro)
# qcbuildloc=Desktop_Qt_5_15_2_clang_64bit
qcbuildloc=Qt_6_2_4_for_macOS

# qtversion=5.15.2
qtversion=6.2.4

#deploy=~/Qt/${qtversion}/macos/clang_64bit/macdeployqt
deploy=~/Qt/${qtversion}/macos/bin/macdeployqt

toprecoll=$top/recoll/src
appdir=$toprecoll/build-recoll-win-${qcbuildloc}-Release/recoll.app
rclindexdir=$toprecoll/windows/build-recollindex-${qcbuildloc}-Release
rclqdir=$toprecoll/windows/build-recollq-${qcbuildloc}-Release
bindir=$appdir/Contents/MacOS
datadir=$appdir/Contents/Resources

dmg=$appdir/../recoll.dmg

version=`cat $toprecoll/RECOLL-VERSION.txt`

test -d $appdir || fatal Must first have built recoll in $appdir

cp $rclindexdir/recollindex $bindir || exit 1
cp $rclqdir/recollq $bindir || exit 1

cp $top/antiword/antiword $bindir || exit 1
mkdir -p $datadir/antiword || exit 1
cp $top/antiword/Resources/* $datadir/antiword || exit 1


rm -f $dmg ~/Documents/recoll-$version-*.dmg

$deploy $appdir -dmg || exit 1


hash=`(cd $top/recoll;git log -n 1  | head -1  | awk '{print $2}' |cut -b 1-8)`
dte=`date +%Y%m%d`
mv $dmg ~/Documents/recoll-$version-$dte-$hash.dmg || exit 1
ls -l ~/Documents/recoll-$version-*.dmg

