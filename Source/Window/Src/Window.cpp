/*=============================================================================
	Window.cpp: GUI window management code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#pragma warning( disable : 4201 )
#define STRICT
#pragma pack(push,8)
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#pragma pack(pop)

#include "Engine.h"
#include "Window.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

WNDPROC WTabControl::SuperProc;
WNDPROC WLabel::SuperProc;
WNDPROC WGroupBox::SuperProc;
WNDPROC WCustomLabel::SuperProc;
WNDPROC WListView::SuperProc;
WNDPROC WEdit::SuperProc;
WNDPROC WRichEdit::SuperProc;
WNDPROC WListBox::SuperProc;
WNDPROC WCheckListBox::SuperProc;
WNDPROC WBitmapButton::SuperProc;
WNDPROC WColorButton::SuperProc;
WNDPROC WTrackBar::SuperProc;
WNDPROC WProgressBar::SuperProc;
WNDPROC WComboBox::SuperProc;
WNDPROC WButton::SuperProc;
WNDPROC WPropertySheet::SuperProc;
WNDPROC WToolTip::SuperProc;
WNDPROC WCoolButton::SuperProc;
WNDPROC WUrlButton::SuperProc;
WNDPROC WCheckBox::SuperProc;
WNDPROC WScrollBar::SuperProc;
WNDPROC WTreeView::SuperProc;
INT WWindow::ModalCount=0;
TArray<WWindow*> WWindow::_Windows;
TArray<WWindow*> WWindow::_DeleteWindows;
TArray<WProperties*> WProperties::PropertiesWindows;
WINDOW_API WLog* GLogWindow=NULL;
WINDOW_API HBRUSH hBrushBlack;
WINDOW_API HBRUSH hBrushWhite;
WINDOW_API HBRUSH hBrushOffWhite;
WINDOW_API HBRUSH hBrushHeadline;
WINDOW_API HBRUSH hBrushCurrent;
WINDOW_API HBRUSH hBrushDark;
WINDOW_API HBRUSH hBrushGrey;
WINDOW_API HBRUSH hBrushGreyWindow;
WINDOW_API HBRUSH hBrushGrey160;
WINDOW_API HBRUSH hBrushGrey180;
WINDOW_API HBRUSH hBrushGrey197;
WINDOW_API HBRUSH hBrushCyanHighlight;
WINDOW_API HBRUSH hBrushCyanLow;
WINDOW_API HBRUSH hBrushDarkGrey;
WINDOW_API HFONT hFontUrl;
WINDOW_API HFONT hFontText;
WINDOW_API HFONT hFontHeadline;
WINDOW_API HINSTANCE hInstanceWindow;
WINDOW_API UBOOL GNotify=0;
WCoolButton* WCoolButton::GlobalCoolButton=NULL;
WINDOW_API UINT WindowMessageOpen;
WINDOW_API UINT WindowMessageMouseWheel;
WINDOW_API NOTIFYICONDATA NID;
#if UNICODE
WINDOW_API NOTIFYICONDATAA NIDA;
WINDOW_API UBOOL (WINAPI* Shell_NotifyIconWX)( DWORD dwMessage, PNOTIFYICONDATAW pnid )=NULL;
WINDOW_API UBOOL (WINAPI* SHGetSpecialFolderPathWX)( HWND hwndOwner, LPTSTR lpszPath, INT nFolder, UBOOL fCreate );
#endif

IMPLEMENT_PACKAGE(Window)

/*-----------------------------------------------------------------------------
	Window manager.
-----------------------------------------------------------------------------*/

W_IMPLEMENT_CLASS(WWindow)
W_IMPLEMENT_CLASS(WControl)
W_IMPLEMENT_CLASS(WTabControl)
W_IMPLEMENT_CLASS(WLabel)
W_IMPLEMENT_CLASS(WGroupBox)
W_IMPLEMENT_CLASS(WCustomLabel)
W_IMPLEMENT_CLASS(WListView)
W_IMPLEMENT_CLASS(WButton)
W_IMPLEMENT_CLASS(WPropertySheet)
W_IMPLEMENT_CLASS(WToolTip)
W_IMPLEMENT_CLASS(WCoolButton)
W_IMPLEMENT_CLASS(WUrlButton)
W_IMPLEMENT_CLASS(WComboBox)
W_IMPLEMENT_CLASS(WEdit)
W_IMPLEMENT_CLASS(WRichEdit)
W_IMPLEMENT_CLASS(WTerminalBase)
W_IMPLEMENT_CLASS(WTerminal)
W_IMPLEMENT_CLASS(WLog)
W_IMPLEMENT_CLASS(WDialog)
W_IMPLEMENT_CLASS(WCrashBoxDialog);
W_IMPLEMENT_CLASS(WTrackBar)
W_IMPLEMENT_CLASS(WProgressBar)
W_IMPLEMENT_CLASS(WListBox)
W_IMPLEMENT_CLASS(WItemBox)
W_IMPLEMENT_CLASS(WCheckListBox)
W_IMPLEMENT_CLASS(WBitmapButton)
W_IMPLEMENT_CLASS(WColorButton)
W_IMPLEMENT_CLASS(WPropertiesBase)
W_IMPLEMENT_CLASS(WDragInterceptor)
W_IMPLEMENT_CLASS(WPictureButton)
W_IMPLEMENT_CLASS(WUDNWindow)
W_IMPLEMENT_CLASS(WThinScrollBar)
W_IMPLEMENT_CLASS(WTimeScrollBar)
W_IMPLEMENT_CLASS(WPropertyPage)
W_IMPLEMENT_CLASS(WSplitterPane)
W_IMPLEMENT_CLASS(WSplitter)
W_IMPLEMENT_CLASS(WSplitterContainer)
W_IMPLEMENT_CLASS(WProperties)
W_IMPLEMENT_CLASS(WObjectProperties)
W_IMPLEMENT_CLASS(WClassProperties)
W_IMPLEMENT_CLASS(WConfigProperties)
W_IMPLEMENT_CLASS(WWizardPage)
W_IMPLEMENT_CLASS(WWizardDialog)
W_IMPLEMENT_CLASS(WEditTerminal)
W_IMPLEMENT_CLASS(WCheckBox)
W_IMPLEMENT_CLASS(WScrollBar)
W_IMPLEMENT_CLASS(WTreeView)

class UWindowManager : public USubsystem
{
	DECLARE_CLASS(UWindowManager,UObject,CLASS_Transient,Window);

	// Constructor.
	UWindowManager()
	{
		guard(UWindowManager::UWindowManager);

		// Init common controls.
		InitCommonControls();

		// Get addresses of procedures that don't exist in Windows 95.
#if UNICODE
		HMODULE hModShell32 = GetModuleHandle( TEXT("SHELL32.DLL") );
		*(FARPROC*)&Shell_NotifyIconWX       = GetProcAddress( hModShell32, "Shell_NotifyIconW" );
		*(FARPROC*)&SHGetSpecialFolderPathWX = GetProcAddress( hModShell32, "SHGetSpecialFolderPathW" );
#endif

		// Save instance.
		hInstanceWindow = hInstance;

		LoadLibraryX(TEXT("RICHED32.DLL"));

		// Implement window classes.
		IMPLEMENT_WINDOWSUBCLASS(WListBox,TEXT("LISTBOX"));
		IMPLEMENT_WINDOWSUBCLASS(WItemBox,TEXT("LISTBOX"));
		IMPLEMENT_WINDOWSUBCLASS(WCheckListBox,TEXT("LISTBOX"));
		IMPLEMENT_WINDOWSUBCLASS(WBitmapButton,TEXT("BUTTON"));
		IMPLEMENT_WINDOWSUBCLASS(WColorButton,TEXT("BUTTON"));
		IMPLEMENT_WINDOWSUBCLASS(WTabControl,WC_TABCONTROL);
		IMPLEMENT_WINDOWSUBCLASS(WLabel,TEXT("STATIC"));
		IMPLEMENT_WINDOWSUBCLASS(WGroupBox,TEXT("BUTTON"));
		IMPLEMENT_WINDOWSUBCLASS(WCustomLabel,TEXT("STATIC"));
		IMPLEMENT_WINDOWSUBCLASS(WListView,TEXT("SysListView32"));
		IMPLEMENT_WINDOWSUBCLASS(WEdit,TEXT("EDIT"));
		IMPLEMENT_WINDOWSUBCLASS(WRichEdit,TEXT("RICHEDIT"));
		IMPLEMENT_WINDOWSUBCLASS(WComboBox,TEXT("COMBOBOX"));
		IMPLEMENT_WINDOWSUBCLASS(WEditTerminal,TEXT("EDIT"));
		IMPLEMENT_WINDOWSUBCLASS(WButton,TEXT("BUTTON"));
		IMPLEMENT_WINDOWSUBCLASS(WPropertySheet,TEXT("STATIC"));
		IMPLEMENT_WINDOWSUBCLASS(WToolTip,TOOLTIPS_CLASS);
		IMPLEMENT_WINDOWSUBCLASS(WCoolButton,TEXT("BUTTON"));
		IMPLEMENT_WINDOWSUBCLASS(WUrlButton,TEXT("BUTTON"));
		IMPLEMENT_WINDOWSUBCLASS(WCheckBox,TEXT("BUTTON"));
		IMPLEMENT_WINDOWSUBCLASS(WScrollBar,TEXT("SCROLLBAR"));
		IMPLEMENT_WINDOWSUBCLASS(WTreeView,WC_TREEVIEW);
		IMPLEMENT_WINDOWSUBCLASS(WTrackBar,TRACKBAR_CLASS);
		IMPLEMENT_WINDOWSUBCLASS(WProgressBar,PROGRESS_CLASS);
		IMPLEMENT_WINDOWCLASS(WTerminal,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WLog,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WCrashBoxDialog,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WProperties,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WObjectProperties,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WConfigProperties,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WClassProperties,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WWizardDialog,0);
		IMPLEMENT_WINDOWCLASS(WWizardPage,0);
		IMPLEMENT_WINDOWCLASS(WDragInterceptor,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WPictureButton,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WUDNWindow,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WThinScrollBar,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WTimeScrollBar,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WPropertyPage,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WSplitter,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WSplitterPane,CS_DBLCLKS);
		IMPLEMENT_WINDOWCLASS(WSplitterContainer,CS_DBLCLKS);
		//WC_HEADER (InitCommonControls)
		//UPDOWN_CLASS (InitCommonControls)
		//STATUSCLASSNAME (InitCommonControls)
		//TOOLBARCLASSNAME (InitCommonControls)

		// Create brushes.
		hBrushBlack			= CreateSolidBrush( RGB(0,0,0) );
		hBrushWhite			= CreateSolidBrush( RGB(255,255,255) );
		hBrushOffWhite		= CreateSolidBrush( RGB(224,224,224) );
		hBrushHeadline		= CreateSolidBrush( RGB(200,200,200) );
		hBrushCurrent		= CreateSolidBrush( RGB(160,160,160) );
		hBrushDark			= CreateSolidBrush( RGB(64,64,64) );
		hBrushGrey			= CreateSolidBrush( RGB(128,128,128) );
		hBrushGreyWindow	= CreateSolidBrush( GetSysColor( COLOR_3DFACE ) );
		hBrushGrey160		= CreateSolidBrush( RGB(160,160,160) );
		hBrushGrey180		= CreateSolidBrush( RGB(180,180,180) );
		hBrushGrey197		= CreateSolidBrush( RGB(197,197,197) );
		hBrushCyanHighlight	= CreateSolidBrush( RGB(0,200,200) );
		hBrushCyanLow		= CreateSolidBrush( RGB(0,140,140) );

		// Create fonts.
		HDC hDC       = GetDC( NULL );
#ifndef JAPANESE
		hFontText     = CreateFont( -MulDiv(9/*PointSize*/,  GetDeviceCaps(hDC, LOGPIXELSY), 72), 0, 0, 0, 0, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, TEXT("Arial") );
		hFontUrl      = CreateFont( -MulDiv(9/*PointSize*/,  GetDeviceCaps(hDC, LOGPIXELSY), 72), 0, 0, 0, 0, 0, 1, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, TEXT("Arial") );
		hFontHeadline = CreateFont( -MulDiv(15/*PointSize*/, GetDeviceCaps(hDC, LOGPIXELSY), 72), 0, 0, FW_BOLD, 1, 1, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, TEXT("Arial") );
#else
		hFontText     = (HFONT)( GetStockObject( DEFAULT_GUI_FONT ) );
		hFontUrl      = (HFONT)( GetStockObject( DEFAULT_GUI_FONT ) );
		hFontHeadline = (HFONT)( GetStockObject( DEFAULT_GUI_FONT ) );
#endif
		ReleaseDC( NULL, hDC );

		// Custom window messages.
		WindowMessageOpen       = RegisterWindowMessageX( TEXT("UnrealOpen") );
		WindowMessageMouseWheel = RegisterWindowMessageX( TEXT("MSWHEEL_ROLLMSG") );

		unguard;
	}

