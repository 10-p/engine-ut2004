/*=============================================================================
	UnEngine.cpp: Unreal engine main.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnLinker.h"
#include "FFileManagerArc.h"

/*-----------------------------------------------------------------------------
	Object class implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UEngine);
IMPLEMENT_CLASS(URenderDevice);

/*-----------------------------------------------------------------------------
	Engine init and exit.
-----------------------------------------------------------------------------*/

//
// Construct the engine.
//
UEngine::UEngine()
{
	guard(UEngine::UEngine);
	unguard;
}

//
// Init class.
//
void UEngine::StaticConstructor()
{
	guard(UEngine::StaticConstructor);

	new(GetClass(),TEXT("CacheSizeMegs"),      RF_Public)UIntProperty (CPP_PROPERTY(CacheSizeMegs      ), TEXT("Settings"), CPF_Config );

	unguard;
}

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) ENGINE_API FName ENGINE_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "EngineClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

// Register natives.
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "EngineClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVES_ONLY
#undef NAMES_ONLY

//
// Init audio.
//
void UEngine::InitAudio()
{
	guard(UEngine::InitAudio);
	if
	(	UseSound
	&&	GIsClient
	&&	!ParseParam(appCmdLine(),TEXT("NOSOUND")) )
	{
		UClass* AudioClass = StaticLoadClass( UAudioSubsystem::StaticClass(), NULL, TEXT("ini:Engine.Engine.AudioDevice"), NULL, LOAD_NoFail, NULL );
		Audio = ConstructObject<UAudioSubsystem>( AudioClass );
		if( !Audio->Init() )
		{
#if defined(__ANDROID__)
			// Android: audio init failure is a warning, and the game continues without sound.
			debugf( NAME_Warning, TEXT("Audio unavailable on Android (no OpenAL) -- continuing without sound.") );
#else
			debugf( NAME_Error, TEXT("Audio initialization failed.") );
#endif
			delete Audio;
			Audio = NULL;
		}
	}
	unguard;
}

//
// Initialize the engine.
//
void UEngine::Init()
{
	guard(UEngine::Init);

	// Add the intrinsic names.
	#define NAMES_ONLY
	#define AUTOGENERATE_NAME(name) ENGINE_##name = FName(TEXT(#name),FNAME_Intrinsic);
	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "EngineClasses.h"
	#undef AUTOGENERATE_FUNCTION
	#undef AUTOGENERATE_NAME
	#undef NAMES_ONLY

	// Subsystems.
	FURL::StaticInit();
	GEngineMem.Init( 65536 );
#ifdef _XBOX
	GCache.Init( 1024 * 1024 * 1, 4096 );
#else
	GCache.Init( 1024 * 1024 * CacheSizeMegs, 4096 );
#endif
	GEngineStats.Init();

	// Initialize random number generator.
	if( GUseFixedTimeStep )
		appRandInit( 0 );
	else
		appRandInit( appCycles() );
	
	if(!GStatGraph)
		GStatGraph = new FStatGraph();

	if(!GTempLineBatcher)
		GTempLineBatcher = new FTempLineBatcher();

	// Objects.
	Cylinder = new UPrimitive;

	// Add to root.
	AddToRoot();

	// Create GGlobalTempObjects
	GGlobalTempObjects = new UGlobalTempObjects;

	// Ensure all native classes are loaded.
	for( TObjectIterator<UClass> It ; It ; ++It )
		if( !It->GetLinker() )
			LoadObject<UClass>( It->GetOuter(), It->GetName(), NULL, LOAD_Quiet|LOAD_NoWarn, NULL );

	debugf( NAME_Init, TEXT("Unreal engine initialized") );
	unguard;
}

//
// Exit the engine.
//
void UEngine::Destroy()
{
	guard(UEngine::Destroy);

	// Remove from root.
	RemoveFromRoot();

	// Shut down all subsystems.
	Audio	= NULL;
	Client	= NULL;
	FURL::StaticExit();
	GEngineMem.Exit();
	GCache.Exit( 1 );
	if(GStatGraph)
	{
		delete GStatGraph;
		GStatGraph = NULL;
	}
	
	if(GTempLineBatcher)
	{
		delete GTempLineBatcher;
		GTempLineBatcher = NULL;
	}
	
	Super::Destroy();
	unguard;
}

//
// Flush all caches.
//
void UEngine::Flush( UBOOL AllowPrecache )
{
	guard(UEngine::Flush);

	GCache.Flush();
	if( Client )
		Client->Flush( AllowPrecache );

	unguard;
}

//
// Update Gamma/Brightness/Contrast settings.
//
void UEngine::UpdateGamma()
{
	guard(UEngine::UpdateGamma);

	if( Client )
		Client->UpdateGamma();

	unguard;
}

//
// Restore Gamma/Brightness/Contrast settings.
//
void UEngine::RestoreGamma()
{
	guard(UEngine::RestoreGamma);

	if( Client )
		Client->RestoreGamma();

	unguard;
}

//
// Tick rate.
//
FLOAT UEngine::GetMaxTickRate()
{
	guard(UEngine::GetMaxTickRate);
	return ( GIsEditor ? 30 : 0);
	unguard;
}

//
// Progress indicator.
//
void UEngine::SetProgress( const TCHAR* CmdStr,  const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds )
{
	guard(UEngine::SetProgress);
	unguard;
}

//
// Serialize.
//
void UEngine::Serialize( FArchive& Ar )
{
	guard(UGameEngine::Serialize);

	Super::Serialize( Ar );
	Ar << Cylinder << Client << Audio << GRenDev;

	unguardobj;
}

#if DEMOVERSION
INT UEngine::DummyFunctionToBreakCompatibility( INT i )
{
	return i++;
}
#endif	

INT UEngine::AnotherDummyFunctionToBreakCompatibility( INT i )
{
	return i++;
}

INT UEngine::ReallyBreakCompatibility( INT i )
{
	return i++;
}

/*-----------------------------------------------------------------------------
	Input.
-----------------------------------------------------------------------------*/

struct FTextureSizeSort
{
	UBitmapMaterial*	Texture;
	INT					Size;

	FTextureSizeSort(UBitmapMaterial* InTexture)
	{
		Texture = InTexture;
		Size = 0;

		FTexture*	RenderInterface = Texture->GetRenderInterface()->GetTextureInterface();

		for(INT MipIndex = RenderInterface->GetFirstMip();MipIndex < RenderInterface->GetNumMips();MipIndex++)
			Size += GetBytesPerPixel(RenderInterface->GetFormat(),(RenderInterface->GetWidth() >> MipIndex) * (RenderInterface->GetHeight() >> MipIndex));
	}

