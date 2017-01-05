project(Dark-Templar-Mount-Daemon)

cmake_minimum_required( VERSION 2.8.8 )

set(DTMD_VERSION_MAJOR 1)
set(DTMD_VERSION_MINOR 10)
set(DTMD_VERSION_PATCH 5)
set(DTMD_VERSION "${DTMD_VERSION_MAJOR}.${DTMD_VERSION_MINOR}.${DTMD_VERSION_PATCH}")

set(CPACK_PACKAGE_VERSION_MAJOR ${DTMD_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${DTMD_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${DTMD_VERSION_PATCH})
set(CPACK_GENERATOR "TBZ2")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_SOURCE_IGNORE_FILES
	".git;build;CMakeLists.txt.user;CMakeLists.txt.user.*;ignore;TODO;cppcheck-linux.sh")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${DTMD_VERSION}")
# MUST be after CPACK_* variables.
include(CPack)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set (OS_LINUX true)
	add_definitions(-DOS_Linux)
endif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
	set (OS_FREEBSD true)
	add_definitions(-DOS_FreeBSD)
endif (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")

# installation directory configuration
set(CONFIG_DIR "/etc" CACHE PATH "Config files directory")

set(LIB_SUFFIX "" CACHE STRING "Define suffix of directory name (32/64)")
set(EXEC_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX} CACHE PATH "Installation prefix for executables and object code libraries" FORCE)
set(BIN_INSTALL_DIR ${EXEC_INSTALL_PREFIX}/bin CACHE PATH "Installation prefix for user executables" FORCE)
set(SBIN_INSTALL_DIR ${EXEC_INSTALL_PREFIX}/sbin CACHE PATH "Installation prefix for user executables" FORCE)
set(LIB_INSTALL_DIR ${EXEC_INSTALL_PREFIX}/lib${LIB_SUFFIX} CACHE PATH  "Installation prefix for object code libraries" FORCE)
set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_PREFIX}/include CACHE PATH "Installation prefix for C header files" FORCE)
set(DATA_PREFIX "${CMAKE_INSTALL_PREFIX}/share" CACHE PATH "Prefix prepended to data install directories" FORCE)

option(ENABLE_SYSLOG "enable installation of desktop entry files" ON)
option(ENABLE_LIBVERSION "enable libraries versioning" ON)
option(ENABLE_CXX "enable C++" ON)
option(ENABLE_CONSOLE_CLIENT "enable console client" ON)

if (OS_LINUX)
	option(WITH_MTAB_WRITEABLE "use read-write mtab file" ON)
	option(DISABLE_EXT_MOUNT "disable external mount")
endif (OS_LINUX)

set(PIDFILE_PATH "/var/run/dtmd.pid" CACHE PATH "Path of pidfile")
set(SOCKET_PATH "/var/run/dtmd.socket" CACHE PATH "Path of socket")

add_definitions(-DDATA_PREFIX=\"${DATA_PREFIX}\")
add_definitions(-DCONFIG_DIR=\"${CONFIG_DIR}\")

if (OS_LINUX)
	set(MTAB_DIR "/etc" CACHE PATH "mtab file location directory")
	message(STATUS "To change location directory of mtab file use -DMTAB_DIR=path")
endif (OS_LINUX)

if (ENABLE_SYSLOG)
	add_definitions(-DENABLE_SYSLOG)
endif (ENABLE_SYSLOG)

message(STATUS "System: ${CMAKE_SYSTEM_NAME}" )

if (OS_LINUX)
	if (WITH_MTAB_WRITEABLE)
		add_definitions(-DMTAB_DIR=\"${MTAB_DIR}\")
	else (WITH_MTAB_WRITEABLE)
		add_definitions(-DMTAB_READONLY)
	endif (WITH_MTAB_WRITEABLE)

	if (DISABLE_EXT_MOUNT)
		add_definitions(-DDISABLE_EXT_MOUNT)
	endif (DISABLE_EXT_MOUNT)
endif (OS_LINUX)

if (OS_LINUX)
	message(STATUS "set LINUX_UDEV to force enable udev backend or disable it and instead use linux kernel")
endif (OS_LINUX)

message(STATUS "set CLIENT_QT to force enable or disable Qt client")
message(STATUS "To change pid file path use -DPIDFILE_PATH=path")
message(STATUS "To change socket path use -DSOCKET_PATH=path")

# don't USE -O3 with GCC, it causes less precise calculations
if (CMAKE_COMPILER_IS_GNUCC)
	set (CMAKE_C_FLAGS_RELEASE "-O2 -pipe -march=native -Wall -Wextra -Wno-unused-result -DNDEBUG")
	set (CMAKE_CXX_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})

	set (CMAKE_C_FLAGS_DEBUG "-O0 -pipe -march=native -Wall -Wextra -Wno-unused-result -g -ggdb")
	set (CMAKE_CXX_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})
endif (CMAKE_COMPILER_IS_GNUCC)

# clang compiler options
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	set (CLANG_WARNINGS "-Weverything -Wno-missing-prototypes -Wno-padded -Wno-sign-conversion -Wno-conversion -Wno-disabled-macro-expansion -Wno-cast-align" )

	set (CMAKE_C_FLAGS_RELEASE "-O3 -pipe ${CLANG_WARNINGS} -DNDEBUG")
	set (CMAKE_CXX_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})

	set (CMAKE_C_FLAGS_DEBUG "-O0 -pipe ${CLANG_WARNINGS} -g -ggdb")
	set (CMAKE_CXX_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})
