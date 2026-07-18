#=============================================================================
# Unreal speex makefile for non-Windows platforms.
#=============================================================================

# Unreal directory. (Required by makefile-header.)
UNREAL_DIR		= ../../..

# Include global definitions.
include $(UNREAL_DIR)/makefile-header

#-----------------------------------------------------------------------------
# Directories.
#-----------------------------------------------------------------------------

SRC_DIRS		= $(SPEEX_SRC)
INC_DIRS		= $(SPEEX_INC)
BUILD_DIR		= $(SPEEX_BUILD)

#-----------------------------------------------------------------------------
# Compiler/linker options.
#-----------------------------------------------------------------------------

CXX			= $(STD_CC)
CXXFLAGS		= $(STD_CXXFLAGS) $(EXT_CXXFLAGS) -I$(SPEEX_INC) \
                    -DGPackage=GPackagespeex -DThisPackage=speex -DEPIC_48K=1
LDFLAGS			= $(STD_LDFLAGS) $(EXT_LDFLAGS)$(SPEEX_SONAME)
AS			= $(STD_AS)
ASFLAGS			= $(STD_ASFLAGS)

#-----------------------------------------------------------------------------
# Files.
#-----------------------------------------------------------------------------

WINDEFFILE := $(SRC_DIRS)\speex.def

OBJS =				\
	nb_celp.o \
	sb_celp.o \
	lpc.o \
	ltp.o \
	lsp.o \
	quant_lsp.o \
	lsp_tables_nb.o \
	gain_table.o \
	gain_table_lbr.o \
	cb_search.o \
	filters.o \
	bits.o \
	modes.o \
	vq.o \
	high_lsp_tables.o \
	vbr.o \
	hexc_table.o \
	exc_5_256_table.o \
	exc_5_64_table.o \
	exc_8_128_table.o \
	exc_10_32_table.o \
	exc_10_16_table.o \
	exc_20_32_table.o \
	hexc_10_32_table.o \
	misc.o \
	speex_header.o \
	speex_callbacks.o \
	math_approx.o \
	stereo.o \
	preprocess.o \
	smallft.o \
	lbr_48k_tables.o \
	jitter.o \
	mdf.o

OUT = $(SPEEX)

#-----------------------------------------------------------------------------
# Rules.
#-----------------------------------------------------------------------------

# Use general rules.
include $(UNREAL_DIR)/makefile-general

#-----------------------------------------------------------------------------
# The End.
#-----------------------------------------------------------------------------