	// FExec interface.
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar )
	{
		guard(UWindowManager::Exec);
		return 0;
		unguard;
	}

	// UObject interface.
	void Serialize( FArchive& Ar )
	{
		guard(UWindowManager::Serialize);
		Super::Serialize( Ar );
		for( INT i=0; i<WWindow::_Windows.Num(); i++ )
			WWindow::_Windows(i)->Serialize( Ar );
		for( INT i=0; i<WWindow::_DeleteWindows.Num(); i++ )
			WWindow::_DeleteWindows(i)->Serialize( Ar );
		unguard;
	}
	void Destroy()
	{
		guard(UWindowManager::Destroy);
		Super::Destroy();

		// Delete brushes.
		DeleteObject( hBrushBlack );
		DeleteObject( hBrushWhite );
		DeleteObject( hBrushOffWhite );
		DeleteObject( hBrushHeadline );
		DeleteObject( hBrushCurrent );
		DeleteObject( hBrushDark );
		DeleteObject( hBrushGrey );
		DeleteObject( hBrushGreyWindow );
		DeleteObject( hBrushGrey160 );
		DeleteObject( hBrushGrey180 );
		DeleteObject( hBrushGrey197 );
		DeleteObject( hBrushCyanHighlight );
		DeleteObject( hBrushCyanLow );

		check(GWindowManager==this);
		GWindowManager = NULL;
		if( !GIsCriticalError )
			Tick( 0.0 );
		WWindow::_Windows.Empty();
		WWindow::_DeleteWindows.Empty();
		WProperties::PropertiesWindows.Empty();
		unguard;
	}

	// USubsystem interface.
	void Tick( FLOAT DeltaTime )
	{
		guard(UWindowManager::Tick);
		while( WWindow::_DeleteWindows.Num() )
		{
			WWindow* W = WWindow::_DeleteWindows( 0 );
			delete W;
			check(WWindow::_DeleteWindows.FindItemIndex(W)==INDEX_NONE);
		}
		unguard;
	}
};
IMPLEMENT_CLASS(UWindowManager);

/*-----------------------------------------------------------------------------
	Functions.
-----------------------------------------------------------------------------*/

WINDOW_API HBITMAP LoadFileToBitmap( const TCHAR* Filename, INT& SizeX, INT& SizeY )
{
	guard(LoadFileToBitmap);
	TArray<BYTE> Bitmap;
	if( appLoadFileToArray(Bitmap,Filename) )
	{
		HDC              hDC = GetDC(NULL);
		BITMAPFILEHEADER* FH = (BITMAPFILEHEADER*)&Bitmap(0);
		BITMAPINFO*       BM = (BITMAPINFO      *)(FH+1);
		BITMAPINFOHEADER* IH = (BITMAPINFOHEADER*)(FH+1);
		RGBQUAD*          RQ = (RGBQUAD         *)(IH+1);
		BYTE*             BY = (BYTE            *)(RQ+((PTRINT)(1<<IH->biBitCount)));
		SizeX                = IH->biWidth;
		SizeY                = IH->biHeight;
		HBITMAP      hBitmap = CreateDIBitmap( hDC, IH, CBM_INIT, BY, BM, DIB_RGB_COLORS );
		ReleaseDC( NULL, hDC );
		return hBitmap;
	}
	return NULL;
	unguard;
}

WINDOW_API void InitWindowing()
{
	guard(InitWindowing);
	GWindowManager = new UWindowManager;
	GWindowManager->AddToRoot();
	unguard;
}

WINDOW_API void GOnKillFocus( HWND hwnd )
{
	if( UTexture::__Client )
	{
		if( !hwnd )
			hwnd = ::GetForegroundWindow();
		DWORD ProcID;
		GetWindowThreadProcessId(hwnd, &ProcID);
		if( ProcID != GetCurrentProcessId() )
			UTexture::__Client->RestoreGamma();
	}
}


WINDOW_API void GOnSetFocus( HWND hwnd )
{
	if( UTexture::__Client )
	{
		DWORD ProcID;
		GetWindowThreadProcessId(hwnd, &ProcID);
		if( ProcID != GetCurrentProcessId() )
			UTexture::__Client->UpdateGamma();
	}
}

/*-----------------------------------------------------------------------------
	WWindow.
-----------------------------------------------------------------------------*/

// Use this procedure for modeless dialogs.
INT_PTR CALLBACK WWindow::StaticDlgProc( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	guard(WWindow::StaticDlgProc);
	INT i;
	for( i=0; i<_Windows.Num(); i++ )
		if( _Windows(i)->hWnd==hwndDlg )
			break;
	if( i==_Windows.Num() && uMsg==WM_INITDIALOG )
	{
		WWindow* WindowCreate = (WWindow*)lParam;
		check(WindowCreate);
		check(!WindowCreate->hWnd);
		WindowCreate->hWnd = hwndDlg;
		for( i=0; i<_Windows.Num(); i++ )
			if( _Windows(i)==WindowCreate )
				break;
		check(i<_Windows.Num());
	}
	if( i!=_Windows.Num() && !GIsCriticalError )
	{
		_Windows(i)->WndProc( uMsg, wParam, lParam );			
	}

	// Give up cycles.
	//
	::Sleep(0);

	return 0;
	unguard;
}

LONG APIENTRY WWindow::StaticWndProc( HWND hWnd, UINT Message, WPARAM wParam, LPARAM lParam )
{
	guard(WWindow::StaticProc);
	INT i;
	for( i=0; i<_Windows.Num(); i++ )
		if( _Windows(i)->hWnd==hWnd )
			break;

	// Disable autoplay while any Unreal windows are open.
	static UINT QueryCancelAutoPlay = RegisterWindowMessage(TEXT("QueryCancelAutoPlay"));
	if( Message == QueryCancelAutoPlay )
    {
		SetWindowLong(hWnd, DWLP_MSGRESULT, TRUE);
		return 1;
    }

	if( i==_Windows.Num() && (Message==WM_NCCREATE || Message==WM_INITDIALOG) )
	{
		WWindow* WindowCreate
		=	Message!=WM_NCCREATE
		?	(WWindow*)lParam
		:	(GetWindowLongX(hWnd,GWL_EXSTYLE) & WS_EX_MDICHILD)
		?	(WWindow*)((MDICREATESTRUCT*)((CREATESTRUCT*)lParam)->lpCreateParams)->lParam
		:	(WWindow*)((CREATESTRUCT*)lParam)->lpCreateParams;
		check(WindowCreate);
		check(!WindowCreate->hWnd);
		WindowCreate->hWnd = hWnd;
		for( i=0; i<_Windows.Num(); i++ )
			if( _Windows(i)==WindowCreate )
				break;
		check(i<_Windows.Num());
	}
	if( i==_Windows.Num() || GIsCriticalError )
	{
		// Gets through before WM_NCCREATE: WM_GETMINMAXINFO
		return DefWindowProcX( hWnd, Message, wParam, lParam );
	}
	else
	{
		return _Windows(i)->WndProc( Message, wParam, lParam );			
	}
	unguard;
}

WNDPROC WWindow::RegisterWindowClass( const TCHAR* Name, DWORD Style )
{
	guard(WWindow::RegisterWindowClass);
#if UNICODE
	if( GUnicodeOS )
	{
		WNDCLASSEXW Cls;
		appMemzero( &Cls, sizeof(Cls) );
		Cls.cbSize			= sizeof(Cls);
		Cls.style			= Style;
		Cls.lpfnWndProc		= (WNDPROC) StaticWndProc;
		Cls.hInstance		= hInstanceWindow;
		Cls.hIcon			= LoadIconIdX(hInstanceWindow,(GIsEditor?IDICON_Editor:IDICON_Mainframe));
		Cls.lpszClassName	= Name;
		Cls.hIconSm			= LoadIconIdX(hInstanceWindow,(GIsEditor?IDICON_Editor:IDICON_Mainframe));
		verify(RegisterClassExW( &Cls ));
	}
	else
#endif
	{
		WNDCLASSEXA Cls;
		appMemzero( &Cls, sizeof(Cls) );
		Cls.cbSize			= sizeof(Cls);
		Cls.style			= Style;
		Cls.lpfnWndProc		= (WNDPROC) StaticWndProc;
		Cls.hInstance		= hInstanceWindow;
		Cls.hIcon			= LoadIconIdX(hInstanceWindow,(GIsEditor?IDICON_Editor:IDICON_Mainframe));
		Cls.lpszClassName	= TCHAR_TO_ANSI(Name);
		Cls.hIconSm			= LoadIconIdX(hInstanceWindow,(GIsEditor?IDICON_Editor:IDICON_Mainframe));
		verify(RegisterClassExA( &Cls ));
	}
	return NULL;
	unguard;
}

#if WIN_OBJ
void WWindow::Destroy()
{
	guard(WWindow::Destroy);
	Super::Destroy();
	MaybeDestroy();
	bShow = FALSE;
	WWindow::_DeleteWindows.RemoveItem( this );
	unguard;
}
#else
WWindow::~WWindow()
{
	guard(WWindow:;~WWindow);
	MaybeDestroy();
	WWindow::_DeleteWindows.RemoveItem( this );
	unguard;
}
#endif

FRect WWindow::GetClientRect() const
{
	RECT R;
	::GetClientRect( hWnd, &R );
	return FRect( R );
}

void WWindow::MoveWindow( FRect R, UBOOL bRepaint )
{
	::MoveWindow( hWnd, R.Min.X, R.Min.Y, R.Width(), R.Height(), bRepaint );
}

void WWindow::MoveWindow( INT Left, INT Top, INT Width, INT Height, UBOOL bRepaint )
{
	::MoveWindow( hWnd, Left, Top, Width, Height, bRepaint );
}

void WWindow::ResizeWindow( INT Width, INT Height, UBOOL bRepaint )
{
	FRect rc = GetWindowRect();
	::MoveWindow( hWnd, rc.Min.X, rc.Min.Y, Width, Height, bRepaint );
}

