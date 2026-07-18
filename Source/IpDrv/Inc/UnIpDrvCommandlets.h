/*=============================================================================
	UnIpDrvCommandlets.h: IpDrv package commandlet declarations.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.

Revision history:
	* (4/14/99) Created by Brandon Reinhart
	* (4/15/99) Ported to UCC Commandlet interface by Brandon Reinhart

Todo:
	* Gspy style interface could be expanded to complete compliance
	  (allows for custom queries)
=============================================================================*/

#include "UnForcePacking_begin.h"

class UCompressCommandlet : public UCommandlet
{
	DECLARE_CLASS(UCompressCommandlet, UCommandlet, CLASS_Transient,IpDrv);

	INT Main( const TCHAR* Parms );
};

class UDecompressCommandlet : public UCommandlet
{
	DECLARE_CLASS(UDecompressCommandlet, UCommandlet, CLASS_Transient,IpDrv);

	INT Main( const TCHAR* Parms );
};

#include "UnForcePacking_end.h"

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

