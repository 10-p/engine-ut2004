/*=============================================================================
	UModifier.h.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// UMaterial interface
virtual BYTE RequiredUVStreams() { return Material ? Material->RequiredUVStreams() : 1; }
virtual UBOOL RequiresSorting() { return Material ? Material->RequiresSorting() : 0; }
virtual UBOOL RequiresNormal() { return Material ? Material->RequiresNormal() : 0; }
virtual UBOOL IsTransparent() { return Material ? Material->IsTransparent() : 0; }
virtual INT MaterialUSize() { return Material ? Material->MaterialUSize() : 0; }
virtual INT MaterialVSize() { return Material ? Material->MaterialVSize() : 0; }
virtual void PreSetMaterial(FLOAT TimeSeconds) { if( Material ) Material->CheckFallback()->PreSetMaterial(TimeSeconds); }
virtual UBOOL CheckCircularReferences( TArray<UMaterial*>& History );
virtual void PostEditChange();

