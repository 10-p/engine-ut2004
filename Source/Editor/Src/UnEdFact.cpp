/*=============================================================================
	UnEdFact.cpp: Editor class factories.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EditorPrivate.h"
#include <stdio.h>
#include "../../Engine/Inc/UnDDraw.h" //!!

#if __UNIX__  // !!! FIXME: This is dumb. --ryan.
#define winAnsiToTCHAR ANSI_TO_TCHAR
#endif

/*------------------------------------------------------------------------------
	ULevelFactoryNew implementation.
------------------------------------------------------------------------------*/

void ULevelFactoryNew::StaticConstructor()
{
	guard(ULevelFactoryNew::StaticConstructor);

	SupportedClass			= ULevel::StaticClass();
	bCreateNew				= 1;
	bShowPropertySheet		= 1;
	bShowCategories			= 0;
	Description				= TEXT("Level");
	InContextCommand		= TEXT("Create New Level");
	OutOfContextCommand		= TEXT("Create New Level");
	CloseExistingWindows	= 1;
	LevelTitle				= TEXT("Untitled");
	Author					= TEXT("Unknown");

	new(GetClass(),TEXT("LevelTitle"),           RF_Public)UStrProperty(CPP_PROPERTY(LevelTitle          ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Author"),               RF_Public)UStrProperty(CPP_PROPERTY(Author              ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("CloseExistingWindows"), RF_Public)UBoolProperty(CPP_PROPERTY(CloseExistingWindows), TEXT(""), CPF_Edit );

	unguard;
}
ULevelFactoryNew::ULevelFactoryNew()
{
	guard(ULevelFactoryNew::ULevelFactoryNew);
	unguard;
}
void ULevelFactoryNew::Serialize( FArchive& Ar )
{
	guard(ULevelFactoryNew::Serialize);
	Super::Serialize( Ar );

	Ar << LevelTitle << Author << CloseExistingWindows;

	unguard;
}
UObject* ULevelFactoryNew::FactoryCreateNew( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, FFeedbackContext* Warn )
{
	guard(ULevelFactoryNew::FactoryCreateNew);

	//!!needs updating: optionally close existing windows, create NEW level, create new wlevelframe
	GEditor->Trans->Reset( TEXT("clearing map") );
	GEditor->Level->RememberActors();
	GEditor->Level = new( GEditor->Level->GetOuter(), TEXT("MyLevel") )ULevel( GEditor, 0 );
	GEditor->Level->GetLevelInfo()->Title  = LevelTitle;
	GEditor->Level->GetLevelInfo()->Author = Author;

	GEditor->Level->ReconcileActors();
	GEditor->ResetSound();
	GEditor->RedrawLevel( GEditor->Level );
	GEditor->NoteSelectionChange( GEditor->Level );
	GEditor->EdCallback(EDC_MapChange,0,1);
	GEditor->Cleanse( 1, TEXT("starting new map") );

	return GEditor->Level;
	unguard;
}
IMPLEMENT_CLASS(ULevelFactoryNew);

/*------------------------------------------------------------------------------
	UClassFactoryNew implementation.
------------------------------------------------------------------------------*/

void UClassFactoryNew::StaticConstructor()
{
	guard(UClassFactoryNew::StaticConstructor);

	SupportedClass		= UClass::StaticClass();
	bCreateNew			= 1;
	bShowPropertySheet	= 1;
	bShowCategories		= 0;
	Description			= TEXT("UnrealScript Class");
	InContextCommand	= TEXT("Create New Subclass");
	OutOfContextCommand	= TEXT("Create New Class");
	ClassName			= TEXT("MyClass");
	ClassPackage		= CreatePackage( NULL, TEXT("MyPackage") );
	Superclass			= AActor::StaticClass();
	new(GetClass(),TEXT("ClassName"),    RF_Public)UNameProperty  (CPP_PROPERTY(ClassName   ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("ClassPackage"), RF_Public)UObjectProperty(CPP_PROPERTY(ClassPackage), TEXT(""), CPF_Edit, UPackage::StaticClass() );
	new(GetClass(),TEXT("Superclass"),   RF_Public)UClassProperty (CPP_PROPERTY(Superclass  ), TEXT(""), CPF_Edit, UObject::StaticClass() );

	unguard;
}
UClassFactoryNew::UClassFactoryNew()
{
	guard(UClassFactoryNew::UClassFactoryNew);
	unguard;
}
void UClassFactoryNew::Serialize( FArchive& Ar )
{
	guard(UClassFactoryNew::Serialize);
	Super::Serialize( Ar );

	Ar << ClassName << ClassPackage << Superclass;

	unguard;
}
UObject* UClassFactoryNew::FactoryCreateNew( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, FFeedbackContext* Warn )
{
	guard(UClassFactoryNew::FactoryCreateNew);
	return NULL;
	unguard;
}
IMPLEMENT_CLASS(UClassFactoryNew);

/*------------------------------------------------------------------------------
	UTextureFactoryNew implementation.
------------------------------------------------------------------------------*/

void UTextureFactoryNew::StaticConstructor()
{
	guard(UTextureFactoryNew::StaticConstructor);

	SupportedClass		= UTexture::StaticClass();
	bCreateNew			= 1;
	bShowPropertySheet	= 1;
	bShowCategories		= 0;
	Description			= TEXT("Procedural Texture");
	InContextCommand	= TEXT("Create New Texture");
	OutOfContextCommand	= TEXT("Create New Texture");
	TextureName			= TEXT("MyTexture");
	TexturePackage		= CreatePackage( NULL, TEXT("MyPackage") );
	TextureClass		= UTexture::StaticClass();
	USize				= 256;
	VSize				= 256;

	new(GetClass(),TEXT("TextureName"),    RF_Public)UNameProperty  (CPP_PROPERTY(TextureName   ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("TexturePackage"), RF_Public)UObjectProperty(CPP_PROPERTY(TexturePackage), TEXT(""), CPF_Edit, UPackage::StaticClass() );
	new(GetClass(),TEXT("TextureClass"),   RF_Public)UClassProperty (CPP_PROPERTY(TextureClass  ), TEXT(""), CPF_Edit, UObject::StaticClass() );
	new(GetClass(),TEXT("USize"),          RF_Public)UIntProperty   (CPP_PROPERTY(USize         ), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("VSize"),          RF_Public)UIntProperty   (CPP_PROPERTY(VSize         ), TEXT(""), CPF_Edit );

	unguard;
}
UTextureFactoryNew::UTextureFactoryNew()
{
	guard(UTextureFactoryNew::UTextureFactoryNew);
	unguard;
}
void UTextureFactoryNew::Serialize( FArchive& Ar )
{
	guard(UTextureFactoryNew::Serialize);
	Super::Serialize( Ar );

	Ar << TextureName << TexturePackage << TextureClass << USize << VSize;

	unguard;
}
UObject* UTextureFactoryNew::FactoryCreateNew( UClass* Class, UObject* InParent, FName Name, DWORD Flags, UObject* Context, FFeedbackContext* Warn )
{
	guard(UTextureFactoryNew::FactoryCreateNew);
	return NULL;
	unguard;
}
IMPLEMENT_CLASS(UTextureFactoryNew);

/*------------------------------------------------------------------------------
	UClassFactoryUC implementation.
------------------------------------------------------------------------------*/

void UClassFactoryUC::StaticConstructor()
{
	guard(UClassFactoryUC::StaticConstructor);

	SupportedClass = UClass::StaticClass();
	new(Formats)FString(TEXT("uc;Unreal class definitions"));
	bCreateNew = 0;
	bText  = 1;
	bMulti = 1;

	unguard;
}
UClassFactoryUC::UClassFactoryUC()
{
	guard(UClassFactoryUC::UClassFactoryUC);
	unguard;
}
UObject* UClassFactoryUC::FactoryCreateText
(
	ULevel*				InLevel,
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(UClassFactoryUC::FactoryCreateText);
	const TCHAR* InBuffer=Buffer;
	FString StrLine, ClassName, BaseClassName;

	// Validate format.
	if( Class != UClass::StaticClass() )
	{
		Warn->Logf( TEXT("Can only import classes"), Type );
		return NULL;
	}
	if( appStricmp(Type,TEXT("UC"))!=0 )
	{
		Warn->Logf( TEXT("Can't import classes from files of type '%s'"), Type );
		return NULL;
	}

	// Import the script text.
    TArray<FName> DependentOn; //amb,gam
	FStringOutputDevice ScriptText, DefaultPropText, CppText;

	// mh, rjp
	INT CommentDim = 0;	// The layer of multi-line comment we are in.
	// -- mh, rjp

	while( ParseLine(&Buffer,StrLine,1) )
	{
		const TCHAR* Str=*StrLine, *Temp;
		UBOOL bProcess = CommentDim <= 0;	// for skipping nested multi-line comments

		if( ParseCommand(&Str,TEXT("cpptext")) && bProcess)
		{
			ScriptText.Logf( TEXT("// (cpptext)\r\n// (cpptext)\r\n")  );
			ParseLine(&Buffer,StrLine,1);
			Str = *StrLine;
			ParseNext( &Str );
			if( *Str!='{' )
				appErrorf( TEXT("%s: Missing '{' after cpptext."), *ClassName );

			// Get cpptext.
			while( ParseLine(&Buffer,StrLine,1) )
			{
				ScriptText.Logf( TEXT("// (cpptext)\r\n")  );
				Str = *StrLine;
				if( *Str=='}' )
					break;
				CppText.Logf( TEXT("%s\r\n"), *StrLine );
			}
		}
		else
		if( ParseCommand(&Str,TEXT("defaultproperties")) && bProcess )
		{
			// Get default properties text.
			while( ParseLine(&Buffer,StrLine,1) )
			{
				bProcess = CommentDim <= 0;
				INT Pos = INDEX_NONE, EndPos = INDEX_NONE, StrBegin = INDEX_NONE, StrEnd = INDEX_NONE;
				StrBegin = StrLine.InStr(TEXT("\""));
				if (StrBegin >= 0)
					StrEnd = StrLine.Mid(StrBegin + 1).InStr(TEXT("\"")) + StrBegin + 1;


				// Stub out the comments, ignoring anything inside literal strings.
				Pos = StrLine.InStr(TEXT("//"));
				if ( Pos>=0 )
				{
					if (StrBegin == INDEX_NONE || Pos < StrBegin || Pos > StrEnd)
						StrLine = StrLine.Left( Pos );
			
					if (StrLine == TEXT(""))
						continue;
				}
				
				// mh, rjp -- look for a /* ... */ block, ignoring anything inside literal strings
				Pos = StrLine.InStr(TEXT("/*"));
				EndPos = StrLine.InStr(TEXT("*/"));
				if ( Pos >=0 )
				{
					if (StrBegin == INDEX_NONE || Pos < StrBegin || Pos > StrEnd)
					{
						if (EndPos != INDEX_NONE && (EndPos < StrBegin || EndPos > StrEnd))
						{
							StrLine = StrLine.Left(Pos) + StrLine.Mid(EndPos + 2);
							EndPos = INDEX_NONE;
						}
						else 
						{
							StrLine = StrLine.Left( Pos );
							CommentDim++;
						}
					}
					bProcess = CommentDim <= 1;
				}
				if( EndPos>=0 )
				{
					if (StrBegin == INDEX_NONE || EndPos < StrBegin || EndPos > StrEnd)
					{
						StrLine = StrLine.Mid( EndPos+2 );
						CommentDim--;
					}

					bProcess = CommentDim <= 0;
				}
				
				Str = *StrLine;
				ParseNext( &Str );
				if( *Str=='}' && bProcess )
					break;

				if (!bProcess || StrLine == TEXT(""))
					continue;
				DefaultPropText.Logf( TEXT("%s\r\n"), *StrLine );
			}
		}
		else
		{
			// Get script text.
			ScriptText.Logf( TEXT("%s\r\n"), *StrLine );
			INT Pos = INDEX_NONE, EndPos = INDEX_NONE, StrBegin = INDEX_NONE, StrEnd = INDEX_NONE;
			StrBegin = StrLine.InStr(TEXT("\""));
			if (StrBegin >= 0)
			{	
				// mh -- find the end of a string (taking care of escapes)
				StrEnd = StrBegin;
				do {
					StrEnd = StrLine.Mid(StrEnd + 1).InStr(TEXT("\"")) + StrEnd + 1;
				}
				while ((StrLine[StrEnd-1] == '\\') && (StrLine[StrEnd-2] != '\\'));
			}

			// Stub out the comments, ignoring anything inside literal strings.
			Pos = StrLine.InStr(TEXT("//"));
			if ( Pos>=0 )
			{
				if (StrBegin == INDEX_NONE || Pos < StrBegin || Pos > StrEnd)
					StrLine = StrLine.Left( Pos );
		
				if (StrLine == TEXT(""))
					continue;
			}
			
			// mh, rjp -- look for a / * ... * / block, ignoring anything inside literal strings
			Pos = StrLine.InStr(TEXT("/*"));
			EndPos = StrLine.InStr(TEXT("*/"));
			if ( Pos >=0 )
			{
				if (StrBegin == INDEX_NONE || Pos < StrBegin || Pos > StrEnd)
				{
					if (EndPos != INDEX_NONE && (EndPos < StrBegin || EndPos > StrEnd))
					{
						StrLine = StrLine.Left(Pos) + StrLine.Mid(EndPos + 2);
						EndPos = INDEX_NONE;
					}
					else 
					{
						StrLine = StrLine.Left( Pos );
						CommentDim++;
					}
				}
				bProcess = CommentDim <= 1;
			}
			if( EndPos>=0 )
			{
				if (StrBegin == INDEX_NONE || EndPos < StrBegin || EndPos > StrEnd)
				{
					StrLine = StrLine.Mid( EndPos+2 );
					CommentDim--;
				}

				bProcess = CommentDim <= 0;
			}
			
			if (!bProcess || StrLine == TEXT(""))
				continue;

			Str = *StrLine;
			// Get class name.
			if( ClassName==TEXT("") && (Temp=appStrfind(Str, TEXT("class")))!=0 )
			{
				Temp+=6;
				ParseToken( Temp, ClassName, 0 );
			}
			if
			(	BaseClassName==TEXT("")
			&&	(Temp=appStrfind(Str, TEXT("extends")))!=0 )
			{
				Temp+=7;
				ParseToken( Temp, BaseClassName, 0 );
				while( BaseClassName.Right(1)==TEXT(";") )
					BaseClassName = BaseClassName.LeftChop( 1 );
			}

            // amb, gam ---
            Temp = Str;
            while ((Temp=appStrfind(Temp, TEXT("dependson(")))!=0)
            {
                Temp+=10;
                TCHAR ass[NAME_SIZE];
                INT i = 0;
                while (*Temp != ')' && i<NAME_SIZE)
                    ass[i++] = *Temp++;
                ass[i] = 0;
                FName dependsOnName(ass);
                DependentOn.AddUniqueItem(dependsOnName);
            }
            // --- amb,gam
		}
	}

	debugf(TEXT("Class: %s extends %s"),*ClassName,*BaseClassName);

	// Handle failure.
	if( ClassName==TEXT("") || (BaseClassName==TEXT("") && ClassName!=TEXT("Object")) )
	{
		Warn->Logf ( NAME_Error, TEXT("Bad class definition '%s'/'%s'/%i/%i"), *ClassName, *BaseClassName, BufferEnd-InBuffer, appStrlen(InBuffer) ); // gam
		return NULL;
	}
	else if( ClassName!=*Name )
	{
		Warn->Logf ( NAME_Error, TEXT("Script vs. class name mismatch (%s/%s)"), *Name, *ClassName ); // gam
	}

	UClass* ResultClass = FindObject<UClass>( InParent, *ClassName );
	if( ResultClass && (ResultClass->GetFlags() & RF_Native) )
	{
		// Gracefully update an existing hardcoded class.
		debugf( NAME_Log, TEXT("Updated native class '%s'"), ResultClass->GetFullName() );
		ResultClass->ClassFlags &= ~(CLASS_Parsed | CLASS_Compiled);
	}
	else
	{
		// Create new class.
		ResultClass = new( InParent, *ClassName, Flags )UClass( NULL );

		// Find or forward-declare base class.
		ResultClass->SuperField = FindObject<UClass>( InParent, *BaseClassName );
		if( !ResultClass->SuperField )
			ResultClass->SuperField = FindObject<UClass>( ANY_PACKAGE, *BaseClassName );
		if( !ResultClass->SuperField )
			ResultClass->SuperField = new( InParent, *BaseClassName )UClass( ResultClass );
		debugf( NAME_Log, TEXT("Imported: %s"), ResultClass->GetFullName() );
	}

	// Set class info.
	ResultClass->ScriptText      = new( ResultClass, TEXT("ScriptText"),   RF_NotForClient|RF_NotForServer )UTextBuffer( *ScriptText );
	ResultClass->DefaultPropText = DefaultPropText;
    ResultClass->DependentOn = DependentOn; //amb,gam
	if( CppText.Len() )
		ResultClass->CppText     = new( ResultClass, TEXT("CppText"),      RF_NotForClient|RF_NotForServer )UTextBuffer( *CppText );

	return ResultClass;
	unguard;
}
IMPLEMENT_CLASS(UClassFactoryUC);

/*------------------------------------------------------------------------------
	ULevelFactory.
------------------------------------------------------------------------------*/

static void ForceValid( ULevel* Level, UStruct* Struct, BYTE* Data )
{
	guard(ForceValid);
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Struct); It; ++It )
	{
		for( INT i=0; i<It->ArrayDim; i++ )
		{
			BYTE* Value = Data + It->Offset + i*It->ElementSize;
			if( Cast<UObjectProperty>(*It) )
			{
				UObject*& Obj = *(UObject**)Value;
				if( Cast<AActor>(Obj) )
				{
					INT j;
					for( j=0; j<Level->Actors.Num(); j++ )
						if( Level->Actors(j)==Obj )
							break;
					if( j==Level->Actors.Num() )
					{
						debugf( NAME_Log, TEXT("Usurped %s"), Obj->GetClass()->GetName() );
						Obj = NULL;
					}
				}
			}
			else if( Cast<UStructProperty>(*It) )
			{
				ForceValid( Level, ((UStructProperty*)*It)->Struct, Value );
			}
		}
	}
	unguard;
}

void ULevelFactory::StaticConstructor()
{
	guard(ULevelFactory::StaticConstructor);

	SupportedClass = ULevel::StaticClass();
	new(Formats)FString(TEXT("t3d;Unreal model text"));
	bCreateNew = 0;
	bText = 1;

	unguard;
}
ULevelFactory::ULevelFactory()
{
	guard(ULevelFactory::ULevelFactory);
	unguard;
}
UObject* ULevelFactory::FactoryCreateText
(
	ULevel*				InLevel,
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(ULevelFactory::FactoryCreateText);
	TMap<AActor*,FString> Map;

	// Create (or replace) the level object.
	InLevel->CompactActors();
	check(InLevel->Actors.Num()>1);
	check(Cast<ALevelInfo>(InLevel->Actors(0)));
	check(Cast<ABrush>(InLevel->Actors(1)));

	// Init actors.
	guard(InitActors);
	for( INT i=0; i<InLevel->Actors.Num(); i++ )
	{
		if( InLevel->Actors(i) )
		{
			InLevel->Actors(i)->bTempEditor = 1;

			if(InLevel->Actors(i)->bSelected)
				GEditor->SelectActor( InLevel, InLevel->Actors(i), 0, 0 );
		}
	}
	unguard;

	// Assumes data is being imported over top of a new, valid map.
	ParseNext( &Buffer );
	if( !GetBEGIN( &Buffer, TEXT("MAP")) )
		return InLevel;

	// Import everything.
	INT ImportedActive=appStricmp(Type,TEXT("paste"))==0;
	FString StrLine;
    const TCHAR *BufferStart = Buffer; // gam
	while( ParseLine(&Buffer,StrLine) )
	{
        Warn->StatusUpdatef( Buffer - BufferStart, BufferEnd - BufferStart, TEXT("Reading file") ); // gam

		const TCHAR* Str = *StrLine;
		if( GetEND(&Str,TEXT("MAP")) )
		{
			// End of brush polys.
			break;
		}
		else if( GetBEGIN(&Str,TEXT("BRUSH")) )
		{
			guard(ParseBrush);
			// Warn->StatusUpdatef( 0, 0, TEXT("Importing Brushes") ); gam
			TCHAR BrushName[NAME_SIZE];
			if( Parse(Str,TEXT("NAME="),BrushName,NAME_SIZE) )
			{
				ABrush* Actor;
				if( !ImportedActive )
				{
					// Parse the active brush, which has already been allocated.
					Actor          = InLevel->Brush();
					ImportedActive = 1;
				}
				else
				{
					// Parse a new brush which has not yet been allocated.
					Actor             = InLevel->SpawnBrush();
					GEditor->SelectActor( InLevel, Actor, 1, 0 );
					Actor->Brush      = new( InParent, NAME_None, RF_NotForClient|RF_NotForServer )UModel( NULL );			
				}

				// Import.
				Actor->SetFlags       ( RF_NotForClient | RF_NotForServer );
				Actor->Brush->SetFlags( RF_NotForClient | RF_NotForServer );
				UModelFactory* It = new UModelFactory;
				Actor->Brush = (UModel*)It->FactoryCreateText(InLevel,UModel::StaticClass(),InParent,Actor->Brush->GetFName(),0,Actor,Type,Buffer,BufferEnd,Warn);
				check(Actor->Brush);
				if( (Actor->PolyFlags&PF_Portal) && !(Actor->PolyFlags&PF_Translucent) )
					Actor->PolyFlags |= PF_Invisible;
			}
			unguard;
		}
		else if( GetBEGIN(&Str,TEXT("ACTOR")) )
		{
			guard(ParseActor);
			UClass* TempClass;
			if( ParseObject<UClass>( Str, TEXT("CLASS="), TempClass, ANY_PACKAGE ) )
			{
				// Get actor name.
				FName ActorName(NAME_None);
				Parse( Str, TEXT("NAME="), ActorName );

				// Make sure this name is unique.
				AActor* Found=NULL;
				if( ActorName!=NAME_None )
					Found = FindObject<AActor>( InParent, *ActorName );
				if( Found )
					Found->Rename();

				// Import it.
				AActor* Actor = InLevel->SpawnActor( TempClass, ActorName, FVector(0,0,0), FRotator(0,0,0), NULL, 1, 0 );
				check(Actor);

				// Get property text.
				FString PropText, StrLine;
				while
				(	GetEND( &Buffer, TEXT("ACTOR") )==0
				&&	ParseLine( &Buffer, StrLine ) )
				{
					PropText += *StrLine;
					PropText += TEXT("\r\n");
				}
				Map.Set( Actor, *PropText );

				// Handle class.
				if( Cast<ALevelInfo>(Actor) )
				{
					// Copy the one LevelInfo to position #0.
					check(InLevel->Actors.Num()>0);
					INT iActor=0; InLevel->Actors.FindItem( Actor, iActor );
					InLevel->Actors(0)       = Actor;
					InLevel->Actors(iActor)  = NULL;
					InLevel->Actors.ModifyItem(0);
					InLevel->Actors.ModifyItem(iActor);
				}
				else if( Actor->GetClass()==ABrush::StaticClass() && !ImportedActive )
				{
					// Copy the active brush to position #1.
					INT iActor=0; InLevel->Actors.FindItem( Actor, iActor );
					InLevel->Actors(1)       = Actor;
					InLevel->Actors(iActor)  = NULL;
					InLevel->Actors.ModifyItem(1);
					InLevel->Actors.ModifyItem(iActor);
					ImportedActive = 1;
				}
			}
			unguard;
		}
		else if( GetBEGIN(&Str,TEXT("SURFACE")) )
		{
			guard(ParseSurface);

			FString PropText, StrLine;

			UMaterial* Material = NULL;
			FVector Base, TextureU, TextureV, Normal;
			DWORD PolyFlags = 0;
			INT Check = 0;

			Base = FVector(0,0,0);
			TextureU = FVector(0,0,0);
			TextureV = FVector(0,0,0);
			Normal = FVector(0,0,0);

			do
			{
				if( ParseCommand(&Buffer,TEXT("TEXTURE")) )
				{
					Buffer++;	// Move past the '=' sign

					FString	TextureName(Buffer);
					
					TextureName = TextureName.Left(TextureName.InStr(TEXT("\r")));

					Material = Cast<UMaterial>(StaticLoadObject( UMaterial::StaticClass(), NULL, *TextureName, NULL, LOAD_NoWarn, NULL ));

					Check++;
				}
				else if( ParseCommand(&Buffer,TEXT("BASE")) )
				{
					GetFVECTOR( Buffer, Base );
					Check++;
				}
				else if( ParseCommand(&Buffer,TEXT("TEXTUREU")) )
				{
					GetFVECTOR( Buffer, TextureU );
					Check++;
				}
				else if( ParseCommand(&Buffer,TEXT("TEXTUREV")) )
				{
					GetFVECTOR( Buffer, TextureV );
					Check++;
				}
				else if( ParseCommand(&Buffer,TEXT("NORMAL")) )
				{
					GetFVECTOR( Buffer, Normal );
					Check++;
				}
				else if( ParseCommand(&Buffer,TEXT("POLYFLAGS")) )
				{
					Parse( Buffer, TEXT("="), PolyFlags );
					Check++;
				}
			}
			while
			(	GetEND( &Buffer, TEXT("SURFACE") )==0
				&&	ParseLine( &Buffer, StrLine ) );

			if( Check == 6 )
			{
				GEditor->Trans->Begin( TEXT("paste texture to surface") );

				for( INT i = 0 ; i < InLevel->Model->Surfs.Num() ; i++ )
				{
					FBspSurf* Surf = &InLevel->Model->Surfs(i);
					FVector SurfNormal = InLevel->Model->Vectors( Surf->vNormal );
					
					InLevel->Model->ModifySurf( i, 1 );

					if( Surf->PolyFlags & PF_Selected )
					{
						FVector NewBase, NewTextureU, NewTextureV;

						NewBase = Base;
						NewTextureU = TextureU;
						NewTextureV = TextureV;

						// Need to compensate for changes in the polygon normal
						FRotator DeltaRot = (Normal * -1).Rotation();
						DeltaRot += SurfNormal.Rotation();

						NewBase = Base.TransformPointBy( GMath.UnitCoords * DeltaRot );
						NewTextureU = TextureU.TransformVectorBy( GMath.UnitCoords * DeltaRot );
						NewTextureV = TextureV.TransformVectorBy( GMath.UnitCoords * DeltaRot );

						Surf->Material = Material;
						Surf->pBase = GEditor->bspAddPoint( InLevel->Model, &NewBase, 1 );
						Surf->vTextureU = GEditor->bspAddVector( InLevel->Model, &NewTextureU, 0 );
						Surf->vTextureV = GEditor->bspAddVector( InLevel->Model, &NewTextureV, 0 );
						Surf->PolyFlags = PolyFlags;

						Surf->PolyFlags &= ~PF_Selected;

						GEditor->polyUpdateMaster( InLevel->Model, i, 1 );
					}
				}

				InLevel->Model->ClearRenderData(GEditor->GRenDev);

				GEditor->Trans->End();
			}

			unguard;
		}
	}

	// Import actor properties.
	// We do this after creating all actors so that actor references can be matched up.
	guard(Finishing);
	check(Cast<ALevelInfo>(InLevel->Actors(0)));
	for( INT i=0; i<InLevel->Actors.Num(); i++ )
	{
		AActor* Actor = InLevel->Actors(i);
		if( Actor )
		{
			if(!Actor->bTempEditor)
				GEditor->SelectActor( InLevel, Actor, !Actor->bTempEditor, 0 );

			FString*	PropText = Map.Find(Actor);
			UBOOL		ActorChanged = 0;
			if( PropText )
			{
				guard(ImportActorProperties);
				ImportProperties( Actor->GetClass(), (BYTE*)Actor, InLevel, **PropText, InParent, Warn, 0 );
				ActorChanged = 1;
				unguard;
			}

			if(Actor->Level != InLevel->Actors(0))
			{
				guard(FixupActor);
				Actor->Level  = (ALevelInfo*)InLevel->Actors(0);
				Actor->Region = FPointRegion( (ALevelInfo*)InLevel->Actors(0) );

				ForceValid( InLevel, Actor->GetClass(), (BYTE*)Actor );
				ActorChanged = 1;
				unguard;
			}

			if(ActorChanged)
			{
				// Notify actor its properties have changed.
				Actor->PostEditChange();
			}

			// Take this opportunity to clean up any CSG_Active brushes that might be
			// in the level.  These are useless unless they're the builder brush.
			if( Actor->IsStaticBrush()
					&& ((ABrush*)Actor)->CsgOper == CSG_Active
					&& ((ABrush*)Actor) != InLevel->Brush()
					&& !Actor->IsVolumeBrush() )
				InLevel->DestroyActor( Actor );
		}
	}
	unguard;

    #if DO_CHECK
    {
        INT i;

	    for( i=0; i<InLevel->Actors.Num(); i++ )
        {
		    AActor* Actor = InLevel->Actors(i);

    		if( !Actor )
                continue;

            check( Actor->Level );
            check( Actor->XLevel );
        }
    }
    #endif

    // --- gam

	return InLevel;
	unguard;
}
IMPLEMENT_CLASS(ULevelFactory);

// Bitmap compression types.
enum EBitmapCompression
{
	BCBI_RGB       = 0,
	BCBI_RLE8      = 1,
	BCBI_RLE4      = 2,
	BCBI_BITFIELDS = 3,
};

// .BMP file header.
#pragma pack(push,1)
struct FBitmapFileHeader
{
    _WORD bfType;
    DWORD bfSize;
    _WORD bfReserved1;
    _WORD bfReserved2;
    DWORD bfOffBits;
	friend FArchive& operator<<( FArchive& Ar, FBitmapFileHeader& H )
	{
		guard(FBitmapFileHeader<<);
		Ar << H.bfType << H.bfSize << H.bfReserved1 << H.bfReserved2 << H.bfOffBits;
		return Ar;
		unguard;
	}
};
#pragma pack(pop)

// .BMP subheader.
#pragma pack(push,1)
struct FBitmapInfoHeader
{
    DWORD biSize;
    DWORD biWidth;
    DWORD biHeight;
    _WORD biPlanes;
    _WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    DWORD biXPelsPerMeter;
    DWORD biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
	friend FArchive& operator<<( FArchive& Ar, FBitmapInfoHeader& H )
	{
		guard(FBitmapInfoHeader<<);
		Ar << H.biSize << H.biWidth << H.biHeight;
		Ar << H.biPlanes << H.biBitCount;
		Ar << H.biCompression << H.biSizeImage;
		Ar << H.biXPelsPerMeter << H.biYPelsPerMeter;
		Ar << H.biClrUsed << H.biClrImportant;
		return Ar;
		unguard;
	}
};
#pragma pack(pop)

/*-----------------------------------------------------------------------------
	UPolysFactory.
-----------------------------------------------------------------------------*/

struct FASEMaterial
{
	FASEMaterial()
	{
		Width = Height = 256;
		UTiling = VTiling = 1;
		Material = NULL;
		bTwoSided = bUnlit = bAlphaTexture = bMasked = bTranslucent = 0;
	}
	FASEMaterial( TCHAR* InName, INT InWidth, INT InHeight, FLOAT InUTiling, FLOAT InVTiling, UMaterial* InMaterial, UBOOL InTwoSided, UBOOL InUnlit, UBOOL InAlphaTexture, UBOOL InMasked, UBOOL InTranslucent )
	{
		appStrcpy( Name, InName );
		Width = InWidth;
		Height = InHeight;
		UTiling = InUTiling;
		VTiling = InVTiling;
		Material = InMaterial;
		bTwoSided = InTwoSided;
		bUnlit = InUnlit;
		bAlphaTexture = InAlphaTexture;
		bMasked = InMasked;
		bTranslucent = InTranslucent;
	}

	TCHAR Name[128];
	INT Width, Height;
	FLOAT UTiling, VTiling;
	UMaterial* Material;
	UBOOL bTwoSided, bUnlit, bAlphaTexture, bMasked, bTranslucent;
};

struct FASEMaterialHeader
{
	FASEMaterialHeader()
	{
	}

	TArray<FASEMaterial> Materials;
};

void UPolysFactory::StaticConstructor()
{
	guard(UPolysFactory::StaticConstructor);

	SupportedClass = UPolys::StaticClass();
	new(Formats)FString(TEXT("t3d;Unreal brush text"));
	bCreateNew = 0;
	bText = 1;

	unguard;
}
UPolysFactory::UPolysFactory()
{
	guard(UPolysFactory::UPolysFactory);
	unguard;
}
UObject* UPolysFactory::FactoryCreateText
(
	ULevel*				InLevel,
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(UPolysFactory::FactoryCreateText);

	// Create polys.
	UPolys* Polys = Context ? CastChecked<UPolys>(Context) : new(InParent,Name,Flags)UPolys;

	// Eat up if present.
	GetBEGIN( &Buffer, TEXT("POLYLIST") );

	// Parse all stuff.
	INT First=1, GotBase=0;
	FString StrLine, ExtraLine;
	FPoly Poly;
	while( ParseLine( &Buffer, StrLine ) )
	{
		const TCHAR* Str = *StrLine;
		if( GetEND(&Str,TEXT("POLYLIST")) )
		{
			// End of brush polys.
			break;
		}
		//
		//
		// AutoCad - DXF File
		//
		//
		else if( appStrstr(Str,TEXT("ENTITIES")) && First )
		{
			debugf(NAME_Log,TEXT("Reading Autocad DXF file"));
			INT Started=0, NumPts=0, IsFace=0;
			FVector PointPool[4096];
			FPoly NewPoly; NewPoly.Init();

			while
			(	ParseLine( &Buffer, StrLine, 1 )
			&&	ParseLine( &Buffer, ExtraLine, 1 ) )
			{
				// Handle the line.
				Str = *ExtraLine;
				INT Code = appAtoi(*StrLine);
				if( Code==0 )
				{
					// Finish up current poly.
					if( Started )
					{
						if( NewPoly.NumVertices == 0 )
						{
							// Got a vertex definition.
							NumPts++;
						}
						else if( NewPoly.NumVertices>=3 && NewPoly.NumVertices<FPoly::MAX_VERTICES )
						{
							// Got a poly definition.
							if( IsFace ) NewPoly.Reverse();
							NewPoly.Base = NewPoly.Vertex[0];
							NewPoly.Finalize(0);
							new(Polys->Element)FPoly( NewPoly );
						}
						else
						{
							// Bad.
							Warn->Logf( TEXT("DXF: Bad vertex count %i"), NewPoly.NumVertices );
						}
						
						// Prepare for next.
						NewPoly.Init();
					}
					Started=0;

					if( ParseCommand(&Str,TEXT("VERTEX")) )
					{
						// Start of new vertex.
						PointPool[NumPts] = FVector(0,0,0);
						Started = 1;
						IsFace  = 0;
					}
					else if( ParseCommand(&Str,TEXT("3DFACE")) )
					{
						// Start of 3d face definition.
						Started = 1;
						IsFace  = 1;
					}
					else if( ParseCommand(&Str,TEXT("SEQEND")) )
					{
						// End of sequence.
						NumPts=0;
					}
					else if( ParseCommand(&Str,TEXT("EOF")) )
					{
						// End of file.
						break;
					}
				}
				else if( Started )
				{
					// Replace commas with periods to handle european dxf's.
					//for( TCHAR* Stupid = appStrchr(*ExtraLine,','); Stupid; Stupid=appStrchr(Stupid,',') )
					//	*Stupid = '.';

					// Handle codes.
					if( Code>=10 && Code<=19 )
					{
						// X coordinate.
						if( IsFace && Code-10==NewPoly.NumVertices )
						{
							NewPoly.Vertex[NewPoly.NumVertices++] = FVector(0,0,0);
						}
						NewPoly.Vertex[Code-10].X = PointPool[NumPts].X = appAtof(*ExtraLine);
					}
					else if( Code>=20 && Code<=29 )
					{
						// Y coordinate.
						NewPoly.Vertex[Code-20].Y = PointPool[NumPts].Y = appAtof(*ExtraLine);
					}
					else if( Code>=30 && Code<=39 )
					{
						// Z coordinate.
						NewPoly.Vertex[Code-30].Z = PointPool[NumPts].Z = appAtof(*ExtraLine);
					}
					else if( Code>=71 && Code<=79 && (Code-71)==NewPoly.NumVertices )
					{
						INT iPoint = Abs(appAtoi(*ExtraLine));
						if( iPoint>0 && iPoint<=NumPts )
							NewPoly.Vertex[NewPoly.NumVertices++] = PointPool[iPoint-1];
						else debugf( NAME_Warning, TEXT("DXF: Invalid point index %i/%i"), iPoint, NumPts );
					}
				}
			}
		}
		//
		//
		// 3D Studio MAX - ASC File
		//
		//
		else if( appStrstr(Str,TEXT("Tri-mesh,")) && First )
		{
			debugf( NAME_Log, TEXT("Reading 3D Studio ASC file") );
			FVector PointPool[4096];

			AscReloop:
			int NumVerts = 0, TempNumPolys=0, TempVerts=0;
			while( ParseLine( &Buffer, StrLine ) )
			{
				Str = *StrLine;

				FString VertText = FString::Printf( TEXT("Vertex %i:"), NumVerts );
				FString FaceText = FString::Printf( TEXT("Face %i:"), TempNumPolys );
				if( appStrstr(Str,*VertText) )
				{
					PointPool[NumVerts].X = appAtof(appStrstr(Str,TEXT("X:"))+2);
					PointPool[NumVerts].Y = appAtof(appStrstr(Str,TEXT("Y:"))+2);
					PointPool[NumVerts].Z = appAtof(appStrstr(Str,TEXT("Z:"))+2);
					NumVerts++;
					TempVerts++;
				}
				else if( appStrstr(Str,*FaceText) )
				{
					Poly.Init();
					Poly.NumVertices=3;
					Poly.Vertex[0] = PointPool[appAtoi(appStrstr(Str,TEXT("A:"))+2)];
					Poly.Vertex[1] = PointPool[appAtoi(appStrstr(Str,TEXT("B:"))+2)];
					Poly.Vertex[2] = PointPool[appAtoi(appStrstr(Str,TEXT("C:"))+2)];
					Poly.Base = Poly.Vertex[0];
					Poly.Finalize(0);
					new(Polys->Element)FPoly(Poly);
					TempNumPolys++;
				}
				else if( appStrstr(Str,TEXT("Tri-mesh,")) )
					goto AscReloop;
			}
			debugf( NAME_Log, TEXT("Imported %i vertices, %i faces"), TempVerts, Polys->Element.Num() );
		}
		//
		//
		// 3D Studio MAX - ASE File
		//
		//
		else if( appStrstr(Str,TEXT("*3DSMAX_ASCIIEXPORT")) && First )
		{
			debugf( NAME_Log, TEXT("Reading 3D Studio ASE file") );

			TArray<FVector> Vertex;						// 1 FVector per entry
			TArray<INT> FaceIdx;						// 3 INT's for vertex indices per entry
			TArray<INT> FaceMaterialsIdx;				// 1 INT for material ID per face
			TArray<FVector> TexCoord;					// 1 FVector per entry
			TArray<INT> FaceTexCoordIdx;				// 3 INT's per entry
			TArray<FASEMaterialHeader> ASEMaterialHeaders;	// 1 per material (multiple sub-materials inside each one)
			TArray<DWORD>	SmoothingGroups;			// 1 DWORD per face.
			
            TArray<FVector> Tex2Coord;					// sjs - 1 FVector per entry
			TArray<INT> FaceTex2CoordIdx;				// sjs - 3 INT's per entry
			
			INT NumVertex = 0, NumFaces = 0, NumTVertex = 0, NumTFaces = 0, ASEMaterialRef = -1;
            INT NumTVertex2 = 0;
            INT NumTFaces2 = 0;

            UBOOL IgnoreMcdGeometry = 0;

			enum {
				GROUP_NONE			= 0,
				GROUP_MATERIAL		= 1,
				GROUP_GEOM			= 2,
			} Group;

			enum {
				SECTION_NONE		= 0,
				SECTION_MATERIAL	= 1,
				SECTION_MAP_DIFFUSE	= 2,
				SECTION_VERTS		= 3,
				SECTION_FACES		= 4,
				SECTION_TVERTS		= 5,
				SECTION_TFACES		= 6,
                SECTION_TVERTS2     = 7,
                SECTION_TFACES2     = 8,
			} Section;

			Group = GROUP_NONE;
			Section = SECTION_NONE;
			while( ParseLine( &Buffer, StrLine ) )
			{
				Str = *StrLine;

				if( Group == GROUP_NONE )
				{
					if( StrLine.InStr(TEXT("*MATERIAL_LIST")) != -1 )
						Group = GROUP_MATERIAL;
					else if( StrLine.InStr(TEXT("*GEOMOBJECT")) != -1 )
						Group = GROUP_GEOM;
				}
				else if ( Group == GROUP_MATERIAL )
				{
					static FLOAT UTiling = 1, VTiling = 1;
					static UMaterial* Material = NULL;
					static INT Height = 256, Width = 256;
					static UBOOL bTwoSided = 0, bUnlit = 0, bAlphaTexture = 0, bMasked = 0, bTranslucent = 0;

					// Determine the section and/or extract individual values
					if( StrLine == TEXT("}") )
						Group = GROUP_NONE;
					else if( StrLine.InStr(TEXT("*MATERIAL ")) != -1 )
						Section = SECTION_MATERIAL;
					else if( StrLine.InStr(TEXT("*MATERIAL_WIRE")) != -1 )
					{
						if( StrLine.InStr(TEXT("*MATERIAL_WIRESIZE")) == -1 )
							bTranslucent = 1;
					}
					else if( StrLine.InStr(TEXT("*MATERIAL_TWOSIDED")) != -1 )
					{
						bTwoSided = 1;
					}
					else if( StrLine.InStr(TEXT("*MATERIAL_SELFILLUM")) != -1 )
					{
						INT Pos = StrLine.InStr( TEXT("*") );
						FString NewStr = StrLine.Right( StrLine.Len() - Pos );
						FLOAT temp;
						appSSCANF( *NewStr, TEXT("*MATERIAL_SELFILLUM %f"), &temp );
						if( temp == 100.f || temp == 1.f )
							bUnlit = 1;
					}
					else if( StrLine.InStr(TEXT("*MATERIAL_TRANSPARENCY")) != -1 )
					{
						INT Pos = StrLine.InStr( TEXT("*") );
						FString NewStr = StrLine.Right( StrLine.Len() - Pos );
						FLOAT temp;
						appSSCANF( *NewStr, TEXT("*MATERIAL_TRANSPARENCY %f"), &temp );
						if( temp > 0.f )
							bAlphaTexture = 1;
					}
					else if( StrLine.InStr(TEXT("*MATERIAL_SHADING")) != -1 )
					{
						INT Pos = StrLine.InStr( TEXT("*") );
						FString NewStr = StrLine.Right( StrLine.Len() - Pos );
						TCHAR Buffer[20];
						appSSCANF( *NewStr, TEXT("*MATERIAL_SHADING %s"), Buffer );
						if( !appStrcmp( Buffer, TEXT("Constant") ) )
							bMasked = 1;
					}
					else if( StrLine.InStr(TEXT("*MAP_DIFFUSE")) != -1 )
					{
						Section = SECTION_MAP_DIFFUSE;
						Material = NULL;
						UTiling = VTiling = 1;
						Width = Height = 256;
					}
					else
					{
						if ( Section == SECTION_MATERIAL )
						{
							// We are entering a new material definition.  Allocate a new material header.
							new( ASEMaterialHeaders )FASEMaterialHeader();
							Section = SECTION_NONE;
						}
						else if ( Section == SECTION_MAP_DIFFUSE )
						{
							if( StrLine.InStr(TEXT("*BITMAP")) != -1 )
							{
								// Remove tabs from the front of this string.  The number of tabs differs
								// depending on how many materials are in the file.
								INT Pos = StrLine.InStr( TEXT("*") );
								FString NewStr = StrLine.Right( StrLine.Len() - Pos );

								NewStr = NewStr.Right( NewStr.Len() - NewStr.InStr(TEXT("\\"), -1 ) - 1 );	// Strip off path info
								NewStr = NewStr.Left( NewStr.Len() - 5 );									// Strip off '.bmp"' at the end

								// Find the texture
								Material = NULL;
								for( TObjectIterator<UMaterial> It; It ; ++It )
								{
									FString TexName = It->GetName();
									if( !appStrcmp( *TexName.Caps(), *NewStr.Caps() ) )
									{
										Material = *It;
										Width = Material->MaterialUSize();
										Height = Material->MaterialVSize();
										break;
									}
								}
							}
							else if( StrLine.InStr(TEXT("*UVW_U_TILING")) != -1 )
							{
								INT Pos = StrLine.InStr( TEXT("*") );
								FString NewStr = StrLine.Right( StrLine.Len() - Pos );
								appSSCANF( *NewStr, TEXT("*UVW_U_TILING %f"), &UTiling );
							}
							else if( StrLine.InStr(TEXT("*UVW_V_TILING")) != -1 )
							{
								INT Pos = StrLine.InStr( TEXT("*") );
								FString NewStr = StrLine.Right( StrLine.Len() - Pos );
								appSSCANF( *NewStr, TEXT("*UVW_V_TILING %f"), &VTiling );

								check(ASEMaterialHeaders.Num());
								new( ASEMaterialHeaders(ASEMaterialHeaders.Num()-1).Materials )FASEMaterial( (TCHAR*)*Name, Width, Height, UTiling, VTiling, Material, bTwoSided, bUnlit, bAlphaTexture, bMasked, bTranslucent );

								Section = SECTION_NONE;
								bTwoSided = bUnlit = bAlphaTexture = bMasked = bTranslucent = 0;
							}
						}
					}
				}
				else if ( Group == GROUP_GEOM )
				{
					// Determine the section and/or extract individual values
					if( StrLine == TEXT("}") )
                    {
                        IgnoreMcdGeometry = 0;
						Group = GROUP_NONE;
                    }
                    // See if this is an MCD thing
                    else if( StrLine.InStr(TEXT("*NODE_NAME")) != -1 )
                    {
                        TCHAR NodeName[512];                     
                        appSSCANF( Str, TEXT("\t*NODE_NAME \"%s\""), &NodeName );
                        if( appStrstr(NodeName, TEXT("MCD")) == NodeName )
                            IgnoreMcdGeometry = 1;
                        else 
                            IgnoreMcdGeometry = 0;
                    }

                    // Now do nothing if it's an MCD Geom
                    if( !IgnoreMcdGeometry )
					{              
						if( StrLine.InStr(TEXT("*MESH_NUMVERTEX")) != -1 )
							appSSCANF( Str, TEXT("\t\t*MESH_NUMVERTEX %d"), &NumVertex );
						else if( StrLine.InStr(TEXT("*MESH_NUMFACES")) != -1 )
							appSSCANF( Str, TEXT("\t\t*MESH_NUMFACES %d"), &NumFaces );
						else if( StrLine.InStr(TEXT("*MESH_VERTEX_LIST")) != -1 )
							Section = SECTION_VERTS;
						else if( StrLine.InStr(TEXT("*MESH_FACE_LIST")) != -1 )
							Section = SECTION_FACES;
						else if( StrLine.InStr(TEXT("*MESH_NUMTVERTEX")) != -1 )
						{
							if( Section == SECTION_TFACES2 ) // sjs
								appSSCANF( Str, TEXT("\t\t*MESH_NUMTVERTEX %d"), &NumTVertex2 );
							else
								appSSCANF( Str, TEXT("\t\t*MESH_NUMTVERTEX %d"), &NumTVertex );
						}
						else if( StrLine == TEXT("\t\t*MESH_TVERTLIST {") )
							Section = SECTION_TVERTS;
						else if( StrLine.InStr(TEXT("*MESH_NUMTVFACES")) != -1 )
						{
							if( Section == SECTION_TFACES2 ) // sjs
								appSSCANF( Str, TEXT("\t\t*MESH_NUMTVERTEX %d"), &NumTFaces2 );
							else
								appSSCANF( Str, TEXT("\t\t*MESH_NUMTVFACES %d"), &NumTFaces );
						}

						else if( StrLine.InStr(TEXT("*MATERIAL_REF")) != -1 )
							appSSCANF( Str, TEXT("\t*MATERIAL_REF %d"), &ASEMaterialRef );
						else if( StrLine == TEXT("\t\t*MESH_TFACELIST {") )
							Section = SECTION_TFACES;
						// sjs ---
						else if( StrLine.InStr(TEXT("*MESH_MAPPINGCHANNEL 2")) != -1 )
							Section = SECTION_TFACES2;
						else if( StrLine == TEXT("\t\t\t*MESH_TFACELIST {") )
							Section = SECTION_TFACES2;
						else if( StrLine == TEXT("\t\t\t*MESH_TVERTLIST {") )
							Section = SECTION_TVERTS2;
						// --- sjs
						else
						{
							// Extract data specific to sections
							if( Section == SECTION_VERTS )
							{
								if( StrLine.InStr(TEXT("\t\t}")) != -1 )
									Section = SECTION_NONE;
								else
								{
									int temp;
									FVector vtx;
									appSSCANF( Str, TEXT("\t\t\t*MESH_VERTEX    %d\t%f\t%f\t%f"),
										&temp, &vtx.X, &vtx.Y, &vtx.Z );
									new(Vertex)FVector(vtx);
								}
							}
							else if( Section == SECTION_FACES )
							{
								if( StrLine.InStr(TEXT("\t\t}")) != -1 )
									Section = SECTION_NONE;
								else
								{
									INT temp, idx1, idx2, idx3;
									appSSCANF( Str, TEXT("\t\t\t*MESH_FACE %d:    A: %d B: %d C: %d"),
										&temp, &idx1, &idx2, &idx3 );
									new(FaceIdx)INT(idx1);
									new(FaceIdx)INT(idx2);
									new(FaceIdx)INT(idx3);

									FString	SmoothingString;

									if(Parse(*StrLine,TEXT("*MESH_SMOOTHING "),SmoothingString))
									{
										DWORD	SmoothingMask = 0;

										while(SmoothingString.Len())
										{
											INT	Length = SmoothingString.InStr(TEXT(",")),
												SmoothingGroup = (Length != -1) ? appAtoi(*SmoothingString.Left(Length)) : appAtoi(*SmoothingString);

											if(SmoothingGroup <= 32)
												SmoothingMask |= (1 << (SmoothingGroup - 1));

											SmoothingString = (Length != -1) ? SmoothingString.Right(SmoothingString.Len() - Length - 1) : TEXT("");
										};

										SmoothingGroups.AddItem(SmoothingMask);
									}
									else
										SmoothingGroups.AddItem(0);

									// Sometimes "MESH_SMOOTHING" is a blank instead of a number, so we just grab the 
									// part of the string we need and parse out the material id.
									INT MaterialID;
									StrLine = StrLine.Right( StrLine.Len() - StrLine.InStr( TEXT("*MESH_MTLID"), -1 ) - 1 );
									appSSCANF( *StrLine , TEXT("MESH_MTLID %d"), &MaterialID );
									new(FaceMaterialsIdx)INT(MaterialID);
								}
							}
							else if( Section == SECTION_TVERTS )
							{
								if( StrLine.InStr(TEXT("\t\t}")) != -1 )
									Section = SECTION_NONE;
								else
								{
									int temp;
									FVector vtx;
									appSSCANF( Str, TEXT("\t\t\t*MESH_TVERT %d\t%f\t%f"),
										&temp, &vtx.X, &vtx.Y );
									vtx.Z = 0;
									new(TexCoord)FVector(vtx);
								}
							}
							else if( Section == SECTION_TFACES )
							{
								if( StrLine == TEXT("\t\t}") )
									Section = SECTION_NONE;
								else
								{
									int temp, idx1, idx2, idx3;
									appSSCANF( Str, TEXT("\t\t\t*MESH_TFACE %d\t%d\t%d\t%d"),
										&temp, &idx1, &idx2, &idx3 );
									new(FaceTexCoordIdx)INT(idx1);
									new(FaceTexCoordIdx)INT(idx2);
									new(FaceTexCoordIdx)INT(idx3);
								}
							}
							// sjs ---
							else if( Section == SECTION_TVERTS2 )
							{
								if( StrLine.InStr(TEXT("\t\t}")) != -1 )
									Section = SECTION_NONE;
								else
								{
									int temp;
									FVector vtx;
									appSSCANF( Str, TEXT("\t\t\t\t*MESH_TVERT %d\t%f\t%f"),
										&temp, &vtx.X, &vtx.Y );
									vtx.Z = 0;
									new(Tex2Coord)FVector(vtx);
								}
							}
							else if( Section == SECTION_TFACES2 )
							{
								if( StrLine == TEXT("\t\t}") )
									Section = SECTION_NONE;
								else
								{
									int temp, idx1, idx2, idx3;
									appSSCANF( Str, TEXT("\t\t\t\t*MESH_TFACE %d\t%d\t%d\t%d"),
										&temp, &idx1, &idx2, &idx3 );
									new(FaceTex2CoordIdx)INT(idx1);
									new(FaceTex2CoordIdx)INT(idx2);
									new(FaceTex2CoordIdx)INT(idx3);
								}
							}
							// --- sjs
						}
					}
				}
			}

			// Create the polys from the gathered info.
			if( FaceIdx.Num() != FaceTexCoordIdx.Num() )
			{
				appMsgf( 0, TEXT("ASE Importer Error : Did you forget to include geometry AND materials in the export?\n\nFaces : %d, Texture Coordinates : %d"),
					FaceIdx.Num(), FaceTexCoordIdx.Num());
				continue;
			}
			if( ASEMaterialRef == -1 )
			{
				appMsgf( 0, TEXT("ASE Importer Warning : This object has no material reference (current texture will be applied to object).") );
			}
			for( int x = 0 ; x < FaceIdx.Num() ; x += 3 )
			{
				Poly.Init();
				Poly.NumVertices = 3;
				Poly.Vertex[0] = Vertex( FaceIdx(x) );
				Poly.Vertex[1] = Vertex( FaceIdx(x+1) );
				Poly.Vertex[2] = Vertex( FaceIdx(x+2) );

				FASEMaterial ASEMaterial;
				if( ASEMaterialRef != -1 )
					if( ASEMaterialHeaders(ASEMaterialRef).Materials.Num() )
						if( ASEMaterialHeaders(ASEMaterialRef).Materials.Num() == 1 )
							ASEMaterial = ASEMaterialHeaders(ASEMaterialRef).Materials(0);
						else
						{
							// Sometimes invalid material references appear in the ASE file.  We can't do anything about
							// it, so when that happens just use the first material.
							if( FaceMaterialsIdx(x/3) >= ASEMaterialHeaders(ASEMaterialRef).Materials.Num() )
								ASEMaterial = ASEMaterialHeaders(ASEMaterialRef).Materials(0);
							else
								ASEMaterial = ASEMaterialHeaders(ASEMaterialRef).Materials( FaceMaterialsIdx(x/3) );
						}

				if( ASEMaterial.Material )
					Poly.Material = ASEMaterial.Material;

				Poly.SmoothingMask = SmoothingGroups(x / 3);

				Poly.Finalize(1);

				// Apply proper surface flags.
				Poly.PolyFlags = (ASEMaterial.bTwoSided?PF_TwoSided:0) | (ASEMaterial.bUnlit?PF_Unlit:0) | (ASEMaterial.bAlphaTexture?PF_AlphaTexture:0) | (ASEMaterial.bMasked?PF_Masked:0) | (ASEMaterial.bTranslucent?PF_Translucent:0);

				// The brushes come in flipped across the X axis, so adjust for that.
				FVector Flip(-1,1,1);
				Poly.Vertex[0] *= Flip;
				Poly.Vertex[1] *= Flip;
				Poly.Vertex[2] *= Flip;

				FVector	ST1 = TexCoord(FaceTexCoordIdx(x + 0)),
						ST2 = TexCoord(FaceTexCoordIdx(x + 1)),
						ST3 = TexCoord(FaceTexCoordIdx(x + 2));

                // sjs ---
                FVector	uv2[3] = { FVector(0,0,0), FVector(0,0,0), FVector(0,0,0) };
                if ( FaceTex2CoordIdx.Num() )
                {
                    uv2[0] = Tex2Coord(FaceTex2CoordIdx(x + 0));
                    uv2[0].Y = 1.0f - uv2[0].Y;
					uv2[1] = Tex2Coord(FaceTex2CoordIdx(x + 1));
                    uv2[1].Y = 1.0f - uv2[1].Y;
					uv2[2] = Tex2Coord(FaceTex2CoordIdx(x + 2));
                    uv2[2].Y = 1.0f - uv2[2].Y;
                }
                // --- sjs

				// FIXME : this doesn't work with non-square textures.  Why not?
				FTexCoordsToVectors(
					Poly.Vertex[0],
					FVector(ST1.X * ASEMaterial.Width * ASEMaterial.UTiling,(1.0f - ST1.Y) * ASEMaterial.Height * ASEMaterial.VTiling,ST1.Z),
					Poly.Vertex[1],
					FVector(ST2.X * ASEMaterial.Width * ASEMaterial.UTiling,(1.0f - ST2.Y) * ASEMaterial.Height * ASEMaterial.VTiling,ST2.Z),
					Poly.Vertex[2],
					FVector(ST3.X * ASEMaterial.Width * ASEMaterial.UTiling,(1.0f - ST3.Y) * ASEMaterial.Height * ASEMaterial.VTiling,ST3.Z),
					&Poly.Base,
					&Poly.TextureU,
					&Poly.TextureV
					);

				Poly.Reverse();
				Poly.CalcNormal();
                Poly.UV[0] = uv2[2];
                Poly.UV[1] = uv2[1];
                Poly.UV[2] = uv2[0];

				new(Polys->Element)FPoly(Poly);
			}

			debugf( NAME_Log, TEXT("Imported %i vertices, %i faces"), NumVertex, NumFaces );
		}
		//
		//
		// T3D FORMAT
		//
		//
		else if( GetBEGIN(&Str,TEXT("POLYGON")) )
		{
			// Init to defaults and get group/item and texture.
			Poly.Init();
			Parse( Str, TEXT("LINK="), Poly.iLink );
			Parse( Str, TEXT("ITEM="), Poly.ItemName );
			Parse( Str, TEXT("FLAGS="), Poly.PolyFlags );
			Poly.PolyFlags &= ~PF_NoImport;

			FString TextureName;
			Parse( Str, TEXT("TEXTURE="), TextureName );

			Poly.Material = Cast<UMaterial>(StaticLoadObject( UMaterial::StaticClass(), NULL, *TextureName, NULL, LOAD_NoWarn, NULL ));

			// Fix to make it work with #exec import of T3D textured brushes  - Erik
			if( ! Poly.Material)
				Poly.Material = Cast<UMaterial> ( StaticFindObject( UMaterial::StaticClass(), ANY_PACKAGE, *TextureName));		
		}
		else if( ParseCommand(&Str,TEXT("PAN")) )
		{
			INT	PanU = 0,
				PanV = 0;

			Parse( Str, TEXT("U="), PanU );
			Parse( Str, TEXT("V="), PanV );

			Poly.Base += Poly.TextureU * PanU;
			Poly.Base += Poly.TextureV * PanV;
		}
		else if( ParseCommand(&Str,TEXT("ORIGIN")) )
		{
			GotBase=1;
			GetFVECTOR( Str, Poly.Base );
		}
		else if( ParseCommand(&Str,TEXT("VERTEX")) )
		{
			if( Poly.NumVertices < FPoly::MAX_VERTICES )
			{
				GetFVECTOR( Str, Poly.Vertex[Poly.NumVertices] );
				Poly.NumVertices++;
			}
		}
		else if( ParseCommand(&Str,TEXT("TEXTUREU")) )
		{
			GetFVECTOR( Str, Poly.TextureU );
		}
		else if( ParseCommand(&Str,TEXT("TEXTUREV")) )
		{
			GetFVECTOR( Str, Poly.TextureV );
		}
		else if( GetEND(&Str,TEXT("POLYGON")) )
		{
			if( !GotBase )
				Poly.Base = Poly.Vertex[0];
			if( Poly.Finalize(1)==0 )
				new(Polys->Element)FPoly(Poly);
			GotBase=0;
		}
	}

	// Success.
	return Polys;
	unguard;
}
IMPLEMENT_CLASS(UPolysFactory);

/*-----------------------------------------------------------------------------
	UModelFactory.
-----------------------------------------------------------------------------*/

void UModelFactory::StaticConstructor()
{
	guard(UModelFactory::StaticConstructor);

	SupportedClass = UModel::StaticClass();
	new(Formats)FString(TEXT("t3d;Unreal model text"));
	bCreateNew = 0;
	bText = 1;

	unguard;
}
UModelFactory::UModelFactory()
{
	guard(UModelFactory::UModelFactory);
	unguard;
}
UObject* UModelFactory::FactoryCreateText
(
	ULevel*				InLevel,
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(UModelFactory::FactoryCreateText);
	ABrush* TempOwner = (ABrush*)Context;
	UModel* Model = new( InParent, Name, Flags )UModel( TempOwner, 1 );

	const TCHAR* StrPtr;
	FString StrLine;
	if( TempOwner )
	{
		TempOwner->InitPosRotScale();
		TempOwner->bSelected   = 0;
		TempOwner->bTempEditor = 0;
	}
	while( ParseLine( &Buffer, StrLine ) )
	{
		StrPtr = *StrLine;
		if( GetEND(&StrPtr,TEXT("BRUSH")) )
		{
			break;
		}
		else if( GetBEGIN (&StrPtr,TEXT("POLYLIST")) )
		{
			UPolysFactory* PolysFactory = new UPolysFactory;
			Model->Polys = (UPolys*)PolysFactory->FactoryCreateText(InLevel,UPolys::StaticClass(),InParent,NAME_None,0,NULL,Type,Buffer,BufferEnd,Warn);
			check(Model->Polys);
		}
		if( TempOwner )
		{
			if      (ParseCommand(&StrPtr,TEXT("PREPIVOT"	))) GetFVECTOR 	(StrPtr,TempOwner->PrePivot);
			else if (ParseCommand(&StrPtr,TEXT("SCALE"		))) GetFSCALE 	(StrPtr,TempOwner->MainScale);
			else if (ParseCommand(&StrPtr,TEXT("POSTSCALE"	))) GetFSCALE 	(StrPtr,TempOwner->PostScale);
			else if (ParseCommand(&StrPtr,TEXT("LOCATION"	))) GetFVECTOR	(StrPtr,TempOwner->Location);
			else if (ParseCommand(&StrPtr,TEXT("ROTATION"	))) GetFROTATOR  (StrPtr,TempOwner->Rotation,1);
			if( ParseCommand(&StrPtr,TEXT("SETTINGS")) )
			{
				Parse( StrPtr, TEXT("CSG="), TempOwner->CsgOper );
				Parse( StrPtr, TEXT("POLYFLAGS="), TempOwner->PolyFlags );
			}
		}
	}
	if( GEditor )
		GEditor->bspValidateBrush( Model, 1, 0 );

	return Model;
	unguard;
}
IMPLEMENT_CLASS(UModelFactory);

/*-----------------------------------------------------------------------------
	USoundFactory.
-----------------------------------------------------------------------------*/

void USoundFactory::StaticConstructor()
{
	guard(USoundFactory::StaticConstructor);

	SupportedClass = USound::StaticClass();
	new(Formats)FString(TEXT("wav;Wave audio files"));
	bCreateNew = 0;

	unguard;
}
USoundFactory::USoundFactory()
{
	guard(USoundFactory::USoundFactory);
	unguard;
}
UObject* USoundFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		FileType,
	const BYTE*&		Buffer,
	const BYTE*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(USoundFactory::FactoryCreateBinary);
	if
	(	appStricmp(FileType, TEXT("WAV"))==0
	||	appStricmp(FileType, TEXT("MP3"))==0 )
	{
		// Wave file.
		USound* Sound = new(InParent,Name,Flags)USound;
		Sound->SetFileType (FName(FileType)); // gam
		Sound->GetData().Add( BufferEnd-Buffer ); // gam
		appMemcpy( &Sound->GetData()(0), Buffer, Sound->GetData().Num() ); // gam
		return Sound;
	}
	else if( appStricmp(FileType, TEXT("UFX"))==0 )//oldver
	{
		// Outdated ufx file.
		Warn->Logf( TEXT("Invalid old-format sound %s"), *Name );
		return NULL;
	}
	else
	{
		// Unrecognized.
		Warn->Logf( TEXT("Unrecognized sound format '%s' in %s"), FileType, *Name );
		return NULL;
	}
	unguard;
}
IMPLEMENT_CLASS(USoundFactory);

/*-----------------------------------------------------------------------------
	UPrefabFactory.
-----------------------------------------------------------------------------*/

void UPrefabFactory::StaticConstructor()
{
	guard(UPrefabFactory::StaticConstructor);

	SupportedClass = UPrefab::StaticClass();
	new(Formats)FString(TEXT("T3D;Prefab files"));
	bCreateNew = 0;

	unguard;
}
UPrefabFactory::UPrefabFactory()
{
	guard(UPrefabFactory::UPrefabFactory);
	unguard;
}
UObject* UPrefabFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		FileType,
	const BYTE*&		Buffer,
	const BYTE*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(UPrefabFactory::FactoryCreateBinary);
	if
	(	appStricmp(FileType, TEXT("T3D"))==0 )
	{
		// Prefab file.
		UPrefab* Prefab = new(InParent,Name,Flags)UPrefab;
		Prefab->T3DText = winAnsiToTCHAR( (ANSICHAR*)Buffer );
		return Prefab;
	}
	else
	{
		// Unrecognized.
		Warn->Logf( TEXT("Unrecognized prefab format '%s' in %s"), FileType, *Name );
		return NULL;
	}
	unguard;
}
IMPLEMENT_CLASS(UPrefabFactory);

