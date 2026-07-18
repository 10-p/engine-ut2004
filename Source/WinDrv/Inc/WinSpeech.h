/*=============================================================================
	WinSpeech.h: Speech recognition using MS SAPI
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel.
=============================================================================*/

#ifndef _INC_WINSPEECH
#define _INC_WINSPEECH


/*-----------------------------------------------------------------------------
	FSpeechRecognitionSAPI.
-----------------------------------------------------------------------------*/

class FSpeechRecognitionSAPI
{
public:
	CComPtr<ISpRecoGrammar>	RecognizerGrammar;
	CComPtr<ISpRecoContext>	RecognizerContext;
	CComPtr<ISpRecognizer>	RecognizerEngine;

	const static DWORD Event = WM_USER + 190;

	~FSpeechRecognitionSAPI();
	UBOOL Init(void* hWnd);
	UBOOL LoadGrammar( const FString& Name );
	UBOOL StartRecognition();
	UBOOL StopRecognition();

	void Destroy();
};


/*-----------------------------------------------------------------------------
	FSpeechAudioInput.
-----------------------------------------------------------------------------*/

//
//	Based on sample code from the MS Speech SDK and Dave Bartolomeo (MS).
//

template <class T> class CCriticalSectionLock
{
private:
	T*  m_pObject;

public:
    CCriticalSectionLock(T* pObject)
    {
        m_pObject = pObject;
        m_pObject->Lock();
    }
    ~CCriticalSectionLock()
    {
        m_pObject->Unlock();
    }
};

class FSpeechAudioInput : public ISpAudio
{
public:
	FSpeechAudioInput( FAudioCaptureDevice* );
	~FSpeechAudioInput();

	// --- IUnknown ---
	STDMETHODIMP QueryInterface( REFIID iid, void ** ppvObject );
	ULONG STDMETHODCALLTYPE AddRef() { return 0; }
	ULONG STDMETHODCALLTYPE Release() { return 0; }

	//--- ISequentialStream ---
	STDMETHODIMP Read( void* pv, ULONG cb, ULONG* pcbRead );
	STDMETHODIMP Write( void const* pv, ULONG cb, ULONG* pcbWritten ) { return E_NOTIMPL; }
 
	//--- IStream ---
	STDMETHODIMP Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER __RPC_FAR *plibNewPosition);
	STDMETHODIMP SetSize(ULARGE_INTEGER libNewSize) { return E_NOTIMPL; }
	STDMETHODIMP CopyTo(IStream *pstm, ULARGE_INTEGER cb, ULARGE_INTEGER *pcbRead, ULARGE_INTEGER *pcbWritten) { return E_NOTIMPL; }
	STDMETHODIMP Commit(DWORD grfCommitFlags) { return E_NOTIMPL; }
	STDMETHODIMP Revert(void) { return E_NOTIMPL; }
	STDMETHODIMP LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) { return E_NOTIMPL; }
	STDMETHODIMP UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) { return E_NOTIMPL; }
	STDMETHODIMP Stat(STATSTG *pstatstg, DWORD grfStatFlag) { return E_NOTIMPL; }
	STDMETHODIMP Clone(IStream **ppstm) { return E_NOTIMPL; }

	//--- ISpStreamFormat ---
	STDMETHODIMP GetFormat(GUID * pguidFormatId, WAVEFORMATEX ** ppCoMemWaveFormatEx);

	//--- ISpAudio ---
	STDMETHODIMP SetState(SPAUDIOSTATE NewState, ULONGLONG ullReserved );
	STDMETHODIMP SetFormat(REFGUID rguidFmtId, const WAVEFORMATEX * pWaveFormatEx) { return E_NOTIMPL; }
	STDMETHODIMP GetStatus(SPAUDIOSTATUS *pStatus);
	STDMETHODIMP SetBufferInfo(const SPAUDIOBUFFERINFO * pInfo) { return E_NOTIMPL; }
	STDMETHODIMP GetBufferInfo(SPAUDIOBUFFERINFO * pInfo) { return E_NOTIMPL; }
	STDMETHODIMP GetDefaultFormat(GUID * pFormatId, WAVEFORMATEX ** ppCoMemWaveFormatEx);
	STDMETHODIMP_(HANDLE) EventHandle();
	STDMETHODIMP GetVolumeLevel(ULONG *pLevel);
	STDMETHODIMP SetVolumeLevel(ULONG Level) { return E_NOTIMPL; }
	STDMETHODIMP GetBufferNotifySize(ULONG *pcbSize);
	STDMETHODIMP SetBufferNotifySize(ULONG cbSize);

	// --- Unreal ---	
	FLOAT Tick();

	// --- Helper ---
	void Lock()	{ EnterCriticalSection(&m_CriticalSection); }
	void Unlock() { LeaveCriticalSection(&m_CriticalSection); }

private:

	void UpdateNotifyEvent();

	FAudioCaptureDevice*	m_Capture;

	SPAUDIOSTATE			m_eState;
	ULONG					m_nNotifySize;
	HANDLE					m_shNotifyEvent;
	HANDLE					m_shClosedEvent;
	ULONGLONG				m_nBytesRead;
	bool					m_bNotifySignalled;
	DWORD					m_CaptureOffset;
	FLOAT					m_LastGain;

	CRITICAL_SECTION		m_CriticalSection;
};

#define AUTO_SPEECH_LOCK CCriticalSectionLock< FSpeechAudioInput > lck(this)

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

