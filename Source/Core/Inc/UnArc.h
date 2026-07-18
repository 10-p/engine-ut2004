/*=============================================================================
	UnArc.h: Unreal archive class.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "Core.h"

/*-----------------------------------------------------------------------------
	Archive.
-----------------------------------------------------------------------------*/

//
// Archive class. Used for loading, saving, and garbage collecting
// in a byte order neutral way.
//
class CORE_API FArchive
{
public:
	// FArchive interface.
	virtual ~FArchive()
	{}
	virtual void Serialize( void* V, INT Length )
	{}
	virtual void SerializeBits( void* V, INT LengthBits )
	{
		Serialize( V, (LengthBits+7)/8 );
		if( IsLoading() )
			((BYTE*)V)[LengthBits/8] &= ((1<<(LengthBits&7))-1);
	}
	virtual void SerializeInt( DWORD& Value, DWORD Max )
	{
		ByteOrderSerialize( &Value, sizeof(Value) );
	}
	virtual void Preload( UObject* Object )
	{}
	virtual void CountBytes( SIZE_T InNum, SIZE_T InMax )
	{}
	virtual FArchive& operator<<( class FName& N )
	{
		return *this;
	}
	virtual FArchive& operator<<( class UObject*& Res )
	{
		return *this;
	}
	virtual INT MapName( FName* Name )
	{
		return 0;
	}
	virtual INT MapObject( UObject* Object )
	{
		return 0;
	}
	virtual INT Tell()
	{
		return INDEX_NONE;
	}
	virtual INT TotalSize()
	{
		return INDEX_NONE;
	}
	virtual UBOOL AtEnd()
	{
		INT Pos = Tell();
		return Pos!=INDEX_NONE && Pos>=TotalSize();
	}
	virtual UBOOL AtStopper()
	{
		INT Pos = Tell();
		return Pos!=INDEX_NONE && Stopper!=INDEX_NONE && Pos>=Stopper;
	}
	virtual void SetStopper( INT InStopper=INDEX_NONE )
	{
		Stopper = InStopper;
	}
	virtual void Seek( INT InPos )
	{}
	virtual void AttachLazyLoader( FLazyLoader* LazyLoader )
	{}
	virtual void DetachLazyLoader( FLazyLoader* LazyLoader )
	{}
	virtual void Precache( INT HintCount )
	{}
	virtual void Flush()
	{}
	virtual UBOOL Close()
	{
		return !ArIsError;
	}
	virtual UBOOL GetError()
	{
		return ArIsError;
	}

	// Hardcoded datatype routines that may not be overridden.
	FArchive& ByteOrderSerialize( void* V, INT Length )
	{
#if __INTEL_BYTE_ORDER__
		Serialize( V, Length );
#else
		if( ArIsPersistent )
		{
			// Transferring between memory and file, so flip the byte order.
			for( INT i=Length-1; i>=0; i-- )
				Serialize( (BYTE*)V + i, 1 );
		}
		else
		{
			// Transferring around within memory, so keep the byte order.
			Serialize( V, Length );
		}
#endif
		return *this;
	}

	void ThisContainsCode()	{ArContainsCode=true;}	// Sets that this package contains code
	// Constructor.
	FArchive()
	:	ArVer			(PACKAGE_FILE_VERSION)
	,	ArNetVer		(ENGINE_NEGOTIATION_VERSION)
	,	ArLicenseeVer	(PACKAGE_FILE_VERSION_LICENSEE)
	,	ArIsLoading		(0)
	,	ArIsSaving		(0)
	,   ArIsTrans		(0)
	,	ArIsPersistent  (0)
	,   ArIsError       (0)
	,	ArIsCriticalError(0)
	,	ArForEdit		(1)
	,	ArForClient		(1)
	,	ArForServer		(1)
	,	ArContainsCode  (0)
	,	Stopper			(INDEX_NONE)
	,	ArNetPackData	(1)
	,	ArMaxSerializeSize(0)
	{}

	// Status accessors.
	INT Ver()				{return ArVer;}
	INT NetVer()			{return ArNetVer&0x7fffffff;}
	INT LicenseeVer()		{return ArLicenseeVer;}
	UBOOL IsLoading()		{return ArIsLoading;}
	UBOOL IsSaving()		{return ArIsSaving;}
	UBOOL IsTrans()			{return ArIsTrans;}
	UBOOL IsNet()			{return (ArNetVer&0x80000000)!=0;}
	UBOOL IsPersistent()    {return ArIsPersistent;}
	UBOOL IsError()         {return ArIsError;}
	UBOOL IsCriticalError() {return ArIsCriticalError;}
	UBOOL ForEdit()			{return ArForEdit;}
	UBOOL ForClient()		{return ArForClient;}
	UBOOL ForServer()		{return ArForServer;}
	UBOOL ContainsCode()	{return ArContainsCode;}
	UBOOL NetPackData()		{return ArNetPackData;}

	// Friend archivers.
	CORE_API friend FArchive& operator<<( FArchive& Ar, ANSICHAR& C );
	CORE_API friend FArchive& operator<<( FArchive& Ar, BYTE& B );
	CORE_API friend FArchive& operator<<( FArchive& Ar, SBYTE& B );
	CORE_API friend FArchive& operator<<( FArchive& Ar, _WORD& W );
	CORE_API friend FArchive& operator<<( FArchive& Ar, SWORD& S );
	CORE_API friend FArchive& operator<<( FArchive& Ar, DWORD& D );
	CORE_API friend FArchive& operator<<( FArchive& Ar, INT& I );
	CORE_API friend FArchive& operator<<( FArchive& Ar, FLOAT& F );
	CORE_API friend FArchive& operator<<( FArchive& Ar, SQWORD& S );
	CORE_API friend FArchive& operator<<( FArchive &Ar, QWORD& Q );
	CORE_API friend FArchive& operator<<( FArchive& Ar, FString& A );

#ifndef __PSX2_EE__
	CORE_API friend FArchive& operator<<( FArchive& Ar, DOUBLE& F );
#endif

#ifdef __GNUC__
	CORE_API friend FArchive& operator<<( FArchive& Ar, PTRINT& P );
	CORE_API friend FArchive& operator<<( FArchive& Ar, UNICHAR& C );
#endif


protected:
	// Status variables.
	INT ArVer;
	INT ArNetVer;
	INT ArLicenseeVer;
	UBOOL ArIsLoading;
	UBOOL ArIsSaving;
	UBOOL ArIsTrans;
	UBOOL ArIsPersistent;
	UBOOL ArForEdit;
	UBOOL ArForClient;
	UBOOL ArForServer;
	UBOOL ArIsError;
	UBOOL ArIsCriticalError;
	UBOOL ArContainsCode;			// Quickly tell if an archive contains script code
	UBOOL ArNetPackData;
	INT ArMaxSerializeSize;

	// Prevent archive passing a set stopper position
	INT Stopper;
};

/*-----------------------------------------------------------------------------
	FArchive macros.
-----------------------------------------------------------------------------*/

//
// Class for serializing objects in a compactly, mapping small values
// to fewer bytes.
//
class CORE_API FCompactIndex
{
public:
	INT Value;
	CORE_API friend FArchive& operator<<( FArchive& Ar, FCompactIndex& I );
};

//
// Archive constructor.
//
template <class T> T Arctor( FArchive& Ar )
{
	T Tmp;
	Ar << Tmp;
	return Tmp;
}

// Macro to serialize an integer as a compact index.
#define AR_INDEX(intref) \
	(*(FCompactIndex*)&(intref))

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

