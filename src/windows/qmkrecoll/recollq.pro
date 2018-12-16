
QT       -= core gui

TARGET = recollq
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app

DEFINES += BUILDING_RECOLL
DEFINES -= UNICODE
DEFINES -= _UNICODE
DEFINES += _MBCS
DEFINES += PSAPI_VERSION=1


SOURCES += \
../../query/recollqmain.cpp

INCLUDEPATH += ../../common ../../index ../../internfile ../../query \
            ../../unac ../../utils ../../aspell ../../rcldb ../../qtgui \
            ../../xaposix ../../confgui ../../bincimapmime 

windows {
    contains(QMAKE_CC, gcc){
        # MingW
        QMAKE_CXXFLAGS += -std=c++11 -Wno-unused-parameter
    }
    contains(QMAKE_CC, cl){
        # Visual Studio
    }
  LIBS += \
    C:/recoll/src/windows/build-librecoll-Desktop_Qt_5_8_0_MinGW_32bit-Release/release/librecoll.dll \
    -lshlwapi -lpsapi -lkernel32

  INCLUDEPATH += ../../windows
}
