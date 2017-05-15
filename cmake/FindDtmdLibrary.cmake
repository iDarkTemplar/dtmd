# - Try to find Dtmd-Library library
# Once done this will define
#  DTMDLIBRARY_FOUND - System has Dtmd-Library
#  DTMDLIBRARY_INCLUDE_DIRS - The Dtmd-Library include directories
#  DTMDLIBRARY_LIBRARIES - The libraries needed to use Dtmd-Library

if (DtmdLibrary_FIND_REQUIRED)
	find_package(DtmdMisc REQUIRED)
else (DtmdLibrary_FIND_REQUIRED)
	find_package(DtmdMisc)
endif (DtmdLibrary_FIND_REQUIRED)

if (DTMDMISC_FOUND)
	find_path(DTMDLIBRARY_INCLUDE_DIR dtmd-library.h)

	find_library(DTMDLIBRARY_LIBRARY NAMES dtmd-library libdtmd-library)

	set(DTMDLIBRARY_LIBRARIES ${DTMDLIBRARY_LIBRARY})
	set(DTMDLIBRARY_INCLUDE_DIRS ${DTMDLIBRARY_INCLUDE_DIR})

	include(FindPackageHandleStandardArgs)
	# handle the QUIETLY and REQUIRED arguments and set DTMDLIBRARY_FOUND to TRUE
	# if all listed variables are TRUE
	find_package_handle_standard_args(DtmdLibrary DEFAULT_MSG
	                                  DTMDLIBRARY_LIBRARY DTMDLIBRARY_INCLUDE_DIR)

	mark_as_advanced(DTMDLIBRARY_INCLUDE_DIR DTMDLIBRARY_LIBRARY)
else (DTMDMISC_FOUND)
	set (DTMDLIBRARY_FOUND FALSE)
endif (DTMDMISC_FOUND)
