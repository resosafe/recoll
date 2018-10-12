#-------------------------------------------------
#
# Project created by QtCreator 2015-10-03T09:04:49
#
#-------------------------------------------------

QT       -= core gui

TARGET = librecoll
TEMPLATE = lib

DEFINES += LIBRECOLL_LIBRARY BUILDING_RECOLL
DEFINES -= UNICODE
DEFINES -= _UNICODE
DEFINES += _MBCS
DEFINES += PSAPI_VERSION=1

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
../../common/unacpp.cpp \
../../common/utf8fn.cpp \
../../index/webqueue.cpp \
../../index/webqueuefetcher.cpp \
../../index/checkretryfailed.cpp \
../../index/fetcher.cpp \
../../index/exefetcher.cpp \
../../index/fsfetcher.cpp \
../../index/fsindexer.cpp \
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
../../utils/conftree.cpp \
../../utils/copyfile.cpp \
../../utils/cpuconf.cpp \
../../utils/ecrontab.cpp \
../../utils/zlibut.cpp \
../../utils/zlibut.h \
../../windows/execmd_w.cpp \
../../windows/fnmatch.c \
../../windows/wincodepages.cpp \
../../utils/fileudi.cpp \
../../utils/fstreewalk.cpp \
../../utils/hldata.cpp \
../../utils/idfile.cpp \
../../utils/log.cpp \
../../utils/md5.cpp \
../../utils/md5ut.cpp \
../../utils/mimeparse.cpp \
../../utils/pathut.cpp \
../../utils/pxattr.cpp \
../../utils/rclionice.cpp \
../../utils/rclutil.cpp \
../../utils/readfile.cpp \
../../utils/smallut.cpp \
../../utils/strmatcher.cpp \
../../utils/transcode.cpp \
../../utils/wipedir.cpp \
../../windows/strptime.cpp \
../../windows/dirent.c

INCLUDEPATH += ../../common ../../index ../../internfile ../../query \
            ../../unac ../../utils ../../aspell ../../rcldb ../../qtgui \
            ../../xaposix ../../confgui ../../bincimapmime 

windows {
    contains(QMAKE_CC, gcc){
        # MingW
        QMAKE_CXXFLAGS += -std=c++11 -pthread -Wno-unused-parameter
    }
    contains(QMAKE_CC, cl){
        # Visual Studio
    }
  LIBS += c:/temp/xapian-core-1.4.5/.libs/libxapian-30.dll \
      c:/temp/zlib-1.2.8/zlib1.dll -liconv -lshlwapi -lpsapi -lkernel32
  INCLUDEPATH += ../../windows \
            C:/temp/xapian-core-1.4.5/include

#  LIBS += c:/temp/xapian-core-1.2.21/.libs/libxapian-22.dll \
#       c:/temp/zlib-1.2.8/zlib1.dll -liconv -lshlwapi -lpsapi -lkernel32
#  INCLUDEPATH += ../../windows \
            C:/temp/xapian-core-1.2.21/include
}

unix {
    target.path = /usr/lib
    INSTALLS += target
}
