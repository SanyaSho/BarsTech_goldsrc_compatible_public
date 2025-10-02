#-----------------------------------------------------------------------------
# TIER1.CMAKE
#
# SanyaSho (2025)
#-----------------------------------------------------------------------------

cmake_minimum_required( VERSION 3.20 FATAL_ERROR )

set( TIER1_SOURCE_FILES )
BEGIN_SRC( TIER1_SOURCE_FILES "Source Files" )
	SRC_GRP(
		SOURCES
		"bitbuf.cpp"
		"byteswap.cpp"
		"CharacterSet.cpp"
		"checksum_crc.cpp"
		"checksum_md5.cpp"
		"ConVar.cpp"
		"DataManager.cpp"
		"Diff.cpp"
		"generichash.cpp"
		"interface.cpp"
		"JobThread.cpp"
		"KeyValues.cpp"
		"MemPool.cpp"
		"MemStack.cpp"
		"netadr.cpp"
		"processor_detect.cpp"
		"RangeCheckedVar.cpp"
		"StringPool.cpp"
		"strtools.cpp"
		"tier1.cpp"
		"TokenReader.cpp"
		"unicode_strtools.cpp"
		"UtlBuffer.cpp"
		"UtlString.cpp"
		"UtlSymbol.cpp"
		"xboxstubs.cpp"
	)
END_SRC( TIER1_SOURCE_FILES "Source Files" )

set( TIER1_HEADER_FILES )
BEGIN_SRC( TIER1_HEADER_FILES "Header Files" )
	SRC_GRP(
		SOURCES
		"${SRCDIR}/public/tier1/bitbuf.h"
		"${SRCDIR}/public/tier1/byteswap.h"
		"${SRCDIR}/public/tier1/CharacterSet.h"
		"${SRCDIR}/public/tier1/checksum_crc.h"
		"${SRCDIR}/public/tier1/checksum_md5.h"
		"${SRCDIR}/public/tier1/ConVar.h"
		"${SRCDIR}/public/tier1/DataManager.h"
		"${SRCDIR}/public/tier1/Diff.h"
		"${SRCDIR}/public/tier1/FmtStr.h"
		"${SRCDIR}/public/tier1/generichash.h"
		"${SRCDIR}/public/tier1/interface.h"
		"${SRCDIR}/public/tier1/JobThread.h"
		"${SRCDIR}/public/tier1/KeyValues.h"
		"${SRCDIR}/public/tier1/MemPool.h"
		"${SRCDIR}/public/tier1/MemStack.h"
		"${SRCDIR}/public/tier1/netadr.h"
		"${SRCDIR}/public/tier1/processor_detect.h"
		"${SRCDIR}/public/tier1/RangeCheckedVar.h"
		"${SRCDIR}/public/tier1/RefCount.h"
		"${SRCDIR}/public/tier1/SmartPtr.h"
		"${SRCDIR}/public/tier1/StringPool.h"
		"${SRCDIR}/public/tier1/strtools.h"
		"${SRCDIR}/public/tier1/tier1.h"
		"${SRCDIR}/public/tier1/TokenReader.h"
		"${SRCDIR}/public/tier1/UtlBidirectionalSet.h"
		"${SRCDIR}/public/tier1/UtlBuffer.h"
		"${SRCDIR}/public/tier1/UtlDict.h"
		"${SRCDIR}/public/tier1/UtlFixedLinkedList.h"
		"${SRCDIR}/public/tier1/UtlFixedMemory.h"
		"${SRCDIR}/public/tier1/UtlFlags.h"
		"${SRCDIR}/public/tier1/UtlHandleTable.h"
		"${SRCDIR}/public/tier1/UtlHash.h"
		"${SRCDIR}/public/tier1/UtlLinkedList.h"
		"${SRCDIR}/public/tier1/UtlMap.h"
		"${SRCDIR}/public/tier1/UtlMemory.h"
		"${SRCDIR}/public/tier1/UtlMultiList.h"
		"${SRCDIR}/public/tier1/UtlNTree.h"
		"${SRCDIR}/public/tier1/UtlPriorityQueue.h"
		"${SRCDIR}/public/tier1/UtlQueue.h"
		"${SRCDIR}/public/tier1/UtlRBTree.h"
		"${SRCDIR}/public/tier1/UtlSortVector.h"
		"${SRCDIR}/public/tier1/UtlStack.h"
		"${SRCDIR}/public/tier1/UtlString.h"
		"${SRCDIR}/public/tier1/UtlStringMap.h"
		"${SRCDIR}/public/tier1/UtlSymbol.h"
		"${SRCDIR}/public/tier1/UtlVector.h"
	)
END_SRC( TIER1_HEADER_FILES "Header Files" )

add_library(
	tier1 STATIC
	${TIER1_SOURCE_FILES} ${TIER1_HEADER_FILES}
)

target_include_directories(
	tier1 PRIVATE

	"${SRCDIR}" # FIXME(SanyaSho): Some headers want to include "public/"

	"${SRCDIR}/public"
	"${SRCDIR}/public/tier1"
	"${SRCDIR}/public/tier0"
)

target_compile_definitions(
	tier1 PRIVATE

	MINMAX
	ALTERNATE_LE
)
