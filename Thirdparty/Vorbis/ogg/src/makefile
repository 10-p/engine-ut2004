#=============================================================================
# Unreal ogg makefile for non-Windows platforms.
#=============================================================================

# Unreal directory. (Required by makefile-header.)
UNREAL_DIR		= ../../..

# Include global definitions.
include $(UNREAL_DIR)/makefile-header

#-----------------------------------------------------------------------------
# Directories.
#-----------------------------------------------------------------------------

SRC_DIRS		= $(OGG_SRC)
INC_DIRS		= $(OGG_INC)
BUILD_DIR		= $(OGG_BUILD)

#-----------------------------------------------------------------------------
# Compiler/linker options.
#-----------------------------------------------------------------------------

CXX			= $(STD_CC)
CXXFLAGS		= $(STD_CXXFLAGS) $(EXT_CXXFLAGS) -I$(OGG_INC) \
                    -DGPackage=GPackageogg -DThisPackage=ogg
LDFLAGS			= $(STD_LDFLAGS) $(EXT_LDFLAGS)$(OGG_SONAME)
AS			= $(STD_AS)
ASFLAGS			= $(STD_ASFLAGS)

#-----------------------------------------------------------------------------
# Files.
#-----------------------------------------------------------------------------

WINDEFFILE := $(SRC_DIRS)\..\win32\ogg.def

OBJS =				\
	bitwise.o \
	framing.o

OUT = $(OGG)

#-----------------------------------------------------------------------------
# Rules.
#-----------------------------------------------------------------------------

# Use general rules.
include $(UNREAL_DIR)/makefile-general

#-----------------------------------------------------------------------------
# The End.
#-----------------------------------------------------------------------------
