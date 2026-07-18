/*=============================================================================
	UnEditor.cpp: Unreal editor main file
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EditorPrivate.h"
#include "UnRender.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

EDITOR_API class UEditorEngine* GEditor;

/*-----------------------------------------------------------------------------
	UEditorEngine.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UEditorEngine);

/*-----------------------------------------------------------------------------
	Init & Exit.
-----------------------------------------------------------------------------*/

//
// Construct the UEditorEngine class.
//
void UEditorEngine::StaticConstructor()
{
	guard(UEditorEngine::StaticConstructor);

	UArrayProperty* A = new(GetClass(),TEXT("EditPackages"),RF_Public)UArrayProperty( CPP_PROPERTY(EditPackages), TEXT("Advanced"), CPF_Config );
	A->Inner = new(A,TEXT("StrProperty0"),RF_Public)UStrProperty;
	
	UArrayProperty* B = new(GetClass(),TEXT("CutdownPackages"),RF_Public)UArrayProperty( CPP_PROPERTY(CutdownPackages), TEXT("Advanced"), CPF_Config );
	B->Inner = new(B,TEXT("StrProperty1"),RF_Public)UStrProperty;

	unguard;
}

//
// Construct the editor.
//
UEditorEngine::UEditorEngine()
:	EditPackages( E_NoInit ),
	CutdownPackages( E_NoInit )
{}

//
// Editor early startup.
//
void UEditorEngine::InitEditor()
{
	guard(UEditorEngine::InitEditor);

	// Init names.
	#define NAMES_ONLY
	#define AUTOGENERATE_NAME(name) extern EDITOR_API FName EDITOR_##name; EDITOR_##name=FName(TEXT(#name),FNAME_Intrinsic);
	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "EditorClasses.h"
	#undef DECLARE_NAME
	#undef NAMES_ONLY

	// Call base.
	UEngine::Init();
	InitAudio();

	// Topics.
	GTopics.Init();

	// Make sure properties match up.
	VERIFY_CLASS_OFFSET(A,Actor,Owner);
	VERIFY_CLASS_OFFSET(A,PlayerController,Player);

	// Allocate temporary model.
	TempModel = new UModel( NULL, 1 );

	// Settings.
	Mode			= EM_None;
	MovementSpeed	= 4.0;
	FastRebuild		= 0;
	Bootstrapping	= 0;

	unguard;
}

//
// Init the editor.
//
void UEditorEngine::Init()
{
	guard(UEditorEngine::Init);

	// Init editor.
	GEditor = this;
	InitEditor();

	// Init transactioning.
	Trans = CreateTrans();

	// Load classes for editing.
	BeginLoad();
	for( INT i=0; i<EditPackages.Num(); i++ )
		if( !LoadPackage( NULL, *EditPackages(i), LOAD_NoWarn ) )
				appErrorf( TEXT("Can't find edit package '%s'"), *EditPackages(i) );
	EndLoad();

	// Init the client.
	UClass* ClientClass = StaticLoadClass( UClient::StaticClass(), NULL, TEXT("ini:Engine.Engine.ViewportManager"), NULL, LOAD_NoFail, NULL );
	Client = (UClient*)StaticConstructObject( ClientClass );
	Client->Init( this );
	check(Client);

	// Checks.
	VERIFY_CLASS_OFFSET(U,EditorEngine,ParentContext);
	//!!if( sizeof(*this) !=GetClass()->GetPropertiesSize() )
	//	appErrorf( "Editor size mismatch: C++ %i / UnrealScript %i", sizeof(*this), GetClass()->GetPropertiesSize() );
	//!!check(sizeof(*this)==GetClass()->GetPropertiesSize());

	// Init Render Device
	UClass* RenDevClass = StaticLoadClass( URenderDevice::StaticClass(), NULL, TEXT("D3DDrv.D3DRenderDevice"), NULL, LOAD_NoFail, NULL );
	GRenDev = ConstructObject<URenderDevice>( RenDevClass );
	GRenDev->Init();

	// Info.
	UPackage* LevelPkg = CreatePackage( NULL, TEXT("MyLevel") );
	Level = new( LevelPkg, TEXT("MyLevel") )ULevel( this, 0 );

	// Objects.
	Cylinder = new UPrimitive;
	Results  = new( GetTransientPackage(), TEXT("Results") )UTextBuffer;

	// Purge garbage.
	Cleanse( 0, TEXT("startup") );

	// Subsystem init messsage.
	debugf( NAME_Init, TEXT("Editor engine initialized") );

	unguard;
};
void UEditorEngine::Destroy()
{
	guard(UEditorEngine::Destroy);

	// Shut down transaction tracking system.
	if( Trans )
	{
		if( GUndo )
			debugf( NAME_Warning, TEXT("Warning: A transaction is active") );
		Trans->Reset( TEXT("shutdown") );
	}

	// Topics.
	GTopics.Exit();
	Level = NULL;

	// Remove editor array from root.
	debugf( NAME_Exit, TEXT("Editor shut down") );

	Super::Destroy();
	unguard;
}
void UEditorEngine::Serialize( FArchive& Ar )
{
	guard(UEditorEngine::Serialize);
	Super::Serialize(Ar);
	Ar << Tools;
	unguard;
}
void UEditorEngine::RedrawLevel( ULevel* Level )
{
	guard(UEditorEngine::RedrawLevel);
	if( Client && !ParentContext )
		for( INT i=0; i<Client->Viewports.Num(); i++ )
			if( (Client->Viewports(i)->IsOrtho() || Client->Viewports(i)->IsPerspective()) )
			{
					//&& (Client->Viewports(i)->Actor->GetLevel()==Level || Level==NULL) )
				Client->Viewports(i)->Repaint( 1 );
			}
	unguard;
}
void UEditorEngine::ResetSound()
{
	guard(UEditorEngine::ResetSound);
#if 0
	if( Audio )

		for( int i=0; i<Client->Viewports.Num(); i++ )
			if( !Client->Viewports(i)->IsOrtho() )
				Audio->SetViewport( Client->Viewports(i) );
#endif
	unguard;
}

