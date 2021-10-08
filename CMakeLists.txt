cmake_minimum_required( VERSION 3.20.0 )

project(Dark-Templar-Mount-Daemon
	VERSION 2.6.1
	LANGUAGES C CXX)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set (OS_LINUX true)
	add_definitions(-DOS_Linux)
endif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
	set (OS_FREEBSD true)
	add_definitions(-DOS_FreeBSD)
endif (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")

include(GNUInstallDirs)

# installation directory configuration
set(CONFIG_DIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}" CACHE PATH "Config files directory")
set(DATA_PREFIX "${CMAKE_INSTALL_FULL_DATADIR}" CACHE PATH "Prefix prepended to data install directories")
set(PIDFILE_PATH "/run/dtmd.pid" CACHE PATH "Path of pidfile")
set(SOCKET_PATH "/run/dtmd.socket" CACHE PATH "Path of socket")

option(ENABLE_SYSLOG "enable installation of desktop entry files" ON)
option(ENABLE_LIBVERSION "enable libraries versioning" ON)
option(ENABLE_CXX "enable C++" ON)
option(ENABLE_CONSOLE_CLIENT "enable console client" ON)
option(ENABLE_QT_CLIENT "enable qt-based client" ON)

if (OS_LINUX)
	option(DISABLE_EXT_MOUNT "disable external mount")
endif (OS_LINUX)

add_definitions(-DDATA_PREFIX=\"${DATA_PREFIX}\")
add_definitions(-DCONFIG_DIR=\"${CONFIG_DIR}\")

if (OS_LINUX)
	set(MTAB_DIR "/etc" CACHE PATH "mtab file location directory")
	message(STATUS "To change location directory of mtab file use -DMTAB_DIR=path")
endif (OS_LINUX)

if (ENABLE_SYSLOG)
	add_definitions(-DENABLE_SYSLOG)
endif (ENABLE_SYSLOG)

if (OS_LINUX)
	add_definitions(-DMTAB_DIR=\"${MTAB_DIR}\")

	if (DISABLE_EXT_MOUNT)
		add_definitions(-DDISABLE_EXT_MOUNT)
	endif (DISABLE_EXT_MOUNT)
endif (OS_LINUX)

if (OS_LINUX)
	option(LINUX_UDEV "use udev backend")
	message(STATUS "set LINUX_UDEV to force enable udev backend or disable it and instead use linux userspace interfaces")
endif (OS_LINUX)

message(STATUS "To change pid file path use -DPIDFILE_PATH=path")
message(STATUS "To change socket path use -DSOCKET_PATH=path")

if (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
	add_definitions(-DNDEBUG)
endif (NOT CMAKE_BUILD_TYPE STREQUAL "Debug")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (ENABLE_QT_CLIENT)
	if (NOT ${ENABLE_CXX})
		message(FATAL_ERROR "Qt client requires c++ library being enabled")
	endif (NOT ${ENABLE_CXX})
endif (ENABLE_QT_CLIENT)

if (UNIX)
	set( CMAKE_THREAD_PREFER_PTHREAD 1 )
endif (UNIX)
find_package (Threads REQUIRED)
find_package (DtCommand 2.0.0 REQUIRED)
include_directories( ${DtCommand_INCLUDE_DIRS} )

add_definitions(-D_FILE_OFFSET_BITS=64)
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/library )
include_directories( ${CMAKE_CURRENT_BINARY_DIR} )

if (ENABLE_QT_CLIENT)
	find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)
endif (ENABLE_QT_CLIENT)

CONFIGURE_FILE( ${CMAKE_CURRENT_SOURCE_DIR}/dtmd.h.in ${CMAKE_CURRENT_BINARY_DIR}/dtmd.h @ONLY )

set ( LINUX_UDEV_SOURCES daemon/modules/linux-udev/system_module.c )
set ( LINUX_UDEV_HEADERS )
set ( LINUX_UDEV_LIBS )

set ( UNIX_USERSPACE_SOURCES daemon/modules/unix-userspace/system_module.c )
set ( UNIX_USERSPACE_HEADERS )
set ( UNIX_USERSPACE_LIBS pthread )

set ( MISC_LIBRARY_SOURCES library/dtmd-misc.c )
set ( MISC_LIBRARY_HEADERS library/dtmd-misc.h )

set ( LIBRARY_SOURCES library/dtmd-library.c )
set ( LIBRARY_HEADERS library/dtmd-library.h library/dt-print-helpers.h )
set ( LIBRARY_LIBS ${CMAKE_THREAD_LIBS_INIT} ${DtCommand_LIBRARIES} )

set ( LIBRARY_CXX_SOURCES library/dtmd-library++.cpp )
set ( LIBRARY_CXX_HEADERS library/dtmd-library++.hpp )
set ( LIBRARY_CXX_LIBS ${CMAKE_THREAD_LIBS_INIT} )

if (OS_LINUX)
	set ( LIBRARY_LIBS ${LIBRARY_LIBS} rt )
endif (OS_LINUX)

set ( DAEMON_SOURCES daemon/daemon-main.c daemon/actions.c daemon/filesystem_mnt.c daemon/filesystem_opts.c daemon/label.c daemon/lists.c daemon/mnt_funcs.c daemon/config_file.c daemon/poweroff.c )
set ( DAEMON_HEADERS                      daemon/actions.h daemon/filesystem_mnt.h daemon/filesystem_opts.h daemon/label.h daemon/lists.h daemon/mnt_funcs.h daemon/config_file.h daemon/poweroff.h daemon/dtmd-internal.h daemon/system_module.h daemon/log.h daemon/return_codes.h library/dt-print-helpers.h )
set ( DAEMON_LIBS ${DtCommand_LIBRARIES} )

set ( DTMD_CONFIG_SOURCES tools/dtmd-config.c )
set ( DTMD_CONFIG_HEADERS )
set ( DTMD_CONFIG_LIBS )

set ( CONSOLE_CLIENT_SOURCES client/console/main.c )
set ( CONSOLE_CLIENT_HEADERS )
set ( CONSOLE_CLIENT_LIBS ${DtCommand_LIBRARIES} )

set ( QT_CLIENT_SOURCES client/qt/main.cpp client/qt/control.cpp )
set ( QT_CLIENT_QT_HEADERS                 client/qt/control.hpp )
set ( QT_CLIENT_HEADERS )
set ( QT_CLIENT_UI )
set ( QT_CLIENT_RES )
set ( QT_CLIENT_LIBS )
set ( QT_IMAGES
	client/qt/images/disc.mounted.png
	client/qt/images/disc.png
	client/qt/images/fail.png
	client/qt/images/normal.png
	client/qt/images/notify.png
	client/qt/images/removable_disk.mounted.png
	client/qt/images/removable_disk.png
	client/qt/images/sdcard.mounted.png
	client/qt/images/sdcard.png
	client/qt/images/success.png
	client/qt/images/working.png
)

include(FindPkgConfig)

set (BUILD_BACKEND_UDEV FALSE)

if (OS_LINUX)
	if (DEFINED LINUX_UDEV)
		if (${LINUX_UDEV})
			pkg_check_modules(LIBUDEV libudev REQUIRED)
			set (BUILD_BACKEND_UDEV TRUE)
		endif (${LINUX_UDEV})
	else (DEFINED LINUX_UDEV)
		pkg_check_modules(LIBUDEV libudev)

		if (DEFINED LIBUDEV_FOUND)
			if (${LIBUDEV_FOUND} EQUAL 1)
				set (BUILD_BACKEND_UDEV TRUE)
			endif (${LIBUDEV_FOUND} EQUAL 1)
		endif (DEFINED LIBUDEV_FOUND)
	endif (DEFINED LINUX_UDEV)
endif (OS_LINUX)

if (OS_LINUX AND BUILD_BACKEND_UDEV)
	include_directories( ${LIBUDEV_INCLUDE_DIRS} )

	set ( DAEMON_SOURCES ${DAEMON_SOURCES} ${LINUX_UDEV_SOURCES} )
	set ( DAEMON_HEADERS ${DAEMON_HEADERS} ${LINUX_UDEV_HEADERS} )
	set ( DAEMON_LIBS    ${DAEMON_LIBS}    ${LINUX_UDEV_LIBS}    ${LIBUDEV_LIBRARIES} )
	message(STATUS "Subsystem: udev")
endif (OS_LINUX AND BUILD_BACKEND_UDEV)

if (OS_FREEBSD OR (OS_LINUX AND (NOT BUILD_BACKEND_UDEV)))
	if (OS_LINUX AND (NOT BUILD_BACKEND_UDEV))
		pkg_check_modules(BLKID blkid REQUIRED)
		include_directories( ${BLKID_INCLUDE_DIRS} )
	endif (OS_LINUX AND (NOT BUILD_BACKEND_UDEV))

	set ( DAEMON_SOURCES ${DAEMON_SOURCES} ${UNIX_USERSPACE_SOURCES} )
	set ( DAEMON_HEADERS ${DAEMON_HEADERS} ${UNIX_USERSPACE_HEADERS} )
	set ( DAEMON_LIBS    ${DAEMON_LIBS}    ${UNIX_USERSPACE_LIBS} )

	if (OS_LINUX AND (NOT BUILD_BACKEND_UDEV))
		set ( DAEMON_LIBS ${DAEMON_LIBS} ${BLKID_LIBRARIES} )
	endif (OS_LINUX AND (NOT BUILD_BACKEND_UDEV))

	if (OS_FREEBSD)
		set ( DAEMON_LIBS ${DAEMON_LIBS} geom cam )
	endif (OS_FREEBSD)

	message(STATUS "Subsystem: native")
endif (OS_FREEBSD OR (OS_LINUX AND (NOT BUILD_BACKEND_UDEV)))

add_library( dtmd-misc SHARED ${MISC_LIBRARY_SOURCES} ${MISC_LIBRARY_HEADERS} )
if (ENABLE_LIBVERSION)
	set_target_properties( dtmd-misc PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR} )
endif (ENABLE_LIBVERSION)
target_link_libraries( dtmd-misc )

add_library( dtmd-library SHARED ${LIBRARY_SOURCES} ${LIBRARY_HEADERS} )
if (ENABLE_LIBVERSION)
	set_target_properties( dtmd-library PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR} )
endif (ENABLE_LIBVERSION)
target_link_libraries( dtmd-library ${LIBRARY_LIBS} dtmd-misc )

add_executable( dtmd-daemon ${DAEMON_SOURCES} ${DAEMON_HEADERS})
target_link_libraries( dtmd-daemon ${DAEMON_LIBS} dtmd-misc )

add_executable( dtmd-config ${DTMD_CONFIG_SOURCES} ${DTMD_CONFIG_HEADERS})
target_link_libraries( dtmd-config ${DTMD_CONFIG_LIBS} )

if (ENABLE_CONSOLE_CLIENT)
	add_executable( dtmd ${CONSOLE_CLIENT_SOURCES} ${CONSOLE_CLIENT_HEADERS})
	target_link_libraries( dtmd dtmd-library ${CONSOLE_CLIENT_LIBS} )
endif (ENABLE_CONSOLE_CLIENT)

if (ENABLE_CXX)
	add_library( dtmd-library++ SHARED ${LIBRARY_CXX_SOURCES} ${LIBRARY_CXX_HEADERS} )
	if (ENABLE_LIBVERSION)
		set_target_properties( dtmd-library++ PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR} )
	endif (ENABLE_LIBVERSION)
	target_link_libraries( dtmd-library++ ${LIBRARY_CXX_LIBS} dtmd-library dtmd-misc )
