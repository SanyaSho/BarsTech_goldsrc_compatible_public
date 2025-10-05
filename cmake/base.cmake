#-----------------------------------------------------------------------------
# BASE.CMAKE
#
# SanyaSho (2025)
#-----------------------------------------------------------------------------

set( IS_WINDOWS 0 )
set( IS_POSIX 0 )

if( WIN32 )
	set( IS_WINDOWS 1 )
elseif( POSIX )
	set( IS_POSIX 1 )
endif()

set( IS_RELEASE 0 )
set( IS_DEBUG 0 )

if ( "${CMAKE_BUILD_TYPE}" STREQUAL "Release" )
	set( IS_RELEASE 1 )
elseif ( "${CMAKE_BUILD_TYPE}" STREQUAL "Debug" )
	set( IS_DEBUG 1 )
endif()

if( ${IS_WINDOWS} )
	string( REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" )

	add_compile_options(
		/W3

		/permissive

		/MP

		/GR # Enable Run-Time Type Information
		/GF # Enable String Pooling
		/fp:fast # Floating Point Model
		/GS # Buffer Security Check

		$<${IS_RELEASE}:/Oi> # Enable Intrinsic Functions
		$<${IS_RELEASE}:/Ot> # Favor Fast Code
		$<${IS_RELEASE}:/Gy> # Enable Function-Level Linking

		/FC # Full path to source

		# Inline Function Expansion
		$<${IS_RELEASE}:/Ob2>
		$<${IS_DEBUG}:/Ob0>
	)

	add_link_options(
		$<$<CONFIG:Debug>:/DEBUG:FASTLINK>
		$<$<CONFIG:Release>:/DEBUG:FULL>
	)

	add_compile_definitions(
		USE_BREAKPAD_HANDLER
		_CRT_SECURE_NO_WARNINGS

		_ALLOW_RUNTIME_LIBRARY_MISMATCH
		_ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH
		_ALLOW_MSC_VER_MISMATCH
	)
endif()
