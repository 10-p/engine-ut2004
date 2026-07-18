/*=============================================================================
	Browser : Base class for browser windows
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

#include "UnrealEd.h"

static INT CDECL ClassSortCompare( const void *elem1, const void *elem2 )
{
	return appStricmp((*(UClass**)elem1)->GetName(),(*(UClass**)elem2)->GetName());
}
void Query( ULevel* Level, const TCHAR* Item, FString* pOutput, UPackage* InPkg )
{
	guard(Query);

	check(InPkg);

	int		NumResults = 0;
	UClass	*Results[MAX_RESULTS];
	FString Work;

	if( ParseCommand(&Item,TEXT("QUERY")) )
	{
		InPkg = ANY_PACKAGE;

		UClass *Parent = NULL;
		ParseObject<UClass>(Item,TEXT("PARENT="),Parent,ANY_PACKAGE);

		// Make a list of all child classes.
		for( TObjectIterator<UClass> It; It && NumResults<MAX_RESULTS; ++It )
			if( It->GetSuperClass()==Parent ) //&& It->IsIn( InPkg ) )
				Results[NumResults++] = *It;

		// Return the results.
		for( INT i=0; i<NumResults; ++i )
		{
			// See if this item has children.
			INT Children = 0;
			for( TObjectIterator<UClass> It; It; ++It )
				if( It->GetSuperClass()==Results[i] )
					Children++;

			// Add to result string.
			if( i>0 ) Work += TEXT(",");
			Work += FString::Printf( TEXT("%s%s%s"),
				Children ? TEXT("C") : TEXT("_"),
				(Results[i]->ClassFlags & CLASS_Placeable) && !(Results[i]->ClassFlags & CLASS_Abstract) ? TEXT("*") : TEXT(" "),
				Results[i]->GetName() );
		}

		*pOutput = Work;
	}
	if( ParseCommand(&Item,TEXT("GETCHILDREN")) )
	{
		UClass *Parent = NULL;
		ParseObject<UClass>(Item,TEXT("CLASS="),Parent,InPkg ? InPkg : ANY_PACKAGE);
		UBOOL Concrete=0; ParseUBOOL( Item, TEXT("CONCRETE="), Concrete );

		// Make a list of all child classes.
		for( TObjectIterator<UClass> It; It && NumResults<MAX_RESULTS; ++It )
			if( It->IsChildOf(Parent) && (!Concrete || !(It->ClassFlags & CLASS_Abstract)) )
				Results[NumResults++] = *It;

		// Sort them by name.
		appQsort( Results, NumResults, sizeof(UClass*), ClassSortCompare );

		// Return the results.
		for( INT i=0; i<NumResults; ++i )
		{
			if( i>0 ) Work += TEXT(",");
			Work += FString::Printf( TEXT("%s"), Results[i]->GetName() );
		}

		*pOutput = Work;
	}
	unguard;
}

// --------------------------------------------------------------
//
// WBrowser
//
// --------------------------------------------------------------

WBrowser::WBrowser( FName InPersistentName, WWindow* InOwnerWindow, HWND InEditorFrame )
:	WWindow( InPersistentName, InOwnerWindow )
{
	check(InOwnerWindow);
	bDocked = 0;
	MenuID = 0;
	BrowserID = -1;
	hwndEditorFrame = InEditorFrame;
	Description = TEXT("Browser");
}

// WWindow interface.
void WBrowser::OpenWindow( UBOOL bChild )
{
	guard(WBrowser::OpenWindow);
	MdiChild = 0;

	PerformCreateWindowEx
	(
		0,
		NULL,
		(bChild ? WS_CHILD  : WS_OVERLAPPEDWINDOW) | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		0,
		0,
		320,
		200,
		OwnerWindow ? OwnerWindow->hWnd : NULL,
		NULL,
		hInstance
	);
	bDocked = bChild;
	Show(0);
	unguard;
}
INT WBrowser::OnSysCommand( INT Command )
{
	guard(WBrowser::OnSysCommand);
	if( Command == SC_CLOSE )
	{
		Show(0);
		return 1;
	}

	return 0;
	unguard;
}
INT WBrowser::OnSetCursor()
{
	guard(WDlgExtrude::OnSetCursor);
	WWindow::OnSetCursor();
	SetCursor(LoadCursorIdX(NULL,IDC_ARROW));
	return 0;
	unguard;
}
void WBrowser::OnCreate()
{
	guard(WBrowser::OnCreate);
	WWindow::OnCreate();

	// Load windows last position.
	//
	INT X, Y, W, H;

	if(!GConfig->GetInt( *PersistentName, TEXT("X"), X, TEXT("UnrealEd.ini") ))	X = 0;
	if(!GConfig->GetInt( *PersistentName, TEXT("Y"), Y, TEXT("UnrealEd.ini") ))	Y = 0;
	if(!GConfig->GetInt( *PersistentName, TEXT("W"), W, TEXT("UnrealEd.ini") ))	W = 512;
	if(!GConfig->GetInt( *PersistentName, TEXT("H"), H, TEXT("UnrealEd.ini") ))	H = 384;

	if( !W ) W = 320;
	if( !H ) H = 200;

	::MoveWindow( hWnd, X, Y, W, H, TRUE );

	unguard;
}
void WBrowser::SetCaption( FString* Tail )
{
	guard(WBrowser::SetCaption);
	FString Caption;

	Caption = Description;

	if( Tail && Tail->Len() )
		Caption = *FString::Printf(TEXT("%s - %s"), *Caption, *Tail );

	if( IsDocked() )
		OwnerWindow->SetText( *Caption );
	else
		SetText( *Caption );

	unguard;
}
FString	WBrowser::GetCaption()
{
	guard(WBrowser::GetCaption);
	return GetText();
	unguard;
}
void WBrowser::UpdateMenu()
{
	guard(WBrowser::UpdateMenu);
	unguard;
}
void WBrowser::OnDestroy()
{
	guard(WBrowser::OnDestroy);

	// Save Window position (base class doesn't always do this properly)
	// (Don't do this if the window is minimized.)
	//
	if( !::IsIconic( hWnd ) && !::IsZoomed( hWnd ) )
	{
		RECT R;
		::GetWindowRect(hWnd, &R);

		GConfig->SetInt( *PersistentName, TEXT("Active"), bShow, TEXT("UnrealEd.ini") );
		GConfig->SetInt( *PersistentName, TEXT("Docked"), bDocked, TEXT("UnrealEd.ini") );
		GConfig->SetInt( *PersistentName, TEXT("X"), R.left, TEXT("UnrealEd.ini") );
		GConfig->SetInt( *PersistentName, TEXT("Y"), R.top, TEXT("UnrealEd.ini") );
		GConfig->SetInt( *PersistentName, TEXT("W"), R.right - R.left, TEXT("UnrealEd.ini") );
		GConfig->SetInt( *PersistentName, TEXT("H"), R.bottom - R.top, TEXT("UnrealEd.ini") );
	}

	_DeleteWindows.AddItem( this );

	WWindow::OnDestroy();
	unguard;
}
void WBrowser::OnPaint()
{
	guard(WBrowser::OnPaint);
	PAINTSTRUCT PS;
	HDC hDC = BeginPaint( *this, &PS );
	FillRect( hDC, GetClientRect(), (HBRUSH)(COLOR_BTNFACE+1) );
	EndPaint( *this, &PS );
	unguard;
}
void WBrowser::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	guard(WBrowser::OnSize);
	WWindow::OnSize(Flags, NewX, NewY);
	PositionChildControls();
	InvalidateRect( hWnd, NULL, FALSE );
	unguard;
}
void WBrowser::PositionChildControls( void )
{
	guard(WBrowser::PositionChildControls);
	unguard;
}
// Searches a list of filenames and replaces all single NULL's with | characters.  This allows
// the regular parse routine to work correctly.  The return value is the number of NULL's
// that were replaced -- if this is greater than zero, you have multiple filenames.
//
INT WBrowser::FormatFilenames( char* _pchFilenames )
{
	guard(WBrowser::FormatFilenames);
	char *pch = _pchFilenames;
	INT l_iNULLs = 0;

	while( true )
	{
		if( *pch == '\0' )
		{
			if( *(pch+1) == '\0') break;

			*pch = '|';
			l_iNULLs++;
		}
		pch++;
	}

	return l_iNULLs;
	unguard;
}
FString WBrowser::GetCurrentPathName( void )
{
	guard(WBrowser::GetCurrentPathName);
	return TEXT("");
	unguard;
}
void WBrowser::RefreshAll()
{
	guard(WBrowser::RefreshAll);
	unguard;
}
void WBrowser::OnCommand( INT Command )
{
	guard(WBrowser::OnCommand);
	switch( Command ) {

		case IDMN_MB_DOCK:
		{
			bDocked = !bDocked;
			SendMessageX( hwndEditorFrame, WM_COMMAND, bDocked ? WM_BROWSER_DOCK : WM_BROWSER_UNDOCK, BrowserID );
		}
		break;

		default:
			WWindow::OnCommand(Command);
			break;
	}
	unguard;
}
void WBrowser::Show( UBOOL Show )
{
	WWindow::Show(Show);
	if( Show )
		BringWindowToTop( hWnd );
}

// Takes a fully pathed filename, and just returns the name.
// i.e. "c:\test\file.txt" gets returned as "file".
//
FString GetFilenameOnly( FString Filename)
{
	guard(GetFilenameOnly);
	FString NewFilename = Filename;

	while( NewFilename.InStr( TEXT("\\") ) != -1 )
		NewFilename = NewFilename.Mid( NewFilename.InStr( TEXT("\\") ) + 1, NewFilename.Len() );

	if( NewFilename.InStr( TEXT(".") ) != -1 )
		NewFilename = NewFilename.Left( NewFilename.InStr( TEXT(".") ) );

	return NewFilename;
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

