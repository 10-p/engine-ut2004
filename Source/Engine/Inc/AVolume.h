/*=============================================================================
	AVolume.h: Class functions residing in the AVolume class.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

INT Encompasses(FVector point);
void SetVolumes(const TArray<class AVolume*>& Volumes);
void SetVolumes();
UBOOL ShouldTrace(AActor *SourceActor, DWORD TraceFlags);
virtual void PostBeginPlay();
virtual UBOOL IsAVolume() {return true;}

