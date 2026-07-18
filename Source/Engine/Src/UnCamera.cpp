/*=============================================================================
	UnViewport.cpp: Generic Unreal viewport code
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	URenderDevice.
-----------------------------------------------------------------------------*/

#define LINE_NEAR_CLIP_Z 1.f

//
// Init URenderDevice class.
//
void URenderDevice::StaticConstructor()
{
	guard(URenderDevice::StaticConstructor);

	new(GetClass(),TEXT("HighDetailActors"),		RF_Public)UBoolProperty(CPP_PROPERTY(HighDetailActors		), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("SuperHighDetailActors"),	RF_Public)UBoolProperty(CPP_PROPERTY(SuperHighDetailActors	), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("DetailTextures"),			RF_Public)UBoolProperty(CPP_PROPERTY(DetailTextures			), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("UseCompressedLightmaps"),	RF_Public)UBoolProperty(CPP_PROPERTY(UseCompressedLightmaps ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("UseStencil"),				RF_Public)UBoolProperty(CPP_PROPERTY(UseStencil				), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("Use16bit"),				RF_Public)UBoolProperty(CPP_PROPERTY(Use16bit				), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("Use16bitTextures"),		RF_Public)UBoolProperty(CPP_PROPERTY(Use16bitTextures		), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("LowQualityTerrain"),		RF_Public)UBoolProperty(CPP_PROPERTY(LowQualityTerrain		), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("SkyboxHack"),				RF_Public)UBoolProperty(CPP_PROPERTY(SkyboxHack				), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("TerrainLOD"),				RF_Public)UIntProperty (CPP_PROPERTY(TerrainLOD				), TEXT("Client"), CPF_Config );
	DecompFormat = TEXF_P8;

	unguard;
}

/*-----------------------------------------------------------------------------
	UViewport object implementation.
-----------------------------------------------------------------------------*/

void UViewport::Serialize( FArchive& Ar )
{
	guard(UViewport::Serialize);
	Super::Serialize( Ar );
	Ar << Console << MiscRes << Canvas << RenDev << Input;
	unguard;
}
void* UViewport::GetServer()
{
	guard(UViewport::GetServer);
	return NULL;
	unguard;
}
IMPLEMENT_CLASS(UViewport);

/*-----------------------------------------------------------------------------
	Dragging.
-----------------------------------------------------------------------------*/

//
// Set mouse dragging.
// The mouse is dragging when and only when one or more button is held down.
//
UBOOL UViewport::SetDrag( UBOOL NewDrag )
{
	guard(UViewport::SetDrag);
	UBOOL Result = Dragging;
	Dragging = NewDrag;
	if( GIsRunning )
	{
		if( Dragging && !Result )
		{
			// First hit.
			GetOuterUClient()->Engine->MouseDelta( this, MOUSE_FirstHit, 0, 0 );
		}
		else if( Result && !Dragging )
		{
			// Last release.
			GetOuterUClient()->Engine->MouseDelta( this, MOUSE_LastRelease, 0, 0 );
		}
	}
	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	Custom viewport creation and destruction.
-----------------------------------------------------------------------------*/

//
// UViewport constructor.  Creates the viewport object but doesn't open its window.
//
UViewport::UViewport() : RenderTarget(this)
{
	guard(UViewport::UViewport);

	// Update viewport array.
	GetOuterUClient()->Viewports.AddItem( this );

	// Create canvas.
	UClass* CanvasClass = StaticLoadClass( UCanvas::StaticClass(), NULL, TEXT("ini:Engine.Engine.Canvas"), NULL, LOAD_NoFail, NULL );
	Canvas = CastChecked<UCanvas>(StaticConstructObject( CanvasClass ));
	Canvas->Init( this );

	// Create input system.
	UClass* InputClass = StaticLoadClass( UInput::StaticClass(), NULL, TEXT("ini:Engine.Engine.Input"), NULL, LOAD_NoFail, NULL );
	Input = (UInput*)StaticConstructObject( InputClass );

	// Set initial time.
	CurrentTime				= GUseFixedTimeStep ? 0 : GCurrentTime;
	LastUpdateTime			= CurrentTime;

	UseSoftwareRasterizer	= 0;

	TerrainPointAtLocation	= FVector(0,0,0);
	MouseScreenPos			= FVector(0,0,0);
	MouseClientPos			= FVector(0,0,0);
	bLockOnSelectedActors	= 0;
	bShowLargeVertices		= 0;
	LockedActor				= NULL;
	ColorBytes				= 4; //!!HARDCODED - Engine only supports 32 bit at the moment - vogel

	ScaleX	= ScaleY		= 1.f;

	bRenderCinematics		= 0;
	CinematicsRatio			= 1.66f;

	DirtyViewport			= 0;

	// Setup pointer to the IM and then the default Console
	InteractionMaster		= GetOuterUClient()->InteractionMaster;
	
	if (InteractionMaster)
	{
		Console					= InteractionMaster->Console;

		// Create the Player Menu if one is defaulted (which can happilly overwrite the console :)
		// gam --- InteractionMaster->eventAddInteraction(TEXT("ini:Engine.Engine.DefaultPlayerMenu"),this);
	}

	unguard;
}

//
// Close a viewport.
//warning: Lots of interrelated stuff is happening here between the viewport code,
// object code, platform-specific viewport manager code, and Windows.
//
void UViewport::Destroy()
{
	guard(UViewport::Destroy);

	// Temporary for editor!!
	if( GetOuterUClient()->Engine->Audio && GetOuterUClient()->Engine->Audio->GetViewport()==this )
		GetOuterUClient()->Engine->Audio->SetViewport( NULL );

	// Close the viewport window.
	guard(CloseWindow);
	CloseWindow();
	unguard;

	// Delete the input subsystem.
	guard(ExitInput);
	delete Input;
	unguard;

	// Delete the console.
	guard(DeleteConsole);
	if( Console )
		delete Console;
	unguard;

	// Delete the canvas.
	guard(DeleteCanvas);
	delete Canvas;
	unguard;

	// Shut down rendering.
	guard(DeleteRenDev);
	if( RenDev )
	{
		if( RenDev != GetOuterUClient()->Engine->GRenDev ) 
		{
			RenDev->Exit(this);
			delete RenDev;
		}
		else
			RenDev->Flush(this);
		RenDev = NULL;
	}
	unguard;

	// Remove from viewport list.
	GetOuterUClient()->Viewports.RemoveItem( this );

	Super::Destroy();
	unguardobj;
}

/*---------------------------------------------------------------------------------------
	Viewport information functions.
---------------------------------------------------------------------------------------*/

//
// Is this camera a wireframe view?
//
UBOOL UViewport::IsWire()
{
	guard(UViewport::IsWire);
	return
		Actor &&
		(	(Actor->GetLevel()->Model->Nodes.Num()==0
			||	Actor->RendMap==REN_OrthXY
			||	Actor->RendMap==REN_OrthXZ
			||	Actor->RendMap==REN_OrthYZ
			||	Actor->RendMap==REN_Wire 
			)
			&&  ( Actor->RendMap!=REN_StaticMeshBrowser
			&&	Actor->RendMap!=REN_Prefab
			&&	Actor->RendMap!=REN_MeshView
			&&	Actor->RendMap!=REN_Animation

#ifdef WITH_LIPSINC
			&&  Actor->RendMap!=REN_LIPSinc
#endif
			&&	Actor->RendMap!=REN_MaterialEditor
			&&	Actor->RendMap!=REN_PrefabCompiled
			&&	Actor->RendMap!=REN_MatineePreview
			&&	Actor->RendMap!=REN_TexView )
		);

	unguard;
}

/*-----------------------------------------------------------------------------
	Viewport locking & unlocking.
-----------------------------------------------------------------------------*/

//
// Lock the viewport for rendering.
//
UBOOL UViewport::Lock( BYTE* HitData, INT* HitSize )
{
	guard(UViewport::Lock);
	check(RenDev);

	// Set info.
	if( GUseFixedTimeStep )
		CurrentTime += GFixedTimeStep;
	else
		CurrentTime = GCurrentTime;
	HitTesting = HitData!=NULL;
	FrameCount++;

	// Lock rendering device.
	RI = RenDev->Lock( this, HitData, HitSize );

	NextStencilMask = 1 << DEPTH_COMPLEXITY_BITS(this);

	return (RI != NULL);

	unguard;
}

//
// Unlock the viewport.
//
void UViewport::Unlock()
{
	guard(UViewport::Unlock);
	check(Actor);
	check(RenDev);
	check(HitSizes.Num()==0);

	// Unlock rendering device.
	RenDev->Unlock( RI );

	PendingFrame = 1;

	unguard;
}

//
// Present the contents of the framebuffer in the viewport.
//
void UViewport::Present()
{
	check(PendingFrame);
	PendingFrame = 0;

	if (bDumpingFrames)
		Exec(TEXT("SHOT"));

	MovieEncodeFrame();

	RenDev->Present( this );

	// Update time.

	LastUpdateTime = CurrentTime;
	DirtyViewport = 0;
}

/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/
INT QueueScreenShot = 0; // sjs - label hack

//
// UViewport command line.
//
UBOOL UViewport::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UViewport::Exec);
	check(Actor);
	if( Input && Input->Exec(Cmd,Ar) )
	{
		return 1;
	}
	else if( RenDev && RenDev->Exec(Cmd,Ar) )
	{
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("ISFULLSCREEN")) )
	{
		Ar.Log( IsFullscreen() ? TEXT("true") : TEXT("false"));
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("GETPING")) )
	{
		Ar.Logf( TEXT("0") );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("INJECT")) )
	{
		UNetDriver* Driver = Actor->GetLevel()->NetDriver;
		if( Driver && Driver->ServerConnection )
		{
			Ar.Logf(TEXT("Injecting: %s"),Cmd);
			Driver->ServerConnection->Logf( Cmd );
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("NETSPEED")) )
	{
		INT Rate = appAtoi(Cmd);
		UNetDriver* Driver = Actor->GetLevel()->NetDriver;
		if ( (Rate >= 1800) && (Actor->Level->TimeSeconds > Actor->NextSpeedChange) )
		{
			if ( Actor->Level->NetMode != NM_Standalone )
				Actor->NextSpeedChange = Actor->Level->TimeSeconds + 15;

			if ( !Driver || !Driver->ServerConnection )
			{
				GetDefault<UPlayer>()->ConfiguredInternetSpeed = Rate;
				GetDefault<UPlayer>()->SaveConfig();
				return 1;
			}

			if ( Driver->ServerConnection->URL.HasOption(TEXT("LAN")) )
				GetDefault<UPlayer>()->ConfiguredLanSpeed = Rate;
			else GetDefault<UPlayer>()->ConfiguredInternetSpeed = Rate;
			GetDefault<UPlayer>()->SaveConfig();

			CurrentNetSpeed = Driver->ServerConnection->CurrentNetSpeed = Clamp( Rate, 1800, Driver->MaxClientRate );
			Driver->ServerConnection->Logf(TEXT("NETSPEED %i"), Rate);
		}

		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOWALL")) )
	{
		for( INT i=0; i<Actor->GetLevel()->Actors.Num(); i++ )
			if( Actor->GetLevel()->Actors(i) )
				Actor->GetLevel()->Actors(i)->bHidden = 0;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("REPORT")) )
	{
		FStringOutputDevice Str;
		Str.Log ( TEXT("Report:\r\n") );
		Str.Logf( TEXT("   Version: %s\r\n"), GBuildLabel ); // gam 
		Str.Logf( TEXT("   URL: %s\r\n"), *Actor->GetLevel()->URL.String(0,1) );
		Str.Logf( TEXT("   Location: %i %i %i\r\n"), (INT)Actor->Location.X, (INT)Actor->Location.Y, (INT)Actor->Location.Z );

        // gam ---
		if( appStrlen( Cmd ) > 0 )
    		Str.Logf( TEXT("   Comment: %s\r\n"), Cmd );
		// --- gam

		if( Actor->Level->Game==NULL )
		{
			Str.Logf( TEXT("   Network client\r\n") );
		}
		else
		{
			Str.Logf( TEXT("   Game class: %s\r\n"), Actor->Level->Game->GetClass()->GetName() );
		}
		appClipboardCopy( *Str );
		
		debugf( TEXT("%s"), *Str ); // gam
		
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOT")) )
	{
#if 1 
		FMemMark Mark(GMem);
		FColor* Buf = new(GMem,SizeX*SizeY)FColor;
		RenDev->ReadPixels( this, Buf );

		FString ShotDir;
		if ( !GConfig->GetString( TEXT("Screenshots"), TEXT("Location"), ShotDir, TEXT("User.ini")) )
			ShotDir = TEXT("..\\ScreenShots");

		GFileManager->MakeDirectory( *ShotDir );

		FString ShotName;

		if( !ParseToken( Cmd, ShotName, 0 ) )
		{
			if ( !GConfig->GetString( TEXT("Screenshots"), TEXT("ShotMask"), ShotName, TEXT("user.ini")) )
				ShotName=TEXT("Shot%c");
		}

		ShotName = ShotName.Replace(TEXT("%m"),*Actor->Level->Title);

		INT Year, Month, DayOfWeek, Day, Hour, Min,Sec,MSec;
		appSystemTime(Year, Month, DayOfWeek, Day, Hour, Min,Sec,MSec);

		ShotName = ShotName.Replace(TEXT("%td"), *FString::Printf(TEXT("%i-%i-%i-%i"),Year,Month,Day,((Hour*3600)+(Min*60)+Sec)*1000+MSec));
		ShotName = ShotName.Replace(TEXT("%d"),  *FString::Printf(TEXT("%i-%i-%i"),Month,Day,Year));
		ShotName = ShotName.Replace(TEXT("%t"),  *FString::Printf(TEXT("%i"),((Hour*3600)+(Min*60)+Sec)*1000+MSec));

		if ( Actor && Actor->PlayerReplicationInfo )
			ShotName = ShotName.Replace(TEXT("%p"), *Actor->PlayerReplicationInfo->PlayerName);

		INT ShotCount=0;
		GConfig->GetInt(TEXT("Screenshots"),TEXT("ShotCount"),ShotCount,TEXT("User.ini"));

		ShotName = ShotName.Replace(TEXT("%c"), *FString::Printf(TEXT("%05i"),ShotCount));
		ShotName = FString::Printf(TEXT("%s\\%s"),*ShotDir,*ShotName);

		appCreateBitmap( *ShotName, SizeX, SizeY, (DWORD*) Buf, GFileManager );

		ShotCount++;
		GConfig->SetInt(TEXT("Screenshots"),TEXT("ShotCount"),ShotCount,TEXT("User.ini"));

		Mark.Pop();
#else
        QueueScreenShot = 1;
#endif
        return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOWACTORS")) )
	{
		if( Actor->Level->NetMode != NM_Standalone )
			return 0;

		Actor->ShowFlags |= SHOW_Actors;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("HIDEACTORS")) )
	{
		if( Actor->Level->NetMode != NM_Standalone )
			return 0;

		Actor->ShowFlags &= ~SHOW_Actors;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("RMODE")) )
	{
		if( Actor->Level->NetMode != NM_Standalone )
			return 0;

		INT Mode = appAtoi(Cmd);
		if( Mode>REN_None )
			Actor->RendMap = Mode;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("REND")) )
	{
		if( Actor->Level->NetMode != NM_Standalone )
			return 0;

		if( ParseCommand(&Cmd,TEXT("BLEND")) || ParseCommand(&Cmd,TEXT("NORMALS")))
		{
			bShowNormals = !bShowNormals;
			return 1;
		}
		if( ParseCommand(&Cmd,TEXT("BONE")) )
		{		
			bShowBones = !bShowBones;
			return 1;
		}
		if( ParseCommand(&Cmd,TEXT("SKIN")) )
		{		
			bHideSkin = !bHideSkin;
			return 1;
		}
		if( ParseCommand(&Cmd,TEXT("BOUND")) )
		{
			bShowBounds = !bShowBounds;
			return 1;
		}
		if( ParseCommand(&Cmd,TEXT("DEFAULT")) || ParseCommand(&Cmd,TEXT("RESET")))
		{
			bShowBounds = false;
			bShowNormals = false;
			bShowBones = false;
            bShowCollisionBounds = false; //amb
			return 1;
		}        
        // amb ---
        if( ParseCommand(&Cmd,TEXT("COLLISION")) )
        {
            bShowCollisionBounds = !bShowCollisionBounds;
            return 1;
        }
        // --- amb
				
		return 0;
	}
	else if( ParseCommand(&Cmd,TEXT("SHOW")) )
	{
		if( Actor->Level->NetMode != NM_Standalone )
			return 0;

		if( ParseCommand(&Cmd,TEXT("ACTORS")) )
			Actor->ShowFlags ^= SHOW_Actors;
		else if( ParseCommand(&Cmd,TEXT("ACTORINFO")) )
			Actor->ShowFlags ^= SHOW_ActorInfo;
		else if( ParseCommand(&Cmd,TEXT("STATICMESHES")) )
			Actor->ShowFlags ^= SHOW_StaticMeshes;
		else if( ParseCommand(&Cmd,TEXT("TERRAIN")) )
			Actor->ShowFlags ^= SHOW_Terrain;
		else if( ParseCommand(&Cmd,TEXT("FOG")) )
			Actor->ShowFlags ^= SHOW_DistanceFog;
		else if( ParseCommand(&Cmd,TEXT("SKY")) )
			Actor->ShowFlags ^= SHOW_Backdrop;
		else if( ParseCommand(&Cmd,TEXT("CORONAS")) )
			Actor->ShowFlags ^= SHOW_Coronas;
		else if( ParseCommand(&Cmd,TEXT("PARTICLES")) )
			Actor->ShowFlags ^= SHOW_Particles;
		else if( ParseCommand(&Cmd,TEXT("BSP")) )
			Actor->ShowFlags ^= SHOW_BSP;
		else if( ParseCommand(&Cmd,TEXT("RADII")) )
			Actor->ShowFlags ^= SHOW_ActorRadii;
		else if( ParseCommand(&Cmd,TEXT("FLUID")) )
			Actor->ShowFlags ^= SHOW_FluidSurfaces;
		else if( ParseCommand(&Cmd,TEXT("PROJECTORS")) )
			Actor->ShowFlags ^= SHOW_Projectors;
		else if( ParseCommand(&Cmd,TEXT("FALLBACKMATERIALS")) )
			Actor->ShowFlags ^= SHOW_NoFallbackMaterials;
		else if( ParseCommand(&Cmd,TEXT("COLLISION")) )
			Actor->ShowFlags ^= SHOW_Collision;
		else if( ParseCommand(&Cmd,TEXT("VOLUMES")))
			Actor->ShowFlags ^= SHOW_Volumes;

		// Invalidate all actors' static filter state.
		for(INT ActorIndex = 0;ActorIndex < Actor->XLevel->Actors.Num();ActorIndex++)
			Actor->XLevel->ResetStaticFilterState( Actor->XLevel->Actors(ActorIndex) );

		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("CINEMATICS")) )		// CINEMATICS [0/1]
	{
		if( Actor->Level->NetMode != NM_Standalone )
			return 0;

		// If 0 or 1 is specified, use that as the resulting bool.  Otherwise just toggle.
		if( ::appStrlen( Cmd ) )
			bRenderCinematics = appAtoi(Cmd);
		else
			bRenderCinematics = !bRenderCinematics;
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("CINEMATICSRATIO")) )
	{
		if( Actor->Level->NetMode != NM_Standalone )
			return 0;

		CinematicsRatio = appAtof(Cmd);
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("FIXEDVISIBILITY")) )
	{
		if( Actor->Level->NetMode != NM_Standalone )
			return 0;

		Actor->UseFixedVisibility = !Actor->UseFixedVisibility;
		if( Actor->UseFixedVisibility )
		{
			AActor*	CameraActor		= Actor;
			Actor->FixedLocation	= Actor->Location;
			Actor->FixedRotation	= Actor->Rotation;
			Actor->eventPlayerCalcView(CameraActor,Actor->FixedLocation,Actor->FixedRotation);
		}
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("TOGGLEREFRAST")) )
	{
		UseSoftwareRasterizer = !UseSoftwareRasterizer;
		RenDev->SetRes( this, SizeX, SizeY, IsFullscreen(), 0 );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("EXEC")) )
	{
		TCHAR Filename[512]; // gam
		if( ParseToken( Cmd, Filename, ARRAY_COUNT(Filename), 0 ) )
			ExecMacro( Filename, Ar );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("DUMPFRAMES")) )
	{
		if( ParseCommand(&Cmd,TEXT("START")) )
		{
			bDumpingFrames = true;
			GUseFixedTimeStep	= 1;
			GFixedTimeStep		= 1.f / 30;
			return 1;
		}
		else
		if( ParseCommand(&Cmd,TEXT("STOP")) )
		{
			if (GUseFixedTimeStep)
			{
				GUseFixedTimeStep = 0;
				GFixedTimeStep = 0;
				bDumpingFrames = false;
			}
			return 1;
		}
		return 1;
	}
			
	else if( ParseCommand(&Cmd,TEXT("RECORDMOVIE")) )
	{
		if( ParseCommand(&Cmd,TEXT("START")) )
		{
			FString	Filename;
			FLOAT	Quality = 0.5f;
			FLOAT   FixedFPS= 30.0;
			INT		Width	= 320,
					Height	= 240;

			if( !Parse( Cmd, TEXT("FILENAME="), Filename ) )
			{
				Ar.Logf( TEXT("Usage is: RECORDMOVIE START FILENAME=<x> QUALITY=<0-1> WIDTH=<x> HEIGHT=<x> FPS=<x>") );
				return 0;
			}

			Parse( Cmd, TEXT("QUALITY="), Quality	);	
			Parse( Cmd, TEXT("WIDTH=")  , Width		);	
			Parse( Cmd, TEXT("HEIGHT=") , Height	);
			Parse( Cmd, TEXT("FPS=")	, FixedFPS	);
		
			GUseFixedTimeStep	= 1;
			GFixedTimeStep		= 1.f / FixedFPS;

			MovieEncodeStart( Filename, Quality, Width, Height );

			return 1;
		}
		else
		if( ParseCommand(&Cmd,TEXT("STOP")) )
		{
			if (GUseFixedTimeStep)
			{
				GUseFixedTimeStep = 0;
				GFixedTimeStep = 0;
			}
			MovieEncodeStop();
			return 1;
		}
		else
		{
			Ar.Logf( TEXT("Usage is: RECORDMOVIE START FILENAME=<x> QUALITY=<0-1> WIDTH=<x> HEIGHT=<x> FPS=<x>") );
			Ar.Logf( TEXT("Usage is: RECORDMOVIE STOP") );
			return 0;
		}
	}
	
	else if( Console && Console->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
	{
		return 1;
	}
	else if( UPlayer::Exec(Cmd,Ar) )
	{
		return 1;
	}
	else return 0;
	unguard;
}