endif (ENABLE_CXX)

if (ENABLE_QT_CLIENT)
	qt_wrap_cpp(QT_CLIENT_MOC_SOURCES ${QT_CLIENT_QT_HEADERS})
	qt_wrap_ui(QT_CLIENT_UI_SOURCES ${QT_CLIENT_UI})
	qt_add_resources(QT_CLIENT_RESOURCES ${QT_CLIENT_RES})

	add_executable (dtmd-qt ${QT_CLIENT_SOURCES} ${QT_CLIENT_QT_HEADERS} ${QT_CLIENT_HEADERS} ${QT_CLIENT_MOC_SOURCES} ${QT_CLIENT_UI_SOURCES} ${QT_CLIENT_RESOURCES})
	target_include_directories(dtmd-qt PRIVATE ${Qt6Core_INCLUDE_DIRS} ${Qt6Gui_INCLUDE_DIRS} ${Qt6Widgets_INCLUDE_DIRS})
	target_link_libraries (dtmd-qt dtmd-library++ ${QT_CLIENT_LIBS} ${Qt6Core_LIBRARIES} ${Qt6Gui_LIBRARIES} ${Qt6Widgets_LIBRARIES} )
endif (ENABLE_QT_CLIENT)

ENABLE_TESTING()

set (TEST_SOURCES_decode_label daemon/label.c tests/decode_label_test.c tests/dt_tests.h)
set (TEST_LIBS_decode_label )

