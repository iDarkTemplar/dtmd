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
	set (CMAKE_C_FLAGS_RELEASE "-O2 -pipe -march=native -Wall -Wextra")
	set (CMAKE_CXX_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})

	set (CMAKE_C_FLAGS_DEBUG "-O0 -pipe -march=native -Wall -Wextra -g -ggdb")
	set (CMAKE_CXX_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})
endif (CMAKE_COMPILER_IS_GNUCC)

# clang compiler options
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	set (CLANG_WARNINGS "-Weverything -Wno-missing-prototypes -Wno-padded -Wno-sign-conversion -Wno-conversion -Wno-disabled-macro-expansion -Wno-cast-align" )

	set (CMAKE_C_FLAGS_RELEASE "-O3 -pipe ${CLANG_WARNINGS}")
	set (CMAKE_CXX_FLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE})

	set (CMAKE_C_FLAGS_DEBUG "-O0 -pipe ${CLANG_WARNINGS} -g -ggdb")
	set (CMAKE_CXX_FLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG})
endif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

add_definitions(-D_FILE_OFFSET_BITS=64)
include_directories( ${CMAKE_CURRENT_SOURCE_DIR} )

set ( DAEMON_SOURCES daemon-main.c lists.c )
set ( DAEMON_HEADERS dtmd.h        lists.h )

add_executable( dtmd-daemon ${DAEMON_SOURCES} ${DAEMON_HEADERS})
target_link_libraries( dtmd-daemon udev )

# installation config
install(TARGETS dtmd-daemon RUNTIME DESTINATION ./sbin)