FRect WWindow::GetWindowRect( UBOOL bConvert ) const
{
	RECT R;
	::GetWindowRect( hWnd, &R );
	if( bConvert )
		return OwnerWindow ? OwnerWindow->ScreenToClient(R) : FRect(R);
	else
		return FRect(R);
}

FPoint WWindow::ClientToScreen( const FPoint& InP )
{
	POINT P;
	P.x = InP.X;
	P.y = InP.Y;
	::ClientToScreen( hWnd, &P );
	return FPoint( P.x, P.y );
}

FPoint WWindow::ScreenToClient( const FPoint& InP )
{
	POINT P;
	P.x = InP.X;
	P.y = InP.Y;
	::ScreenToClient( hWnd, &P );
	return FPoint( P.x, P.y );
}

FRect WWindow::ClientToScreen( const FRect& InR )
{
	return FRect( ClientToScreen(InR.Min), ClientToScreen(InR.Max) );
}

FRect WWindow::ScreenToClient( const FRect& InR )
{
	return FRect( ScreenToClient(InR.Min), ScreenToClient(InR.Max) );
}

FPoint WWindow::GetCursorPos()
{
	FPoint Mouse;
	::GetCursorPos( Mouse );
	return ScreenToClient( Mouse );
}

void WWindow::Show( UBOOL Show )
{
	guard(WWindow::Show);
	bShow = Show;
	ShowWindow( hWnd, Show ? (::IsIconic(hWnd) ? SW_RESTORE : SW_SHOW) : SW_HIDE );
	unguard;
}

void WWindow::Serialize( FArchive& Ar )
{
	guard(WWindow::Serialize);
	//!!UObject interface.
	//!!Super::Serialize( Ar );
	Ar << PersistentName;
	unguard;
}

const TCHAR* WWindow::GetPackageName()
{
	return TEXT("Window");
}

void WWindow::DoDestroy()
{
	guard(WWindow::DoDestroy);
	if( NotifyHook )
		NotifyHook->NotifyDestroy( this );
	if( hWnd )
		DestroyWindow( *this );
	_Windows.RemoveItem( this );
	unguard;
}

LPARAM WWindow::WndProc( UINT Message, WPARAM wParam, LPARAM lParam )
{
	guard(WWindow::WndProc);
	try
	{
		LastwParam = wParam;
		LastlParam = lParam;

		// Message snoop.
		if( Snoop )
		{
			if( Message==WM_CHAR )
				Snoop->SnoopChar( this, wParam );
			else if( Message==WM_KEYDOWN )
				Snoop->SnoopKeyDown( this, wParam );
			else if( Message==WM_LBUTTONDOWN )
				Snoop->SnoopLeftMouseDown( this, FPoint(LOWORD(lParam),HIWORD(lParam)) );
			else if( Message==WM_RBUTTONDOWN )
				Snoop->SnoopRightMouseDown( this, FPoint(LOWORD(lParam),HIWORD(lParam)) );
		}

		// Special multi-window activation handling.
		if( !MdiChild && !ModalCount )
		{
			static UBOOL AppActive=0;
			if( Message==WM_ACTIVATEAPP )
			{
				AppActive = wParam;
				SendMessageX( hWnd, WM_NCACTIVATE, wParam, 0 );

			}
			else if( Message==WM_NCACTIVATE && AppActive && !wParam )
			{
				return 1;
			}
		}

		// Message processing.
		if( Message==WM_DESTROY ) { OnDestroy(); }
		else if( Message==WM_DRAWITEM )
		{
			DRAWITEMSTRUCT* Info = (DRAWITEMSTRUCT*)lParam;
			for( INT i=0; i<Controls.Num(); i++ )
				if( ((WWindow*)Controls(i))->hWnd==Info->hwndItem )
					{((WWindow*)Controls(i))->OnDrawItem(Info); break;}
			return 1;
		}
		else if( Message==WM_MEASUREITEM )
		{
			MEASUREITEMSTRUCT* Info = (MEASUREITEMSTRUCT*)lParam;
			for( INT i=0; i<Controls.Num(); i++ )
				if( ((WWindow*)Controls(i))->ControlId==Info->CtlID )
					{((WWindow*)Controls(i))->OnMeasureItem(Info); break;}
			return 1;
		}
		else if( Message==WM_CLOSE )
        {
            // gam ---
    	    if (!OnClose())
                return (0);
            // --- gam
        }
		else if( Message==WM_CHAR ) { OnChar( wParam ); }
		else if( Message==WM_KEYDOWN ) { OnKeyDown( wParam ); }
		else if( Message==WM_PAINT ) { OnPaint(); }
		else if( Message==WM_CREATE ) { OnCreate(); }
		else if( Message==WM_TIMER ) { OnTimer(); }
		else if( Message==WM_INITDIALOG ) { OnInitDialog(); }
		else if( Message==WM_ENTERIDLE ) { OnEnterIdle(); }
		else if( Message==WM_SETFOCUS )
		{
extern void GOnSetFocus(HWND);
			GOnSetFocus( (HWND)wParam );
			OnSetFocus( (HWND)wParam );
		}
		else if( Message==WM_ACTIVATE ) { OnActivate( LOWORD(wParam)!=0 ); }
		else if( Message==WM_KILLFOCUS )
		{
extern void GOnKillFocus(HWND);
			GOnKillFocus( (HWND)wParam );
			OnKillFocus( (HWND)wParam );
		}
		else if( Message==WM_SIZE ) { OnSize( wParam, LOWORD(lParam), HIWORD(lParam) ); }
		else if( Message==WM_WINDOWPOSCHANGING )
		{
			WINDOWPOS* wpos = (LPWINDOWPOS)lParam;
			OnWindowPosChanging( &wpos->x, &wpos->y, &wpos->cx, &wpos->cy );
		}
		else if( Message==WM_MOVE ) { OnMove( LOWORD(lParam), HIWORD(lParam) ); }
		else if( Message==WM_PASTE ) { OnPaste(); }
		else if( Message==WM_SHOWWINDOW ) { OnShowWindow( wParam ); }
		else if( Message==WM_COPYDATA ) { OnCopyData( (HWND)wParam, (COPYDATASTRUCT*)lParam ); }
		else if( Message==WM_CAPTURECHANGED ) { OnReleaseCapture(); }
		else if( Message==WM_MDIACTIVATE ) { OnMdiActivate( (HWND)lParam==hWnd ); }
		else if( Message==WM_MOUSEMOVE ) { OnMouseMove( wParam, FPoint(LOWORD(lParam), HIWORD(lParam)) ); }
		else if( Message==WM_LBUTTONDOWN ) { OnLeftButtonDown(); }
		else if( Message==WM_LBUTTONDBLCLK ) { OnLeftButtonDoubleClick(); }
		else if( Message==WM_MBUTTONDBLCLK ) { OnMiddleButtonDoubleClick(); }
		else if( Message==WM_RBUTTONDBLCLK ) { OnRightButtonDoubleClick(); }
		else if( Message==WM_RBUTTONDOWN ) { OnRightButtonDown(); }
		else if( Message==WM_LBUTTONUP ) { OnLeftButtonUp(); }
		else if( Message==WM_RBUTTONUP ) { OnRightButtonUp(); }
		else if( Message==WM_CUT ) { OnCut(); }
		else if( Message==WM_COPY ) { OnCopy(); }
		else if( Message==WM_UNDO ) { OnUndo(); }
		else if( Message==WM_ERASEBKGND ) { if( OnEraseBkgnd() ) return 1; }
		else if( Message==WM_SETCURSOR )
		{
			if( OnSetCursor() )
				return 1;
		}
		else if( Message==WM_NOTIFY )
		{
			for( INT i=0; i<Controls.Num(); i++ )
				if(wParam==((WWindow*)Controls(i))->ControlId
						&& ((WWindow*)Controls(i))->InterceptControlCommand(Message,wParam,lParam) )
					return 1;
			OnCommand( wParam );
		}
		else if( Message==WM_VSCROLL ) { OnVScroll( wParam, lParam ); }
		else if( Message==WM_KEYUP) { OnKeyUp( wParam, lParam ); }
		else if( Message==WM_COMMAND || Message==WM_HSCROLL )
		{
			// Allow for normal handling as well as control delegates
			if( Message==WM_HSCROLL )
				OnHScroll( wParam, lParam );

			for( INT i=0; i<Controls.Num(); i++ )
				if((HWND)lParam==((WWindow*)Controls(i))->hWnd
						&& ((WWindow*)Controls(i))->InterceptControlCommand(Message,wParam,lParam) )
					return 1;
			OnCommand( wParam );
		}
		else if( Message==WM_SYSCOMMAND )
		{
			if( OnSysCommand( wParam ) )
				return 1;
		}
		else if( Message==WM_UDN_GETHELPTOPIC )
		{
			return UDNHelpTopic;
		}
		return CallDefaultProc( Message, wParam, lParam );
	}
	catch( const TCHAR* )
	{
		// This exception prevents the message from being routed to the default proc.
		return 0;
	}
	unguard;
}

LPARAM WWindow::CallDefaultProc( UINT Message, WPARAM wParam, LPARAM lParam )
{
	if( MdiChild )
		return DefMDIChildProcX( hWnd, Message, wParam, lParam );
	else
		return DefWindowProcX( hWnd, Message, wParam, lParam );
}

UBOOL WWindow::InterceptControlCommand( UINT Message, WPARAM wParam, LPARAM lParam )
{
	return 0;
}

FString WWindow::GetText()
{
	guard(WWindow::GetText);
	check(hWnd);
	INT Length = GetLength();
#if UNICODE
	if( GUnicode && !GUnicodeOS )
	{
		ANSICHAR* ACh = (ANSICHAR*)appAlloca((Length+1)*sizeof(ANSICHAR));
		SendMessageA( *this, WM_GETTEXT, Length+1, (LPARAM)ACh );
		return appFromAnsi(ACh);
	}
	else
#endif
	{
		TCHAR* Text = (TCHAR*)appAlloca((Length+1)*sizeof(TCHAR));
		SendMessage( *this, WM_GETTEXT, Length+1, (LPARAM)Text );
		return Text;
	}
	unguard;
}

void WWindow::SetText( const TCHAR* Text )
{
	guard(WWindow::SetText);
	if( !::IsWindow( hWnd ) ) return;
	SendMessageLX( *this, WM_SETTEXT, 0, Text );
	unguard;
}

INT WWindow::GetLength()
{
	guard(WWindow::GetLength);
	check(hWnd);
	return SendMessageX( *this, WM_GETTEXTLENGTH, 0, 0 );
	unguard;
}

void WWindow::SetNotifyHook( FNotifyHook* InNotifyHook )
{
	guard(WWindow::SetNotifyHook);
	NotifyHook = InNotifyHook;
	unguard;
}