/*-----------------------------------------------------------------------------
	UTextureFactory.
-----------------------------------------------------------------------------*/

// .PCX file header.
#pragma pack(push,1)
class FPCXFileHeader
{
public:
	BYTE	Manufacturer;		// Always 10.
	BYTE	Version;			// PCX file version.
	BYTE	Encoding;			// 1=run-length, 0=none.
	BYTE	BitsPerPixel;		// 1,2,4, or 8.
	_WORD	XMin;				// Dimensions of the image.
	_WORD	YMin;				// Dimensions of the image.
	_WORD	XMax;				// Dimensions of the image.
	_WORD	YMax;				// Dimensions of the image.
	_WORD	XDotsPerInch;		// Horizontal printer resolution.
	_WORD	YDotsPerInch;		// Vertical printer resolution.
	BYTE	OldColorMap[48];	// Old colormap info data.
	BYTE	Reserved1;			// Must be 0.
	BYTE	NumPlanes;			// Number of color planes (1, 3, 4, etc).
	_WORD	BytesPerLine;		// Number of bytes per scanline.
	_WORD	PaletteType;		// How to interpret palette: 1=color, 2=gray.
	_WORD	HScreenSize;		// Horizontal monitor size.
	_WORD	VScreenSize;		// Vertical monitor size.
	BYTE	Reserved2[54];		// Must be 0.
	friend FArchive& operator<<( FArchive& Ar, FPCXFileHeader& H )
	{
		guard(FPCXFileHeader<<);
		Ar << H.Manufacturer << H.Version << H.Encoding << H.BitsPerPixel;
		Ar << H.XMin << H.YMin << H.XMax << H.YMax << H.XDotsPerInch << H.YDotsPerInch;
		for( INT i=0; i<ARRAY_COUNT(H.OldColorMap); i++ )
			Ar << H.OldColorMap[i];
		Ar << H.Reserved1 << H.NumPlanes;
		Ar << H.BytesPerLine << H.PaletteType << H.HScreenSize << H.VScreenSize;
		for( INT i=0; i<ARRAY_COUNT(H.Reserved2); i++ )
			Ar << H.Reserved2[i];
		return Ar;
		unguard;
	}
};
#pragma pack(pop)

