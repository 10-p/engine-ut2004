/*=============================================================================
	USetupDefinitionWindows.cpp: Unreal Windows installer/filer code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#pragma warning( disable : 4201 )
#define STRICT
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <io.h>
#include <dbt.h>
#include <direct.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include "DSetup.h"
#include "Res\resource.h"
#include "SetupPrivate.h"
#include "Window.h"
#include "USetupDefinitionWindows.h"

//!!duplicated
static void regSet( HKEY Base, FString Dir, FString Key, FString Value )
{
	guard(regSet);
	HKEY hKey = NULL;
	// gam ---
	verifySlow(RegCreateKeyX( Base, *Dir, &hKey )==ERROR_SUCCESS);
	verifySlow(RegSetValueExX( hKey, *Key, 0, REG_SZ, (BYTE*)*Value, (Value.Len()+1)*sizeof(TCHAR) )==ERROR_SUCCESS);
	verifySlow(RegCloseKey( hKey )==ERROR_SUCCESS);
	// --- gam
	unguard;
}
static HKEY regGetRootKey( FString Root )
{
	guard(regGetRootKey);
	HKEY Key
	=	Root==TEXT("HKEY_CLASSES_ROOT")	 ? HKEY_CLASSES_ROOT
	:	Root==TEXT("HKEY_CURRENT_USER")	 ? HKEY_CURRENT_USER
	:	Root==TEXT("HKEY_LOCAL_MACHINE") ? HKEY_LOCAL_MACHINE
	:	NULL;
	if( !Key )
		appErrorf( TEXT("Invalid root registry key %s"), *Root );
	return Key;
	unguard;
}
static UBOOL regGet( HKEY Base, FString Dir, FString Key, FString& Str )
{
	guard(regGetFString);
	HKEY hKey = NULL;
	if( RegOpenKeyX( Base, *Dir, &hKey )==ERROR_SUCCESS )
	{
		TCHAR Buffer[4096]=TEXT("");
		DWORD Type=REG_SZ, BufferSize=sizeof(Buffer);
		if
		(	RegQueryValueExX( hKey, *Key, 0, &Type, (BYTE*)Buffer, &BufferSize )==ERROR_SUCCESS
		&&	Type==REG_SZ )
		{
			Str = Buffer;
			return 1;
		}
	}
	Str = TEXT("");
	return 0;
	unguard;
}

// HRESULT checking.
#define verifyHRESULT(fn) {HRESULT hRes=fn; if( hRes!=S_OK ) appErrorf( TEXT(#fn) TEXT(" failed (%08X)"), hRes );}
#define verifyHRESULTSlow(fn) if(fn){}

/*-----------------------------------------------------------------------------
	USetupShortcut.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(USetupShortcut);

/*-----------------------------------------------------------------------------
	USetupGroupWindows.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(USetupGroupWindows);

/*-----------------------------------------------------------------------------
	USetupDefinitionWindows.
-----------------------------------------------------------------------------*/

