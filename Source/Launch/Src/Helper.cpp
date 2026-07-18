/*=============================================================================
	Helper.cpp: ugly ugly ugly helper functions.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel.
=============================================================================*/

#define _WIN32_DCOM 1

#pragma pack(push,8)
#include <windows.h>
#include <wbemidl.h>
#include <comutil.h>
#pragma pack(pop)

// Relies on COM being initialized.
DWORD GetPrimaryAdapterVideoMemory()
{
#ifdef _WIN64
    return(0);
#else
	IWbemLocator*			Locator			= NULL;
	IWbemServices*			Services		= NULL;
	IEnumWbemClassObject*	InstanceEnum	= NULL;
	IWbemClassObject*		Instance		= NULL;

	if( FAILED( CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (void**)&Locator) ) )
		return 0;

	if( FAILED( Locator->ConnectServer(_bstr_t("root\\cimv2"), NULL, NULL, NULL, 0, NULL, NULL, &Services) ) )
	{
		Locator->Release();
		return 0;
	}
	else
		Locator->Release();

	if( FAILED( CoSetProxyBlanket(Services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE) ) )
		return 0;

	if( FAILED( Services->CreateInstanceEnum(_bstr_t("Win32_VideoController"), WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &InstanceEnum) ) )
	{
		Services->Release();
		return 0;
	}
	else
		Services->Release();

	ULONG NumObjects = 0;

	if( FAILED( InstanceEnum->Next(WBEM_INFINITE, 1, &Instance, &NumObjects) ) || NumObjects != 1 )
	{
		InstanceEnum->Release();
		return 0;
	}
		
	VARIANT v;
	VariantInit(&v);
	if( FAILED( Instance->Get(_bstr_t("AdapterRAM"), 0, &v, NULL, NULL) ) )
	{
		Instance->Release();
		InstanceEnum->Release();
		return 0;
	}
	else
	{
		VariantClear(&v);
		Instance->Release();
		InstanceEnum->Release();
		return V_UI4(&v);        
	}
#endif
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

