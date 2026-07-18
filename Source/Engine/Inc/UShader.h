/*=============================================================================
	UShader.h.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

//
// UMaterial interface
//

virtual INT MaterialUSize();
virtual INT MaterialVSize();
virtual UBOOL RequiresSorting();
virtual UBOOL IsTransparent();
virtual BYTE RequiredUVStreams();
virtual UBOOL RequiresNormal();
virtual void PreSetMaterial(FLOAT TimeSeconds);

virtual UBOOL CheckCircularReferences( TArray<UMaterial*>& History );
virtual void PostEditChange();
virtual	UMaterial* CheckFallback();
virtual UBOOL HasFallback();

