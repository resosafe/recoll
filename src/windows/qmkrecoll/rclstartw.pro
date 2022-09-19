
QT       -= core gui

TARGET = rclstartw
TEMPLATE = app

DEFINES += BUILDING_RECOLL
DEFINES += UNICODE
DEFINES += PSAPI_VERSION=1
DEFINES += __WIN32__

SOURCES += \
../rclstartw.cpp

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
  LIBS += -lshlwapi -lpsapi -lkernel32

  INCLUDEPATH += ../../windows
}