#pragma pack(push,1)
struct FTGAFileHeader
{
	BYTE IdFieldLength;
	BYTE ColorMapType;
	BYTE ImageTypeCode;		// 2 for uncompressed RGB format
	_WORD ColorMapOrigin;
	_WORD ColorMapLength;
	BYTE ColorMapEntrySize;
	_WORD XOrigin;
	_WORD YOrigin;
	_WORD Width;
	_WORD Height;
	BYTE BitsPerPixel;
	BYTE ImageDescriptor;
	friend FArchive& operator<<( FArchive& Ar, FTGAFileHeader& H )
	{
		guard(FTGAFileHeader<<);
		Ar << H.IdFieldLength << H.ColorMapType << H.ImageTypeCode;
		Ar << H.ColorMapOrigin << H.ColorMapLength << H.ColorMapEntrySize;
		Ar << H.XOrigin << H.YOrigin << H.Width << H.Height << H.BitsPerPixel;
		Ar << H.ImageDescriptor;
		return Ar;
		unguard;
	}
};
#pragma pack(pop)

#pragma pack(push,1)
struct FTGAFileFooter
{
	DWORD ExtensionAreaOffset;
	DWORD DeveloperDirectoryOffset;
	BYTE Signature[16];
	BYTE TrailingPeriod;
	BYTE NullTerminator;
};
#pragma pack(pop)

