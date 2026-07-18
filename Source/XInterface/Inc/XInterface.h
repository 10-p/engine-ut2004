//=============================================================================
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#ifndef __XINTERFACE_H__
#define __XINTERFACE_H__

#include "../../IpDrv/Inc/UnIpDrv.h"
#include "../../XGame/Inc/XGame.h"

#ifndef XINTERFACE_API
#define XINTERFACE_API DLL_IMPORT
#endif

extern "C" {
    void RegisterNamesXInterface(void);
}


XINTERFACE_API void   GUIappSprintf( INT MaxLen, TCHAR* Dest, const TCHAR* Fmt, ... );

#include "XInterfaceClasses.h"

#if __STATIC_LINK
#define NAMES_ONLY
#define NATIVE_DEFS_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "XInterfaceClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVE_DEFS_ONLY
#undef NAMES_ONLY
#endif

#ifndef INVALIDMENU
#define INVALIDMENU ( Controller == NULL || (MenuOwner == NULL && !this->IsA(UGUIPage::StaticClass())) )
#endif

#ifndef INVALIDRENDER
#define INVALIDRENDER ( (bRequiresStyle && Style == NULL) || (Canvas == NULL) || INVALIDMENU )
#endif

#ifndef DECLAREBOUNDS
#define DECLAREBOUNDS FLOAT AW(ActualWidth()), AL(ActualLeft()), AH(ActualHeight()), AT(ActualTop())
#endif

#endif