/*-----------------------------------------------------------------------------
	Tick.
-----------------------------------------------------------------------------*/

//
// Time passes...
//
void UEditorEngine::Tick( float DeltaSeconds )
{
	guard(UEditorEngine::Tick);

	// Update subsystems.
	StaticTick();				
	GCache.Tick();

	// Find active realtime camera.
	UViewport* RealtimeViewport = NULL;
	for( INT i=0; i<Client->Viewports.Num(); i++ )
	{
		UViewport* Viewport = Client->Viewports(i);
		if( Viewport->Current && Viewport->IsRealtime() )
			RealtimeViewport = Viewport;
	}

	// Update the level.
	if( Level )
		Level->Tick( RealtimeViewport ? LEVELTICK_ViewportsOnly : LEVELTICK_TimeOnly, DeltaSeconds );

	// Update audio.
	if( Audio )
	{
		clock(GStats.DWORDStats(GEngineStats.STATS_Game_AudioTickCycles));

		UViewport* AudioViewport = NULL;
		for( INT vp = 0 ; vp < dED_MAX_VIEWPORTS && !AudioViewport ; vp++ )
		{
			AudioViewport = FindObject<UViewport>( ANY_PACKAGE, *FString::Printf(TEXT("U2Viewport%d"), vp) );

			// We don't want orthographic viewports
			if( AudioViewport && AudioViewport->IsOrtho() )
				AudioViewport = NULL;
		}

		if( AudioViewport )
		{
			FCameraSceneNode SceneNode(
				AudioViewport,
				&AudioViewport->RenderTarget,
				AudioViewport->Actor,
				AudioViewport->Actor->Location,
				AudioViewport->Actor->Rotation,
				90
			);

			Audio->Update( &SceneNode );
		}
		unclock(GStats.DWORDStats(GEngineStats.STATS_Game_AudioTickCycles));
	}

	// Render everything.
	if( Client )
		Client->Tick();

	unguard;
}

/*-----------------------------------------------------------------------------
	Garbage collection.
-----------------------------------------------------------------------------*/

//
// Clean up after a major event like loading a file.
//
void UEditorEngine::Cleanse( UBOOL Redraw, const TCHAR* TransReset )
{
	guard(UEditorEngine::Cleanse);
	check(TransReset);
	if( GIsRunning && !Bootstrapping )
	{
		// Collect garbage.
		CollectGarbage( RF_Native | RF_Standalone );

		// Reset the transaction tracking system if desired.
		Trans->Reset( TransReset );

		// Flush the cache.
		Flush(0);

		// Redraw the levels.
		if( Redraw )
			RedrawLevel( Level );
	}
	unguard;
}