endif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

if (DEFINED CLIENT_QT)
	if ((${CLIENT_QT} EQUAL 4) OR (${CLIENT_QT} EQUAL 5))
		if (NOT ${ENABLE_CXX})
			message(FATAL_ERROR "Qt Client requires c++ library being enabled")
		endif (NOT ${ENABLE_CXX})
	endif ((${CLIENT_QT} EQUAL 4) OR (${CLIENT_QT} EQUAL 5))
endif (DEFINED CLIENT_QT)

if (UNIX)
	set( CMAKE_THREAD_PREFER_PTHREAD 1 )
endif (UNIX)
find_package (Threads REQUIRED)
find_package (DtCommand REQUIRED)
include_directories( ${DT_CMD_INCLUDE_DIRS} )

add_definitions(-D_FILE_OFFSET_BITS=64)
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/library )
include_directories( ${CMAKE_CURRENT_BINARY_DIR} )

set (BUILD_QT FALSE)

if (DEFINED CLIENT_QT)
	if (${CLIENT_QT} EQUAL 4)
		find_package(Qt4 REQUIRED COMPONENTS QtCore QtGui)
		set (BUILD_QT TRUE)
	endif (${CLIENT_QT} EQUAL 4)

	if (${CLIENT_QT} EQUAL 5)
		find_package(Qt5Core REQUIRED)
		find_package(Qt5Gui REQUIRED)
		find_package(Qt5Widgets REQUIRED)
		set (BUILD_QT TRUE)
	endif (${CLIENT_QT} EQUAL 5)
else (DEFINED CLIENT_QT)
	if (${ENABLE_CXX})
		find_package(Qt5Core)
		find_package(Qt5Gui)
		find_package(Qt5Widgets)

		if ((DEFINED Qt5Core_FOUND) AND (DEFINED Qt5Gui_FOUND) AND (DEFINED Qt5Widgets_FOUND))
			if ((${Qt5Core_FOUND} EQUAL 1) AND (${Qt5Gui_FOUND} EQUAL 1) AND (${Qt5Widgets_FOUND} EQUAL 1))
				set (BUILD_QT TRUE)
				set (CLIENT_QT 5)
			endif ((${Qt5Core_FOUND} EQUAL 1) AND (${Qt5Gui_FOUND} EQUAL 1) AND (${Qt5Widgets_FOUND} EQUAL 1))
		endif ((DEFINED Qt5Core_FOUND) AND (DEFINED Qt5Gui_FOUND) AND (DEFINED Qt5Widgets_FOUND))

		if (NOT ${BUILD_QT})
			find_package(Qt4 COMPONENTS QtCore QtGui)

			if ((DEFINED QT_QTCORE_FOUND) AND (DEFINED QT_QTGUI_FOUND))
				if ((${QT_QTCORE_FOUND} EQUAL 1) AND (${QT_QTGUI_FOUND} EQUAL 1))
					set (BUILD_QT TRUE)
					set (CLIENT_QT 4)
				endif ((${QT_QTCORE_FOUND} EQUAL 1) AND (${QT_QTGUI_FOUND} EQUAL 1))
			endif ((DEFINED QT_QTCORE_FOUND) AND (DEFINED QT_QTGUI_FOUND))
		endif (NOT ${BUILD_QT})
	endif (${ENABLE_CXX})
endif (DEFINED CLIENT_QT)

CONFIGURE_FILE( ${CMAKE_CURRENT_SOURCE_DIR}/dtmd.h.in ${CMAKE_CURRENT_BINARY_DIR}/dtmd.h @ONLY )

