/*=============================================================================
	WinDivX.cpp: Movie encoding using DivX codec
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by Daniel Vogel.
=============================================================================*/

#include "WinDrv.h"

#if WITH_DIVX

#ifdef WIN32
#pragma comment ( lib, "VFW32.lib" )
#pragma comment ( lib, "encore.lib" )
#pragma comment ( lib, "preprocess.lib" )
#pragma comment ( lib, "decore.lib" )
#endif


// In theory the codec shouldn't use more than 6 * 1280 * 720 though better safe than sorry.
static const INT ENCODE_BUFFER_LEN = 7 * 1280 * 720; 

// Hooks to get static linking to work.
extern "C" PVOID g_hResourceHandle = 0;
extern const bool g_bAllowOfflineActivation=false;
extern "C" void do_bgregister(){}

//
//	FDivXEncoder::FDivXEncoder.
//
FDivXEncoder::FDivXEncoder( UViewport* InViewport, FString Filename, FLOAT Quality, INT Width, INT Height ) :
	OutputFile( NULL ),
	OutputStream( NULL ),
	MovieBuffer( NULL ),
	EncoderHandle( NULL ),
	Viewport( InViewport ),
	FrameCount( 0 ),
	Initialized( 0 )
{
	guard(FDivXEncoder::FDivXEncoder);

	HRESULT hr;
	
	AVIFileInit();

	if( FAILED( hr=AVIFileOpen( &OutputFile, *Filename, OF_CREATE|OF_WRITE, NULL ) ) )
	{
		debugf(TEXT("DIVX: Can't create '%s'"), *Filename);
		return;
	} 

	SETTINGS Settings;
	SetProfile( &Settings, Quality, Width, Height );

	appMemzero( &BitmapInfo, sizeof(BITMAPINFOHEADER) );
	BitmapInfo.biSize			= sizeof(BITMAPINFOHEADER);
	
	// Encoded format.
	BitmapInfo.biWidth			= Settings.resize_width;
	BitmapInfo.biHeight			= Settings.resize_height;
	BitmapInfo.biPlanes			= 1;
	BitmapInfo.biBitCount		= 24;
	BitmapInfo.biCompression	= mmioFOURCC('D', 'I', 'V', 'X');

	// Stream info.
	appMemzero( &StreamInfo, sizeof(AVISTREAMINFO) );
	StreamInfo.fccType			= streamtypeVIDEO;
	StreamInfo.fccHandler		= mmioFOURCC('D', 'I', 'V', 'X');
	StreamInfo.dwRate			= 30;
	StreamInfo.dwScale			= 1;

	if( FAILED( hr=AVIFileCreateStream( OutputFile, &OutputStream, &StreamInfo ) ) )
	{
		debugf(TEXT("DIVX: Stream creation failed."));
		return;
	}

	if( FAILED( hr=AVIStreamSetFormat( OutputStream, 0, &BitmapInfo, sizeof(BitmapInfo)) ) )
	{
		debugf(TEXT("DIVX: Setting stream format failed."));
		return;
	}

	if( ENCORE_VERSION > encore(NULL, ENC_OPT_VERSION, NULL, NULL) )
		appErrorf(TEXT("DIVX: Incompatible interfaces detected."));

	// Raw format.
	BitmapInfo.biWidth			= Viewport->SizeX;
	BitmapInfo.biHeight			= Viewport->SizeY;
	BitmapInfo.biPlanes			= 1;
	BitmapInfo.biBitCount		= 32;
	BitmapInfo.biCompression	= 0;

	if( ENC_OK != (hr=encore(&EncoderHandle, ENC_OPT_INIT, &BitmapInfo, &Settings)) )
	{
		debugf(TEXT("DIVX: Encoder initialization failed with error code %i"), hr );
		appErrorf(TEXT("DIVX: Encoder initialization failed with error code %i"), hr );
		return;
	}

	MovieBuffer = new BYTE[ENCODE_BUFFER_LEN];

	Initialized = 1;

	unguard;
};


//
//	FDivXEncoder::~FDivXEncoder.
//
FDivXEncoder::~FDivXEncoder()
{
	guard(FDivXEncoder::~FDivXEncoder);

	if( OutputStream )
		AVIStreamRelease(OutputStream);
	
	if( OutputFile )
		AVIFileRelease(OutputFile);	

	AVIFileExit();

	if( EncoderHandle )		
		encore( EncoderHandle, ENC_OPT_RELEASE, 0, 0);

	delete [] MovieBuffer;
	
	unguard;
};


//
//	FDivXEncoder::EncodeFrame.
//
void FDivXEncoder::EncodeFrame( FColor* RawData )
{
	guard(FDivXEncoder::EncodeFrame);

	ENC_FRAME	Frame;
	ENC_RESULT	Result;
	HRESULT		hr;
	DWORD		dwKeyFrame = 0;

	appMemzero( &Frame, sizeof(Frame) );
	Frame.image					= RawData;
	Frame.bitstream				= MovieBuffer;
	Frame.produce_empty_frame	= 0;
	Frame.timestamp				= FrameCount;

	do 
	{
		if( ENC_OK == (hr=encore(EncoderHandle, ENC_OPT_ENCODE, &Frame, &Result)) )
		{
			if( Frame.length && FAILED( hr=AVIStreamWrite( OutputStream, FrameCount, 1, MovieBuffer, (long) Frame.length, dwKeyFrame, NULL, NULL ) ) )
				debugf(TEXT("DIVX: Error while writing to stream."));
		}
		else
			debugf(TEXT("DIVX: Encode Error: %i"),hr);

		Frame.image	= NULL;
	} 
	while( (Frame.length > 0) && (hr == ENC_OK) );
	
	FrameCount++;

	unguard;
};

//
//	FDivXEncoder::SetProfile.
//
void FDivXEncoder::SetProfile( SETTINGS* Settings, FLOAT Quality, INT Width, INT Height )
{
	guard(FDivXEncoder::SetProfile);

	appMemzero( Settings, sizeof(SETTINGS) );

	Width	= Clamp( Width, 160, 1280 );
	Height	= Clamp( Height, 120, 720 );

	// Set kind of sane values.
	Settings->vbr_mode				= RCMODE_1PASS_CONSTANT_Q;
	Settings->quantizer				= Clamp( 31.f - Quality * 30.f, 1.f, 31.f );
	Settings->use_bidirect			= 1;

	Settings->input_clock			= appRound( 1.f / GFixedTimeStep );
	Settings->input_frame_period	= 1;

	Settings->max_key_interval		= 300;
	Settings->key_frame_threshold	= 50;
    
	Settings->quality				= 192;

	Settings->data_partitioning		= 0;
	Settings->quarter_pel			= 1;
	Settings->use_gmc				= 1;

	Settings->psychovisual			= 1;
	Settings->pv_strength_frame		= 1;
	Settings->pv_strength_MB		= -1;
	
	Settings->resize_mode			= 0;
	Settings->bicubic_B				= 0.0;
	Settings->bicubic_C				= 0.5;
	
	Settings->temporal_enable		= 1;
	Settings->spatial_passes		= 3;
	Settings->temporal_level		= 1.0;
	Settings->spatial_level			= 1.0;

	if( (Width != Viewport->SizeX) || (Height != Viewport->SizeY) )
		Settings->enable_resize		= 1;

	Settings->resize_height			= Height;
	Settings->resize_width			= Width;
	
	unguard;
}

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/