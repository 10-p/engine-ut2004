/*=============================================================================
	UnEngineWin.h: Unreal engine windows-specific code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.	
        * Secret Level's changes, integrated 07/03/02 - Erik de Neve
	  - Broken up Main() into ctor/ looping body /dtor - to enable (Maya-)
	    plugin's external activation of the editor+engine as a child process;
            code from Secret Level's Michael Arnold.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Splash screen.
-----------------------------------------------------------------------------*/

//
// Splash screen, implemented with old-style Windows code so that it
// can be opened super-fast before initialization.
//
INT_PTR CALLBACK SplashDialogProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return 0;
}
static HWND hWndSplash = NULL;
void InitSplash( const TCHAR* Filename )
{
	guard(InitSplash);
	
    HBITMAP hBitmap    = NULL;
    INT     BitmapX    = 0;
    INT     BitmapY    = 0;

	FWindowsBitmap Bitmap(1);

	if( Filename )
    {
		if ( !Bitmap.LoadFile(Filename) )
			return;
		hBitmap = Bitmap.GetBitmapHandle();
		BitmapX = Bitmap.SizeX;
		BitmapY = Bitmap.SizeY;
	}

	hWndSplash = TCHAR_CALL_OS(CreateDialogW(hInstance,MAKEINTRESOURCEW(IDDIALOG_Splash), NULL, SplashDialogProc),CreateDialogA(hInstance, MAKEINTRESOURCEA(IDDIALOG_Splash), NULL, SplashDialogProc) );
	if( hWndSplash )
	{
		HWND hWndLogo = GetDlgItem(hWndSplash,IDC_Logo);
		if( hWndLogo )
		{
			SetWindowPos(hWndSplash,HWND_TOPMOST,(GetSystemMetrics(SM_CXSCREEN)-BitmapX)/2,(GetSystemMetrics(SM_CYSCREEN)-BitmapY)/2,BitmapX,BitmapY,SWP_SHOWWINDOW);
			SetWindowPos(hWndSplash,HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
			SendMessageX( hWndLogo, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmap );
			UpdateWindow( hWndSplash );
		}
	}
	unguard;
}
void ExitSplash()
{
	guard(ExitSplash);
	if (hWndSplash)
		DestroyWindow( hWndSplash );
	hWndSplash = NULL;
	unguard;
}

/*-----------------------------------------------------------------------------
	System Directories.
-----------------------------------------------------------------------------*/

TCHAR SysDir[256]=TEXT(""), WinDir[256]=TEXT(""), ThisFile[256]=TEXT("");
void InitSysDirs()
{
#if UNICODE
	if( !GUnicodeOS )
	{
		ANSICHAR ASysDir[256]="", AWinDir[256]="", AThisFile[256]="";
		GetSystemDirectoryA( ASysDir, ARRAY_COUNT(ASysDir) );
		GetWindowsDirectoryA( AWinDir, ARRAY_COUNT(AWinDir) );
		GetModuleFileNameA( NULL, AThisFile, ARRAY_COUNT(AThisFile) );
        // sjs --- changed these from ANSI_TO_TCHAR(x), was causing stack corruption on win98
		appStrcpy( SysDir, appFromAnsi(ASysDir) );
		appStrcpy( WinDir, appFromAnsi(AWinDir) );
		appStrcpy( ThisFile, appFromAnsi(AThisFile) );
        // --- sjs
	}
	else
#endif
	{
		GetSystemDirectory( SysDir, ARRAY_COUNT(SysDir) );
		GetWindowsDirectory( WinDir, ARRAY_COUNT(WinDir) );
		GetModuleFileName( NULL, ThisFile, ARRAY_COUNT(ThisFile) );
	}
	if( !appStricmp( &ThisFile[appStrlen(ThisFile) - 4], TEXT(".ICD") ) )
		appStrcpy( &ThisFile[appStrlen(ThisFile) - 4], TEXT(".EXE") );
}

/*-----------------------------------------------------------------------------
	Config wizard.
-----------------------------------------------------------------------------*/

class WConfigWizard : public WWizardDialog
{
	DECLARE_WINDOWCLASS(WConfigWizard,WWizardDialog,Startup)
	WLabel LogoStatic;
	FWindowsBitmap LogoBitmap;
	UBOOL Cancel;
	FString Title;
	WConfigWizard()
	: LogoStatic(this,IDC_Logo)
	, Cancel(0)
	{
		InitSysDirs();
	}
	void OnInitDialog()
	{
		guard(WStartupWizard::OnInitDialog);
		WWizardDialog::OnInitDialog();
		SendMessageX( *this, WM_SETICON, ICON_BIG, (WPARAM)LoadIconIdX(hInstance,IDICON_Mainframe) );
		LogoBitmap.LoadFile( TEXT("..\\Help\\InstallerLogo.bmp") );
		SendMessageX( LogoStatic, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)LogoBitmap.GetBitmapHandle() );
		SetText( *Title );
		SetForegroundWindow( hWnd );
		unguard;
	}
};

