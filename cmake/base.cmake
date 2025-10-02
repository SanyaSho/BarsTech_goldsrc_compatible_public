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

if( ${IS_WINDOWS} )
	string( REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" )

	add_compile_options(
		/W3
	)

	add_compile_definitions(
		USE_BREAKPAD_HANDLER
		_CRT_SECURE_NO_WARNINGS
	)
endif()