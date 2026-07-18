/*=============================================================================
	Browser : Base class for browser windows
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

enum {MAX_RESULTS=1024};

/*-----------------------------------------------------------------------------
	WBrowser
-----------------------------------------------------------------------------*/

class WBrowser : public WWindow
{
	DECLARE_WINDOWCLASS(WBrowser,WWindow,Window)

	FString SavePkgName, Description, DefCaption;
	INT MenuID, BrowserID;
	HWND hwndEditorFrame;
	HMENU hmenu;

	// Structors.
	WBrowser( FName InPersistentName, WWindow* InOwnerWindow, HWND InEditorFrame );

	// WWindow interface.
	void OpenWindow( UBOOL bChild );
	INT OnSysCommand( INT Command );
	INT OnSetCursor();
	void OnCreate();
	virtual void SetCaption( FString* Tail = NULL );
	virtual FString	GetCaption();
	virtual void UpdateMenu();
	void OnDestroy();
	void OnPaint();
	void OnSize( DWORD Flags, INT NewX, INT NewY );
	virtual void PositionChildControls( void );
	// Searches a list of filenames and replaces all single NULL's with | characters.  This allows
	// the regular parse routine to work correctly.  The return value is the number of NULL's
	// that were replaced -- if this is greater than zero, you have multiple filenames.
	//
	INT FormatFilenames( char* _pchFilenames );
	virtual FString GetCurrentPathName( void );
	virtual void RefreshAll();
	void OnCommand( INT Command );
	inline UBOOL IsDocked() { return bDocked; }
	virtual void Show( UBOOL Show );

private:
	UBOOL bDocked;	// If TRUE, then this browser is docked inside the master browser window
};

// Takes a fully pathed filename, and just returns the name.
// i.e. "c:\test\file.txt" gets returned as "file".
//
FString GetFilenameOnly( FString Filename);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