// Unreal painter format. Like TGA header - with header bits flipped and bitmap rows scrambled; unmodified footer.
#pragma pack(push,1)
struct FUPTFileHeader
{
	BYTE  UPTTag[4];
	DWORD seed;
	// Rest is xor255-ed TGA.
	BYTE IdFieldLength;
	BYTE ColorMapType;
	BYTE ImageTypeCode;		
	_WORD ColorMapOrigin;
	_WORD ColorMapLength;
	BYTE ColorMapEntrySize;
	_WORD XOrigin;
	_WORD YOrigin;
	_WORD Width;
	_WORD Height;
	BYTE BitsPerPixel;
	BYTE ImageDescriptor;

	friend FArchive& operator<<( FArchive& Ar, FUPTFileHeader& H )
	{
		guard( FUPTFileHeader<< );
		Ar << H.UPTTag[0] << H.UPTTag[1] << H.UPTTag[2] << H.UPTTag[3];
		Ar << H.seed;
		Ar << H.IdFieldLength << H.ColorMapType << H.ImageTypeCode;
		Ar << H.ColorMapOrigin << H.ColorMapLength << H.ColorMapEntrySize;
		Ar << H.XOrigin << H.YOrigin << H.Width << H.Height << H.BitsPerPixel;
		Ar << H.ImageDescriptor;
		return Ar;
		unguard;
	}
};
#pragma pack(pop)