	friend INT Compare(FTextureSizeSort& A,FTextureSizeSort& B)
	{
		return A.Size - B.Size;
	}
};

//
// This always going to be the last exec handler in the chain. It
// handles passing the command to all other global handlers.
//
UBOOL UEngine::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UEngine::Exec);

	// FLUSHCONFIG: force the config cache out to the filesystem, on demand.
	//
	// First, so no handler below can shadow it, and in UEngine because UEngine::Exec is the point
	// `ue_exec_command` reaches. SaveConfig already ends in GConfig->Flush(0); this is the explicit
	// "persist now" the host calls before harvesting the files into durable storage (a browser tab is
	// closed, never shut down).
	{
		const TCHAR* FlushStr = Cmd;
		if( ParseCommand(&FlushStr,TEXT("FLUSHCONFIG")) )
		{
			GConfig->Flush( 0 );
			Ar.Log( TEXT("ufront: flushed config to disk (ok)") );
			return 1;
		}
	}

	// See if any other subsystems claim the command.
	if( GSys    && GSys->Exec		(Cmd,Ar) ) return 1;
#ifdef _XBOX
	//!!vogel: use xbdbsmon to read output 
	if( UObject::StaticExec			(Cmd,*GLog) ) return 1;
#else
	if( UObject::StaticExec			(Cmd,Ar) ) return 1;
#endif
	if( GCache.Exec					(Cmd,Ar) ) return 1;
	if( GExec   && GExec->Exec      (Cmd,Ar) ) return 1;
	if( Client  && Client->Exec		(Cmd,Ar) ) return 1;
	if( Audio   && Audio->Exec		(Cmd,Ar) ) return 1;
	if( GStatGraph && GStatGraph->Exec(Cmd,Ar) ) return 1;

	// Handle engine command line.
	if( ParseCommand(&Cmd,TEXT("FLUSH")) )
	{
		Flush(1);
		Ar.Log( TEXT("Flushed engine caches") );
		return 1;
	}

#ifdef WITH_LIPSINC
	else if( ParseCommand(&Cmd,TEXT("LIPSINC")) )
	{
		// FIXME: HACK !!

		const TCHAR *Str = Cmd;

		TCHAR LIPSincAnimName[256];
		if(	ParseToken( Str, LIPSincAnimName, ARRAY_COUNT(LIPSincAnimName), 1 ) )
		{
			Ar.Logf( TEXT("Playing LIPSinc Animation: %s"), LIPSincAnimName );

			if( 1 )
			{
				AActor* Found   = NULL;

				APawn*  Player  = NULL;
				FLOAT   MinDist = 999999.0;

				for( TObjectIterator<APawn> It; It; ++It )
				{
					if ( It->IsHumanControlled() )
					{
						Player = *It;
					}
				}

				if( Player )
				{
					Ar.Logf(TEXT("Player found at location [%f, %f, %f]"), Player->Location.X, Player->Location.Y,
						Player->Location.Z);
				}
				else
				{
					Ar.Logf(TEXT("No Player?!"));
				}

				for( TObjectIterator<AActor> It; It; ++It )
				{
					FLOAT Dist = Player ? FDist(It->Location,Player->Location) : 0.0;
					if
						(	(!Player || It->GetLevel()==Player->GetLevel())
						&&	(!It->bDeleteMe)
						&&  (It->Mesh)
						&&	(It->Mesh->IsA(USkeletalMesh::StaticClass()) )
						&&  (Dist!=0.0)
						&&	(Dist<MinDist) )
					{
						USkeletalMesh* SkelMesh = (USkeletalMesh*)It->Mesh;
						if( SkelMesh->LIPSincController )
						{
							MinDist = Dist;
							Found   = *It;
						}
					}
				}

				if( Found )
				{
					Ar.Logf( TEXT("Found a skeletal mesh %s"), Found->GetName());

					//USkeletalMesh* SkelMesh = (USkeletalMesh*)Found->Mesh;

					//if( SkelMesh->LIPSincController )
					//{
					Ar.Logf( TEXT("Starting..."));
					((USkeletalMeshInstance*)Found->Mesh->MeshGetInstance(Found))->PlayLIPSincAnim(LIPSincAnimName, 1.0, 200.0, 1.0);
					//}
				}
			}
			else Ar.Logf( TEXT("No Skeletal Meshes Found.") );
		}
		return 1;
	}