void WWindow::OnCopyData( HWND hWndSender, COPYDATASTRUCT* CD ) {}
void WWindow::OnSetFocus( HWND hWndLosingFocus ) {}
void WWindow::OnKillFocus( HWND hWndGaininFocus ) {}
void WWindow::OnSize( DWORD Flags, INT NewX, INT NewY ) {}
void WWindow::OnWindowPosChanging( INT* NewX, INT* NewY, INT* NewWidth, INT* NewHeight ) {}
void WWindow::OnMove( INT NewX, INT NewY ) {}
void WWindow::OnCommand( INT Command ) {}
INT WWindow::OnSysCommand( INT Command ) { return 0; }
void WWindow::OnActivate( UBOOL Active ) { VerifyPosition(); }
void WWindow::OnChar( TCHAR Ch ) {}
void WWindow::OnKeyDown( TCHAR Ch ) {}
void WWindow::OnCut() {}
void WWindow::OnCopy() {}
void WWindow::OnPaste() {}
void WWindow::OnShowWindow( UBOOL bShow ) {}
void WWindow::OnUndo() {}
UBOOL WWindow::OnEraseBkgnd() { return 0; }
void WWindow::OnVScroll( WPARAM wParam, LPARAM lParam ) {}
void WWindow::OnHScroll( WPARAM wParam, LPARAM lParam ) {}
void WWindow::OnKeyUp( WPARAM wParam, LPARAM lParam ) {}
void WWindow::OnPaint() {}
void WWindow::OnCreate() {}
void WWindow::OnDrawItem( DRAWITEMSTRUCT* Info ) {}
void WWindow::OnMeasureItem( MEASUREITEMSTRUCT* Info ) {}
void WWindow::OnInitDialog() {}
void WWindow::OnEnterIdle() {}
void WWindow::OnMouseEnter() {}
void WWindow::OnMouseLeave() {}
void WWindow::OnMouseHover() {}
void WWindow::OnTimer() {}
void WWindow::OnReleaseCapture() {}
void WWindow::OnMdiActivate( UBOOL Active ) {}
void WWindow::OnMouseMove( DWORD Flags, FPoint Location ) {}
void WWindow::OnLeftButtonDown() {}
void WWindow::OnLeftButtonDoubleClick() {}
void WWindow::OnMiddleButtonDoubleClick() {}
void WWindow::OnRightButtonDoubleClick() {}
void WWindow::OnRightButtonDown() {}
void WWindow::OnLeftButtonUp() {}
void WWindow::OnRightButtonUp() {}
void WWindow::OnFinishSplitterDrag( WDragInterceptor* Drag, UBOOL Success ) {}
INT WWindow::OnSetCursor() { return 0; }

bool WWindow::OnClose() // gam
{
	guard(WWindow::OnClose);
	if( MdiChild )
		SendMessage( OwnerWindow->hWnd, WM_MDIDESTROY, (WPARAM)hWnd, 0 );
	else if( hWnd )
	{
		// gam ---
		::DestroyWindow( hWnd ); 
		hWnd = NULL;
		// --- gam
	}
    return (true); // gam
	unguard;
}

void WWindow::OnDestroy()
{
	guard(WWindow::OnDestroy);
	check(hWnd);
	if( PersistentName!=NAME_None )
	{
		FRect R = GetWindowRect();
		if( !IsZoomed(hWnd) )
			GConfig->SetString( TEXT("WindowPositions"), *PersistentName, *FString::Printf( TEXT("(X=%i,Y=%i,XL=%i,YL=%i)"), R.Min.X, R.Min.Y, R.Width(), R.Height() ), TEXT("User.ini") ); //amb
	}
	_Windows.RemoveItem( this );
	hWnd = NULL;
	unguard;
}

void WWindow::SaveWindowPos()
{
	guard(WWindow::SaveWindowPos);
	unguard;
}

void WWindow::MaybeDestroy()
{
	guard(WWindow::MaybeDestroy);
	if( !Destroyed )
	{
		Destroyed=1;
		DoDestroy();
	}
	unguard;
}

void WWindow::_CloseWindow()
{
	guard(WWindow::_CloseWindow);
	check(hWnd);
	DestroyWindow( *this );
	unguard;
}

void WWindow::SetFont( HFONT hFont )
{
	guard(WWindow::SetFont);
	SendMessageX( *this, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(0,0) );
	unguard;
}
void WWindow::PerformCreateWindowEx( DWORD dwExStyle, LPCTSTR lpWindowName, DWORD dwStyle, INT x, INT y, INT nWidth, INT nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance )
{
	guard(PerformCreateWindowEx);
	check(hWnd==NULL);

	// Retrieve remembered position.
	TCHAR Pos[256];
	if
	(	PersistentName!=NAME_None 
	&&	GConfig->GetString( TEXT("WindowPositions"), *PersistentName, Pos, ARRAY_COUNT(Pos), TEXT("User.ini") ) ) //amb
	{
		// Get saved position.
		Parse( Pos, TEXT("X="), x );
		Parse( Pos, TEXT("Y="), y );
		if( (dwStyle&WS_SIZEBOX) || (dwStyle&WS_THICKFRAME) )
		{
			Parse( Pos, TEXT("XL="), nWidth );
			Parse( Pos, TEXT("YL="), nHeight );
		}

		// Count identical windows already opened.
		INT Count=0;
		for( INT i=0; i<_Windows.Num(); i++ )
		{
			Count += _Windows(i)->PersistentName==PersistentName;
		}
		if( Count )
		{
			// Move away.
			x += Count*16;
			y += Count*16;
		}

		VerifyPosition();
	}

	// Create window.
	_Windows.AddItem( this );
	TCHAR ClassName[256];
	GetWindowClassName( ClassName );
	//hinstance must match window class hinstance!!
	HWND hWndCreated = TCHAR_CALL_OS(CreateWindowEx(dwExStyle,ClassName,lpWindowName,dwStyle,x,y,nWidth,nHeight,hWndParent,hMenu,hInstanceWindow,this),CreateWindowExA(dwExStyle,TCHAR_TO_ANSI(ClassName),TCHAR_TO_ANSI(lpWindowName),dwStyle,x,y,nWidth,nHeight,hWndParent,hMenu,hInstanceWindow,this));
	if( !hWndCreated )
		appErrorf( TEXT("CreateWindowEx failed: %s"), appGetSystemErrorMessage() );
	check(hWndCreated);
	check(hWndCreated==hWnd);
	unguard;
}

// Makes sure the window is on the screen.  If it's off the screen, it moves it to the top/left edges.
void WWindow::VerifyPosition()
{
	RECT winrect;
	::GetWindowRect( hWnd, &winrect );
	RECT screenrect;
	screenrect.right = ::GetSystemMetrics( SM_CXVIRTUALSCREEN );
	screenrect.bottom = ::GetSystemMetrics( SM_CYVIRTUALSCREEN );

	if( winrect.left >= screenrect.right+4 || winrect.left < -4 )
		winrect.left = 0;
	if( winrect.top >= screenrect.bottom+4 || winrect.top < -4 )
		winrect.top = 0;

	::SetWindowPos( hWnd, HWND_TOP, winrect.left, winrect.top, winrect.right, winrect.bottom, SWP_NOSENDCHANGING | SWP_NOZORDER | SWP_NOSIZE );
}

void WWindow::SetRedraw( UBOOL Redraw )
{
	guard(WWindow::SetRedraw);
	SendMessageX( *this, WM_SETREDRAW, Redraw, 0 );
	unguard;
}

// Used in the editor ... used to draw window edges in custom colors.
void WWindow::MyDrawEdge( HDC hdc, LPRECT qrc, UBOOL bRaised )
{
	guard(WWindow::MyDrawEdge);

	HPEN penOld, penRaised = CreatePen( PS_SOLID, 1, RGB(159,159,159) ),
		penSunken = CreatePen( PS_SOLID, 1, RGB(106,106,106) );
	HDC	hDC = GetDC( hWnd );

	RECT rc = *qrc;
	rc.right -= 1;
	rc.bottom -= 1;

	penOld = (HPEN)SelectObject( hDC, (bRaised ? penRaised : penSunken ) );
	::MoveToEx( hDC, rc.left, rc.top, NULL );
	::LineTo( hDC, rc.right, rc.top );
	::MoveToEx( hDC, rc.left, rc.top, NULL );
	::LineTo( hDC, rc.left, rc.bottom);
	SelectObject( hDC, penOld );

	penOld = (HPEN)SelectObject( hDC, (bRaised ? penSunken : penRaised ) );
	::MoveToEx( hDC, rc.right, rc.bottom, NULL );
	::LineTo( hDC, rc.right, rc.top );
	::MoveToEx( hDC, rc.right, rc.bottom, NULL );
	::LineTo( hDC, rc.left, rc.bottom );
	SelectObject( hDC, penOld );

	DeleteObject( penRaised );
	DeleteObject( penSunken );
	::ReleaseDC( hWnd, hDC );

	unguard;
}

/*-----------------------------------------------------------------------------
	WControl.
-----------------------------------------------------------------------------*/

#if WIN_OBJ
void WControl::Destroy()
{
	Super::Destroy();
	check(OwnerWindow);
	OwnerWindow->Controls.RemoveItem( this );
}
#else
WControl::~WControl()
{
	if( OwnerWindow )
		OwnerWindow->Controls.RemoveItem( this );
}
#endif

// WWindow interface.
LPARAM WControl::CallDefaultProc( UINT Message, WPARAM wParam, LPARAM lParam )
{
	if( ::IsWindow( hWnd ) )
		return CallWindowProcX( WindowDefWndProc, hWnd, Message, wParam, lParam );
	else
		return 1;
}
WNDPROC WControl::RegisterWindowClass( const TCHAR* Name, const TCHAR* WinBaseClass )
{
	guard(WControl::RegisterWindowClass);
	WNDPROC SuperProc=NULL;
#if UNICODE
	if( GUnicodeOS )
	{
		WNDCLASSEXW Cls;
		appMemzero( &Cls, sizeof(Cls) );
		Cls.cbSize        = sizeof(Cls);
		verify( GetClassInfoExW( NULL, WinBaseClass, &Cls ) );
		SuperProc         = Cls.lpfnWndProc;
		Cls.lpfnWndProc   = (WNDPROC) WWindow::StaticWndProc;
		Cls.lpszClassName = Name;
		Cls.hInstance     = hInstanceWindow;
		check(Cls.lpszMenuName==NULL);
		verify(RegisterClassExW( &Cls ));
	}
	else
#endif
	{
		WNDCLASSEXA Cls;
		appMemzero( &Cls, sizeof(Cls) );
		Cls.cbSize        = sizeof(Cls);
		verify( GetClassInfoExA( NULL, TCHAR_TO_ANSI(WinBaseClass), &Cls ) );
		SuperProc         = Cls.lpfnWndProc;
		Cls.lpfnWndProc   = (WNDPROC) WWindow::StaticWndProc;
		Cls.lpszClassName = TCHAR_TO_ANSI(Name);
		Cls.hInstance     = hInstanceWindow;
		check(Cls.lpszMenuName==NULL);
		verify(RegisterClassExA( &Cls ));
	}
	return SuperProc;
	unguard;
}

/*-----------------------------------------------------------------------------
	WThinScrollBar.
-----------------------------------------------------------------------------*/

void WThinScrollBar::OpenWindow( INT X, INT Y, INT XL, INT YL)
{
	guard(WThinScrollBar::OpenWindow);

	PerformCreateWindowEx
	(
		0,
		NULL,
		WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		X, Y,
		XL, YL,
		OwnerWindow ? OwnerWindow->hWnd : NULL,
		NULL,
		hInstance
	);

	unguard;
}

void WThinScrollBar::OnLeftButtonDoubleClick()
{
	OnLeftButtonDown();
}