//
// Execute a macro on this viewport.
//
void UViewport::ExecMacro( const TCHAR* Filename, FOutputDevice& Ar )
{
	guard(UViewport::ExecMacro);

	// Create text buffer and prevent garbage collection.
	UTextBuffer* Text = ImportObject<UTextBuffer>( Actor->GetLevel(), GetTransientPackage(), NAME_None, 0, Filename );
	if( Text )
	{
		Text->AddToRoot();
		debugf( TEXT("Execing %s"), Filename );
		TCHAR Temp[256];
		const TCHAR* Data = *Text->Text;
		while( ParseLine( &Data, Temp, ARRAY_COUNT(Temp) ) )
			Exec( Temp, Ar );
		Text->RemoveFromRoot();
		delete Text;
	}
	else Ar.Logf( NAME_ExecWarning, LocalizeError("FileNotFound",TEXT("Core")), Filename );

	unguard;
}

/*-----------------------------------------------------------------------------
	UViewport FArchive interface.
-----------------------------------------------------------------------------*/

//
// Output a message on the viewport's console.
//

void UViewport::Serialize( const TCHAR* Data, EName MsgType )
{
	guard(UViewport::Serialize);

	if (InteractionMaster)
		InteractionMaster->MasterProcessMessage(Data, MsgType);

    // gam ---
    GLog->Serialize( Data, MsgType );
    // --- gam

	// Pass to console.
//	if( Console )
		//Console->Serialize( Data, MsgType );

	unguard;
}

