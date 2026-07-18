/*=============================================================================
	EdHook.cpp: UnrealEd VB hooks.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

// Includes.
#pragma warning( disable : 4201 )
#pragma warning( disable : 4310 )
#define STRICT
#pragma pack(push,8)
#include <windows.h>
#include <shlobj.h>
#pragma pack(pop)
#include "UnrealEd.h"
#include "UnRender.h"
#include "..\..\Core\Inc\UnMsg.h"

// Thread exchange.
HANDLE			hEngineThreadStarted;
HANDLE			hEngineThread;
HWND			hWndEngine;
DWORD			EngineThreadId;
DLL_EXPORT FStringOutputDevice GetPropResult;
const TCHAR*	GTopic;
const TCHAR*	GItem;
const TCHAR*	GValue;
TCHAR*			GCommand;

extern int GLastScroll;
extern FString GMapExt;

// Misc.
UEngine* Engine;

// Config.
#include "FConfigCacheIni.h"

/*-----------------------------------------------------------------------------
	Editor hook exec.
-----------------------------------------------------------------------------*/

void UUnrealEdEngine::NotifyDestroy( void* Src )
{
	guard(UUnrealEdEngine::NotifyDestroy);
	if( Src==ActorProperties )
		ActorProperties = NULL;
	if( Src==LevelProperties )
		LevelProperties = NULL;
	if( Src==Preferences )
		Preferences = NULL;
	if( Src==UseDest )
		UseDest = NULL;
	unguard;
}
void UUnrealEdEngine::NotifyPreChange( void* Src )
{
	guard(UUnrealEdEngine::NotifyPreChange);
	Trans->Begin( TEXT("Edit Properties") );
	unguard;
}
void UUnrealEdEngine::NotifyPostChange( void* Src )
{
	guard(UUnrealEdEngine::NotifyPostChange);
	Trans->End();
	if( Src==Preferences )
	{
		GCache.Flush();
		for( TObjectIterator<UViewport> It; It; ++It )
			It->Actor->FovAngle = FovAngle;
	}
	else if( Src==ActorProperties )
	{
		EdCallback( EDC_ActorPropertiesChange, 1, 0 );
	}
	EdCallback( EDC_RedrawAllViewports, 0, 0 );
	RedrawLevel( Level );
	unguard;
}
AUTOREGISTER_TOPIC(TEXT("Obj"),ObjTopicHandler);
void ObjTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	guard(ObjTopicHandler::Get);
	if( ParseCommand(&Item,TEXT("QUERY")) )
	{
		UClass* Class;
		if( ParseObject<UClass>(Item,TEXT("TYPE="),Class,ANY_PACKAGE) )
		{
			UPackage* BasePackage;
			UPackage* RealPackage;
			TArray<UObject*> Results;
			if( !ParseObject<UPackage>( Item, TEXT("PACKAGE="), BasePackage, NULL ) )
			{
				// Objects in any package.
				for( FObjectIterator It; It; ++It )
					if( It->IsA(Class) )
						Results.AddItem( *It );
			}
			else if( !ParseObject<UPackage>( Item, TEXT("GROUP="), RealPackage, BasePackage ) )
			{
				// All objects beneath BasePackage.
				for( FObjectIterator It; It; ++It )
					if( It->IsA(Class) && It->IsIn(BasePackage) )
						Results.AddItem( *It );
			}
			else
			{
				// All objects within RealPackage.
				for( FObjectIterator It; It; ++It )
					if( It->IsA(Class) && It->IsIn(RealPackage) )
						Results.AddItem( *It );
			}
			for( INT i=0; i<Results.Num(); i++ )
			{
				if( i )
					Ar.Log( TEXT(" ") );
				Ar.Log( Results(i)->GetName() );
			}
		}
	}
	else if( ParseCommand(&Item,TEXT("PACKAGES")) )
	{
		UClass* Class;
		if( ParseObject<UClass>(Item,TEXT("CLASS="),Class,ANY_PACKAGE) )
		{
			TArray<UObject*> List;
			for( FObjectIterator It; It; ++It )
			{
				if( It->IsA(Class) && It->GetOuter()!=UObject::GetTransientPackage() )
				{
					check(It->GetOuter());
					UObject* TopParent;
					for( TopParent=It->GetOuter(); TopParent->GetOuter()!=NULL; TopParent=TopParent->GetOuter() );
					if( Cast<UPackage>(TopParent) )
						List.AddUniqueItem( TopParent );
				}
			}
			for( INT i=0; i<List.Num(); i++ )
			{
				if( i )
					Ar.Log( TEXT(",") );
				Ar.Log( List(i)->GetName() );
			}
		}
	}
	else if( ParseCommand(&Item,TEXT("DELETE")) )
	{
		UPackage* Pkg=ANY_PACKAGE;
		UClass*   Class;
		UObject*  Object;
		ParseObject<UPackage>( Item, TEXT("PACKAGE="), Pkg, NULL );
		if( !ParseObject<UClass>( Item,TEXT("CLASS="), Class, ANY_PACKAGE )
				|| !ParseObject(Item,TEXT("OBJECT="),Class,Object,Pkg) )
			Ar.Logf( TEXT("Object not found") );
		else
			if( UObject::IsReferenced( Object, RF_Native | RF_Public, 0 ) )
				Ar.Logf( TEXT("%s is in use"), Object->GetFullName() );
			else
			{
				guard(MarkPackageAsDirty);
				UObject* Pkg = Object;
				while( Pkg->GetOuter() )
					Pkg = Pkg->GetOuter();
				if( Pkg && Pkg->IsA(UPackage::StaticClass() ) )
					((UPackage*)Pkg)->bDirty = 1;
				unguard;
				
				delete Object;
			}
	}
	else if( ParseCommand(&Item,TEXT("GROUPS")) )
	{
		UClass* Class;
		UPackage* Pkg;
		if
		(	ParseObject<UPackage>(Item,TEXT("PACKAGE="),Pkg,NULL)
		&&	ParseObject<UClass>(Item,TEXT("CLASS="),Class,ANY_PACKAGE) )
		{
			TArray<UObject*> List;
			for( FObjectIterator It; It; ++It )
				if( It->IsA(Class) && It->GetOuter() && It->GetOuter()->GetOuter()==Pkg )
					List.AddUniqueItem( It->GetOuter() );
			for( INT i=0; i<List.Num(); i++ )
			{
				if( i )
					Ar.Log( TEXT(",") );
				Ar.Log( List(i)->GetName() );
			}
		}
	}
	else if( ParseCommand(&Item,TEXT("BROWSECLASS")) )
	{
		Ar.Log( GUnrealEd->BrowseClass->GetName() );
	}
	unguard;
}
void ObjTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Data )
{
	guard(ObjTopicHandler::Set);
	if( ParseCommand(&Item,TEXT("NOTECURRENT")) )
	{
		UClass* Class;
		UObject* Object;
		if
		(	GUnrealEd->UseDest
		&&	ParseObject<UClass>( Data, TEXT("CLASS="), Class, ANY_PACKAGE )
		&&	ParseObject( Data, TEXT("OBJECT="), Class, Object, ANY_PACKAGE ) )
		{
			TCHAR Temp[256];
			appSprintf( Temp, TEXT("%s'%s'"), Object->GetClass()->GetName(), Object->GetPathName() );
			GUnrealEd->UseDest->SetValue( Temp );
		}
	}
	unguard;
}
void UUnrealEdEngine::NotifyExec( void* Src, const TCHAR* Cmd )
{
	guard(UUnrealEdEngine::NotifyExec);
	if( ParseCommand(&Cmd,TEXT("BROWSECLASS")) )
	{
		ParseObject( Cmd, TEXT("CLASS="), BrowseClass, ANY_PACKAGE );
		UseDest = (WProperties*)Src;
		EdCallback( EDC_Browse, 1, 0 );
	}
	else if( ParseCommand(&Cmd,TEXT("USECURRENT")) )
	{
		ParseObject( Cmd, TEXT("CLASS="), BrowseClass, ANY_PACKAGE );
		UseDest = (WProperties*)Src;
		EdCallback( EDC_UseCurrent, 1, 0 );
	}
	else if( ParseCommand(&Cmd,TEXT("USECOLOR")) )
	{
		UseDest = (WProperties*)Src;
		edcamSetMode( EM_EyeDropper );
	}
	else if( ParseCommand(&Cmd,TEXT("FINDACTOR")) )
	{
		UseDest = (WProperties*)Src;
		edcamSetMode( EM_FindActor );
	}
	else if( ParseCommand(&Cmd,TEXT("NEWOBJECT")) )
	{
		FTreeItem* Dest = (FTreeItem*)Src;
		UClass* Cls;
		ParseObject( Cmd, TEXT("CLASS="), Cls, ANY_PACKAGE );
		UObject* Outer;
		ParseObject( Cmd, TEXT("OUTER="), Outer, NULL );
		if( Cls && Outer )
		{
			UObject* NewObject = StaticConstructObject( Cls, Outer, NAME_None, RF_Public|((Cls->ClassFlags&CLASS_Localized) ? RF_PerObjectLocalized : 0) );
			if( NewObject )
				Dest->SetValue( NewObject->GetPathName() );
		}
	}
	unguard;
}
void UUnrealEdEngine::UpdatePropertiesWindows()
{
	guard(UUnrealEdEngine::UpdatePropertiesWindow);
	if( ActorProperties )
	{
		TArray<UObject*> SelectedActors;
		for( INT i=0; i<Level->Actors.Num(); i++ )
			if( Level->Actors(i) && Level->Actors(i)->bSelected )
				SelectedActors.AddItem( Level->Actors(i) );
		ActorProperties->Root.SetObjects( &SelectedActors(0), SelectedActors.Num() );
	}
	for( INT i=0; i<WProperties::PropertiesWindows.Num(); i++ )
	{
		WProperties* Properties=WProperties::PropertiesWindows(i);
		if( Properties!=ActorProperties && Properties!=Preferences )
			Properties->ForceRefresh();
	}
	unguard;
}


