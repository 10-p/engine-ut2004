/*=============================================================================
	TipOfTheDay : Shows the "Tip of the Day" at editor startup
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

class WDlgTipOfTheDay : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgTipOfTheDay,WDialog,UnrealEd)

	// Variables.
	WCheckBox ShowTipsCheck;
	WButton NextTipButton, CloseButton;
	HBITMAP bitmapTOTD;
	BITMAP bm;
	FString CurrentTip;

	// Constructor.
	WDlgTipOfTheDay( UObject* InContext, WWindow* InOwnerWindow )
	:	WDialog			( TEXT("Tip of the Day"), IDDIALOG_TOTD, InOwnerWindow )
	,	CloseButton		( this, IDOK,			FDelegate(this,(TDelegate)OnClose) )
	,	NextTipButton	( this, IDPB_NEXT_TIP,	FDelegate(this,(TDelegate)OnNextTip) )
	,	ShowTipsCheck	( this, IDCK_SHOW_TIPS )
	{
		bitmapTOTD = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_TIPOFTHEDAY), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	check(bitmapTOTD);
		GetObjectA( bitmapTOTD, sizeof(BITMAP), (LPSTR)&bm );
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgTipOfTheDay::OnInitDialog);
		WDialog::OnInitDialog();

		UBOOL bShowTips;
		if(!GConfig->GetInt( TEXT("Tip Of The Day"), TEXT("ShowTips"), bShowTips, TEXT("UnrealEd.ini") ))	bShowTips = FALSE;
		ShowTipsCheck.SetCheck( bShowTips ? BST_CHECKED : BST_UNCHECKED );

		CurrentTip = GetRandomTip();

		unguard;
	}
	void OnPaint()
	{
		guard(WDlgTipOfTheDay::OnPaint);
		WDialog::OnPaint();

		PAINTSTRUCT PS;
		HDC hDC = BeginPaint( *this, &PS ),
			hdcMem, hdcWnd;
		HBITMAP hBitmap, hbitmapold;

		// Display the bitmap
		hdcWnd = GetDC( hWnd );
		hdcMem = CreateCompatibleDC( hdcWnd );
		hBitmap = CreateCompatibleBitmap( hdcWnd, bm.bmWidth, bm.bmHeight );
		hbitmapold = (HBITMAP)SelectObject( hdcMem, bitmapTOTD );
		
		BitBlt( hDC,
			4, 4,
			bm.bmWidth, bm.bmHeight,
			hdcMem,
			0, 0,
			SRCCOPY
		);

		// Draw the text for the tip
		RECT rc;
		rc.left = 73;
		rc.top = 60;
		rc.right = 395;
		rc.bottom = 197;
		SetBkMode( hDC, TRANSPARENT );
		SelectObject( hDC, GetStockObject(DEFAULT_GUI_FONT) );
		DrawText( hDC, *CurrentTip, CurrentTip.Len(), &rc, DT_LEFT | DT_WORDBREAK );

		SelectObject( hdcMem, hbitmapold );
		DeleteObject( hBitmap );
		ReleaseDC( hWnd, hdcWnd );
		DeleteDC( hdcMem );

		EndPaint( *this, &PS );

		unguard;
	}
	virtual void DoModeless( UBOOL bShow )
	{
		guard(WDlgTipOfTheDay::DoModeless);
		_Windows.AddItem( this );
		hWnd = CreateDialogParamA( hInstance, MAKEINTRESOURCEA(IDDIALOG_TOTD), OwnerWindow?OwnerWindow->hWnd:NULL, (DLGPROC)StaticDlgProc, (LPARAM)this);
		if( !hWnd )
			appGetLastError();
		Show( bShow );
		unguard;
	}
	FString GetRandomTip()
	{
		INT TotalTips;
		if( GConfig->GetInt( TEXT("Tips"), TEXT("TotalTips"), TotalTips, TEXT("UnrealEdTips.ini") ) )
		{
			FString Tip, Key;
			Key = FString::Printf(TEXT("Tip%d"), appRand()%TotalTips );
			GConfig->GetString( TEXT("Tips"), *Key, Tip, TEXT("UnrealEdTips.ini") );
			return Tip;
		}
		else
			return TEXT("UnrealEd couldn't find the tip file?  ('UnrealEdTips.ini')");
	}
	void OnNextTip()
	{
		CurrentTip = GetRandomTip();
		InvalidateRect( hWnd, NULL, 0 );
	}
	bool OnClose() // gam
	{
		UBOOL bShowTips = ShowTipsCheck.IsChecked();
		GConfig->SetInt( TEXT("Tip Of The Day"), TEXT("ShowTips"), bShowTips, TEXT("UnrealEd.ini") );

		EndDialogTrue();
        return true;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

