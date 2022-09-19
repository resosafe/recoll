TARGET = xapian-check
QT       -= core gui
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app

DEFINES += HAVE_CONFIG_H

RECOLLDEPS = ../../../../recolldeps/msvc
XAPIANDIR = $$RECOLLDEPS/xapian-core/

SOURCES += \
        $$XAPIANDIR/bin/xapian-check.cc

INCLUDEPATH += $$XAPIANDIR \
            $$XAPIANDIR/include \
            $$XAPIANDIR/common

windows {
  DEFINES += __WIN32__
  DEFINES += UNICODE
  contains(QMAKE_CC, cl){
    # msvc
    DEFINES += USING_STATIC_LIBICONV
    INCLUDEPATH += \
      ../../../../recolldeps/msvc/zlib-1.2.11/ \
      ../../../../recolldeps/msvc/wlibiconv/include
    LIBS += \
      -L../build-libxapian-Desktop_Qt_5_14_2_MSVC2017_32bit-Release/release \
        -llibxapian \
      -L$$RECOLLDEPS/build-libiconv-Desktop_Qt_5_14_2_MSVC2017_32bit-Release/release/ \
        -llibiconv \
      $$RECOLLDEPS/zlib-1.2.11/zdll.lib \
      -lrpcrt4 -lws2_32 -luser32 -lshell32 \
      -lshlwapi -lpsapi -lkernel32
   }
}
