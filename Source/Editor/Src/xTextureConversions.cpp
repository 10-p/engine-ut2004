//=============================================================================
// xTextureConversions - any dxt, detail, other texture mangling on import
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#if !WIN32
#include <time.h>  // work around macro clash.  --ryan.
#endif

#include "EditorPrivate.h"

#if WIN32
#include <process.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

// !! Changes 'FileName' to a converted ref a .dds if conversion is successful
UBOOL UEditorEngine::DDSConversion( TCHAR* FileName, const TCHAR* Str, FOutputDevice& Ar )
{
#if !WIN32
    debugf(TEXT("Error: This is unsupported on non-Win32 systems at the moment."));
#else
    FString Util = TEXT("xDxTex.exe"); // must be in ./System
    
	int ret = -1;
	FString UtilCmdLine = TEXT("");
	FString ddsName = TEXT("");
	FString srcName = TEXT("");
	srcName = FileName;

    INT dxt = 1; // default to dxt1		
	Parse( Str, TEXT("DXT="), dxt );

    if ( dxt==-1 ) // don't compress terrain alpha layers and heightfields
        return 1;

#if 0
	FString execPath;
    FString Dir = FileName;
    while( Dir.Len() && Dir.Right(1)!=PATH_SEPARATOR )
        Dir = Dir.LeftChop(1);
    execPath = *Dir;
    if ( execPath.Len() && appStrstr( FileName, TEXT(":") ) == NULL )
	{
		srcName = execPath;
		srcName += FileName;
	}
#endif
		

	ddsName = srcName;
	ddsName = ddsName.LeftChop(3);
	ddsName += TEXT("dds");

	UBOOL doDDS = 1;

	// figure out if we should bother generating the dds
	if ( GFileManager->FileSize( *ddsName ) > 0 )
	{
		struct _stat	srcStat, ddsStat;
		int				srcTime = 0, ddsTime = 0;
		int				result;
		result = _stat( appToAnsi( *srcName ), &srcStat );
		if( result == 0 )
		{
			srcTime = srcStat.st_mtime;
		}
		result = _stat( appToAnsi( *ddsName ), &ddsStat );
		if( result == 0 )
		{
			ddsTime = ddsStat.st_mtime;
		}
		if ( srcTime < ddsTime )
		{
			doDDS = 0;
            ret = 0; // gam
		}
	}

	if ( doDDS )
	{
		FString defaultDir = GFileManager->GetDefaultDirectory();
		if (appStrncmp(*defaultDir, appBaseDir(), defaultDir.Len()-1)==0)
			UtilCmdLine = FString::Printf(TEXT("\"%s\" "), *srcName );
		else
			UtilCmdLine = FString::Printf(TEXT("\"%s\\%s\" "), *defaultDir, *srcName );
        UBOOL DoMips;
        ParseUBOOL( Str, TEXT("Mips="), DoMips );
        if ( DoMips )
			UtilCmdLine += TEXT("-m ");

		switch (dxt)
		{
			case 1:
				UtilCmdLine += TEXT("DXT1 ");
				break;
			case 3:
				UtilCmdLine += TEXT("DXT3 ");
				break;
			case 5:
				UtilCmdLine += TEXT("DXT5 ");
				break;
			default:
				debugf(TEXT("Error: Invalid DXT import specification."));
		}

		if (appStrncmp(*defaultDir, appBaseDir(), defaultDir.Len()-1)==0)
			UtilCmdLine += FString::Printf(TEXT("\"%s\""), *ddsName );
		else
			UtilCmdLine += FString::Printf(TEXT("\"%s\\%s\""), *defaultDir, *ddsName );

		debugf(TEXT("Shelling %s %s"), *Util, *UtilCmdLine );
		GFileManager->SetDefaultDirectory( appBaseDir() );
		ret = _spawnl( _P_WAIT, appToAnsi( *Util ), appToAnsi( *Util ), appToAnsi( *UtilCmdLine ), NULL, NULL );
		if ( ret == -1 )
			Ar.Logf(TEXT("*** Error spawning %s errno: %d"), *Util, errno );
		GFileManager->SetDefaultDirectory( *defaultDir );
	}

    if ( ret != -1 )
	    appStrcpy( FileName, *ddsName );

#endif  // WIN32

    return 1;
}