void WThinScrollBar::OnLeftButtonDown()
{
	guard(WThinScrollBar::OnLeftButtonDown);

	if( !IsWindowEnabled(hWnd)) return;

	FPoint pt = GetCursorPos();
	OldMouseLocation = pt;

	FRect ThumbRect = GetThumbRect();

	if( ThumbRect.Contains( pt ) )
	{
		ThumbBrush = hBrushCyanHighlight;
		SetCapture( hWnd );
	}
	else
	{
		ThumbBrush = hBrushCyanLow;

		if( pt.Y < ThumbRect.Min.Y )
			ThumbPosPct -= 10;
		else
			ThumbPosPct += 10;

		if( ThumbPosPct < 0 ) ThumbPosPct = 0;
		if( ThumbPosPct + ThumbSzPct > 100 ) ThumbPosPct = 100 - ThumbSzPct;

		PosChangedDelegate();
	}

	unguard;
}

void WThinScrollBar::OnMouseMove( DWORD Flags, FPoint MouseLocation )
{
	guard(WThinScrollBar::OnMouseMove);

	if( GetCapture() == hWnd )
	{
		FRect rc = GetClientRect();

		INT MouseDelta = MouseLocation.Y - OldMouseLocation.Y;
		if( MouseDelta != 0 )
		{
			FLOAT ThumbDeltaPct = MouseDelta / (FLOAT)rc.Max.Y;

			ThumbPosPct += ThumbDeltaPct * 100;
			if( ThumbPosPct < 0 ) ThumbPosPct = 0;
			if( ThumbPosPct + ThumbSzPct > 100 ) ThumbPosPct = 100 - ThumbSzPct;

			PosChangedDelegate();
		}

		OldMouseLocation = MouseLocation;
	}

	InvalidateRect( hWnd, NULL, 0 );

	unguard;
}

void WThinScrollBar::OnLeftButtonUp()
{
	guard(WThinScrollBar::OnLeftButtonUp);
	ThumbBrush = hBrushCyanLow;
	ReleaseCapture();

	InvalidateRect( hWnd, NULL, 0 );
	unguard;
}

void WThinScrollBar::OnDestroy()
{
	guard(WThinScrollBar::OnDestroy);
	WWindow::OnDestroy();
	unguard;
}

INT WThinScrollBar::OnSetCursor()
{
	guard(WThinScrollBar::OnSetCursor);
	WWindow::OnSetCursor();
	SetCursor(LoadCursorIdX(hInstanceWindow,IDC_HandGrab));
	return 1;
	unguard;
}

FRect WThinScrollBar::GetThumbRect()
{
	guard(WThinScrollBar::GetThumbRect);

	RECT rc;
	::GetClientRect( hWnd, &rc );

	rc.top = rc.bottom * (ThumbPosPct/100.f);
	rc.bottom  = rc.bottom  * ((ThumbPosPct + ThumbSzPct)/100.f);

	return FRect( rc );
	unguard;
}

void WThinScrollBar::OnPaint()
{
	guard(WThinScrollBar::OnPaint);

	PAINTSTRUCT PS;
	HDC hDC = BeginPaint( *this, &PS );

	// Background
	RECT rc;
	::GetClientRect( hWnd, &rc );
	FillRect( hDC, &rc, IsWindowEnabled(hWnd) ? hBrushBlack : hBrushGrey );
	MyDrawEdge( hDC, &rc, 1 );

	// Thumb
	if( IsWindowEnabled(hWnd) )
	{
		FRect rect = GetThumbRect();
		FillRect( hDC, (RECT*)rect, ThumbBrush );
	}

	EndPaint( *this, &PS );
	unguard;
}

INT WThinScrollBar::GetPos()
{
	return MaxVal * (ThumbPosPct / 100.f);
}

void WThinScrollBar::SetPos( INT InPos, FLOAT InThumbSzPct )
{
	check(MaxVal);

	ThumbPosPct = (InPos / (FLOAT)MaxVal) * 100.f;
	ThumbSzPct = InThumbSzPct;

	InvalidateRect( hWnd, NULL, 0 );
}

void WThinScrollBar::SetRange( INT InMaxVal )
{
	check( InMaxVal > 0 );
	MaxVal = InMaxVal;

	InvalidateRect( hWnd, NULL, 0 );
}

/*-----------------------------------------------------------------------------
	WTimeScrollBar.
-----------------------------------------------------------------------------*/

void WTimeScrollBar::OpenWindow( INT X, INT Y, INT XL )
{
	guard(WTimeScrollBar::OpenWindow);

	PerformCreateWindowEx
	(
		0,
		NULL,
		WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		X, Y,
		XL, 17,
		OwnerWindow ? OwnerWindow->hWnd : NULL,
		NULL,
		hInstance
	);

	unguard;
}

void WTimeScrollBar::OnLeftButtonDoubleClick()
{
	OnLeftButtonDown();
}

void WTimeScrollBar::OnLeftButtonDown()
{
	guard(WTimeScrollBar::OnLeftButtonDown);

	if( !IsWindowEnabled(hWnd)) return;

	FPoint pt = GetCursorPos();
	OldMouseLocation = pt;

	FRect ThumbRect = GetThumbRect();

	if( ThumbRect.Contains( pt ) )
	{
		SetCapture( hWnd );
	}
	else
	{
		if( pt.X < ThumbRect.Min.X )
			ThumbPosPct -= 10;
		else
			ThumbPosPct += 10;

		if( ThumbPosPct < 0 ) ThumbPosPct = 0;
		if( ThumbPosPct > 100 ) ThumbPosPct = 100;

		PosChangedDelegate();
	}

	unguard;
}

void WTimeScrollBar::OnMouseMove( DWORD Flags, FPoint MouseLocation )
{
	guard(WTimeScrollBar::OnMouseMove);

	if( GetCapture() == hWnd )
	{
		FRect rc = GetClientRect();

		INT MouseDelta = MouseLocation.X - OldMouseLocation.X;
		if( MouseDelta != 0 )
		{
			FLOAT ThumbDeltaPct = MouseDelta / (FLOAT)(rc.Max.X - ThumbInfo.bmWidth - 4);

			ThumbPosPct += ThumbDeltaPct * 100;
			if( ThumbPosPct < 0 )
				ThumbPosPct = 0;
			if( ThumbPosPct > 100 )
				ThumbPosPct = 100;

			PosChangedDelegate();
		}

		OldMouseLocation = MouseLocation;
	}

	InvalidateRect( hWnd, NULL, 0 );

	unguard;
}

void WTimeScrollBar::OnLeftButtonUp()
{
	guard(WTimeScrollBar::OnLeftButtonUp);
	ReleaseCapture();
	InvalidateRect( hWnd, NULL, 0 );
	unguard;
}

void WTimeScrollBar::OnDestroy()
{
	guard(WTimeScrollBar::OnDestroy);
	WWindow::OnDestroy();
	DeleteObject(hbmThumb);
	DeleteObject(hbmLeft);
	DeleteObject(hbmBody);
	DeleteObject(hbmRight);
	unguard;
}

INT WTimeScrollBar::OnSetCursor()
{
	guard(WTimeScrollBar::OnSetCursor);
	WWindow::OnSetCursor();
	SetCursor(LoadCursorIdX(NULL,IDC_ARROW));
	return 1;
	unguard;
}

FRect WTimeScrollBar::GetThumbRect()
{
	guard(WTimeScrollBar::GetThumbRect);

	RECT rc;
	::GetClientRect( hWnd, &rc );

	rc.top += 2;
	rc.bottom -= 2;
	rc.left = ((rc.right - ThumbInfo.bmWidth - 4) * (ThumbPosPct/100.f)) + 2;
	rc.right = rc.left + ThumbInfo.bmWidth;

	return FRect( rc );
	unguard;
}

void WTimeScrollBar::OnPaint()
{
	guard(WTimeScrollBar::OnPaint);

	PAINTSTRUCT PS;
	HDC hDC = BeginPaint( *this, &PS );

	RECT rc;
	::GetClientRect( hWnd, &rc );

	HDC hdcMem;
	HBITMAP hbmOld;

	hdcMem = CreateCompatibleDC(hDC);

	HBITMAP bmpBackBuffer = CreateCompatibleBitmap( hDC, rc.right, rc.bottom );
	HDC hdcBackBuffer = CreateCompatibleDC(hDC);
	SelectObject( hdcBackBuffer, bmpBackBuffer );

	// BODY
	hbmOld = (HBITMAP)SelectObject(hdcMem, hbmBody);

	StretchBlt(hdcBackBuffer,
		0, 0, rc.right, rc.bottom,
		hdcMem,
		0, 0, BodyInfo.bmWidth, BodyInfo.bmHeight,
		SRCCOPY);

	// LEFT
	SelectObject(hdcMem, hbmLeft);
	BitBlt(hdcBackBuffer, 0, 0, LeftInfo.bmWidth, LeftInfo.bmHeight, hdcMem, 0, 0, SRCCOPY);

	// RIGHT
	SelectObject(hdcMem, hbmRight);
	BitBlt(hdcBackBuffer, rc.right-1, 0, RightInfo.bmWidth, RightInfo.bmHeight, hdcMem, 0, 0, SRCCOPY);

	// THUMB
	SelectObject(hdcMem, hbmThumb);
	FRect rect = GetThumbRect();
	BitBlt(hdcBackBuffer, rect.Min.X, 2, ThumbInfo.bmWidth, ThumbInfo.bmHeight, hdcMem, 0, 0, SRCCOPY);

	// Blt the back buffer to the main DC
	BitBlt(hDC, 0, 0, rc.right, rc.bottom, hdcBackBuffer, 0, 0, SRCCOPY);

	// Clean up.
	//
	SelectObject( hdcMem, hbmOld );
	DeleteObject( bmpBackBuffer );
	DeleteDC(hdcMem);
	DeleteDC(hdcBackBuffer);

	EndPaint( *this, &PS );

	unguard;
}

INT WTimeScrollBar::GetPos()
{
	return MaxVal * (ThumbPosPct / 100.f);
}

FLOAT WTimeScrollBar::GetPct()
{
	return (ThumbPosPct / 100.f);
}

void WTimeScrollBar::SetPos( INT InPos )
{
	check(MaxVal);

	ThumbPosPct = (InPos / (FLOAT)MaxVal) * 100.f;
	InvalidateRect( hWnd, NULL, 0 );
}

void WTimeScrollBar::SetPct( FLOAT InPct )
{
	InPct = Max<FLOAT>( InPct, 0.f );
	InPct = Min<FLOAT>( InPct, 100.f );

	ThumbPosPct = InPct;
	InvalidateRect( hWnd, NULL, 0 );
}

void WTimeScrollBar::SetRange( INT InMaxVal )
{
	check( InMaxVal > 0 );
	MaxVal = InMaxVal;

	InvalidateRect( hWnd, NULL, 0 );
}

void WTimeScrollBar::OnRightButtonDown()
{
	guard(WCheckBox::OnRightButtonDown);

	FPoint pt = GetCursorPos();
	FRect ThumbRect = GetThumbRect();
	if( ThumbRect.Contains( pt ) )
		SendMessageX( OwnerWindow->hWnd, WM_COMMAND, WM_EDC_RTCLICKMATINEETIMESLIDER, ControlId );
	unguard;
}

/*-----------------------------------------------------------------------------
	WUDNWindow.
-----------------------------------------------------------------------------*/

void WUDNWindow::OpenWindow()
{
	guard(WUDNWindow::OpenWindow);
	MdiChild = 0;

	PerformCreateWindowEx
	(
		0,
		NULL,
		WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		0,
		0,
		1,
		1,
		OwnerWindow->hWnd,
		NULL,
		hInstance
	);
	Show(0);

	unguard;
}

