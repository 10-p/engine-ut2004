/*=============================================================================
	UnArc.cpp: basic FArchive serializing friends.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Moved out of UnArc.h for ISO C++ compliance by Ryan C. Gordon.
=============================================================================*/

#include "CorePrivate.h"

// Friend archivers.

CORE_API FArchive& operator<<( FArchive& Ar, ANSICHAR& C )
{
	Ar.Serialize( &C, 1 );
	return Ar;
}

CORE_API FArchive& operator<<( FArchive& Ar, BYTE& B )
{
	Ar.Serialize( &B, 1 );
	return Ar;
}

CORE_API FArchive& operator<<( FArchive& Ar, SBYTE& B )
{
	Ar.Serialize( &B, 1 );
	return Ar;
}

CORE_API FArchive& operator<<( FArchive& Ar, _WORD& W )
{
	Ar.ByteOrderSerialize( &W, sizeof(W) );
	return Ar;
}

CORE_API FArchive& operator<<( FArchive& Ar, SWORD& S )
{
	Ar.ByteOrderSerialize( &S, sizeof(S) );
	return Ar;
}

CORE_API FArchive& operator<<( FArchive& Ar, DWORD& D )
{
	Ar.ByteOrderSerialize( &D, sizeof(D) );
	return Ar;
}

CORE_API FArchive& operator<<( FArchive& Ar, INT& I )
{
	Ar.ByteOrderSerialize( &I, sizeof(I) );
	return Ar;
}

CORE_API FArchive& operator<<( FArchive& Ar, SQWORD& S )
{
	Ar.ByteOrderSerialize( &S, sizeof(S) );
	return Ar;
}

CORE_API FArchive& operator<<( FArchive& Ar, FLOAT& F )
{
	Ar.ByteOrderSerialize( &F, sizeof(F) );
	return Ar;
}

#ifndef __PSX2_EE__
CORE_API FArchive& operator<<( FArchive& Ar, DOUBLE& F )
{
	Ar.ByteOrderSerialize( &F, sizeof(F) );
	return Ar;
}
#endif

#ifndef _WIN64   // apparently PTRINT covers this... --ryan.
CORE_API FArchive& operator<<( FArchive &Ar, QWORD& Q )
{
	Ar.ByteOrderSerialize( &Q, sizeof(Q) );
	return Ar;
}
#endif

#if ((!defined _WIN32) || (defined _WIN64))
CORE_API FArchive& operator<<( FArchive& Ar, PTRINT& P )
{
	Ar.ByteOrderSerialize( &P, sizeof(P) );
	return Ar;
}
#endif


#if defined(__GNUC__) && !defined(NO_UNICODE_OS_SUPPORT)
CORE_API FArchive& operator<<( FArchive& Ar, UNICHAR& C )
{
	Ar.ByteOrderSerialize( &C, sizeof(C) );
	return Ar;
}
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/


