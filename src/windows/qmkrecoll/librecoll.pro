#-------------------------------------------------
#
# Project created by QtCreator 2015-10-03T09:04:49
#
#-------------------------------------------------

QT       -= core gui

TARGET = librecoll
TEMPLATE = lib

DEFINES += LIBRECOLL_LIBRARY BUILDING_RECOLL
DEFINES += READFILE_ENABLE_MINIZ
DEFINES += READFILE_ENABLE_MD5
DEFINES += READFILE_ENABLE_ZLIB

SOURCES += \
../../aspell/rclaspell.cpp \
../../bincimapmime/convert.cc \
../../bincimapmime/mime-parsefull.cc \
../../bincimapmime/mime-parseonlyheader.cc \
../../bincimapmime/mime-printbody.cc \
../../bincimapmime/mime.cc \
../../common/webstore.cpp \
../../common/cstr.cpp \
../../common/rclconfig.cpp \
../../common/rclinit.cpp \
../../common/syngroups.cpp \
../../common/textsplit.cpp \
../../common/textsplitko.cpp \
../../common/unacpp.cpp \
../../common/utf8fn.cpp \
../../index/webqueuefetcher.cpp \
../../index/fetcher.cpp \
../../index/exefetcher.cpp \
../../index/fsfetcher.cpp \
../../index/idxdiags.cpp \
../../index/idxstatus.cpp \
../../index/indexer.cpp \
../../index/mimetype.cpp \
../../index/subtreelist.cpp \
../../internfile/extrameta.cpp \
../../internfile/htmlparse.cpp \
../../internfile/internfile.cpp \
../../internfile/mh_exec.cpp \
../../internfile/mh_execm.cpp \
../../internfile/mh_html.cpp \
../../internfile/mh_mail.cpp \
../../internfile/mh_mbox.cpp \
../../internfile/mh_text.cpp \
../../internfile/mh_xslt.cpp \
../../internfile/mimehandler.cpp \
../../internfile/myhtmlparse.cpp \
../../internfile/txtdcode.cpp \
../../internfile/uncomp.cpp \
../../query/docseq.cpp \
../../query/docseqdb.cpp \
../../query/docseqhist.cpp \
../../query/dynconf.cpp \
../../query/filtseq.cpp \
../../query/plaintorich.cpp \
../../query/qresultstore.cpp \
../../query/recollq.cpp \
../../query/reslistpager.cpp \
../../query/sortseq.cpp \
../../query/wasaparse.cpp \
../../query/wasaparseaux.cpp \
../../rcldb/daterange.cpp \
../../rcldb/expansiondbs.cpp \
../../rcldb/rclabstract.cpp \
../../rcldb/rclabsfromtext.cpp \
../../rcldb/rcldb.cpp \
../../rcldb/rcldoc.cpp \
../../rcldb/rcldups.cpp \
../../rcldb/rclquery.cpp \
../../rcldb/rclterms.cpp \
../../rcldb/searchdata.cpp \
../../rcldb/searchdatatox.cpp \
../../rcldb/searchdataxml.cpp \
../../rcldb/stemdb.cpp \
../../rcldb/stoplist.cpp \
../../rcldb/synfamily.cpp \
../../rcldb/rclvalues.cpp \
../../rcldb/rclvalues.h \
../../unac/unac.cpp \
../../utils/appformime.cpp \
../../utils/base64.cpp \
../../utils/cancelcheck.cpp \
../../utils/chrono.cpp \
../../utils/circache.cpp \
../../utils/closefrom.cpp \
../../utils/cmdtalk.cpp \
../../utils/conftree.cpp \
../../utils/copyfile.cpp \
../../utils/cpuconf.cpp \
../../utils/dlib.cpp \
../../utils/ecrontab.cpp \
../../utils/listmem.cpp \
../../utils/utf8iter.cpp \
../../utils/zlibut.cpp \
../../utils/zlibut.h \
../../utils/fileudi.cpp \
../../utils/fstreewalk.cpp \
../../utils/hldata.cpp \
../../utils/idfile.cpp \
../../utils/log.cpp \
../../utils/md5.cpp \
../../utils/md5ut.cpp \
../../utils/mimeparse.cpp \
../../utils/miniz.cpp \
../../utils/pathut.cpp \
../../utils/pxattr.cpp \
../../utils/rclutil.cpp \
../../utils/readfile.cpp \
../../utils/smallut.cpp \
../../utils/strmatcher.cpp \
../../utils/transcode.cpp \
../../utils/wipedir.cpp