#endif

	else if( ParseCommand(&Cmd,TEXT("CLOCK")) )
	{
		GIsClocking = !GIsClocking;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("STAT")) )
	{
		INT Result = 0;
		if( ParseCommand(&Cmd,TEXT("ANIM")) )
		{
			bShowAnimStats = !bShowAnimStats;
			Result = 1;
		}
		else if( ParseCommand(&Cmd,TEXT("LIPSINC")) )
		{
			// ifdef WITH_LIPSINC
			bShowLIPSincStats = !bShowLIPSincStats;
			return 1;
		}
		else if( ParseCommand(&Cmd,TEXT("LIGHT")) )
		{
			bShowLightStats = !bShowLightStats;
			Result = 1;
		}
		else if( ParseCommand(&Cmd,TEXT("DEFAULT")) || ParseCommand(&Cmd,TEXT("RESET")))
		{
			bShowAnimStats		= 0;
			
			// ifdef WITH_LIPSINC
			bShowLIPSincStats   = 0;

			bShowRenderStats	= 0;
			bShowHardwareStats	= 0;
			bShowMatineeStats	= 0;
			bShowGameStats		= 0;
			bShowAudioStats		= 0;
			bShowNetStats		= 0;
			bShowHistograph		= 0;
			Result = 1;
		}
		else if(ParseCommand(&Cmd,TEXT("FPS")))
		{
			bShowFrameRate = !bShowFrameRate;
			Result = 1;
		}
		else if(ParseCommand(&Cmd,TEXT("RENDER")))
		{
			bShowRenderStats = !bShowRenderStats;
			Result = 1;
		}
		else if(ParseCommand(&Cmd,TEXT("HARDWARE")))
		{
			bShowHardwareStats = !bShowHardwareStats;
			Result = 1;
		}
		else if(ParseCommand(&Cmd,TEXT("GAME")))
		{
			bShowGameStats = !bShowGameStats;
			Result = 1;
		}
		else if(ParseCommand(&Cmd,TEXT("HISTOGRAPH")))
		{
			bShowHistograph = !bShowHistograph;
			Result = 1;
		}
		else if(ParseCommand(&Cmd,TEXT("XBOXMEM")))
		{
			bShowXboxMemStats = !bShowXboxMemStats;
			Result = 1;
		}
		else if(ParseCommand(&Cmd,TEXT("MATINEE")))
		{
			bShowMatineeStats = !bShowMatineeStats;
			Result = 1;
		}
		else if(ParseCommand(&Cmd,TEXT("AUDIO")))
		{
			bShowAudioStats = !bShowAudioStats;
			Result = 1;
		}
		else if(ParseCommand(&Cmd,TEXT("NET")))
		{
			bShowNetStats		= !bShowNetStats;
			Result = 1;
		}
		else if(ParseCommand(&Cmd,TEXT("ALL")))
		{
			//bShowAnimStats	= 1;
			
			// ifdef WITH_LIPSINC
			bShowLIPSincStats   = 1;

			bShowFrameRate		= 1;
			bShowRenderStats	= 1;
			bShowHardwareStats	= 1;
			bShowMatineeStats	= 1;
			bShowGameStats		= 1;
			bShowAudioStats		= 1;
			bShowNetStats		= 1;
			Result = 1;
		}
		else if(ParseCommand(&Cmd,TEXT("NONE")))
		{
			bShowAnimStats		= 0;

			// ifdef WITH_LIPSINC
			bShowLIPSincStats   = 0;

			bShowFrameRate		= 0;
			bShowRenderStats	= 0;
			bShowHardwareStats	= 0;
			bShowMatineeStats	= 0;
			bShowGameStats		= 0;
			bShowAudioStats		= 0;
			bShowNetStats		= 0;
			Result = 1;
		}

		if ( Result )
		{
			if ( bShowAnimStats || bShowRenderStats || bShowHardwareStats || bShowMatineeStats
				|| bShowGameStats || bShowAudioStats || bShowNetStats )
			{
				GIsClocking = 1;
			}
			else
				GIsClocking = 0;
			return 1;
		}
		else
			return 0;
	}
	else if(ParseCommand(&Cmd,TEXT("texstats")))
	{
		// Dump texture stats in video memory.

		TArray<FTextureSizeSort>	SizeSortedTextures;

		for(TObjectIterator<UBitmapMaterial> It;It;++It)
		{
			FTexture*	RenderInterface = It->GetRenderInterface() ? It->GetRenderInterface()->GetTextureInterface() : NULL;

			if(RenderInterface && GRenDev->ResourceCached(RenderInterface->GetCacheId()))
				SizeSortedTextures.AddItem(FTextureSizeSort(*It));
		}

		Sort(&SizeSortedTextures(0),SizeSortedTextures.Num());

		INT	TotalTextureSize = 0;

		for(INT TextureIndex = 0;TextureIndex < SizeSortedTextures.Num();TextureIndex++)
		{
			FTexture*	RenderInterface = SizeSortedTextures(TextureIndex).Texture->GetRenderInterface()->GetTextureInterface();

			Ar.Logf(
				TEXT("%u bytes\t%ux%u\t%u mips\t%u BPP: %s"),
				SizeSortedTextures(TextureIndex).Size,
				RenderInterface->GetWidth() >> RenderInterface->GetFirstMip(),
				RenderInterface->GetHeight() >> RenderInterface->GetFirstMip(),
				RenderInterface->GetNumMips() - RenderInterface->GetFirstMip(),
				GetBytesPerPixel(RenderInterface->GetFormat(),8),
				SizeSortedTextures(TextureIndex).Texture->GetFullName()
				);

			TotalTextureSize += SizeSortedTextures(TextureIndex).Size;
		}

		Ar.Logf(
			TEXT("Total texture size: %u bytes"),
			TotalTextureSize
			);

		return 1;
	}
    else if( ParseCommand(&Cmd,TEXT("SHIP")) )
    {
        GShowBuildLabel = !GShowBuildLabel;
        return 1;
    }
    // --- gam
	else if( ParseCommand(&Cmd,TEXT("CRACKURL")) )
	{
		FURL URL(NULL,Cmd,TRAVEL_Absolute);
		if( URL.Valid )
		{
			Ar.Logf( TEXT("     Protocol: %s"), *URL.Protocol );
			Ar.Logf( TEXT("         Host: %s"), *URL.Host );
			Ar.Logf( TEXT("         Port: %i"), URL.Port );
			Ar.Logf( TEXT("          Map: %s"), *URL.Map );
			Ar.Logf( TEXT("   NumOptions: %i"), URL.Op.Num() );
			for( INT i=0; i<URL.Op.Num(); i++ )
				Ar.Logf( TEXT("     Option %i: %s"), i, *URL.Op(i) );
			Ar.Logf( TEXT("       Portal: %s"), *URL.Portal );
			Ar.Logf( TEXT("       String: '%s'"), *URL.String() );
		}
		else Ar.Logf( TEXT("BAD URL") );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("RENDEREMULATE")) )
	{
		if( ParseCommand(&Cmd,TEXT("gf1")) )
			GRenDev->SetEmulationMode( HEM_GeForce1 );
		else
		if( ParseCommand(&Cmd,TEXT("gf2")) )
			GRenDev->SetEmulationMode( HEM_GeForce1 );
		else
		if( ParseCommand(&Cmd,TEXT("xbox")) )
			GRenDev->SetEmulationMode( HEM_XBox );
		else
			GRenDev->SetEmulationMode( HEM_None );
		return 1;
	}
	else return 0;
	unguard;
}

//
// Key handler.
//
UBOOL UEngine::Key( UViewport* Viewport, EInputKey Key, TCHAR Unicode )
{
	guard(UEngine::Key);

	// Allow the Interaction Master to process the event.  If it doesn't for right now
	// we continue down the orignal road until I get the console recoding completed :)
	if( !GIsRunning )
		return false;
	else if(Client->InteractionMaster)
		return Client->InteractionMaster->MasterProcessKeyType(Key,Unicode); 
	else
		return false;

	unguard;
}

//
// Input event handler.
//
UBOOL UEngine::InputEvent( UViewport* Viewport, EInputKey iKey, EInputAction State, FLOAT Delta )
{
	guard(UEngine::InputEvent);


	// Allow the Interaction Master to process the event.  If it doesn't for right now
	// we continue down the orignal road until I get the console recoding completed :)

	if( !GIsRunning )
	{
		return 0;
	}
	else
	{
		if ( (Client->InteractionMaster) && ( Client->InteractionMaster->MasterProcessKeyEvent(iKey, State, Delta ) ) )
		{
			if ( (State==IST_Release) || (Client->InteractionMaster->bRequireRawJoystick) )
				Viewport->Input->PreProcess(iKey, State, Delta);

			return 1;
		}
		else if ( Viewport->Input->PreProcess( iKey, State, Delta ) && Viewport->Input->Process( *GLog, iKey, State, Delta ) )
		{
			// Input system handled it.
			return 1;
		}
	}

	return 0;

	unguard;
}

