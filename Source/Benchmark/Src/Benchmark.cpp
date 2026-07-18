/*=============================================================================
	Benchmark.cpp: Standalone benchmark launcher.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel
=============================================================================*/

//
// Yes, this is a quick hack.
//

#include <windows.h>
#include <stdio.h>
#include "resource.h"

#define DUMMY_FILE "dummy.ben"

static char Resolution[1024] = "800x600";


FLOAT RunTest( char* Arguments )
{
	FLOAT Average = 0.f;
	SHELLEXECUTEINFO Info;
	memset( &Info, 0, sizeof(Info) );
		
	char NewArguments[1024];
	strcpy( NewArguments, Arguments );
	strcat( NewArguments, Resolution );

	Info.cbSize			= sizeof(Info);
	Info.fMask			= SEE_MASK_NOCLOSEPROCESS;
	Info.lpVerb			= "open";
	Info.lpFile			= "ut2004.exe";
	Info.lpParameters	= NewArguments;
	Info.nShow			= SW_SHOWNORMAL;

	if( !ShellExecuteEx( &Info ) )
		throw( "Couldn't launch batch file." );

	if( Info.hProcess )
	{
		WaitForSingleObject( Info.hProcess, INFINITE );
		CloseHandle( Info.hProcess );

		HANDLE FileHandle = CreateFile( 
			DUMMY_FILE, 
			GENERIC_READ, 
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING, 
			FILE_ATTRIBUTE_NORMAL, 
			NULL 
		);
        
		if( FileHandle==INVALID_HANDLE_VALUE )
			throw( "Couldn't read results from temporary file." );

		char Result[1024];
		DWORD BytesRead = 0;

		if( ReadFile( FileHandle, Result, sizeof( Result ), &BytesRead, NULL ) == 0 )
			throw( "Couldn't read results from temporary file." );

		CloseHandle( FileHandle );

		if( BytesRead == 0 )
			throw( "Temporary file is corrupt." );
		
		Average = (FLOAT) atof( Result );
	}
	else
		throw( "Error launching game." );

	return Average;
}


BOOL CALLBACK ResolutionDialogProc(HWND hWndDlg, UINT Message, WPARAM wParam, LPARAM lParam) 
{ 
	switch( Message ) 
	{ 
		case WM_COMMAND: 
			switch (LOWORD(wParam)) 
			{
			case IDC_RADIO1:
				strcpy( Resolution, "640x480" );
				return TRUE;
			case IDC_RADIO2:
				strcpy( Resolution, "800x600" );
				return TRUE;
			case IDC_RADIO3:
				strcpy( Resolution, "1024x768" );
				return TRUE;
			case IDC_RADIO4:
				strcpy( Resolution, "1280x960" );
				return TRUE;
			case IDC_RADIO5:
				strcpy( Resolution, "1600x1200" );
				return TRUE;
			case IDSTART: 
				EndDialog(hWndDlg, wParam); 
				return TRUE; 
			} 
	} 
	return FALSE; 
} 


//
//	WinMain
//
int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE PreviousInstance,LPSTR CommandLine,int ShowCommand)
{
	try
	{
		DialogBox(
			hInstance, 
			MAKEINTRESOURCE(IDD_DIALOG1), 
			NULL, 
			(DLGPROC)ResolutionDialogProc
		); 

		SetFileAttributes( DUMMY_FILE, 0 );
            
		HANDLE FileHandle = CreateFile( 
			DUMMY_FILE, 
			GENERIC_WRITE, 
			FILE_SHARE_READ, 
			NULL, 
			CREATE_ALWAYS, 
			FILE_ATTRIBUTE_NORMAL, 
			NULL
		);
		if( FileHandle==INVALID_HANDLE_VALUE )
			throw( "Couldn't create temporary file." );
		
		CloseHandle( FileHandle );

		FLOAT	AverageFlyby	= 0.f,
				AverageBotmatch	= 0.f;
		
		AverageFlyby += RunTest( "dm-antalus?game=engine.gameinfo -benchmark -seconds=77 -exec=..\\Benchmark\\Stuff\\flybyexec.txt ini=..\\Benchmark\\Stuff\\MaxDetail.ini userini=..\\Benchmark\\Stuff\\MaxDetailUser.ini -nosound -UPT -" );
		AverageFlyby += RunTest( "dm-asbestos?game=engine.gameinfo -benchmark -seconds=70 -exec=..\\Benchmark\\Stuff\\flybyexec.txt ini=..\\Benchmark\\Stuff\\MaxDetail.ini userini=..\\Benchmark\\Stuff\\MaxDetailUser.ini -nosound -UPT -" );

		AverageBotmatch += RunTest( "dm-antalus?spectatoronly=true?numbots=12?quickstart=true?attractcam=true -benchmark -seconds=77 -exec=..\\Benchmark\\Stuff\\botmatchexec.txt ini=..\\Benchmark\\Stuff\\MaxDetail.ini userini=..\\Benchmark\\Stuff\\MaxDetailUser.ini -nosound -UPT -" );
		AverageBotmatch += RunTest( "dm-asbestos?spectatoronly=true?numbots=12?quickstart=true?attractcam=true -benchmark -seconds=77 -exec=..\\Benchmark\\Stuff\\botmatchexec.txt ini=..\\Benchmark\\Stuff\\MaxDetail.ini userini=..\\Benchmark\\Stuff\\MaxDetailUser.ini -nosound -UPT -" );

		if( AverageFlyby == 0.f || AverageBotmatch == 0.f )
			throw( "Invalid score" );

		AverageFlyby /= 2;
		AverageBotmatch /= 2;

		char Result[4096];
		sprintf( Result, "Flyby : %f\r\nBotmatch : %f\r\n\r\nResolution : %s", AverageFlyby, AverageBotmatch, Resolution );

		MessageBox(NULL,Result,"Benchmark Results",MB_OK);
	}
	catch( char* Error )
	{
		MessageBox(NULL,Error,"Benchmark Error",MB_OK);
		return -1;
	}

	return 0;
}