class WConfigPageSafeOptions : public WWizardPage
{
	DECLARE_WINDOWCLASS(WConfigPageSafeOptions,WWizardPage,Startup)
	WConfigWizard* Owner;
	WButton NoSoundButton, ResButton, ResetConfigButton, NoProcessorButton, NoJoyButton, SoftwareButton;
	WConfigPageSafeOptions( WConfigWizard* InOwner )
	: WWizardPage		( TEXT("ConfigPageSafeOptions"), IDDIALOG_ConfigPageSafeOptions, InOwner )
	, Owner				(InOwner)
	, NoSoundButton		(this,IDC_NoSound)
	, ResButton			(this,IDC_Res)
	, ResetConfigButton	(this,IDC_ResetConfig)
	, NoProcessorButton	(this,IDC_NoProcessor)
	, NoJoyButton		(this,IDC_NoJoy)
	, SoftwareButton	(this,IDC_SoftwareRenderer)
	{}
	void OnInitDialog()
	{
		WWizardPage::OnInitDialog();
		SendMessageX( NoSoundButton,     BM_SETCHECK, 1, 0 );
		SendMessageX( ResButton,         BM_SETCHECK, 1, 0 );
		SendMessageX( ResetConfigButton, BM_SETCHECK, 0, 0 );
		SendMessageX( NoProcessorButton, BM_SETCHECK, 1, 0 );
		SendMessageX( NoJoyButton,       BM_SETCHECK, 1, 0 );
		SendMessageX( SoftwareButton,    BM_SETCHECK, 1, 0 );
	}
	const TCHAR* GetNextText()
	{
		return LocalizeGeneral(TEXT("Run"),TEXT("Startup"));
	}
	WWizardPage* GetNext()
	{
		FString CmdLine;
		if( SendMessageX(NoSoundButton,BM_GETCHECK,0,0)==BST_CHECKED )
			CmdLine+=TEXT(" -nosound");
		if( SendMessageX(ResButton,BM_GETCHECK,0,0)==BST_CHECKED )
			CmdLine+=TEXT(" -defaultres");
		if( SendMessageX(ResetConfigButton,BM_GETCHECK,0,0)==BST_CHECKED )
			GFileManager->Delete( *(FString(appPackage())+TEXT(".ini")) );
		if( SendMessageX(NoProcessorButton,BM_GETCHECK,0,0)==BST_CHECKED )
			CmdLine+=TEXT(" -nommx -nokni -nok6 -nosse");
		if( SendMessageX(NoJoyButton,BM_GETCHECK,0,0)==BST_CHECKED )
			CmdLine+=TEXT(" -nojoy");
		if( SendMessageX(SoftwareButton,BM_GETCHECK,0,0)==BST_CHECKED )
			CmdLine+=TEXT(" -software");
		ShellExecuteX( NULL, TEXT("open"), ThisFile, *CmdLine, appBaseDir(), SW_SHOWNORMAL );
		Owner->EndDialog(0);
		return NULL;
	}
};

class WConfigPageSafeMode : public WWizardPage
{
	DECLARE_WINDOWCLASS(WConfigPageSafeMode,WWizardPage,Startup)
	WConfigWizard* Owner;
	WCoolButton RunButton, SafeModeButton, WebButton;
	WConfigPageSafeMode( WConfigWizard* InOwner )
	: WWizardPage    ( TEXT("ConfigPageSafeMode"), IDDIALOG_ConfigPageSafeMode, InOwner )
	, RunButton      ( this, IDC_Run,      FDelegate(this,(TDelegate)&WConfigPageSafeMode::OnRun) )
	, SafeModeButton ( this, IDC_SafeMode, FDelegate(this,(TDelegate)&WConfigPageSafeMode::OnSafeMode) )
	, WebButton      ( this, IDC_Web,      FDelegate(this,(TDelegate)&WConfigPageSafeMode::OnWeb) )
	, Owner          (InOwner)
	{}
	void OnRun()
	{
		Owner->EndDialog(1);
	}
	void OnSafeMode()
	{
		Owner->Advance( new WConfigPageSafeOptions(Owner) );
	}
	void OnWeb()
	{
		ShellExecuteX( *this, TEXT("open"), LocalizeGeneral(TEXT("WebPage"),TEXT("Startup")), TEXT(""), appBaseDir(), SW_SHOWNORMAL );
		Owner->EndDialog(0);
	}
	const TCHAR* GetNextText()
	{
		return NULL;
	}
};


/*-----------------------------------------------------------------------------
	Exec hook.
-----------------------------------------------------------------------------*/

