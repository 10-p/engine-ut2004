//=============================================================================
// XInterface - Native Interface Package
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#include "XInterface.h"

IMPLEMENT_PACKAGE(XInterface);

#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) XINTERFACE_API FName XINTERFACE_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "XInterfaceClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

// sjs --- import natives
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "XInterfaceClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVES_ONLY
#undef NAMES_ONLY
// --- sjs


void RegisterNamesXInterface()
{
    #define NAMES_ONLY
	#define AUTOGENERATE_NAME(name) extern XINTERFACE_API FName XINTERFACE_##name; XINTERFACE_##name=FName(TEXT(#name),FNAME_Intrinsic);
	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "XInterfaceClasses.h"
	#undef DECLARE_NAME
	#undef NAMES_ONLY
}

#if !__STATIC_LINK
    struct FXInterfaceInitManager
    {
        FXInterfaceInitManager()
        {
            RegisterNamesXInterface();
        }
    } XInterfaceInitManager;
#endif

