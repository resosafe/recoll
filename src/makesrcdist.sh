#!/bin/sh
# @(#$Id: makesrcdist.sh,v 1.11.2.1 2006-11-28 07:19:37 dockes Exp $  (C) 2005 J.F.Dockes
# A shell-script to make a recoll source distribution

#set -x

TAR=/usr/bin/tar
 
targetdir=${targetdir-/tmp}
dotag=${dotag-yes}

if test ! -d qtgui;then
    echo "Should be executed in the master recoll directory"
    exit 1
fi

version=`cat VERSION`
versionforcvs=`echo $version | sed -e 's/\./_/g'`

topdir=$targetdir/recoll-$version
if test ! -d $topdir ; then
    mkdir $topdir || exit 1
else 
    echo "Removing everything under $topdir Ok ? (y/n)"
    read rep 
    if test $rep = 'y';then
    	rm -rf $topdir/*
    fi
fi

################################### Documentation
###### Html doc
RECOLLDOC=${RECOLLDOC:=doc/user}
(cd $RECOLLDOC;make) || exit 1

###### Text Doc
chmod +w README INSTALL
cat <<EOF > README

More documentation can be found in the doc/ directory or at http://www.recoll.org


EOF
cat <<EOF > INSTALL

More documentation can be found in the doc/ directory or at http://www.recoll.org


EOF

echo "Dumping html documentation to text files"
links -dump ${RECOLLDOC}/usermanual.html >> README

links -dump ${RECOLLDOC}/rcl.install.html >> INSTALL
links -dump ${RECOLLDOC}/rcl.install.external.html >> INSTALL
links -dump ${RECOLLDOC}/rcl.install.building.html >> INSTALL
links -dump ${RECOLLDOC}/rcl.install.config.html >> INSTALL

cvs commit -m '' README INSTALL

# Clean up this dir and copy the dist-specific files 
make clean
yes | clean.O
rm -f lib/*.dep
$TAR chfX - excludefile .  | (cd $topdir;$TAR xf -)

# Fix the single/multiple page link in the header (we dont deliver the
# multi-page version and the file name is wrong anyway
sed -e '/\.\/index\.html/d' -e '/\.\/book\.html/d' \
    < $topdir/doc/user/usermanual.html > $topdir/doc/user/u1.html
diff $topdir/doc/user/u1.html $topdir/doc/user/usermanual.html
mv -f $topdir/doc/user/u1.html $topdir/doc/user/usermanual.html

# We tag .. as there is the 'packaging/' directory in there
CVSTAG="RECOLL_$versionforcvs"
[ $dotag = "yes" ] && (cd ..;cvs tag -F $CVSTAG .)

out=recoll-$version.tar.gz
(cd $targetdir ; \
    $TAR chf - recoll-$version | \
    	gzip > $out)
echo "$targetdir/$out created"