/*---------------------------------------------------------------------------------------
	Topics.
---------------------------------------------------------------------------------------*/
void UEditorEngine::Get( const TCHAR* Topic, const TCHAR* Item, FOutputDevice& Ar )
{
	guard(UEditorEngine::Get);
	GTopics.Get( Level, Topic, Item, Ar );
	unguard;
}
void UEditorEngine::Set( const TCHAR* Topic, const TCHAR* Item, const TCHAR* Value )
{
	guard(UEditorEngine::Set);
	GTopics.Set( Level, Topic, Item, Value );
	unguard;
}

/*---------------------------------------------------------------------------------------
	Link topics.
---------------------------------------------------------------------------------------*/

// Enum.
AUTOREGISTER_TOPIC(TEXT("Enum"),EnumTopicHandler);
void EnumTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	guard(EnumTopicHandler::Get);

	UEnum* Enum = FindObject<UEnum>( ANY_PACKAGE, Item );
	if( Enum )
	{
		for( int i=0; i<Enum->Names.Num(); i++ )
		{
			if( i > 0 )
				Ar.Logf(TEXT(","));
			Ar.Logf( TEXT("%i - %s"), i, *Enum->Names(i) );
		}
	}
	unguard;
}
void EnumTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Value )
{}

// Sound.
AUTOREGISTER_TOPIC(TEXT("Sound"),SoundTopicHandler);
void SoundTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	guard(SoundTopicHandler::Get);
	if( ParseCommand(&Item,TEXT("FILETYPE")) )
	{
		TCHAR Name[NAME_SIZE];
		UPackage* Package=ANY_PACKAGE;
		ParseObject<UPackage>( Item, TEXT("PACKAGE="), Package, NULL );
		if( Parse( Item, TEXT("NAME="), Name, ARRAY_COUNT(Name) ) )
		{
			USound* Sound = FindObject<USound>( Package, Name );
			if( Sound )
				Ar.Log( *Sound->GetFileType() ); // gam
		}
	}
	unguard;
}
void SoundTopicHandler::Set(ULevel *Level, const TCHAR* Item, const TCHAR* Data)
{}

// Text.
AUTOREGISTER_TOPIC(TEXT("Text"),TextTopicHandler);
void TextTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	guard(TextTopicHandler::Get);
	UTextBuffer* Text = FindObject<UTextBuffer>( ANY_PACKAGE, Item );
	if( Text && Text->Text.Len() )
		Ar.Log( *Text->Text );
	unguard;
}
void TextTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Data )
{
	guard(TextTopicHandler::Set);
	UTextBuffer* Text = FindObject<UTextBuffer>( ANY_PACKAGE, Item );
	if( Text )
	{
		Text->SetFlags( RF_SourceModified );
		Text->Text.Empty();
		Text->Log( Data );
	}
	unguard;
}

// Script.
AUTOREGISTER_TOPIC(TEXT("Script"),ScriptTopicHandler);
void ScriptTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	guard(ScriptTopicHandler::Get);
	UClass* Class = FindObject<UClass>( ANY_PACKAGE, Item );
	UTextBuffer* Text = Class ? Class->ScriptText : NULL;
	if( Text && Text->Text.Len() )
		Ar.Log( *Text->Text );
	unguard;
}
void ScriptTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Data )
{
	guard(ScriptTopicHandler::Set);
	UClass* Class = FindObject<UClass>( ANY_PACKAGE, Item );
	if( Class && Class->ScriptText )
	{
		if( appStrcmp( *Class->ScriptText->Text, Data ) )
		{
			Class->ScriptText->Text = Data;
			Class->SetFlags( RF_SourceModified );
		}
	}
	unguard;
}

// ScriptPos.
AUTOREGISTER_TOPIC(TEXT("ScriptPos"),ScriptPosTopicHandler);
void ScriptPosTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	guard(ScriptPosTopicHandler::Get);
	UClass* Class = FindObject<UClass>( ANY_PACKAGE, Item );
	UTextBuffer* Text = Class ? Class->ScriptText : NULL;
	if( Text )
		Ar.Logf( TEXT("%i"), Text->Pos );
	unguard;
}
void ScriptPosTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Data )
{
	guard(ScriptPosTopicHandler::Set);
	UClass* Class = FindObject<UClass>( ANY_PACKAGE, Item );
	UTextBuffer* Text = Class ? Class->ScriptText : NULL;
	if( Text ) Text->Pos = appAtoi(Data);
	unguard;
}


