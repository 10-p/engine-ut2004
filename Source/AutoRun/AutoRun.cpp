/*=============================================================================
	AutoRun.cpp: Small autorun exectuable for extra CDs
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	This wouldn't be necessary if Windows XP didn't attempt to scan CDs for
	"Volume AutoPlay" content even if they have a valid AutoRun.INF (unless
	it has an Open= line).  It's possible to disable XP AutoPlay from inside
	the installer but it requires about 1000 lines of COM.  This was easier
	and solved the problem outside the installer.

Revision history:
	* Created by Jack Porter
=============================================================================*/


//#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers
#include <windows.h>

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPTSTR    lpCmdLine,
                     int       nCmdShow)
{

	// See if Unreal or Filer is running.
	HANDLE hMutex = CreateMutex( NULL, 0, "UnrealIsRunning" );
	if( GetLastError()==ERROR_ALREADY_EXISTS )
		CloseHandle( hMutex );
	else
	{
		CloseHandle( hMutex );
#ifdef AUTORUN
		MessageBox(NULL, "Please press OK and then insert CD number 1 to install UT2004.", "Insert UT2004 CD 1", MB_OK );
#endif

#ifdef EXTRASAUTORUN
#if 0
		// CD 6 is now play disc

		if( MessageBox(NULL, "This CD contains supplemental bonus content.  Would you like to browse the CD?", "Supplemental Content", MB_YESNO ) == IDYES )
		{
			char dir[300];
			if( GetModuleFileName( hInstance, dir, 256 ) )
			{
				size_t i;
				for( i=strlen(dir)-1; i>0; i-- )
					if( dir[i-1]=='\\' || dir[i-1]=='/' )
						break;
				dir[i]=0;
				strcat( dir, "Extras");
				ShellExecute( NULL, "open", dir, "", dir, SW_SHOWNORMAL );
			}
	}
#endif
#endif
	}
	return 0;
}