USetupDefinitionWindows::USetupDefinitionWindows( WWindow* InOwnerWindow )
: OwnerWindow( InOwnerWindow )
{}
UBOOL USetupDefinitionWindows::GetRegisteredProductFolder( FString Product, FString& Folder )
{
	guard(USetupDefinitionWindows::GetRegisteredProduct);
	return regGet( HKEY_LOCAL_MACHINE, US + TEXT("Software\\Unreal Technology\\Installed Apps\\") + Product, TEXT("Folder"), Folder )!=0;
	unguard;
}
void USetupDefinitionWindows::SetupFormatStrings()
{
	guard(USetupDefinitionWindows::SetupFormatStrings);
	USetupDefinition::SetupFormatStrings();

	// Get system directory.
#if UNICODE
	if( !GUnicodeOS )
	{
		ANSICHAR ASysDir[256]="", AWinDir[256]="";
		GetSystemDirectoryA( ASysDir, ARRAY_COUNT(ASysDir) );
		GetWindowsDirectoryA( AWinDir, ARRAY_COUNT(AWinDir) );
		WinSysPath = appFromAnsi(ASysDir);
		WinPath    = appFromAnsi(AWinDir);
	}
	else
#endif
	{
		TCHAR TSysDir[256]=TEXT(""), TWinDir[256]=TEXT("");
		GetSystemDirectory( TSysDir, ARRAY_COUNT(TSysDir) );
		GetWindowsDirectory( TWinDir, ARRAY_COUNT(TWinDir) );
		WinSysPath = TSysDir;
		WinPath    = TWinDir;
	}
	GConfig->SetString( TEXT("Setup"), TEXT("WinPath"), *WinPath, *ConfigFile );
	GConfig->SetString( TEXT("Setup"), TEXT("WinSysPath"), *WinSysPath, *ConfigFile );

	// Per user folders.
	TCHAR Temp[MAX_PATH]=TEXT("");
	SHGetSpecialFolderPathX( *OwnerWindow, Temp, CSIDL_DESKTOP,  0 );
	DesktopPath = Temp;
	GConfig->SetString( TEXT("Setup"), TEXT("DesktopPath"), *DesktopPath, *ConfigFile );

	SHGetSpecialFolderPathX( *OwnerWindow, Temp, CSIDL_PROGRAMS,  0 );
	ProgramsPath = Temp;
	GConfig->SetString( TEXT("Setup"), TEXT("ProgramsPath"), *ProgramsPath, *ConfigFile );

	SHGetSpecialFolderPathX( *OwnerWindow, Temp, CSIDL_FAVORITES,  0 );
	FavoritesPath = Temp;
	GConfig->SetString( TEXT("Setup"), TEXT("FavoritesPath"), *FavoritesPath, *ConfigFile );

	SHGetSpecialFolderPathX( *OwnerWindow, Temp, CSIDL_STARTUP,  0 );
	StartupPath = Temp;
	GConfig->SetString( TEXT("Setup"), TEXT("StartupPath"), *StartupPath, *ConfigFile );

	// Common folders.
	SHGetSpecialFolderPathX( *OwnerWindow, Temp, CSIDL_COMMON_PROGRAMS,  0 );
	CommonProgramsPath = *Temp ? Temp : *ProgramsPath;
	GConfig->SetString( TEXT("Setup"), TEXT("CommonProgramsPath"), *CommonProgramsPath, *ConfigFile );

	SHGetSpecialFolderPathX( *OwnerWindow, Temp, CSIDL_COMMON_FAVORITES,  0 );
	CommonFavoritesPath = *Temp ? Temp : *FavoritesPath;
	GConfig->SetString( TEXT("Setup"), TEXT("CommonFavoritesPath"), *CommonFavoritesPath, *ConfigFile );

	SHGetSpecialFolderPathX( *OwnerWindow, Temp, CSIDL_COMMON_STARTUP ,  0 );
	CommonStartupPath = *Temp ? Temp : *StartupPath;
	GConfig->SetString( TEXT("Setup"), TEXT("CommonStartupPath"), *CommonStartupPath, *ConfigFile );

	unguard;
}

//!! FIXME: the verifies below are failing for some reason.
#undef verifySlow
#define verifySlow

