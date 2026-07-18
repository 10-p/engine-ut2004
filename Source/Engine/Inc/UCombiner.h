/*=============================================================================
	UCombiner.h.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

//
// UMaterial interface
//

//!!MAT handle texcoord scaling
virtual INT MaterialUSize() { return Max( Material1?Material1->MaterialUSize():0, Material2?Material2->MaterialUSize():0 ); }
virtual INT MaterialVSize() { return Max( Material1?Material1->MaterialVSize():0, Material2?Material2->MaterialVSize():0 ); }
virtual UBOOL RequiresSorting() { return 0; }
virtual UBOOL IsTransparent() { return 0; }
virtual BYTE RequiredUVStreams() { return (Material1 ? Material1->RequiredUVStreams() : 1) | (Material2 ? Material2->RequiredUVStreams() : 1); }
virtual UBOOL RequiresNormal() { return (Material1 ? Material1->RequiresNormal() : 0) | (Material2 ? Material2->RequiresNormal() : 0); }
virtual UBOOL CheckCircularReferences( TArray<UMaterial*>& History );
virtual void PostEditChange();
virtual void PreSetMaterial(FLOAT TimeSeconds);

