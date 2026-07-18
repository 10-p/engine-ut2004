/*=============================================================================
	WinSpeech.cpp: Speech recognition using MS SAPI
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#include "WinDrv.h"

#ifndef _WIN64

//
//	FSpeechRecognitionSAPI implementation.
//

FSpeechRecognitionSAPI::~FSpeechRecognitionSAPI()
{
	Destroy();
}

UBOOL FSpeechRecognitionSAPI::Init(void* hWnd)
{
	HRESULT hr;

	// Create a recognition engine.	
	if( FAILED( hr = RecognizerEngine.CoCreateInstance(CLSID_SpInprocRecognizer) ) )
		return 0;

	// Set our input stream as the input to use.
	if( FAILED( hr = RecognizerEngine->SetInput( UWindowsViewport::SpeechAudioInput, FALSE) ) )
		return 0;

	// Create the command recognition context.
	if( FAILED( hr = RecognizerEngine->CreateRecoContext( &RecognizerContext ) ) )
		return 0;

	// Let SR know which window/ message we want it to send event information to.
	if( FAILED( hr = RecognizerContext->SetNotifyWindowMessage( (HWND) hWnd, Event, 0, 0 ) ) )
		return 0;

	// Tell SR what types of events interest us.
	if( FAILED( hr = RecognizerContext->SetInterest( SPFEI(SPEI_RECOGNITION), SPFEI(SPEI_RECOGNITION) ) ) )
		return 0;

	// Success!
	return 1;
}

UBOOL FSpeechRecognitionSAPI::LoadGrammar( const FString& Name )
{
	// Ensure we're initialized.
	if( !RecognizerContext )
	{
		debugf(NAME_Error, TEXT("SR: Recognizer context not set up."));
		return 0;
	}

	// Housekeeping.
	if( RecognizerGrammar )
		RecognizerGrammar.Release();

	// Try to load and activate grammar.
	if( SUCCEEDED( RecognizerContext->CreateGrammar(0, &RecognizerGrammar) ) && SUCCEEDED( RecognizerGrammar->LoadCmdFromFile( *Name, SPLO_STATIC ) ) )
	{
		debugf(NAME_Log, TEXT("SR: Loaded grammar file %s"), *Name );
		return 1;
	}
	else
	{
		debugf(NAME_Log, TEXT("SR: Failed to load grammar file %s"), *Name );
		return 0;
	}
}

UBOOL FSpeechRecognitionSAPI::StartRecognition()
{
	// Ensure we're initialized.
	if( !RecognizerGrammar )
		return 0;

	return SUCCEEDED( RecognizerGrammar->SetRuleState(NULL, NULL, SPRS_ACTIVE ) );
}

UBOOL FSpeechRecognitionSAPI::StopRecognition()
{
	// Ensure we're initialized.
	if( !RecognizerGrammar )
		return 0;

	return SUCCEEDED( RecognizerGrammar->SetRuleState(NULL, NULL, SPRS_INACTIVE ) );
}

void FSpeechRecognitionSAPI::Destroy()
{
	// Release grammar, if loaded
	if( RecognizerGrammar )
		RecognizerGrammar.Release();

	// Release recognition context, if created
	if ( RecognizerContext )
	{
		RecognizerContext->SetNotifySink(NULL);
		RecognizerContext.Release();
	}

	// Release recognition engine instance, if created
	if ( RecognizerEngine )
		RecognizerEngine.Release();
}


//
//	FSpeechAudioInput implementation.
//


//
//	Constructor/ destructor.
//

FSpeechAudioInput::FSpeechAudioInput( FAudioCaptureDevice* Capture ) : 
	m_bNotifySignalled( false ),
	m_nNotifySize( 640 ),
	m_eState( SPAS_CLOSED ),
	m_nBytesRead( 0 ),
	m_Capture( Capture ),
	m_CaptureOffset( 0 ),
	m_LastGain( 0 )
{
	InitializeCriticalSection(&m_CriticalSection);

	m_shNotifyEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	m_shClosedEvent = CreateEvent(NULL, TRUE, TRUE, NULL);

	m_Capture->Record();
}

FSpeechAudioInput::~FSpeechAudioInput()
{
	if (m_shNotifyEvent)
		CloseHandle(m_shNotifyEvent);

	if (m_shClosedEvent)
		CloseHandle(m_shClosedEvent);
		
	m_Capture->Stop();

	DeleteCriticalSection(&m_CriticalSection);
}

//
//	IUnknown interface.
//

HRESULT FSpeechAudioInput::QueryInterface( REFIID iid, void ** ppv )
{
	if( iid == IID_IUnknown )
	{
		*ppv = static_cast<IStream*>(this);
	}
	if( iid == IID_IStream )
	{
		*ppv = static_cast<IStream*>(this);
	}
	else
	if( iid == IID_ISpStreamFormat )
	{
		*ppv = static_cast<ISpStreamFormat*>(this);
	}
	else
	if( iid == IID_ISpAudio )
	{
		*ppv = static_cast<ISpAudio*>(this);
	}
	else
	{
		*ppv = NULL;
		return E_NOINTERFACE;
	}
	static_cast<IUnknown*>(*ppv)->AddRef();
	return S_OK;
}

//
// ISequentialStream interface.
//

STDMETHODIMP FSpeechAudioInput::Read( void* pBuffer, ULONG nBytes, ULONG* pnBytesRead )
{
	if( pnBytesRead != NULL )
		*pnBytesRead = 0;

	HANDLE Events[2] = { m_shNotifyEvent, m_shClosedEvent };
	DWORD ObjectWait = ::WaitForMultipleObjects( 2, Events, FALSE, INFINITE );
	if( ObjectWait == WAIT_OBJECT_0 )
	{
		AUTO_SPEECH_LOCK;

		DWORD NumSamples	= nBytes / sizeof(SHORT);

		m_LastGain = m_Capture->GetSampleData( m_CaptureOffset, pBuffer, NumSamples, CAPTURE_16KHZ_SHORT );

		m_nBytesRead += NumSamples * sizeof(SHORT);
		if( pnBytesRead != NULL )
			*pnBytesRead = NumSamples * sizeof(SHORT);

		UpdateNotifyEvent();
	}

	return S_OK;
}

//
//	IStream interface.
//

STDMETHODIMP FSpeechAudioInput::Seek( LARGE_INTEGER nOffset, DWORD eOrigin, ULARGE_INTEGER* pnNewPosition )
{
	AUTO_SPEECH_LOCK;

	switch( eOrigin )
	{
	case STREAM_SEEK_CUR:
		//assert( nOffset.QuadPart == 0 );
		if( pnNewPosition != NULL )
			pnNewPosition->QuadPart = 0;
		return S_OK;
		break;

	case STREAM_SEEK_END:
		//assert( false );
		break;

	case STREAM_SEEK_SET:
		//assert( false );
		break;

	default:
		//assert( false );
		break;
	}

	return E_NOTIMPL;
}

//
//	ISpStreamFormat interface.
//

STDMETHODIMP FSpeechAudioInput::GetFormat(GUID * pguidFormat, WAVEFORMATEX ** ppFormat)
{
	if( pguidFormat != NULL )
	{
		*pguidFormat = GUID_NULL;
	}
	if( ppFormat != NULL )
	{
		*ppFormat = NULL;
	}
	if( (pguidFormat == NULL) || (ppFormat == NULL) )
	{
		return E_POINTER;
	}

	WAVEFORMATEX* pFormat = static_cast< WAVEFORMATEX* >( ::CoTaskMemAlloc( sizeof( WAVEFORMATEX ) ) );

	memset( pFormat, 0, sizeof(WAVEFORMATEX));
	pFormat->wFormatTag			= WAVE_FORMAT_PCM;
	pFormat->nSamplesPerSec		= 16000;
	pFormat->wBitsPerSample		= 16;
	pFormat->nChannels			= 1;
	pFormat->nBlockAlign		= pFormat->nChannels * ( pFormat->wBitsPerSample / 8 );
	pFormat->nAvgBytesPerSec	= pFormat->nBlockAlign * pFormat->nSamplesPerSec;

	*ppFormat		= pFormat;
	*pguidFormat	= SPDFID_WaveFormatEx;
	
	return S_OK;
}

//
//	ISpAudio interface.
//

STDMETHODIMP FSpeechAudioInput::SetState( SPAUDIOSTATE eState, ULONGLONG nReserved )
{
	AUTO_SPEECH_LOCK;

	m_eState = eState;
	if( (m_eState == SPAS_CLOSED) || (m_eState == SPAS_PAUSE) )
	{
		SetEvent( m_shClosedEvent );
	}
	else
	{
		ResetEvent( m_shClosedEvent );
		m_CaptureOffset = m_Capture->GetCurrentOffset();
	}

	return S_OK;
}


STDMETHODIMP FSpeechAudioInput::GetStatus( SPAUDIOSTATUS* pStatus )
{
	AUTO_SPEECH_LOCK;

	DWORD AvailableSamples		= m_Capture->GetAvailableSamples( m_CaptureOffset, CAPTURE_16KHZ_SHORT );

	pStatus->cbFreeBuffSpace	= 0;
	pStatus->cbNonBlockingIO	= AvailableSamples * sizeof(SHORT);
	pStatus->dwReserved1		= 0;
	pStatus->dwReserved2		= 0;
	pStatus->State				= m_eState;
	pStatus->CurSeekPos			= m_nBytesRead;
	pStatus->CurDevicePos		= m_nBytesRead + AvailableSamples * sizeof(SHORT);

	return S_OK;
}

STDMETHODIMP FSpeechAudioInput::GetDefaultFormat( GUID* pguidFormat, WAVEFORMATEX** ppFormat )
{
	if( pguidFormat != NULL )
	{
		*pguidFormat = GUID_NULL;
	}
	if( ppFormat != NULL )
	{
		*ppFormat = NULL;
	}
	if( (pguidFormat == NULL) || (ppFormat == NULL) )
	{
		return E_POINTER;
	}

	WAVEFORMATEX* pFormat = static_cast< WAVEFORMATEX* >( ::CoTaskMemAlloc( sizeof( WAVEFORMATEX ) ) );

	memset( pFormat, 0, sizeof(WAVEFORMATEX));
	pFormat->wFormatTag			= WAVE_FORMAT_PCM;
	pFormat->nSamplesPerSec		= 16000;
	pFormat->wBitsPerSample		= 16;
	pFormat->nChannels			= 1;
	pFormat->nBlockAlign		= pFormat->nChannels * ( pFormat->wBitsPerSample / 8 );
	pFormat->nAvgBytesPerSec	= pFormat->nBlockAlign * pFormat->nSamplesPerSec;

	*ppFormat		= pFormat;
	*pguidFormat	= SPDFID_WaveFormatEx;
	
	return S_OK;
}

STDMETHODIMP_( HANDLE ) FSpeechAudioInput::EventHandle()
{
	return m_shNotifyEvent;
}

STDMETHODIMP FSpeechAudioInput::GetVolumeLevel( ULONG* pnVolume )
{
	if( pnVolume == NULL )
		return E_POINTER;
	
	*pnVolume = 10000;
	return S_OK;
}

STDMETHODIMP FSpeechAudioInput::GetBufferNotifySize( ULONG* pnBytes )
{
	AUTO_SPEECH_LOCK;

	if( pnBytes == NULL )
		return E_POINTER;

	*pnBytes = m_nNotifySize;

	return S_OK;
}

STDMETHODIMP FSpeechAudioInput::SetBufferNotifySize( ULONG nBytes )
{
	AUTO_SPEECH_LOCK;

	if( nBytes != m_nNotifySize )
	{
		m_nNotifySize = nBytes;
		UpdateNotifyEvent();
	}

	return S_OK;
}

void FSpeechAudioInput::UpdateNotifyEvent()
{
	DWORD NumBytesAvailable = 2 * m_Capture->GetAvailableSamples( m_CaptureOffset, CAPTURE_16KHZ_SHORT );

	bool bShouldNotify = NumBytesAvailable >= m_nNotifySize;
	if( bShouldNotify && !m_bNotifySignalled )
	{
		SetEvent( m_shNotifyEvent );
		m_bNotifySignalled = true;
	}
	else if( !bShouldNotify && m_bNotifySignalled )
	{
		ResetEvent( m_shNotifyEvent );
		m_bNotifySignalled = false;
	}
}

FLOAT FSpeechAudioInput::Tick()
{
	AUTO_SPEECH_LOCK;

	UpdateNotifyEvent();

	return m_LastGain;
}

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/