void USetupDefinitionWindows::ProcessExtra( FString Key, FString Value, UBOOL Selected, INT GroupCDNum, FInstallPoll* Poll )
{
	guard(USetupDefinitionWindows::ProcessExtra);
	if( Selected && Key==TEXT("WinRegistry") )
	{
		// Create registry items.
		INT Pos=Value.InStr(TEXT("="));
		check(Pos>=0);
		FString RegKey   = Value.Left(Pos);
		FString RegValue = Value.Mid(Pos+1);

		// Update uninstallation log.
		UninstallLogAdd( TEXT("WinRegistry"), *RegKey, 0, 1 );

		// Get root key.
		Pos               = RegKey.InStr(TEXT("\\"));
		check(Pos>=0);
		FString Root      = Pos>=0 ? RegKey.Left(Pos)  : RegKey;
		RegKey            = Pos>=0 ? RegKey.Mid(Pos+1) : TEXT("");
		HKEY Key          = regGetRootKey( Root );
		Pos               = RegKey.InStr(TEXT("\\"),1);
		check(Pos>=0);
		regSet( Key, *RegKey.Left(Pos), *RegKey.Mid(Pos+1), *RegValue );
	}
	else if( Selected && Key==TEXT("WinRegistryNoOverwrite") )
	{
		// Create registry items, don't overwrite any existing values.
		INT Pos=Value.InStr(TEXT("="));
		check(Pos>=0);
		FString RegKey   = Value.Left(Pos);
		FString RegValue = Value.Mid(Pos+1);
		FString FullRegKey = RegKey;

		// Get root key.
		Pos               = RegKey.InStr(TEXT("\\"));
		check(Pos>=0);
		FString Root      = Pos>=0 ? RegKey.Left(Pos)  : RegKey;
		RegKey            = Pos>=0 ? RegKey.Mid(Pos+1) : TEXT("");
		HKEY Key          = regGetRootKey( Root );
		Pos               = RegKey.InStr(TEXT("\\"),1);
		check(Pos>=0);

		FString Existing;
		if( !regGet( Key, *RegKey.Left(Pos), *RegKey.Mid(Pos+1), Existing ) )
		{
			regSet( Key, *RegKey.Left(Pos), *RegKey.Mid(Pos+1), *RegValue );
			// Update uninstallation log.
			UninstallLogAdd( TEXT("WinRegistry"), *FullRegKey, 0, 1 );
		}
	}
	else if( Selected && Key==TEXT("Shortcut") )
	{
		USetupShortcut* Shortcut = new(GetOuter(),*Value)USetupShortcut;

		// Get icon.
		INT Pos=Shortcut->Icon.InStr(TEXT(",")), IconIndex=0;
		if( Pos>=0 )
		{
			IconIndex = appAtoi( *(Shortcut->Icon.Mid(Pos+1)) );
			Shortcut->Icon = Shortcut->Icon.Left( Pos );
		}

		// Expand parameters.
		Shortcut->Template         = Format( Shortcut->Template        , *Value );
		Shortcut->Command          = Format( Shortcut->Command         , *Value );
		Shortcut->Parameters       = Format( Shortcut->Parameters      , *Value );
		Shortcut->Icon             = Format( Shortcut->Icon            , *Value );
		Shortcut->WorkingDirectory = Format( Shortcut->WorkingDirectory, *Value );
		GFileManager->MakeDirectory( *BasePath(Shortcut->Template), 1 );

		// Update uninstallation log.
		UninstallLogAdd( TEXT("Shortcut"), *Shortcut->Template, 0, 1 );

		// Make Windows shortcut.
#if UNICODE
		if( !GUnicodeOS )
		{
			IShellLinkA* psl=NULL;
			verify(SUCCEEDED(CoCreateInstance( CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLinkA, (void**)&psl )));
			verifySlow(psl->SetPath( appToAnsi(*Shortcut->Command) )==NOERROR);
			verifySlow(psl->SetWorkingDirectory( appToAnsi(*Shortcut->WorkingDirectory) )==NOERROR);
			verifySlow(psl->SetArguments( appToAnsi(*Shortcut->Parameters) )==NOERROR);
			if( Shortcut->Icon!=TEXT("") )
				verifySlow(psl->SetIconLocation( appToAnsi(*Shortcut->Icon), IconIndex )==NOERROR);
			IPersistFile* ppf=NULL;
			verify(SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf)));
			verifyHRESULTSlow(ppf->Save(*Shortcut->Template,TRUE));
			ppf->Release();
			psl->Release();
		}
		else