#pragma pack(push,1)
struct FUPTFileFooter
{
	DWORD ExtensionAreaOffset;
	DWORD DeveloperDirectoryOffset;
	BYTE Signature[16];
	BYTE TrailingPeriod;
	BYTE NullTerminator;
};
#pragma pack(pop)


#ifndef UPTSCRAMBLEFACTOR
#define UPTSCRAMBLEFACTOR (0)
#endif
#ifndef UPTSCRAMBLEOFFSET
#define UPTSCRAMBLEOFFSET (0)
#endif
//
// Unreal painter format support code ( reversible row scrambling. )
//
void UPTFillMixArray( TArray<DWORD>& MixArray, const DWORD seed)
{
	guard( UPTFillMixArray );
	for( INT i=0; i< MixArray.Num(); i++)
	{
		MixArray(i) = i; 
	}	
	DWORD scramble = seed;
	for( INT i=0;i<MixArray.Num(); i++)
	{		
		scramble = (DWORD)scramble*(DWORD)UPTSCRAMBLEFACTOR + (DWORD)UPTSCRAMBLEOFFSET;
		DWORD index = scramble % (DWORD)MixArray.Num();				
		INT LineA = i;     
		INT LineB = index; 
		DWORD temp = MixArray(LineA);
		MixArray(LineA) = MixArray(LineB);
		MixArray(LineB) = temp;						
	}	
	unguard;
}
void DescrambleUPT( DWORD* Bitmap, DWORD Seed, INT XSize, INT YSize )
{	
	guard(DescrambleUPT);
	TArray<DWORD> MixArray;
	TArray<DWORD> TempShuffle;
	MixArray.Add( YSize );
	TempShuffle.Add( YSize );
	UPTFillMixArray( MixArray, Seed );					

	for( INT i=0; i<XSize; i++)
	{
		for( INT j=0; j<YSize; j++)
		{
			TempShuffle(YSize-j-1) = *((Bitmap+j*XSize)+i);   // Imply vertical flip.
		}
		for( INT j=0; j<YSize; j++)
		{										
			*((Bitmap+MixArray(j)*XSize)+i) = TempShuffle(j); 
		}
	}	
	unguard;
}
void ScrambleUPT( DWORD* Bitmap, DWORD Seed, INT XSize, INT YSize )
{
	guard(ScrambleUPT);

	TArray<DWORD> MixArray;
	TArray<DWORD> TempShuffle;
	MixArray.Add( YSize );
	TempShuffle.Add( YSize );
	UPTFillMixArray( MixArray, Seed );					

	for( INT i=0; i<XSize; i++)
	{
		for( INT j=0; j<YSize; j++)
		{
			TempShuffle(j) = *((Bitmap+j*XSize)+i);
		}
		for( INT j=0; j<YSize; j++)
		{												
			*((Bitmap+(YSize-j-1 ) *XSize)+i) = TempShuffle( MixArray(j) );  // Imply vertical flip.
		}
	}	

	unguard;
}