// FExecHook.
class FExecHook : public FExec, public FNotifyHook
{
private:
	WConfigProperties* Preferences;
	void NotifyDestroy( void* Src )
	{
		if( Src==Preferences )
			Preferences = NULL;
	}
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar )
	{
		guard(FExecHook::Exec);
		if( ParseCommand(&Cmd,TEXT("SHOWLOG")) )
		{
			TObjectIterator<UEngine> EngineIt;
			if(	
			   EngineIt 
			&& EngineIt->Client 
			&& EngineIt->Client->Viewports.Num() 
			&& GLogWindow )
			{
				if( !GIsEditor )
					EngineIt->Client->Viewports(0)->Exec(TEXT("ENDFULLSCREEN"));
				GLogWindow->Show(1);
				SetFocus( *GLogWindow );
				GLogWindow->Display.ScrollCaret();
			}
			return 1;
		}
		else if( ParseCommand(&Cmd,TEXT("TakeFocus")) )
		{
			TObjectIterator<UEngine> EngineIt;
			if
			(	EngineIt
			&&	EngineIt->Client
			&&	EngineIt->Client->Viewports.Num() )
				SetForegroundWindow( (HWND)EngineIt->Client->Viewports(0)->GetWindow() );
			return 1;
		}
        // sjs ---
		else if( ParseCommand(&Cmd,TEXT("EditDefault")) )
		{
			UClass* Class;
			TObjectIterator<UEngine> EngineIt;
			if( EngineIt && ParseObject<UClass>( Cmd, TEXT("Class="), Class, ANY_PACKAGE ) )
			{
				AActor* Found   = Class->GetDefaultActor();
				if( Found )
				{
					Found->bSelected = 1;
					WObjectProperties* P = new WObjectProperties( TEXT("EditActor"), 0, TEXT(""), NULL, 1 );
					P->OpenWindow( (HWND)EngineIt->Client->Viewports(0)->GetWindow() );
					P->Root.SetObjects( (UObject**)&Found, 1 );
					P->Show(1);
				}
			}
			else Ar.Logf( TEXT("Missing class") );
			return 1;
		}
		else if( ParseCommand(&Cmd,TEXT("SAVEPROP")) )
		{
			TCHAR TempFname[MAX_PATH];
			Parse( Cmd, TEXT("File="), TempFname, ARRAY_COUNT(TempFname) ); // gam
			FString exportValue;
			FStringOutputDevice Buffer;
			for( TObjectIterator<AActor> It; It; ++It )
			{
				AActor* Actor = *It;
				if( Actor && Actor->bSelected && !Actor->bDeleteMe )
				{
					UClass* pClass	= Actor->GetClass();
					Buffer.Logf( TEXT("class * extends %s;\r\n\r\n"), pClass->GetName() );
					Buffer.Logf( TEXT("defaultproperties\r\n{\r\n") );
					for ( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> ItF(pClass); ItF; ++ItF )
					{
						if ( !(ItF->PropertyFlags & CPF_Edit) )
							continue;
						if ( appStricmp( ItF->GetName(), TEXT("Location")) == 0 )
							continue;
						if ( appStricmp( ItF->GetName(), TEXT("Rotation")) == 0 )
							continue;
						if ( appStricmp( ItF->GetName(), TEXT("Name")) == 0 )
							continue;
						exportValue.Empty();
						// support arrays
						for( INT Index=0; Index<ItF->ArrayDim; Index++ )
						{
							TCHAR Value[2048]; // amb: increased buffer size TODO: fix ExportText buffer overflow
							if ( ItF->ExportText( Index, Value, (BYTE*)Actor, &pClass->Defaults(0), 0) )
							{
                                Value[2047] = 0; // amb: cap it just in case the buffer is not big enough
								if( ItF->ArrayDim!=1 )
									exportValue += FString::Printf( TEXT("\t%s(%i)="), ItF->GetName(), Index );
								else
									exportValue += FString::Printf( TEXT("\t%s="), ItF->GetName() );
								exportValue += Value;
								exportValue += TEXT("\r\n");
							}
						}
						Buffer.Logf( *exportValue );
						//debugf( TEXT("%s"), *exportValue );
					}
					Buffer.Logf( TEXT("}") );
					Buffer.Logf( TEXT("\r\n\r\n") );
				}
			}
			/* TODO: fix this!
            for( TObjectIterator<UClass> ItC; ItC; ++ItC )
			{
				if( ItC && ItC->GetDefaultActor()->bSelected )
				{
					AActor * Actor = ItC->GetDefaultActor();
					UClass* pClass	= Actor->GetClass();
					Buffer.Logf( TEXT("class TEMPLATE* extends %s;\r\n\r\n"), pClass->GetName() );
					Buffer.Logf( TEXT("defaultproperties\r\n{\r\n") );
					for ( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> ItF(pClass); ItF; ++ItF )
					{
						if ( !(ItF->PropertyFlags & CPF_Edit) )
							continue;
						if ( appStricmp( ItF->GetName(), TEXT("Location")) == 0 )
							continue;
						if ( appStricmp( ItF->GetName(), TEXT("Rotation")) == 0 )
							continue;
						if ( appStricmp( ItF->GetName(), TEXT("Name")) == 0 )
							continue;
						exportValue.Empty();
						// support arrays
						for( INT Index=0; Index<ItF->ArrayDim; Index++ )
						{
							TCHAR Value[1024];
							if ( ItF->ExportText( Index, Value, (BYTE*)Actor, &pClass->Defaults(0), 0) )
							{
								if( ItF->ArrayDim!=1 )
									exportValue += FString::Printf( TEXT("\t%s(%i)="), ItF->GetName(), Index );
								else
									exportValue += FString::Printf( TEXT("\t%s="), ItF->GetName() );
								exportValue += Value;
								exportValue += TEXT("\r\n");
							}
						}
						Buffer.Logf( *exportValue );
						//debugf( TEXT("%s"), *exportValue );
					}
					Buffer.Logf( TEXT("}") );
					Buffer.Logf( TEXT("\r\n\r\n") );
				}
			}*/
			appSaveStringToFile( Buffer, TempFname );
			return 1;
		}
        /*
        else if( ParseCommand(&Cmd,TEXT("LOADPROP")) )
	    {
            TCHAR TempFname[MAX_PATH];
		    if ( Parse( Cmd, TEXT("File="), TempFname, ARRAY_COUNT(TempFname) ) ) // gam
		    {
			    UTextBuffer* Text = ImportObject<UTextBuffer>(GEditor->Level, GetTransientPackage(), NAME_None, 0, TempFname );
			    if( Text )
			    {
				    Text->AddToRoot();
				    FString* PropText = &Text->Text;
				    if( PropText )
				    {
					    guard(ImportActorProperties);
					    for( INT i=0; i<Level->Actors.Num(); i++ )
					    {
						    AActor* Actor = Level->Actors(i);
						    if( Actor && Actor->bSelected )
                            {
							    ImportProperties( Actor->GetClass(), (BYTE*)Actor, Actor->GetLevel(), **PropText, Actor->GetOuter(), GWarn, 0 );
                                Actor->PostEditChange();
                            }
					    }
					    unguard;
				    }
				    Text->RemoveFromRoot();
				    delete Text;
			    }
		    }
		    return 1;
	    }*/
		// --- sjs

        else if( ParseCommand(&Cmd,TEXT("EditObj")) )
        {
			TObjectIterator<UEngine> EngineIt;

            UObject* Obj = FindObject<UObject>( ANY_PACKAGE, Cmd );

			if( !EngineIt )
            	Ar.Logf( TEXT("EngineIt stinky") );
            else if( !Obj )
            	Ar.Logf( TEXT("Object \"%s\" not found"), Cmd );
            else
            {
				if( !GIsEditor )
					EngineIt->Client->Viewports(0)->Exec(TEXT("ENDFULLSCREEN"));
				WObjectProperties* P = new WObjectProperties( TEXT("EditObj"), 0, TEXT(""), NULL, 1 );
				P->OpenWindow( (HWND)EngineIt->Client->Viewports(0)->GetWindow() );
				P->Root.SetObjects( &Obj, 1 );
				P->Show(1);
            }

			return 1;
        }

		else if( ParseCommand(&Cmd,TEXT("EditActor")) )
		{
			UClass* Class;
			TObjectIterator<UEngine> EngineIt;
#if 1 //NEW (mdf) (Name= support)
			UObject* Found   = NULL;

			if( EngineIt )
			{
				AActor* Player  = EngineIt->Client ? EngineIt->Client->Viewports(0)->Actor : NULL;
				if ( !Player || (Player->Level->NetMode != NM_Client) )
				{
					if( ParseObject<UClass>( Cmd, TEXT("Class="), Class, ANY_PACKAGE ) )
					{
						if( !Found )
						{
							FLOAT   MinDist = 999999.0;
							for( TObjectIterator<AActor> It; It; ++It )
							{
								FLOAT Dist = Player ? FDist(It->Location,Player->Location) : 0.0;
								if
								(	(!Player || It->GetLevel()==Player->GetLevel())
								&&	(!It->bDeleteMe)
								&&	(It->IsA( Class) )
								&&	(Dist<MinDist) )
								{
									MinDist = Dist;
									Found   = *It;
								}
							}
						}
					}
					else
					{
						FName ActorName;
						if( Parse( Cmd, TEXT("Name="), ActorName ) )
						{
							// look for actor by name
							for( TObjectIterator<AActor> It; It; ++It )
							{
								if( !It->bDeleteMe && It->GetName() == *ActorName )
								{
									Found = *It;
									break;
								}
							}
						}
					}
				}

				if( Found )
				{
					if( !GIsEditor )
						EngineIt->Client->Viewports(0)->Exec(TEXT("ENDFULLSCREEN"));
					WObjectProperties* P = new WObjectProperties( TEXT("EditActor"), 0, TEXT(""), NULL, 1 );
					P->OpenWindow( (HWND)EngineIt->Client->Viewports(0)->GetWindow() );
					P->Root.SetObjects( (UObject**)&Found, 1 );
					P->Show(1);
				}
				else 
					Ar.Logf( TEXT("Target not found") );
			}
#else
			if( EngineIt && ParseObject<UClass>( Cmd, TEXT("Class="), Class, ANY_PACKAGE ) )
			{
				UObject* Found   = NULL;
				if( ParseCommand(&Cmd,TEXT("Defaults")) )
				{
					Found = Class->GetDefaultObject();
				}
				else
				{
					AActor* Player  = EngineIt->Client ? EngineIt->Client->Viewports(0)->Actor : NULL;
					FLOAT   MinDist = 999999.0;
					for( TObjectIterator<AActor> It; It; ++It )
					{
						FLOAT Dist = Player ? FDist(It->Location,Player->Location) : 0.0;
						if
						(	(!Player || It->GetLevel()==Player->GetLevel())
						&&	(!It->bDeleteMe)
						&&	(It->IsA( Class) )
						&&	(Dist<MinDist) )
						{
							MinDist = Dist;
							Found   = *It;
						}
					}
				}
				if( Found )
				{
					if( !GIsEditor )
						EngineIt->Client->Viewports(0)->Exec(TEXT("ENDFULLSCREEN"));

					WObjectProperties* P = new WObjectProperties( TEXT("EditActor"), 0, TEXT(""), NULL, 1 );
					P->OpenWindow( (HWND)EngineIt->Client->Viewports(0)->GetWindow() );
					P->Root.SetObjects( (UObject**)&Found, 1 );
					P->Show(1);
				}
				else 
					Ar.Logf( TEXT("Actor not found") );
			}
			else 
				Ar.Logf( TEXT("Missing class") );
#endif
			return 1;
		}
		else if( ParseCommand(&Cmd,TEXT("CopyToClipboard")) )
		{
			UClass* Class;
			TObjectIterator<UEngine> EngineIt;
			if( EngineIt && ParseObject<UClass>( Cmd, TEXT("Class="), Class, ANY_PACKAGE ) )
			{
				UObject* Found   = NULL;
				if( ParseCommand(&Cmd,TEXT("Defaults")) )
				{
					Found = Class->GetDefaultObject();
				}
				else
				{
					AActor* Player  = EngineIt->Client ? EngineIt->Client->Viewports(0)->Actor : NULL;
					FLOAT   MinDist = 999999.0;
					for( TObjectIterator<AActor> It; It; ++It )
					{
						FLOAT Dist = Player ? FDist(It->Location,Player->Location) : 0.0;
						if
						(	(!Player || It->GetLevel()==Player->GetLevel())
						&&	(!It->bDeleteMe)
						&&	(It->IsA( Class) )
						&&	(Dist<MinDist) )
						{
							MinDist = Dist;
							Found   = *It;
						}
					}
				}
				if( Found )
				{
					FStringOutputDevice Ar;
					UExporter::ExportToOutputDevice( Found, NULL, Ar, TEXT("T3D"), 0 );
					appClipboardCopy( *Ar );
				}
				else Ar.Logf( TEXT("Actor not found") );
			}
			else Ar.Logf( TEXT("Missing class") );
			return 1;
		}
		else if( ParseCommand(&Cmd,TEXT("HideLog")) )
		{
			if( GLogWindow )
				GLogWindow->Show(0);
			return 1;
		}
		else if( ParseCommand(&Cmd,TEXT("Preferences")) )
		{
			TObjectIterator<UEngine> EngineIt;
			if(	
			   EngineIt 
			&& EngineIt->Client 
			&& EngineIt->Client->Viewports.Num() 
			&& !GIsEditor
			)
				EngineIt->Client->Viewports(0)->Exec(TEXT("ENDFULLSCREEN"));

			if( !Preferences )
			{
				Preferences = new WConfigProperties( TEXT("Preferences"), LocalizeGeneral("AdvancedOptionsTitle",TEXT("Window")) );
				Preferences->SetNotifyHook( this );
				Preferences->OpenWindow( GLogWindow ? GLogWindow->hWnd : NULL );
				Preferences->ForceRefresh();
			}
			Preferences->Show(1);
			SetFocus( *Preferences );
			return 1;
		}
		else return 0;
		unguard;
	}