INT UEngine::ChallengeResponse( INT Challenge )
{
	guard(UEngine::ChallengeResponse);
	return 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	UServerCommandlet.
-----------------------------------------------------------------------------*/

void UServerCommandlet::StaticConstructor()
{
	guard(UServerCommandlet::StaticConstructor);

	LogToStdout = 1;
	IsClient    = 0;
	IsEditor    = 0;
	IsServer    = 1;
	LazyLoad    = 1;

	unguard;
}

#include <float.h> // sjs test!

#if __UNIX__  // rcg05102003 Added stdio console reading.  --ryan.
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

INT UServerCommandlet::Main( const TCHAR* Parms )
{
	guard(UServerCommandlet::Main);

	// Language.
	TCHAR Temp[256];
	if( GConfig->GetString( TEXT("Engine.Engine"), TEXT("Language"), Temp, ARRAY_COUNT(Temp) ) )
	UObject::SetLanguage( Temp );

	appResetTimer(); // sjs

	// Create the editor class.
	UClass* EngineClass = UObject::StaticLoadClass( UEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.GameEngine"), NULL, LOAD_NoFail, NULL );
	UEngine* Engine = ConstructObject<UEngine>( EngineClass );
	Engine->Init();

	#if __UNIX__  // rcg05102003 Added stdio console reading.  --ryan.
	int stdinfd = -1;
	if (Engine->ServerReadsStdin)
	{
		debugf(NAME_Init, TEXT("ServerReadsStdin: Setting stdin non-blocking."));
		setbuf(stdin, NULL);  // no buffering.
		stdinfd = fileno(stdin);  // probably zero, but just in case...
		int flags = fcntl(stdinfd, F_GETFL, 0);
		if (flags == -1)
			stdinfd = -1;
		else
		{
			flags |= O_NONBLOCK;
			if (fcntl(stdinfd, F_SETFL, flags) == -1)
			{
				debugf(NAME_Warning, TEXT("Failed to set stdin non-blocking."));
				stdinfd = -1;
			}
		}
	}
	#endif

	// Main loop.
	GIsRunning = 1;
	DOUBLE OldTime = appSeconds();
	DOUBLE SecondStartTime = OldTime;
	INT TickCount = 0;
	while( GIsRunning && !GIsRequestingExit )
	{
		// Clear stats (will also update old stats).
		GStats.Clear();

		#if __UNIX__  // rcg05102003 Added stdio console reading.  --ryan.
		guard(stdin_reading);
		if (stdinfd != -1)  // don't read if it would block/fail or not wanted.
		{
			static FStringNoInit stdincmd;
			char stdinbuf[128];
			int rc = read(stdinfd, stdinbuf, sizeof (stdinbuf) - 1);
			if (rc == -1)
			{
				if ((errno != EAGAIN) && (errno != EWOULDBLOCK))
				{
					debugf( NAME_Warning, TEXT("read(stdin) failed! errno=%d"), (int) errno );
					stdinfd = -1;  // stdin died?! Oh well, stop reading from it.
				}
			}
			else
			{
				stdinbuf[rc] = '\0';
				for (int i = 0; i < rc; i++)
				{
					char *ptr = stdinbuf + i;
					if ((*ptr == '\r') || (*ptr == '\n'))  // endline.
					{
						*ptr = '\0';  // chop endline.
						stdincmd += stdinbuf;
						if (stdincmd.Len() > 0)
						{
							//debugf(TEXT("Executing command from stdin: [%s]."), *stdincmd);
							Engine->Exec( *stdincmd );
							stdincmd = TEXT("");
						}
						rc -= i + 1;
						memmove(stdinbuf, ptr + 1, rc);
						i = -1; // will be zero when hitting top of loop...
					}
				}

				if (rc > 0)  // partial string...
					stdincmd += stdinbuf;
			}
			unguard;
		}
		#endif

		// Update the world.
		guard(UpdateWorld);
		DOUBLE NewTime = appSeconds();
		GCurrentTime = NewTime;
		GDeltaTime = NewTime - OldTime;
		if( GDeltaTime < 0.f )
			GDeltaTime = 1 / 20.f;
		Engine->Tick( GDeltaTime );
		// sjs --- engine::tick may load a new map and cause the timing to be reset (this is a good thing)
		if( appSeconds() < NewTime )
			SecondStartTime = NewTime = GCurrentTime = appSeconds();
		// --- sjs
		OldTime = NewTime;
		TickCount++;
		if( OldTime > SecondStartTime + 1 )
		{
			Engine->CurrentTickRate = (FLOAT)TickCount / (OldTime - SecondStartTime);
			SecondStartTime = OldTime;
			TickCount = 0;
		}
		unguard;

		// Enforce optional maximum tick rate.
		guard(EnforceTickRate);
		FLOAT MaxTickRate = Engine->GetMaxTickRate();
		if( MaxTickRate>0.f )
		{
			FLOAT Delta = (1.f/MaxTickRate) - (appSeconds()-OldTime);
			appSleep( Max(0.f,Delta) );
		}
		unguard;
	}
	GIsRunning = 0;
	return 0;
	unguard;
}

IMPLEMENT_CLASS(UServerCommandlet)


/*-----------------------------------------------------------------------------
	UUModUnpackCommandlet.
-----------------------------------------------------------------------------*/

void UUModUnpackCommandlet::StaticConstructor()
{
	guard(UUModUnpackCommandlet::StaticConstructor);

	LogToStdout     = 1;
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 1;
	ShowErrorCount  = 1;

	unguard;
}

INT UUModUnpackCommandlet::Main( const TCHAR* Parms )
{
	guard(UUModUnpackCommandlet::Main);

    UBOOL listOnly = 0;
	FString Cmd;
	ParseToken(Parms,Cmd,0);
    if (Cmd == "-l")
        listOnly = 1;
    else if (Cmd == "-x")
        listOnly = 0;
    else
    {
		GWarn->Logf( TEXT("Unknown Command [%s]"),*Cmd);
		GIsRequestingExit=1;
        return 0;
    }

    FString ArchiveName;
	if( !ParseToken(Parms,ArchiveName,0) )
		appErrorf(TEXT("Source file not specified"));

    if (!listOnly)
    {
        GWarn->Logf(TEXT("=================================================="));
        GWarn->Logf(TEXT(" Unpacking [%s] ..."), *ArchiveName);
        GWarn->Logf(TEXT("=================================================="));
    }

    FFileManagerArc *fmarc = new FFileManagerArc(GFileManager,*ArchiveName,1);
	fmarc->Init(0);

    INT unpacked = 0;
    INT max = fmarc->Header._Items_.Num();
    for (INT i = 0; i < max; i++)
    {
        FArchiveItem &item = fmarc->Header._Items_(i);
        FString fname = FromArcFilename(*item._Filename_);
        if ((fname == TEXT("manifest.ini")) || (fname == TEXT("manifest.int")))
            continue;

        // !!! FIXME: This is lame.
        TCHAR *ptr = &fname[0];
        while (*ptr)
        {
            if ((*ptr == '\\') || (*ptr == '/'))
                *ptr = PATH_SEPARATOR[0];
            ptr++;
        }

        if (listOnly)
        {
            GWarn->Logf(TEXT("%s"), *fname);
            continue;
        }

        INT fsize = item.Size;

        // actually unpack it...
        GWarn->Logf(TEXT(" + [%s] (%d bytes) ..."), *fname, fsize);

        FArchive *rarc = fmarc->CreateFileReader(*fname, 0, GError);
        if (rarc == NULL)
        {
            GWarn->Log(NAME_Warning,TEXT("Failed to create archive reader."));
            continue;
        }

        FArchive *warc = GFileManager->CreateFileWriter(*fname, FILEWRITE_NoReplaceExisting, GError);
        if (warc == NULL)
        {
            GWarn->Log(NAME_Warning,TEXT("Failed to create archive writer."));
            GWarn->Log(TEXT("   (perhaps file already exists?)"));
            delete rarc;
            continue;
        }

        BYTE buffer[1024];
        INT writecount = 0;
        while (writecount < fsize)
        {
            INT maxread = fsize - writecount;
            if (maxread > sizeof (buffer))
                maxread = sizeof (buffer);

            // !!! FIXME: Do these _always_ terminate the program on error?
            rarc->Serialize(buffer, maxread);
            warc->Serialize(buffer, maxread);

            writecount += maxread;
        }

        unpacked++;
        delete rarc;
        delete warc;
    }

    if (!listOnly)
    {
        GWarn->Logf(TEXT(""));
        GWarn->Logf(TEXT("    Unpacked %d files."), unpacked);
        GWarn->Logf(TEXT(""));
    }

    delete fmarc;
	GIsRequestingExit=1;
	return 0;
	unguard;
}

IMPLEMENT_CLASS(UUModUnpackCommandlet)


/*-----------------------------------------------------------------------------
	UMasterMD5Commandlet.
-----------------------------------------------------------------------------*/

void UMasterMD5Commandlet::StaticConstructor()
{
	guard(UMasterMD5Commandlet::StaticConstructor);

	LogToStdout     = 1;
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 1;
	ShowErrorCount  = 1;

	unguard;
}

// Process a directory for requested files

INT UMasterMD5Commandlet::ProcessDirectory(FString Directory, const TCHAR* Parms)
{
	guard(UMasterMD5Commandlet::ProcessDirectory);

	FString Wildcard;		
	FString SearchPath;
	if( !ParseToken(Parms,Wildcard,0) )
		appErrorf(TEXT("Source file(s) not specified"));
	do
	{
		INT	NewRevision=0;

		if ( appStrlen(Parms)>0 )
		{
			Parms++; // Skip the space
			NewRevision = appAtoi(Parms);
		}

		SearchPath.Empty();
		SearchPath = FString::Printf(TEXT("%s%s"),*Directory,*Wildcard);

		TArray<FString> FilesFound = GFileManager->FindFiles( *SearchPath, 1, 0 );
		for (INT i=0; i<FilesFound.Num(); i++)
		{
			FString Pkg = FString::Printf(TEXT("%s%s"),*Directory,*FilesFound(i));

			FString MD5Str;
			FString GUID;

			if ( DoQuickMD5(*Pkg,MD5Str,GUID) )
			{
				INT Index=-1;
				for (INT j=0;j<PackageValidation.Num();j++)
				{
					if ( !appStricmp(*GUID, *PackageValidation(j)->PackageID) )
					{
						Index = j;	// Reset the Index to 
						break;
					}
				}

				if (Index==-1)
				{
					debugf(TEXT("Adding New GUID %s for %s"),*GUID, *Pkg);
					debugf(TEXT("   Allowed MD5 0 [%s]"),*MD5Str);

					Index = PackageValidation.Num();
					PackageValidation.AddItem(ConstructObject<UPackageCheckInfo>(UPackageCheckInfo::StaticClass(),OutputPackage,NAME_None,RF_Public));
					PackageValidation(Index)->PackageID = GUID;
					new(PackageValidation(Index)->AllowedIDs)FString(MD5Str);

					PackageValidation(Index)->Native = true;
					PackageValidation(Index)->RevisionLevel = NewRevision;
				}
				else	// GUID Already exists
				{

					bool bFound=false;
					debugf(TEXT("GUID %s already exists.. "), *GUID);
					
					for (INT k=0;k<PackageValidation(Index)->AllowedIDs.Num();k++)
					{
						if ( !appStrcmp(*PackageValidation(Index)->AllowedIDs(k), *MD5Str ) ) 
						{
							bFound=true;
							break;
						}
					}

					if (!bFound)
					{
						debugf(TEXT("   Allowed MD5 %i [%s]"),PackageValidation(Index)->AllowedIDs.Num(),*MD5Str);
						new(PackageValidation(Index)->AllowedIDs)FString(MD5Str);
					}
					else
						debugf(TEXT("   MD5 %s is already allowed"),*MD5Str);
				}
			}
		}
	}
	while( ParseToken(Parms,Wildcard,0) );

	return 0;

	unguard;
}

INT UMasterMD5Commandlet::AddPackagesToDatabase(UBOOL Fresh, const TCHAR* Parms)
{
	guard(UMasterMD5Comandlet::AddPackagesToDatabase);
	PackageValidation.Empty();

	if (!Fresh)
		OutputPackage = LoadPackage( NULL, TEXT("Packages.md5"), 0 );
	
	if (!Fresh && OutputPackage)
    {
		GWarn->Logf( TEXT("Loading existing MD5 information..."));
		
		// Build the PackageValidation Array for quick lookup
	    for( FObjectIterator It; It; ++It )
	    {
		    UPackageCheckInfo *Info = (UPackageCheckInfo *) *It;
		    if(Info && Info->IsIn( OutputPackage ) )
			    PackageValidation.AddItem(Info);
	    }
    }
	else
	{
		GWarn->Logf( TEXT("Creating a new MD5 Database..."));
		OutputPackage = CreatePackage(NULL,TEXT("Packages.md5"));

		if (OutputPackage==NULL)
		{
			GWarn->Logf(TEXT("Failed!"));
			GIsRequestingExit=1;
			return 0;
		}
	}

	TArray<FString> DirsFound = GFileManager->FindFiles( TEXT("..\\*.*"), 0, 1 );
	for (INT i=0; i<DirsFound.Num(); i++)
	{
		FString ThisDir=FString::Printf(TEXT("..\\%s\\"),*DirsFound(i));
		ProcessDirectory(ThisDir,Parms);
	}
	
	DirsFound = GFileManager->FindFiles( TEXT("..\\MD5\\*.*"), 0, 1 );
	for (INT i=0; i<DirsFound.Num(); i++)
	{
		FString ThisDir=FString::Printf(TEXT("..\\MD5\\%s\\"),*DirsFound(i));
		ProcessDirectory(ThisDir,Parms);
	}

	GWarn->Logf( TEXT("=================================================="));
	GWarn->Logf( TEXT(" No of Packages in Array: %i"),PackageValidation.Num());
	GWarn->Logf( TEXT("=================================================="));

	FString Text=TEXT("");
	INT BestRevision = -1;
	for (INT i=0; i< PackageValidation.Num(); i++)
	{
		UPackageCheckInfo* P = PackageValidation(i);
	
		Text += P->PackageID;
		Text += FString::Printf(TEXT("MD5=%s\n\n"),*P->AllowedIDs(0));
		
		if (P->RevisionLevel > BestRevision)
			BestRevision = P->RevisionLevel;

		GWarn->Logf(TEXT("  Package GUID: %s Revision: %i Native %i"),*P->PackageID,P->RevisionLevel,P->Native);
		for (INT j=0;j<P->AllowedIDs.Num();j++)
			GWarn->Logf(TEXT("    MD5 #%i [%s]"),j,*P->AllowedIDs(j));
	}

	GWarn->Logf( TEXT("=================================================="));
	GWarn->Logf( TEXT("This MD5 Database is at revision level %i"),BestRevision);

	appSaveStringToFile(Text,TEXT("Packages.txt"), GFileManager);

	SavePackage(OutputPackage,NULL,RF_Public,TEXT("Packages.md5"),GWarn,NULL);
	GIsRequestingExit=1;
	return 0;

	unguard;
}

INT UMasterMD5Commandlet::ShowDatabase(const TCHAR* Parms)	
{
	guard(UMasterMD5Commandlet::ShowDatabase);

	UBOOL bBareList=false;
	Parms++;
	if (!appStricmp(Parms,TEXT("-b")))
			bBareList=true;

	OutputPackage = LoadPackage( NULL, TEXT("Packages.md5"), 0 );
	
	if (OutputPackage)
    {
		if (!bBareList)
		{
			GWarn->Logf( TEXT("Loading existing MD5 information..."));
			GWarn->Logf( TEXT(" "));
		}

		INT Count = 0;
		INT MaxRevision=-1;

	    for( FObjectIterator It; It; ++It )
	    {
		    UPackageCheckInfo *Info = (UPackageCheckInfo *) *It;
		    if(Info && Info->IsIn( OutputPackage ) )
			{
				// Display information about this PackageCheckInfo

				if (!bBareList)
					GWarn->Logf(TEXT("GUID: %s [rl=%i]"),*Info->PackageID,Info->RevisionLevel);

				for (INT i=0;i<Info->AllowedIDs.Num();i++)
				{
					if (!bBareList)
						GWarn->Logf(TEXT("    id %i: %s"),i,*Info->AllowedIDs(i));
					else
						GWarn->Logf(TEXT("%s\t%s\t%i"),*Info->PackageID,*Info->AllowedIDs(i),Info->RevisionLevel);
				}

				Count++;


				if (Info->RevisionLevel > MaxRevision)
					MaxRevision = Info->RevisionLevel;

			}
	    }

		if (!bBareList)
		{
			GWarn->Logf( TEXT("=================================================="));
			GWarn->Logf( TEXT(" No of Packages in database: %i"),Count);
			GWarn->Logf( TEXT(" Highest Revision Level    : %i"),MaxRevision);
			GWarn->Logf( TEXT("=================================================="));
		}

    }
	else
		GWarn->Logf( TEXT("Master MD5 Database does not exist!"));

	GIsRequestingExit=1;
	return 0;

	unguard;
}

INT UMasterMD5Commandlet::Revision(const TCHAR* Parms)		
{
	guard(UMasterMD5Commandlet::Revision);

	Parms++; // Skip the space

	INT	NewRevision = appAtoi(Parms);
	debugf(TEXT("Revision::Parms [%s][%i]"),Parms,NewRevision);

	if ( NewRevision>=0 )
	{
		OutputPackage = LoadPackage( NULL, TEXT("Packages.md5"), 0 );
		if (OutputPackage)
		{
			GWarn->Logf( TEXT("Loading existing MD5 information..."));
			GWarn->Logf( TEXT(" "));

			for( FObjectIterator It; It; ++It )
			{
				UPackageCheckInfo *Info = (UPackageCheckInfo *) *It;
				if(Info && Info->IsIn( OutputPackage ) )
				{
					Info->RevisionLevel = NewRevision;					
					GWarn->Logf(TEXT("Setting [%s] to revision level %i"),*Info->PackageID,Info->RevisionLevel);
				}
			}
			SavePackage(OutputPackage,NULL,RF_Public,TEXT("Packages.md5"),GWarn,NULL);
			GWarn->Logf( TEXT("=================================================="));
			GWarn->Logf( TEXT(" Database Updated"));
			GWarn->Logf( TEXT("=================================================="));

		}
		else
			GWarn->Logf( TEXT("Master MD5 Database does not exist!"));
	}
	else
		GWarn->Logf(TEXT("Illegal revision number"));

	GIsRequestingExit=1;
	return 0;

	unguard;
}

INT UMasterMD5Commandlet::FullMD5(const TCHAR* Parms)
{

	guard(UMasterMD5Commandlet::FullMD5);

	Parms++;

	GWarn->Logf( TEXT("Performing full MD5 on %s"),Parms);
	GWarn->Logf( TEXT(" "));

	
	FArchive* MD5Ar = GFileManager->CreateFileReader( Parms );
	int BytesToRead;
	if( !MD5Ar )
	{
		GWarn->Logf( TEXT("  ERROR: Could not open %s for reading!"),Parms);
		GWarn->Logf( TEXT(" "));
		return -1;
	}

	BYTE* MD5Buffer = (BYTE*)appMalloc(65535, TEXT(""));

	FMD5Context PMD5Context;
	appMD5Init( &PMD5Context );

	while ( MD5Ar->Tell() < MD5Ar->TotalSize() )
	{
		BytesToRead = MD5Ar->TotalSize() - MD5Ar->Tell();
		if (BytesToRead>65535)
			BytesToRead=65535;

		MD5Ar->Serialize(MD5Buffer, BytesToRead);
		appMD5Update( &PMD5Context, MD5Buffer, BytesToRead);
	}
	BYTE Digest[16];
	appMD5Final( Digest, &PMD5Context );

	// Convert to a string

	FString MD5Str;
	for (int i=0; i<16; i++)
		MD5Str += FString::Printf(TEXT("%02x"), Digest[i]);	


	delete MD5Ar;

	MD5Ar = GFileManager->CreateFileReader( Parms );
	if( !MD5Ar )
	{
		GWarn->Logf( TEXT("  ERROR: Could not open %s for reading!"),Parms);
		GWarn->Logf( TEXT(" "));
		return 0;
	}

	FPackageFileSummary Summary;
	*MD5Ar << Summary; 

	GWarn->Logf( TEXT(" GUID = [%s]"),Summary.Guid.String());
	GWarn->Logf( TEXT("  MD5 = [%s]"),*MD5Str);
	GWarn->Logf( TEXT(" "));

	// Free the buffer

	appFree(MD5Buffer);

	delete MD5Ar;

	return 0;
	unguard;
}

INT UMasterMD5Commandlet::QuickMD5(const TCHAR* Parms)
{
	guard(UMasterMD5Commandlet::QuickMD5);

	Parms++;

	GWarn->Logf( TEXT("Performing Quick MD5 on %s"),Parms);
	GWarn->Logf( TEXT(" "));
	
	FString MD5Str;
	FString Guid;
	
	if (!DoQuickMD5(Parms, MD5Str,Guid) ) // FString::Printf(TEXT("%s"),DoQuickMD5(Parms));
		return 0;

	GWarn->Logf( TEXT("  MD5 = [%s]"),*MD5Str);
	GWarn->Logf( TEXT(" "));

	return 0;
	unguard;

}

//FString UMasterMD5Commandlet::DoQuickMD5(const TCHAR *Parms)
INT UMasterMD5Commandlet::DoQuickMD5(const TCHAR *Parms, FString& MD5, FString& Guid)
{
	guard(UMasterMD5Commandlet::DoQuickMD5);

	FArchive* MD5Ar = GFileManager->CreateFileReader( Parms );
	if( !MD5Ar )
	{
		GWarn->Logf( TEXT("  ERROR: Could not open %s for reading!"),Parms);
		GWarn->Logf( TEXT(" "));
		return 0;
	}

	FMD5Context PMD5Context;
	
	BYTE QuickMD5Digest[16];	

	appMD5Init( &PMD5Context );

	FPackageFileSummary Summary;
	*MD5Ar << Summary; 

	// MD5sum the package summary...
	// !!! FIXME: Can we just hook this up to the serialization code?
	// !!! FIXME: Same code is in UnLinker.cpp!
	appMD5Update32( &PMD5Context, Summary.Tag );
	appMD5Update32( &PMD5Context, Summary.GetFileVersionComplete() );
	appMD5Update32( &PMD5Context, Summary.PackageFlags );
	appMD5Update32( &PMD5Context, Summary.NameCount );
	appMD5Update32( &PMD5Context, Summary.NameOffset );
	appMD5Update32( &PMD5Context, Summary.ExportCount );
	appMD5Update32( &PMD5Context, Summary.ExportOffset );
	appMD5Update32( &PMD5Context, Summary.ImportCount );
	appMD5Update32( &PMD5Context, Summary.ImportOffset );
	appMD5Update32( &PMD5Context, Summary.Guid.A );
	appMD5Update32( &PMD5Context, Summary.Guid.B );
	appMD5Update32( &PMD5Context, Summary.Guid.C );
	appMD5Update32( &PMD5Context, Summary.Guid.D );

	// MD5 the Generations table.
	for (int i=0;i<Summary.Generations.Num(); i++)
	{
		appMD5Update32(&PMD5Context, Summary.Generations(i).ExportCount);
		appMD5Update32(&PMD5Context, Summary.Generations(i).NameCount);
	}

	// MD5 the name table

	if( Summary.NameCount > 0 )
	{
		MD5Ar->Seek( Summary.NameOffset );
		for( INT i=0; i<Summary.NameCount; i++ )
		{
			// Read the name entry from the file.
			FNameEntry NameEntry;
			*MD5Ar << NameEntry; 

			appMD5Update32(&PMD5Context, NameEntry.Flags);

			// This is a mistake; the "sizeof" makes this "4" without ever calling appStrlen!!
			//appMD5Update(&PMD5Context, (BYTE*)&NameEntry.Name,sizeof(appStrlen(NameEntry.Name) * sizeof(TCHAR) ) );

			// Here's a byte-safe version that assumes we're sending four bytes. Remove this when
			//  fixing the above version.  --ryan.
            appMD5Update16(&PMD5Context, (_WORD) NameEntry.Name[0]);
            appMD5Update16(&PMD5Context, (_WORD) NameEntry.Name[1]);
		}
	}
	
	// Finalize the QuickMD5

	appMD5Final( QuickMD5Digest, &PMD5Context );
	delete MD5Ar;

	// Diplay the MD5

	MD5 = TEXT("");
	for (INT i=0; i<16; i++)
		MD5 += FString::Printf(TEXT("%02x"), QuickMD5Digest[i]);	

	Guid = FString::Printf(TEXT("%s"),Summary.Guid.String());
	return 1;
	unguard;
}

INT UMasterMD5Commandlet::Jack(const TCHAR* Parms)
{
	guard(UMasterMD5Comandlet::Jack);


	GWarn->Logf( TEXT("Dumping MD5 for Master Server"),Parms);
	GWarn->Logf( TEXT(" "));

	FString SearchPath;
	if( !ParseToken(Parms,SearchPath,0) )
		appErrorf(TEXT("Source file(s) not specified"));

	FString Revision;
	if (!ParseToken(Parms,Revision,0) )
		appErrorf(TEXT("Revision not specified"));

	Parms++;
	GWarn->Logf( TEXT("Parms: %s %s %s"), *SearchPath, *Revision,Parms);

	TArray<FString> DirsFound = GFileManager->FindFiles( *SearchPath , 1, 0 );

	for (INT i=0; i<DirsFound.Num(); i++)
	{

		FString MD5Str;
		FString GUID;

		if ( DoQuickMD5(*DirsFound(i),MD5Str,GUID) )
		{
			FString FileName = DirsFound(i);
			INT j = FileName.InStr( TEXT("\\"), 1 );
			if( j != -1 )
				FileName = FileName.Mid(j+1);
			GWarn->Logf( TEXT("INSERT INTO packagemd5 (guid, md5, revision, description) VALUES ('%s','%s',%s,'%s %s');"),*GUID, *MD5Str,*Revision, Parms, *FileName );
		}
		else
			GWarn->Logf( TEXT("  Error loaded %s"),*DirsFound(i));
	}
	
	return 0;

	unguard;
}

INT UMasterMD5Commandlet::Web(const TCHAR* Parms)
{
	guard(UMasterMD5Comandlet::Web);


	GWarn->Logf( TEXT("Dumping MD5 for Master Server in Web format"),Parms);
	GWarn->Logf( TEXT(" "));

	FString SearchPath;
	if( !ParseToken(Parms,SearchPath,0) )
		appErrorf(TEXT("Source file(s) not specified"));

	Parms++;
	GWarn->Logf( TEXT("Parms: %s"), *SearchPath);

	TArray<FString> DirsFound = GFileManager->FindFiles( *SearchPath , 1, 0 );

	GWarn->Logf(TEXT("-----------------"));
	for (INT i=0; i<DirsFound.Num(); i++)
	{

		FString MD5Str;
		FString GUID;

		if ( DoQuickMD5(*DirsFound(i),MD5Str,GUID) )
		{
			FString FileName = DirsFound(i);
			INT j = FileName.InStr( TEXT("\\"), 1 );
			if( j != -1 )
				FileName = FileName.Mid(j+1);
			GWarn->Logf( TEXT("%s %s %s"),*GUID, *MD5Str,*FileName );
		}
		else
			appErrorf( TEXT("  Error loaded %s"),*DirsFound(i));
	}
	GWarn->Logf(TEXT("-----------------"));

	return 0;

	unguard;
}

INT UMasterMD5Commandlet::ShowGUID(const TCHAR* Parms)				// Show the GUID of a package
{
	guard(UMasterMD5Commandlet::ShowGUID);

	Parms++;

	GWarn->Logf( TEXT(" Loading %s..."),Parms);

	FArchive* MD5Ar = GFileManager->CreateFileReader( Parms );
	MD5Ar = GFileManager->CreateFileReader( Parms );
	if( !MD5Ar )
	{
		GWarn->Logf( TEXT("  ERROR: Could not open %s for reading!"),Parms);
		GWarn->Logf( TEXT(" "));
		return 0;
	}

	FPackageFileSummary Summary;
	*MD5Ar << Summary; 

	GWarn->Logf( TEXT(" GUID = [%s]"),Summary.Guid.String());
	GWarn->Logf( TEXT(" "));

	delete MD5Ar;

	return 0;

	unguard;
}
INT UMasterMD5Commandlet::LinksTo(const TCHAR* Parms)				// Shows what a package links to
{
	guard(UMasterMD5Commandlet::LinksTo);

	Parms++;
	
	BeginLoad();
	ULinkerLoad* Linker = GetPackageLinker( NULL, Parms, LOAD_NoFail, NULL, NULL );
	EndLoad();
	if (!Linker)
		GWarn->Logf(TEXT("Could not load %s"),Parms);
    else
    {
		if ( Linker->LinksToCode() )
			GWarn->Logf(TEXT("%s links to code"),Parms);
		else
			GWarn->Logf(TEXT("%s does not link to code"),Parms);
    }

	delete Linker;

	return 0;
	unguard;
}

INT UMasterMD5Commandlet::GlobalID(const TCHAR* Parms)				// Shows what a package links to
{
	guard(UMasterMD5Commandlet::GlobalID);

	Parms++;

	const ANSICHAR* AnsiChallenge = appToAnsi( Parms );
	BYTE Digest[16];
	FMD5Context Context;
	appMD5Init( &Context );
	appMD5Update( &Context, (unsigned char*)AnsiChallenge, appStrlen( Parms ) );
	appMD5Final( Digest, &Context );

	FString HashStr;


	for (INT i=0; i<16; i++)
		HashStr += FString::Printf(TEXT("%02x"), Digest[i]);	
	
	FString FullKey = FString::Printf(TEXT("1230asadkgjk358dmvmbjt6838320yjkdhnchjg4958%s"),*HashStr);
	
	// Perform the MD5 hash of it

	const ANSICHAR* Hash = appToAnsi(*FullKey);
	BYTE HashDigest[16];

	appMD5Init( &Context );
	appMD5Update( &Context, (unsigned char*)Hash, FullKey.Len() );
	appMD5Final( HashDigest, &Context );
	
	FString SecureKey;

	for (INT i=0; i<16; i++)
		SecureKey += FString::Printf(TEXT("%02x"), HashDigest[i]);	
	
	//GWarn->Logf(TEXT("Original Key: %s"),Parms);
	//GWarn->Logf(TEXT("Actual Hesh : %s"),*HashStr);
	//GWarn->Logf(TEXT("Global Id   : %s"),*SecureKey);

	GWarn->Logf(TEXT("This command has been removed!"));

	return 0;
	unguard;
}

INT UMasterMD5Commandlet::Main( const TCHAR* Parms )
{
	guard(UMasterMD5Commandlet::Main);

	GWarn->Logf( TEXT("=================================================="));
	GWarn->Logf( TEXT(" MD5 Database Management"));
	GWarn->Logf( TEXT("=================================================="));
	GWarn->Logf( TEXT(" "));

	FString Cmd;
	ParseToken(Parms,Cmd,0);
	
	if ( !appStricmp(*Cmd, TEXT("-a")) )
		AddPackagesToDatabase(false, Parms);	

	else if ( !appStricmp(*Cmd,TEXT("-c")) )
		AddPackagesToDatabase(true, Parms);	

	else if ( !appStricmp(*Cmd, TEXT("-s")) )
		ShowDatabase(Parms);

	else if ( !appStricmp(*Cmd, TEXT("-r")) )
		Revision(Parms);

	else if ( !appStricmp(*Cmd, TEXT("-f")) )
		FullMD5(Parms);

	else if ( !appStricmp(*Cmd, TEXT("-q")) )
		QuickMD5(Parms);

	else if ( !appStricmp(*Cmd, TEXT("-j")) )
		Jack(Parms);

	else if ( !appStricmp(*Cmd, TEXT("-w")) )
		Web(Parms);

	else if ( !appStricmp(*Cmd, TEXT("-g")) )
		ShowGUID(Parms);

	else if ( !appStricmp(*Cmd, TEXT("-l")) )
		LinksTo(Parms);

	else if ( !appStricmp(*Cmd, TEXT("-i")) )
		GlobalID(Parms);		
	
	else
		GWarn->Logf( TEXT("Unknown Command [%s]"),Parms);
	
	return 0;

	unguard;
}

IMPLEMENT_CLASS(UMasterMD5Commandlet);

/*-----------------------------------------------------------------------------
	UGlobalTempObjects.
-----------------------------------------------------------------------------*/

ENGINE_API UGlobalTempObjects* GGlobalTempObjects = NULL;
IMPLEMENT_CLASS(UGlobalTempObjects)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

