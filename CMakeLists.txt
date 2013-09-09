project(Dark-Templar-Mount-Daemon)

cmake_minimum_required( VERSION 2.8.8 )

set(DTMD_VERSION_MAJOR 0)
set(DTMD_VERSION_MINOR 1)
set(DTMD_VERSION_PATCH 0)
set(DTMD_VERSION "${DTMD_VERSION_MAJOR}.${DTMD_VERSION_MINOR}.${DTMD_VERSION_PATCH}")

set(CPACK_PACKAGE_VERSION_MAJOR ${DTMD_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${DTMD_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${DTMD_VERSION_PATCH})
set(CPACK_GENERATOR "TBZ2")
set(CPACK_SOURCE_GENERATOR "TBZ2")
set(CPACK_SOURCE_IGNORE_FILES
	".git;build;CMakeLists.txt.user;CMakeLists.txt.user.*")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${DTMD_VERSION}")
# MUST be after CPACK_* variables.
include(CPack)

# installation directory configuration
set(INST_PREFIX "/usr" CACHE PATH "Prefix prepended to install directories")
set(CMAKE_INSTALL_PREFIX "${INST_PREFIX}" CACHE INTERNAL "Prefix prepended to install directories" FORCE)

message(STATUS "To change installation directory use -DINST_PREFIX=path, for example, -DINST_PREFIX=/usr")

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

if (UNIX)
	set( CMAKE_THREAD_PREFER_PTHREAD 1 )
endif (UNIX)
find_package (Threads REQUIRED)

add_definitions(-D_FILE_OFFSET_BITS=64)
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )
include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/library )

set ( LINUX_UDEV_SOURCES daemon/linux/udev/udev.c )
set ( LINUX_UDEV_HEADERS daemon/linux/udev/udev.h )
set ( LINUX_UDEV_LIBS udev )

set ( MISC_LIBRARY_SOURCES library/dtmd-misc.c )
set ( MISC_LIBRARY_HEADERS library/dtmd-misc.h )

set ( LIBRARY_SOURCES library/dtmd-library.c )
set ( LIBRARY_HEADERS library/dtmd-library.h dtmd.h )
set ( LIBRARY_LIBS ${CMAKE_THREAD_LIBS_INIT} )

if (UNIX AND NOT APPLE)
	set ( LIBRARY_LIBS ${LIBRARY_LIBS} rt)
endif (UNIX AND NOT APPLE)

set ( DAEMON_SOURCES daemon/daemon-main.c daemon/actions.c daemon/filesystems.c daemon/lists.c daemon/mnt_funcs.c )
set ( DAEMON_HEADERS dtmd.h               daemon/actions.h daemon/filesystems.h daemon/lists.h daemon/mnt_funcs.h )
set ( DAEMON_LIBS )

set ( CONSOLE_CLIENT_SOURCES client/console/main.c )
set ( CONSOLE_CLIENT_HEADERS dtmd.h )
set ( CONSOLE_CLIENT_LIBS )

set ( LIBRARY_SOURCES ${LIBRARY_SOURCES} )
set ( LIBRARY_HEADERS ${LIBRARY_HEADERS} )

set ( DAEMON_SOURCES ${DAEMON_SOURCES} )
set ( DAEMON_HEADERS ${DAEMON_HEADERS} )

set ( DAEMON_SOURCES ${DAEMON_SOURCES} ${LINUX_UDEV_SOURCES} )
set ( DAEMON_HEADERS ${DAEMON_HEADERS} ${LINUX_UDEV_HEADERS} )
set ( DAEMON_LIBS    ${DAEMON_LIBS}    ${LINUX_UDEV_LIBS} )
add_definitions(-DSUBSYSTEM_LINUX_UDEV)

add_library( dtmd-misc SHARED ${MISC_LIBRARY_SOURCES} ${MISC_LIBRARY_HEADERS} )
target_link_libraries( dtmd-misc )

add_library( dtmd-library SHARED ${LIBRARY_SOURCES} ${LIBRARY_HEADERS} )
target_link_libraries( dtmd-library ${LIBRARY_LIBS} dtmd-misc )

add_executable( dtmd-daemon ${DAEMON_SOURCES} ${DAEMON_HEADERS})
target_link_libraries( dtmd-daemon ${DAEMON_LIBS} dtmd-misc )

add_executable( dtmd ${CONSOLE_CLIENT_SOURCES} ${CONSOLE_CLIENT_HEADERS})
target_link_libraries( dtmd dtmd-library ${CONSOLE_CLIENT_LIBS} )

ENABLE_TESTING()

set (TEST_SOURCES_commands tests/commands_test.c tests/dt_tests.h)
set (TEST_LIBS_commands dtmd-misc)

set (TEST_SOURCES_decode_label tests/decode_label_test.c tests/dt_tests.h)
set (TEST_LIBS_decode_label dtmd-misc)

set (ALL_TESTS commands decode_label)
foreach (CURRENT_TEST ${ALL_TESTS})
	add_executable( ${CURRENT_TEST}_test ${TEST_SOURCES_${CURRENT_TEST}})
	target_link_libraries( ${CURRENT_TEST}_test ${TEST_LIBS_${CURRENT_TEST}} )
	add_test( ${CURRENT_TEST}_test ${CMAKE_CURRENT_BINARY_DIR}/${CURRENT_TEST}_test )
endforeach (CURRENT_TEST)

# installation config
install(TARGETS dtmd-daemon RUNTIME DESTINATION ./sbin)
install(TARGETS dtmd        RUNTIME DESTINATION ./bin)