public:
	FExecHook()
	: Preferences( NULL )
	{}
};

/*-----------------------------------------------------------------------------
	Startup and shutdown.
-----------------------------------------------------------------------------*/

#ifndef _EDITOR_
//
// Initialize.
//
static UEngine* InitEngine()
{
	guard(InitEngine);
    appResetTimer(); // sjs
	DOUBLE LoadTime = appSeconds();

	// Set exec hook.
	static FExecHook GLocalHook;
	GExec = &GLocalHook;

	// Create mutex so installer knows we're running.
	CreateMutexX( NULL, 0, TEXT("UnrealIsRunning"));
	UBOOL AlreadyRunning;
	AlreadyRunning = (GetLastError()==ERROR_ALREADY_EXISTS);

	// First-run menu.
	INT FirstRun=0;
	GConfig->GetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );
	if( ParseParam(appCmdLine(),TEXT("FirstRun")) )
		FirstRun=0;
	if( FirstRun<220 )
	{
		// Migrate savegames.
		TArray<FString> Saves = GFileManager->FindFiles( TEXT("..\\Save\\*.usa"), 1, 0 );
		for( TArray<FString>::TIterator It(Saves); It; ++It )
		{
			INT Pos = appAtoi(**It+4);
			FString Section = TEXT("UnrealShare.UnrealSlotMenu");
			FString Key     = FString::Printf(TEXT("SlotNames[%i]"),Pos);
			if( appStricmp(GConfig->GetStr(*Section,*Key,TEXT("user")),TEXT(""))==0 )
				GConfig->SetString(*Section,*Key,TEXT("Saved game"),TEXT("user"));
		}
	}

	// EXEC from command-line
	FString Command;
	if( Parse(appCmdLine(),TEXT("consolecommand="), Command) )
	{
		debugf(TEXT("Executing console command %s"),*Command);
		GExec->Exec( *Command, *GLog );
		return NULL;
	}

	// Test render device.
	FString Device;
	if( Parse(appCmdLine(),TEXT("testrendev="),Device) )
	{
		debugf(TEXT("Detecting %s"),*Device);
		try
		{
			UClass* Cls = LoadClass<URenderDevice>( NULL, *Device, NULL, 0, NULL );
			GConfig->SetInt(*Device,TEXT("DescFlags"),RDDESCF_Incompatible);
			GConfig->Flush(0);
			if( Cls )
			{
				URenderDevice* RenDev = ConstructObject<URenderDevice>(Cls);
				if( RenDev )
				{
					if( RenDev->Init() )
					{
						debugf(TEXT("Successfully detected %s"),*Device);
					}
					else
					{
						delete RenDev;
						RenDev = NULL;
					}
				}
			}
		} catch( ... ) {}
		FArchive* Ar = GFileManager->CreateFileWriter(TEXT("Detected.ini"),0);
		if( Ar )
			delete Ar;
		return NULL;
	}

    // gam ---
    if( ParseParam(appCmdLine(),TEXT("debugging")) && (GFileManager->FileSize(TEXT("Running.ini")) >= 0) )
        GFileManager->Delete(TEXT("Running.ini"),0,0);
    // --- gam

	// Config UI.
	guard(ConfigUI);
	if( !GIsEditor && GIsClient )
	{
		WConfigWizard D;
		WWizardPage* Page = NULL;
		if( ParseParam(appCmdLine(),TEXT("safe")) || appStrfind(appCmdLine(),TEXT("readini")) )
		{
			Page = new WConfigPageSafeMode(&D);
			D.Title=LocalizeGeneral(TEXT("SafeMode"),TEXT("Startup"));
		}
// vogel: unwanted behaviour.
#if 0
		else if( FirstRun<ENGINE_VERSION )
		{
			GConfig->SetString(TEXT("Engine.Engine"),TEXT("RenderDevice"),TEXT("D3DDrv.D3DRenderDevice"));
		}
#endif
		//else if( !AlreadyRunning && GFileManager->FileSize(TEXT("Running.ini"))>=0 )
		//	{Page = new WConfigPageSafeMode(&D); D.Title=LocalizeGeneral(TEXT("RecoveryMode"),TEXT("Startup"));}
		if( Page )
		{
			ExitSplash();
			D.Advance( Page );
			if( !D.DoModal() )
				return NULL;
			InitSplash(NULL);
		}
	}
	unguard;

	// Create is-running semaphore file.
	FArchive* Ar = GFileManager->CreateFileWriter(TEXT("Running.ini"),0);
	if( Ar )
		delete Ar;

	// Update first-run.
	if( FirstRun<ENGINE_VERSION )
		FirstRun = ENGINE_VERSION;
	GConfig->SetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );


	// Create the global engine object.
	UClass* EngineClass;
	if( !GIsEditor )
	{
		// Create game engine.
		EngineClass = UObject::StaticLoadClass( UGameEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.GameEngine"), NULL, LOAD_NoFail, NULL );
	}
	else
	{
		// Editor.
		EngineClass = UObject::StaticLoadClass( UEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail, NULL );
	}
	UEngine* Engine = ConstructObject<UEngine>( EngineClass );
	Engine->Init();
	debugf( TEXT("Startup time: %f seconds"), appSeconds()-LoadTime );

	return Engine;
	unguard;
}

