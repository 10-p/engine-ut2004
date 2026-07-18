/*=============================================================================
	UFinalBlend.h.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// UMaterial interface
virtual UBOOL RequiresSorting();
virtual void PostEditChange();
virtual UBOOL IsTransparent();
virtual UBOOL GetValidated();
virtual void SetValidated( UBOOL InValidated );

