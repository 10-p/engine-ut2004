//=============================================================================
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================
#ifndef __XGAME_H__
#define __XGAME_H__

#include "Engine.h"
#define MAX_NAME_SIZE (INT)(512)
#define DECO_TEXT_MAX_SIZE (INT)(16 * 1024)		// Maximum size of the entire deco text content
#define DECO_TEXT_MAX_COLUMNS (INT)(1024 - 1)	// Maximum size of a single line of deco text
#include "XGameClasses.h"

#if __STATIC_LINK
#define NAMES_ONLY
#define NATIVE_DEFS_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "XGameClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVE_DEFS_ONLY
#undef NAMES_ONLY
#endif

#endif

