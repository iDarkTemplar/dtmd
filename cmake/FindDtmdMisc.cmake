# - Try to find Dtmd-Misc library
# Once done this will define
#  DTMDMISC_FOUND - System has Dtmd-Misc
#  DTMDMISC_INCLUDE_DIRS - The Dtmd-Misc include directories
#  DTMDMISC_LIBRARIES - The libraries needed to use Dtmd-Misc

find_path(DTMDMISC_CORE_INCLUDE_DIR dtmd.h)

find_path(DTMDMISC_INCLUDE_DIR dtmd-misc.h)

find_library(DTMDMISC_LIBRARY NAMES dtmd-misc libdtmd-misc)

set(DTMDMISC_LIBRARIES ${DTMDMISC_LIBRARY})
set(DTMDMISC_INCLUDE_DIRS ${DTMDMISC_INCLUDE_DIR} ${DTMDMISC_CORE_INCLUDE_DIR})

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set DTMDMISC_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(DtmdMisc DEFAULT_MSG
                                  DTMDMISC_LIBRARY DTMDMISC_CORE_INCLUDE_DIR DTMDMISC_INCLUDE_DIR)

mark_as_advanced(DTMDMISC_INCLUDE_DIR DTMDMISC_CORE_INCLUDE_DIR DTMDMISC_LIBRARY)
