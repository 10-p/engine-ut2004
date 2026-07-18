/*=============================================================================
	UnRenderResource.cpp: Render resource implementation.
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(URenderResource);
IMPLEMENT_CLASS(UVertexStreamBase);
IMPLEMENT_CLASS(UVertexStreamVECTOR);
IMPLEMENT_CLASS(UVertexStreamCOLOR);
IMPLEMENT_CLASS(UVertexStreamUV);
IMPLEMENT_CLASS(UVertexStreamPosNormTex);
IMPLEMENT_CLASS(UVertexBuffer);
IMPLEMENT_CLASS(UIndexBuffer);
IMPLEMENT_CLASS(USkinVertexBuffer);

//	URenderResource
void URenderResource::Serialize(FArchive& Ar)
{
	guard(URenderResource::Serialize);

	Super::Serialize(Ar);

	Ar << Revision;

	unguard;
}


// UIndexBuffer
void UIndexBuffer::Serialize(FArchive& Ar)
{
	guard(UIndexBuffer::Serialize);

	Super::Serialize(Ar);

	Ar << Indices;

	unguard;
}


// USkinVertexBuffer
void USkinVertexBuffer::Serialize(FArchive& Ar)
{
	guard(UIndexBuffer::Serialize);

	Super::Serialize(Ar);

	Ar << Vertices;

	unguard;
}

