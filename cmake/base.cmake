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

if( WIN32 )
	set( _DLL_EXT ".dll" )
elseif( LINUX )
	set( _DLL_EXT ".so" )
endif()

add_compile_definitions(
	_DLL_EXT=${_DLL_EXT}
)

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

		$<$<CONFIG:Release>:/Oi> # Enable Intrinsic Functions
		$<$<CONFIG:Release>:/Ot> # Favor Fast Code
		$<$<CONFIG:Release>:/Gy> # Enable Function-Level Linking

		/FC # Full path to source

		# Inline Function Expansion
		$<$<CONFIG:Release>:/Ob2>
		$<$<CONFIG:Debug>:/Ob0>
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