struct FDDSFileHeader
{
	DWORD Magic;
	DDSURFACEDESC2  desc;
};

void UTextureFactory::StaticConstructor()
{
	guard(UTextureFactory::StaticConstructor);

	SupportedClass = UTexture::StaticClass();
	new(Formats)FString(TEXT("bmp;Bitmap files;pcx;PC Painbrush files"));
	bCreateNew = 0;

	unguard;
}
UTextureFactory::UTextureFactory()
{
	guard(UTextureFactory::UTextureFactory);
	unguard;
}
TCHAR* GFile=NULL;
UObject* UTextureFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const BYTE*&		Buffer,
	const BYTE*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(UTextureFactory::FactoryCreate);
	UTexture* Texture = NULL;

	const FTGAFileHeader*    TGA   = (FTGAFileHeader *)Buffer;
	const FUPTFileHeader*    UPT   = (FUPTFileHeader *)Buffer;
	const FPCXFileHeader*    PCX   = (FPCXFileHeader *)Buffer;
	const FBitmapFileHeader* bmf   = (FBitmapFileHeader *)(Buffer + 0);
    const FBitmapInfoHeader* bmhdr = (FBitmapInfoHeader *)(Buffer + sizeof(FBitmapFileHeader));
	const FDDSFileHeader*	 DDS   = (FDDSFileHeader *)Buffer;
	
	// Validate it. 
	INT Length = BufferEnd - Buffer;

	// UPT  - Unreal Painter Tool format.	
	UBOOL bIsUPT = false;
	DWORD UPTSeed = 0;
	if( Length >= (sizeof(FTGAFileHeader) + 8 ) && appStricmp(TEXT("UPT"), Type) == 0) 
	{
		UPTSeed = UPT->seed;		
		TGA = (FTGAFileHeader*) ( (BYTE*)UPT+8 ); // Skip the UPT header 
		bIsUPT = true; // Know to de-scramble bitmap rows after loading.
		for( INT i=0; i< sizeof(FTGAFileHeader); i++ )
		{
			((BYTE*)TGA)[i] = 255 ^ ((BYTE*)TGA)[i];
		}		
	}	
	
	//
	// BMP
	//
    if( (Length>=sizeof(FBitmapFileHeader)+sizeof(FBitmapInfoHeader)) && Buffer[0]=='B' && Buffer[1]=='M' &&
        appStricmp(TEXT("bmp"), Type) == 0) //amb
    {
		if( (bmhdr->biWidth&(bmhdr->biWidth-1)) || (bmhdr->biHeight&(bmhdr->biHeight-1)) )
		{
			Warn->Logf( TEXT("Texture dimensions are not powers of two") );
			return NULL;
		}
		if( bmhdr->biCompression != BCBI_RGB )
		{
			Warn->Logf( TEXT("RLE compression of BMP images not supported") );
			return NULL;
		}
		if( bmhdr->biPlanes==1 && bmhdr->biBitCount==8 )
		{
			// Do palette.
			UPalette* Palette = new( InParent, NAME_None, RF_Public )UPalette;
			const BYTE* bmpal = (BYTE*)Buffer + sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader);
			Palette->Colors.Empty();

			UBOOL UseAlpha = 0;
			for( INT i=0; i<Min<INT>(NUM_PAL_COLORS,bmhdr->biClrUsed?bmhdr->biClrUsed:NUM_PAL_COLORS); i++ )
				if( bmpal[i*4+3] != 0 )
				{
					UseAlpha = 1;
					break;
				}
	
			if( UseAlpha )
				Flags = (Flags | PF_AlphaTexture) & ~PF_Masked;

			// Set texture properties.			
			Texture = CastChecked<UTexture>(StaticConstructObject( Class, InParent, Name, Flags ) );
			Texture->Init( bmhdr->biWidth, bmhdr->biHeight );
			Texture->PostLoad();

			for( INT i=0; i<Min<INT>(NUM_PAL_COLORS,bmhdr->biClrUsed?bmhdr->biClrUsed:NUM_PAL_COLORS); i++ )
				new( Palette->Colors )FColor( bmpal[i*4+2], bmpal[i*4+1], bmpal[i*4+0], UseAlpha ? bmpal[i*4+3] : 255 );
			while( Palette->Colors.Num()<NUM_PAL_COLORS )
				new(Palette->Colors)FColor(0,0,0,255);
			Texture->Palette = Palette->ReplaceWithExisting();

			// Copy upside-down scanlines.
			for( INT y=0; y<(INT)bmhdr->biHeight; y++ )
				appMemcpy
				(
					&Texture->Mips(0).DataArray((bmhdr->biHeight - 1 - y) * bmhdr->biWidth),
					(BYTE*)Buffer + bmf->bfOffBits + y * Align(bmhdr->biWidth,4),
					bmhdr->biWidth
				);
		}
		else if( bmhdr->biPlanes==1 && bmhdr->biBitCount==24 )
		{
			// Set texture properties.
			Texture = CastChecked<UTexture>(StaticConstructObject( Class, InParent, Name, Flags ) );
			Texture->Format = TEXF_RGBA8;
			Texture->Init( bmhdr->biWidth, bmhdr->biHeight );
			Texture->PostLoad();

			// Copy upside-down scanlines.
			const BYTE* Ptr = (BYTE*)Buffer + bmf->bfOffBits;
			for( INT y=0; y<(INT)bmhdr->biHeight; y++ )
			{
				BYTE* DestPtr = &Texture->Mips(0).DataArray((bmhdr->biHeight - 1 - y) * bmhdr->biWidth * 4);
				BYTE* SrcPtr = (BYTE*) &Ptr[y * Align(bmhdr->biWidth * 3, 4)];
				for( INT x=0; x<(INT)bmhdr->biWidth; x++ )
				{
					*DestPtr++ = *SrcPtr++;
					*DestPtr++ = *SrcPtr++;
					*DestPtr++ = *SrcPtr++;
					*DestPtr++ = 0xFF;
				}
			}
		}
		else if( bmhdr->biPlanes==1 && bmhdr->biBitCount==16 )
		{
			Texture = CastChecked<UTexture>(StaticConstructObject( Class, InParent, Name, Flags ) );
			Texture->Format = TEXF_G16;
			Texture->Init( bmhdr->biWidth, bmhdr->biHeight );
			Texture->PostLoad();

			// Copy upside-down scanlines.
			const BYTE* Ptr = (BYTE*)Buffer + bmf->bfOffBits;
			for( INT y=0; y<(INT)bmhdr->biHeight; y++ )
			{
				_WORD* DestPtr = (_WORD*)&Texture->Mips(0).DataArray((bmhdr->biHeight - 1 - y) * bmhdr->biWidth * 2);
				_WORD* SrcPtr = (_WORD*) &Ptr[y * bmhdr->biWidth * 2];
				appMemcpy( DestPtr, SrcPtr, (INT)bmhdr->biWidth * 2 );
			}
		}
		else
		{
            Warn->Logf( TEXT("BMP uses an unsupported format (%i/%i)"), bmhdr->biPlanes, bmhdr->biBitCount );
            return NULL;
		}
    }
	//
	// PCX
	//
	else if( Length >= sizeof(FPCXFileHeader) && PCX->Manufacturer==10 &&
             appStricmp(TEXT("pcx"), Type) == 0) //amb
	{
		INT NewU = PCX->XMax + 1 - PCX->XMin;
		INT NewV = PCX->YMax + 1 - PCX->YMin;
		if( (NewU&(NewU-1)) || (NewV&(NewV-1)) )
		{
			Warn->Logf( TEXT("Texture dimensions are not powers of two") );
			return NULL;
		}
		else if( PCX->NumPlanes==1 && PCX->BitsPerPixel==8 )
		{
			// Set texture properties.
			Texture = CastChecked<UTexture>(StaticConstructObject( Class, InParent, Name, Flags ) );
			Texture->Init( NewU, NewV );
			Texture->PostLoad();

			// Import it.
			BYTE* DestPtr	= &Texture->Mips(0).DataArray(0);
			BYTE* DestEnd	= DestPtr + Texture->Mips(0).DataArray.Num();
			Buffer += 128;
			while( DestPtr < DestEnd )
			{
				BYTE Color = *Buffer++;
				if( (Color & 0xc0) == 0xc0 )
				{
					INT RunLength = Color & 0x3f;
					Color     = *Buffer++;
					appMemset( DestPtr, Color, Min(RunLength,(INT)(DestEnd - DestPtr)) );
					DestPtr  += RunLength;
				}
				else *DestPtr++ = Color;
			}

			// Do the palette.
			UPalette* Palette = new( InParent, NAME_None, RF_Public )UPalette;
			BYTE* PCXPalette = (BYTE *)(BufferEnd - NUM_PAL_COLORS * 3);
			Palette->Colors.Empty();
			for( INT i=0; i<NUM_PAL_COLORS; i++ )
				new(Palette->Colors)FColor(PCXPalette[i*3+0],PCXPalette[i*3+1],PCXPalette[i*3+2],255);
			Texture->Palette = Palette->ReplaceWithExisting();
		}
		else if( PCX->NumPlanes==3 && PCX->BitsPerPixel==8 )
		{
			// Set texture properties.
			Texture = CastChecked<UTexture>(StaticConstructObject( Class, InParent, Name, Flags ) );
			Texture->Format = TEXF_RGBA8;
			Texture->Init( NewU, NewV );
			Texture->PostLoad();

			// Copy upside-down scanlines.
			Buffer += 128;
			INT CountU = Min<INT>(PCX->BytesPerLine,NewU);
			TArray<BYTE>& Dest = Texture->Mips(0).DataArray;
			for( INT i=0; i<NewV; i++ )
			{

				// We need to decode image one line per time building RGB image color plane by color plane.
                INT RunLength, Overflow=0;
                BYTE Color=0;
				for( INT ColorPlane=2; ColorPlane>=0; ColorPlane-- )
				{
					for( INT j=0; j<CountU; j++ )
					{
						if(!Overflow)
						{
							Color = *Buffer++;
							if((Color & 0xc0) == 0xc0)
							{
								RunLength=Min ((Color&0x3f), CountU-j);
								Overflow=(Color&0x3f)-RunLength;
								Color=*Buffer++;
							}
							else
								RunLength = 1;
						}
						else
						{
							RunLength=Min (Overflow, CountU-j);
							Overflow=Overflow-RunLength;
						}
 
						for( INT k=j; k<j+RunLength; k++ )
							Dest( (i*NewU+k)*4 + ColorPlane ) = Color;
						j+=RunLength-1;
					}
				}				
			}
		}
		else
		{
            Warn->Logf( TEXT("PCX uses an unsupported format (%i/%i)"), PCX->NumPlanes, PCX->BitsPerPixel );
            return NULL;
		}
	}
	//
	// TGA & UPT
	//
	else if( Length >= sizeof(FTGAFileHeader) &&
             ( ( appStricmp(TEXT("tga"), Type) == 0) ||
			   ( appStricmp(TEXT("upt"), Type) == 0) )
			 )
	{
		if(TGA->ImageTypeCode == 10) // 10 = RLE compressed 
		{			
			// RLE compression: CHUNKS: 1 -byte header, high bit 0 = raw, 1 = compressed
			// bits 0-6 are a 7-bit count; count+1 = number of raw pixels following, or rle pixels to be expanded. 

			if( TGA->BitsPerPixel == 32 )
			{
				Texture = CastChecked<UTexture>(StaticConstructObject(Class,InParent,Name,Flags));
				Texture->Format = TEXF_RGBA8;
				Texture->Init(TGA->Width,TGA->Height);
				Texture->PostLoad();

				BYTE*	IdData = (BYTE*)TGA + sizeof(FTGAFileHeader); 
				BYTE*	ColorMap = IdData + TGA->IdFieldLength;
				BYTE*	ImageData = (BYTE*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);				
				DWORD*	TextureData = (DWORD*) &Texture->Mips(0).DataArray(0);
				BYTE    Pixel[4];
				INT     RLERun = 0;
				INT     RAWRun = 0;

				for(INT Y = TGA->Height-1; Y >=0; Y--) // Y-flipped.
				{					
					for(INT X = 0;X < TGA->Width;X++)
					{						
						if( RLERun > 0 )
							RLERun--;  // reuse current Pixel data.
						else if( RAWRun == 0 ) // new raw pixel or RLE-run.
						{
							BYTE RLEChunk = *(ImageData++);							
							if( RLEChunk & 0x80 )
							{
								RLERun = ( RLEChunk & 0x7F ) + 1;
								RAWRun = 1;
							}
							else
							{
								RAWRun = ( RLEChunk & 0x7F ) + 1;
							}
						}							
						// Retrieve new pixel data - raw run or single pixel for RLE stretch.
						if( RAWRun > 0 )
						{
							*(DWORD*)&Pixel = *(DWORD*)ImageData; // RGBA 32-bit dword.
							ImageData += 4;
							RAWRun--;
							RLERun--;
						}
						// Store.
						*( (TextureData + Y*TGA->Width)+X ) = *(DWORD*)&Pixel;
					}
				}
				// UPT - descrambling.
				if( bIsUPT )
				{					
					DescrambleUPT( TextureData, UPTSeed, TGA->Width, TGA->Height );
				}				
			}
			else if( TGA->BitsPerPixel == 24 )
			{	
				Texture = CastChecked<UTexture>(StaticConstructObject(Class,InParent,Name,Flags));
				Texture->Format = TEXF_RGBA8;
				Texture->Init(TGA->Width,TGA->Height);
				Texture->PostLoad();

				BYTE*	IdData = (BYTE*)TGA + sizeof(FTGAFileHeader); 
				BYTE*	ColorMap = IdData + TGA->IdFieldLength;
				BYTE*	ImageData = (BYTE*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);
				DWORD*	TextureData = (DWORD*) &Texture->Mips(0).DataArray(0);
				BYTE    Pixel[4];
				INT     RLERun = 0;
				INT     RAWRun = 0;
				
				for(INT Y = TGA->Height-1; Y >=0; Y--) // Y-flipped.
				{					
					for(INT X = 0;X < TGA->Width;X++)
					{						
						if( RLERun > 0 )
							RLERun--;  // reuse current Pixel data.
						else if( RAWRun == 0 ) // new raw pixel or RLE-run.
						{
							BYTE RLEChunk = *(ImageData++);
							if( RLEChunk & 0x80 )
							{
								RLERun = ( RLEChunk & 0x7F ) + 1;
								RAWRun = 1;
							}
							else
							{
								RAWRun = ( RLEChunk & 0x7F ) + 1;
							}
						}							
						// Retrieve new pixel data - raw run or single pixel for RLE stretch.
						if( RAWRun > 0 )
						{
							Pixel[0] = *(ImageData++);
							Pixel[1] = *(ImageData++);
							Pixel[2] = *(ImageData++);
							Pixel[3] = 255;
							RAWRun--;
							RLERun--;
						}
						// Store.
						*( (TextureData + Y*TGA->Width)+X ) = *(DWORD*)&Pixel;
					}
				}
				// UPT - descrambling.
				if( bIsUPT )
				{					
					DescrambleUPT( TextureData, UPTSeed, TGA->Width, TGA->Height );
				}				
			}
			else if( TGA->BitsPerPixel == 16 )
			{
				Texture = CastChecked<UTexture>(StaticConstructObject(Class,InParent,Name,Flags));
				Texture->Format = TEXF_G16;
				Texture->Init(TGA->Width,TGA->Height);
				Texture->PostLoad();
				
				BYTE*	IdData = (BYTE*)TGA + sizeof(FTGAFileHeader);
				BYTE*	ColorMap = IdData + TGA->IdFieldLength;				
				_WORD*	ImageData = (_WORD*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);
				_WORD*	TextureData = (_WORD*) &Texture->Mips(0).DataArray(0);
				_WORD    Pixel = 0;
				INT     RLERun = 0;
				INT     RAWRun = 0;

				for(INT Y = TGA->Height-1; Y >=0; Y--) // Y-flipped.
				{					
					for( INT X=0;X<TGA->Width;X++ )
					{						
						if( RLERun > 0 )
							RLERun--;  // reuse current Pixel data.
						else if( RAWRun == 0 ) // new raw pixel or RLE-run.
						{
							BYTE RLEChunk =  *((BYTE*)ImageData);
							ImageData = (_WORD*)(((BYTE*)ImageData)+1);
							if( RLEChunk & 0x80 )
							{
								RLERun = ( RLEChunk & 0x7F ) + 1;
								RAWRun = 1;
							}
							else
							{
								RAWRun = ( RLEChunk & 0x7F ) + 1;
							}
						}							
						// Retrieve new pixel data - raw run or single pixel for RLE stretch.
						if( RAWRun > 0 )
						{ 
							Pixel = *(ImageData++);
							RAWRun--;
							RLERun--;
						}
						// Store.
						*( (TextureData + Y*TGA->Width)+X ) = Pixel;
					}
				}
			}
			else
			{
				Warn->Logf(TEXT("TGA uses an unsupported rle-compressed bit-depth: %u"),TGA->BitsPerPixel);
				return NULL;
			}
		}
		else if(TGA->ImageTypeCode == 2) // 2 = Uncompressed 
		{
			if(TGA->BitsPerPixel == 32)
			{
				Texture = CastChecked<UTexture>(StaticConstructObject(Class,InParent,Name,Flags));
				Texture->Format = TEXF_RGBA8;
				Texture->Init(TGA->Width,TGA->Height);
				Texture->PostLoad();

				BYTE*	IdData = (BYTE*)TGA + sizeof(FTGAFileHeader);
				BYTE*	ColorMap = IdData + TGA->IdFieldLength;
				DWORD*	ImageData = (DWORD*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);
				DWORD*	TextureData = (DWORD*) &Texture->Mips(0).DataArray(0);

				for(INT Y = 0;Y < TGA->Height;Y++)
					appMemcpy(TextureData + Y * TGA->Width,ImageData + (TGA->Height - Y - 1) * TGA->Width,TGA->Width * 4);

				// UPT - descrambling.
				if( bIsUPT )
				{						
					DescrambleUPT( TextureData, UPTSeed, TGA->Width, TGA->Height );
				}
			}
			else if(TGA->BitsPerPixel == 16)
			{
				Texture = CastChecked<UTexture>(StaticConstructObject(Class,InParent,Name,Flags));
				Texture->Format = TEXF_G16;
				Texture->Init(TGA->Width,TGA->Height);
				Texture->PostLoad();

				BYTE*	IdData = (BYTE*)TGA + sizeof(FTGAFileHeader);
				BYTE*	ColorMap = IdData + TGA->IdFieldLength;
				_WORD*	ImageData = (_WORD*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);
				_WORD*	TextureData = (_WORD*) &Texture->Mips(0).DataArray(0);

				// Monochrome 16-bit format - terrain heightmaps.
				for(INT Y = 0;Y < TGA->Height;Y++)
					appMemcpy(TextureData + Y * TGA->Width,ImageData + (TGA->Height - Y - 1) * TGA->Width,TGA->Width * 2);
				
			}            
            else if(TGA->BitsPerPixel == 24)
            {
                Texture = CastChecked<UTexture>(StaticConstructObject(Class,InParent,Name,Flags));
				Texture->Format = TEXF_RGBA8;
				Texture->Init(TGA->Width,TGA->Height);
				Texture->PostLoad();

				BYTE*	IdData = (BYTE*)TGA + sizeof(FTGAFileHeader);
				BYTE*	ColorMap = IdData + TGA->IdFieldLength;
				BYTE*	ImageData = (BYTE*) (ColorMap + (TGA->ColorMapEntrySize + 4) / 8 * TGA->ColorMapLength);
				DWORD*	TextureData = (DWORD*) &Texture->Mips(0).DataArray(0);
                BYTE    Pixel[4];

				for(INT Y = 0;Y < TGA->Height;Y++)
                {
                    for(INT X = 0;X < TGA->Width;X++)
                    {
						Pixel[0] = *(( ImageData+( TGA->Height-Y-1 )*TGA->Width*3 )+X*3+0);
						Pixel[1] = *(( ImageData+( TGA->Height-Y-1 )*TGA->Width*3 )+X*3+1);
						Pixel[2] = *(( ImageData+( TGA->Height-Y-1 )*TGA->Width*3 )+X*3+2);
                        Pixel[3] = 255;
                        *((TextureData+Y*TGA->Width)+X) = *(DWORD*)&Pixel;
                    }
                }
				// UPT - descrambling.
				if( bIsUPT )
				{					
					DescrambleUPT( TextureData, UPTSeed, TGA->Width, TGA->Height );
				}				
            }            
			else
			{
				Warn->Logf(TEXT("TGA uses an unsupported bit-depth: %u"),TGA->BitsPerPixel);
				return NULL;
			}
		}
		else
		{
			Warn->Logf(TEXT("TGA is an unsupported type: %u"),TGA->ImageTypeCode);
			return NULL;
		}
	}
	//
	// DDS
	//
	else if( Length >= sizeof(FDDSFileHeader) && DDS->Magic==0x20534444 &&
             appStricmp(TEXT("dds"), Type) == 0) //amb
	{
		if(!(DDS->desc.dwFlags&DDSD_LINEARSIZE))
		{
			Warn->Logf( TEXT("DDSD_LINEARSIZE flag is not set") );
			return NULL;
		}
		DWORD FourCC = DDS->desc.ddpfPixelFormat.dwFourCC;
		if( (FourCC&0xFF) == 'D' && ((FourCC>>8)&0xFF) == 'X' && ((FourCC>>16)&0xFF) == 'T' )
		{
			Texture = CastChecked<UTexture>(StaticConstructObject(Class,InParent,Name,Flags));
			Texture->Format = TEXF_NODATA;

			switch( (FourCC>>24)&0xFF )
			{
			case '1':
				Texture->Format = TEXF_DXT1;
				break;
			case '3':
				Texture->Format = TEXF_DXT3;
				break;
			case '5':
				Texture->Format = TEXF_DXT5;
				break;
			default:
				Warn->Logf( TEXT("DXT%c is not currently supported."), (FourCC>>24)&0xFF );
				return NULL;
			}

			Texture->Init(DDS->desc.dwWidth,DDS->desc.dwHeight);
			Texture->PostLoad();	
			Texture->Mips.Empty();

			INT w = DDS->desc.dwWidth;
			INT h = DDS->desc.dwHeight;
			INT NumMipmaps = DDS->desc.dwMipMapCount>0 ? DDS->desc.dwMipMapCount : 1;
			Buffer += sizeof(FDDSFileHeader);
			Length -= sizeof(FDDSFileHeader);
			for(INT m=0; m<NumMipmaps; m++ )
			{	
				INT wtmp = Max<INT>( w, 4 );
				INT htmp = Max<INT>( h, 4 );
				INT MipmapSize = Texture->Format==TEXF_DXT1 ? wtmp*htmp/2 : wtmp*htmp;
				FMipmap* NewMipmap = new(Texture->Mips)FMipmap( Max(Texture->UBits-m,0), Max(Texture->VBits-m,0), MipmapSize );
				check( MipmapSize <= Length );
				appMemcpy( &NewMipmap->DataArray(0), Buffer, MipmapSize );
				Buffer += MipmapSize;
				Length -= MipmapSize;
				w = (w+1)/2;
				h = (h+1)/2;
			}
		}
		else
		{
			Warn->Logf( TEXT("Format %04x is not DXTn"), FourCC );
			return NULL;
		}
	}
	else
	{
		// Unknown format.
        Warn->Logf( TEXT("Bad image format for texture import") );
        return NULL;
 	}

	// See if part of an animation sequence.
	check(Texture);
	if( appStrlen(Texture->GetName())>=4 )
	{
		TCHAR Temp[256];
		appStrcpy( Temp, Texture->GetName() );
		TCHAR* End = Temp + appStrlen(Temp) - 4;
		if( End[0]=='_' && appToUpper(End[1])=='A' && appIsDigit(End[2]) && appIsDigit(End[3]) )
		{
			INT i = appAtoi( End+2 );
			debugf( NAME_Log, TEXT("Texture animation frame %i: %s"), i, Texture->GetName() );
			if( i>0 )
			{
				appSprintf( End+2, TEXT("%02i"), i-1 );
				UTexture* Other = FindObject<UTexture>( Texture->GetOuter(), Temp );
				if( Other )
				{
					Other->AnimNext = Texture;
					debugf( NAME_Log, TEXT("   Linked to previous: %s"), Other->GetName() );
				}
			}
			if( i<99 )
			{
				appSprintf( End+2, TEXT("%02i"), i+1 );
				UTexture* Other = FindObject<UTexture>( Texture->GetOuter(), Temp );
				if( Other )
				{
					Texture->AnimNext = Other;
					debugf( NAME_Log, TEXT("   Linked to next: %s"), Other->GetName() );
				}
			}
		}
	}

	return Texture;
	unguard;
}
IMPLEMENT_CLASS(UTextureFactory);