INCLUDEPATH += ../../common ../../index ../../internfile ../../query \
            ../../unac ../../utils ../../aspell ../../rcldb ../../qtgui \
            ../../xaposix ../../confgui ../../bincimapmime 

windows {
  # VC only defines __WIN32, not __WIN32__ . For some reason xapian uses
  #  __WIN32__ which it actually defines in conf_post.h if __WIN32 is
  #  set. Reason: mystery.
  DEFINES += __WIN32__
  DEFINES += PSAPI_VERSION=1
  defines += UNICODE

  SOURCES += \
  ../../windows/execmd_w.cpp \
  ../../windows/fnmatch.c \
  ../../windows/wincodepages.cpp \
  ../../windows/strptime.cpp

  contains(QMAKE_CC, gcc){
    # MingW
    # This is necessary to avoid an undefined impl__xmlFree.
    # See comment in libxml/xmlexports.h
    DEFINES += LIBXML_STATIC
    RECOLLDEPS = C:/recolldeps
    QMAKE_CXXFLAGS += -std=c++11 -pthread -Wno-unused-parameter
    LIBS += \
      $$RECOLLDEPS/mingw/libxslt/libxslt-1.1.29/win32/bin.mingw/libxslt.a \
      $$RECOLLDEPS/mingw/libxml2/libxml2-2.9.4+dfsg1/win32/bin.mingw/libxml2.a \
      $$RECOLLDEPS/mingw/xapian-core-1.4.11/.libs/libxapian-30.dll \
      $$RECOLLDEPS/mingw/zlib-1.2.8/zlib1.dll \
      -luuid -luser32 -lshell32 -liconv -lshlwapi -lpsapi -lkernel32
    INCLUDEPATH += ../../windows \
      $$RECOLLDEPS/mingw/xapian-core-1.4.11/include \
      $$RECOLLDEPS/mingw/libxslt/libxslt-1.1.29/ \
      $$RECOLLDEPS/mingw/libxml2/libxml2-2.9.4+dfsg1/include
  }

  contains(QMAKE_CC, cl){
    # MSVC
    RECOLLDEPS = ../../../../recolldeps
    CONFIG += staticlib
    DEFINES += USING_STATIC_LIBICONV
    INCLUDEPATH += ../../windows \
      $$RECOLLDEPS/msvc/xapian-core/include \
      $$RECOLLDEPS/msvc/zlib-1.2.11/ \
      $$RECOLLDEPS/msvc/libxslt/libxslt-1.1.29/ \
      $$RECOLLDEPS/msvc/libxml2/libxml2-2.9.4+dfsg1/include \
      $$RECOLLDEPS/msvc/wlibiconv/include
  }

}

unix:!mac {
    target.path = /usr/lib
    INSTALLS += target
}

mac {
    CONFIG += staticlib
    QMAKE_APPLE_DEVICE_ARCHS = x86_64 arm64
    # This is necessary to avoid an undefined impl__xmlFree.
    # See comment in libxml/xmlexports.h
    DEFINES += LIBXML_STATIC
    RECOLLDEPS = ../../../../
    QMAKE_CXXFLAGS += -std=c++11 -pthread -Wno-unused-parameter
    INCLUDEPATH += \
      $$RECOLLDEPS/xapian-core-1.4.18/include 
}