set ( LINUX_UDEV_SOURCES daemon/modules/linux-udev/udev.c )
set ( LINUX_UDEV_HEADERS )
set ( LINUX_UDEV_LIBS )

set ( UNIX_KERNEL_SOURCES daemon/modules/unix-kernel/kernel.c )
set ( UNIX_KERNEL_HEADERS )
set ( UNIX_KERNEL_LIBS pthread )

set ( MISC_LIBRARY_SOURCES library/dtmd-misc.c )
set ( MISC_LIBRARY_HEADERS library/dtmd-misc.h )

set ( LIBRARY_SOURCES library/dtmd-library.c )
set ( LIBRARY_HEADERS library/dtmd-library.h )
set ( LIBRARY_LIBS ${CMAKE_THREAD_LIBS_INIT} ${DT_CMD_LIBRARIES} )

set ( LIBRARY_CXX_SOURCES library/dtmd-library++.cpp )
set ( LIBRARY_CXX_HEADERS library/dtmd-library++.hpp )
set ( LIBRARY_CXX_LIBS ${CMAKE_THREAD_LIBS_INIT} )

if (OS_LINUX)
	set ( LIBRARY_LIBS ${LIBRARY_LIBS} rt )
endif (OS_LINUX)

set ( DAEMON_SOURCES daemon/daemon-main.c daemon/actions.c daemon/filesystem_mnt.c daemon/filesystem_opts.c daemon/label.c daemon/lists.c daemon/mnt_funcs.c daemon/config_file.c )
set ( DAEMON_HEADERS                      daemon/actions.h daemon/filesystem_mnt.h daemon/filesystem_opts.h daemon/label.h daemon/lists.h daemon/mnt_funcs.h daemon/config_file.h daemon/dtmd-internal.h daemon/system_module.h daemon/log.h daemon/return_codes.h )
set ( DAEMON_LIBS ${DT_CMD_LIBRARIES} )

set ( DTMD_CONFIG_SOURCES tools/dtmd-config.c )
set ( DTMD_CONFIG_HEADERS )
set ( DTMD_CONFIG_LIBS )

set ( CONSOLE_CLIENT_SOURCES client/console/main.c )
set ( CONSOLE_CLIENT_HEADERS )
set ( CONSOLE_CLIENT_LIBS ${DT_CMD_LIBRARIES} )

set ( QT_CLIENT_SOURCES client/qt/main.cpp client/qt/control.cpp client/qt/qcustomdeviceaction.cpp client/qt/qcustomstatefuldeviceaction.cpp )
set ( QT_CLIENT_QT_HEADERS                 client/qt/control.hpp client/qt/qcustomdeviceaction.hpp client/qt/qcustomstatefuldeviceaction.hpp )
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
		if (${LINUX_UDEV} EQUAL 1)
			pkg_check_modules(LIBUDEV libudev REQUIRED)
			set (BUILD_BACKEND_UDEV TRUE)
		endif (${LINUX_UDEV} EQUAL 1)
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

	set ( DAEMON_SOURCES ${DAEMON_SOURCES} ${UNIX_KERNEL_SOURCES} )
	set ( DAEMON_HEADERS ${DAEMON_HEADERS} ${UNIX_KERNEL_HEADERS} )
	set ( DAEMON_LIBS    ${DAEMON_LIBS}    ${UNIX_KERNEL_LIBS} )

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
	set_target_properties( dtmd-misc PROPERTIES VERSION ${DTMD_VERSION} SOVERSION ${DTMD_VERSION_MAJOR} )
endif (ENABLE_LIBVERSION)
target_link_libraries( dtmd-misc )

add_library( dtmd-library SHARED ${LIBRARY_SOURCES} ${LIBRARY_HEADERS} )
if (ENABLE_LIBVERSION)
	set_target_properties( dtmd-library PROPERTIES VERSION ${DTMD_VERSION} SOVERSION ${DTMD_VERSION_MAJOR} )
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
		set_target_properties( dtmd-library++ PROPERTIES VERSION ${DTMD_VERSION} SOVERSION ${DTMD_VERSION_MAJOR} )
	endif (ENABLE_LIBVERSION)
	target_link_libraries( dtmd-library++ ${LIBRARY_CXX_LIBS} dtmd-library dtmd-misc )
endif (ENABLE_CXX)