/*------------------------------------------------------------------------------
	UTextureExporterPCX implementation.
------------------------------------------------------------------------------*/

void UTextureExporterPCX::StaticConstructor()
{
	guard(UTextureExporterPCX::StaticConstructor);

	SupportedClass = UTexture::StaticClass();
	new(Formats)FString(TEXT("PCX"));

	unguard;
}
UBOOL UTextureExporterPCX::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn )
{
	guard(UTextureExporterPCX::ExportBinary);
	UTexture* Texture = CastChecked<UTexture>( Object );

	// Set all PCX file header properties.
	FPCXFileHeader PCX;
	appMemzero( &PCX, sizeof(PCX) );
	PCX.Manufacturer	= 10;
	PCX.Version			= 05;
	PCX.Encoding		= 1;
	PCX.BitsPerPixel	= 8;
	PCX.XMin			= 0;
	PCX.YMin			= 0;
	PCX.XMax			= Texture->USize-1;
	PCX.YMax			= Texture->VSize-1;
	PCX.XDotsPerInch	= Texture->USize;
	PCX.YDotsPerInch	= Texture->VSize;
	PCX.BytesPerLine	= Texture->USize;
	PCX.PaletteType		= 0;
	PCX.HScreenSize		= 0;
	PCX.VScreenSize		= 0;

	// Figure out format.
	ETextureFormat Format      = (ETextureFormat)Texture->Format;
	Texture->Mips(0).DataArray.Load();

	// Copy all RLE bytes.
	BYTE RleCode=0xc1;
	if( Format==TEXF_RGBA8 )
	{
		PCX.NumPlanes = 3;
		Ar << PCX;
		for( INT Line=0; Line<Texture->VSize; Line++ )
		{
			for( INT ColorPlane = 2; ColorPlane >= 0; ColorPlane-- )
			{
				BYTE* ScreenPtr = &Texture->Mips(0).DataArray(0) + (Line * Texture->USize * 4) + ColorPlane;
				for( INT Row=0; Row<Texture->USize; Row++ )
				{
					if( (*ScreenPtr&0xc0)==0xc0 )
						Ar << RleCode;
					Ar << *ScreenPtr;
					ScreenPtr += 4;
				}
			}
		}
		return 1;
	}
	else if( Format==TEXF_P8 )
	{
		PCX.NumPlanes = 1;
		Ar << PCX;
		BYTE* ScreenPtr = &Texture->Mips(0).DataArray(0);
		for( INT i=0; i<Texture->USize*Texture->VSize; i++ )
		{
			if( (*ScreenPtr&0xc0)==0xc0 )
				Ar << RleCode;
			Ar << *ScreenPtr++;
		}

		// Write PCX trailer then palette.
		BYTE Extra = 12;
		Ar << Extra;
		FColor* Colors = Texture->GetColors();
		for( INT i=0; i<NUM_PAL_COLORS; i++ )
			Ar << Colors[i].R << Colors[i].G << Colors[i].B;
		return 1;
	}
	else return 0;
	unguard;
}
IMPLEMENT_CLASS(UTextureExporterPCX);

/*------------------------------------------------------------------------------
	UTextureExporterBMP implementation.
------------------------------------------------------------------------------*/