void WUDNWindow::Capture()
{
	Show(1);
	SetCapture( hWnd );
	SetCursor( LoadCursor(NULL, IDC_HELP) );
	SetFocus( hWnd );
}

void WUDNWindow::Release()
{
	ReleaseCapture();
	Show(0);
}

void WUDNWindow::OnKeyDown( TCHAR Ch )
{
	// Hitting ESC will abort the context help
	if( Ch == VK_ESCAPE )
		Release();
	else
		WWindow::OnKeyDown( Ch );
}

void WUDNWindow::OnMouseMove( DWORD Flags, FPoint Location )
{
	if( GetCurrentTopicID() )
		SetCursor( LoadCursorIdX(hInstance, IDC_UDN_HelpBig) );
	else
		SetCursor( LoadCursorIdX(hInstance, IDC_UDN_Help) );

	WWindow::OnMouseMove( Flags, Location );
}

INT WUDNWindow::GetCurrentTopicID()
{
	// Figure out which window was clicked and get it's UDN topic ID.
	POINT pt;
	::GetCursorPos( & pt );
	HWND wnd = ::WindowFromPoint( pt );
	return SendMessageX( wnd, WM_UDN_GETHELPTOPIC, 0, 0 );
}

void WUDNWindow::OnLeftButtonDown()
{
	INT Topic = GetCurrentTopicID();

	if( Topic != 0 )
	{
		// Read UDNHelpTopics.ini and get a list of all the help topics associated with this ID
		TArray<FUDNHelpTopic*> HelpTopics;

		for( INT x = 0 ; x < 50 ; x++ )	//!! 50 is a magic number, a guess at the upper limit of help topics for any given ID
		{
			FString TopicKey = FString::Printf( TEXT("%d[%d]"), Topic, x ), Value;
			if( GConfig->GetString( TEXT("Topics"), *TopicKey, Value, TEXT("UDNHelpTopics.ini") ) )
			{
				TArray<FString> Fields;
				if( Value.ParseIntoArray( TEXT(","), &Fields) == 2 )
				{
					HelpTopics.AddItem( new FUDNHelpTopic( Fields(0), Fields(1) ) );
				}
			}
			else
				break;
		}

		if( HelpTopics.Num() )
		{
			INT TopicIdx = 0;

			// If we have more than one help topic, present the user with a popup menu to select the
			// specific help topic they want.
			if( HelpTopics.Num() > 1 )
			{
				HMENU menu = CreatePopupMenu();
				MENUITEMINFOA mif;

				mif.cbSize = sizeof(MENUITEMINFO);
				mif.fMask = MIIM_TYPE | MIIM_ID;
				mif.fType = MFT_STRING;

				for( INT x = 0 ; x < HelpTopics.Num() ; ++x )
				{
					mif.dwTypeData = TCHAR_TO_ANSI( *(HelpTopics(x)->MenuDesc) );
					mif.wID = 19500+x;

					InsertMenuItemA( menu, 99999, FALSE, &mif );
				}

				POINT pt;
				::GetCursorPos( & pt );
				INT ID = TrackPopupMenu( menu,
					TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
					pt.x, pt.y, 0,
					hWnd, NULL);
				DestroyMenu( menu );

				TopicIdx = ID - 19500;
			}

			// Tell windows to display the selected URL
			if( TopicIdx > -1 )
			{
				FString URL = FString::Printf(TEXT("http://udn.epicgames.com/bin/view/Content/%s"), *HelpTopics(TopicIdx)->URL );
				::ShellExecute( NULL, TEXT("open"), *URL, NULL, NULL, SW_SHOWNORMAL );
			}

			// Clean up
			for( INT x = 0 ; x < HelpTopics.Num() ; ++x )
				delete HelpTopics(x);
			HelpTopics.Empty();
		}
		else
			appMsgf(0,TEXT("Help topic not found (%d)."), Topic);
	}
	else
		appMsgf(0,TEXT("Sorry, there's no help topic for the selected item."));

	Release();
}

/*-----------------------------------------------------------------------------
	WEditTerminal.
-----------------------------------------------------------------------------*/

void WEditTerminal::OnChar( TCHAR Ch )
{
	if( Ch!=('C'-'@') )
	{
		OwnerTerminal->TypeChar( Ch );
		throw TEXT("NoRoute");
	}
}

void WEditTerminal::OnRightButtonDown()
{
	throw TEXT("NoRoute");
}

void WEditTerminal::OnPaste()
{
	OwnerTerminal->Paste();
	throw TEXT("NoRoute");
}

void WEditTerminal::OnUndo()
{
	throw TEXT("NoRoute");
}

/*-----------------------------------------------------------------------------
	WTerminal.
-----------------------------------------------------------------------------*/

void WTerminal::Serialize( const TCHAR* Data, EName MsgType )
{
	guard(WTerminal::Serialize);
	if( MsgType==NAME_Title )
	{
		SetText( Data );
		return;
	}
	else if( Shown )
	{
		Display.SetRedraw( 0 );
		INT LineCount = Display.GetLineCount();
		if( LineCount > MaxLines )
		{
			INT NewLineCount = Max(LineCount-SlackLines,0);
			INT Index = Display.GetLineIndex( LineCount-NewLineCount );
			Display.SetSelection( 0, Index );
			Display.SetSelectedText( TEXT("") );
			INT Length = Display.GetLength();
			Display.SetSelection( Length, Length );
			Display.ScrollCaret();
		}
		TCHAR Temp[1024]=TEXT("");
		appStrncat( Temp, *FName(MsgType), ARRAY_COUNT(Temp) );
		appStrncat( Temp, TEXT(": "), ARRAY_COUNT(Temp) );
		appStrncat( Temp, (TCHAR*)Data, ARRAY_COUNT(Temp) );
		appStrncat( Temp, TEXT("\r\n"), ARRAY_COUNT(Temp) );
		appStrncat( Temp, Typing, ARRAY_COUNT(Temp) );
		Temp[ARRAY_COUNT(Temp)-1] = 0;
		SelectTyping();
		Display.SetRedraw( 1 );
		Display.SetSelectedText( Temp );
	}
	unguard;
}

void WTerminal::OnShowWindow( UBOOL bShow )
{
	guard(WTerminal::OnShowWindow);
	WWindow::OnShowWindow( bShow );
	Shown = bShow;
	unguard;
}

void WTerminal::OnCreate()
{
	guard(WTerminal::OnCreate);
	WWindow::OnCreate();
	Display.OpenWindow( 1, 1, 1 );

	Display.SetFont( (HFONT)GetStockObject(DEFAULT_GUI_FONT) );
	Display.SetText( Typing );
	unguard;
}

void WTerminal::OpenWindow( UBOOL bMdi, UBOOL AppWindow )
{
	guard(WTerminal::OpenWindow);
	MdiChild = bMdi;
	PerformCreateWindowEx
	(
		MdiChild
		?	(WS_EX_MDICHILD)
		:	(AppWindow?WS_EX_APPWINDOW:0),
		*FString::Printf( LocalizeGeneral("LogWindow",TEXT("Window")), LocalizeGeneral("Product",TEXT("Core")) ),
		MdiChild
		?	(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
		:	(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX),
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		512,
		256,
		OwnerWindow ? OwnerWindow->hWnd : NULL,
		NULL,
		hInstance
	);
	unguard;
}

void WTerminal::OnSetFocus( HWND hWndLoser )
{
	guard(WTerminal::OnSetFocus);
	WWindow::OnSetFocus( hWndLoser );
	SetFocus( Display );
	unguard;
}

void WTerminal::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	guard(WTerminal::OnSize);
	WWindow::OnSize( Flags, NewX, NewY );
	Display.MoveWindow( FRect(0,0,NewX,NewY), TRUE );
	Display.ScrollCaret();
	unguard;
}

void WTerminal::Paste()
{
	guard(WTerminal::Paste);
	SelectTyping();
	FString Str = appClipboardPaste();
	appStrncat( Typing, *Str, ARRAY_COUNT(Typing) );
	Typing[ARRAY_COUNT(Typing)-1]=0;
	for( INT i=0; Typing[i]; i++ )
		if( Typing[i]<32 || Typing[i]>=127 )
			Typing[i] = 0;
	UpdateTyping();
	unguard;
}

void WTerminal::TypeChar( TCHAR Ch )
{
	guard(WTerminal::TypeChar);
	SelectTyping();
	INT Length = appStrlen(Typing);
	if( Ch>=32 )
	{
		if( Length<ARRAY_COUNT(Typing)-1 )
		{
			Typing[Length]=Ch;
			Typing[Length+1]=0;
		}
	}
	else if( Ch==13 && Length>1 )
	{
		UpdateTyping();
		Display.SetSelectedText( TEXT("\r\n>") );
		TCHAR Temp[ARRAY_COUNT(Typing)];
		appStrcpy( Temp, Typing+1 );
		appStrcpy( Typing, TEXT(">") );
		if( Exec )
			if( !Exec->Exec( Temp, *GLog ) )
				Log( LocalizeError("Exec",TEXT("Core")) );
		SelectTyping();
	}
	else if( (Ch==8 || Ch==127) && Length>1 )
	{
		Typing[Length-1] = 0;
	}
	else if( Ch==27 )
	{
		appStrcpy( Typing, TEXT(">") );
	}
	UpdateTyping();
	if( Ch==22 )
	{
		Paste();
	}
	unguard;
}

void WTerminal::SelectTyping()
{
	guard(WTerminal::SelectTyping);
	INT Length = Display.GetLength();
	Display.SetSelection( Max(Length-appStrlen(Typing),0), Length );
	unguard;
}

void WTerminal::UpdateTyping()
{
	guard(WTerminal::UpdateTyping);
	Display.SetSelectedText( Typing );
	unguard;
}

void WTerminal::SetExec( FExec* InExec )
{
	Exec = InExec;
}

/*-----------------------------------------------------------------------------
	WLog.
-----------------------------------------------------------------------------*/

// A log window.
static void GNotifyExit()
{
	if( GNotify )
		TCHAR_CALL_OS(Shell_NotifyIconWX(NIM_DELETE,&NID),Shell_NotifyIconA(NIM_DELETE,&NIDA));
}

void WLog::OnCreate()
{
	guard(WLog::OnCreate);
	WWindow::OnCreate();
	Display.OpenWindow( 1, 1, 1 );
	Display.SetFont( (HFONT)GetStockObject(ANSI_FIXED_FONT) );
	Display.SetText( Typing );
	unguard;
}

void WLog::SetText( const TCHAR* Text )
{
	guard(WLog::SetText);
	WWindow::SetText( Text );
	if( GNotify )
	{
#if UNICODE
		if( GUnicode && !GUnicodeOS )
		{
			appMemcpy( NIDA.szTip, TCHAR_TO_ANSI(Text), Min<INT>(ARRAY_COUNT(NIDA.szTip),appStrlen(Text)+1) );
			NIDA.szTip[ARRAY_COUNT(NIDA.szTip)-1]=0;
			Shell_NotifyIconA( NIM_MODIFY, &NIDA );
		}
		else
#endif
		{
			appStrncpy( NID.szTip, Text, ARRAY_COUNT(NID.szTip) );
#if UNICODE
			Shell_NotifyIconWX(NIM_MODIFY,&NID);
#else
			Shell_NotifyIconA(NIM_MODIFY,&NID);
#endif
		}
	}
	unguard;
}

