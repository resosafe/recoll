#!/bin/sh

fatal()
{
    echo $*
    exit 1
}
Usage()
{
    fatal mkinstdir.sh targetdir
}

test $# -eq 1 || Usage

DESTDIR=$1

test -d $DESTDIR || mkdir $DESTDIR || fatal cant create $DESTDIR

# Script to make a prototype recoll install directory from locally compiled
# software. *** Needs msys or cygwin ***

################################
# Local values (to be adjusted)

# Recoll src tree
RCL=c:/recoll/src/

ReleaseBuild=y

# Note: unrtf not under recolldeps because it's a clone from the
# original mercurial repository 
UNRTF=c:/unrtf
# antiword is under recolldeps: it's not really maintained any more
# and has no public repository
ANTIWORD=c:/recolldeps/antiword
PYXSLT=C:/recolldeps/pyxslt
PYEXIV2=C:/recolldeps/pyexiv2
#LIBXAPIAN=c:/temp/xapian-core-1.2.21/.libs/libxapian-22.dll
LIBXAPIAN=c:/temp/xapian-core-1.4.5/.libs/libxapian-30.dll
MUTAGEN=C:/temp/mutagen-1.32/
EPUB=C:/temp/epub-0.5.2
FUTURE=C:/temp/python2-future
ZLIB=c:/temp/zlib-1.2.8
POPPLER=c:/temp/poppler-0.36/
LIBWPD=c:/temp/libwpd/libwpd-0.10.0/
LIBREVENGE=c:/temp/libwpd/librevenge-0.0.1.jfd/
CHM=c:/recolldeps/pychm

# Where to find libgcc_s_dw2-1.dll for progs which need it copied
gccpath=`which gcc`
MINGWBIN=`dirname $gccpath`

# Where to copy the Qt Dlls from:
QTBIN=C:/Qt/Qt5.8.0/5.8/mingw53_32/bin

# Qt arch
QTA=Desktop_Qt_5_8_0_MinGW_32bit

RCLW=$RCL/windows/

if test X$ReleaseBuild = X'y'; then
    qtsdir=release
else
    qtsdir=debug
fi
LIBR=$RCLW/build-librecoll-${QTA}-${qtsdir}/${qtsdir}/librecoll.dll
GUIBIN=$RCL/build-recoll-win-${QTA}-${qtsdir}/${qtsdir}/recoll.exe
RCLIDX=$RCLW/build-recollindex-${QTA}-${qtsdir}/${qtsdir}/recollindex.exe
RCLQ=$RCLW/build-recollq-${QTA}-${qtsdir}/${qtsdir}/recollq.exe
RCLS=$RCLW/build-rclstartw-${QTA}-${qtsdir}/${qtsdir}/rclstartw.exe


# Needed for a VS build (which we did not ever complete because of
# missing Qt VS2015 support).
#CONFIGURATION=Release
#PLATFORM=Win32

################
# Script:

FILTERS=$DESTDIR/Share/filters

# checkcopy. 
chkcp()
{
    echo "Copying $@"
    cp $@ || fatal cp $@ failed
}

# Note: can't build static recoll as there is no static qtwebkit (ref: 5.5.0)
copyqt()
{
    cd $DESTDIR
    PATH=$QTBIN:$PATH
    export PATH
    $QTBIN/windeployqt recoll.exe
    # Apparently because the webkit part was grafted "by hand" on the
    # Qt set, we need to copy some dll explicitely
    addlibs="Qt5Core.dll Qt5Multimedia.dll \
Qt5MultimediaWidgets.dll Qt5Network.dll Qt5OpenGL.dll \
Qt5Positioning.dll Qt5PrintSupport.dll Qt5Sensors.dll \
Qt5Sql.dll icudt57.dll \
icuin57.dll icuuc57.dll libQt5WebKit.dll \
libQt5WebKitWidgets.dll \
libxml2-2.dll libxslt-1.dll"
   for i in $addlibs;do
       chkcp $QTBIN/$i $DESTDIR
   done
    chkcp $QTBIN/libwinpthread-1.dll $DESTDIR
    chkcp $QTBIN/libstdc++-6.dll $DESTDIR
}

copyxapian()
{
    chkcp $LIBXAPIAN $DESTDIR
}

copyzlib()
{
    chkcp $ZLIB/zlib1.dll $DESTDIR
}

