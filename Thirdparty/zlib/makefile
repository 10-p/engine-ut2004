#=============================================================================
# Unreal zlib makefile for non-Windows platforms.
#=============================================================================

# Unreal directory. (Required by makefile-header.)
UNREAL_DIR		= ..

# Include global definitions.
include $(UNREAL_DIR)/makefile-header

#-----------------------------------------------------------------------------
# Directories.
#-----------------------------------------------------------------------------

SRC_DIRS		= $(ZLIB_SRC)
INC_DIRS		= $(ZLIB_INC)
BUILD_DIR		= $(ZLIB_BUILD)

#-----------------------------------------------------------------------------
# Compiler/linker options.
#-----------------------------------------------------------------------------

CXX			= $(STD_CC)
CXXFLAGS		= $(STD_CXXFLAGS) $(EXT_CXXFLAGS) -I$(ZLIB_INC) \
                    -DGPackage=GPackagezlib -DThisPackage=zlib
LDFLAGS			= $(STD_LDFLAGS) $(EXT_LDFLAGS)$(ZLIB_SONAME)
AS			= $(STD_AS)
ASFLAGS			= $(STD_ASFLAGS)

#-----------------------------------------------------------------------------
# Files.
#-----------------------------------------------------------------------------

WINDEFFILE := $(SRC_DIRS)\zlibexports.def

OBJS =				\
	adler32.o		\
	compress.o		\
	crc32.o			\
	deflate.o		\
	infblock.o		\
	infcodes.o		\
	inffast.o		\
	inflate.o		\
	inftrees.o		\
	infutil.o		\
	trees.o			\
	uncompr.o		\
	zutil.o

OUT = $(ZLIB)

#-----------------------------------------------------------------------------
# Rules.
#-----------------------------------------------------------------------------

# Use general rules.
include $(UNREAL_DIR)/makefile-general

#-----------------------------------------------------------------------------
# The End.
#-----------------------------------------------------------------------------