// Looks at all currently loaded packages and prompts the user to save them
// if their "bDirty" flag is set.
//
// Returns 0 if the user doesn't want to save one of the packages.
//
static UBOOL SaveDirtyPackages( UEngine* Engine )
{
	guard(SaveDirtyPackages);

	for( TObjectIterator<UPackage> It; It; ++It )
	{
		if( !It->GetOuter() && It->bDirty && appStricmp( It->GetName(), TEXT("mylevel") ) )
		{
			if( appMsgf( 1, TEXT("The package '%s' has been changed and needs to be saved.  Save it now?"), It->GetName() ) )
			{
				OPENFILENAMEA ofn;
				char File[8192] = "\0";

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = NULL;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.lpstrFilter = "Packages (*.*)\0*.*\0\0";
				ofn.lpstrDefExt = "";
				ofn.lpstrTitle = "Save Dirty Package";
				ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

				if( GetSaveFileNameA(&ofn) )
					Engine->Exec( *FString::Printf( TEXT("OBJ SAVEPACKAGE PACKAGE=\"%s\" FILE=\"%s\""), It->GetName(), appFromAnsi( File ) ) );
			}
		}
	}

#ifdef WITH_LIPSINC

	for( TObjectIterator<USkeletalMesh> It; It; ++It )
	{
		USkeletalMesh* SkelMesh = (USkeletalMesh*)*It;
		if( It->IsA(USkeletalMesh::StaticClass()) && SkelMesh->LIPSincController && SkelMesh->LIPSincController->GetDirty() )
		{
			if( appMsgf( 1, TEXT("The LIPSinc Controller attached to skeletal mesh '%s' has been changed and needs to be saved.  Save it now?"), It->GetName() ) )
			{
				debugf(TEXT("[LIPSinc]: Saving Controller..."));
				FString ControllerName;
				ControllerName = TEXT("./LIPSincData/");
				ControllerName += SkelMesh->GetName();
				ControllerName += TEXT(".lad");
				SkelMesh->LIPSincController->SaveToDisk( ControllerName );
				debugf(TEXT("[LIPSinc]: Saved."));
			}
		}
	}

#endif

	return 1;

	unguard;
}