#endif
		{
			IShellLink* psl=NULL;
			verify(SUCCEEDED(CoCreateInstance( CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&psl )));
			verifySlow(psl->SetPath( *Shortcut->Command )==NOERROR);
			verifySlow(psl->SetWorkingDirectory( *Shortcut->WorkingDirectory )==NOERROR);
			verifySlow(psl->SetArguments( *Shortcut->Parameters )==NOERROR);
			if( Shortcut->Icon!=TEXT("") )
				verifySlow(psl->SetIconLocation( *Shortcut->Icon, IconIndex )==NOERROR);
			IPersistFile* ppf=NULL;
			verify(SUCCEEDED(psl->QueryInterface(IID_IPersistFile, (void**)&ppf)));
			verifyHRESULTSlow(ppf->Save(appToUnicode(*Shortcut->Template),TRUE));
			ppf->Release();
			psl->Release();
		}
	}
	else Super::ProcessExtra( Key, Value, Selected, GroupCDNum, Poll );
	unguard;
}
USetupDefinitionWindows* GSetup=NULL;
FInstallPoll* GPoll=NULL;
DWORD WINAPI DirectXSetupCallbackFunction( DWORD Reason, DWORD MsgType, char* szMessage, char* szName, void* pInfo )
{
	guard(DirectXSetupCallbackFunction);
	if( MsgType!=0 )
		return MessageBoxA(*GSetup->OwnerWindow,szMessage,"DirectX Update Caption",MsgType);
	if( szMessage )
		GPoll->Poll(appFromAnsi(szMessage),0,1,0,0);
	return 0;
	unguard;
}
void USetupDefinitionWindows::ProcessPostCopy( FString Key, FString Value, UBOOL Selected, INT GroupCDNum, FInstallPoll* Poll )
{
	guard(USetupDefinitionWindows::ProcessPostCopy);
	USetupDefinition::ProcessPostCopy( Key, Value, Selected, GroupCDNum, Poll );
	if( Selected && Key==TEXT("DirectXHook") )
	{
		if( LocateSourceFile( Value, GroupCDNum, 0, 0 ) )
		{
			// Load DirectX library.
			HMODULE hMod    = LoadLibraryX( *Value );
			UBOOL   Success = 0;
			if( hMod )
			{
				UBOOL requireDx = 1;
				INT(WINAPI*DirectXSetupSetCallbackA)(DSETUP_CALLBACK) = (INT(WINAPI*)(DSETUP_CALLBACK))GetProcAddress( hMod, "DirectXSetupSetCallback" );
				INT(WINAPI*DirectXSetupA)( HWND hWnd, LPSTR lpszRootPath, DWORD dwFlags ) = (INT(WINAPI*)(HWND,LPSTR,DWORD))GetProcAddress( hMod, "DirectXSetupA" );           
#if 0
				//!! This doesn't appear to be working.  DirectXSetupGetVersionA isn't in the exports for the DXSetup.dll we're using. (jack)
				// sjs --- calc version
				INT(WINAPI*DirectXSetupGetVersionA)( DWORD* pVer, DWORD* pRev ) = (INT(WINAPI*)(DWORD*,DWORD*))GetProcAddress( hMod, "DirectXSetupGetVersionA" ); // sjs

				if( DirectXSetupGetVersionA ) 
				{
					DWORD dwVersion = 0;
					DWORD dwRevision = 0;
					if( DirectXSetupGetVersionA(&dwVersion,&dwRevision) )
					{
						// dxdocs "Version and revision numbers can be concatenated into a 64-bit quantity for comparison."
						// "The version number is in the 32 most significant bits and the revision number is in the 32 least significant bits."                    
						DWORD dwRequiredVersion = 0x00040008;
						DWORD dwRequiredRevision = 0x0; // TBA!
						SQWORD requiredRev = (dwRequiredVersion << 32) | dwRequiredRevision;
						SQWORD haveRev = (dwVersion << 32) | dwRevision;
						if ( requiredRev <= haveRev )
						{
							requireDx = 0;
							debugf(TEXT("Required DirectX version already installed!"));
						}                    
/*						appMsgf(0, TEXT("DirectX version is %d.%d.%d.%d\n"),
							HIWORD(dwVersion), LOWORD(dwVersion),
							HIWORD(dwRevision), LOWORD(dwRevision));
*/					}
				}
				// --- sjs
#endif

				if( requireDx && DirectXSetupA && DirectXSetupSetCallbackA )
				{
					GSetup = this;
					GPoll = Poll;
					DirectXSetupSetCallbackA( DirectXSetupCallbackFunction );
					INT Result = DirectXSetupA( *OwnerWindow, const_cast<ANSICHAR*>(TCHAR_TO_ANSI(*BasePath(Value))), DSETUP_DIRECTX );
					if( Result==DSETUPERR_SUCCESS_RESTART )
						MustReboot = 1;
					if( Result>=0 )
						Success = 1;
				}
			}
		}
	}
	//!! Windows XP Pre-sp1 DX8.1 QFE (Q321178)
	if( Selected && Key==TEXT("DirectXQFE") )
	{
		DWORD dwPlatformId, dwMajorVersion, dwMinorVersion;
		FString SPVersion;
#if UNICODE
		if( GUnicode && !GUnicodeOS )
		{
			OSVERSIONINFOA Version;
			Version.dwOSVersionInfoSize = sizeof(OSVERSIONINFOA);
			GetVersionExA(&Version);
			dwPlatformId   = Version.dwPlatformId;
			dwMajorVersion = Version.dwMajorVersion;
			dwMinorVersion = Version.dwMinorVersion;
			SPVersion      = appFromAnsi(Version.szCSDVersion);
		}
		else
#endif
		{
			OSVERSIONINFO Version;
			Version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
			GetVersionEx(&Version);
			dwPlatformId   = Version.dwPlatformId;
			dwMajorVersion = Version.dwMajorVersion;
			dwMinorVersion = Version.dwMinorVersion;
			SPVersion      = Version.szCSDVersion;
		}

		if( dwPlatformId==VER_PLATFORM_WIN32_NT && dwMajorVersion==5 && dwMinorVersion==1 && SPVersion==TEXT("") )
		{
			INT i = Value.InStr(TEXT(","));
			if( i >= 0 )
			{
				FString QfeExe = Value.Left(i);
				FString RegKey = Value.Mid(i+1);
				i = RegKey.InStr( TEXT("\\") );
				if( i >= 0 )
				{
					FString RegRoot = RegKey.Left(i);
					RegKey = RegKey.Mid(i+1);
					HKEY RootKey = regGetRootKey( RegRoot );
					HKEY Key = NULL;
					if( RegOpenKeyX( RootKey, *RegKey, &Key )!=ERROR_SUCCESS )
					{
						LCID Locale = MAKELCID( GetSystemDefaultLangID(), SORT_DEFAULT );
						TCHAR LangAbrev[10];		
						if( 
							TCHAR_CALL_OS( GetLocaleInfoW( Locale, LOCALE_SABBREVLANGNAME, LangAbrev, 10 ),
										GetLocaleInfoA( Locale, LOCALE_SABBREVLANGNAME, (LPSTR)LangAbrev, 10 ) )
						)
						{
							i = QfeExe.InStr(TEXT("%QFELang%"));
							while( i != -1 )
							{
								QfeExe = QfeExe.Left(i) + LangAbrev + QfeExe.Mid( i+appStrlen(TEXT("%QFELang%")) );
								i = QfeExe.InStr(TEXT("%QFELang%"));
							}

							if( LocateSourceFile( QfeExe, GroupCDNum, 0, 1 ) )
							{
								appMsgf( 0, LocalizeGeneral(TEXT("InstallQFE"),TEXT("Setup")));
                                
								TCHAR_CALL_OS(
									ShellExecuteW(*OwnerWindow,TEXT("open"),*QfeExe,TEXT(""),TEXT(""),SW_SHOWNORMAL),
									ShellExecuteA(*OwnerWindow,"open",TCHAR_TO_ANSI(*QfeExe),"","",SW_SHOWNORMAL)
								);                            
							}
						}
					}
				}
			}
		}
	}
	if( Selected && Key==TEXT("OpenALInstaller") )
	{
		if( LocateSourceFile( Value, GroupCDNum, 0, 0 ) )
		{
			TCHAR_CALL_OS(
				ShellExecuteW(*OwnerWindow,TEXT("open"),*Value,TEXT(""),TEXT(""),SW_SHOWNORMAL),
				ShellExecuteA(*OwnerWindow,"open",TCHAR_TO_ANSI(*Value),"","",SW_SHOWNORMAL)
			);
		}
	}
	if( Selected && Key==TEXT("SpeechRedistInstaller") )
	{
		if( LocateSourceFile( Value, GroupCDNum, 0, 0 ) )
		{
			TCHAR_CALL_OS(
				ShellExecuteW(*OwnerWindow,TEXT("open"),*Value,TEXT(""),TEXT(""),SW_SHOWNORMAL),
				ShellExecuteA(*OwnerWindow,"open",TCHAR_TO_ANSI(*Value),"","",SW_SHOWNORMAL)
			);
		}
	}
	if (Selected && Key==TEXT("ECEModInstaller" ) )
	{

		FString FilePathA = FString::Printf(TEXT("%s\\%s"),*SrcPath.Left(2),*Value);
		FString FilePathB = FString::Printf(TEXT("..\\..\\Disk1\\%s"),*Value);

		while ( (GFileManager->FileSize(*FilePathA)<=0) && (GFileManager->FileSize(*FilePathB) <=0) )
		{
			FString Title = Localize( TEXT("General"), TEXT("ECEPromptTitle"), TEXT("Setup") );
			FString Message;
			Message = FString::Printf( Localize( TEXT("General"), TEXT("ECEPromptMessage"), TEXT("Setup") ) );

			if ( !WCDPromptDialog( OwnerWindow, this, *Title, *Message).DoModal() )
				return;
		}

		if (GFileManager->FileSize(*FilePathA)>0)
			FilePathB = FilePathA;

		TCHAR_CALL_OS(
			ShellExecuteW(*OwnerWindow,TEXT("open"),*FilePathB,TEXT(""),TEXT(""),SW_SHOWNORMAL),
			ShellExecuteA(*OwnerWindow,"open",TCHAR_TO_ANSI(*FilePathB),"","",SW_SHOWNORMAL)
		);


	}


	unguard;
}
void USetupDefinitionWindows::ProcessUninstallRemove( FString Key, FString Value, INT GroupCDNum, FInstallPoll* Poll )
{
	guard(USetupDefinitionWindows::ProcessUninstallRemove);
	Poll->Poll(*Value,0,1,UninstallCount++,UninstallTotal);
	if( Key==TEXT("Shortcut") && UpdateRefCount(*Key,*Value,-1)==0 )
	{
		GFileManager->Delete( appFromAnsi(appToAnsi(*Value)),1 );
		RemoveEmptyDirectory( *BasePath(Value) );
	}
	else if( Key==TEXT("WinRegistry") && UpdateRefCount(*Key,*Value,-1)==0 )
	{
		// Get root key.
		FString RegKey    = Value;
		INT Pos           = RegKey.InStr(TEXT("\\"));
		check(Pos>=0);
		FString Root      = Pos>=0 ? RegKey.Left(Pos)  : RegKey;
		RegKey            = Pos>=0 ? RegKey.Mid(Pos+1) : TEXT("");
		HKEY Key          = regGetRootKey( Root );
		Pos               = RegKey.InStr(TEXT("\\"),1);
		check(Pos>=0);
		FString Path      = RegKey.Left(Pos);
		FString Name      = RegKey.Mid(Pos+1);
		HKEY ThisKey=NULL;
		RegOpenKeyExX( Key, *Path, 0, KEY_ALL_ACCESS, &ThisKey );
		if( ThisKey )
		{
			RegDeleteValueX( ThisKey, *Name );
			RegCloseKey( ThisKey );
		}
		while( Path!=TEXT("") )
		{
			RegOpenKeyExX( Key, *Path, 0, KEY_ALL_ACCESS, &ThisKey );
			if( !ThisKey )
				break;
			DWORD SubKeyCount=0;
			RegQueryInfoKeyX( ThisKey, &SubKeyCount );
			RegCloseKey( ThisKey );
			if( SubKeyCount )
				break;
			FString SubKeyName;
			while( Path.Len() && Path.Right(1)!=PATH_SEPARATOR )
			{
				SubKeyName = Path.Right(1) + SubKeyName;
				Path = Path.LeftChop( 1 );
			}
			if( Path.Right(1)==PATH_SEPARATOR )
				Path = Path.LeftChop( 1 );
			HKEY SuperKey = Key;
			if( Path!=TEXT("") )
				RegOpenKeyExX( Key, *Path, 0, KEY_ALL_ACCESS, &SuperKey );
			if( !SuperKey )
				break;
			RegDeleteKeyX( SuperKey, *SubKeyName );
			RegCloseKey( SuperKey );
		}
	}
	else Super::ProcessUninstallRemove( Key, Value, GroupCDNum, Poll );
	unguard;
}
void USetupDefinitionWindows::PreExit()
{
	guard(USetupDefinitionWindows::PreExit);
	if( MustReboot )
	{
		HANDLE hToken=NULL;
		TOKEN_PRIVILEGES tkp;
		OpenProcessToken( GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken );
		LookupPrivilegeValueX( NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid );
		tkp.PrivilegeCount = 1;
		tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;  
		AdjustTokenPrivileges( hToken, FALSE, &tkp, 0, NULL, 0 );
		ExitWindowsEx( EWX_REBOOT, 0 );
	}
	unguard;
}
void USetupDefinitionWindows::PerformInstallCopy()
{
	guard(USetupDefinitionWindows::PerformInstallCopy);
	TCHAR TempPath[MAX_PATH]=TEXT("");
	GetTempPathX( ARRAY_COUNT(TempPath), TempPath );
	FString SetupTempDir;
	for( INT i=0;i<16384;i++ )
	{
		SetupTempDir = FString(TempPath) * FString::Printf(TEXT("Setup%d%d"), i, appRand() );
		if( GFileManager->MakeDirectory( *SetupTempDir ) )
			break;
		else
			SetupTempDir = TEXT("");
	}
	if( SetupTempDir == TEXT("") )
		appErrorf(TEXT("Installer: couldn't find temp directory"));

 	TMultiMap<FString,FString>* Map = GConfig->GetSectionPrivate( TEXT("SetupGroup"), 0, 1, SETUP_INI );
	if( !Map )
		appErrorf(TEXT("Installer: couldn't find SetupGroup") );
	for( TMultiMap<FString,FString>::TIterator It(*Map); It; ++It )
	{
		if( It.Key()==TEXT("Backup") || It.Key()==TEXT("BackupInstall") || It.Key()==TEXT("BackupInstallOpt") )
		{
			FString Src = Format(It.Value());
			if( !LocateSourceFile( Src, 0, 0, 0 ) )
			{
				if( It.Key()!=TEXT("BackupInstallOpt") )
					appErrorf(NAME_FriendlyError, TEXT("Installer failed to find file %s (%s)"), *It.Value(), *Src );
			}
			else
			{
				FString Dest = SetupTempDir*BaseFilename(It.Value());
				if( GFileManager->Copy( *Dest, *Src, 1, 1, 0, NULL ) != COPY_OK )
					appErrorf(NAME_FriendlyError, TEXT("Uninstaller failed to copy file %s (%s) to %s"), *It.Value(), *Src, *Dest );
			}
		}
	}
	FString Launch = SetupTempDir*TEXT("Setup.exe");
	if( (INT)ShellExecuteX( NULL, TEXT("open"), *Launch, *(US+TEXT("cdinstall \"") + *SrcPath + TEXT("\"")), *SetupTempDir, SW_SHOWNORMAL )<=32 )
		appErrorf(NAME_FriendlyError, TEXT("Installer failed to launch %s"), *Launch );
	unguard;    
}
void USetupDefinitionWindows::PerformUninstallCopy()
{
	guard(USetupDefinitionWindows::PerformUninstallCopy);
	TCHAR TempPath[MAX_PATH]=TEXT("");
	GetTempPathX( ARRAY_COUNT(TempPath), TempPath );
 	TMultiMap<FString,FString>* Map = GConfig->GetSectionPrivate( *Product, 0, 1, SETUP_INI );
	if( !Map )
		appErrorf(NAME_FriendlyError, TEXT("Uninstaller couldn't find product: %s"), *Product );
	check(Map);
	for( TMultiMap<FString,FString>::TIterator It(*Map); It; ++It )
	{
		if( It.Key()==TEXT("Backup") || It.Key()==TEXT("BackupUninstall") )
		{
			FString Src = Format(It.Value());
			if( !LocateSourceFile( Src, 0, 0, 0 ) )
				appErrorf(NAME_FriendlyError, TEXT("Uninstaller failed to find file %s (%s)"), *It.Value(), *Src );
			FString Dest = FString(TempPath)*BaseFilename(It.Value());
			if( GFileManager->Copy( *Dest, *Src, 1, 1, 0, NULL ) != COPY_OK )
				appErrorf(NAME_FriendlyError, TEXT("Uninstaller failed to copy file %s (%s) to %s"), *It.Value(), *Src, *Dest );
		}
	}
	FString Launch = FString(TempPath)*TEXT("Setup.exe");
	if( (INT)ShellExecuteX( NULL, TEXT("open"), *Launch, *(US+TEXT("reallyuninstall \"") + Product + TEXT("\" Path=\"") + appBaseDir() + TEXT("\"")), TempPath, SW_SHOWNORMAL )<=32 )
		appErrorf(NAME_FriendlyError, TEXT("Uninstaller failed to launch %s"), *Launch );
	unguard;
}
void USetupDefinitionWindows::CreateRootGroup()
{
	guard(USetupDefinitionWindows::CreateRootGroup);
	USetupGroup::Manager = this;
	RootGroup = new(GetOuter(), TEXT("Setup"))USetupGroupWindows;
	unguard;
}

