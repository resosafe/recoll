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

BUILD=MSVC
#BUILD=MINGW
WEB=WEBKIT
#WEB=WEBENGINE

if test $BUILD = MSVC ; then
    # Recoll src tree
    RCL=/c/users/bill/documents/recoll/src/
    PYRECOLL=${RCL}/python/recoll/
    # Recoll dependancies
    RCLDEPS=/c/users/bill/documents/recolldeps/
    LIBXML=${RCLDEPS}/msvc/libxml2/libxml2-2.9.4+dfsg1/win32/bin.msvc/libxml2.dll
    LIBXSLT=${RCLDEPS}/msvc/libxslt/libxslt-1.1.29/win32/bin.msvc/libxslt.dll
    ZLIB=${RCLDEPS}/msvc/zlib-1.2.11
    # Qt
    QTA=Desktop_Qt_5_14_2_MSVC2017_32bit-Release/release
    QTBIN=C:/Qt/5.14.2/msvc2017/bin
    MINGWBIN=C:/Qt/Tools/mingw730_32/bin/
else
    # Recoll src tree
    RCL=/c/recoll/src/
    # Recoll dependancies
    RCLDEPS=/c/recolldeps/
    QTA=Desktop_Qt_5_8_0_MinGW_32bit-Release/release/
    LIBXAPIAN=${RCLDEPS}/mingw/xapian-core-1.4.11/.libs/libxapian-30.dll
    ZLIB=${RCLDEPS}/mingw/zlib-1.2.8
    QTGCCBIN=C:/qt/Qt5.8.0/Tools/mingw530_32/bin/
    QTBIN=C:/Qt/Qt5.8.0/5.8/mingw53_32/bin/
    MINGWBIN=$QTBIN
    PATH=$MINGWBIN:$QTGCCBIN:$PATH
    export PATH
fi

# We use the mingw-compiled aspell program in both cases, it's only executed as a command
ASPELL=${RCLDEPS}/mingw/aspell-0.60.7/aspell-installed

# Where to find libgcc_s_dw2-1.dll et all for progs compiled with
# c:/MinGW (as opposed to the mingw bundled with qt). This is the same
# for either a msvc or mingw build of recoll itself.
#MINGWBIN=C:/MinGW/bin

RCLW=$RCL/windows/
# Only used for mingw, the msvc one is static
LIBR=$RCLW/build-librecoll-${QTA}/${qtsdir}/librecoll.dll
GUIBIN=$RCL/build-recoll-win-${QTA}/${qtsdir}/recoll.exe
RCLIDX=$RCLW/build-recollindex-${QTA}/${qtsdir}/recollindex.exe
RCLQ=$RCLW/build-recollq-${QTA}/${qtsdir}/recollq.exe
RCLS=$RCLW/build-rclstartw-${QTA}/${qtsdir}/rclstartw.exe
XAPC=$RCLW/build-xapian-check-${QTA}/xapian-check.exe
#PYTHON=${RCLDEPS}py-python3
PYTHON=${RCLDEPS}python-3.7.9-embed-win32
UNRTF=${RCLDEPS}unrtf
ANTIWORD=${RCLDEPS}antiword
PYXSLT=${RCLDEPS}pyxslt
PYEXIV2=${RCLDEPS}pyexiv2
MUTAGEN=${RCLDEPS}mutagen-1.32/
EPUB=${RCLDEPS}epub-0.5.2
FUTURE=${RCLDEPS}python2-future
POPPLER=${RCLDEPS}poppler-0.68.0/
LIBWPD=${RCLDEPS}libwpd/libwpd-0.10.0/
LIBREVENGE=${RCLDEPS}libwpd/librevenge-0.0.1.jfd/
CHM=${RCLDEPS}pychm
MISC=${RCLDEPS}misc
LIBPFF=${RCLDEPS}pffinstall

################
# Script:
FILTERS=$DESTDIR/Share/filters

fatal()
{
    echo $*
    exit 1
}

# checkcopy. 
chkcp()
{
    echo "Copying $@"
    cp $@ || fatal cp $@ failed
}

# Note: can't build static recoll as there is no static qtwebkit (ref:
# 5.5.0)
copyqt()
{
    cd $DESTDIR
    PATH=$QTBIN:$PATH
    export PATH
    $QTBIN/windeployqt recoll.exe

    if test $BUILD = MINGW;then
        # Apparently because the webkit part was grafted "by hand" on
	# the Qt set, we need to copy some dll explicitly
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
    elif test $WEB = WEBKIT ; then
        addlibs="icudt65.dll icuin65.dll icuuc65.dll libxml2.dll libxslt.dll \
          Qt5WebKit.dll Qt5WebKitWidgets.dll"
        for i in $addlibs;do
            chkcp $QTBIN/$i $DESTDIR
        done
    fi
}