// ScriptTop.
AUTOREGISTER_TOPIC(TEXT("ScriptTop"),ScriptTopTopicHandler);
void ScriptTopTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	guard(ScriptTopTopicHandler::Get);
	UClass* Class = FindObject<UClass>( ANY_PACKAGE, Item );
	UTextBuffer* Text = Class ? Class->ScriptText : NULL;
	if( Text ) Ar.Logf(TEXT("%i"),Text->Top);
	unguard;
}
void ScriptTopTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Data )
{
	guard(ScriptTopTopicHandler::Set);
	UClass* Class = FindObject<UClass>( ANY_PACKAGE, Item );
	UTextBuffer* Text = Class ? Class->ScriptText : NULL;
	if( Text ) Text->Top = appAtoi(Data);
	unguard;
}

// Class.
static int CDECL ClassSortCompare( const void *elem1, const void *elem2 )
{
	return appStricmp((*(UClass**)elem1)->GetName(),(*(UClass**)elem2)->GetName());
}
AUTOREGISTER_TOPIC( TEXT("Class"), ClassTopicHandler );
void ClassTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	guard(ClassTopicHandler::Get);
	enum	{MAX_RESULTS=1024};
	int		NumResults = 0;
	UClass	*Results[MAX_RESULTS];

	if( ParseCommand(&Item,TEXT("PACKAGE")) )
	{
		UClass* Class = NULL;
		if( ParseObject<UClass>(Item,TEXT("CLASS="),Class,ANY_PACKAGE) )
			Ar.Log( Class->GetOuter()->GetName() );
	}
	else if( ParseCommand(&Item,TEXT("QUERY")) )
	{
		UClass *Parent = NULL;
		ParseObject<UClass>(Item,TEXT("PARENT="),Parent,ANY_PACKAGE);

		// Make a list of all child classes.
		for( TObjectIterator<UClass> It; It && NumResults<MAX_RESULTS; ++It )
			if( It->GetSuperClass()==Parent )
				Results[NumResults++] = *It;

		// Sort them by name.
		appQsort( Results, NumResults, sizeof(UClass*), ClassSortCompare );

		// Return the results.
		for( INT i=0; i<NumResults; i++ )
		{
			// See if this item has children.
			INT Children = 0;
			for( TObjectIterator<UClass> It; It; ++It )
				if( It->GetSuperClass()==Results[i] )
					Children++;

			// Add to result string.
			if( i>0 ) Ar.Log(TEXT(","));
			Ar.Logf
			(
				TEXT("%s%s|%s"),
				(	Results[i]->GetOuter()->GetFName()==NAME_Engine
				||	Results[i]->GetOuter()->GetFName()==NAME_Core) ? TEXT("*") : TEXT(""),
				Results[i]->GetName(),
				Children ? TEXT("C") : TEXT("X")
			);
		}
	}
	if( ParseCommand(&Item,TEXT("GETCHILDREN")) )
	{
		UClass *Parent = NULL;
		ParseObject<UClass>(Item,TEXT("CLASS="),Parent,ANY_PACKAGE);
		UBOOL Concrete=0; ParseUBOOL( Item, TEXT("CONCRETE="), Concrete );

		// Make a list of all child classes.
		for( TObjectIterator<UClass> It; It && NumResults<MAX_RESULTS; ++It )
			if( It->IsChildOf(Parent) && (!Concrete || !(It->ClassFlags & CLASS_Abstract)) )
				Results[NumResults++] = *It;

		// Sort them by name.
		appQsort( Results, NumResults, sizeof(UClass*), ClassSortCompare );

		// Return the results.
		for( int i=0; i<NumResults; i++ )
		{
			if( i>0 )
				Ar.Log( TEXT(" ") );
			Ar.Log( Results[i]->GetName() );
		}
	}
	else if( ParseCommand(&Item,TEXT("EXISTS")) )
	{
		UClass* Class;
		if (ParseObject<UClass>(Item,TEXT("NAME="),Class,ANY_PACKAGE)) Ar.Log(TEXT("1"));
		else Ar.Log(TEXT("0"));
	}
	else if( ParseCommand(&Item,TEXT("PACKAGE")) )
	{
		UClass *Class;
		if( ParseObject<UClass>( Item, TEXT("CLASS="), Class, ANY_PACKAGE ) )
			Ar.Log( Class->GetOuter()->GetName() );
	}
	unguard;
}
void ClassTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Data )
{}