set (TEST_SOURCES_lists daemon/lists.c tests/lists.c tests/dt_tests.h daemon/label.c daemon/label.h daemon/return_codes.h)
set (TEST_LIBS_lists dtmd-misc)

if (OS_LINUX)
	set (TEST_SOURCES_filesystem_opts daemon/filesystem_opts.c tests/filesystem_opts_test.c tests/dt_tests.h)
	set (TEST_LIBS_filesystem_opts dtmd-misc)
endif (OS_LINUX)

set (ALL_TESTS decode_label lists)

if (OS_LINUX)
	set (ALL_TESTS ${ALL_TESTS} filesystem_opts)
endif (OS_LINUX)

foreach (CURRENT_TEST ${ALL_TESTS})
	add_executable( ${CURRENT_TEST}_test ${TEST_SOURCES_${CURRENT_TEST}})
	target_link_libraries( ${CURRENT_TEST}_test ${TEST_LIBS_${CURRENT_TEST}} )
	add_test( ${CURRENT_TEST}_test ${CMAKE_CURRENT_BINARY_DIR}/${CURRENT_TEST}_test )
endforeach (CURRENT_TEST)

# installation config
install(TARGETS dtmd-misc    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
install(TARGETS dtmd-library LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
install(TARGETS dtmd-daemon  RUNTIME DESTINATION ${CMAKE_INSTALL_SBINDIR} )
install(TARGETS dtmd-config  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} )

if (ENABLE_CONSOLE_CLIENT)
	install(TARGETS dtmd         RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif (ENABLE_CONSOLE_CLIENT)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dtmd.h" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES "library/dtmd-misc.h"                DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES "library/dtmd-library.h"             DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES "daemon/dtmd.conf"                   DESTINATION "${CONFIG_DIR}")
install(FILES "cmake/FindDtmdMisc.cmake"           DESTINATION "${CMAKE_ROOT}/Modules" )
install(FILES "cmake/FindDtmdLibrary.cmake"        DESTINATION "${CMAKE_ROOT}/Modules" )

if (ENABLE_CXX)
	install(TARGETS dtmd-library++ LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} )
	install(FILES "library/dtmd-library++.hpp" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
	install(FILES "cmake/FindDtmdLibrary++.cmake" DESTINATION "${CMAKE_ROOT}/Modules" )
endif (ENABLE_CXX)

if (ENABLE_QT_CLIENT)
	install(TARGETS dtmd-qt      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	install(FILES ${QT_IMAGES} DESTINATION "${DATA_PREFIX}/dtmd" )
endif (ENABLE_QT_CLIENT)