/*-----------------------------------------------------------------------------
	Input.
-----------------------------------------------------------------------------*/

//
// Read input from the viewport.
//
void UViewport::ReadInput( FLOAT DeltaSeconds )
{
	guard(UViewport::ReadInput);
	check(Input);

	// Update input.
	if( DeltaSeconds!=-1.f )
		UpdateInput( 0, DeltaSeconds );

	// Get input from input system.
	Input->ReadInput( DeltaSeconds, *GLog );

	unguard;
}

/*-----------------------------------------------------------------------------
	Viewport hit testing.
-----------------------------------------------------------------------------*/

//
// Viewport hit-test pushing.
//
void UViewport::PushHit( const HHitProxy& Hit, INT Size )
{
	guard(UViewport::PushHit);

	Hit.Size = Size;
	HitSizes.AddItem(Size);
	RI->PushHit( (BYTE*)&Hit, Size );

	unguard;
}

//
// Pop the most recently pushed hit.
//
void UViewport::PopHit( UBOOL bForce )
{
	guard(UViewport::PopHit);
	checkSlow(RenDev);
	checkSlow(HitTesting);
	check(HitSizes.Num());

	RI->PopHit( HitSizes.Pop(), bForce );

	unguard;
}

//
// Execute all hits in the hit buffer.
//
void UViewport::ExecuteHits( const FHitCause& Cause, BYTE* HitData, INT HitSize, TCHAR* HitOverrideClass, FColor* HitColor, AActor** HitActor )
{
	guard(UViewport::ExecuteHits);

	// String together the hit stack.
	HHitProxy* TopHit=NULL, *BestHit=NULL;
	while( HitSize>0 )
	{
		HHitProxy* ThisHit = (HHitProxy*)HitData;
		HitData           += ThisHit->Size;
		HitSize           -= ThisHit->Size;
		ThisHit->Parent    = TopHit;
		TopHit			   = ThisHit;
		
		if( !HitOverrideClass || ThisHit->IsA(HitOverrideClass) )
			BestHit = ThisHit;
	}
	//!! FIXME for Direct3D editor.
	if( HitSize != 0 )
		return;

	// Call the innermost hit.
	if( BestHit )
	{
		if( HitColor )
			*HitColor = BestHit->HitColor;
		else
			if( HitActor )
				*HitActor = BestHit->GetActor();
			else
				BestHit->Click( Cause );
	}

	unguard;
}

//
//	FViewportRenderTarget::GetWidth
//

INT FViewportRenderTarget::GetWidth()
{
	return Viewport->SizeX;
}

//
//	FViewportRenderTarget::GetHeight
//

INT FViewportRenderTarget::GetHeight()
{
	return Viewport->SizeY;
}

//
//	FViewportRenderTarget::GetFormat
//

ETextureFormat FViewportRenderTarget::GetFormat()
{
	if(Viewport->ColorBytes == 2)
		return TEXF_RGB16;
	else
		return TEXF_RGBA8;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