// Actor.
AUTOREGISTER_TOPIC(TEXT("Actor"),ActorTopicHandler);
void ActorTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	guard(ActorTopicHandler::Get);

	// Summarize the level actors.
	int		 n			= 0;
	INT	    AnyClass	= 0;
	UClass*	AllClass	= NULL;
	for( int i=0; i<Level->Actors.Num(); i++ )
	{
		if( Level->Actors(i) && Level->Actors(i)->bSelected )
		{
			if( AnyClass && Level->Actors(i)->GetClass()!=AllClass ) 
				AllClass = NULL;
			else 
				AllClass = Level->Actors(i)->GetClass();
			AnyClass=1;
			n++;
		}
	}
	if( !appStricmp(Item,TEXT("NumSelected")) )
	{
		Ar.Logf( TEXT("%i"), n );
	}
	else if( !appStricmp(Item,TEXT("ClassSelected")) )
	{
		if( AnyClass && AllClass )
			Ar.Logf( TEXT("%s"), AllClass->GetName() );
	}
	else if( !appStrnicmp(Item,TEXT("IsKindOf"),8) )
	{
		// Sees if the one selected actor belongs to a class.
		UClass *Class;
		Ar.Logf( TEXT("%i"), ParseObject<UClass>(Item,TEXT("CLASS="),Class,ANY_PACKAGE) && AllClass && AllClass->IsChildOf(Class) );
	}
	unguard;
}
void ActorTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Data )
{}

// Lev.
AUTOREGISTER_TOPIC(TEXT("Lev"),LevTopicHandler);
void LevTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	guard(LevTopicHandler::Get);

    // gam ---
	if( ParseCommand(&Item,TEXT("MATCHES_INT")) )
    {
        FString PackageName;

        if (!Parse( Item, TEXT("PACKAGE="), PackageName ))
        {
            debugf (NAME_Error, TEXT("Can't GET MATCHES_INT without PACKAGE"));
            return;
        }

        if( IntMatchesPackage( *PackageName ))
            Ar.Logf(TEXT("TRUE"));
        else
            Ar.Logf(TEXT("FALSE"));
    }
    // --- gam
    else
    {
	INT ItemNum = appAtoi( Item );
	if( ItemNum>=0 && ItemNum<ULevel::NUM_LEVEL_TEXT_BLOCKS && Level->TextBlocks[ItemNum] )
		Ar.Log( *Level->TextBlocks[ItemNum]->Text );
    }

	unguard;
}
void LevTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Data )
{
	guard(LevTopicHandler::Set);

	if( !appIsDigit(Item[0]) )
		return; // Item isn't a number.

	int ItemNum = appAtoi( Item );
	if ((ItemNum < 0) || (ItemNum >= ULevel::NUM_LEVEL_TEXT_BLOCKS)) return; // Invalid text block number

	if( !Level->TextBlocks[ItemNum] )
		Level->TextBlocks[ItemNum] = new( Level->GetOuter(), NAME_None, RF_NotForClient|RF_NotForServer )UTextBuffer;
	
	Level->TextBlocks[ItemNum]->Text = Data;

	unguard;
}

// Mesh.
AUTOREGISTER_TOPIC(TEXT("Mesh"),MeshTopicHandler);
void MeshTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	guard(MeshTopicHandler::Get);

	if( !appStrnicmp(Item,TEXT("NUMANIMSEQS"),11) )
	{
		UMesh *Mesh;
		AActor *Actor;
		if( !ParseObject<AActor>(Item,TEXT("ACTOR="),Actor,ANY_PACKAGE) )
			Actor = NULL;
		if( ParseObject<UMesh>(Item,TEXT("NAME="),Mesh,ANY_PACKAGE) )
			Ar.Logf( TEXT("%i"), Mesh->MeshGetInstance(Actor)->GetAnimCount() );
	}
	else if( !appStrnicmp(Item,TEXT("ANIMSEQ"),7) )
	{
		UMesh *Mesh;
		INT   SeqNum;
		if( ParseObject<UMesh>(Item,TEXT("NAME="),Mesh,ANY_PACKAGE)
		&&	(Parse(Item,TEXT("NUM="),SeqNum)) )
		{
			AActor *Actor;
			if( !ParseObject<AActor>(Item,TEXT("ACTOR="),Actor,ANY_PACKAGE) )
				Actor = NULL;
			if( Mesh->MeshGetInstance(Actor)->GetAnimCount() )
			{
				HMeshAnim Seq = Mesh->MeshGetInstance(Actor)->GetAnimIndexed(SeqNum);
				FName SeqName = Mesh->MeshGetInstance(Actor)->AnimGetName(Seq);
				if( SeqName!=NAME_None )
				{
					Ar.Logf
					(
						TEXT("%s                                        %03i %03i"),
						*SeqName,
						SeqNum,
						(INT)Mesh->MeshGetInstance(Actor)->AnimGetFrameCount(Seq)
 					);
				}
			}
		}
	}
	unguard;
}
void MeshTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Data )
{
	guard(MeshTopicHandler::Set);
	unguard;
}

