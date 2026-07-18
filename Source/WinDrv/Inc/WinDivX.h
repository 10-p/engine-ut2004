/*=============================================================================
	WinDivX.h: Movie encoding using DivX codec
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by Daniel Vogel.
=============================================================================*/

#ifndef _INC_WINDIVX
#define _INC_WINDIVX

/*-----------------------------------------------------------------------------
	FDivXEncoder.
-----------------------------------------------------------------------------*/

#if WITH_DIVX

class FDivXEncoder
{
public:
	FDivXEncoder( UViewport* Viewport, FString Filename, FLOAT Quality, INT Width, INT Height );
	~FDivXEncoder();
	void EncodeFrame( FColor* RawData );
	UBOOL IsInitialized() { return Initialized; }

protected:
	void SetProfile( SETTINGS* Settings, FLOAT Quality, INT Width, INT Height );

	UBOOL				Initialized;

	PAVIFILE			OutputFile;
	PAVISTREAM			OutputStream;
	BITMAPINFOHEADER	BitmapInfo;
	AVISTREAMINFO		StreamInfo;

	BYTE*				MovieBuffer;
	void*				EncoderHandle;
	UViewport*			Viewport;

	FLOAT				Quality;
	INT					FrameCount;
};


#else

// Dummy.
class FDivXEncoder
{
public:
	FDivXEncoder( UViewport* Viewport, FString Filename, FLOAT Quality, INT Width, INT Height ){}
	~FDivXEncoder(){}
	void EncodeFrame( FColor* RawData ){}

	UBOOL IsInitialized() { return 0; }
};

#endif


#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

