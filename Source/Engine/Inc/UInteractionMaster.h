/*=============================================================================
	UnInteractionMaster.h: Unreal UModel definition.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Joe Wilcox
=============================================================================*/

UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );
void DisplayCopyright(void);
int MasterProcessKeyType(EInputKey iKey, TCHAR Unicode );
int MasterProcessKeyEvent(EInputKey iKey, EInputAction State, FLOAT Delta );
void MasterProcessTick(float DeltaTime);
void MasterProcessPreRender(UCanvas* Canvas);
void MasterProcessPostRender(UCanvas* Canvas);
void MasterProcessMessage(const FString& Msg, FLOAT MsgLife);

