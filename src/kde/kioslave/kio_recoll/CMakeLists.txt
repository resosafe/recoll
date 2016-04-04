project(kio_recoll)

cmake_minimum_required(VERSION 2.8.12)

include(FeatureSummary)

set(QT_MIN_VERSION 5.2.0)
set(KF5_MIN_VERSION 5.0.0)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Network
    Widgets)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

# CoreAddons?
find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
    KIO)

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

include_directories(
   ${CMAKE_SOURCE_DIR}
   ${CMAKE_BINARY_DIR}
)


## Recoll stuff
add_definitions(
    -DRECOLL_DATADIR="${CMAKE_INSTALL_PREFIX}/share/recoll"
    -DLIBDIR="${CMAKE_INSTALL_PREFIX}/lib"
    -DHAVE_CONFIG_H
)


set(rcltop ${CMAKE_CURRENT_SOURCE_DIR}/../../../)

# Execute recoll configuration to create autoconfig.h and version.h and
# generate a PIC lib
execute_process(COMMAND ${rcltop}/configure --disable-qtgui --disable-x11mon --enable-pic --prefix=${CMAKE_INSTALL_PREFIX} --mandir=${CMAKE_INSTALL_PREFIX}/share/man
		WORKING_DIRECTORY ${rcltop}
)
execute_process(COMMAND /bin/sh mkMake
		WORKING_DIRECTORY ${rcltop}/lib
)

link_directories(${rcltop}/lib ${CMAKE_INSTALL_PREFIX}/lib)

include_directories (${CMAKE_SOURCE_DIR}
  ${rcltop}/aspell 
  ${rcltop}/bincimapmime
  ${rcltop}/common 
  ${rcltop}/index 
  ${rcltop}/internfile 
  ${rcltop}/query 
  ${rcltop}/rcldb 
  ${rcltop}/unac
  ${rcltop}/utils 
  ${rcltop}/qtgui
)

set(kio_recoll_SRCS  kio_recoll.cpp htmlif.cpp dirif.cpp 
${rcltop}/qtgui/guiutils.cpp)


# Had the idea to add e.g. /usr/lib/recoll to the rpath so that the dyn lib 
# will be found at run time. But this does not seem to work with debian 
# which strips RPATH by default (I think there is a way for libs in app-specific
# paths but I did not find it. Link with the .a instead.
#SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib/recoll")

add_library(kio_recoll MODULE ${kio_recoll_SRCS})

add_custom_target(parser
                 COMMAND make wasaparse.tab.cpp
                 WORKING_DIRECTORY ${rcltop}/query
)
add_custom_target(rcllib
                 COMMAND make librecoll.a
                 WORKING_DIRECTORY ${rcltop}/lib
)
add_dependencies(rcllib parser)
add_dependencies(kio_recoll parser rcllib)

target_link_libraries(kio_recoll
recoll 
xapian 
KF5::KIOCore
z 
pthread
)

install(FILES recoll.protocol DESTINATION ${SERVICES_INSTALL_DIR})
install(FILES data/welcome.html	data/help.html
	      DESTINATION  ${DATA_INSTALL_DIR}/kio_recoll)

# Tried but could not use PLUGIN_INSTALL_DIR (/usr/lib64/plugins), or
# /usr/lib64/qt5/plugins/kf5/kio/recoll.so: the module is not found by
# dolphin). Actually that's because of the protocol file. recoll has 
# exec=kio_recoll, file has exec=kf5/kio/file

set_target_properties(kio_recoll PROPERTIES OUTPUT_NAME "kio_recoll")
install(TARGETS kio_recoll DESTINATION ${LIB_INSTALL_DIR}/qt5/plugins)