void WLog::OnShowWindow( UBOOL bShow )
{
	guard(WLog::OnShowWindow);
	WTerminal::OnShowWindow( bShow );
	if( bShow )
	{
		// Load log file.
		if( LogAr )
		{
			delete LogAr;
			FArchive* Reader = GFileManager->CreateFileReader( *LogFilename );
			if( Reader )
			{
#if FORCE_ANSI_LOG
				TArray<ANSICHAR> AnsiText( Reader->TotalSize() );
				Reader->Serialize( &AnsiText(0), AnsiText.Num() );
				delete Reader;
				INT CrCount=0;
				INT Ofs;
				for( Ofs=AnsiText.Num()-1; Ofs>0 && CrCount<MaxLines; Ofs-- )
					CrCount += (AnsiText(Ofs)=='\n');
				while( Ofs<AnsiText.Num() && AnsiText(Ofs)=='\n' )
					Ofs++;
				AnsiText.AddItem( '>' );
				AnsiText.AddItem( 0 );
				TArray<TCHAR> Text( AnsiText.Num() );
				for( INT i=0; i<AnsiText.Num(); i++ )
					Text( i ) = FromAnsi( AnsiText(i) );
				Display.SetText( &Text(Ofs) );
#else
				TArray<TCHAR> Text( Reader->TotalSize() );
				Reader->Serialize( &Text(0), Text.Num()/sizeof(TCHAR)-GUnicode*2 );
				delete Reader;
				INT CrCount=0;
				for( INT Ofs=Text.Num()-1; Ofs>0 && CrCount<MaxLines; Ofs-- )
					CrCount += (Text(Ofs)=='\n');
				while( Ofs<Text.Num() && Text(Ofs)=='\n' )
					Ofs++;
				Text.AddItem( '>' );
				Text.AddItem( 0 );
				Display.SetText( &Text(Ofs) );
#endif
			}
			LogAr = GFileManager->CreateFileWriter( *LogFilename, FILEWRITE_Unbuffered|FILEWRITE_Append );
		}
		INT Length = Display.GetLength();
		Display.SetSelection( Length, Length );
		Display.ScrollCaret();
	}
	unguard;
}

void WLog::OpenWindow( UBOOL bShow, UBOOL bMdi )
{
	guard(WLog::OpenWindow);

	WTerminal::OpenWindow( bMdi, 0 );
	Show( bShow );
	UpdateWindow( *this );
	GLogHook = this;

	// Show dedicated server in tray.
	if( !GIsClient && !GIsEditor )
	{
		NID.cbSize           = sizeof(NID);
		NID.hWnd             = hWnd;
		NID.uID              = 0;
		NID.uFlags           = NIF_ICON | NIF_TIP | NIF_MESSAGE;
		NID.uCallbackMessage = NidMessage;
		NID.hIcon            = LoadIconIdX(hInstanceWindow,(GIsEditor?IDICON_Editor:IDICON_Mainframe));
		NID.szTip[0]         = 0;
#if UNICODE
		if( GUnicode && !GUnicodeOS )
		{
			NIDA.cbSize           = sizeof(NIDA);
			NIDA.hWnd             = hWnd;
			NIDA.uID              = 0;
			NIDA.uFlags           = NIF_ICON | NIF_TIP | NIF_MESSAGE;
			NIDA.uCallbackMessage = NidMessage;
			NIDA.hIcon            = LoadIconIdX(hInstanceWindow,(GIsEditor?IDICON_Editor:IDICON_Mainframe));
			NIDA.szTip[0]         = 0;
			Shell_NotifyIconA(NIM_ADD,&NIDA);
		}
		else
#endif
		{
#if UNICODE
			Shell_NotifyIconWX(NIM_ADD,&NID);
#else
			Shell_NotifyIconA(NIM_ADD,&NID);
#endif
		}
		GNotify = 1;
		atexit( GNotifyExit );
	}

	unguard;
}

void WLog::OnDestroy()
{
	guard(WLog::OnDestroy);

	GLogHook = NULL;
	WTerminal::OnDestroy();

	unguard;
}

void WLog::OnCopyData( HWND hWndSender, COPYDATASTRUCT* CD )
{
	guard(OnCopyData);
	if( Exec )
	{
		debugf( TEXT("WM_COPYDATA: %s"), (TCHAR*)CD->lpData );
		Exec->Exec( TEXT("TakeFocus"), *GLogWindow );
		TCHAR NewURL[1024];
		if
		(	ParseToken(*(const TCHAR**)&CD->lpData,NewURL,ARRAY_COUNT(NewURL),0)
		&&	NewURL[0]!='-')
			Exec->Exec( *(US+TEXT("Open ")+NewURL),*GLogWindow );
	}
	unguard;
}

bool WLog::OnClose() // gam
{
	guard(WLog::OnClose);
	Show( 0 );
	throw TEXT("NoRoute");
    return (true); // gam
	unguard;
}

void WLog::OnCommand( INT Command )
{
	guard(WLog::OnCommand);
	if( Command==ID_LogFileExit || Command==ID_NotifyExit )
	{
		// Exit.
		debugf( TEXT("WLog::OnCommand %s"), Command==ID_LogFileExit ? TEXT("ID_LogFileExit") : TEXT("ID_NotifyExit") );
		appRequestExit( 0 );
	}
	else if( Command==ID_LogAdvancedOptions || Command==ID_NotifyAdvancedOptions )
	{
		// Advanced options.
		if( Exec )
			Exec->Exec( TEXT("PREFERENCES"), *GLogWindow );
	}
	else if( Command==ID_NotifyShowLog )
	{
		// Show log window.
		ShowWindow( hWnd, SW_SHOWNORMAL );
		SetForegroundWindow( hWnd );
	}
	unguard;
}

LPARAM WLog::WndProc( UINT Message, WPARAM wParam, LPARAM lParam )
{
	guard(WLog::WndProc);
	if( Message==NidMessage )
	{
		if( lParam==WM_RBUTTONDOWN || lParam==WM_LBUTTONDOWN )
		{
			// Options.
			POINT P;
			::GetCursorPos( &P );
			HMENU hMenu = LoadLocalizedMenu( hInstanceWindow, IDMENU_NotifyIcon, TEXT("IDMENU_NotifyIcon"), TEXT("Window") );
			SetForegroundWindow( hWnd );
			TrackPopupMenu( GetSubMenu(hMenu,0), lParam==WM_LBUTTONDOWN ? TPM_LEFTBUTTON : TPM_RIGHTBUTTON, P.x, P.y, 0, hWnd, NULL );
			DestroyMenu( hMenu );
			PostMessageX( hWnd, WM_NULL, 0, 0 );
		}
		return 1;
	}
	else return WWindow::WndProc( Message, wParam, lParam );
	unguard;
}

/*-----------------------------------------------------------------------------
	WDialog.
-----------------------------------------------------------------------------*/

LPARAM WDialog::CallDefaultProc( UINT Message, WPARAM wParam, LPARAM lParam )
{
	return 0;
}

INT WDialog::DoModal( HINSTANCE hInst )
{
	guard(WDialog::DoModal);
	check(hWnd==NULL);
	_Windows.AddItem( this );
	ModalCount++;
	INT Result = TCHAR_CALL_OS(DialogBoxParamW(hInst/*!!*/,MAKEINTRESOURCEW(ControlId),OwnerWindow?OwnerWindow->hWnd:NULL,(DLGPROC)StaticWndProc,(LPARAM)this),DialogBoxParamA(hInst/*!!*/,MAKEINTRESOURCEA(ControlId),OwnerWindow?OwnerWindow->hWnd:NULL,(DLGPROC)StaticWndProc,(LPARAM)this));
	ModalCount--;
	return Result;
	unguard;
}

void WDialog::OpenChildWindow( INT InControlId, UBOOL Visible )
{
	guard(WDialog::OpenChildWindow);
	check(!hWnd);
	_Windows.AddItem( this );
	HWND hWndParent = InControlId ? GetDlgItem(OwnerWindow->hWnd,InControlId) : OwnerWindow ? OwnerWindow->hWnd : NULL;
	HWND hWndCreated = TCHAR_CALL_OS(CreateDialogParamW(hInstanceWindow/*!!*/,MAKEINTRESOURCEW(ControlId),hWndParent,(DLGPROC)StaticWndProc,(LPARAM)this),CreateDialogParamA(hInstanceWindow/*!!*/,MAKEINTRESOURCEA(ControlId),hWndParent,(DLGPROC)StaticWndProc,(LPARAM)this));
	verify(hWndCreated); // gam
	check(hWndCreated==hWnd);
	Show( Visible );
	unguard;
}

BOOL CALLBACK WDialog::LocalizeTextEnumProc( HWND hInWmd, LPARAM lParam )
{
	guard(WDialog::LocalizeTextEnumProc);
	FString String;
	TCHAR** Temp = (TCHAR**)lParam;
#if UNICODE
	if( !GUnicodeOS )
	{
		ANSICHAR ACh[1024]="";
		SendMessageA( hInWmd, WM_GETTEXT, ARRAY_COUNT(ACh), (LPARAM)ACh );
		String = appFromAnsi(ACh);
	}
	else
#endif
	{
		TCHAR Ch[1024]=TEXT("");
		SendMessage( hInWmd, WM_GETTEXT, ARRAY_COUNT(Ch), (LPARAM)Ch );
		String = Ch;
	}
	if( FString(String).Left(4)==TEXT("IDC_") )
		SendMessageLX( hInWmd, WM_SETTEXT, 0, LineFormat(Localize(Temp[0],*String,Temp[1])) );
	else if( String==TEXT("IDOK") )
		SendMessageLX( hInWmd, WM_SETTEXT, 0, LineFormat(LocalizeGeneral("OkButton",TEXT("Window"))) );
	else if( String==TEXT("IDCANCEL") )
		SendMessageLX( hInWmd, WM_SETTEXT, 0, LineFormat(LocalizeGeneral("CancelButton",TEXT("Window"))) );
	SendMessageX( hInWmd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(1,0) );
	return 1;
	unguard;
}

void WDialog::LocalizeText( const TCHAR* Section, const TCHAR* Package )
{
	guard(WDialog::LocalizeText);
	const TCHAR* Temp[3];
	Temp[0] = Section;
	Temp[1] = Package;
	Temp[2] = (TCHAR*)this;
	EnumChildWindows( *this, LocalizeTextEnumProc, (LPARAM)Temp );
	LocalizeTextEnumProc( hWnd, (LPARAM)Temp );
	unguard;
}

void WDialog::OnInitDialog()
{
	guard(WDialog::OnInitDialog);
	WWindow::OnInitDialog();
	SendMessageX( hWnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(1,0) );
	for( INT i=0; i<Controls.Num(); i++ )
	{
		// Bind all child controls.
		WControl* Control = Controls(i);
		check(!Control->hWnd);
		Control->hWnd = GetDlgItem( *this, Control->ControlId );
		check(Control->hWnd);
		_Windows.AddItem(Control);
		Control->WindowDefWndProc = (WNDPROC)GetWindowLongX( Control->hWnd, GWLP_WNDPROC );
		SetWindowLongX( Control->hWnd, GWLP_WNDPROC, (LONG_PTR)WWindow::StaticWndProc );
		//warning: Don't set GWL_HINSTANCE, it screws up subclassed edit controls in Win95.
	}
	for( INT i=0; i<Controls.Num(); i++ )
	{
		// Send create to all controls.
		Controls(i)->OnCreate();
	}
	TCHAR Temp[256];
	appSprintf( Temp, TEXT("IDDIALOG_%s"), *PersistentName );
	LocalizeText( Temp, GetPackageName() );
	unguard;
}