/*-----------------------------------------------------------------------------
	UnrealEd 
-----------------------------------------------------------------------------*/

void UUnrealEdEngine::EdCallback( DWORD Code, UBOOL Send, DWORD lParam )
{
	guard(UUnrealEdEngine::EdCallback);

	if( hWndMain )
	{
		int Msg = 0;

		switch( Code )
		{
			case EDC_Browse:					Msg = WM_EDC_BROWSE;	break;
			case EDC_UseCurrent:				Msg = WM_EDC_USECURRENT;	break;
			case EDC_CurTexChange:				Msg = WM_EDC_CURTEXCHANGE;	break;
			case EDC_CurStaticMeshChange:		Msg = WM_EDC_CURSTATICMESHCHANGE;	break;
			case EDC_SelPolyChange:				Msg = WM_EDC_SELPOLYCHANGE;	break;
			case EDC_SelChange:					Msg = WM_EDC_SELCHANGE;	break;
 			case EDC_RtClickTexture:			Msg = WM_EDC_RTCLICKTEXTURE;	break;
			case EDC_RtClickAnimSeq:			Msg = WM_EDC_RTCLICKANIMSEQ;	break;
			case EDC_RtClickMatScene:			Msg = WM_EDC_RTCLICKMATSCENE;	break;
			case EDC_RtClickMatAction:			Msg = WM_EDC_RTCLICKMATACTION;	break;
			case EDC_MaterialTreeClick:			Msg = WM_EDC_MATERIALTREECLICK;	break;
			case EDC_RtClickStaticMesh:			Msg = WM_EDC_RTCLICKSTATICMESH;	break;
			case EDC_RtClickPoly:				Msg = WM_EDC_RTCLICKPOLY;	break;
			case EDC_RtClickActor:				Msg = WM_EDC_RTCLICKACTOR;	break;
			case EDC_RtClickActorStaticMesh:	Msg = WM_EDC_RTCLICKACTORSTATICMESH;	break;
			case EDC_RtClickWindow:				Msg = WM_EDC_RTCLICKWINDOW;	break;
			case EDC_RtClickWindowCanAdd:		Msg = WM_EDC_RTCLICKWINDOWCANADD;	break;
			case EDC_MapChange:					Msg = WM_EDC_MAPCHANGE;	break;
			case EDC_ViewportUpdateWindowFrame:	Msg = WM_EDC_VIEWPORTUPDATEWINDOWFRAME;	break;
			case EDC_SurfProps:					Msg = WM_EDC_SURFPROPS;	break;
			case EDC_SaveMap:					Msg = WM_EDC_SAVEMAP;	break;
			case EDC_SaveMapAs:					Msg = WM_EDC_SAVEMAPAS;	break;
			case EDC_LoadMap:					Msg = WM_EDC_LOADMAP;	break;
			case EDC_PlayMap:					Msg = WM_EDC_PLAYMAP;	break;
			case EDC_CamModeChange:				Msg = WM_EDC_CAMMODECHANGE;	break;
			case EDC_RedrawAllViewports:		Msg = WM_REDRAWALLVIEWPORTS;	break;
//			case EDC_ViewportsDisableRealtime:	Msg = WM_EDC_VIEWPORTSDISABLEREALTIME;	break;
			case EDC_ActorPropertiesChange:		Msg = WM_EDC_ACTORPROPERTIESCHANGE;	break;
			case EDC_RtClickTerrainLayer:		Msg = WM_EDC_RTCLICKTERRAINLAYER;	break;
			case EDC_RefreshEditor:				Msg = WM_EDC_REFRESHEDITOR;	break;
			case EDC_DisplayLoadErrors:			Msg = WM_EDC_DISPLAYLOADERRORS;	break;
			case EDC_RedrawCurrentViewport:		Msg = WM_REDRAWCURRENTVIEWPORT;	break;

#ifdef WITH_LIPSINC
			case EDC_LIPSincAnimListRtClick:	Msg	= WM_EDC_LIPSINCANIMLISTRTCLICK; break;
#endif
		}
		if( Msg )
		{
			if( Send ) SendMessageX( hWndMain, WM_COMMAND, Msg, lParam );
			else       PostMessageX( hWndMain, WM_COMMAND, Msg, lParam );
		}
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	Hook replacements
-----------------------------------------------------------------------------*/

void UUnrealEdEngine::ShowPreferences()
{
	guard(UUnrealEdEngine::ShowPreferences);
	if( !Preferences )
	{
		Preferences = new WConfigProperties( TEXT("Preferences"), LocalizeGeneral(TEXT("AdvancedOptionsTitle"),TEXT("Window")) );
		Preferences->OpenWindow( hWndMain );
		Preferences->SetNotifyHook( this );
		Preferences->ForceRefresh();
	}
	Preferences->Show(1);
	unguard;
}
void UUnrealEdEngine::ShowActorProperties()
{
	guard(UUnrealEdEngine::ShowActorProperties);
	if( !ActorProperties )
	{
		ActorProperties = new WObjectProperties( TEXT("ActorProperties"), CPF_Edit, TEXT(""), NULL, 1 );
		ActorProperties->OpenWindow( hWndMain );
		ActorProperties->SetNotifyHook( this );
	}
	UpdatePropertiesWindows();
	ActorProperties->Show(1);
	unguard;
}

void UUnrealEdEngine::ShowLevelProperties()
{
	guard(UUnrealEdEngine::ShowLevelProperties);
	if( !LevelProperties )
	{
		LevelProperties = new WObjectProperties( TEXT("LevelProperties"), CPF_Edit, TEXT("Level Properties"), NULL, 1 );
		LevelProperties->OpenWindow( hWndMain );
		LevelProperties->SetNotifyHook( this );
	}
	LevelProperties->Root.SetObjects( (UObject**)&Level->Actors(0), 1 );
	LevelProperties->Show(1);
	unguard;
}

void UUnrealEdEngine::PlayMap()
{
	guard(UUnrealEdEngine::PlayMap);
	DisableRealtimeViewports();
	TCHAR Parms[256];
	Exec( *FString::Printf(TEXT("MAP SAVE FILE=..\\Maps\\Autoplay.%s"), *GMapExt), *GLog );
	appSprintf( Parms, TEXT("Autoplay.%s?NumBots=0 HWND=%i %s"), *GMapExt, (INT)hWndMain, GameCommandLine ); // gam
	FString EXEName;
	if( !GConfig->GetString( TEXT("URL"), TEXT("EXEName"), EXEName, TEXT("default.ini") ) )
		EXEName = TEXT("UT2004.exe");
	appLaunchURL( *EXEName, Parms );
	unguard;
}

void UUnrealEdEngine::DisableRealtimeViewports()
{
	guard(UUnrealEdEngine::DisableRealtimeViewports);

	// Loop through all viewports and disable any realtime viewports before running the game.
	for( INT x = 0 ; x < GViewports.Num() ; ++x)
	{
		GViewports(x).ViewportFrame->Viewport->Actor->ShowFlags &= ~SHOW_PlayerCtrl;
		InvalidateRect( GViewports(x).ViewportFrame->hWnd, NULL, 1 );
	}
		
	RedrawAllViewports( 1 );
	unguard;
}

void UUnrealEdEngine::ShowClassProperties( UClass* Class )
{
	guard(UUnrealEdEngine::ShowClassProperties);
	WClassProperties* ClassProperties = new WClassProperties( TEXT("ClassProperties"), CPF_Edit, *FString::Printf(TEXT("Default %s Properties"),Class->GetPathName()), Class );
	ClassProperties->OpenWindow( hWndMain );
	ClassProperties->SetNotifyHook( this );
	ClassProperties->ForceRefresh();
	ClassProperties->Show(1);
	unguard;
}

// gam ---
void UUnrealEdEngine::ShowSoundProperties( USound* Sound )
{
	guard(UUnrealEdEngine::ShowSoundProperties);
	TCHAR Title[256];
	appSprintf( Title, TEXT("Sound %s"), Sound->GetPathName() );
	WObjectProperties* SoundProperties = new WObjectProperties( TEXT("SoundProperties"), CPF_Edit, Title, NULL, 1 );
	SoundProperties->OpenWindow( (HWND)hWndMain );
	SoundProperties->Root.SetObjects( (UObject**)&Sound, 1 );
	SoundProperties->SetNotifyHook( this );
    SoundProperties->ExpandAll();
	SoundProperties->Show(1);
	unguard;
}
// --- gam

/*-----------------------------------------------------------------------------
	The end.
-----------------------------------------------------------------------------*/

