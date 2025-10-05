#-----------------------------------------------------------------------------
# FEATURE_FLAGS.CMAKE
#
# Add new features by adding a set_feature( MY_FEATURE ) below.
#
# Currently available features:
# HL25 - Enable compatibility with HL25
# HL25_WEBM_PLAYER - Enable WebM video support from HL25
#
# SanyaSho (2025)
#-----------------------------------------------------------------------------

#include_guard( GLOBAL )

function( set_feature featureflag )
	add_compile_definitions(
		"FEATURE_${featureflag}"
	)

	set( FEATURE_${featureflag} 1 PARENT_SCOPE )
endfunction()

# Use this function to check if feature is enabled (Example: is_feature_enabled( HL25 HL25 ))
function( is_feature_enabled featureflag retval )
	if( DEFINED FEATURE_${featureflag} )
		set( ${retval} 1 PARENT_SCOPE )
	else()
		set( ${retval} 0 PARENT_SCOPE )
	endif()
endfunction()

#set_feature( HL25 )
set_feature( HL25_WEBM_PLAYER )