//
// Unreal's main message loop.  All windows in Unreal receive messages
// somewhere below this function on the stack.
//

class CMainLoop
{
public:
	CMainLoop( UEngine* InEngine, UBOOL InIncludeMessaging = true );
	~CMainLoop(void);

	void	RunLoop(void);
	bool	Finished(void) const	{return !GIsRunning || GIsRequestingExit;}

private:
	UEngine *		Engine;
	UBOOL			IncludeMessaging;

	DWORD			ThreadId;
	HANDLE			hThread;
	DOUBLE			OldTime;
	DOUBLE			SecondStartTime;
	INT				TickCount;
	DWORD			LastFrameCycles;

	// Benchmarking.
	INT				BMFrames;
	INT				BMDiscardedFrames;
	INT				BMMaxFrames;
	FLOAT			BMSeconds;
	TArray<FString> BMStrings;
	TArray<FLOAT>	BMFrameTimes;
};

inline CMainLoop::CMainLoop( UEngine* InEngine, UBOOL InIncludeMessaging )
:	Engine(InEngine), IncludeMessaging(InIncludeMessaging)
{
	guard(MainLoopCtor);
	check(Engine);

	// Enter main loop.
	guard(EnterMainLoop);
	if( GLogWindow )
		GLogWindow->SetExec( Engine );
	unguard;

	// Loop while running.
	GIsRunning			= 1;
	ThreadId			= GetCurrentThreadId();
	hThread				= GetCurrentThread();
	OldTime				= appSeconds();
	SecondStartTime		= OldTime;
	TickCount			= 0;

	// Benchmarking.
	BMFrames			= 0;
	BMDiscardedFrames	= 10;
	BMSeconds			= 0.0f;
	
	BMMaxFrames			= 0; // gam
	Parse(appCmdLine(),TEXT("SECONDS="),BMMaxFrames);	
	if( GUseFixedTimeStep )
		BMMaxFrames		*= appRound( 1.f / GFixedTimeStep );

	if( GIsBenchmarking && BMMaxFrames )
	{	
		for( INT i=0; i<(BMMaxFrames-BMDiscardedFrames+1); i++ )
			new(BMStrings) FString(TEXT(""));
		if( BMMaxFrames )
			BMFrameTimes.Add(BMMaxFrames-BMDiscardedFrames+1);
	}

	LastFrameCycles		= appCycles();

	// Seed random number generator with constant seed for benchmarking.
	if( GUseFixedTimeStep )
		appRandInit( 0 );

	unguard;
}

