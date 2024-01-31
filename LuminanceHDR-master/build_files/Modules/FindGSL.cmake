# - Find GSL
# Find the GNU Scientific Library (GSL) includes and library
#
# This module defines
#  GSL_FOUND
#  GSL_LIBRARIES for GSL only
#  GSLCBLAS_LIBRARIES for GSL CBLAS
#  GSL_INCLUDE_DIR
#  GSLCBLAS_INCLUDE_DIR (not yet defined)
#

FIND_PATH(GSL_INCLUDE_DIR gsl/gsl_rng.h
  /usr/local/include
  /usr/include
  /opt/local/include
)

FIND_PATH(GSLCBLAS_INCLUDE_DIR gsl/gsl_cblas.h
  /usr/local/include
  /usr/include
  /opt/local/include
)

IF(GSL_USE_STATIC_LIBS)
  SET( _GSL_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
  IF(WIN32)
    SET(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
  ELSE(WIN32)
    SET(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
  ENDIF(WIN32)
ENDIF(GSL_USE_STATIC_LIBS)

FIND_LIBRARY(GSL_LIBRARY NAMES gsl)
FIND_LIBRARY(GSL_CBLAS_LIBRARY NAMES gslcblas)

IF(GSL_USE_STATIC_LIBS)
  SET(CMAKE_FIND_LIBRARY_SUFFIXES ${_GSL_ORIG_CMAKE_FIND_LIBRARY_SUFFIXES})
ENDIF(GSL_USE_STATIC_LIBS)

IF(GSL_LIBRARY AND GSL_CBLAS_LIBRARY AND GSL_INCLUDE_DIR AND GSLCBLAS_INCLUDE_DIR)
  SET(GSL_LIBRARIES ${GSL_LIBRARY})
  SET(GSLCBLAS_LIBRARIES ${GSL_CBLAS_LIBRARY})
  SET(GSL_FOUND "YES")
ELSE()
  SET(GSL_FOUND "NO")
ENDIF()

IF(GSL_FOUND)
  IF(NOT GSL_FIND_QUIETLY)
    MESSAGE(STATUS "Found GNU Scientific Library (GSL_LIBRARY = ${GSL_LIBRARY})")
    MESSAGE(STATUS "Found GNU Scientific Library: (GSL_CBLAS_LIBRARY = ${GSL_CBLAS_LIBRARY})")
    MESSAGE(STATUS "Found GNU Scientific Library: (GSL_INCLUDE_DIR = ${GSL_INCLUDE_DIR})")
  ENDIF(NOT GSL_FIND_QUIETLY)
ELSE(GSL_FOUND)
  IF(GSL_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "Could not find GNU Scientific Library")
  ENDIF(GSL_FIND_REQUIRED)
ENDIF(GSL_FOUND)

MARK_AS_ADVANCED(
  GSL_LIBRARY
  GSL_CBLAS_LIBRARY
  GSL_INCLUDE_DIR
)