void WDialog::EndDialog( INT Result )
{
	::EndDialog( hWnd, Result );
}

void WDialog::EndDialogTrue()
{
	EndDialog( 1 );
}

void WDialog::EndDialogFalse()
{
	EndDialog( 0 );
}

void WDialog::CenterInOwnerWindow()
{
	// Center the dialog inside of it's parent window (if it has one)
	if( OwnerWindow )
	{
		FRect rc = GetWindowRect( 0 ),
			rcOwner = OwnerWindow->GetWindowRect();

		int X = ((rcOwner.Width() - rc.Width() ) / 2),
			Y = ((rcOwner.Height() - rc.Height() ) / 2);
		::SetWindowPos( hWnd, HWND_TOP, X, Y, 0, 0, SWP_NOSIZE );
	}
}

void WDialog::Show( UBOOL Show )
{
	WWindow::Show(Show);
	if( Show )
		BringWindowToTop( hWnd );
}

/*-----------------------------------------------------------------------------
	WDragInterceptor.
-----------------------------------------------------------------------------*/

void WDragInterceptor::OpenWindow()
{
	guard(WDragInterceptor::OpenWindow);
	PerformCreateWindowEx( 0, NULL, WS_CHILD | WS_VISIBLE, 0, 0, 0, 0, *OwnerWindow, NULL, hInstance );
	OldMouseLocation = OwnerWindow->GetCursorPos();
	DragStart = DragPos;
	SetCapture( *this );
	SetFocus( *this );
	ClipCursor( ClientToScreen(DragClamp-DragPos+OwnerWindow->GetCursorPos()) );
	ToggleDraw( NULL );
	unguard;
}

void WDragInterceptor::ToggleDraw( HDC hInDC )
{
	guard(WDragInterceptor::ToggleDraw);

	HDC hDC = hInDC ? hInDC : GetDC(*OwnerWindow);
	HBRUSH OldBrush = (HBRUSH)SelectObject( hDC, hBrushWhite );
	if( DragIndices.X!=INDEX_NONE )
		PatBlt( hDC, DragPos.X, 0, DrawWidth.X, OwnerWindow->GetClientRect().Height(), PATINVERT );
	if( DragIndices.Y!=INDEX_NONE )
		PatBlt( hDC, 0, DragPos.Y, OwnerWindow->GetClientRect().Width(), DrawWidth.Y, PATINVERT );
	if( !hInDC )
		ReleaseDC( hWnd, hDC );
	SelectObject( hDC, OldBrush );

	unguard;
}

void WDragInterceptor::OnKeyDown( TCHAR Ch )
{
	if( Ch==VK_ESCAPE )
	{
		Success = 0;
		ReleaseCapture();
	}
}

void WDragInterceptor::OnMouseMove( DWORD Flags, FPoint MouseLocation )
{
	guard(WDragInterceptor::OnMouseMove);
	ToggleDraw( NULL );
	for( INT i=0; i<FPoint::Num(); i++ )
		if( DragIndices(i)!=INDEX_NONE )
			DragPos(i) = Clamp( DragPos(i) + MouseLocation(i) - OldMouseLocation(i), DragClamp.Min(i), DragClamp.Max(i) );
	ToggleDraw( NULL );
	OldMouseLocation = MouseLocation;
	unguard;
}

void WDragInterceptor::OnReleaseCapture()
{
	guard(WDragInterceptor::OnReleaseCapture);
	ClipCursor( NULL );
	ToggleDraw( NULL );
	OwnerWindow->OnFinishSplitterDrag( this, Success );
	DestroyWindow( *this );
	unguard;
}

void WDragInterceptor::OnLeftButtonUp()
{
	guard(WDragInterceptor::OnLeftButtonUp);
	ReleaseCapture();
	unguard;
}

/*-----------------------------------------------------------------------------
	WSplitterPane.
-----------------------------------------------------------------------------*/

void WSplitterPane::OpenWindow()
{
	guard(WSplitterPane::OpenWindow);
	PerformCreateWindowEx
	(
		0,
		NULL,
		WS_CHILD | WS_VISIBLE,
		0, 0,
		0, 0,
		OwnerWindow->hWnd,
		NULL,
		hInstance
	);
	SendMessageX( hWnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(0,0) );
	unguard;
}

void WSplitterPane::OnPaint()
{
	guard(WSplitterPane::OnPaint);
	PAINTSTRUCT PS;
	HDC hDC = BeginPaint( *this, &PS );

	FRect Rect = GetClientRect();
	FillRect( hDC, Rect, GetSysColorBrush( COLOR_3DFACE ) );

	EndPaint( *this, &PS );
	unguard;
}

/*-----------------------------------------------------------------------------
	WSplitter.
-----------------------------------------------------------------------------*/

void WSplitter::OpenWindow( UBOOL InVertical )
{
	guard(WSplitter::OpenWindow);
	PerformCreateWindowEx
	(
		0,
		NULL,
		WS_CHILD | WS_VISIBLE,
		0, 0,
		0, 0,
		OwnerWindow->hWnd,
		NULL,
		hInstance
	);
	SendMessageX( hWnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(0,0) );

	bVertical = InVertical;
	unguard;
}

void WSplitter::OnPaint()
{
	guard(WSplitter::OnPaint);
	PAINTSTRUCT PS;
	HDC hDC = BeginPaint( *this, &PS );

	FRect Rect = GetClientRect();
	FillRect( hDC, Rect, hBrushGreyWindow );
	DrawEdge( hDC, Rect, (GetCapture() == hWnd) ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT );

	EndPaint( *this, &PS );
	unguard;
}

INT WSplitter::OnSetCursor()
{
	guard(WSplitter::OnSetCursor);
	SetCursor(LoadCursorIdX(hInstanceWindow, bVertical ? IDC_SplitWE : IDC_SplitNS));
	return 1;
	unguard;
}

void WSplitter::OnLeftButtonDown()
{
	guard(WSplitter::OnLeftButtonDown);

	FPoint pt = GetCursorPos();
	SetCapture( hWnd );
	InvalidateRect( hWnd, NULL, 1 );

	unguard;
}

void WSplitter::OnMouseMove( DWORD Flags, FPoint MouseLocation )
{
	guard(WSplitter::OnMouseMove);

	if( GetCapture() == hWnd )
	{
		// Figure out where the mouse is relative to the parent windows client area

		FRect rc = OwnerWindow->GetClientRect();
		FPoint MouseLocation;
		::GetCursorPos( MouseLocation );
		MouseLocation = OwnerWindow->ScreenToClient( MouseLocation );

		if( bVertical )
			Pct = ( MouseLocation.X / (FLOAT)(rc.Max.X-STANDARD_SPLITTER_SZ) ) * 100.f;
		else
			Pct = ( MouseLocation.Y / (FLOAT)(rc.Max.Y-STANDARD_SPLITTER_SZ) ) * 100.f;

		Pct = Max<FLOAT>( Pct, 0.f );
		Pct = Min<FLOAT>( Pct, 100.f );

		PositionSplitter();
	}

	unguard;
}

void WSplitter::OnLeftButtonUp()
{
	guard(WSplitter::OnLeftButtonUp);
	ReleaseCapture();
	InvalidateRect( hWnd, NULL, 0 );
	unguard;
}

void WSplitter::PositionSplitter()
{
	guard(WSplitter::PositionSplitter);

	RECT rc;
	::GetClientRect( OwnerWindow->hWnd, &rc );

	if( bVertical )
		::MoveWindow( hWnd, (rc.right-STANDARD_SPLITTER_SZ)*(Pct/100.f), 0, STANDARD_SPLITTER_SZ, rc.bottom, 1 );
	else
		::MoveWindow( hWnd, 0, (rc.bottom-STANDARD_SPLITTER_SZ)*(Pct/100.f), rc.right, STANDARD_SPLITTER_SZ, 1 );

	((WSplitterContainer*)OwnerWindow)->PositionPanes();

	SendMessageX( OwnerWindow->OwnerWindow->hWnd, WM_COMMAND, WM_POSITIONCHILDCONTROLS, 0L );

	unguard;
}

/*-----------------------------------------------------------------------------
	WSplitterContainer.
-----------------------------------------------------------------------------*/

void WSplitterContainer::OpenWindow( UBOOL InVertical )
{
	guard(WSplitterContainer::OpenWindow);

	PerformCreateWindowEx
	(
		0,
		NULL,
		WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		0, 0,
		0, 0,
		OwnerWindow ? OwnerWindow->hWnd : NULL,
		NULL,
		hInstance
	);
	SendMessageX( hWnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(0,0) );

	SplitterBar = new WSplitter( this );
	SplitterBar->OpenWindow( InVertical );
	SetPct( 15.f );

	Pane1 = new WSplitterPane( this );
	Pane1->OpenWindow();

	Pane2 = new WSplitterPane( this );
	Pane2->OpenWindow();

	unguard;
}

void WSplitterContainer::SetPct( FLOAT InPct )
{
	SplitterBar->Pct = InPct;
	SplitterBar->PositionSplitter();
}

void WSplitterContainer::OnDestroy()
{
	guard(WSplitterContainer::OnDestroy);
	WWindow::OnDestroy();

	delete SplitterBar;
	delete Pane1;
	delete Pane2;

	unguard;
}

// Figures out the locations/sizes of the 2 panes
void WSplitterContainer::PositionPanes()
{
	if( !Pane1 ) return;

	FRect rc = GetClientRect();

	if( SplitterBar->bVertical )
	{
		FLOAT Pane1Width = ((rc.Max.X-STANDARD_SPLITTER_SZ) * (SplitterBar->Pct/100.f));
		::MoveWindow( Pane1->hWnd,
			0,
			0,
			Pane1Width,
			rc.Max.Y,
			1 );

		FRect rcPane1 = Pane1->GetClientRect();

		::MoveWindow( Pane2->hWnd,
			Pane1Width + STANDARD_SPLITTER_SZ,
			0,
			rc.Max.X - Pane1Width - STANDARD_SPLITTER_SZ,
			rc.Max.Y,
			1 );
	}
	else
	{
		FLOAT Pane1Height = ((rc.Max.Y-STANDARD_SPLITTER_SZ) * (SplitterBar->Pct/100.f));
		::MoveWindow( Pane1->hWnd,
			0,
			0,
			rc.Max.X,
			Pane1Height,
			1 );

		FRect rcPane1 = Pane1->GetClientRect();

		::MoveWindow( Pane2->hWnd,
			0,
			Pane1Height + STANDARD_SPLITTER_SZ,
			rc.Max.X,
			rc.Max.Y - Pane1Height - STANDARD_SPLITTER_SZ,
			1 );

	}

	if( ParentContainer )
		ParentContainer->RefreshControls();
}

void WSplitterContainer::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	guard(WSplitterContainer::OnSize);
	WWindow::OnSize(Flags, NewX, NewY);
	if( SplitterBar )
		SplitterBar->PositionSplitter();
	unguard;
}

void WSplitterContainer::OnPaint()
{
	guard(WSplitterContainer::OnPaint);
	PAINTSTRUCT PS;
	HDC hDC = BeginPaint( *this, &PS );

	FRect Rect = GetClientRect();
	FillRect( hDC, Rect, hBrushBlack );

	EndPaint( *this, &PS );
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

