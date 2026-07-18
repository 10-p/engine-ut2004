/*=============================================================================
	Controls.cpp: Control implementations
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#pragma warning( disable : 4201 )
#define STRICT
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include "Engine.h"
#include "Window.h"

/*-----------------------------------------------------------------------------
	WCrashBoxDialog.
-----------------------------------------------------------------------------*/

void WCrashBoxDialog::OnInitDialog()
{
	guard(WCrashBoxDialog::OnInitDialog);
	WDialog::OnInitDialog();
	HDC hDC = GetDC(hWnd);
	ReleaseDC(hWnd,hDC);
	TextEdit.SetFont( hFontText );
	SetText( *Caption );
	
	// gam ---
	FString NewMessage;

	NewMessage = GBuildLabel;
	NewMessage += TEXT("\r\n\r\n");

	NewMessage += TEXT("OS: ");
	NewMessage += GMachineOS;
	NewMessage += TEXT("\r\n");

	NewMessage += TEXT("CPU: ");
	NewMessage += GMachineCPU;
	NewMessage += TEXT("\r\n");

	NewMessage += TEXT("Video: ");
	NewMessage += GMachineVideo;
	NewMessage += TEXT("\r\n");
	NewMessage += TEXT("\r\n");

	NewMessage += Message;
	NewMessage += TEXT("\r\n");
	NewMessage += TEXT("\r\n");

#if 0
	FString	BugReportString;

	if(appLoadFileToString(BugReportString,TEXT("BugReport.txt")))
		NewMessage += BugReportString;
#endif

	Message = NewMessage;
    // --- gam
    
	TextEdit.SetText( *Message );
	SetFocus( OkButton );
	MessageBeep(MB_ICONHAND);
	unguard;
}

void WCrashBoxDialog::OnCopy()
{
	appClipboardCopy( *Message );
}

static FString FormatError( const TCHAR* Message )
{
	FString Result;
	for( INT i=0;Message[i];i++ )
	{
		if( (Message[i]>='a'&&Message[i]<='z') || (Message[i]>='A'&&Message[i]<='Z') || (Message[i]>='0'&&Message[i]<='9') )
			Result = Result + FString::Printf(TEXT("%c"),Message[i]);
		else
			Result = Result + FString::Printf(TEXT("%%%02x"),Message[i]);
	}
	return Result;
}

void WCrashBoxDialog::OnBugReport()
{
	FString BugReportURL = 
		FString::Printf( TEXT("%s?version=%d&error=%s"), 
				Localize( TEXT("IDDIALOG_CrashBox"),TEXT("BugReportURL"),TEXT("Window") ),
				ENGINE_VERSION,
				*FormatError(*Message) );

	appLaunchURL( *BugReportURL, TEXT("") );
	EndDialogTrue();
}

/*-----------------------------------------------------------------------------
	WWizardPage.
-----------------------------------------------------------------------------*/

void WWizardPage::OnCurrent()
{}

WWizardPage* WWizardPage::GetNext()
{
	return NULL;
}

const TCHAR* WWizardPage::GetBackText()
{
	return LocalizeGeneral("BackButton",TEXT("Window"));
}

const TCHAR* WWizardPage::GetNextText()
{
	return LocalizeGeneral("NextButton",TEXT("Window"));
}

const TCHAR* WWizardPage::GetFinishText()
{
	return NULL;
}

const TCHAR* WWizardPage::GetCancelText()
{
	return LocalizeGeneral("CancelButton",TEXT("Window"));
}

UBOOL WWizardPage::GetShow()
{
	return 1;
}

void WWizardPage::OnCancel()
{
	guard(WWizardPage::OnCancel);
	((WDialog*)Owner)->EndDialog( 0 );
	unguard;
}

/*-----------------------------------------------------------------------------
	WWizardDialog.
-----------------------------------------------------------------------------*/

void WWizardDialog::OnInitDialog()
{
	guard(WWizardDialog::OnInitDialog);
	WDialog::OnInitDialog();
	SendMessageX( *this, WM_SETICON, ICON_BIG, (WPARAM)LoadIconIdX(hInstance,(GIsEditor?IDICON_Editor:IDICON_Mainframe)) );
	RefreshPage();
	unguard;
}

void WWizardDialog::Advance( WWizardPage* NewPage )
{
	guard(WWizardDialog::Advanced);
	check(NewPage);
	Pages.AddItem( NewPage );
	if( hWnd )
		RefreshPage();
	if( !Pages.Last()->GetShow() )
		OnNext();
	unguard;
}

void WWizardDialog::RefreshPage()
{
	guard(WWizardDialog::RefreshPage);
	if( Pages.Num() )
	{
		WWizardPage* Page = Pages.Last();
		if( !Page->hWnd )
			Page->OpenChildWindow( IDC_PageHolder, 1 );
		BackButton  .SetVisibleText( Pages.Num()>1 ? Page->GetBackText() : NULL );
		NextButton  .SetVisibleText( Page->GetNextText  () );
		FinishButton.SetVisibleText( Page->GetFinishText() );
		CancelButton.SetVisibleText( Page->GetCancelText() );
		if( Pages.Num()>1 )
			Pages(Pages.Num()-2)->Show(0);
		Pages.Last()->OnCurrent();
	}
	unguard;
}

void WWizardDialog::OnDestroy()
{
	guard(WWizardDialog::OnDestroy);
	for( INT i=0; i<Pages.Num(); i++ )
		delete Pages(i);
	WDialog::OnDestroy();
	unguard;
}

void WWizardDialog::OnBack()
{
	guard(WWizardDialog::OnBack);
	if( Pages.Num()>1 )
	{
		Pages(Pages.Num()-2)->Show(1);
		delete Pages.Pop();
		RefreshPage();
		if( !Pages.Last()->GetShow() )
			OnBack();
	}
	unguard;
}

void WWizardDialog::OnNext()
{
	guard(WWizardDialog::OnNext);
	if( Pages.Num() && Pages.Last()->GetNextText() )
	{
		WWizardPage* GotNext = Pages.Last()->GetNext();
		if( GotNext )
			Advance( GotNext );
	}
	unguard;
}

void WWizardDialog::OnFinish()
{
	guard(WWizardDialog::OnFinish);
	EndDialog( 1 );
	unguard;
}

void WWizardDialog::OnCancel()
{
	guard(WWizardDialog::OnCancel);
	Pages.Last()->OnCancel();
	unguard;
}

bool WWizardDialog::OnClose() // gam
{
	guard(WLog::OnClose);
	Pages.Last()->OnCancel();
	throw TEXT("NoRoute");
    return true; // gam
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

