project(Dark-Templar-Mount-Daemon)

cmake_minimum_required( VERSION 2.8.8 )

set(DTMD_VERSION_MAJOR 0)
set(DTMD_VERSION_MINOR 2)
set(DTMD_VERSION_PATCH 0)
set(DTMD_VERSION "${DTMD_VERSION_MAJOR}.${DTMD_VERSION_MINOR}.${DTMD_VERSION_PATCH}")

set(CPACK_PACKAGE_VERSION_MAJOR ${DTMD_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${DTMD_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${DTMD_VERSION_PATCH})
set(CPACK_GENERATOR "TBZ2")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_SOURCE_IGNORE_FILES
	".git;build;CMakeLists.txt.user;CMakeLists.txt.user.*;ignore;TODO")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${DTMD_VERSION}")
# MUST be after CPACK_* variables.
include(CPack)

# installation directory configuration
set(INST_PREFIX "/usr" CACHE PATH "Prefix prepended to install directories")
set(DATA_PREFIX "${INST_PREFIX}/share" CACHE PATH "Prefix prepended to data install directories")
set(HEADERS_PREFIX "${INST_PREFIX}/include" CACHE PATH "Prefix prepended to headers install directories")
set(CMAKE_INSTALL_PREFIX "${INST_PREFIX}" CACHE INTERNAL "Prefix prepended to install directories" FORCE)

message(STATUS "To change installation directory use -DINST_PREFIX=path, for example, -DINST_PREFIX=/usr")
message(STATUS "To change data installation directory use -DDATA_PREFIX=path")

add_definitions(-DDATA_PREFIX=\"${DATA_PREFIX}\")

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	message(STATUS "To disable external mount using, use -DDISABLE_EXT_MOUNT")
endif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

message(STATUS "System: ${CMAKE_SYSTEM_NAME}" )
if (${CMAKE_SIZEOF_VOID_P} EQUAL 8)
	message(STATUS "OS: 64 bit")
	set (INSTALL_DIR_LIB "lib64")
else (${CMAKE_SIZEOF_VOID_P} EQUAL 8)
	message(STATUS "OS: 32 bit")
	set (INSTALL_DIR_LIB "lib32")
endif (${CMAKE_SIZEOF_VOID_P} EQUAL 8)

if (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set (OS_LINUX true)
	add_definitions(-DOS=Linux)

	if (DEFINED DISABLE_EXT_MOUNT)
		add_definitions(-DDISABLE_EXT_MOUNT)
	endif (DEFINED DISABLE_EXT_MOUNT)

endif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
	set (OS_FREEBSD true)
	add_definitions(-DOS=FreeBSD)
endif (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")

if (OS_LINUX)
	message(STATUS "set LINUX_NO_UDEV to switch interface from udev to linux kernel")
endif (OS_LINUX)

message(STATUS "set DISABLE_CXX to disable dtmd-library++ library")
message(STATUS "set CLIENT_QT to force enable or disable Qt client")

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
	set (FORCE_QT 1)

	if ("${CLIENT_QT}" EQUAL 1)
		if (DEFINED DISABLE_CXX)
			message(FATAL_ERROR "Qt Client requires c++ library being enabled")
		endif (DEFINED DISABLE_CXX)

		set (BUILD_QT_CLIENT 1)
	endif ("${CLIENT_QT}" EQUAL 1)
endif (DEFINED CLIENT_QT)

if (UNIX)
	set( CMAKE_THREAD_PREFER_PTHREAD 1 )
endif (UNIX)
find_package (Threads REQUIRED)

add_definitions(-D_FILE_OFFSET_BITS=64)
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/library )

if (DEFINED FORCE_QT)
	if (DEFINED BUILD_QT_CLIENT)
		find_package(Qt4 REQUIRED COMPONENTS QtCore QtGui)
	endif (DEFINED BUILD_QT_CLIENT)
else (DEFINED FORCE_QT)
	find_package(Qt4 COMPONENTS QtCore QtGui)
	if (${QT_QTCORE_FOUND} AND ${QT_QTGUI_FOUND})
		set (BUILD_QT_CLIENT 1)
	endif (${QT_QTCORE_FOUND} AND ${QT_QTGUI_FOUND})
endif (DEFINED FORCE_QT)

set ( LINUX_UDEV_SOURCES daemon/linux/udev/udev.c )
set ( LINUX_UDEV_HEADERS )
set ( LINUX_UDEV_LIBS udev )

set ( LINUX_KERNEL_SOURCES daemon/linux/kernel/kernel.c )
set ( LINUX_KERNEL_HEADERS )
set ( LINUX_KERNEL_LIBS )

set ( MISC_LIBRARY_SOURCES library/dtmd-misc.c )
set ( MISC_LIBRARY_HEADERS library/dtmd-misc.h )

set ( LIBRARY_SOURCES library/dtmd-library.c )
set ( LIBRARY_HEADERS library/dtmd-library.h dtmd.h )
set ( LIBRARY_LIBS ${CMAKE_THREAD_LIBS_INIT} )

set ( LIBRARY_CXX_SOURCES library/dtmd-library++.cpp )
set ( LIBRARY_CXX_HEADERS library/dtmd-library++.hpp )
set ( LIBRARY_CXX_LIBS ${CMAKE_THREAD_LIBS_INIT} )

if (OS_LINUX)
	set ( LIBRARY_LIBS ${LIBRARY_LIBS} rt)
endif (OS_LINUX)

set ( DAEMON_SOURCES daemon/daemon-main.c daemon/actions.c daemon/filesystems.c daemon/label.c daemon/lists.c daemon/mnt_funcs.c )
set ( DAEMON_HEADERS dtmd.h               daemon/actions.h daemon/filesystems.h daemon/label.h daemon/lists.h daemon/mnt_funcs.h daemon/dtmd-internal.h daemon/system_module.h )
set ( DAEMON_LIBS )

set ( CONSOLE_CLIENT_SOURCES client/console/main.c )
set ( CONSOLE_CLIENT_HEADERS dtmd.h )
set ( CONSOLE_CLIENT_LIBS )

set ( QT_CLIENT_SOURCES client/qt/main.cpp client/qt/control.cpp client/qt/qcustomdeviceaction.cpp client/qt/qcustomstatefuldeviceaction.cpp)
set ( QT_CLIENT_QT_HEADERS                 client/qt/control.hpp client/qt/qcustomdeviceaction.hpp client/qt/qcustomstatefuldeviceaction.hpp)
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

if (OS_LINUX)
	if (LINUX_NO_UDEV)
		include(FindPkgConfig)

		pkg_check_modules(BLKID blkid REQUIRED)

		include_directories( ${BLKID_INCLUDE_DIRS} )

		set ( DAEMON_SOURCES ${DAEMON_SOURCES} ${LINUX_KERNEL_SOURCES} )
		set ( DAEMON_HEADERS ${DAEMON_HEADERS} ${LINUX_KERNEL_HEADERS} )
		set ( DAEMON_LIBS    ${DAEMON_LIBS}    ${LINUX_KERNEL_LIBS}    ${BLKID_LIBRARIES} )
		add_definitions(-DSUBSYSTEM_LINUX_KERNEL)
		message(STATUS "Linux Subsystem: kernel")
	else (LINUX_NO_UDEV)
		set ( DAEMON_SOURCES ${DAEMON_SOURCES} ${LINUX_UDEV_SOURCES} )
		set ( DAEMON_HEADERS ${DAEMON_HEADERS} ${LINUX_UDEV_HEADERS} )
		set ( DAEMON_LIBS    ${DAEMON_LIBS}    ${LINUX_UDEV_LIBS} )
		add_definitions(-DSUBSYSTEM_LINUX_UDEV)
		message(STATUS "Linux Subsystem: udev")
	endif (LINUX_NO_UDEV)
endif (OS_LINUX)

add_library( dtmd-misc SHARED ${MISC_LIBRARY_SOURCES} ${MISC_LIBRARY_HEADERS} )
target_link_libraries( dtmd-misc )

add_library( dtmd-library SHARED ${LIBRARY_SOURCES} ${LIBRARY_HEADERS} )
target_link_libraries( dtmd-library ${LIBRARY_LIBS} dtmd-misc )

add_executable( dtmd-daemon ${DAEMON_SOURCES} ${DAEMON_HEADERS})
target_link_libraries( dtmd-daemon ${DAEMON_LIBS} dtmd-misc )

add_executable( dtmd ${CONSOLE_CLIENT_SOURCES} ${CONSOLE_CLIENT_HEADERS})
target_link_libraries( dtmd dtmd-library ${CONSOLE_CLIENT_LIBS} )

if (NOT DEFINED DISABLE_CXX)
	add_library( dtmd-library++ SHARED ${LIBRARY_CXX_SOURCES} ${LIBRARY_CXX_HEADERS} )
	target_link_libraries( dtmd-library++ ${LIBRARY_CXX_LIBS} dtmd-library dtmd-misc )
endif (NOT DEFINED DISABLE_CXX)

if (DEFINED BUILD_QT_CLIENT)
	include_directories (${QT_INCLUDE_DIR} ${QT_QTCORE_INCLUDE_DIR} ${QT_QTGUI_INCLUDE_DIR})
	QT4_WRAP_CPP(QT_CLIENT_MOC_SOURCES ${QT_CLIENT_QT_HEADERS})
	QT4_WRAP_UI(QT_CLIENT_UI_SOURCES ${QT_CLIENT_UI})
	QT4_ADD_RESOURCES(QT_CLIENT_RESOURCES ${QT_CLIENT_RES})

	add_executable (dtmd-qt ${QT_CLIENT_SOURCES} ${QT_CLIENT_QT_HEADERS} ${QT_CLIENT_HEADERS} ${QT_CLIENT_MOC_SOURCES} ${QT_CLIENT_UI_SOURCES} ${QT_CLIENT_RESOURCES})
	target_link_libraries (dtmd-qt dtmd-library++ ${QT_CLIENT_LIBS} ${QT_QTCORE_LIBRARY} ${QT_QTGUI_LIBRARY} )
endif (DEFINED BUILD_QT_CLIENT)

ENABLE_TESTING()

set (TEST_SOURCES_commands tests/commands_test.c tests/dt_tests.h)
set (TEST_LIBS_commands dtmd-misc)

set (TEST_SOURCES_decode_label daemon/label.c tests/decode_label_test.c tests/dt_tests.h)
set (TEST_LIBS_decode_label )

set (ALL_TESTS commands decode_label)
foreach (CURRENT_TEST ${ALL_TESTS})
	add_executable( ${CURRENT_TEST}_test ${TEST_SOURCES_${CURRENT_TEST}})
	target_link_libraries( ${CURRENT_TEST}_test ${TEST_LIBS_${CURRENT_TEST}} )
	add_test( ${CURRENT_TEST}_test ${CMAKE_CURRENT_BINARY_DIR}/${CURRENT_TEST}_test )
endforeach (CURRENT_TEST)

# installation config
install(TARGETS dtmd-misc    LIBRARY DESTINATION ${INSTALL_DIR_LIB} )
install(TARGETS dtmd-library LIBRARY DESTINATION ${INSTALL_DIR_LIB} )
install(TARGETS dtmd-daemon  RUNTIME DESTINATION sbin)
install(TARGETS dtmd         RUNTIME DESTINATION bin)

install(FILES "dtmd.h" DESTINATION "${HEADERS_PREFIX}")
install(FILES "library/dtmd-misc.h" DESTINATION "${HEADERS_PREFIX}")
install(FILES "library/dtmd-library.h" DESTINATION "${HEADERS_PREFIX}")

if (NOT DEFINED DISABLE_CXX)
	install(TARGETS dtmd-library++ LIBRARY DESTINATION ${INSTALL_DIR_LIB} )
	install(FILES "library/dtmd-library++.hpp" DESTINATION "${HEADERS_PREFIX}")
endif (NOT DEFINED DISABLE_CXX)

if (DEFINED BUILD_QT_CLIENT)
	install(TARGETS dtmd-qt      RUNTIME DESTINATION bin)
	install(FILES ${QT_IMAGES} DESTINATION "${DATA_PREFIX}/dtmd" )
endif (DEFINED BUILD_QT_CLIENT)