inline void	CMainLoop::RunLoop(void)
{
	// Ever really needed ? - Erik
	if( Finished() ) 
		return;

	// Clear stats (will also update old stats).
	GStats.Clear();

    if( (GIsBenchmarking || GIsRecordingMovie) && BMMaxFrames )
	{
		if( BMFrames == BMDiscardedFrames )
			BMSeconds = 0.0f;
		else if( BMFrames > BMMaxFrames )
			appRequestExit(0);
	}

	// Update the world.
	DOUBLE NewTime   = appSeconds();
	FLOAT  DeltaTime = NewTime - OldTime;

	guard(UpdateWorld);
	if( GUseFixedTimeStep )
	{
		GDeltaTime = GFixedTimeStep;
		GCurrentTime += GFixedTimeStep;
		if( BMFrames < BMMaxFrames )
			BMSeconds += DeltaTime;
		BMFrames++;
	}
	else
	{
		GCurrentTime = NewTime;
		GDeltaTime = DeltaTime;
	}

	//Update.
	if( GDeltaTime < 0.f )
		GDeltaTime = 1 / 20.f;
	Engine->Tick( GDeltaTime );
	// sjs --- engine::tick may load a new map and cause the timing to be reset (this is a good thing)
	if( appSeconds() < NewTime )
           SecondStartTime = NewTime = GCurrentTime = appSeconds();
	// --- sjs
	if( GWindowManager )
		GWindowManager->Tick( GDeltaTime );

	TickCount++;
	OldTime = NewTime;

	if( OldTime > SecondStartTime + 1 )
	{
		if( GUseFixedTimeStep )
			Engine->CurrentTickRate = 1 / GFixedTimeStep;
		else
			Engine->CurrentTickRate = (FLOAT)TickCount / (OldTime - SecondStartTime);
		SecondStartTime = OldTime;
		TickCount = 0;
	}
	unguard;

	// Enforce optional maximum tick rate.
	guard(EnforceTickRate);
	if( !GUseFixedTimeStep && IncludeMessaging )
	{		
		FLOAT MaxTickRate = Engine->GetMaxTickRate();
		if( MaxTickRate>0.0 )
		{
			FLOAT Delta = (1.0/MaxTickRate) - (appSeconds()-OldTime);
			appSleep( Max(0.f,Delta) );
		}
	}
	unguard;

	if( IncludeMessaging ) 
	{
		// Handle all incoming messages.
		guard(MessagePump);
		MSG Msg;
		while( PeekMessageX(&Msg,NULL,0,0,PM_REMOVE) )
		{
			if( Msg.message == WM_QUIT )
			{
				// When closing down the editor, check to see if there are any unsaved dirty packages.
				if( GIsEditor )
					SaveDirtyPackages( Engine );

				GIsRequestingExit = 1;
			}

			guard(TranslateMessage);
			TranslateMessage( &Msg );
			unguardf(( TEXT("%08X %i"), (INT)((PTRINT)Msg.hwnd), Msg.message ));

			guard(DispatchMessage);
			DispatchMessageX( &Msg );
			unguardf(( TEXT("%08X %i"), (INT)((PTRINT)Msg.hwnd), Msg.message ));
		}
		unguard;
	}

	// If editor thread doesn't have the focus, don't suck up too much CPU time.
	if( GIsEditor && IncludeMessaging )
	{
		guard(ThrottleEditor);
		static UBOOL HadFocus=1;
		UBOOL HasFocus = (GetWindowThreadProcessId(GetForegroundWindow(),NULL) == ThreadId );
		if( HadFocus && !HasFocus )
		{
			// Drop our priority to speed up whatever is in the foreground.
			SetThreadPriority( hThread, THREAD_PRIORITY_BELOW_NORMAL );
		}
		else if( HasFocus && !HadFocus )
		{
			// Boost our priority back to normal.
			SetThreadPriority( hThread, THREAD_PRIORITY_NORMAL );
		}
		if( !HasFocus )
		{
			// Surrender the rest of this timeslice.
			Sleep(0);
		}
		HadFocus = HasFocus;
		unguard;
	}

	GStats.DWORDStats( GEngineStats.STATS_Frame_TotalCycles ) = appCycles() - LastFrameCycles;
	GStats.DWORDStats( GEngineStats.STATS_Game_ScriptCycles ) = GScriptCycles;
	GScriptCycles	= 0;
	LastFrameCycles = appCycles();

	if( GIsBenchmarking && BMMaxFrames )
	{
		if( BMFrames == 1 )
		{
			// Get descriptions.
			GStats.UpdateString( BMStrings(0), 1 );
			BMFrameTimes(0) = 0.f;
		}
		else
		{
			// Get frame stats. (Index already implies the +1 for descriptions)
			INT BMStringsIndex = Clamp( BMFrames - BMDiscardedFrames, 2, BMMaxFrames - BMDiscardedFrames );
			GStats.UpdateString( BMStrings(BMStringsIndex - 1), 0 );
			BMFrameTimes( BMStringsIndex - 1 ) = GStats.DWORDStats( GEngineStats.STATS_Frame_TotalCycles ) * GSecondsPerCycle * 1000.f;
		}
	}
}

