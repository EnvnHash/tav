# - Find NiTe2 - this file should be located in the CMAKE_MODULE_PATH
# Find NiTe2 headers and libraries
#
#  NITE2_INCLUDE_DIRS - where to find NiTe.h, etc.
#  NITE2_LIBRARIES    - List of libraries when using NiTe.
#  NITE2_FOUND        - True if NiTe found.
#=============================================================================

find_path( NITE2_INCLUDE_DIR
	NAMES NiTE.h
        HINTS
		/usr/local/include/Nite2
	PATHS $ENV{NITE2_INCLUDE}
	)

find_path( NITE2_LIBRARY_DIR
	NAMES NiTE.ini
        HINTS
		/usr/local/include/Nite2
	PATHS $ENV{NITE2_LIRBARY_DIR}
	)

if( WIN32 )
	find_path( OPENNI2_DLL NAMES Kinect.dll
		PATHS $ENV{OPENNI2_REDIST}/OpenNI2/Drivers
	)
endif()

find_library( NITE2_LIBRARY
	NAMES NiTE2
	HINTS   /usr/local/lib/Nite2
	PATHS
		$ENV{NITE2_LIB}
		$ENV{NITE2_REDIST64}
	)



# handle the QUIETLY and REQUIRED arguments and set NITE2_FOUND to TRUE if
# all listed variables are TRUE
include( FindPackageHandleStandardArgs )
FIND_PACKAGE_HANDLE_STANDARD_ARGS( NITE2 REQUIRED_VARS NITE2_LIBRARY NITE2_INCLUDE_DIR NITE2_LIBRARY_DIR )

# Copy the results to the output variables.
if( NITE2_FOUND )
  set( NITE2_LIBRARIES ${NITE2_LIBRARY} )
  set( NITE2_INI_DIR ${NITE2_LIBRARY_DIR} )
  set( NITE2_INCLUDE_DIRS ${NITE2_INCLUDE_DIR} )
  if( WIN32 )
    set( NITE2_OPENNI2_DRIVER_DIRS ${OPENNI2_DLL} )
  endif()
endif()

mark_as_advanced( NITE2_INCLUDE_DIR NITE2_LIBRARY NIT2_LIBRARY_DIR )

if( WIN32 )
	mark_as_advanced( OPENNI2_DLL )
endif()