// Texture.
AUTOREGISTER_TOPIC(TEXT("Texture"),TextureTopicHandler);
void TextureTopicHandler::Get( ULevel* Level, const TCHAR* Item, FOutputDevice& Ar )
{
	guard(TextureTopicHandler::Get);
	UTexture* Texture;
	if( ParseCommand(&Item,TEXT("CurrentMaterial")) )
	{
		if( GEditor->CurrentMaterial )
			Ar.Log( GEditor->CurrentMaterial->GetPathName() );
	}
	else if( ParseObject<UTexture>(Item,TEXT("TEXTURE="),Texture,ANY_PACKAGE) )
	{
		if( ParseCommand(&Item,TEXT("PALETTE")) )
		{
			Ar.Logf( TEXT("%s"), Texture->Palette->GetPathName() );
		}
		else if( ParseCommand(&Item,TEXT("SIZE")) )
		{
			Ar.Logf( TEXT("%i,%i"), Texture->USize, Texture->VSize );
		}
	}
	unguard;
}
void TextureTopicHandler::Set( ULevel* Level, const TCHAR* Item, const TCHAR* Value )
{}

/*-----------------------------------------------------------------------------
	Object property porting.
-----------------------------------------------------------------------------*/
// gam ---
struct FDefinedProperty
{
    UProperty *Property;
    INT Index;

    bool operator== ( const FDefinedProperty& Other ) const
    {
        return( (Property == Other.Property) && (Index == Other.Index) );
    }
};
// --- gam