# Note that pychm and pyhwp are pre-copied into the py-python3 python
# distribution directory. The latter also needs olefile and six (also
# copied to the python tree
copypython()
{
    set -x
    mkdir -p ${DESTDIR}/Share/filters/python
    rsync -av $PYTHON/ ${DESTDIR}/Share/filters/python || exit 1
    chkcp $PYTHON/python.exe $DESTDIR/Share/filters/python/python.exe
    chkcp $MISC/hwp5html $FILTERS
}

copyrecoll()
{
    
    chkcp $GUIBIN $DESTDIR
    chkcp $RCLIDX $DESTDIR
    chkcp $RCLQ $DESTDIR 
    chkcp $RCLS $DESTDIR 
    chkcp $ZLIB/zlib1.dll $DESTDIR
    if test $BUILD = MINGW;then
        chkcp $LIBXAPIAN $DESTDIR
	chkcp $LIBR $DESTDIR 
        chkcp $MINGWBIN/libgcc_s_dw2-1.dll $DESTDIR
    else
        chkcp $XAPC $DESTDIR
	chkcp $LIBXML $DESTDIR
	chkcp $LIBXSLT $DESTDIR
    fi
    chkcp $RCL/COPYING                  $DESTDIR/COPYING.txt
    chkcp $RCL/doc/user/usermanual.html $DESTDIR/Share/doc
    chkcp $RCL/doc/user/docbook-xsl.css $DESTDIR/Share/doc
    mkdir -p $DESTDIR/Share/doc/webhelp
    rsync -av $RCL/doc/user/webhelp/docs/* $DESTDIR/Share/doc/webhelp || exit 1
    chkcp $RCL/sampleconf/fields          $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/fragment-buttons.xml  $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/mimeconf        $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/mimeview        $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/mimemap         $DESTDIR/Share/examples
    chkcp $RCL/windows/mimeconf           $DESTDIR/Share/examples/windows
    chkcp $RCL/windows/mimeview           $DESTDIR/Share/examples/windows
    chkcp $RCL/sampleconf/recoll.conf     $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/recoll.qss      $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/recoll-common.qss $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/recoll-dark.qss $DESTDIR/Share/examples
    chkcp $RCL/sampleconf/recoll-dark.css $DESTDIR/Share/examples

    chkcp $RCL/python/recoll/recoll/rclconfig.py $FILTERS
    chkcp $RCL/python/recoll/recoll/conftree.py $FILTERS
    rm -f $FILTERS/rclimg*
    chkcp $RCL/filters/*       $FILTERS
    rm -f $FILTERS/rclimg $FILTERS/rclimg.py
    chkcp $RCLDEPS/rclimg/rclimg.exe $FILTERS
    chkcp $RCL/qtgui/mtpics/*  $DESTDIR/Share/images
    chkcp $RCL/qtgui/i18n/*.qm $DESTDIR/Share/translations
    chkcp $RCL/desktop/recoll.ico $DESTDIR/Share
}

copyantiword()
{
    # MINGW
    bindir=$ANTIWORD/
    test -d $Filters/Resources || mkdir -p $FILTERS/Resources || exit 1
    chkcp  $bindir/antiword.exe            $FILTERS
    rsync -av  $ANTIWORD/Resources/*       $FILTERS/Resources || exit 1
}

copyunrtf()
{
     bindir=$UNRTF/Windows/

    test -d $FILTERS/Share || mkdir -p $FILTERS/Share || exit 1
    chkcp  $bindir/unrtf.exe               $FILTERS
    chkcp  $UNRTF/outputs/*.conf           $FILTERS/Share
    chkcp  $UNRTF/outputs/SYMBOL.charmap   $FILTERS/Share
    # libiconv-2 originally comes from mingw
    chkcp $MISC/libiconv-2.dll $FILTERS
}

copymutagen()
{
    cp -rp $MUTAGEN/build/lib/mutagen $FILTERS
    # chkcp to check that mutagen is where we think it is
    chkcp $MUTAGEN/build/lib/mutagen/mp3.py $FILTERS/mutagen
}

# Not used any more, the epub python code is bundled with recoll
copyepub()
{
    cp -rp $EPUB/build/lib/epub $FILTERS
    # chkcp to check that epub is where we think it is
    chkcp $EPUB/build/lib/epub/opf.py $FILTERS/epub
}

# We used to copy the future module to the filters dir, but it is now
# part of the origin Python tree in recolldeps. (2 dirs:
# site-packages/builtins, site-packages/future)
copyfuture()
{
    cp -rp $FUTURE/future $FILTERS/
    cp -rp $FUTURE/builtins $FILTERS/
    # chkcp to check that things are where we think they are
    chkcp $FUTURE/future/builtins/newsuper.pyc $FILTERS/future/builtins
}

# Replaced by mutagen
copypyexiv2()
{
    cp -rp $PYEXIV2/pyexiv2 $FILTERS
    chkcp $PYEXIV2/libexiv2python.pyd $FILTERS/
}

# Replaced by lxml for python3
copypyxslt()
{
    chkcp $PYXSLT/libxslt.py $FILTERS/
    cp -rp $PYXSLT/* $FILTERS
}

copypoppler()
{
    test -d $FILTERS/poppler || mkdir $FILTERS/poppler || \
        fatal cant create poppler dir
    for f in pdftotext.exe pdfinfo.exe pdftoppm.exe \
             freetype6.dll \
             jpeg62.dll \
             libgcc_s_dw2-1.dll \
             libpng16-16.dll \
             libpoppler*.dll \
             libstdc++-6.dll \
             libtiff3.dll \
             zlib1.dll \
             ; do
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
    chkcp $MINGWBIN/libwinpthread-1.dll $DEST
}

copychm()
{
    DEST=$FILTERS
    cp -rp $CHM/chm $DEST || fatal "can't copy pychm"
}

copypff()
{
    DEST=$FILTERS
    cp -rp $LIBPFF $DEST || fatal "can't copy pffinstall"
	DEST=$DEST/pffinstall/mingw32/bin
    chkcp $LIBPFF/mingw32/bin/pffexport.exe $DEST
    chkcp $MINGWBIN/libgcc_s_dw2-1.dll $DEST
    chkcp $MINGWBIN/libstdc++-6.dll $DEST
    chkcp $MINGWBIN/libwinpthread-1.dll $DEST
}

copyaspell()
{
    DEST=$FILTERS
    cp -rp $ASPELL $DEST || fatal "can't copy $ASPELL"
    DEST=$DEST/aspell-installed/mingw32/bin
    # Check that we do have an aspell.exe.
    chkcp $ASPELL/mingw32/bin/aspell.exe $DEST
    chkcp $MINGWBIN/libgcc_s_dw2-1.dll $DEST
    chkcp $MINGWBIN/libstdc++-6.dll $DEST
    chkcp $MINGWBIN/libwinpthread-1.dll $DEST
}

# Recoll python package. Only when compiled with msvc as this is what
# the standard Python dist is built with
copypyrecoll()
{
    # e.g. build: "/c/Program Files (x86)/Python39-32/python" setup-win.py bdist_wheel
    if test $BUILD = MSVC ; then
        DEST=${DESTDIR}/Share/dist
        test -d $DEST || mkdir $DEST || fatal cant create $DEST
        rm -f ${DEST}/*
        for v in 37;do
            PYRCLWHEEL=${PYRECOLL}/dist/Recoll-${VERSION}-cp${v}-cp${v}m-win32.whl
            chkcp ${PYRCLWHEEL} $DEST
        done
        for v in 38 39;do
            PYRCLWHEEL=${PYRECOLL}/dist/Recoll-${VERSION}-cp${v}-cp${v}-win32.whl
            chkcp ${PYRCLWHEEL} $DEST
        done
    fi
}

# First check that the config is ok
diff -q $RCL/common/autoconfig.h $RCL/common/autoconfig-win.h || \
    fatal autoconfig.h and autoconfig-win.h differ
VERSION=`cat $RCL/RECOLL-VERSION.txt`
CFVERS=`grep PACKAGE_VERSION $RCL/common/autoconfig.h | \
cut -d ' ' -f 3 | sed -e 's/"//g'`
test "$VERSION" = "$CFVERS" ||
    fatal Versions in RECOLL-VERSION.txt and autoconfig.h differ


echo Packaging version $CFVERS

for d in doc examples examples/windows filters images translations; do
    test -d $DESTDIR/Share/$d || mkdir -p $DESTDIR/Share/$d || \
        fatal mkdir $d failed
done

# copyrecoll must stay before copyqt so that windeployqt can do its thing
copyrecoll
copypyrecoll
copyqt
copyaspell
copypoppler
copyantiword
copyunrtf
copymutagen
copywpd
copypff
copypython

echo "MKINSTDIR OK"