UBOOL USetupDefinitionWindows::PromptCD( INT CDNum, const TCHAR* Filename )
{
	FString Title = Localize( TEXT("General"), TEXT("CDPromptTitle"), TEXT("Setup") );
	
	FString Message;
	if( CDNum == LastCDNum )
		Message = FString::Printf( Localize( TEXT("General"), TEXT("LastCDPromptMessage"), TEXT("Setup") ) );
	else
		Message = FString::Printf( Localize( TEXT("General"), TEXT("CDPromptMessage"), TEXT("Setup") ), CDNum );

    return WCDPromptDialog( OwnerWindow, this, *Title, *Message ).DoModal();
}

IMPLEMENT_CLASS(USetupDefinitionWindows);

/*-----------------------------------------------------------------------------
	WCDPromptDialog.
-----------------------------------------------------------------------------*/


WCDPromptDialog::WCDPromptDialog( WWindow* InOwnerWindow, USetupDefinition* Manager, const TCHAR* InTitle, const TCHAR* InMessage )
: WDialog	  ( TEXT("CDPrompt"), IDDIALOG_CDPrompt, InOwnerWindow )
, CDPromptText( this, IDC_CDPromptMessage )
, OkButton    ( this, IDOK, FDelegate(this,(TDelegate)EndDialogTrue) )
, CancelButton( this, IDCANCEL, FDelegate(this,(TDelegate)EndDialogFalse) )
, Title       ( InTitle )
, Message     ( InMessage )
{}

void WCDPromptDialog::OnInitDialog()
{
	guard(WCDPromptDialog::OnInitDialog);
	WDialog::OnInitDialog();
	SetText( *Title );
	CDPromptText.SetText( LineFormat(*Message) );
	unguard;
}

LONG WCDPromptDialog::WndProc( UINT Message, UINT wParam, LONG lParam )
{
	guard(WCDPromptDialog::WndProc);

	// User inserted CD - close dialog.
	if( Message == WM_DEVICECHANGE && wParam == DBT_DEVICEARRIVAL )
		EndDialogTrue();

	return WDialog::WndProc( Message, wParam, lParam );
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