//
// Import text properties.
//
EDITOR_API const TCHAR* ImportProperties
(
	UClass*				ObjectClass,
	BYTE*				Object,
	ULevel*				Level,
	const TCHAR*		Data,
	UObject*			InParent,
	FFeedbackContext*	Warn,
	INT					Depth
)
{
	guard(ImportProperties);
	check(ObjectClass!=NULL);
	check(Object!=NULL);

	TCHAR StrLine[4096];
	// If bootstrapping, check the class we're BEGIN OBJECTing has had its properties imported.
	if( GEditor->Bootstrapping && Depth==0 )
	{
		const TCHAR* TempData = Data;
		while( ParseLine( &TempData, StrLine, ARRAY_COUNT(StrLine) ) )
		{
			const TCHAR* Str = StrLine;
			if( GetBEGIN(&Str,TEXT("Object")))
			{
				UClass* SubObjectClass;
				if(	ParseObject<UClass>(Str,TEXT("Class="),SubObjectClass,ANY_PACKAGE) )
				{
					if( (SubObjectClass->ClassFlags&CLASS_NeedsDefProps) )
						return NULL;
				}
			}
		}
	}

    // gam ---
    TArray<FDefinedProperty> DefinedProperties;
    // --- gam

	
	// Parse all objects stored in the actor.
	// Build list of all text properties.
	UBOOL ImportedBrush = 0;
	while( ParseLine( &Data, StrLine, ARRAY_COUNT(StrLine) ) )
	{
		const TCHAR* Str = StrLine;

		if( GetBEGIN(&Str,TEXT("Brush")) && ObjectClass->IsChildOf(ABrush::StaticClass()) )
		{
			// Parse brush on this line.
			guard(Brush);
			TCHAR BrushName[NAME_SIZE];
			if( Parse( Str, TEXT("Name="), BrushName, NAME_SIZE ) )
			{
				// If a brush with this name already exists in the
				// level, rename the existing one.  This is necessary
				// because we can't rename the brush we're importing without
				// losing our ability to associate it with the actor properties
				// that reference it.
				UModel* ExistingBrush = FindObject<UModel>( InParent, BrushName );
				if( ExistingBrush )
					ExistingBrush->Rename();

				// Create model.
				UModelFactory* ModelFactory = new UModelFactory;
				ModelFactory->FactoryCreateText( Level,UModel::StaticClass(), InParent, BrushName, 0, NULL, TEXT("t3d"), Data, Data+appStrlen(Data), GWarn );
				ImportedBrush = 1;
			}
			unguard;
		}
		else if( GetBEGIN(&Str,TEXT("StaticMesh")))
		{
			// Parse static mesh on this line.

			guard(StaticMesh);

			TCHAR	StaticMeshName[NAME_SIZE];

			if(Parse(Str,TEXT("Name="),StaticMeshName,NAME_SIZE))
			{
				// Rename any static meshes that have the desired name.

				UStaticMesh*	ExistingStaticMesh = FindObject<UStaticMesh>(InParent,StaticMeshName);

				if(ExistingStaticMesh)
					ExistingStaticMesh->Rename();

				// Parse the static mesh.

				UStaticMeshFactory*	StaticMeshFactory = new UStaticMeshFactory;

				StaticMeshFactory->FactoryCreateText(Level,UStaticMesh::StaticClass(),InParent,FName(StaticMeshName),0,NULL,TEXT("t3d"),Data,Data + appStrlen(Data),GWarn);

				delete StaticMeshFactory;
			}

			unguard;
		}
		else if( GetBEGIN(&Str,TEXT("Object")))
		{
			// Parse subobject default properties.
			// Note: default properties subobjects have compiled class as their Outer (used for localization).
			guard(SubObject);
			TCHAR	SubObjectName[NAME_SIZE];
			UClass*	SubObjectClass;
			UObject* SubObjectOuter = GEditor->Bootstrapping ? ObjectClass : InParent;
			if(	ParseObject<UClass>(Str,TEXT("Class="),SubObjectClass,ANY_PACKAGE) )
			{
				if( !(SubObjectClass->ClassFlags & CLASS_Compiled) )
					GWarn->Logf( NAME_Error, TEXT("BEGIN OBJECT: Can't create subobject as Class %s hasn't been compiled yet."), SubObjectClass->GetName() );

				UObject* NewSubObject;
				if( Parse(Str,TEXT("Name="),SubObjectName,NAME_SIZE) )
				{
					UObject* OldObject;
					if( (OldObject = FindObject<UObject>(SubObjectOuter,SubObjectName)) != NULL )
					{
						if( GEditor->Bootstrapping )
							GWarn->Logf( NAME_Error, TEXT("BEGIN OBJECT: name %s redefined."), SubObjectName );
						else
							OldObject->Rename();
					}
					NewSubObject = ConstructObject<UObject>( SubObjectClass, SubObjectOuter, SubObjectName, RF_Public|((SubObjectClass->ClassFlags&CLASS_Localized) ? RF_PerObjectLocalized : 0) );
				}
				else
					NewSubObject = ConstructObject<UObject>( SubObjectClass, SubObjectOuter, NAME_None, (SubObjectClass->ClassFlags&CLASS_Localized) ? RF_PerObjectLocalized : 0 );

				Data = ImportProperties( SubObjectClass, (BYTE*)NewSubObject, Level, Data, SubObjectOuter, Warn, Depth+1 );
			}

			unguard;
		}
		else if( GetEND(&Str,TEXT("Actor")) || GetEND(&Str,TEXT("DefaultProperties")) || (GetEND(&Str,TEXT("Object")) && Depth) )
		{
			// End of properties.
			break;
		}
		else
		{
			// Property.
			TCHAR Token[4096];
			while( *Str==' ' || *Str==9 )
				Str++;
			const TCHAR* Start=Str;
			while( *Str && *Str!='=' && *Str!='(' && *Str!='[' )
				Str++;
			if( *Str )
			{
				appStrncpy( Token, Start, Str-Start+1 );
				// strip trailing whitespace on token
				INT l = appStrlen(Token);
				while( l && (Token[l-1]==' ' || Token[l-1]==9) )
				{
					Token[l-1] = 0;
					--l;
				}
				INT Index=-1;

				// Process array subscript
				if( *Str=='(' || *Str=='[' )
				{
					Str++;
					Index = appAtoi(Str);
					while( *Str && *Str!=')' && *Str!=']' )
						Str++;
					if( !*Str++ )
					{
						Warn->Logf( NAME_ExecWarning, TEXT("%s: Missing ')' in default properties subscript: %s"), ObjectClass->GetPathName(), StrLine );
						continue;
					}
				}
				// strip whitespace before =
				while( *Str==' ' || *Str == 9 )
					Str++;
				if( *Str++!='=' )
				{
					Warn->Logf( NAME_ExecWarning, TEXT("%s: Missing '=' in default properties assignment: %s"), ObjectClass->GetPathName(), StrLine );
					continue;
				}
				// strip whitespace after =
				while( *Str==' ' || *Str == 9 )
					Str++;

				UProperty* Property = FindField<UProperty>( ObjectClass, Token );
				if( !Property )
				{
					// Check for a delegate property
					FString DelegateName = FString::Printf(TEXT("__%s__Delegate"), Token );
					Property = FindField<UDelegateProperty>( ObjectClass, *DelegateName );
                    if( !Property )
					{
						Warn->Logf( NAME_Error, TEXT("%s: Unknown property in defaults: %s"), ObjectClass->GetPathName(), StrLine ); // gam
						continue;
					}
				}
				if( ( Index>=Property->ArrayDim && !Property->IsA(UArrayProperty::StaticClass()) ) )
				{
					Warn->Logf( NAME_Error, TEXT("%s: Out of bound array default property (%i/%i)"), ObjectClass->GetPathName(), Index, Property->ArrayDim ); // gam
					continue;
				}

				FDefinedProperty D;
				D.Property = Property;
				D.Index = Index;
				if( DefinedProperties.FindItemIndex( D ) != INDEX_NONE )
				{
					Warn->Logf( NAME_Error, TEXT("redundant data: %s"), StrLine );
						continue;
				}
				DefinedProperties.AddItem( D );

				if( appStricmp(Property->GetName(),TEXT("Name"))!=0 )
				{
					l = appStrlen(Str);
					while( l && (Str[l-1]==';' || Str[l-1]==' ' || Str[l-1]==9) )
					{
						*(TCHAR*)(&Str[l-1]) = 0;
						--l;
					}
					if( Property->IsA(UStrProperty::StaticClass()) && (!l || *Str != '"' || Str[l-1] != '"') )
						Warn->Logf( NAME_Error, TEXT("%s: Missing '\"' in string default properties : %s"), ObjectClass->GetPathName(), StrLine );
			
					if (Index > -1 && Property->IsA(UArrayProperty::StaticClass())) //set single dynamic array element
					{
						FArray* Array=(FArray*)(Object + Property->Offset);
						UArrayProperty* ArrayProp = (UArrayProperty*)Property;
						if (Index>=Array->Num())
							Array->AddZeroed(ArrayProp->Inner->ElementSize,Index-Array->Num()+1);
						ArrayProp->Inner->ImportText( Str, (BYTE*)Array->GetData() + Index * ArrayProp->Inner->ElementSize, PPF_Delimited | PPF_CheckReferences );
					}
					else
					if( Property->IsA(UDelegateProperty::StaticClass()) )
					{
						if (Index == -1) Index = 0;
						FString Temp;
						if( appStrchr(Str, '.')==NULL && appStricmp(Str,TEXT("None")) )
						{
							Temp = Depth ? InParent->GetName() : ObjectClass->GetName();
							Temp = Temp + TEXT(".") + Str;
						}
						else
							Temp = Str;
						FScriptDelegate* D = (FScriptDelegate*)(Object + Property->Offset + Index*Property->ElementSize);
						D->Object = NULL;
						D->FunctionName = NAME_None;

						Property->ImportText( *Temp, Object + Property->Offset + Index*Property->ElementSize, PPF_Delimited | PPF_CheckReferences  );
						if( !D->Object || D->FunctionName==NAME_None )
						{
							if ( Temp != TEXT("None") )
                                Warn->Logf( NAME_Error, TEXT("%s: Delegate assignment failed: %s"), ObjectClass->GetPathName(), StrLine );
						}
					}
					else
					{
						if (Index == -1) Index = 0;
						Property->ImportText( Str, Object + Property->Offset + Index*Property->ElementSize, PPF_Delimited | PPF_CheckReferences  );
					}
				}
			}
		}
	}

	// Prepare brush.
	if( ImportedBrush && ObjectClass->IsChildOf(ABrush::StaticClass()) )
	{
		guard(PrepBrush);
		check(GIsEditor);
		ABrush* Actor = (ABrush*)Object;
		if( Actor->bStatic )
		{
			// Prepare static brush.
			Actor->SetFlags       ( RF_NotForClient | RF_NotForServer );
			Actor->Brush->SetFlags( RF_NotForClient | RF_NotForServer );
		}
		else
		{
			// Prepare moving brush.
			GEditor->csgPrepMovingBrush( Actor );
		}
		unguard;
	}
	return Data;
	unguard;
}


/*---------------------------------------------------------------------------------------
	Misc.
---------------------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UAnimNotifyProps);

/*---------------------------------------------------------------------------------------
	The End.
---------------------------------------------------------------------------------------*/

