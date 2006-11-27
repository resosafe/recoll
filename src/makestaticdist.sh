#!/bin/sh
#set -x
# A shell-script to make a recoll static binary distribution:

TAR=tar
 
targetdir=${targetdir-/tmp}

if test ! -d qtgui;then
    echo "Should be executed in the master recoll directory"
    exit 1
fi

version=`cat VERSION`
sys=`uname -s`
sysrel=`uname -r`

topdirsimple=recoll-${version}-${sys}-${sysrel}
topdir=$targetdir/$topdirsimple

tarfile=$targetdir/recoll-${version}-${sys}-${sysrel}.tgz

if test ! -d $topdir ; then
    mkdir $topdir || exit 1
else 
    echo "Removing everything under $topdir Ok ? (y/n)"
    read rep 
    if test $rep = 'y';then
    	rm -rf $topdir/*
    fi
fi


rm -f index/recollindex qtgui/recoll
make static || exit 1
strip index/recollindex qtgui/recoll

files="COPYING README INSTALL VERSION Makefile recoll.desktop recollinstall
filters sampleconf doc/user doc/man
index/recollindex index/rclmon.sh qtgui/recoll qtgui/i18n/*.qm 
qtgui/mtpics/*.png recoll.png"

$TAR chf - $files  | (cd $topdir; $TAR xf -)

# Remove any install dependancy
chmod +w $topdir/Makefile
sed -e '/^install:/c\
install: ' < $topdir/Makefile > $topdir/toto && \
	 mv $topdir/toto $topdir/Makefile


(cd $targetdir ; \
    $TAR chf - $topdirsimple | \
    	gzip > $tarfile)

echo $tarfile created