copyrecoll()
{
#    bindir=$RCL/windows/$PLATFORM/$CONFIGURATION/
#    chkcp  $bindir/recollindex.exe         $DESTDIR
#    chkcp  $bindir/recollq.exe             $DESTDIR
#    chkcp  $bindir/pthreadVC2.dll          $DESTDIR
    chkcp $LIBR $DESTDIR 
    chkcp $GUIBIN $DESTDIR
    chkcp $RCLIDX $DESTDIR
    chkcp $RCLQ $DESTDIR 
    chkcp $RCLS $DESTDIR 

    chkcp $RCL/COPYING                  $DESTDIR/COPYING.txt
    chkcp $RCL/doc/user/usermanual.html $DESTDIR/Share/doc
    chkcp $RCL/doc/user/docbook-xsl.css $DESTDIR/Share/doc
    mkdir -p $DESTDIR/Share/doc/webhelp
    cp -rp $RCL/doc/user/webhelp/docs/* $DESTDIR/Share/doc/webhelp
    chkcp $RCL/sampleconf/fields        $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/fragbuts.xml  $DESTDIR/Share/examples
    chkcp $RCL/windows/mimeconf         $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/mimemap       $DESTDIR/Share/examples
    chkcp $RCL/windows/mimeview         $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/recoll.conf   $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/recoll.qss    $DESTDIR/Share/examples

    chkcp $RCL/python/recoll/recoll/rclconfig.py $FILTERS
    chkcp $RCL/filters/*       $FILTERS 
    chkcp $RCL/qtgui/mtpics/*  $DESTDIR/Share/images
    chkcp $RCL/qtgui/i18n/*.qm $DESTDIR/Share/translations
}

copyantiword()
{
    # MS VS
    #bindir=$ANTIWORD/Win32-only/$PLATFORM/$CONFIGURATION
    # MINGW
    bindir=$ANTIWORD/

    test -d $Filters/Resources || mkdir -p $FILTERS/Resources || exit 1
    chkcp  $bindir/antiword.exe            $FILTERS
    chkcp  $ANTIWORD/Resources/*           $FILTERS/Resources
}

copyunrtf()
{
#    bindir=$UNRTF/Windows/$PLATFORM/$CONFIGURATION
     bindir=$UNRTF/Windows/

    test -d $FILTERS/Share || mkdir -p $FILTERS/Share || exit 1
    chkcp  $bindir/unrtf.exe               $FILTERS
    chkcp  $UNRTF/outputs/*.conf           $FILTERS/Share
    chkcp  $UNRTF/outputs/SYMBOL.charmap   $FILTERS/Share
    # libiconv2 is not present in qt, get it from mingw direct. is C, should
    # be compatible
    chkcp c:/MinGW/bin/libiconv-2.dll $FILTERS
}

copymutagen()
{
    cp -rp $MUTAGEN/build/lib/mutagen $FILTERS
    # chkcp to check that mutagen is where we think it is
    chkcp $MUTAGEN/build/lib/mutagen/mp3.py $FILTERS/mutagen
}

copyepub()
{
    cp -rp $EPUB/build/lib/epub $FILTERS
    # chkcp to check that epub is where we think it is
    chkcp $EPUB/build/lib/epub/opf.py $FILTERS/epub
}
copyfuture()
{
    cp -rp $FUTURE/future $FILTERS/
    cp -rp $FUTURE/builtins $FILTERS/
    # chkcp to check that things are where we think they are
    chkcp $FUTURE/future/builtins/newsuper.pyc $FILTERS/future/builtins
}

copypyexiv2()
{
    cp -rp $PYEXIV2/pyexiv2 $FILTERS
    chkcp $PYEXIV2/libexiv2python.pyd $FILTERS/
}

copyxslt()
{
    chkcp $PYXSLT/libxslt.py $FILTERS/
    cp -rp $PYXSLT/* $FILTERS
}

copypoppler()
{
    test -d $FILTERS/poppler || mkdir $FILTERS/poppler || \
        fatal cant create poppler dir
    for f in pdftotext.exe libpoppler.dll freetype6.dll jpeg62.dll \
             libpng16-16.dll zlib1.dll libtiff3.dll \
             libgcc_s_dw2-1.dll libstdc++-6.dll; do
        chkcp $POPPLER/bin/$f $FILTERS/poppler
    done
}

copywpd()
{
    DEST=$FILTERS/wpd
    test -d $DEST || mkdir $DEST || fatal cant create poppler dir $DEST

    for f in librevenge-0.0.dll librevenge-generators-0.0.dll \
             librevenge-stream-0.0.dll; do
        chkcp $LIBREVENGE/src/lib/.libs/$f $DEST
    done
    chkcp $LIBWPD/src/lib/.libs/libwpd-0.10.dll $DEST
    chkcp $LIBWPD/src/conv/html/.libs/wpd2html.exe $DEST
    chkcp $MINGWBIN/libgcc_s_dw2-1.dll $DEST
    chkcp $MINGWBIN/libstdc++-6.dll $DEST
    chkcp $ZLIB/zlib1.dll $DEST
    chkcp $QTBIN/libwinpthread-1.dll $DEST
}

copychm()
{
    DEST=$FILTERS
    cp -rp $CHM/chm $DEST || fatal "can't copy pychm"
}

for d in doc examples filters images translations; do
    test -d $DESTDIR/Share/$d || mkdir -p $DESTDIR/Share/$d || \
        fatal mkdir $d failed
done

# copyrecoll must stay before copyqt so that windeployqt can do its thing
copyrecoll
copyqt
copyxapian
copyzlib
copypoppler
copyantiword
copyunrtf
copyxslt
copyfuture
copymutagen
copyepub
copypyexiv2
copywpd
copychm
