/*=============================================================================
	FFeedbackContextWindows.h: Unreal Windows user interface interaction.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

/*-----------------------------------------------------------------------------
	FFeedbackContextWindows.
-----------------------------------------------------------------------------*/

//
// Feedback context.
//
class FFeedbackContextWindows : public FFeedbackContext
{
public:
	// Variables.
	INT SlowTaskCount;
	HWND hWndProgressBar, hWndProgressText, hWndProgressDlg, hWndMapCheckDlg;

    // gam ---
    TCHAR LastProgressText[4096];
    // --- gam

	// Constructor.
	FFeedbackContextWindows()
	: SlowTaskCount( 0 )
	, hWndProgressBar( 0 )
	, hWndProgressText( 0 )
	, hWndProgressDlg( 0 )
	, hWndMapCheckDlg( 0 )
	{}
	void Serialize( const TCHAR* V, EName Event )
	{
		guard(FFeedbackContextWindows::Serialize);
		if( Event==NAME_UserPrompt && (GIsClient || GIsEditor) )
			::MessageBox( NULL, V, LocalizeError("Warning",TEXT("Core")), MB_OK|MB_TASKMODAL );
		else
        {
            // gam ---
			debugf( Event, TEXT("%s"), V );

            if( Event==NAME_Error )
                ErrorCount++;
            else if ( Event==NAME_Warning || Event==NAME_ExecWarning || Event==NAME_ScriptWarning )
			    WarningCount++;
            // --- gam
        }
		unguard;
	}
	UBOOL YesNof( const TCHAR* Fmt, ... )
	{
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt, Fmt );

		guard(FFeedbackContextWindows::YesNof);
		if( GIsClient || GIsEditor )
			return( ::MessageBox( NULL, TempStr, LocalizeError("Question",TEXT("Core")), MB_YESNO|MB_TASKMODAL ) == IDYES);
		else
			return 0;
		unguard;
	}
	void MapCheck_Show()
	{
		guard(FFeedbackContextWindows::MapCheck_Show);
		SendMessageX( hWndMapCheckDlg, WM_COMMAND, WM_MC_SHOW, 0L );
		unguard;
	}
	// This is the same as MapCheck_Show, except it won't display the error box if there are no errors in it.
	void MapCheck_ShowConditionally()
	{
		guard(FFeedbackContextWindows::MapCheck_ShowConditionally);
		SendMessageX( hWndMapCheckDlg, WM_COMMAND, WM_MC_SHOW_COND, 0L );
		unguard;
	}
	void MapCheck_Hide()
	{
		guard(FFeedbackContextWindows::MapCheck_Hide);
		SendMessageX( hWndMapCheckDlg, WM_COMMAND, WM_MC_HIDE, 0L );
		unguard;
	}
	void MapCheck_Clear()
	{
		guard(FFeedbackContextWindows::MapCheck_Clear);
		SendMessageX( hWndMapCheckDlg, WM_COMMAND, WM_MC_CLEAR, 0L );
		unguard;
	}
	void MapCheck_Add( INT InType, void* InActor, const TCHAR* InMessage )
	{
		guard(FFeedbackContextWindows::MapCheck_Add);
		MAPCHECK MC;
		MC.Type = InType;
		MC.Actor = (AActor*)InActor;
		MC.Message = InMessage;
		SendMessageX( hWndMapCheckDlg, WM_COMMAND, WM_MC_ADD, (LPARAM)&MC );
		unguard;
	}
	void BeginSlowTask( const TCHAR* Task, UBOOL StatusWindow )
	{
		guard(FFeedbackContextWindows::BeginSlowTask);
		::ShowWindow( hWndProgressDlg, SW_SHOW );
		if( hWndProgressBar && hWndProgressText )
		{
			SendMessageLX( hWndProgressText, WM_SETTEXT, (WPARAM)0, Task );
			SendMessageX( hWndProgressBar, PBM_SETRANGE, (WPARAM)0, MAKELPARAM(0, 100) );

			UpdateWindow( hWndProgressDlg );
			UpdateWindow( hWndProgressText );
			UpdateWindow( hWndProgressBar ); // gam

			{	// flush all messages
				MSG mfm_msg;
				while(::PeekMessage(&mfm_msg, hWndProgressDlg, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&mfm_msg);
					DispatchMessage(&mfm_msg);
				}
			}
		}
		GIsSlowTask = ++SlowTaskCount>0;
		unguard;
	}
	void EndSlowTask()
	{
		guard(FFeedbackContextWindows::EndSlowTask);
		check(SlowTaskCount>0);
		GIsSlowTask = --SlowTaskCount>0;
		if( !GIsSlowTask )
			::ShowWindow( hWndProgressDlg, SW_HIDE );
		unguard;
	}
	UBOOL VARARGS StatusUpdatef( INT Numerator, INT Denominator, const TCHAR* Fmt, ... )
	{
		guard(FFeedbackContextWindows::StatusUpdatef);
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt, Fmt );
		if( GIsSlowTask && hWndProgressBar && hWndProgressText )
		{
            // gam ---
            if( (appStrlen (TempStr) > 0) && appStrcmp( LastProgressText, TempStr) )
            {
			    SendMessageLX( hWndProgressText, WM_SETTEXT, (WPARAM)0, TempStr );
    			UpdateWindow( hWndProgressText );
                appStrcpy( LastProgressText, TempStr);
            }
            // --- gam

			SendMessageX( hWndProgressBar, PBM_SETPOS, (WPARAM)(Denominator ? 100*Numerator/Denominator : 0), (LPARAM)0 );
			UpdateWindow( hWndProgressDlg );
			UpdateWindow( hWndProgressText );
			UpdateWindow( hWndProgressBar );

			{	// flush all messages
				MSG mfm_msg;
				while(::PeekMessage(&mfm_msg, hWndProgressDlg, 0, 0, PM_REMOVE)) {
					TranslateMessage(&mfm_msg);
					DispatchMessage(&mfm_msg);
				}
			}
		}
		return 1;
		unguard;
	}
	void SetContext( FContextSupplier* InSupplier )
	{}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

