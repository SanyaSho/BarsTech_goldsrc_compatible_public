#-----------------------------------------------------------------------------
# CMAKELISTS.TXT
#
# SanyaSho (2025)
#-----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.20 FATAL_ERROR )

set( CMAKE_POSITION_INDEPENDENT_CODE ON )

project( GLQuake CXX C )

set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_VISIBILITY_PRESET hidden )

set_property( GLOBAL PROPERTY USE_FOLDERS ON )

set( SRCDIR "${CMAKE_CURRENT_LIST_DIR}" )
set( GAMEDIR "${SRCDIR}/game" )

link_directories(
	"${SRCDIR}/lib"

	"${SRCDIR}/lib/public"
)

include( "cmake/srcgrp.cmake" )

include( "cmake/base.cmake" )
include( "cmake/simpleinstall.cmake" )
include( "cmake/projects.cmake" )