inline CMainLoop::~CMainLoop(void)
{
	guard(CMainLoopDtor);

	GIsRunning = 0;
	if( GIsBenchmarking && BMMaxFrames )
	{
		INT Frames		= BMMaxFrames - BMDiscardedFrames; 
		UBOOL LogLowFPS = ParseParam(appCmdLine(),TEXT("ONLYLOGLOWFPS"));
		FLOAT LowFPS	= 100.f; // in ms

		// Level.
		FString LevelName = Engine->Client->Viewports(0)->Actor->GetViewTarget()->XLevel->GetPathName();
		LevelName = LevelName.LeftChop( 8 );

		// Get time & date.
		INT Year, Month, DayOfWeek, Day, Hour, Minutes, Sec, MSec;
		appSystemTime( Year, Month, DayOfWeek, Day, Hour, Minutes, Sec, MSec );
		FString DateTime = FString::Printf(TEXT("%i-%02i-%02i-%02i-%02i-%02i"),Year,Month,Day,Hour,Minutes,Sec);

		// Machine Details.
		FString MachineString = FString::Printf(TEXT("%s\r\n%s\r\n%s\r\n%s\r\n\r\n%s\r\n\r\n"),GBuildLabel,GMachineOS,GMachineCPU,GMachineVideo,appCmdLine());
		FString OutputString = MachineString;
		FString LowFPSString = TEXT("");

		// Count how many frames take more than LowFPS ms.
		if( LogLowFPS )
		{
			INT Count = 0;
			for( INT i=1; i<Frames; i++ )	
				if( BMFrameTimes(i) > LowFPS )
					Count++;
			
			LowFPSString = FString::Printf(TEXT("High frametimes: %i / %i == %f percent \r\n\r\n"), Count, Frames, 100.f * FLOAT(Count) / Frames ); 
			OutputString += LowFPSString;
		}

		// Determine min/ max framerate and score.
		FLOAT	MinFPS = 1000.f,
				MaxFPS = 0.f,
				AvgFPS = Frames / BMSeconds,
				Score  = 0.f;

		// Check for MAXFPS command line option.
		FLOAT FPSCap = 0;
		Parse(appCmdLine(),TEXT("MAXFPS="),FPSCap);
		if( FPSCap <= 0 )
			FPSCap = 10000;
		FLOAT FrameTimeCap = 1000.f / FPSCap;

		for( INT i=1; i<Frames; i++ )
		{
			// Calculate min/max.
			FLOAT FPS = 1000.f / BMFrameTimes(i);
			MinFPS = Min( MinFPS, FPS );
			MaxFPS = Max( MaxFPS, FPS );

			// Calculate score.
			Score += Max( BMFrameTimes(i), FrameTimeCap ) ;
		}

		Score = Frames / Score * 1000.f;

		// Output detailed results to a file.
		INT LastRand = appRand();
		for( INT i=0; i<Frames; i++ )
		{
			if( LogLowFPS && i!=0 )
			{
				if( BMFrameTimes(i) > LowFPS )
					OutputString += BMStrings(i);
			}
			else
				OutputString += BMStrings(i);
		}
		TCHAR File[1024];
		appSprintf( File, TEXT("..\\Benchmark\\CSVs\\stats-%s.csv"), *DateTime );
		appSaveStringToFile( OutputString, File );

		// Output average framerate.
		OutputString = TEXT("");
		appLoadFileToString( OutputString, TEXT("..\\Benchmark\\benchmark.log") );
		OutputString += FString::Printf(TEXT("%f / %f / %f fps -- Score = %f        rand[%i]\r\n"), MinFPS, AvgFPS, MaxFPS, Score, LastRand );
		appSaveStringToFile( OutputString, TEXT("..\\Benchmark\\benchmark.log") );
		OutputString = MachineString;
		if( LogLowFPS )
			OutputString += LowFPSString;
		OutputString += FString::Printf(TEXT("%f / %f / %f fps         rand[%i]\r\nScore = %f\r\n"), MinFPS, AvgFPS, MaxFPS, LastRand, Score );
		appSaveStringToFile( OutputString, *FString::Printf(TEXT("..\\Benchmark\\Results\\avgfps-%s.log"), *DateTime ) );
	
		// Output low framerate stats.
		OutputString = TEXT("");
		appLoadFileToString( OutputString, TEXT("..\\Benchmark\\lowframerate.log") );
		OutputString += FString::Printf(TEXT("%s\r\n%f / %f / %f fps\r\nScore = %f\r\n%s\r\n"), *LevelName, MinFPS, AvgFPS, MaxFPS, Score, *LowFPSString );
		appSaveStringToFile( OutputString, TEXT("..\\Benchmark\\lowframerate.log") );

		// Output average for benchmark launcher.
		if( ParseParam(appCmdLine(),TEXT("UPT") ) )
		{
			OutputString = FString::Printf(TEXT("%f"), AvgFPS );
			appSaveStringToFile( OutputString, TEXT("dummy.ben") );
		}

		GLog->Flush();
		GFileManager->Copy( *FString::Printf(TEXT("..\\Benchmark\\Logs\\ut2004-%s.log"),*DateTime), TEXT("ut2004.log") );
	}
	// Exit main loop.
	guard(ExitMainLoop);
	if( GLogWindow )
		GLogWindow->SetExec( NULL );
	GExec = NULL;
	unguard;

	unguard;
}

static void MainLoop( UEngine* Engine )
{
	guard(MainLoop);
	check(Engine);

	CMainLoop* theLoop = new CMainLoop(Engine);
	
	// Flush registry.	
	RegFlushKey(HKEY_CURRENT_USER);
	RegFlushKey(HKEY_CURRENT_CONFIG);
	RegFlushKey(HKEY_CLASSES_ROOT);
	RegFlushKey(HKEY_LOCAL_MACHINE);

	while (!theLoop->Finished()) 
	{
		theLoop->RunLoop();
	}
	
	delete theLoop;

	unguard;
}

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/























