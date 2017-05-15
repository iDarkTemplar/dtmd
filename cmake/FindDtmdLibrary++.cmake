# - Try to find Dtmd-Library++ library
# Once done this will define
#  DTMDLIBRARY++_FOUND - System has Dtmd-Library++
#  DTMDLIBRARY++_INCLUDE_DIRS - The Dtmd-Library++ include directories
#  DTMDLIBRARY++_LIBRARIES - The libraries needed to use Dtmd-Library++

if (DtmdLibrary++_FIND_REQUIRED)
	find_package(DtmdLibrary REQUIRED)
else (DtmdLibrary++_FIND_REQUIRED)
	find_package(DtmdLibrary)
endif (DtmdLibrary++_FIND_REQUIRED)

if (DTMDLIBRARY_FOUND)
	find_path(DTMDLIBRARY++_INCLUDE_DIR dtmd-library++.hpp)

	find_library(DTMDLIBRARY++_LIBRARY NAMES dtmd-library++ libdtmd-library++)

	set(DTMDLIBRARY++_LIBRARIES ${DTMDLIBRARY++_LIBRARY})
	set(DTMDLIBRARY++_INCLUDE_DIRS ${DTMDLIBRARY++_INCLUDE_DIR})

	include(FindPackageHandleStandardArgs)
	# handle the QUIETLY and REQUIRED arguments and set DTMDLIBRARY++_FOUND to TRUE
	# if all listed variables are TRUE
	find_package_handle_standard_args(DtmdLibrary++ DEFAULT_MSG
	                                  DTMDLIBRARY++_LIBRARY DTMDLIBRARY++_INCLUDE_DIR)

	mark_as_advanced(DTMDLIBRARY++_INCLUDE_DIR DTMDLIBRARY++_LIBRARY)
else (DTMDLIBRARY_FOUND)
	set(DTMDLIBRARY++_FOUND FALSE)
endif (DTMDLIBRARY_FOUND)