void UTextureExporterBMP::StaticConstructor()
{
	guard(UTextureExporterBMP::StaticConstructor);

	SupportedClass = UTexture::StaticClass();
	new(Formats)FString(TEXT("BMP"));

	unguard;
}
UBOOL UTextureExporterBMP::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn )
{
	guard(UTextureExporterBMP::ExportBinary);
	UTexture* Texture = CastChecked<UTexture>( Object );

	// Figure out format.
	ETextureFormat Format = (ETextureFormat)Texture->Format;
	Texture->Mips(0).DataArray.Load();

	FBitmapFileHeader bmf;
	FBitmapInfoHeader bmhdr;

	// File header.
	bmf.bfType      = 'B' + (256*(INT)'M');
    bmf.bfReserved1 = 0;
    bmf.bfReserved2 = 0;
	INT biSizeImage;

	switch( Format )
	{
	case TEXF_RGBA8:
		biSizeImage		= Texture->USize * Texture->VSize * 3;
		bmf.bfOffBits   = sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader);
		bmhdr.biBitCount= 24;
		break;
	case TEXF_P8:
		biSizeImage		= Texture->USize * Texture->VSize * 1;
		bmf.bfOffBits   = sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader) + 256*4;
		bmhdr.biBitCount= 8;
		break;
	case TEXF_G16:
		biSizeImage		= Texture->USize * Texture->VSize * 2;
		bmf.bfOffBits   = sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader);
		bmhdr.biBitCount= 16;
		break;
	default:
		return 0;
	}
	bmf.bfSize		= bmf.bfOffBits + biSizeImage;
	Ar << bmf;

	// Info header.
    bmhdr.biSize          = sizeof(FBitmapInfoHeader);
    bmhdr.biWidth         = Texture->USize;
    bmhdr.biHeight        = Texture->VSize;
    bmhdr.biPlanes        = 1;
    bmhdr.biCompression   = BCBI_RGB;
    bmhdr.biSizeImage     = biSizeImage;
    bmhdr.biXPelsPerMeter = 0;
    bmhdr.biYPelsPerMeter = 0;
    bmhdr.biClrUsed       = 0;
    bmhdr.biClrImportant  = 0;
	Ar << bmhdr;

	switch( Format )
	{
	case TEXF_RGBA8:
		{
			// Upside-down scanlines.
			for( INT i=Texture->VSize-1; i>=0; i-- )
			{
				BYTE* ScreenPtr = &Texture->Mips(0).DataArray(i*Texture->USize*4);
				for( INT j=Texture->USize; j>0; j-- )
				{
					Ar << *ScreenPtr++;
					Ar << *ScreenPtr++;
					Ar << *ScreenPtr++;
					*ScreenPtr++;
				}
			}
		}
		return 1;
	case TEXF_P8:
		{
			// Palette.
			FColor* Colors = Texture->GetColors();
			for( INT i=0; i<256; i++ )
				Ar << Colors[i].B << Colors[i].G << Colors[i].R << Colors[i].A;

			// Upside-down scanlines.
			for( INT i=Texture->VSize-1; i>=0; i-- )
			{
				Ar.Serialize( &Texture->Mips(0).DataArray(i*Texture->USize), Texture->USize );
				if( Align(Texture->USize,4)-Texture->USize )
				{
					INT Padding=0;
					Ar.Serialize( &Padding, Align(Texture->USize,4)-Texture->USize );
				}
			}
		}
		return 1;
	case TEXF_G16:
		{
			for( INT i=Texture->VSize-1; i>=0; i-- )
				Ar.Serialize( &Texture->Mips(0).DataArray(i*Texture->USize*2), Texture->USize*2 );
		}
		return 1;
	}
	return 0;
	unguard;
}
IMPLEMENT_CLASS(UTextureExporterBMP);

/*------------------------------------------------------------------------------
	UTextureExporterTGA implementation.
------------------------------------------------------------------------------*/

void UTextureExporterTGA::StaticConstructor()
{
	guard(UTextureExporterTGA::StaticConstructor);

	SupportedClass = UTexture::StaticClass();
	new(Formats)FString(TEXT("TGA"));

	unguard;
}
UBOOL UTextureExporterTGA::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn )
{
	guard(UTextureExporterTGA::ExportBinary);
	UTexture* Texture = CastChecked<UTexture>( Object );

	if( Texture->Format == TEXF_RGBA8 )
	{
		Texture->Mips(0).DataArray.Load();

		FTGAFileHeader TGA;
		appMemzero( &TGA, sizeof(TGA) );
		TGA.ImageTypeCode = 2;
		TGA.BitsPerPixel = 32;
		TGA.Height = Texture->VSize;
		TGA.Width = Texture->USize;

		Ar.Serialize( &TGA, sizeof(TGA) );

		for( INT Y=0;Y < Texture->VSize;Y++ )
			Ar.Serialize( &Texture->Mips(0).DataArray( (Texture->VSize - Y - 1) * Texture->USize * 4 ), Texture->USize * 4 );

		FTGAFileFooter Ftr;
		appMemzero( &Ftr, sizeof(Ftr) );
		appMemcpy( Ftr.Signature, appToAnsi(TEXT("TRUEVISION-XFILE")), 16 );
		Ftr.TrailingPeriod = '.';
		Ar.Serialize( &Ftr, sizeof(Ftr) );

		return 1;
	}
	return 0;
	unguard;
}
IMPLEMENT_CLASS(UTextureExporterTGA);


/*------------------------------------------------------------------------------
UTextureExporterUPT implementation.
------------------------------------------------------------------------------*/

void UTextureExporterUPT::StaticConstructor()
{
	guard(UTextureExporterUPT::StaticConstructor);

	SupportedClass = UTexture::StaticClass();
	new(Formats)FString(TEXT("UPT"));

	unguard;
}
UBOOL UTextureExporterUPT::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn )
{
	guard(UTextureExporterUPT::ExportBinary);
	UTexture* Texture = CastChecked<UTexture>( Object );

	if( Texture->Format == TEXF_RGBA8 )
	{
		Texture->Mips(0).DataArray.Load();

		FUPTFileHeader UPT;
		appMemzero( &UPT, sizeof(UPT) );

		// 'UPT/0' header tag.
		UPT.UPTTag[0] = 0x55;
		UPT.UPTTag[1] = 0x50;
		UPT.UPTTag[2] = 0x54;
		UPT.UPTTag[3] = 0x00;

		UPT.ImageTypeCode = 2;
		UPT.BitsPerPixel = 32;
		UPT.Height = Texture->VSize;
		UPT.Width = Texture->USize;	
		#ifdef __GNUC__
            UPT.seed = appFrand()*appFrand()*(FLOAT)0x7FFF000000000000ll;
        #else
		    UPT.seed = appFrand()*appFrand()*(FLOAT)0x7FFF000000000000;
        #endif

		// Flip bits.
		for( INT i=8; i<sizeof(FUPTFileHeader); i++ )
			((BYTE*)&UPT)[i] = 255 ^ ((BYTE*)&UPT)[i];
		Ar.Serialize( &UPT, sizeof(UPT) );
		// Unflip.
		for( INT i=8; i<sizeof(FUPTFileHeader); i++ )
			((BYTE*)&UPT)[i] = 255 ^ ((BYTE*)&UPT)[i];

		// Copy, then scramble bitmap.
		TArray<BYTE> ExportData;
		Texture->Mips(0).DataArray.Load();
		ExportData.Add( Texture->Mips(0).DataArray.Num() );
		appMemcpy( &(ExportData(0)),&(Texture->Mips(0).DataArray(0)), Texture->Mips(0).DataArray.Num());
		ScrambleUPT( (DWORD*)&(ExportData(0)), UPT.seed, UPT.Width, UPT.Height );

		// Write out rows upside-down.
		for( INT Y=0;Y < Texture->VSize;Y++ ) 
			Ar.Serialize( &ExportData( (Texture->VSize - Y - 1) * Texture->USize * 4 ), Texture->USize * 4 );

		FTGAFileFooter Ftr;
		appMemzero( &Ftr, sizeof(Ftr) );// UPT footer signature is effectively "".
		Ftr.TrailingPeriod = '.';
		Ar.Serialize( &Ftr, sizeof(Ftr) );

		return 1;
	}
	return 0;
	unguard;
}
IMPLEMENT_CLASS(UTextureExporterUPT);


/*------------------------------------------------------------------------------
	UTextureExporterDDS implementation. - sjs
------------------------------------------------------------------------------*/
void UTextureExporterDDS::StaticConstructor()
{
	guard(UTextureExporterDDS::StaticConstructor);

	SupportedClass = UTexture::StaticClass();
	new(Formats)FString(TEXT("DDS"));

	unguard;
}

#ifndef DDSD_MIPMAPCOUNT
#define DDSD_MIPMAPCOUNT 131072
#endif
UBOOL UTextureExporterDDS::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn )
{
	guard(UTextureExporterDDS::ExportBinary);
	UTexture* Texture = CastChecked<UTexture>( Object );

	if ( !(Texture->Format == TEXF_DXT1 || Texture->Format == TEXF_DXT3 || Texture->Format == TEXF_DXT5) )
		return 0;


	FDDSFileHeader hdr;
	appMemset( &hdr.desc, 0, sizeof(DDSURFACEDESC2) );

	hdr.Magic				= 0x20534444;
	hdr.desc.dwFlags		= DDSD_LINEARSIZE|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT|DDSD_MIPMAPCOUNT|DDSD_CAPS;
	hdr.desc.dwSize			= sizeof(DDSURFACEDESC2);
	hdr.desc.dwWidth		= Texture->USize;
	hdr.desc.dwHeight		= Texture->VSize;
	hdr.desc.dwMipMapCount	= Texture->Mips.Num();

	if ( Texture->Format == TEXF_DXT1 )
		hdr.desc.ddpfPixelFormat.dwFourCC = ('D' | 'X'<<8 | 'T'<<16 | '1'<<24 );
	else if ( Texture->Format == TEXF_DXT3 )
		hdr.desc.ddpfPixelFormat.dwFourCC = ('D' | 'X'<<8 | 'T'<<16 | '3'<<24 );
	else if ( Texture->Format == TEXF_DXT5 )
		hdr.desc.ddpfPixelFormat.dwFourCC = ('D' | 'X'<<8 | 'T'<<16 | '5'<<24 );

	Ar.Serialize( &hdr, sizeof(hdr) );

	for(DWORD m=0; m<hdr.desc.dwMipMapCount; m++ )
	{
		Texture->Mips(m).DataArray.Load();
		Ar.Serialize( &Texture->Mips(m).DataArray(0), Texture->Mips(m).DataArray.Num() );
		Texture->Mips(m).DataArray.Unload();
	}

	Ar.Close();

	return 1;
	
	unguard;
}
IMPLEMENT_CLASS(UTextureExporterDDS);

/*------------------------------------------------------------------------------
	UFontFactory.
------------------------------------------------------------------------------*/

//
//	Fast pixel-lookup.
//
static inline BYTE AT( BYTE* Screen, int SXL, int X, int Y )
{
	return Screen[X+Y*SXL];
}

//
// Codepage 850 -> Latin-1 mapping table:
//
BYTE FontRemap[256] = 
{
	  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
	 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
	 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
	 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,

	 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
	 80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
	 96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
	112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,

	000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,000,
	032,173,184,156,207,190,124,245,034,184,166,174,170,196,169,238,
	248,241,253,252,239,230,244,250,247,251,248,175,172,171,243,168,

	183,181,182,199,142,143,146,128,212,144,210,211,222,214,215,216,
	209,165,227,224,226,229,153,158,157,235,233,234,154,237,231,225,
	133,160,131,196,132,134,145,135,138,130,136,137,141,161,140,139,
	208,164,149,162,147,228,148,246,155,151,163,150,129,236,232,152,
};

//
//	Find the border around a font glyph that starts at x,y (it's upper
//	left hand corner).  If it finds a glyph box, it returns 0 and the
//	glyph 's length (xl,yl).  Otherwise returns -1.
//
static UBOOL ScanFontBox( UTexture* Texture, INT X, INT Y, INT& XL, INT& YL )
{
	guard(UFont_ScanFontBox);
	BYTE* Data = &Texture->Mips(0).DataArray(0);
	INT FontXL = Texture->USize;

	// Find x-length.
	INT NewXL = 1;
	while ( AT(Data,FontXL,X+NewXL,Y)==255 && AT(Data,FontXL,X+NewXL,Y+1)!=255 )
		NewXL++;

	if( AT(Data,FontXL,X+NewXL,Y)!=255 )
		return 0;

	// Find y-length.
	INT NewYL = 1;
	while( AT(Data,FontXL,X,Y+NewYL)==255 && AT(Data,FontXL,X+1,Y+NewYL)!=255 )
		NewYL++;

	if( AT(Data,FontXL,X,Y+NewYL)!=255 )
		return 0;

	XL = NewXL - 1;
	YL = NewYL - 1;

	return 1;
	unguard;
}

void UFontFactory::StaticConstructor()
{
	guard(UFontFactory::StaticConstructor);

	SupportedClass = UFont::StaticClass();

	unguard;
}
UFontFactory::UFontFactory()
{
	guard(UFontFactory::UFontFactory);
	unguard;
}

#define NUM_FONT_CHARS 256

UObject* UFontFactory::FactoryCreateBinary
(
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const BYTE*&		Buffer,
	const BYTE*			BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(UFontFactory::FactoryCreateBinary);
	check(Class==UFont::StaticClass());
	UFont* Font = new( InParent, Name, Flags )UFont;
	UTexture* Tex = CastChecked<UTexture>( UTextureFactory::FactoryCreateBinary( UTexture::StaticClass(), Font, NAME_None, 0, Context, Type, Buffer, BufferEnd, Warn ) );
	Font->Textures.AddItem(Tex);
	if( Tex )
	{
		// Init.
		Tex->bMasked = 1;
		BYTE* TextureData = &Tex->Mips(0).DataArray(0);
		Font->Characters.AddZeroed( NUM_FONT_CHARS );

		// Scan in all fonts, starting at glyph 32.
		INT i = 32;
		INT Y = 0;
		do
		{
			INT X = 0;
			while( AT(TextureData,Tex->USize,X,Y)!=255 && Y<Tex->VSize )
			{
				X++;
				if( X >= Tex->USize )
				{
					X = 0;
					if( ++Y >= Tex->VSize )
						break;
				}
			}

			// Scan all glyphs in this row.
			if( Y < Tex->VSize )
			{
				INT XL=0, YL=0, MaxYL=0;
				while( i<Font->Characters.Num() && ScanFontBox(Tex,X,Y,XL,YL) )
				{
					Font->Characters(i).StartU = X+1;
					Font->Characters(i).StartV = Y+1;
					Font->Characters(i).USize  = XL;
					Font->Characters(i).VSize  = YL;
					Font->Characters(i).TextureIndex = 0;
					X += XL + 1;
					i++;
					if( YL > MaxYL )
						MaxYL = YL;
				}
				Y += MaxYL + 1;
			}
		} while( i<Font->Characters.Num() && Y<Tex->VSize );

		// Cleanup font data.
		for( i=0; i<Tex->Mips.Num(); i++ )
			for( INT j=0; j<Tex->Mips(i).DataArray.Num(); j++ )
				if( Tex->Mips(i).DataArray(j)==255 )
					Tex->Mips(i).DataArray(j) = 0;

		// Remap old fonts.
		TArray<FFontCharacter> Old = Font->Characters;
		for( i=0; i<Font->Characters.Num(); i++ )
			Font->Characters(i) = Old(FontRemap[i]);
		return Font;
	}
	else return NULL;
	unguard;
}
IMPLEMENT_CLASS(UFontFactory);

/*------------------------------------------------------------------------------
	UMaterialFactory.
------------------------------------------------------------------------------*/


void UMaterialFactory::execConsoleCommand( FFrame& Stack, RESULT_DECL )
{
	guard(UMaterialFactory::execConsoleCommand);
	P_GET_STR(Cmd);
	GEditor->Exec( *Cmd );
	P_FINISH;
	unguard;
}
IMPLEMENT_CLASS(UMaterialFactory);

/*------------------------------------------------------------------------------
	The end.
------------------------------------------------------------------------------*/





