if (BUILD_QT)
	if (${CLIENT_QT} EQUAL 4)
		include_directories (${QT_INCLUDE_DIR} ${QT_QTCORE_INCLUDE_DIR} ${QT_QTGUI_INCLUDE_DIR})
		QT4_WRAP_CPP(QT_CLIENT_MOC_SOURCES ${QT_CLIENT_QT_HEADERS})
		QT4_WRAP_UI(QT_CLIENT_UI_SOURCES ${QT_CLIENT_UI})
		QT4_ADD_RESOURCES(QT_CLIENT_RESOURCES ${QT_CLIENT_RES})
		set(DTMD_QT_LIBRARIES ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY})
	endif (${CLIENT_QT} EQUAL 4)

	if (${CLIENT_QT} EQUAL 5)
		include_directories (${Qt5Core_INCLUDE_DIR} ${Qt5Gui_INCLUDE_DIR} ${Qt5Widgets_INCLUDE_DIR})
		QT5_WRAP_CPP(QT_CLIENT_MOC_SOURCES ${QT_CLIENT_QT_HEADERS})
		QT5_WRAP_UI(QT_CLIENT_UI_SOURCES ${QT_CLIENT_UI})
		QT5_ADD_RESOURCES(QT_CLIENT_RESOURCES ${QT_CLIENT_RES})
		set(DTMD_QT_LIBRARIES ${Qt5Core_LIBRARIES} ${Qt5Gui_LIBRARIES} ${Qt5Widgets_LIBRARIES})
	endif (${CLIENT_QT} EQUAL 5)

	message(STATUS "Using Qt version ${CLIENT_QT}")

	add_executable (dtmd-qt ${QT_CLIENT_SOURCES} ${QT_CLIENT_QT_HEADERS} ${QT_CLIENT_HEADERS} ${QT_CLIENT_MOC_SOURCES} ${QT_CLIENT_UI_SOURCES} ${QT_CLIENT_RESOURCES})
	target_link_libraries (dtmd-qt dtmd-library++ ${QT_CLIENT_LIBS} ${DTMD_QT_LIBRARIES} )
endif (BUILD_QT)

ENABLE_TESTING()

set (TEST_SOURCES_decode_label daemon/label.c tests/decode_label_test.c tests/dt_tests.h)
set (TEST_LIBS_decode_label )

if (OS_LINUX)
	set (TEST_SOURCES_filesystem_opts daemon/filesystem_opts.c tests/filesystem_opts_test.c tests/dt_tests.h)
	set (TEST_LIBS_filesystem_opts dtmd-misc)
endif (OS_LINUX)

set (ALL_TESTS decode_label)

if (OS_LINUX)
	set (ALL_TESTS ${ALL_TESTS} filesystem_opts)
endif (OS_LINUX)

foreach (CURRENT_TEST ${ALL_TESTS})
	add_executable( ${CURRENT_TEST}_test ${TEST_SOURCES_${CURRENT_TEST}})
	target_link_libraries( ${CURRENT_TEST}_test ${TEST_LIBS_${CURRENT_TEST}} )
	add_test( ${CURRENT_TEST}_test ${CMAKE_CURRENT_BINARY_DIR}/${CURRENT_TEST}_test )
endforeach (CURRENT_TEST)

# installation config
install(TARGETS dtmd-misc    LIBRARY DESTINATION ${LIB_INSTALL_DIR} )
install(TARGETS dtmd-library LIBRARY DESTINATION ${LIB_INSTALL_DIR} )
install(TARGETS dtmd-daemon  RUNTIME DESTINATION ${SBIN_INSTALL_DIR} )
install(TARGETS dtmd-config  RUNTIME DESTINATION ${BIN_INSTALL_DIR} )

if (ENABLE_CONSOLE_CLIENT)
	install(TARGETS dtmd         RUNTIME DESTINATION ${BIN_INSTALL_DIR})
endif (ENABLE_CONSOLE_CLIENT)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/dtmd.h" DESTINATION "${INCLUDE_INSTALL_DIR}")
install(FILES "library/dtmd-misc.h"                DESTINATION "${INCLUDE_INSTALL_DIR}")
install(FILES "library/dtmd-library.h"             DESTINATION "${INCLUDE_INSTALL_DIR}")
install(FILES "daemon/dtmd.conf"                   DESTINATION "${CONFIG_DIR}")

if (ENABLE_CXX)
	install(TARGETS dtmd-library++ LIBRARY DESTINATION ${LIB_INSTALL_DIR} )
	install(FILES "library/dtmd-library++.hpp" DESTINATION "${INCLUDE_INSTALL_DIR}")
endif (ENABLE_CXX)

if (BUILD_QT)
	install(TARGETS dtmd-qt      RUNTIME DESTINATION ${BIN_INSTALL_DIR})
	install(FILES ${QT_IMAGES} DESTINATION "${DATA_PREFIX}/dtmd" )
endif (BUILD_QT)
