/*=============================================================================
	UInteraction.h: Unreal UModel definition.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Joe Wilcox
=============================================================================*/

void  virtual NativeMessage(const FString Msg, FLOAT MsgLife);
UBOOL virtual NativeKeyType(BYTE& iKey, TCHAR Unicode );
UBOOL virtual NativeKeyEvent(BYTE& iKey, BYTE& State, FLOAT Delta );
void  virtual NativeTick(FLOAT DeltaTime);
void  virtual NativePreRender(class UCanvas* Canvas);
void  virtual NativePostRender(class UCanvas* Canvas);

