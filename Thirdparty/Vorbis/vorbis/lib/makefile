#=============================================================================
# Unreal vorbis makefile for non-Windows platforms.
#=============================================================================

# Unreal directory. (Required by makefile-header.)
UNREAL_DIR		= ../../..

# Include global definitions.
include $(UNREAL_DIR)/makefile-header

#-----------------------------------------------------------------------------
# Directories.
#-----------------------------------------------------------------------------

SRC_DIRS		= $(VORBIS_SRC)
INC_DIRS		= $(VORBIS_INC)
BUILD_DIR		= $(VORBIS_BUILD)

#-----------------------------------------------------------------------------
# Compiler/linker options.
#-----------------------------------------------------------------------------

CXX			= $(STD_CC)
CXXFLAGS		= $(STD_CXXFLAGS) $(EXT_CXXFLAGS) -I$(OGG_INC) -I$(VORBIS_INC) \
                    -DGPackage=GPackagevorbis -DThisPackage=vorbis
LDFLAGS			= $(STD_LDFLAGS) $(EXT_LDFLAGS)$(VORBIS_SONAME)
LIBS         = $(OGG)
AS			= $(STD_AS)
ASFLAGS			= $(STD_ASFLAGS)

#-----------------------------------------------------------------------------
# Files.
#-----------------------------------------------------------------------------

WINDEFFILE := $(call CONVPATH,$(SRC_DIRS)/../win32/vorbisall.def)

OBJS = \
	analysis.o \
    bitrate.o \
    block.o \
    codebook.o \
    envelope.o \
    floor0.o \
    floor1.o \
    info.o \
    lpc.o \
    lsp.o \
    mapping0.o \
    mdct.o \
    psy.o \
    registry.o \
    res0.o \
    sharedbook.o \
    smallft.o \
    synthesis.o \
    time0.o \
    vorbisfile.o \
    window.o

OUT = $(VORBIS)

#-----------------------------------------------------------------------------
# Rules.
#-----------------------------------------------------------------------------

# Use general rules.
include $(UNREAL_DIR)/makefile-general

#-----------------------------------------------------------------------------
# The End.
#-----------------------------------------------------------------------------
