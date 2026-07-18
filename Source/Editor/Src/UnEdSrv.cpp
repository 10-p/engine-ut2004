/*=============================================================================
	UnEdSrv.cpp: UEditorEngine implementation, the Unreal editing server
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EditorPrivate.h"
#include "UnPath.h"
#include "UnEdLevelStat.h"

#define NODECA 1

//
// FMemoryReader and FMemoryWriter are needed for texture duplication code
// !!vogel: GUglyHackFlags & 8
//
class FMemoryReader : public FArchive
{
public:
	FMemoryReader( TArray<BYTE>& InBytes )
	:	Bytes	( InBytes )
	,	Offset	( 0 )
	{
		ArIsLoading = ArIsTrans = 1;
	}
private:
	void Serialize( void* Data, INT Num )
	{
		checkSlow(Offset+Num<=Bytes.Num());
		appMemcpy( Data, &Bytes(Offset), Num );
		Offset += Num;
	}
	void Seek( INT InPos )
	{
		Offset = InPos;
	}
	INT Tell()
	{
		return Offset;
	}
	TArray<BYTE>& Bytes;
	INT Offset;
};
class FMemoryWriter : public FArchive
{
public:
	FMemoryWriter( TArray<BYTE>& InBytes )
	:	Bytes	( InBytes )
	,	Offset	( 0 )
	{
		ArIsSaving = ArIsTrans = 1;
	}
private:
	void Serialize( void* Data, INT Num )
	{
		//INT Index = Bytes.Add(Num);
		Bytes.Add(Num);
		appMemcpy( &Bytes(Offset), Data, Num );
		Offset+=Num;
	}
	void Seek( INT InPos )
	{
		Offset = InPos;
	}
	INT Tell()
	{
		return Offset;
	}
	TArray<BYTE>& Bytes;
	INT Offset;
};

extern TArray<FVertexHit> VertexHitList;
extern ENGINE_API FRebuildTools GRebuildTools;

//Batch Detail Texture Editing added by Legend on 4/12/2000
static UTexture* CurrentDetailTexture = 0;

void UEditorEngine::polygonDeleteMarkers()
{
	guard(polygonDeleteMarkers);

	if( !GEditor || !GEditor->Level ) return;

	for( INT i = 0 ; i < GEditor->Level->Actors.Num() ; i++ )
	{
		AActor* pActor = GEditor->Level->Actors(i);
		if( pActor && pActor->IsA(APolyMarker::StaticClass()) )
		{
			pActor->bDeleteMe = 0;	// Make sure they get destroyed!!
			GEditor->Level->DestroyActor( pActor );
		}
	}

	GEditor->RedrawLevel( GEditor->Level );
	GEditor->NoteSelectionChange( GEditor->Level );

	unguard;
}

void UEditorEngine::brushclipDeleteMarkers()
{
	guard(brushclipDeleteMarkers);

	if( !GEditor || !GEditor->Level ) return;

	for( INT i = 0 ; i < GEditor->Level->Actors.Num() ; i++ )
	{
		AActor* pActor = GEditor->Level->Actors(i);
		if( pActor && pActor->IsA(AClipMarker::StaticClass()) )
		{
			pActor->bDeleteMe = 0;	// Make sure they get destroyed!!
			GEditor->Level->DestroyActor( pActor );
		}
	}

	GEditor->RedrawLevel( GEditor->Level );
	GEditor->NoteSelectionChange( GEditor->Level );

	unguard;
}

//
// Builds a huge poly aligned with the specified plane.  This poly is
// carved up by the calling routine and used as a capping poly following a clip operation.
//

FPoly edBuildInfiniteFPoly( FPlane* InPlane )
{
	guard(edBuildInfiniteFPoly);

	FVector Axis1, Axis2;

	// Find two non-problematic axis vectors.
	InPlane->FindBestAxisVectors( Axis1, Axis2 );

	// Set up the FPoly.
	FPoly EdPoly;
	EdPoly.Init();
	EdPoly.NumVertices = 4;
	EdPoly.Normal.X    = InPlane->X;
	EdPoly.Normal.Y    = InPlane->Y;
	EdPoly.Normal.Z    = InPlane->Z;
	EdPoly.Base        = EdPoly.Normal * InPlane->W;
	EdPoly.Vertex[0]   = EdPoly.Base + Axis1*HALF_WORLD_MAX + Axis2*HALF_WORLD_MAX;
	EdPoly.Vertex[1]   = EdPoly.Base - Axis1*HALF_WORLD_MAX + Axis2*HALF_WORLD_MAX;
	EdPoly.Vertex[2]   = EdPoly.Base - Axis1*HALF_WORLD_MAX - Axis2*HALF_WORLD_MAX;
	EdPoly.Vertex[3]   = EdPoly.Base + Axis1*HALF_WORLD_MAX - Axis2*HALF_WORLD_MAX;

	return EdPoly;
	unguard;
}

// Creates a giant brush, aligned with the specified plane.
void brushclipBuildGiantBrush( ABrush* GiantBrush, FPlane Plane, ABrush* SrcBrush )
{
	guard(brushclipBuildGiantBrush);

	GiantBrush->Location = FVector(0,0,0);
	GiantBrush->PrePivot = FVector(0,0,0);
	GiantBrush->CsgOper = SrcBrush->CsgOper;
	GiantBrush->SetFlags( RF_Transactional );
	GiantBrush->PolyFlags = 0;

	verify(GiantBrush->Brush);
	verify(GiantBrush->Brush->Polys);

	GiantBrush->Brush->Polys->Element.Empty();

	// Create a list of vertices that can be used for the new brush
	FVector vtxs[8];

	Plane = Plane.Flip();
	FPoly TempPoly = edBuildInfiniteFPoly( &Plane );
	TempPoly.Finalize(0);
	vtxs[0] = TempPoly.Vertex[0];	vtxs[1] = TempPoly.Vertex[1];
	vtxs[2] = TempPoly.Vertex[2];	vtxs[3] = TempPoly.Vertex[3];

	Plane = Plane.Flip();
	FPoly TempPoly2 = edBuildInfiniteFPoly( &Plane );
	vtxs[4] = TempPoly2.Vertex[0] + (TempPoly2.Normal * -(WORLD_MAX));	vtxs[5] = TempPoly2.Vertex[1] + (TempPoly2.Normal * -(WORLD_MAX));
	vtxs[6] = TempPoly2.Vertex[2] + (TempPoly2.Normal * -(WORLD_MAX));	vtxs[7] = TempPoly2.Vertex[3] + (TempPoly2.Normal * -(WORLD_MAX));

	// Create the polys for the new brush.
	FPoly newPoly;

	// TOP
	newPoly.Init();
	newPoly.NumVertices = 4;
	newPoly.Base = newPoly.Vertex[0] = vtxs[0];	newPoly.Vertex[1] = vtxs[1];	newPoly.Vertex[2] = vtxs[2];	newPoly.Vertex[3] = vtxs[3];
	newPoly.Finalize(0);
	new(GiantBrush->Brush->Polys->Element)FPoly(newPoly);

	// BOTTOM
	newPoly.Init();
	newPoly.NumVertices = 4;
	newPoly.Base = newPoly.Vertex[0] = vtxs[4];	newPoly.Vertex[1] = vtxs[5];	newPoly.Vertex[2] = vtxs[6];	newPoly.Vertex[3] = vtxs[7];
	newPoly.Finalize(0);
	new(GiantBrush->Brush->Polys->Element)FPoly(newPoly);

	// SIDES
	// 1
	newPoly.Init();
	newPoly.NumVertices = 4;
	newPoly.Base = newPoly.Vertex[0] = vtxs[1];	newPoly.Vertex[1] = vtxs[0];	newPoly.Vertex[2] = vtxs[7];	newPoly.Vertex[3] = vtxs[6];
	newPoly.Finalize(0);
	new(GiantBrush->Brush->Polys->Element)FPoly(newPoly);

	// 2
	newPoly.Init();
	newPoly.NumVertices = 4;
	newPoly.Base = newPoly.Vertex[0] = vtxs[2];	newPoly.Vertex[1] = vtxs[1];	newPoly.Vertex[2] = vtxs[6];	newPoly.Vertex[3] = vtxs[5];
	newPoly.Finalize(0);
	new(GiantBrush->Brush->Polys->Element)FPoly(newPoly);

	// 3
	newPoly.Init();
	newPoly.NumVertices = 4;
	newPoly.Base = newPoly.Vertex[0] = vtxs[3];	newPoly.Vertex[1] = vtxs[2];	newPoly.Vertex[2] = vtxs[5];	newPoly.Vertex[3] = vtxs[4];
	newPoly.Finalize(0);
	new(GiantBrush->Brush->Polys->Element)FPoly(newPoly);

	// 4
	newPoly.Init();
	newPoly.NumVertices = 4;
	newPoly.Base = newPoly.Vertex[0] = vtxs[0];	newPoly.Vertex[1] = vtxs[3];	newPoly.Vertex[2] = vtxs[4];	newPoly.Vertex[3] = vtxs[7];
	newPoly.Finalize(0);
	new(GiantBrush->Brush->Polys->Element)FPoly(newPoly);

	// Finish creating the new brush.
	GiantBrush->Brush->BuildBound();

	unguard;
}

void ClipBrushAgainstPlane( FPlane InPlane, ABrush* InBrush, UBOOL InSel )
{
	guard(ClipBrushAgainstPlane);

	// Create a giant brush to use in the intersection process.
	ABrush* GiantBrush = GEditor->Level->SpawnBrush();
	GiantBrush->Brush = new( InBrush->GetOuter(), NAME_None, RF_NotForClient|RF_NotForServer )UModel( NULL );
	brushclipBuildGiantBrush( GiantBrush, InPlane, InBrush );

	// Create a BSP for the brush that is being clipped.
	GEditor->bspBuild( InBrush->Brush, BSP_Optimal, 15, 70, 1, 0 );
	GEditor->bspRefresh( InBrush->Brush, 1 );
	GEditor->bspBuildBounds( InBrush->Brush );

	// Intersect the giant brush with the source brushes BSP.  This will give us the finished, clipping brush
	// contained inside of the giant brush.
	GEditor->bspBrushCSG( GiantBrush, InBrush->Brush, 0, CSG_Intersect, 0, 0 );
	GEditor->bspUnlinkPolys( GiantBrush->Brush );

	// You need at least 4 polys left over to make a valid brush.
	if( GiantBrush->Brush->Polys->Element.Num() < 4 )
		GEditor->Level->DestroyActor( GiantBrush );
	else
	{
		// Have to special case this if we're clipping the builder brush
		if( InBrush == GEditor->Level->Brush() )
		{
			GiantBrush->CopyPosRotScaleFrom( InBrush );
			GiantBrush->PolyFlags = InBrush->PolyFlags;
			GiantBrush->bSelected = InSel;

			GEditor->Level->Brush()->Modify();
			GEditor->csgCopyBrush( GEditor->Level->Brush(), GiantBrush, 0, 0, 0 );

			GEditor->Level->DestroyActor( GiantBrush );
		}
		else
		{
			// Now we need to insert the giant brush into the actor list where the old brush was in order
			// to preserve brush ordering.

			// Copy all actors into a temp list.
			TArray<AActor*> TempList;
			for( INT i = 2 ; i < GEditor->Level->Actors.Num() - 1; i++ )
				if( GEditor->Level->Actors(i) )
				{
					TempList.AddItem( GEditor->Level->Actors(i) );

					// Once we find the source actor, add the new brush right after it.
					if( (ABrush*)GEditor->Level->Actors(i) == InBrush )
						TempList.AddItem( GiantBrush );
				}

			// Now reload the levels actor list with the templist we created above.
			GEditor->Level->Actors.Remove( 2, GEditor->Level->Actors.Num() - 2 );
			for( INT j = 0; j < TempList.Num() ; j++ )
				GEditor->Level->Actors.AddItem( TempList(j) );

			GiantBrush->CopyPosRotScaleFrom( InBrush );
			GiantBrush->PolyFlags = InBrush->PolyFlags;
			GiantBrush->bSelected = InSel;

			// Clean the brush up.
			for( INT poly = 0 ; poly < GiantBrush->Brush->Polys->Element.Num() ; poly++ )
			{
				FPoly* Poly = &(GiantBrush->Brush->Polys->Element(poly));
				Poly->iLink = poly;
				Poly->Normal = FVector(0,0,0);
				Poly->Finalize(0);
			}

			// One final pass to clean the polyflags of all temporary settings.
			for( INT poly = 0 ; poly < GiantBrush->Brush->Polys->Element.Num() ; poly++ )
			{
				FPoly* Poly = &(GiantBrush->Brush->Polys->Element(poly));
				Poly->PolyFlags &= ~PF_EdCut;
				Poly->PolyFlags &= ~PF_EdProcessed;
			}
		}
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	UnrealEd safe command line.
-----------------------------------------------------------------------------*/

// Returns a ppinter to the viewport which has the input focus.
UViewport* UEditorEngine::GetCurrentViewport()
{
	guard(UEditorEngine::GetCurrentViewport);
	for( INT i = 0; i < Client->Viewports.Num(); i++ )
		if( Client->Viewports(i)->Current )
			return Client->Viewports(i);

	return Client->GetLastCurrent();
	unguard;
}

void UEditorEngine::RedrawAllViewports( UBOOL bLevelViewportsOnly )
{
	guard(UEditorEngine::RedrawAllViewports);
	for( INT i = 0; i < Client->Viewports.Num(); i++ )
	{
		if( !bLevelViewportsOnly
				|| ( Client->Viewports(i)->IsPerspective() || Client->Viewports(i)->IsOrtho() ) )
			Client->Viewports(i)->Repaint(1);
	}
	unguard;
}

void UEditorEngine::RedrawCurrentViewport()
{
	guard(UEditorEngine::RedrawCurrentViewport);
	UViewport* vp = GetCurrentViewport();
	if( vp )
		vp->Repaint( 1 );
	unguard;
}

//
// Execute a macro.
//
void UEditorEngine::ExecMacro( const TCHAR* Filename, FOutputDevice& Ar )
{
	guard(UEditorEngine::ExecMacro);

	// Create text buffer and prevent garbage collection.
	UTextBuffer* Text = ImportObject<UTextBuffer>( GEditor->Level, GetTransientPackage(), NAME_None, 0, Filename );
	if( Text )
	{
		Text->AddToRoot();
		debugf( TEXT("Execing %s"), Filename );
		TCHAR Temp[MAX_EDCMD];
		const TCHAR* Data = *Text->Text;
		while( ParseLine( &Data, Temp, ARRAY_COUNT(Temp) ) )
			Exec( Temp, Ar );
		Text->RemoveFromRoot();
		delete Text;
	}
	else Ar.Logf( NAME_ExecWarning, LocalizeError("FileNotFound",TEXT("Editor")), Filename );

	unguard;
}

//
// Execute a command that is safe for rebuilds.
//
UBOOL UEditorEngine::SafeExec( const TCHAR* InStr, FOutputDevice& Ar )
{
	guard(UEditorEngine::SafeExec);
	TCHAR TempFname[MAX_EDCMD], TempStr[MAX_EDCMD], TempName[NAME_SIZE];
	const TCHAR* Str=InStr;
	if( ParseCommand(&Str,TEXT("MACRO")) || ParseCommand(&Str,TEXT("EXEC")) )//oldver (exec)
	{
		TCHAR Filename[MAX_EDCMD];
		if( ParseToken( Str, Filename, ARRAY_COUNT(Filename), 0 ) )
			ExecMacro( Filename, Ar );
		return 1;
	}
	// sjs ---
	else if( ParseCommand(&Str,TEXT("DECAANIM")) )
	{
		#if NODECA
			FString tmp = FString::Printf(TEXT("ANIM %s"), Str);
			SafeExec(*tmp, Ar );
			return 1;
		#else
			if( Exec_DecaAnim( Str, Ar ) )
				return 1;
		#endif	
	}
	else if( ParseCommand(&Str,TEXT("DECAMESH")) )
	{
		#if NODECA
			FString tmp = FString::Printf(TEXT("MESH %s"), Str);
			SafeExec(*tmp, Ar );
			return 1;
		#else
			if( Exec_DecaMesh( Str, Ar ) )
				return 1;
		#endif
	}
	else if(ParseCommand(&Str,TEXT("STATICMESH")))
	{
		// amb, jij ---
        if (ParseCommand(&Str, TEXT("IMPORT")))
        {
            if (Exec_StaticMeshImport(Str, Ar))
                return 1;
			
			if (Exec_StaticMesh(Str, Ar))
				return 1;

        }
	}
	// --- sjs
	else if( ParseCommand(&Str,TEXT("NEW")) )
	{
		// Generalized object importing.
		DWORD   Flags         = RF_Public|RF_Standalone;
		if( ParseCommand(&Str,TEXT("STANDALONE")) )
			Flags = RF_Public|RF_Standalone;
		else if( ParseCommand(&Str,TEXT("PUBLIC")) )
			Flags = RF_Public;
		else if( ParseCommand(&Str,TEXT("PRIVATE")) )
			Flags = 0;
		FString ClassName     = ParseToken(Str,0);
		UClass* Class         = FindObject<UClass>( ANY_PACKAGE, *ClassName );
		if( !Class )
		{
			Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized or missing factor class %s"), *ClassName );
			return 1;
		}
		FString  PackageName  = ParentContext ? ParentContext->GetName() : TEXT("");
		FString  FileName     = TEXT("");
		FString  ObjectName   = TEXT("");
		UClass*  ContextClass = NULL;
		UObject* Context      = NULL;
		Parse( Str, TEXT("Package="), PackageName );
		Parse( Str, TEXT("File="), FileName );
		ParseObject( Str, TEXT("ContextClass="), UClass::StaticClass(), *(UObject**)&ContextClass, NULL );
		ParseObject( Str, TEXT("Context="), ContextClass, Context, NULL );
		if
		(	!Parse( Str, TEXT("Name="), ObjectName )
		&&	FileName!=TEXT("") )
		{
			// Deduce object name from filename.
			ObjectName = FileName;
			for( ; ; )
			{
				INT i=ObjectName.InStr(PATH_SEPARATOR);
				if( i==-1 )
					i=ObjectName.InStr(TEXT("/"));
				if( i==-1 )
					break;
				ObjectName = ObjectName.Mid( i+1 );
			}
			if( ObjectName.InStr(TEXT("."))>=0 )
				ObjectName = ObjectName.Left( ObjectName.InStr(TEXT(".")) );
		}
		UFactory* Factory = NULL;
		if( Class->IsChildOf(UFactory::StaticClass()) )
			Factory = ConstructObject<UFactory>( Class );
		UObject* Object = UFactory::StaticImportObject
		(
			GEditor->Level,
			Factory ? Factory->SupportedClass : Class,
			CreatePackage(NULL,*PackageName),
			*ObjectName,
			Flags,
			*FileName,
			Context,
			Factory,
			Str,
			GWarn
		);
		if( !Object )
			Ar.Logf( NAME_ExecWarning, TEXT("Failed factoring: %s"), InStr );
		Flush(0);
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("LOAD") ) )
	{
		// Object file loading.
		if( Parse( Str, TEXT("FILE="), TempFname, 256) )
		{
			if( !ParentContext )
				Level->RememberActors();
			TCHAR PackageName[256]=TEXT("");
			UObject* Pkg=NULL;
			if( Parse( Str, TEXT("Package="), PackageName, ARRAY_COUNT(PackageName) ) )
			{
				TCHAR Temp[256], *End;
				appStrcpy( Temp, PackageName );
				End = appStrchr(Temp,'.');
				if( End )
					*End++ = 0;
				Pkg = CreatePackage( NULL, PackageName );
			}
			Pkg = LoadPackage( Pkg, TempFname, 0 );
			if( *PackageName )
				ResetLoaders( Pkg, 0, 1 );
			Flush(0);
			if( !ParentContext )
			{
				Level->ReconcileActors();
				RedrawLevel(Level);
			}
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("Texture")) )
	{
		if( ParseCommand(&Str,TEXT("Import")) )
		{
			// Texture importing.
			//->FACTOR TEXTURE ...
			FName PkgName = ParentContext ? ParentContext->GetFName() : NAME_None;
			Parse( Str, TEXT("Package="), PkgName );
			if( PkgName!=NAME_None && Parse( Str, TEXT("File="), TempFname, ARRAY_COUNT(TempFname) ) )
			{
#if 0
				// sjs ---
				if ( appStricmp(&TempFname[appStrlen(TempFname)-3],TEXT("tga"))==0
                    || appStricmp(&TempFname[appStrlen(TempFname)-3],TEXT("bmp"))==0)
                {
                    DDSConversion( TempFname, Str, Ar );
                }
                // --- sjs
#endif
				UPackage* Pkg = CreatePackage(NULL,*PkgName);
				Pkg->bDirty = 1;
				if( !Parse( Str, TEXT("Name="),  TempName,  NAME_SIZE ) )
				{
					// Deduce package name from filename.
					TCHAR* End = TempFname + appStrlen(TempFname);
					while( End>TempFname && End[-1]!=PATH_SEPARATOR[0] && End[-1]!='/' )
						End--;
					appStrncpy( TempName, End, NAME_SIZE );
					if( appStrchr(TempName,'.') )
						*appStrchr(TempName,'.') = 0;
				}

				GWarn->BeginSlowTask( TEXT("Importing texture"), 1 );
				UBOOL DoMips=1;
				ParseUBOOL( Str, TEXT("Mips="), DoMips );
				extern TCHAR* GFile;
				GFile = TempFname;
				FName GroupName = NAME_None;
				if( Parse( Str, TEXT("GROUP="), GroupName ) && GroupName!=NAME_None )
					Pkg = CreatePackage(Pkg,*GroupName);

				// If the texture already exists, save it's pointer.
				UTexture* ExistingTexture = Cast<UTexture>(StaticFindObject( UTexture::StaticClass(), Pkg, TempName ));
				UBOOL bTextureExists = (ExistingTexture != NULL);

				// sjs, gam ---
				BYTE        PrevSurfaceType = EST_Default;
	            BYTE        PrevLODSet = LODSET_World;
                BYTE        PrevNormalLOD = 0;
                BYTE        PrevUClamp = TC_Wrap;
                BYTE        PrevVClamp = TC_Wrap;
                UMaterial*  PrevDetail = NULL;
                FLOAT       PrevDetailScale = 1.0;
                
				if ( bTextureExists )
                {
					PrevSurfaceType = ExistingTexture->SurfaceType;

                    PrevLODSet = ExistingTexture->LODSet;
                    PrevNormalLOD = ExistingTexture->NormalLOD;
                    PrevUClamp = ExistingTexture->UClampMode;
                    PrevVClamp = ExistingTexture->VClampMode;
                    PrevDetail = ExistingTexture->Detail;
                    PrevDetailScale = ExistingTexture->DetailScale;
                }
				// --- sjs, gam

				UTexture* Texture = ImportObject<UTexture>( GEditor->Level, Pkg, TempName, RF_Public|RF_Standalone, TempFname );
				if( Texture )
				{
					// If the texture is not powers of two in each direction, abort the import.
					if( !FIsPowerOfTwo( Texture->USize ) || !FIsPowerOfTwo( Texture->VSize ) )
					{
						appMsgf(0,TEXT("\"%s\" cannot be imported because it's dimensions are not powers of two (%dx%d)"),
							Texture->GetPathName(), Texture->USize, Texture->VSize );
						delete Texture;
						GWarn->EndSlowTask();

						return 0;
					}

					DWORD TexFlags=0;
					Parse( Str, TEXT("LODSet="), Texture->LODSet );
					Parse( Str, TEXT("TexFlags="), TexFlags );

					// sjs, gam ---
					UBOOL DoAlpha=0;
					ParseUBOOL( Str, TEXT("Alpha="), DoAlpha );
                    Texture->bAlphaTexture = DoAlpha;
                    INT NormalLOD = 0;
					Parse( Str, TEXT("NormalLOD="), NormalLOD );
                    Texture->NormalLOD = NormalLOD;
                    // --- sjs, gam

                    UBOOL bMasked=0, bAlphaTexture=0, bModulated=0, bAdditive=0;
					ParseUBOOL( Str, TEXT("MASKED="), bMasked );		
					ParseUBOOL( Str, TEXT("ALPHATEXTURE="), bAlphaTexture );
					ParseUBOOL( Str, TEXT("MODULATED="), bModulated );
					ParseUBOOL( Str, TEXT("ADDITIVE="), bAdditive );
                    bAlphaTexture |= DoAlpha;
					Texture->bMasked = bMasked ? 1 : 0;
					Texture->bAlphaTexture = bAlphaTexture ? 1 : 0;

					ParseObject<UTexture>( Str, TEXT("NEXT="), Texture->AnimNext, ANY_PACKAGE );

					Texture->CreateMips( DoMips || bModulated || bAdditive, 1, bModulated || bAdditive, bModulated ? FColor(128,128,128,0) : FColor(0,0,0,0) );
					Texture->CreateColorRange();

					INT DXTFormat;
					if( Parse( Str, TEXT("DXT="), DXTFormat ) )
					{
						FDXTCompressionOptions CompressionOptions;
						appMemzero(&CompressionOptions,sizeof(CompressionOptions));
						
						if( bModulated || bAdditive )
						{
							CompressionOptions.Fade			= 1;
							CompressionOptions.Border		= 1;
							if( bModulated )
							{
								CompressionOptions.BorderColor	= FColor(128,128,128,0);
								CompressionOptions.FadeToColor	= FColor(128,128,128,0);
							}
							else
							{
								CompressionOptions.BorderColor	= FColor(0,0,0,0);
								CompressionOptions.FadeToColor	= FColor(0,0,0,0);
							}
							CompressionOptions.FadeAmount	= 100;
						}
						
						//GWarn->Logf(TEXT("%s"), Texture->GetPathName());
						Texture->Compress( DXTFormat == 1 ? TEXF_DXT1 : DXTFormat == 3 ? TEXF_DXT3 : TEXF_DXT5, DoMips || bModulated, &CompressionOptions );
					}

					UBOOL AlphaTrick=0;
					ParseUBOOL( Str, TEXT("ALPHATRICK="), AlphaTrick );
					if( AlphaTrick )
						for( INT i=0; i<256; i++ )
							Texture->Palette->Colors(i).A = Texture->Palette->Colors(i).B;

					FString UClampMode;
					if( Parse( Str, TEXT("UCLAMPMODE="), UClampMode ) )
					{
						if( UClampMode.Caps() == TEXT("CLAMP") )
							Texture->UClampMode = TC_Clamp;
						else
						if( UClampMode.Caps() == TEXT("WRAP") )
							Texture->UClampMode = TC_Wrap;
					}

					FString VClampMode;
					if( Parse( Str, TEXT("VCLAMPMODE="), VClampMode ) )
					{
						if( VClampMode.Caps() == TEXT("CLAMP") )
							Texture->VClampMode = TC_Clamp;
						else
						if( VClampMode.Caps() == TEXT("WRAP") )
							Texture->VClampMode = TC_Wrap;
					}

                    // gam, sjs ---
					if( bTextureExists )
					{
				        Texture->SurfaceType = PrevSurfaceType;

                        Texture->LODSet = PrevLODSet;
                        Texture->NormalLOD = PrevNormalLOD;
                        Texture->UClampMode = PrevUClamp;
                        Texture->VClampMode = PrevVClamp;
                        Texture->Detail = PrevDetail;
                        Texture->DetailScale = PrevDetailScale;
					}
                    // --- gam, sjs

					if( bTextureExists )
					{
						Flush(0);

						RedrawLevel(Level);
					}

					debugf( NAME_Log, TEXT("Imported %s"), Texture->GetFullName() );
				}
				else Ar.Logf( NAME_Error, TEXT("Import texture %s from %s failed"), TempName, TempFname ); // gam
				GWarn->EndSlowTask();
				GCache.Flush( 0, ~0, 1 );
			}
			else Ar.Logf( NAME_ExecWarning, TEXT("Missing file or name") );
			return 1;
		}
	}
	else if( ParseCommand(&Str,TEXT("FONT")) )//oldver
	{
		if( ParseCommand(&Str,TEXT("IMPORT")) )//oldver
			return SafeExec( *(US+TEXT("NEW FONTFACTORY ")+Str), Ar ); 
		if( ParseCommand(&Str,TEXT("INFO")) )
		{
			UFont* Font;
			ParseObject<UFont>( Str, TEXT("Font="), Font, ANY_PACKAGE );

			if( !Font )
			{
				Ar.Logf( NAME_ExecWarning, TEXT("Couldn't locate font") );	
				return 1;
			}
			for( INT p=0; p<Font->Textures.Num(); p++ )
				Ar.Logf( TEXT(" %s(%d): %s"), Font->GetName(), p, Font->Textures(p)->GetName() );

			return 1;
		}
		if( ParseCommand(&Str,TEXT("UPDATE")) )
		{
			guard(UpdateFont);
			UFont* Font;
			UTexture* Texture;
			INT Page;
			ParseObject<UFont>( Str, TEXT("Font="), Font, ANY_PACKAGE );
			ParseObject<UTexture>( Str, TEXT("Texture="), Texture, ANY_PACKAGE );
			Parse( Str, TEXT("Page="), Page );

			if( !Font )
			{
				Ar.Logf( NAME_ExecWarning, TEXT("Couldn't locate font") );	
				return 1;
			}

			if( !Texture )
			{
				Ar.Logf( NAME_ExecWarning, TEXT("Couldn't locate texture") );	
				return 1;
			}

			if( Page>=0 && Page<Font->Textures.Num() )
			{
				UObject* Old = Font->Textures(Page);
				Font->Textures(Page) = Texture;
				if( UObject::IsReferenced( Old, RF_Native | RF_Public, 0 ) )
					Ar.Logf( TEXT("Can't delete - old font page texture is in use") );
				else
					delete Old;
			}
			else
				Ar.Logf( NAME_ExecWarning, TEXT("Page %d out of range 0..%d for Font %s"), Page, Font->Textures.Num()-1, Font->GetFullName() );	
			return 1;
			unguard;
		}
	}
	else if( ParseCommand(&Str,TEXT("OBJ")) )//oldver
	{
		UClass* Type;
		if( ParseCommand( &Str, TEXT("LOAD") ) )//oldver
			return SafeExec( *(US+TEXT("LOAD ")+Str), Ar ); 
		else if( ParseCommand(&Str,TEXT("IMPORT")) )//oldver
			if( ParseObject<UClass>( Str, TEXT("TYPE="), Type, ANY_PACKAGE ) )
				return SafeExec( *(US+TEXT("NEW STANDALONE ")+Type->GetName()+TEXT(" ")+Str), Ar ); 
		return 0;
	}
	else if( ParseCommand( &Str, TEXT("MESHMAP")) )
	{
		if( ParseCommand( &Str, TEXT("SCALE") ) )
		{
			// Mesh scaling.
			UMesh* Mesh;
			if( ParseObject<UMesh>( Str, TEXT("MESHMAP="), Mesh, ANY_PACKAGE ) )
			{
				FVector Scale(0.f,0.f,0.f);
				GetFVECTOR( Str, Scale );
				Mesh->MeshGetInstance(NULL)->SetScale( Scale );				
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Missing meshmap") );
			return 1;
		}
		else if( ParseCommand( &Str, TEXT("SETTEXTURE") ) )
		{
			// Mesh texture mapping.
			UMesh* Mesh;
			UMaterial* Material;
			INT Num;
			if
			(	ParseObject<UMesh>( Str, TEXT("MESHMAP="), Mesh, ANY_PACKAGE )
			&&	ParseObject<UMaterial>( Str, TEXT("TEXTURE="), Material, ANY_PACKAGE )
			&&	Parse( Str, TEXT("NUM="), Num )			
			&&  Mesh->IsA(ULodMesh::StaticClass())
			&&	Num<((ULodMesh*)Mesh)->Materials.Num() )
			{
				((ULodMesh*)Mesh)->Materials( Num ) = Material;
				/*
				FLOAT TextureLod=1.0f;
				Parse( Str, TEXT("TLOD="), TextureLod );
				if( Num < Mesh->TextureLOD.Num() )
					Mesh->TextureLOD( Num ) *= TextureLod;
				*/
				debugf( TEXT("Added texture number: %i total %i for mesh %s"), Num, ((ULodMesh*)Mesh)->Materials.Num(), Mesh->GetName() );
			}
			else Ar.Logf( NAME_ExecWarning, TEXT("Missing meshmap, texture, or num (%s)"), Str );
			return 1;
		}		
	}
	else if( ParseCommand(&Str,TEXT("ANIM")) )
	{
		if( ParseCommand(&Str,TEXT("IMPORT")) )
		{
			// ANIM animating hierarchy object import.
			if
			(	Parse( Str, TEXT("ANIM="), TempName, ARRAY_COUNT(TempName) )
			&&	Parse( Str, TEXT("ANIMFILE="), TempStr, ARRAY_COUNT(TempStr) )
			)
			{
				UBOOL Unmirror=0, ZeroTex=0; 
				INT UnMirrorTex; 
				ParseUBOOL( Str, TEXT("UNMIRROR="), Unmirror );
				ParseUBOOL( Str, TEXT("ZEROTEX="), ZeroTex );
				if( !Parse( Str, TEXT("UNMIRRORTEX="), UnMirrorTex ) )
					UnMirrorTex = -1;
				FLOAT CompDefault = 1.0f;
				Parse( Str, TEXT("COMPRESS="), CompDefault );
				animationImport( TempName, ParentContext, TempStr, Unmirror, CompDefault );
			}
			else Ar.Log(NAME_ExecWarning,TEXT("Bad ANIM IMPORT"));
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("SEQUENCE")) )
		{
			// Set up skeletal animation sequences. 
			UMeshAnimation *Anim;
			MotionChunkDigestInfo MoveInfo;

			//FMeshAnimSeq Seq;
			INT NumFrames;
			INT StartFrame;
			FLOAT AnimRate;
			FLOAT TrackTime;

			if
			(	ParseObject<UMeshAnimation>( Str, TEXT("ANIM="), Anim, ANY_PACKAGE )
			&&	Parse( Str, TEXT("SEQ="), MoveInfo.Name )
			&&	Parse( Str, TEXT("STARTFRAME="), StartFrame )
			&&	Parse( Str, TEXT("NUMFRAMES="), NumFrames ) 
			&&  Anim->DigestHelper )
			{
				// Optional parameters
				Parse( Str, TEXT("GROUP="), MoveInfo.Group );

				if( !Parse( Str, TEXT("RATE="), AnimRate ))
					AnimRate = 1.0f;				
				
				if( !Parse( Str, TEXT("TRACKTIME="), TrackTime ))
					TrackTime = 1.0f;
				
				// Detect which anim sequence to change, or make a new one.
				INT i;
				for( i=0; i<Anim->DigestHelper->MovesInfo.Num(); i++ )
					if( Anim->DigestHelper->MovesInfo(i).Name==MoveInfo.Name )
						break;

				if( i<Anim->DigestHelper->MovesInfo.Num() )
				{
					Anim->DigestHelper->MovesInfo(i)=MoveInfo;
				}
				else
				{	i = Anim->DigestHelper->MovesInfo.Num();
					Anim->DigestHelper->MovesInfo.AddItem(MoveInfo);
				}

				// Parse boolean switches
				FString TempStr;
				if( Parse( Str, TEXT("ROOTTRACK"), TempStr))				   
					Anim->DigestHelper->MovesInfo(i).RootInclude = 1;

				if( Parse( Str, TEXT("ROOTONLY"), TempStr))				   
					Anim->DigestHelper->MovesInfo(i).RootInclude = 2;
				// Override default compression factor?
				FLOAT Comp;
				if( Parse( Str, TEXT("COMPRESS="),Comp))
					Anim->DigestHelper->MovesInfo(i).KeyReduction=Comp; 
				else
					Anim->DigestHelper->MovesInfo(i).KeyReduction=Anim->DigestHelper->CompFactor; 

				// Start bone index from bone fname.
				if ( Parse( Str, TEXT("STARTBONE="), TempFname, 80 ) )
				{
					Anim->DigestHelper->MovesInfo(i).StartBone = animGetBoneIndex( Anim, TempFname );
					debugf(TEXT("Start bone assignment %i name %s for anim %s"),Anim->DigestHelper->MovesInfo(i).StartBone,TempFname,*(Anim->DigestHelper->MovesInfo(i).Name));
				}
				else
					Anim->DigestHelper->MovesInfo(i).StartBone = 0;

				Parse( Str, TEXT("MAXKEYS="), Anim->DigestHelper->MovesInfo(i).KeyQuotum );
									
				Anim->DigestHelper->MovesInfo(i).FirstRawFrame = StartFrame; //Anim->AnimSeqs(i).StartFrame;
				Anim->DigestHelper->MovesInfo(i).NumRawFrames = NumFrames;  
				Anim->DigestHelper->MovesInfo(i).AnimRate = AnimRate;    // 
				Anim->DigestHelper->MovesInfo(i).TrackTime = TrackTime; // total time - override by PlayRate  ?
			}
			else 
			if 
			( ParseObject<UMeshAnimation>( Str, TEXT("ANIM="), Anim, ANY_PACKAGE )
			&&	Parse( Str, TEXT("SEQ="), MoveInfo.Name ))
			{
				// Implicit-animation selected parameters override.
				FName GroupName;

				if( !Parse( Str, TEXT("GROUP="), GroupName ))
					GroupName = NAME_None;
				if( !Parse( Str, TEXT("RATE="), AnimRate ))
					AnimRate = 0.0f;				

				FLOAT Comp;
				if( !Parse( Str, TEXT("COMPRESS="),Comp))
					Comp = 0.0f;

				// Find the existing from-PSA Raw sequence info to override.
				for( INT t=0; t<Anim->DigestHelper->RawAnimSeqInfo.Num(); t++)
				{
					if( FName(appFromAnsi( Anim->DigestHelper->RawAnimSeqInfo(t).Name)) == MoveInfo.Name )
					{
						debugf(TEXT("Overriding for animation: %s rate %f group %s "),*(MoveInfo.Name),AnimRate,*(GroupName) );

						if( AnimRate > 0.0f) Anim->DigestHelper->RawAnimSeqInfo(t).AnimRate = AnimRate;
						if( Comp > 0.0f) Anim->DigestHelper->RawAnimSeqInfo(t).KeyReduction = Comp;
						if( GroupName != NAME_None )
						{
							TCHAR TempStr[256];
							appSprintf(TempStr,*GroupName);							
							// Trick: copying string to ascii, not to a TCHAR array!
							for( INT n=0; n<64; n++)
							{
							     Anim->DigestHelper->RawAnimSeqInfo(t).Group[n]=(char)TempStr[n];
							}							
						}
					}
				}
			}
			else
				Ar.Log(NAME_ExecWarning,TEXT("Bad ANIM SEQUENCE"));			
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("NOTIFY")) )
		{
			// Animation notifications.
			UMeshAnimation *Anim;
			FName SeqName;
			FMeshAnimNotify Notify;
			FLOAT Frame = -1.0f;
			if
			(	ParseObject<UMeshAnimation>( Str, TEXT("ANIM="), Anim, ANY_PACKAGE )
			&&	Parse( Str, TEXT("SEQ="), SeqName )
			&&	( Parse( Str, TEXT("TIME="), Notify.Time ) || Parse( Str, TEXT("FRAME="), Frame) ) )
			{
				if( !ParseObject<UAnimNotify>( Str, TEXT("OBJECT="), Notify.NotifyObject, ANY_PACKAGE ) )
				{
					FName NotifyFunction;
					if( Parse( Str, TEXT("FUNCTION="), NotifyFunction ) )
					{	
						Notify.NotifyObject = ConstructObject<UAnimNotify_Script>( UAnimNotify_Script::StaticClass(), Anim->GetOuter() );
						Cast<UAnimNotify_Script>(Notify.NotifyObject)->NotifyName = NotifyFunction;
					}
					else
					{
						Ar.Log( NAME_ExecWarning, TEXT("Bad ANIM NOTIFY: missing OBJECT= or FUNCTION=") );
						return 1;
					}
				}
				
				FMeshAnimSeq* Seq = Anim->GetAnimSeq( SeqName );
				if( Seq ) 
				{
					if( Frame >= 0.0f) 
					{						
						// Valid Frame indicators are in range [0..(NumFrames-1)]
						if( (Seq->NumFrames > 1) && (Frame < ((FLOAT)Seq->NumFrames-1)) )
							Notify.Time = ( Frame / ((FLOAT)Seq->NumFrames-1 ) );							
						else
							Ar.Log( NAME_ExecWarning, TEXT(" Out-of-range notification frame in ANIM NOTIFY"));
							
					}
					new( Seq->Notifys )FMeshAnimNotify( Notify );
				}
					else Ar.Log( NAME_ExecWarning, TEXT("Unknown skeletal animation sequence in ANIM NOTIFY") );
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Bad ANIM NOTIFY") );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("DIGEST")) )
		{
			// Animation final digest - along the lines of our Sequences and Notifys.
			UMeshAnimation *Anim;
	
			if(	ParseObject<UMeshAnimation>( Str, TEXT("ANIM="), Anim, ANY_PACKAGE ))
			{
				// Should never be called - except in case of an pre-existing animation being redigested?
				if( !Anim->DigestHelper) 
				{
					Anim->InitForDigestion();
				}

				// Write debugging info to log if verbose mode requested.
				UBOOL bVerbose;
				FString TempStr;
				bVerbose = Parse( Str, TEXT("VERBOSE"), TempStr);

				// if binary sequence info required, we'll fill our MovesInfo with the Anim->DigestHelper->RawAnimSeqInfo; otherwise
				// throw away all RawAnimSeqInfo.
				UBOOL bUseRawSequences;
				bUseRawSequences = Parse( Str,TEXT("USERAWINFO"),TempStr);
				if( !bUseRawSequences ) Anim->DigestHelper->RawAnimSeqInfo.Empty();
		
				if( bVerbose )
				{
					debugf(TEXT("Skeletal animation digest: raw animation key memory: %i Bytes."),Anim->DigestHelper->RawAnimKeys.Num()*sizeof(VQuatAnimKey));
					if( bUseRawSequences ) debugf(TEXT("Using predefined animation sequences from file."));
				}		
				// Digest and compress the movements.
				digestMovementRepertoire(Anim);
				// Erase the raw data.
				Anim->DigestHelper->RawAnimKeys.Empty();
				Anim->DigestHelper->MovesInfo.Empty();
				if( bVerbose )
				{
					debugf(TEXT("Skeletal animation digest: final animation key memory: %i Bytes."),Anim->MemFootprint());
				}
					
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Bad ANIM AnimCompress") );
			return 1;
		}
	}	
	else if( ParseCommand(&Str,TEXT("MESH")) )
	{
#if 0 /* Pre LOD */
		if( ParseCommand(&Str,TEXT("IMPORT")) )
		{
			// Mesh importing.
			TCHAR TempStr1[MAX_EDCMD];
			if
			(	Parse( Str, TEXT("MESH="), TempName, ARRAY_COUNT(TempName) )
			&&	Parse( Str, TEXT("ANIVFILE="), TempStr, ARRAY_COUNT(TempStr) )
			&&	Parse( Str, TEXT("DATAFILE="), TempStr1, ARRAY_COUNT(TempStr1) ) )
			{
				UBOOL Unmirror=0, ZeroTex=0; INT UnMirrorTex;
				ParseUBOOL( Str, TEXT("UNMIRROR="), Unmirror );
				ParseUBOOL( Str, TEXT("ZEROTEX="), ZeroTex );
				if( !Parse( Str, TEXT("UNMIRRORTEX="), UnMirrorTex ) )
					UnMirrorTex = -1;
				meshVertImport( TempName, ParentContext, TempStr, TempStr1, Unmirror, ZeroTex, UnMirrorTex );
			}
			else Ar.Log(NAME_ExecWarning,TEXT("Bad MESH IMPORT"));
			return 1;
		}
#else		
		if( ParseCommand( &Str, TEXT("SCALE") ) )
		{
			// Mesh scaling.
			UMesh* Mesh;
			if( ParseObject<UMesh>( Str, TEXT("MESH="), Mesh, ANY_PACKAGE ) )
			{
				FVector Scale(0.f,0.f,0.f);
				GetFVECTOR( Str, Scale );
				Mesh->MeshGetInstance(NULL)->SetScale( Scale );				
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Missing Mesh") );
			return 1;
		}
        else
		if( ParseCommand( &Str, TEXT("SETTEXTURE") ) )
		{
			// Mesh texture mapping.
			UMesh* Mesh;
			UMaterial* Material;
			INT Num;
			if
			(	ParseObject<UMesh>( Str, TEXT("MESH="), Mesh, ANY_PACKAGE )
			&&	ParseObject<UMaterial>( Str, TEXT("TEXTURE="), Material, ANY_PACKAGE )
			&&	Parse( Str, TEXT("NUM="), Num )			
			&&  Mesh->IsA(ULodMesh::StaticClass())
			&&	Num<((ULodMesh*)Mesh)->Materials.Num() )
			{
				((ULodMesh*)Mesh)->Materials( Num ) = Material;
				/*
				FLOAT TextureLod=1.0f;
				Parse( Str, TEXT("TLOD="), TextureLod );
				if( Num < Mesh->TextureLOD.Num() )
					Mesh->TextureLOD( Num ) *= TextureLod;
				*/
				debugf( TEXT("Added texture number: %i total %i for mesh %s"), Num, ((ULodMesh*)Mesh)->Materials.Num(), Mesh->GetName() );
			}
			else Ar.Logf( NAME_ExecWarning, TEXT("Missing meshmap, texture, or num (%s)"), Str );
			return 1;
        }
        else
		if( ParseCommand(&Str,TEXT("MODELIMPORT")) )
		{
			// MODELIMPORT - Skeletal mesh object import.
			if
			(	Parse( Str, TEXT("MESH="), TempName, ARRAY_COUNT(TempName) )
			&&	Parse( Str, TEXT("MODELFILE="), TempStr, ARRAY_COUNT(TempStr) )
			)
			{
				UBOOL Unmirror=0, ZeroTex=0; 

				FLODProcessInfo LODInfo;
				LODInfo.LevelOfDetail = true; 
				LODInfo.ApplySmoothingGroups = false;
				LODInfo.Style = 0;				
				LODInfo.SampleFrame = 0;
				LODInfo.NoUVData = false;
				LODInfo.Specify = 0;
				
				ParseUBOOL( Str, TEXT("UNMIRROR="), Unmirror );
				ParseUBOOL( Str, TEXT("ZEROTEX="), ZeroTex );

				ParseUBOOL( Str, TEXT("MLOD="),  LODInfo.LevelOfDetail ); 
				Parse( Str,TEXT("LODSTYLE="),	 LODInfo.Style );
				Parse( Str,TEXT("DOSMOOTH="),    LODInfo.ApplySmoothingGroups );
				Parse( Str,TEXT("LODFRAME="),	 LODInfo.SampleFrame );
				ParseUBOOL( Str,TEXT("LODNOTEX="),LODInfo.NoUVData );	
				
				// If true, don't create any lod models but specify them with specific #exec's.
				if( !Parse(Str,TEXT("LODSPECIFY="), LODInfo.Specify ) ) 
					LODInfo.Specify = 0;
				
				meshSkelImport( TempName, ParentContext, TempStr, Unmirror, ZeroTex, 0, &LODInfo );
				
			}
			else Ar.Log(NAME_ExecWarning,TEXT("Bad MODEL IMPORT"));
			return 1;
		}
		else
		if( ParseCommand(&Str,TEXT("FLIPFACES")) )
		{
			UMesh* Mesh;
			// MODEL boned mesh object import.
			if(	ParseObject<UMesh>( Str, TEXT("MESH="), Mesh, ANY_PACKAGE ))
			{				
				if( Mesh->IsA(USkeletalMesh::StaticClass()) )
					((USkeletalMesh*)Mesh)->FlipFaces();
			}
			else Ar.Log(NAME_ExecWarning,TEXT("Bad MODEL IMPORT"));
			return 1;
		}
		else
		if( ParseCommand(&Str,TEXT("LODMODEL")) ) // Generate or import LOD level for a skeletal mesh.
		{
			USkeletalMesh* Mesh;			
			// Skeletal Mesh Discrete LOD level generation.
			if( ParseObject<USkeletalMesh>( Str, TEXT("MESH="), Mesh, ANY_PACKAGE ) )
			{
				INT LodLevel = 0;
				if( !Parse( Str,TEXT("LEVEL="),LodLevel) )
				{
					Ar.Log(NAME_ExecWarning,TEXT("Invalid or no LODLEVEL specified."));
					return 1;
				}
				FLOAT Hysteresis = 0.02f;
				Parse( Str,TEXT("HYSTERESIS="),	Hysteresis );
				FLOAT DisplayFactor = 1.0f;
				Parse( Str,TEXT("DISTANCEFACTOR="), DisplayFactor );
				FLOAT Reduction = 1.0f;
				Parse( Str,TEXT("REDUCTION="),Reduction);
				INT MaxInfluences = 4;
				Parse( Str,TEXT("MAXINFLUENCES="),MaxInfluences);
				UBOOL Coherence = 0;
				Parse( Str,TEXT("COHERENCE="),Coherence);

				FString InputMesh;
				USkeletalMesh* SourceMesh;
				if( ParseObject<USkeletalMesh>( Str, TEXT("FROMMESH="), SourceMesh, ANY_PACKAGE ) )
				{
					// Import from another existing mesh's full LOD data.					
					Mesh->InsertLodModel( LodLevel, SourceMesh, DisplayFactor, Coherence );
				}
				else // generate from existing data.
				{
					Mesh->GenerateLodModel( LodLevel,Reduction,DisplayFactor,MaxInfluences,Coherence);
				}					
				
			}
			else Ar.Log(NAME_ExecWarning,TEXT("Bad LODMODEL generation command."));
			return 1;			
		}
		else
		if( ParseCommand(&Str,TEXT("IMPORT")) )
		{
			// Mesh importing.
			TCHAR TempStr1[MAX_EDCMD];
			if
			(	Parse( Str, TEXT("MESH="), TempName, ARRAY_COUNT(TempName) )
			&&	Parse( Str, TEXT("ANIVFILE="), TempStr, ARRAY_COUNT(TempStr) )
			&&	Parse( Str, TEXT("DATAFILE="), TempStr1, ARRAY_COUNT(TempStr1) ) )
			{
				UBOOL Unmirror=0, ZeroTex=0; INT UnMirrorTex;

				FLODProcessInfo LODInfo;
				LODInfo.LevelOfDetail = true; 
				LODInfo.Style = 0;				
				LODInfo.SampleFrame = 0;
				LODInfo.NoUVData = false;
				LODInfo.Specify = 0;

				ParseUBOOL( Str, TEXT("UNMIRROR="), Unmirror );
				ParseUBOOL( Str, TEXT("ZEROTEX="), ZeroTex );

				ParseUBOOL( Str, TEXT("MLOD="),  LODInfo.LevelOfDetail ); 
				Parse(Str,TEXT("LODSTYLE="),	 LODInfo.Style );
				Parse(Str,TEXT("LODFRAME="),	 LODInfo.SampleFrame );
				ParseUBOOL(Str,TEXT("LODNOTEX="),LODInfo.NoUVData );	
				if( !Parse(Str,TEXT("LODSPECIFY="), LODInfo.Specify ) )
					LODInfo.Specify = 0;

				if( !Parse( Str, TEXT("UNMIRRORTEX="), UnMirrorTex ) )
					UnMirrorTex = -1;
				meshVertImport( TempName, ParentContext, TempStr, TempStr1, Unmirror, ZeroTex, UnMirrorTex, &LODInfo );
			}
			else Ar.Log(NAME_ExecWarning,TEXT("Bad MESH IMPORT"));
			return 1;
		}
		else if( ParseCommand(&Str, TEXT("DROPFRAMES")) )
		{
			UMesh* Mesh;
			INT StartFrame;
			INT NumFrames;
			if
			(	ParseObject<UMesh>( Str, TEXT("MESH="), Mesh, ANY_PACKAGE )
			&&	Parse( Str, TEXT("STARTFRAME="), StartFrame )
			&&	Parse( Str, TEXT("NUMFRAMES="), NumFrames ) )
			{
				if( Mesh->IsA(UVertMesh::StaticClass()) )
					meshDropFrames((UVertMesh*)Mesh, StartFrame, NumFrames);
			}
		}		
		else if( ParseCommand(&Str, TEXT("ATTACHNAME")) )
		{
			USkeletalMesh* Mesh;
			FName BoneName;
			FName TagName;
			if
			(	ParseObject<USkeletalMesh>( Str, TEXT("MESH="), Mesh, ANY_PACKAGE )
			&&	Parse( Str, TEXT("BONE="), BoneName )
			&&	Parse( Str, TEXT("TAG="), TagName ))			
			{						
				// Parse Optional adjustment.
				FRotator TagRotation;
				FVector  TagOrigin;
				TagOrigin = FVector(0.f,0.f,0.f);
				GetFVECTOR( Str, TagOrigin );				
				TagRotation = FRotator(0.f,0.f,0.f);
				GetFROTATOR( Str, TagRotation, 256 );
				FCoords TagCoords = GMath.UnitCoords / TagOrigin / TagRotation;
                //TagCoords = TagCoords.Transpose();

				if( !Mesh->SetAttachAlias( TagName, BoneName, TagCoords ) )
					Ar.Log( NAME_ExecWarning, TEXT("MESH ATTACHNAME bone not found in mesh.") );
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Bad MESH ATTACHNAME parameters.") );
			return 1;
		}
		else if( ParseCommand(&Str, TEXT("DEFAULTANIM")) )
		{
			// Link up a UMeshAnimation object to be the default animation repertoire for a skeletal mesh
			// Meant for backwards compatibility where calling a 'linkanim' would be difficult.
			UMesh* Mesh;
			UMeshAnimation* Anim;
			if
			(	ParseObject<UMesh>( Str, TEXT("MESH="), Mesh, ANY_PACKAGE )
			&&	ParseObject<UMeshAnimation>( Str, TEXT("ANIM="), Anim, ANY_PACKAGE ) )
			{
				if( Mesh->IsA(USkeletalMesh::StaticClass()))
					((USkeletalMesh*)Mesh)->DefaultAnim = Anim;
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Bad MESH DEFAULTANIM") );

		}
		// LodMeshes: parse LOD specific parameters.
		else if( ParseCommand(&Str,TEXT("LODPARAMS")) )
		{
			// Mesh origin.
			UMesh *Mesh;
			if( ParseObject<UMesh>(Str,TEXT("MESH="),Mesh,ANY_PACKAGE) )
			{
				// Ignore the LOD-specific parameters if Mesh is not a true ULodMesh.
				if( Mesh->IsA(ULodMesh::StaticClass()))
				{			
					// If not set, they keep their default values.
					ULodMesh* LodMesh = (ULodMesh*)Mesh;
					
					Parse(Str,TEXT("MINVERTS="),    LodMesh->LODMinVerts);
					Parse(Str,TEXT("STRENGTH="),    LodMesh->LODStrength);
					Parse(Str,TEXT("MORPH="),		LodMesh->LODMorph);
					Parse(Str,TEXT("HYSTERESIS="),	LodMesh->LODHysteresis);
					Parse(Str,TEXT("ZDISP="),       LodMesh->LODZDisplace);					

					// check validity
					if( (LodMesh->LODMorph < 0.0f) || (LodMesh->LODMorph >1.0f) )
					{
						LodMesh->LODMorph = 0.0f;
						Ar.Log( NAME_ExecWarning, TEXT("Bad LOD MORPH supplied."));	
					}
					if( (LodMesh->LODMinVerts < 0) || (LodMesh->LODMinVerts > LodMesh->ModelVerts) )
					{
						LodMesh->LODMinVerts = Max(10,LodMesh->ModelVerts);
						Ar.Log( NAME_ExecWarning, TEXT("Bad LOD MINVERTS supplied."));	
					}
					if( LodMesh->LODStrength < 0.00001f )
					{
						LodMesh->LODStrength = 0.0f;
					}
				}
				else Ar.Log( NAME_ExecWarning, TEXT("Need a LOD mesh (MLOD=1) for these LODPARAMS."));
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Bad MESH LODPARAMS") );
			return 1;
		}
#endif
		else if( ParseCommand(&Str,TEXT("ORIGIN")) )
		{
			// Mesh origin. 
			ULodMesh *Mesh;
			if( ParseObject<ULodMesh>(Str,TEXT("MESH="),Mesh,ANY_PACKAGE) )
			{
				if( Mesh->IsA(ULodMesh::StaticClass()))
				{
					ULodMesh *LodMesh = (ULodMesh*)Mesh;
					GetFVECTOR ( Str, LodMesh->Origin );
					GetFROTATOR( Str, LodMesh->RotOrigin, 256 );
				}
				else Ar.Log( NAME_ExecWarning, TEXT("Unsupported mesh class for ORIGIN"));
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Bad MESH ORIGIN") );
			return 1;
		}
		else if ( ParseCommand(&Str,TEXT("BOUNDINGBOX")) )
		{
			// Override automatically calculated bounding boxes.
			UMesh *Mesh;
			if( ParseObject<UMesh>(Str,TEXT("MESH="),Mesh,ANY_PACKAGE) )
			{
				Parse(Str,TEXT("XMIN="),Mesh->BoundingBox.Min.X);
				Parse(Str,TEXT("YMIN="),Mesh->BoundingBox.Min.Y);
				Parse(Str,TEXT("ZMIN="),Mesh->BoundingBox.Min.Z);
				Parse(Str,TEXT("XMAX="),Mesh->BoundingBox.Max.X);
				Parse(Str,TEXT("YMAX="),Mesh->BoundingBox.Max.Y);
				Parse(Str,TEXT("ZMAX="),Mesh->BoundingBox.Max.Z);
				
				debugf
				(
					NAME_Log,
					TEXT("Adjusted the bounding box: (%f,%f,%f),(%f,%f,%f)"),
					Mesh->BoundingBox.Min.X,
					Mesh->BoundingBox.Min.Y,
					Mesh->BoundingBox.Min.Z,
					Mesh->BoundingBox.Max.X,
					Mesh->BoundingBox.Max.Y,
					Mesh->BoundingBox.Max.Z					
				);
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Bad MESH Bounding Box") );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("SEQUENCE")) )
		{
			// Mesh vertex animation sequence assignment.
			UVertMesh *Mesh;
			FMeshAnimSeq Seq;
			if
			(	ParseObject<UVertMesh>( Str, TEXT("MESH="), Mesh, ANY_PACKAGE )
			&&	Parse( Str, TEXT("SEQ="), Seq.Name )
			&&	Parse( Str, TEXT("STARTFRAME="), Seq.StartFrame )
			&&	Parse( Str, TEXT("NUMFRAMES="), Seq.NumFrames ) )
			{

				Parse( Str, TEXT("RATE="), Seq.Rate );

				FName GroupName = NAME_None;
				Parse( Str, TEXT("GROUP="), GroupName );
				if( GroupName != NAME_None ) 
					Seq.AddGroup(GroupName);

				INT i;
				for( i=0; i<Mesh->MeshGetInstance(NULL)->GetAnimCount(); i++ )
					if( Mesh->AnimSeqs(i).Name==Seq.Name )
						break;
				if( i<Mesh->AnimSeqs.Num() )
					Mesh->AnimSeqs(i)=Seq;
				else
					new( Mesh->AnimSeqs )FMeshAnimSeq( Seq );

				Mesh->AnimSeqs.Shrink();
			}
			else Ar.Log(NAME_ExecWarning,TEXT("Bad MESH SEQUENCE"));
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("NOTIFY")) )
		{
			// Mesh notifications.
			UVertMesh* Mesh;
			FName SeqName;
			FMeshAnimNotify Notify;
			if
			(	ParseObject<UVertMesh>( Str, TEXT("MESH="), Mesh, ANY_PACKAGE )
			&&	Parse( Str, TEXT("SEQ="), SeqName )
			&&	Parse( Str, TEXT("TIME="), Notify.Time )
			&&	Parse( Str, TEXT("FUNCTION="), Notify.Function ) )
			{
				FMeshAnimSeq* Seq = ((UVertMeshInstance*)Mesh->MeshGetInstance(NULL))->GetAnimSeq( SeqName );
				if( Seq ) new( Seq->Notifys )FMeshAnimNotify( Notify );
				else Ar.Log( NAME_ExecWarning, TEXT("Unknown vertex animation sequence in MESH NOTIFY") );
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Bad MESH NOTIFY") );
			return 1;
		}
	}
	else if( ParseCommand( &Str, TEXT("AUDIO")) )//oldver
	{
		if( ParseCommand(&Str,TEXT("IMPORT")) )//oldver
		{
			FString File, Name, Group;
			Parse(Str,TEXT("FILE="),File);
			FString PkgName;
			if( !Parse( Str, TEXT("PACKAGE="), PkgName ) )
				PkgName = ParentContext ? ParentContext->GetName() : Level->GetOuter()->GetName();
			UPackage* Pkg = CreatePackage(NULL,*PkgName);
			Pkg->bDirty = 1;
			if( Parse(Str,TEXT("GROUP="),Group) && Group!=NAME_None )
				Pkg = CreatePackage( Pkg, *Group );
			FString Cmd = US + TEXT("NEW SOUND FILE=") + File + TEXT(" PACKAGE=") + PkgName;
			if( Parse(Str,TEXT("GROUP="),Group) )
				Cmd = Cmd + TEXT(".") + Group;
			if( Parse(Str,TEXT("NAME="),Name) )
				Cmd = Cmd + TEXT(" NAME=") + Name;
			return SafeExec( *Cmd, Ar ); 
		}
	}
	else if( ParseCommand( &Str, TEXT("PREFAB")) )
	{
		if( ParseCommand(&Str,TEXT("IMPORT")) )
		{
			FString File, Name, Group;
			Parse(Str,TEXT("FILE="),File);
			FString PkgName = ParentContext ? ParentContext->GetName() : Level->GetOuter()->GetName();
			Parse( Str, TEXT("PACKAGE="), PkgName );
			UPackage* Pkg = CreatePackage(NULL,*PkgName);
			Pkg->bDirty = 1;
			if( Parse(Str,TEXT("GROUP="),Group) && Group!=NAME_None )
				Pkg = CreatePackage( Pkg, *Group );
			FString Cmd = US + TEXT("NEW PREFAB FILE=") + File + TEXT(" PACKAGE=") + PkgName;
			if( Parse(Str,TEXT("GROUP="),Group) )
				Cmd = Cmd + TEXT(".") + Group;
			if( Parse(Str,TEXT("NAME="),Name) )
				Cmd = Cmd + TEXT(" NAME=") + Name;
			SafeExec( *Cmd, Ar ); 
		}
	}
	return 0;
	unguardf(( TEXT("(%s)"), InStr ));
}

/*-----------------------------------------------------------------------------
	UnrealEd command line.
-----------------------------------------------------------------------------*/

UModel* GBrush = NULL;
const TCHAR* GStream = NULL;
TCHAR TempStr[MAX_EDCMD], TempFname[MAX_EDCMD], TempName[MAX_EDCMD], Temp[MAX_EDCMD];
_WORD Word1, Word2, Word4;

UBOOL UEditorEngine::Exec_StaticMesh( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_StaticMesh);

	if(ParseCommand(&Str,TEXT("FROM")))
	{
		if(ParseCommand(&Str,TEXT("SELECTION")))	// STATICMESH FROM SELECTION PACKAGE=<name> NAME=<name>
		{
			Trans->Begin(TEXT("STATICMESH FROM SELECTION"));
			Level->Modify();
			FinishAllSnaps(Level);

			FName PkgName = NAME_None;
			Parse( Str, TEXT("PACKAGE="), PkgName );
			FName Name = NAME_None;
			Parse( Str, TEXT("NAME="), Name );
			if( PkgName != NAME_None  )
			{
				UPackage* Pkg = CreatePackage(NULL,*PkgName);
				Pkg->bDirty = 1;

				FName GroupName = NAME_None;
				if( Parse( Str, TEXT("GROUP="), GroupName ) && GroupName!=NAME_None )
					Pkg = CreatePackage(Pkg,*GroupName);

				TArray<FStaticMeshTriangle>	Triangles;
				TArray<FStaticMeshMaterial>	Materials;

				for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
				{
					AActor*	Actor = Level->Actors(ActorIndex);

					if( Actor && Actor->bSelected )
					{
						//
						// Static Mesh
						//

						if( Actor->DrawType == DT_StaticMesh && Actor->StaticMesh )
						{
							AStaticMeshActor* StaticMeshActor = Cast<AStaticMeshActor>(Actor);
							check(StaticMeshActor);
							check(StaticMeshActor->StaticMesh);

							if(!StaticMeshActor->StaticMesh->RawTriangles.Num())
								StaticMeshActor->StaticMesh->RawTriangles.Load();

							// Transform the static meshes triangles into worldspace.
							TArray<FStaticMeshTriangle> TempTriangles = StaticMeshActor->StaticMesh->RawTriangles;
							FMatrix	LocalToWorld = StaticMeshActor->LocalToWorld();

							for( INT x = 0 ; x < TempTriangles.Num() ; ++x )
							{
								FStaticMeshTriangle* Tri = &TempTriangles(x);
								Tri->MaterialIndex += Materials.Num();
								for( INT y = 0 ; y < 3 ; ++y )
								{
									Tri->Vertices[y]	= LocalToWorld.TransformFVector(Tri->Vertices[y]);
									Tri->Vertices[y]	-= GetPivotLocation() - Actor->PrePivot;
								}
							}
							
							// Add the into the main list
							Triangles += TempTriangles;
							Materials += StaticMeshActor->StaticMesh->Materials;
						}

						//
						// Mesh
						//

						if( Actor->DrawType == DT_Mesh && Actor->Mesh )
						{
							TArray<FStaticMeshTriangle> TempTriangles;
							GetActorTriangles(TempTriangles,Materials,Actor);

							// Transform the static meshes triangles into worldspace.
							FCoords	LocalToWorld = Actor->ToWorld();

							for( INT x = 0 ; x < TempTriangles.Num() ; ++x )
							{
								FStaticMeshTriangle* Tri = &TempTriangles(x);
								for( INT y = 0 ; y < 3 ; ++y )
								{
									Tri->Vertices[y]	= Tri->Vertices[y].TransformPointBy( LocalToWorld );

									Tri->Vertices[y]	-= GetPivotLocation() - Actor->PrePivot;
								}
							}

							Triangles += TempTriangles;
						}

						//
						// Brush
						//

						if( Actor->IsBrush() )
						{
							ABrush* Brush = Cast<ABrush>(Actor);
							check(Brush);

							TArray<FStaticMeshTriangle> TempTriangles;
							GetBrushTriangles( TempTriangles, Materials, Brush, Brush->Brush );

							// Transform the static meshes triangles into worldspace.
							FCoords	LocalToWorld = Brush->ToWorld();

							for( INT x = 0 ; x < TempTriangles.Num() ; ++x )
							{
								FStaticMeshTriangle* Tri = &TempTriangles(x);
								for( INT y = 0 ; y < 3 ; ++y )
								{
									Tri->Vertices[y]	= Tri->Vertices[y].TransformPointBy( LocalToWorld );

									Tri->Vertices[y]	-= GetPivotLocation() - Actor->PrePivot;
								}
							}

							Triangles += TempTriangles;
						}
					}
				}

				// Create the static mesh
				if( Triangles.Num() )
					CreateStaticMesh(Triangles,Materials,Pkg,Name);
			}

			Trans->End();
			RedrawLevel(Level);
			return 1;
		}
	}
	else if(ParseCommand(&Str,TEXT("TO")))
	{
		if(ParseCommand(&Str,TEXT("BRUSH")))
		{
			Trans->Begin(TEXT("STATICMESH TO BRUSH"));
			GBrush->Modify();

			// Find the first selected static mesh actor.

			AActor*	SelectedActor = NULL;

			for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
				if(Level->Actors(ActorIndex) && Level->Actors(ActorIndex)->bSelected && Level->Actors(ActorIndex)->StaticMesh != NULL)
				{
					SelectedActor = Level->Actors(ActorIndex);
					break;
				}

			if(SelectedActor)
			{
				Level->Brush()->Location = SelectedActor->Location;
				SelectedActor->Location = FVector(0,0,0);

				CreateModelFromStaticMesh(Level->Brush()->Brush,SelectedActor);

				SelectedActor->Location = Level->Brush()->Location;
			}
			else
				Ar.Logf(TEXT("No suitable actors found."));

			Trans->End();
			RedrawLevel(Level);
			return 1;
		}
	}
	if(ParseCommand(&Str,TEXT("REBUILD")))	// STATICMESH REBUILD
	{
		//
		// Forces a rebuild of the selected static mesh.
		//

		Trans->Begin(TEXT("staticmesh rebuild"));

		GWarn->BeginSlowTask(TEXT("Staticmesh rebuilding"),1 );

		if(CurrentStaticMesh)
		{
			CurrentStaticMesh->Modify();
			CurrentStaticMesh->Build();
		}

		GWarn->EndSlowTask();

		Trans->End();
	}
    // gam ---
	else if(ParseCommand(&Str,TEXT("ALLREBUILD")))	// STATICMESH ALLREBUILD
	{
		//
		// Forces a rebuild of the all static meshes.
		//

		Trans->Begin(TEXT("staticmesh rebuild"));
		GWarn->BeginSlowTask(TEXT("Staticmesh rebuilding"),1);

        for( TObjectIterator<UStaticMesh> ItA; *ItA; ++ItA )
        {
            for( TObjectIterator<AActor> ItB; *ItB; ++ItB )
            {
                if( ItB->StaticMesh != *ItA )
                    continue;

			    ItA->Modify();
			    ItA->Build();

                break;
            }
        }

		GWarn->EndSlowTask();
		Trans->End();
	}
    // --- gam
	else if(ParseCommand(&Str,TEXT("SMOOTH")))	// STATICMESH SMOOTH
	{
		//
		// Hack to set the smoothing mask of the triangles in the selected static meshes to 1.
		//

		Trans->Begin(TEXT("staticmesh smooth"));

		GWarn->BeginSlowTask(TEXT("Smoothing static meshes"),1);

		for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
		{
			AActor*	Actor = Level->Actors(ActorIndex);

			if(Actor && Actor->bSelected && Actor->StaticMesh)
			{
				UStaticMesh*	StaticMesh = Actor->StaticMesh;

				StaticMesh->Modify();

				// Generate smooth normals.

				if(!StaticMesh->RawTriangles.Num())
					StaticMesh->RawTriangles.Load();

				for(INT i = 0;i < StaticMesh->RawTriangles.Num();i++)
					StaticMesh->RawTriangles(i).SmoothingMask = 1;

				StaticMesh->Build();
			}
		}

		GWarn->EndSlowTask();

		Trans->End();
	}
	else if(ParseCommand(&Str,TEXT("UNSMOOTH")))	// STATICMESH UNSMOOTH
	{
		//
		// Hack to set the smoothing mask of the triangles in the selected static meshes to 0.
		//

		Trans->Begin(TEXT("staticmesh unsmooth"));

		GWarn->BeginSlowTask(TEXT("Unsmoothing static meshes"),1);

		for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
		{
			AActor*	Actor = Level->Actors(ActorIndex);

			if(Actor && Actor->bSelected && Actor->StaticMesh)
			{
				UStaticMesh*	StaticMesh = Actor->StaticMesh;

				if(!StaticMesh->RawTriangles.Num())
					StaticMesh->RawTriangles.Load();

				StaticMesh->Modify();

				// Generate smooth normals.

				for(INT i = 0;i < StaticMesh->RawTriangles.Num();i++)
				{
					FStaticMeshTriangle&	Triangle1 = StaticMesh->RawTriangles(i);

					Triangle1.SmoothingMask = 0;
				}

				StaticMesh->Build();
			}
		}

		GWarn->EndSlowTask();

		Trans->End();
	}
	else if( ParseCommand(&Str,TEXT("DEFAULT")) )	// STATICMESH DEFAULT NAME=<name>
	{
		CurrentStaticMesh=NULL;
		ParseObject<UStaticMesh>(Str,TEXT("NAME="),CurrentStaticMesh,ANY_PACKAGE);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("ADD")) )		// STATICMESH ADD NAME=<name> [ SNAP=<0/1> ]
	{
		UStaticMesh* StaticMesh;
		if( ParseObject<UStaticMesh>(Str,TEXT("NAME="),StaticMesh,ANY_PACKAGE) )
		{
			UClass* Class = AStaticMeshActor::StaticClass();
			AActor* Default   = Class->GetDefaultActor();
			FVector Collision = FVector(Default->CollisionRadius,Default->CollisionRadius,Default->CollisionHeight);
			INT bSnap;
			Parse(Str,TEXT("SNAP="),bSnap);
			if( bSnap )		Constraints.Snap( ClickLocation, FVector(0, 0, 0) );

			FVector NewLocation;
			FVector Location;
			if( GetFVECTOR( Str, NewLocation ) )
				Location = NewLocation + ClickPlane * (FBoxPushOut(ClickPlane,Collision) + 0.1);
			else
				Location = ClickLocation + ClickPlane * (FBoxPushOut(ClickPlane,Collision) + 0.1);

			if( bSnap )		Constraints.Snap( Location, FVector(0, 0, 0) );
			AActor* Actor = AddActor( Level, Class, Location );
			Actor->StaticMesh = StaticMesh;
			RedrawLevel(Level);
			return 1;
		}
	}
	else if( ParseCommand(&Str,TEXT("KACTORADD")) )		// STATICMESH KACTORADD NAME=<name> [ SNAP=<0/1> ]
	{
		UStaticMesh* StaticMesh;
		if( ParseObject<UStaticMesh>(Str,TEXT("NAME="),StaticMesh,ANY_PACKAGE) )
		{
			if(StaticMesh && StaticMesh->KPhysicsProps)
			{
				UClass* Class = AKActor::StaticClass();
				AActor* Default   = Class->GetDefaultActor();
				FVector Collision = FVector(Default->CollisionRadius,Default->CollisionRadius,Default->CollisionHeight);
				INT bSnap;
				Parse(Str,TEXT("SNAP="),bSnap);
				if( bSnap )		Constraints.Snap( ClickLocation, FVector(0, 0, 0) );
				
				FVector NewLocation;
				FVector Location;
				if( GetFVECTOR( Str, NewLocation ) )
					Location = NewLocation + ClickPlane * (FBoxPushOut(ClickPlane,Collision) + 0.1);
				else
					Location = ClickLocation + ClickPlane * (FBoxPushOut(ClickPlane,Collision) + 0.1);
				
				if( bSnap )		Constraints.Snap( Location, FVector(0, 0, 0) );
				AKActor* Actor = Cast<AKActor>(AddActor( Level, Class, Location ));
				Actor->StaticMesh = StaticMesh;
				RedrawLevel(Level);
				return 1;
			}
			else
			{
				Ar.Logf( NAME_ExecWarning, TEXT("Staticmesh (%s) has no physics properties/collision."), StaticMesh->GetName() );
			}
		}
	}
	else if( ParseCommand(&Str,TEXT("IMPORT")) )
	{
		FName PkgName = ParentContext ? ParentContext->GetFName() : NAME_None;
		Parse( Str, TEXT("Package="), PkgName );
		if( PkgName != NAME_None && Parse( Str, TEXT("FILE="), TempFname, ARRAY_COUNT(TempFname) ) )
		{
			UPackage* Pkg = CreatePackage(NULL,*PkgName);
			Pkg->bDirty = 1;
			if( !Parse( Str, TEXT("NAME="),  TempName,  NAME_SIZE ) )
			{
				// Deduce package name from filename.
				TCHAR* End = TempFname + appStrlen(TempFname);
				while( End>TempFname && End[-1]!=PATH_SEPARATOR[0] && End[-1]!='/' )
					End--;
				appStrncpy( TempName, End, NAME_SIZE );
				if( appStrchr(TempName,'.') )
					*appStrchr(TempName,'.') = 0;
			}

			UBOOL	Collision = 1;
			Parse(Str,TEXT("COLLISION="),Collision);

			GBuildStaticMeshCollision = Collision;

			GWarn->BeginSlowTask( TEXT("Importing static mesh"), 1);
			extern TCHAR* GFile;
			GFile = TempFname;
			FName GroupName = NAME_None;
			if( Parse( Str, TEXT("GROUP="), GroupName ) && GroupName!=NAME_None )
				Pkg = CreatePackage(Pkg,*GroupName);

			// If the texture already exists, save it's pointer.
			UStaticMesh* ExistingStaticMesh = Cast<UStaticMesh>(StaticFindObject( UStaticMesh::StaticClass(), Pkg, TempName ));
			UBOOL bStaticMeshExists = (ExistingStaticMesh != NULL);

			UStaticMesh* StaticMesh = ImportObject<UStaticMesh>( GEditor->Level, Pkg, TempName, RF_Public|RF_Standalone, TempFname );
			if( StaticMesh )
			{
				if( bStaticMeshExists )
				{
					Flush(0);
					RedrawLevel(Level);
				}

				debugf( NAME_Log, TEXT("Imported %s"), StaticMesh->GetFullName() );
			}
			else Ar.Logf( NAME_ExecWarning, TEXT("Import static mesh %s from %s failed"), TempName, TempFname );
			GBuildStaticMeshCollision = 1;
			GWarn->EndSlowTask();
			GCache.Flush( 0, ~0, 1 );
		}
		else Ar.Logf( NAME_ExecWarning, TEXT("Missing file or name") );
		return 1;
	}
	// Take the currently selected static mesh, and save the builder brush as its
	// low poly collision model.
	else if( ParseCommand(&Str,TEXT("SAVEBRUSHASCOLLISION")) )
	{
		// First, find the currently selected actor with a static mesh.
		// Fail if more than one actor with staticmesh is selected.
		AActor* SMActor = NULL;
		for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
		{
			AActor*	Actor = Level->Actors(ActorIndex);
			if(Actor && Actor->bSelected && Actor->StaticMesh)
			{
				if(!SMActor)
					SMActor = Actor;
				else
					return 1; // we already have one - more than one selected, so return.
			}
		}

		if(!SMActor)
			return 1;

		UStaticMesh* StaticMesh = SMActor->StaticMesh;

		// If we already have a collision model for this staticmesh, ask if we want to replace it.
		if(StaticMesh->CollisionModel)
		{
			UBOOL doReplace = appMsgf(1, TEXT("Static Mesh already has a collision model. \nDo you want to replace it with Builder Brush?"));
			if(doReplace)
				StaticMesh->CollisionModel = NULL;
			else
				return 1;
		}

		// Now get the builder brush.
		UModel* builderModel = Level->Brush()->Brush;

		// Need the transform between builder brush space and static mesh actor space.
		FMatrix BrushL2W = Level->Brush()->LocalToWorld();
		FMatrix MeshW2L = SMActor->WorldToLocal();
		FMatrix SMToBB = BrushL2W * MeshW2L;

		// Copy the current builder brush into the static mesh.
		StaticMesh->CollisionModel = new(StaticMesh->GetOuter()) UModel(NULL,1);
		StaticMesh->CollisionModel->Polys = new(StaticMesh->GetOuter()) UPolys;
		StaticMesh->CollisionModel->Polys->Element = builderModel->Polys->Element;

		// Now transform each poly into local space for the selected static mesh.
		for(INT i=0; i<StaticMesh->CollisionModel->Polys->Element.Num(); i++)
		{
			FPoly* Poly = &StaticMesh->CollisionModel->Polys->Element(i);

			for(INT j=0; j<Poly->NumVertices; j++ )
				Poly->Vertex[j]  = SMToBB.TransformFVector(Poly->Vertex[j]);

			Poly->Normal = SMToBB.TransformNormal(Poly->Normal);
			Poly->Normal.Normalize(); // SmToBB might have scaling in it.
		}

		// Build bounding box.
		StaticMesh->CollisionModel->BuildBound();

		// Build BSP for the brush.
		GEditor->bspBuild(StaticMesh->CollisionModel,BSP_Good,15,70,1,0);
		GEditor->bspRefresh(StaticMesh->CollisionModel,1);
		GEditor->bspBuildBounds(StaticMesh->CollisionModel);

#ifdef WITH_KARMA
		// Now - use this as the Karma collision for this static mesh as well.

		// If we dont already have physics props, construct them here.
		if(!StaticMesh->KPhysicsProps)
			 StaticMesh->KPhysicsProps = ConstructObject<UKMeshProps>(UKMeshProps::StaticClass(), StaticMesh);

		// Convert collision model into a collection of Karma convex hulls.
		// NB: This removes any convex hulls that were already part of the collision data.
		KModelToHulls(&StaticMesh->KPhysicsProps->AggGeom, StaticMesh->CollisionModel, FVector(0, 0, 0));

		// Then re-calc the mass props.
		KUpdateMassProps(StaticMesh->KPhysicsProps);
#endif

		// Finally mark the parent package as 'dirty', so user will be prompted if they want to save it etc.
		UObject* Outer = StaticMesh->GetOuter();
		while( Outer && Outer->GetOuter() )
			Outer = Outer->GetOuter();
		if( Outer && Cast<UPackage>(Outer) )
			Cast<UPackage>(Outer)->bDirty = 1;

		Ar.Logf(TEXT("Added collision model to StaticMesh %s."), StaticMesh->GetName() );
	}

	return 0;

	unguard;
}

UBOOL UEditorEngine::Exec_Brush( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_Brush);

	if( ParseCommand(&Str,TEXT("APPLYTRANSFORM")) )
		return Exec( TEXT("ACTOR APPLYTRANSFORM" ) );
	else if( ParseCommand(&Str,TEXT("SET")) )
	{
		Trans->Begin( TEXT("Brush Set") );
		GBrush->Modify();
		FRotator Temp(0.0f,0.0f,0.0f);
		Constraints.Snap( NULL, Level->Brush()->Location, FVector(0.f,0.f,0.f), Temp );
		FModelCoords TempCoords;
		Level->Brush()->BuildCoords( &TempCoords, NULL );
		Level->Brush()->Location -= Level->Brush()->PrePivot.TransformVectorBy( TempCoords.PointXform );
		Level->Brush()->PrePivot = FVector(0.f,0.f,0.f);
		GBrush->Polys->Element.Empty();
		UPolysFactory* It = new UPolysFactory;
		It->FactoryCreateText( Level,UPolys::StaticClass(), GBrush->Polys->GetOuter(), GBrush->Polys->GetName(), 0, GBrush->Polys, TEXT("t3d"), GStream, GStream+appStrlen(GStream), GWarn );
		// Do NOT merge faces.
		bspValidateBrush( GBrush, 0, 1 );
		GBrush->BuildBound();
		Trans->End();
		NoteSelectionChange( Level );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("MORE")) )
	{
		Trans->Continue();
		GBrush->Modify();
		UPolysFactory* It = new UPolysFactory;
		It->FactoryCreateText( Level,UPolys::StaticClass(), GBrush->Polys->GetOuter(), GBrush->Polys->GetName(), 0, GBrush->Polys, TEXT("t3d"), GStream, GStream+appStrlen(GStream), GWarn );
		// Do NOT merge faces.
		bspValidateBrush( Level->Brush()->Brush, 0, 1 );
		GBrush->BuildBound();
		Trans->End();	
		RedrawLevel( Level );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("RESET")) )
	{
		Trans->Begin( TEXT("Brush Reset") );
		Level->Brush()->Modify();
		Level->Brush()->InitPosRotScale();
		Trans->End();
		RedrawLevel(Level);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SCALE")) )
	{
		Trans->Begin( TEXT("Brush Scale") );

		FVector Scale;
		GetFVECTOR( Str, Scale );
		if( !Scale.X ) Scale.X = 1;
		if( !Scale.Y ) Scale.Y = 1;
		if( !Scale.Z ) Scale.Z = 1;

		FVector InvScale( 1 / Scale.X, 1 / Scale.Y, 1 / Scale.Z );

		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			ABrush* Brush = Cast<ABrush>(Level->Actors(i));
			if( Brush && Brush->bSelected && Brush->IsBrush() )
			{
				Brush->Brush->Modify();
				Brush->Brush->Polys->Element.ModifyAllItems();
				for( INT poly = 0 ; poly < Brush->Brush->Polys->Element.Num() ; poly++ )
				{
					FPoly* Poly = &(Brush->Brush->Polys->Element(poly));

					Poly->TextureU *= InvScale;
					Poly->TextureV *= InvScale;
					Poly->Base = ((Poly->Base - Brush->PrePivot) * Scale) + Brush->PrePivot;

					for( INT vtx = 0 ; vtx < Poly->NumVertices ; vtx++ )
						Poly->Vertex[vtx] = ((Poly->Vertex[vtx] - Brush->PrePivot) * Scale) + Brush->PrePivot;

					Poly->CalcNormal();
				}

				Brush->Brush->BuildBound();
			}
		}
		
		Trans->End();
		RedrawLevel(Level);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("MOVETO")) )
	{
		Trans->Begin( TEXT("Brush MoveTo") );
		Level->Brush()->Modify();
		GetFVECTOR( Str, Level->Brush()->Location );
		Trans->End();
		RedrawLevel(Level);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("MOVEREL")) )
	{
		Trans->Begin( TEXT("Brush MoveRel") );
		Level->Brush()->Modify();
		FVector TempVector( 0, 0, 0 );
		GetFVECTOR( Str, TempVector );
		Level->Brush()->Location.AddBounded( TempVector, HALF_WORLD_MAX1 );
		Trans->End();
		RedrawLevel(Level);
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("ADD")))
	{
		Trans->Begin( TEXT("Brush Add") );
		FinishAllSnaps(Level);
		INT DWord1=0;
		Parse( Str, TEXT("FLAGS="), DWord1 );
		Level->Modify();
		ABrush* NewBrush = csgAddOperation( Level->Brush(), Level, DWord1, CSG_Add );
		if( NewBrush )
			bspBrushCSG( NewBrush, Level->Model, DWord1, CSG_Add, 1 );
		Level->Model->ClearRenderData(GRenDev);
		Trans->End();
		RedrawLevel(Level);
		//EdCallback(EDC_MapChange,0,0);
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("ADDMOVER"))) // BRUSH ADDMOVER
	{
		if(CurrentStaticMesh)
		{
			Trans->Begin( TEXT("Brush AddMover") );
			Level->Modify();
			FinishAllSnaps( Level );

			UClass* MoverClass = NULL;
			ParseObject<UClass>( Str, TEXT("CLASS="), MoverClass, ANY_PACKAGE );
			if( !MoverClass || !MoverClass->IsChildOf(AMover::StaticClass()) )
				MoverClass = AMover::StaticClass();

			Level->Modify();
			AMover* Actor = (AMover*)Level->SpawnActor(MoverClass,NAME_None,Level->Brush()->Location);
			if( Actor )
			{
				Actor->StaticMesh = CurrentStaticMesh;
				Actor->SetDrawType(DT_StaticMesh);
				Actor->PostEditChange();
			}
			Trans->End();
			RedrawLevel(Level);
		}

		return 1;
	}
	else if (ParseCommand(&Str,TEXT("ADDVOLUME"))) // BRUSH ADDVOLUME
	{
		Trans->Begin( TEXT("Brush AddVolume") );
		Level->Modify();
		FinishAllSnaps( Level );

		UClass* VolumeClass = NULL;
		ParseObject<UClass>( Str, TEXT("CLASS="), VolumeClass, ANY_PACKAGE );
		if( !VolumeClass || !VolumeClass->IsChildOf(AVolume::StaticClass()) )
			VolumeClass = AVolume::StaticClass();

		Level->Modify();
		AVolume* Actor = (AVolume*)Level->SpawnActor(VolumeClass,NAME_None,Level->Brush()->Location);
		if( Actor )
		{
			csgCopyBrush
			(
				Actor,
				Level->Brush(),
				0,
				RF_Transactional,
				1
			);

			// Set the texture on all polys to NULL.  This stops invisible texture
			// dependencies from being formed on volumes.
			if( Actor->Brush )
				for( INT poly = 0 ; poly < Actor->Brush->Polys->Element.Num() ; ++poly )
				{
					FPoly* Poly = &(Actor->Brush->Polys->Element(poly));
					Poly->Material = NULL;
				}

			Actor->PostEditChange();
		}
		Trans->End();
		RedrawLevel(Level);
		return 1;
	}
	else if(ParseCommand(&Str,TEXT("ADDANTIPORTAL")))
	{
		Trans->Begin(TEXT("Brush AddAntiPortal"));

		AAntiPortalActor*	Actor = (AAntiPortalActor*) Level->SpawnActor(
															AAntiPortalActor::StaticClass(),
															NAME_None,
															Level->Brush()->Location
															);

		if(Actor)
		{
			UConvexVolume*	ConvexVolume = ConstructObject<UConvexVolume>(UConvexVolume::StaticClass(),Level->GetOuter());
			UPolys*			Polys = Level->Brush()->Brush->Polys;
			FMatrix			BrushToLocal = Level->Brush()->LocalToWorld() * FTranslationMatrix(-Level->Brush()->Location);

			for(INT PolygonIndex = 0;PolygonIndex < Polys->Element.Num();PolygonIndex++)
			{
				FPoly*	Poly = &Polys->Element(PolygonIndex);

				for(INT Side = 0;Side < 2;Side++)
				{
					if(Side == 0 || (Poly->PolyFlags & PF_TwoSided))
					{
						FConvexVolumeFace*	Face = new(ConvexVolume->Faces) FConvexVolumeFace;

						for(INT VertexIndex = 0;VertexIndex < Poly->NumVertices;VertexIndex++)
						{
							FVector	Vertex = BrushToLocal.TransformFVector(Poly->Vertex[Side ? Poly->NumVertices - VertexIndex - 1 : VertexIndex]);

							Face->Vertices.AddItem(Vertex);
							ConvexVolume->BoundingBox += Vertex;
						}

						Face->Plane = FPlane(Face->Vertices(0),Face->Vertices(1),Face->Vertices(2));
					}
				}
			}

			for(INT FaceIndex = 0;FaceIndex < ConvexVolume->Faces.Num();FaceIndex++)
			{
				FConvexVolumeFace*	Face = &ConvexVolume->Faces(FaceIndex);

				for(INT OtherFaceIndex = FaceIndex + 1;OtherFaceIndex < ConvexVolume->Faces.Num();OtherFaceIndex++)
				{
					FConvexVolumeFace*	OtherFace = &ConvexVolume->Faces(OtherFaceIndex);

					for(INT VertexIndex = 0;VertexIndex < Face->Vertices.Num();VertexIndex++)
					{
						for(INT OtherVertexIndex = 0;OtherVertexIndex < OtherFace->Vertices.Num();OtherVertexIndex++)
						{
							if(	Face->Vertices(VertexIndex) == OtherFace->Vertices((OtherVertexIndex + 1) % OtherFace->Vertices.Num()) &&
								Face->Vertices((VertexIndex + 1) % Face->Vertices.Num()) == OtherFace->Vertices(OtherVertexIndex))
							{
								FConvexVolumeEdge*	Edge = new(ConvexVolume->Edges) FConvexVolumeEdge;

								Edge->Faces[0] = FaceIndex;
								Edge->Vertices[0] = VertexIndex;

								Edge->Faces[1] = OtherFaceIndex;
								Edge->Vertices[1] = OtherVertexIndex;
							}
						}
					}
				}
			}

			Actor->AntiPortal = ConvexVolume;
			Actor->PostEditChange();
		}

		Trans->End();

		RedrawLevel(Level);
		//EdCallback(EDC_MapChange,0,0);
	}
	else if (ParseCommand(&Str,TEXT("SUBTRACT"))) // BRUSH SUBTRACT
		{
		Trans->Begin( TEXT("Brush Subtract") );
		FinishAllSnaps(Level);
		Level->Modify();
		ABrush* NewBrush = csgAddOperation(Level->Brush(),Level,0,CSG_Subtract); // Layer
		if( NewBrush )
			bspBrushCSG( NewBrush, Level->Model, 0, CSG_Subtract, 1 );
		Level->Model->ClearRenderData(GRenDev);
		Trans->End();
		RedrawLevel(Level);
		//EdCallback(EDC_MapChange,0,0);
		return 1;
		}
	else if (ParseCommand(&Str,TEXT("FROM"))) // BRUSH FROM INTERSECTION/DEINTERSECTION
	{
		if( ParseCommand(&Str,TEXT("INTERSECTION")) )
		{
			Ar.Log( TEXT("Brush from intersection") );
			Trans->Begin( TEXT("Brush From Intersection") );
			GBrush->Modify();
			FinishAllSnaps( Level );
			bspBrushCSG( Level->Brush(), Level->Model, 0, CSG_Intersect, 0 );
			Trans->End();
			RedrawLevel( Level );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("DEINTERSECTION")) )
		{
			Ar.Log( TEXT("Brush from deintersection") );
			Trans->Begin( TEXT("Brush From Deintersection") );
			GBrush->Modify();
			FinishAllSnaps( Level );
			bspBrushCSG( Level->Brush(), Level->Model, 0, CSG_Deintersect, 0 );
			Trans->End();
			RedrawLevel( Level );
			return 1;
		}
	}
	else if( ParseCommand (&Str,TEXT("NEW")) )
	{
		Trans->Begin( TEXT("Brush New") );
		GBrush->Modify();
		GBrush->Polys->Element.Empty();
		Trans->End();
		RedrawLevel( Level );
		return 1;
	}
	else if( ParseCommand (&Str,TEXT("LOAD")) ) // BRUSH LOAD
	{
		if( Parse( Str, TEXT("FILE="), TempFname, 256 ) )
		{
			Trans->Reset( TEXT("loading brush") );
			FVector TempVector = Level->Brush()->Location;
			LoadPackage( Level->GetOuter(), TempFname, 0 );
			Level->Brush()->Location = TempVector;
			bspValidateBrush( Level->Brush()->Brush, 0, 1 );
			Cleanse( 1, TEXT("loading brush") );
			return 1;
		}
	}
	else if( ParseCommand( &Str, TEXT("SAVE") ) )
	{
		if( Parse(Str,TEXT("FILE="),TempFname, 256) )
		{
			Ar.Logf( TEXT("Saving %s"), TempFname );
			SavePackage( Level->GetOuter(), GBrush, 0, TempFname, GWarn );
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("IMPORT")) )
	{
		if( Parse(Str,TEXT("FILE="),TempFname, 256) )
		{
			GWarn->BeginSlowTask( TEXT("Importing brush"), 1);
			Trans->Begin( TEXT("Brush Import") );
			GBrush->Polys->Modify();
			GBrush->Polys->Element.Empty();
			DWORD Flags=0;
			UBOOL Merge=0;
			ParseUBOOL( Str, TEXT("MERGE="), Merge );
			Parse( Str, TEXT("FLAGS="), Flags );
			GBrush->Linked = 0;
			ImportObject<UPolys>( GEditor->Level, GBrush->Polys->GetOuter(), GBrush->Polys->GetName(), 0, TempFname );
			if( Flags )
				for( Word2=0; Word2<TempModel->Polys->Element.Num(); Word2++ )
					GBrush->Polys->Element(Word2).PolyFlags |= Flags;
			for( INT i=0; i<GBrush->Polys->Element.Num(); i++ )
				GBrush->Polys->Element(i).iLink = i;
			if( Merge )
			{
				bspMergeCoplanars( GBrush, 0, 1 );
				bspValidateBrush( GBrush, 0, 1 );
			}
			Trans->End();
			GWarn->EndSlowTask();
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("EXPORT")))
	{
		if( Parse(Str,TEXT("FILE="),TempFname, 256) )
		{
			GWarn->BeginSlowTask( TEXT("Exporting brush"), 1);
			UExporter::ExportToFile( GBrush->Polys, NULL, TempFname );
			GWarn->EndSlowTask();
		}
		else Ar.Log(NAME_ExecWarning,TEXT("Missing filename"));
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("MERGEPOLYS")) ) // BRUSH MERGEPOLYS
	{
		// Merges the polys on all selected brushes
		GWarn->BeginSlowTask( TEXT(""), 1);
		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Merging polys on selected brushes") );
			AActor* Actor = Level->Actors(i);
			if( Actor && Actor->bSelected && Actor->IsBrush() )
				bspValidateBrush( Actor->Brush, 1, 1 );
		}
		RedrawLevel( Level );
		GWarn->EndSlowTask();
	}
	else if( ParseCommand(&Str,TEXT("SEPARATEPOLYS")) ) // BRUSH SEPARATEPOLYS
	{
		GWarn->BeginSlowTask( TEXT(""), 1);
		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Separating polys on selected brushes") );
			AActor* Actor = Level->Actors(i);
			if( Actor && Actor->bSelected && Actor->IsBrush() )
				bspUnlinkPolys( Actor->Brush );
		}
		RedrawLevel( Level );
		GWarn->EndSlowTask();
	}
    // amb, jij ---
    // brush static_import file=c:\shared\lava.lwo
	else if( ParseCommand( &Str, TEXT("STATIC_IMPORT")) )
    {
        if (Exec_StaticMeshImportToEditor(Str, Ar))
            return 1;
    }
    // --- amb, jij

	return 0;

	unguard;
}

UBOOL UEditorEngine::Exec_Paths( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_Paths);

	if (ParseCommand(&Str,TEXT("BUILD")))
	{
		FPathBuilder builder;
		Trans->Reset( TEXT("Paths") );
		Level->Modify();
		INT numpaths = builder.buildPaths( Level );
		RedrawLevel( Level );
		Ar.Logf( TEXT("Built Paths: %d"), numpaths );
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("REMOVE")))
	{
		FPathBuilder builder;
		Trans->Reset( TEXT("Paths") );
		INT numpaths = builder.removePaths( Level );
		RedrawLevel( Level );
		Ar.Logf( TEXT("Removed %d Paths"), numpaths );
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("UNDEFINE")))
	{
		FPathBuilder builder;
		Trans->Reset( TEXT("Paths") );
		builder.undefinePaths( Level );
		RedrawLevel(Level);
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("DEFINE")))
	{
		FPathBuilder builder;
		Trans->Reset( TEXT("Paths") );
		builder.definePaths( Level );
		RedrawLevel(Level);
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("DEFINECHANGED")))
	{
		FPathBuilder builder;
		Trans->Reset( TEXT("Paths") );
		builder.defineChangedPaths( Level );
		RedrawLevel(Level);
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("REVIEW")))
	{
		FPathBuilder builder;
		Trans->Reset( TEXT("Paths") );
		builder.ReviewPaths( Level );
		RedrawLevel(Level);
		return 1;
	}
	return 0;

	unguard;
}

UBOOL UEditorEngine::Exec_BSP( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_BSP);

	if( ParseCommand( &Str, TEXT("REBUILD")) ) // Bsp REBUILD [LAME/GOOD/OPTIMAL] [BALANCE=0-100] [LIGHTS] [MAPS] [REJECT]
	{
		Trans->Reset( TEXT("Rebuilding Bsp") ); // Not tracked transactionally
		Ar.Log(TEXT("Bsp Rebuild"));

		FRebuildOptions* Options = GRebuildTools.GetCurrent();

		GWarn->BeginSlowTask( TEXT("Rebuilding Terrain Arrays"), 1);
		Level->UpdateTerrainArrays();

		GWarn->StatusUpdatef( 0, 0, TEXT("Building polygons") );
		bspBuildFPolys( Level->Model, 1, 0 );

		GWarn->StatusUpdatef( 0, 0, TEXT("Merging planars") );
		bspMergeCoplanars( Level->Model, 0, 0 );

		GWarn->StatusUpdatef( 0, 0, TEXT("Partitioning") );
		bspBuild( Level->Model, Options->BspOpt, Options->Balance, Options->PortalBias, 0, 0 );

		GWarn->StatusUpdatef( 0, 0, TEXT("Building visibility zones") );
		TestVisibility( Level, Level->Model, 0, 0 );

		GWarn->StatusUpdatef( 0, 0, TEXT("Optimizing geometry") );
		bspOptGeom( Level->Model );

		Level->Model->ClearRenderData(GRenDev);

		// Empty EdPolys.
		Level->Model->Polys->Element.Empty();

		for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
		{
			if(Level->Actors(ActorIndex))
				Level->Actors(ActorIndex)->ClearRenderData();
		}

		// Attach projectors.
		guard(ProjectorAttach);
		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			AProjector* Projector = Cast<AProjector>(Level->Actors(i));
			if( Projector )
				Projector->Attach();
		}
		unguard;

		GWarn->EndSlowTask();
		Flush(0);
		RedrawLevel(Level);
		EdCallback( EDC_MapChange, 0, 0 );

		return 1;
	}

	return 0;

	unguard;
}

UBOOL UEditorEngine::Exec_Light( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_Light);

	if( ParseCommand( &Str, TEXT("APPLY") ) )
	{
		UBOOL	SelectedOnly = 0,
				ChangedOnly = 0;

		ParseUBOOL(Str,TEXT("SELECTED="),SelectedOnly);
		ParseUBOOL(Str,TEXT("CHANGED="),ChangedOnly);

		shadowIlluminateBsp(Level,SelectedOnly,ChangedOnly);

		Flush(0);
		RedrawLevel( Level );

		return 1;
	}

	return 0;

	unguard;
}

UBOOL UEditorEngine::Exec_Map( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_Map);

	if (ParseCommand(&Str,TEXT("GRID"))) // MAP GRID [SHOW3D=ON/OFF] [SHOW2D=ON/OFF] [X=..] [Y=..] [Z=..]
	{
		//
		// Before changing grid, force editor to current grid position to avoid jerking:
		//
		FinishAllSnaps (Level);
		GetFVECTOR( Str, Constraints.GridSize );
		RedrawLevel(Level);
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("ROTGRID"))) // MAP ROTGRID [PITCH=..] [YAW=..] [ROLL=..]
	{
		FinishAllSnaps (Level);
		if( GetFROTATOR( Str, Constraints.RotGridSize, 256 ) )
			RedrawLevel(Level);
		return 1;
	}
	else if (ParseCommand(&Str,TEXT("SELECT")))
	{
		Trans->Begin( TEXT("Select"), false ); // gam
		if( ParseCommand(&Str,TEXT("ADDS")) )
			mapSelectOperation( Level, CSG_Add );
		else if( ParseCommand(&Str,TEXT("SUBTRACTS")) )
			mapSelectOperation( Level, CSG_Subtract );
		else if( ParseCommand(&Str,TEXT("SEMISOLIDS")) )
			mapSelectFlags( Level, PF_Semisolid );
		else if( ParseCommand(&Str,TEXT("NONSOLIDS")) )
			mapSelectFlags( Level, PF_NotSolid );
		else if( ParseCommand(&Str,TEXT("FIRST")) )
			mapSelectFirst( Level );
		else if( ParseCommand(&Str,TEXT("LAST")) )
			mapSelectLast( Level );
		Trans->End ();
		RedrawLevel( Level );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("DELETE")) )
	{
		Exec( TEXT("ACTOR DELETE"), Ar );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("BRUSH")) )
	{
		if( ParseCommand (&Str,TEXT("GET")) )
		{
			Trans->Begin( TEXT("Brush Get") );
			mapBrushGet( Level );
			Trans->End();
			RedrawLevel( Level );
			return 1;
		}
		else if( ParseCommand (&Str,TEXT("PUT")) )
		{
			Trans->Begin( TEXT("Brush Put") );
			mapBrushPut( Level );
			Trans->End();
			RedrawLevel( Level );
			return 1;
		}
	}
	else if (ParseCommand(&Str,TEXT("SENDTO")))
	{
		if( ParseCommand(&Str,TEXT("FIRST")) )
		{
			Trans->Begin( TEXT("Map SendTo Front") );
			mapSendToFirst( Level );
			Trans->End();
			RedrawLevel( Level );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("LAST")) )
		{
			Trans->Begin( TEXT("Map SendTo Back") );
			mapSendToLast( Level );
			Trans->End();
			RedrawLevel( Level );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("SWAP")) )
		{
			Trans->Begin( TEXT("Map SendTo Swap") );
			mapSendToSwap( Level );
			Trans->End();
			RedrawLevel( Level );
			return 1;
		}
	}
	else if( ParseCommand(&Str,TEXT("REBUILD")) )
	{
		Trans->Reset( TEXT("rebuilding map") );
		GWarn->BeginSlowTask( TEXT("Rebuilding geometry"), 1);
		Level->GetLevelInfo()->bPathsRebuilt = 0;

		csgRebuild( Level );

		Level->Model->ClearRenderData(GRenDev);

		GWarn->StatusUpdatef( 0, 0, TEXT("Cleaning up...") );

		for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
		{
			if(Level->Actors(ActorIndex))
				Level->Actors(ActorIndex)->ClearRenderData();
		}

		Flush(0);
		RedrawLevel( Level );
		EdCallback( EDC_MapChange, 0, 1 );
		GWarn->EndSlowTask();

		return 1;
	}
	else if( ParseCommand (&Str,TEXT("NEW")) )
	{
		Trans->Reset( TEXT("clearing map") );
		Level->RememberActors();
		ResetLoaders( Level->GetOuter(), 0, 1 );
		Level = new( Level->GetOuter(), TEXT("MyLevel") )ULevel( this, 0 );
		Level->ReconcileActors();
		GTerrainTools.Init();
		ResetSound();
		NoteSelectionChange( Level );
		EdCallback(EDC_MapChange,0,1);
		Cleanse( 1, TEXT("starting new map") );
        Trans->HasBeenSaved(); // gam
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("LOAD") ) )
	{
		if( Parse( Str, TEXT("FILE="), TempFname, 256 ) )
		{
			// Make a list of actors in other levels
			// so we don't clean these up after level change.
			TArray<AActor*> ExternalActors;
			for( TObjectIterator<AActor> It; It; ++It )
				if( Level->Actors.FindItemIndex(*It) == INDEX_NONE )
					ExternalActors.AddItem(*It);

			Trans->Reset( TEXT("loading map") );
			GWarn->BeginSlowTask( TEXT("Loading map"), 1);
			Level->RememberActors();
			ResetLoaders( Level->GetOuter(), 0, 1 );
			EdClearLoadErrors();
			Flush(0);
			LoadPackage( Level->GetOuter(), TempFname, 0 );
			Level->Engine = this;
			Level->ReconcileActors();
			//GTerrainTools.Init();
			ResetSound();
			bspValidateBrush( Level->Brush()->Brush, 0, 1 );
			GWarn->EndSlowTask();
			// Hack fix for actor location being undefined (NAN)
			for ( INT i=0; i<Level->Actors.Num(); i++ )
				if ( Level->Actors(i) && (Level->Actors(i)->Location != Level->Actors(i)->Location) )
					Level->DestroyActor(Level->Actors(i));
			EdCallback( EDC_MapChange, 1, 1 );
			NoteSelectionChange( Level );
			Level->SetFlags( RF_Transactional );
			Level->Model->SetFlags( RF_Transactional );
			if( Level->Model->Polys ) Level->Model->Polys->SetFlags( RF_Transactional );
			// Clean up any actors which aren't in the actor list,
			// unless they're in other levels (eg the anim browser)
			for( TObjectIterator<AActor> It; It; ++It )
			{
				if( Level->Actors.FindItemIndex(*It)==INDEX_NONE )
				{
					if( ExternalActors.FindItemIndex(*It)==INDEX_NONE )
						It->bDeleteMe=1;
				}
				else
				{
					It->bDeleteMe=0;
					if( Cast<ACamera>(*It) )
						It->ClearFlags( RF_Transactional );
					else
						It->SetFlags( RF_Transactional );
					It->PostEditLoad();
				}
			}
			Cleanse( 0, TEXT("loading map") );
			
			// Legend - load cameras positions
			for( INT ii=0; ii<Client->Viewports.Num(); ii++ )
			{
				UViewport* Viewport = Client->Viewports(ii);
				AActor* Camera = Viewport->Actor;
	
				switch( Viewport->Actor->RendMap )
				{
					case REN_OrthXY: // XY stored in XY, zoom in Z
						Camera->Location.X = Level->GetLevelInfo()->CameraLocationTop.X;
						Camera->Location.Y = Level->GetLevelInfo()->CameraLocationTop.Y;
						if( Level->GetLevelInfo()->CameraLocationTop.Z )
							Viewport->Actor->OrthoZoom = Level->GetLevelInfo()->CameraLocationTop.Z;
						break;
					case REN_OrthXZ: // XZ stored in XY, zoom in Z
						Camera->Location.X = Level->GetLevelInfo()->CameraLocationFront.X;
						Camera->Location.Z = Level->GetLevelInfo()->CameraLocationFront.Y;
						if( Level->GetLevelInfo()->CameraLocationFront.Z )
							Viewport->Actor->OrthoZoom = Level->GetLevelInfo()->CameraLocationFront.Z;
						break;
					case REN_OrthYZ: // YZ stored in XY, zoom in Z
						Camera->Location.Y = Level->GetLevelInfo()->CameraLocationSide.X;
						Camera->Location.Z = Level->GetLevelInfo()->CameraLocationSide.Y;
						if( Level->GetLevelInfo()->CameraLocationSide.Z )
							Viewport->Actor->OrthoZoom = Level->GetLevelInfo()->CameraLocationSide.Z;
						break;
					case REN_Wire:
					case REN_Zones:	
					case REN_Polys:	
					case REN_PolyCuts:
					case REN_DynLight:
					case REN_PlainTex:
					case REN_LightingOnly:
						Camera->Location = Level->GetLevelInfo()->CameraLocationDynamic;
						Camera->Rotation = Level->GetLevelInfo()->CameraRotationDynamic;
						break;
				}
			}

			if( GEdLoadErrors.Num() )
				EdCallback(EDC_DisplayLoadErrors,0,1);
            Trans->HasBeenSaved(); // gam
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
		EdCallback(EDC_MapChange,0,1);
		return 1;
	}
	else if( ParseCommand (&Str,TEXT("SAVE")) )
	{
		if( Parse(Str,TEXT("FILE="),TempFname, 256) )
		{
			INT Autosaving = 0;  // Are we autosaving?
			Parse(Str,TEXT("AUTOSAVE="),Autosaving);

	        // gam ---
	        for( INT i = 0 ; i < Level->Actors.Num() ; i++ )
	        {
		        AActor* pActor = Level->Actors(i);
		        
		        if( pActor && ( pActor->IsA(AStaticMeshActor::StaticClass()) || pActor->IsA(AMover::StaticClass()) ) && !pActor->StaticMesh )
		        {
        			debugf( NAME_Warning, TEXT("Removing degenerate %s %s."), pActor->GetClass()->GetName(), pActor->GetName() );
			        GEditor->Level->DestroyActor( pActor );
			    }
	        }
	        // --- gam

			// Remove stray Matinee preview-only actor duplicates.
			for( INT i = 0 ; i < Level->Actors.Num() ; i++ )
	        {
		        AActor* pActor = Level->Actors(i);		        
		        if( pActor &&  pActor->IsA( AMatDemoActor::StaticClass() ) )
		        {
			        GEditor->Level->DestroyActor( pActor );
			    }
	        }

			Level->ShrinkLevel();
			Level->CleanupDestroyed( 1 );
			ALevelInfo* OldInfo = FindObject<ALevelInfo>(Level->GetOuter(),TEXT("LevelInfo0"));
			if( OldInfo && OldInfo!=Level->GetLevelInfo() )
				OldInfo->Rename();
			if( Level->GetLevelInfo()!=OldInfo )
				Level->GetLevelInfo()->Rename(TEXT("LevelInfo0"));
			ULevelSummary* Summary = Level->GetLevelInfo()->Summary = new(Level->GetOuter(),TEXT("LevelSummary"),RF_Public)ULevelSummary;

            // gam ---
            Summary->Title                      = Level->GetLevelInfo()->Title;
            Summary->Author                     = Level->GetLevelInfo()->Author;
			Summary->Description				= Level->GetLevelInfo()->Description;
            Summary->Screenshot                 = Level->GetLevelInfo()->Screenshot;
            Summary->DecoTextName               = Level->GetLevelInfo()->DecoTextName;

            Summary->IdealPlayerCountMin        = Level->GetLevelInfo()->IdealPlayerCountMin;
            Summary->IdealPlayerCountMax        = Level->GetLevelInfo()->IdealPlayerCountMax;

            Summary->HideFromMenus              = Level->GetLevelInfo()->HideFromMenus;

            Summary->SinglePlayerTeamSize       = Level->GetLevelInfo()->SinglePlayerTeamSize;

            Summary->LevelEnterText             = Level->GetLevelInfo()->LevelEnterText;
            // --- gam

			// mo --- hack for Onslaught - add ONSPowerLinkOfficialSetup actors' info to Summary->ExtraInfo
			if (Level->GetLevelInfo()->ExtraInfo == TEXT(""))
			{
				UBOOL bFirstSetup = true;
				for (INT x = 0; x < Level->Actors.Num(); x++)
				{
					AActor *Actor = Level->Actors(x);

					if (Actor)
					{
						FString ActorName = Actor->GetName();
						if (ActorName.InStr("ONSPowerLinkOfficialSetup") != -1)
						{
							UProperty* Property=FindField<UProperty>(Actor->GetClass(), TEXT("SetupName"));
							if(Property)
							{
								TCHAR Temp[1024]=TEXT("");
								Property->ExportText(0, Temp, (BYTE*)Actor, (BYTE*)Actor, PPF_Localized);

								//convert spaces to underscores
								FString TempFString = Temp;
								INT i = TempFString.Caps().InStr(TEXT(" "));
								FString NewTemp = TEXT("");
								while (i != -1)
								{
									NewTemp += TempFString.Left(i) + TEXT("_");
									TempFString = TempFString.Mid(i + 1);
									i = TempFString.Caps().InStr(TEXT(" "));
								}
								NewTemp += TempFString;
								Property->ImportText(*NewTemp, (BYTE*)Actor + Property->Offset, PPF_Localized);

								if (bFirstSetup)
								{
									Summary->ExtraInfo = TEXT("LinkSetups=");
									bFirstSetup = false;
								}
								else
									Summary->ExtraInfo += TEXT(";");
								Summary->ExtraInfo += NewTemp;
							}
						}
					}
				}
			}
			else
				Summary->ExtraInfo = Level->GetLevelInfo()->ExtraInfo;
			// --- mo


            // gam ---
            if( !Autosaving && FString(TempFname).InStr( TEXT("Autoplay") ) < 0 )
                Level->CheckDefaultGameType( TempFname );
            // --- gam

			// Flag portal and antiportal brushes so they're loaded on the client.
			for(FStaticBrushIterator BrushIt(Level);BrushIt;++BrushIt)
			{
				UBOOL	LoadOnClient = 0;

				if(BrushIt->PolyFlags & (PF_AntiPortal | PF_Portal))
				{
					LoadOnClient = 1;
				}
				else
				{
					for(INT PolyIndex = 0;PolyIndex < BrushIt->Brush->Polys->Element.Num();PolyIndex++)
					{
						if(BrushIt->Brush->Polys->Element(PolyIndex).PolyFlags & (PF_AntiPortal | PF_Portal))
						{
							LoadOnClient = 1;
							break;
						}
					}
				}

				if(LoadOnClient)
				{
					BrushIt->ClearFlags(RF_NotForClient | RF_NotForServer);
					BrushIt->Brush->ClearFlags(RF_NotForClient | RF_NotForServer);
					BrushIt->Brush->Polys->ClearFlags(RF_NotForClient | RF_NotForServer);
				}
				else
				{
					BrushIt->SetFlags(RF_NotForClient | RF_NotForServer);
					BrushIt->Brush->SetFlags(RF_NotForClient | RF_NotForServer);
					BrushIt->Brush->Polys->SetFlags(RF_NotForClient | RF_NotForServer);
				}
			}

			// Check for duplicate actor indices.
			INT	NumDuplicates = 0;
			for( INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++ )
			{
				AActor*	Actor = Level->Actors(ActorIndex);

				if(Actor)
					for(INT OtherActorIndex = ActorIndex + 1;OtherActorIndex < Level->Actors.Num();OtherActorIndex++)
						if(Level->Actors(OtherActorIndex) == Actor)
						{
							Level->Actors(OtherActorIndex) = NULL;
							NumDuplicates++;
						}
			}
			Level->CompactActors();
			debugf(TEXT("Removed duplicate actor indices: %u duplicates found."),NumDuplicates);

			// Legend - save camera positions
			for( INT ii=0; ii<Client->Viewports.Num(); ii++ )
			{
				UViewport* Viewport = Client->Viewports(ii);
				AActor* Camera = Viewport->Actor;
				switch( Viewport->Actor->RendMap )
				{
					case REN_OrthXY: // XY stored in XY, zoom in Z
						Level->GetLevelInfo()->CameraLocationTop.X = Camera->Location.X;
						Level->GetLevelInfo()->CameraLocationTop.Y = Camera->Location.Y;
						Level->GetLevelInfo()->CameraLocationTop.Z = Viewport->Actor->OrthoZoom;
						break;
					case REN_OrthXZ: // XZ stored in XY, zoom in Z
						Level->GetLevelInfo()->CameraLocationFront.X = Camera->Location.X;
						Level->GetLevelInfo()->CameraLocationFront.Y = Camera->Location.Z;
						Level->GetLevelInfo()->CameraLocationFront.Z = Viewport->Actor->OrthoZoom;
						break;
					case REN_OrthYZ: // YZ stored in XY, zoom in Z	
						Level->GetLevelInfo()->CameraLocationSide.X = Camera->Location.Y;
						Level->GetLevelInfo()->CameraLocationSide.Y = Camera->Location.Z;
						Level->GetLevelInfo()->CameraLocationSide.Z = Viewport->Actor->OrthoZoom;
						break;
					case REN_Wire:
					case REN_Zones:	
					case REN_Polys:	
					case REN_PolyCuts:
					case REN_DynLight:
					case REN_PlainTex:
					case REN_LightingOnly:
						Level->GetLevelInfo()->CameraLocationDynamic = Camera->Location;
						Level->GetLevelInfo()->CameraRotationDynamic = Camera->Rotation; 
						break;
				}
			}

			if( !Autosaving )	GWarn->BeginSlowTask( TEXT("Saving map"), 1);
			if( !SavePackage( Level->GetOuter(), Level, 0, TempFname, GWarn ) )
				appMsgf( 0, TEXT("Couldn't save package - maybe file is read-only?") );
			else if ( !Autosaving )
			{
				// Automatically export the .ucl file for maps when they are saved
				FString MapName = TempFname;

				// Strip away any leading path stuff - only want the mapname + extension
				if ( MapName.InStr(TEXT("\\")) != INDEX_NONE )
					MapName = MapName.Mid(MapName.InStr(TEXT("\\"),1)+1);

				ExportCacheMap(*MapName);
			}
			if( !Autosaving )	GWarn->EndSlowTask();
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("IMPORT") ) )
	{
		Word1=1;
		DoImportMap:
		if( Parse( Str, TEXT("FILE="), TempFname, 256) )
		{
			Trans->Reset( TEXT("importing map") );
			GWarn->BeginSlowTask( TEXT("Importing map"), 1);
			Level->RememberActors();
			if( Word1 )
				Level = new( Level->GetOuter(), TEXT("MyLevel") )ULevel( this, 0 );
			ImportObject<ULevel>( Level, Level->GetOuter(), Level->GetFName(), RF_Transactional, TempFname );
			Flush(0);
			Level->ReconcileActors();
			GTerrainTools.Init();
			ResetSound();
			if( Word1 )
				SelectNone( Level, 0 );
			GWarn->EndSlowTask();
			EdCallback( EDC_MapChange, 0, 1 );
			NoteSelectionChange( Level );
			Cleanse( 1, TEXT("importing map") );
		}
		else Ar.Log(NAME_ExecWarning,TEXT("Missing filename"));
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("IMPORTADD") ) )
	{
		Word1=0;
		SelectNone( Level, 0 );
		goto DoImportMap;
	}
	else if (ParseCommand (&Str,TEXT("EXPORT")))
	{
		if (Parse(Str,TEXT("FILE="),TempFname, 256))
		{
			GWarn->BeginSlowTask( TEXT("Exporting map"), 1);
			UExporter::ExportToFile( Level, NULL, TempFname );
			GWarn->EndSlowTask();
		}
		else Ar.Log(NAME_ExecWarning,TEXT("Missing filename"));
		return 1;
	}
	else if (ParseCommand (&Str,TEXT("SETBRUSH"))) // MAP SETBRUSH (set properties of all selected brushes)
	{
		Trans->Begin( TEXT("Set Brush Properties") );

		Word1  = 0;  // Properties mask
		INT DWord1 = 0;  // Set flags
		INT DWord2 = 0;  // Clear flags
		INT CSGOper = 0;  // CSG Operation
		INT DrawType = 0;  // Draw type

		FName GroupName=NAME_None;
		if (Parse(Str,TEXT("CSGOPER="),CSGOper))		Word1 |= MSB_CSGOper;
		if (Parse(Str,TEXT("COLOR="),Word2))			Word1 |= MSB_BrushColor;
		if (Parse(Str,TEXT("GROUP="),GroupName))		Word1 |= MSB_Group;
		if (Parse(Str,TEXT("SETFLAGS="),DWord1))		Word1 |= MSB_PolyFlags;
		if (Parse(Str,TEXT("CLEARFLAGS="),DWord2))		Word1 |= MSB_PolyFlags;
		if (Parse(Str,TEXT("DRAWTYPE="),DrawType))		Word1 |= MSB_DrawType;

		mapSetBrush(Level,(EMapSetBrushFlags)Word1,Word2,GroupName,DWord1,DWord2,CSGOper,DrawType);

		Trans->End();
		RedrawLevel(Level);

		return 1;
	}
	else if (ParseCommand (&Str,TEXT("SAVEPOLYS")))
	{
		if (Parse(Str,TEXT("FILE="),TempFname, 256))
		{
			UBOOL DWord2=1;
			ParseUBOOL(Str, TEXT("MERGE="), DWord2 );

			GWarn->BeginSlowTask( TEXT("Exporting map polys"), 1);
			GWarn->StatusUpdatef( 0, 0, TEXT("Building polygons") );
			bspBuildFPolys( Level->Model, 0, 0 );

			if (DWord2)
			{
				GWarn->StatusUpdatef( 0, 0, TEXT("Merging planars") );
				bspMergeCoplanars	(Level->Model,0,1);
			}
			UExporter::ExportToFile( Level->Model->Polys, NULL, TempFname );
			Level->Model->Polys->Element.Empty();

			GWarn->EndSlowTask 	();
			RedrawLevel(Level);
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
		return 1;
	}
	else if (ParseCommand (&Str,TEXT("CHECK")))
	{
		// Checks the map for common errors
		GWarn->MapCheck_Show();
		GWarn->MapCheck_Clear();

		GWarn->BeginSlowTask( TEXT("Checking map"), 1);
		for( INT i=0; i<GEditor->Level->Actors.Num(); i++ )
		{
			GWarn->StatusUpdatef( 0, i, TEXT("Checking map") );
			AActor* pActor = GEditor->Level->Actors(i);
			if( pActor )
				pActor->CheckForErrors();
		}
		GWarn->EndSlowTask();

		return 1;
	}
	else if (ParseCommand (&Str,TEXT("MEMSTAT")))
	{
		FLevelStat levelStats;

		Ar.Logf( NAME_Log, TEXT("[MEMSTAT] Analyzing map: '%s'"), Level->GetFullName() );
		Ar.Logf( NAME_Log, TEXT("============================================================") );

		levelStats.Analyze( Level );

		if( ParseCommand(&Str,TEXT("DETAIL")) )
		{
			levelStats.PrintTextureDetails( Ar );
		}

		levelStats.PrintTextureSummary( Ar ); 
		//levelStats.PrintGeomSummary( Ar );

		Ar.Logf( NAME_Log,
		TEXT("============================================================") );
	}
	else if (ParseCommand (&Str,TEXT("SCALE")))
	{
		FLOAT Factor = 1;
		if( !Parse(Str,TEXT("FACTOR="),Factor) ) return 0;

		Trans->Begin( TEXT("Map Scaling") );

		UBOOL bAdjustLights=0;
		ParseUBOOL( Str, TEXT("ADJUSTLIGHTS="), bAdjustLights );
		UBOOL bScaleSprites=0;
		ParseUBOOL( Str, TEXT("SCALESPRITES="), bScaleSprites );
		UBOOL bScaleLocations=0;
		ParseUBOOL( Str, TEXT("SCALELOCATIONS="), bScaleLocations );
		UBOOL bScaleCollision=0;
		ParseUBOOL( Str, TEXT("SCALECOLLISION="), bScaleCollision );

		GWarn->BeginSlowTask( TEXT("Scaling"), 1);
		NoteActorMovement( Level );
		for( INT i=0; i<GEditor->Level->Actors.Num(); i++ )
		{
			GWarn->StatusUpdatef( 0, i, TEXT("Scaling") );
			AActor* Actor = GEditor->Level->Actors(i);
			if( Actor && Actor->bSelected )
			{
				Actor->Modify();
				if( Actor->IsBrush() )
				{
					ABrush* Brush = Cast<ABrush>(Actor);

					Brush->Brush->Polys->Element.ModifyAllItems();
					for( INT poly = 0 ; poly < Brush->Brush->Polys->Element.Num() ; poly++ )
					{
						FPoly* Poly = &(Brush->Brush->Polys->Element(poly));

						Poly->TextureU /= Factor;
						Poly->TextureV /= Factor;
						Poly->Base = ((Poly->Base - Brush->PrePivot) * Factor) + Brush->PrePivot;

						for( INT vtx = 0 ; vtx < Poly->NumVertices ; vtx++ )
							Poly->Vertex[vtx] = ((Poly->Vertex[vtx] - Brush->PrePivot) * Factor) + Brush->PrePivot;

						Poly->CalcNormal();
					}

					Brush->Brush->BuildBound();
				}
				else if( Actor->DrawType == DT_StaticMesh || Actor->DrawType == DT_Mesh )
				{
					Actor->DrawScale *= Factor;
				}
				else if( bScaleSprites && Actor->DrawType == DT_Sprite )
				{
					Actor->DrawScale *= Factor;
				}

				if( bScaleLocations )
				{
					Actor->Location.X *= Factor;
					Actor->Location.Y *= Factor;
					Actor->Location.Z *= Factor;
				}
				if( bScaleCollision )
				{
					Actor->CollisionHeight *= Factor;
					Actor->CollisionRadius *= Factor;
				}

				if( bAdjustLights && Actor->LightType != LT_None )
				{
					Actor->LightBrightness *= Factor;
					Actor->LightRadius *= Factor;
				}

				Actor->PostEditChange();
			}
		}
		GWarn->EndSlowTask();

		Trans->End();

		return 1;
	}

	return 0;

	unguard;
}

UBOOL UEditorEngine::Exec_Select( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_Select);

	if( ParseCommand(&Str,TEXT("NONE")) )
	{
		Trans->Begin( TEXT("Select None"), false ); // gam
		SelectNone( Level, 1 );
		Trans->End();
		RedrawLevel( Level );
		return 1;
	}

	return 0;

	unguard;
}

UBOOL UEditorEngine::Exec_Poly( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_Poly);

	if( ParseCommand(&Str,TEXT("SELECT")) ) // POLY SELECT [ALL/NONE/INVERSE] FROM [LEVEL/SOLID/GROUP/ITEM/ADJACENT/MATCHING]
	{
		appSprintf( TempStr, TEXT("POLY SELECT %s"), Str );
		if( ParseCommand(&Str,TEXT("NONE")) )
		{
			return Exec( TEXT("SELECT NONE") );
		}
		else if( ParseCommand(&Str,TEXT("ALL")) )
		{
			Trans->Begin( TempStr, false ); // gam
			SelectNone( Level, 0 );
			polySelectAll( Level->Model );
			NoteSelectionChange( Level );
			Trans->End();
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("REVERSE")) )
		{
			Trans->Begin( TempStr, false ); // gam
			polySelectReverse (Level->Model);
			EdCallback(EDC_SelPolyChange,0,0);
			Trans->End();
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("MATCHING")) )
		{
			Trans->Begin( TempStr, false ); // gam
			if 		(ParseCommand(&Str,TEXT("GROUPS")))		polySelectMatchingGroups(Level->Model);
			else if (ParseCommand(&Str,TEXT("ITEMS")))		polySelectMatchingItems(Level->Model);
			else if (ParseCommand(&Str,TEXT("BRUSH")))		polySelectMatchingBrush(Level->Model);
			else if (ParseCommand(&Str,TEXT("TEXTURE")))	polySelectMatchingTexture(Level->Model);
			EdCallback(EDC_SelPolyChange,0,0);
			Trans->End();
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("ADJACENT")) )
		{
			Trans->Begin( TempStr, false ); // gam
			if 	  (ParseCommand(&Str,TEXT("ALL")))			polySelectAdjacents( Level->Model );
			else if (ParseCommand(&Str,TEXT("COPLANARS")))	polySelectCoplanars( Level->Model );
			else if (ParseCommand(&Str,TEXT("WALLS")))		polySelectAdjacentWalls( Level->Model );
			else if (ParseCommand(&Str,TEXT("FLOORS")))		polySelectAdjacentFloors( Level->Model );
			else if (ParseCommand(&Str,TEXT("CEILINGS")))	polySelectAdjacentFloors( Level->Model );
			else if (ParseCommand(&Str,TEXT("SLANTS")))		polySelectAdjacentSlants( Level->Model );
			EdCallback(EDC_SelPolyChange,0,0);
			Trans->End();
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("MEMORY")) )
		{
			Trans->Begin( TempStr, false ); // gam
			if 		(ParseCommand(&Str,TEXT("SET")))		polyMemorizeSet( Level->Model );
			else if (ParseCommand(&Str,TEXT("RECALL")))		polyRememberSet( Level->Model );
			else if (ParseCommand(&Str,TEXT("UNION")))		polyUnionSet( Level->Model );
			else if (ParseCommand(&Str,TEXT("INTERSECT")))	polyIntersectSet( Level->Model );
			else if (ParseCommand(&Str,TEXT("XOR")))		polyXorSet( Level->Model );
			EdCallback(EDC_SelPolyChange,0,0);
			Trans->End();
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("ZONE")) )
		{
			Trans->Begin( TempStr, false ); // gam
			polySelectZone(Level->Model);
			EdCallback(EDC_SelPolyChange,0,0);
			Trans->End();
			return 1;
		}
		RedrawLevel(Level);
	}
	else if( ParseCommand(&Str,TEXT("DEFAULT")) ) // POLY DEFAULT <variable>=<value>...
	{
		CurrentMaterial=NULL;
		ParseObject<UMaterial>(Str,TEXT("TEXTURE="),CurrentMaterial,ANY_PACKAGE);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("EXTRUDE")) )	// POLY EXTRUDE DEPTH=<value>
	{
		Trans->Begin( TEXT("Poly Extrude") );

		INT Depth;
		Parse( Str, TEXT("DEPTH="), Depth );

		Level->Modify();

		// Get a list of all the selected polygons.
		TArray<FPoly> SelectedPolys;	// The selected polygons.
		TArray<AActor*> ActorList;		// The actors that own the polys (in synch with SelectedPolys)

		for( INT x = 0 ; x < Level->Model->Surfs.Num() ; x++ )
		{
			FBspSurf* Surf = &(Level->Model->Surfs(x));
			check(Surf->Actor);
			if( Surf->PolyFlags & PF_Selected )
			{
				FPoly Poly;
				if( polyFindMaster( Level->Model, x, Poly ) )
				{
					new( SelectedPolys )FPoly( Poly );
					ActorList.AddItem( Surf->Actor );
				}
			}
		}

		for( INT x = 0 ; x < SelectedPolys.Num() ; x++ )
		{
			ActorList(x)->Brush->Polys->Element.ModifyAllItems();

			// Find all the polys which are linked to create this surface.
			TArray<FPoly> PolyList;
			polyGetLinkedPolys( (ABrush*)ActorList(x), &SelectedPolys(x), &PolyList );

			// Get a list of the outer edges of this surface.
			TArray<FEdge> EdgeList;
			polyGetOuterEdgeList( &PolyList, &EdgeList );

			// Create new polys from the edges of the selected surface.
			for( INT edge = 0 ; edge < EdgeList.Num() ; edge++ )
			{
				FEdge* Edge = &EdgeList(edge);

				FVector v1 = Edge->Vertex[0],
					v2 = Edge->Vertex[1];

				FPoly NewPoly;
				NewPoly.Init();
				NewPoly.NumVertices = 4;
				NewPoly.Vertex[0] = v1;
				NewPoly.Vertex[1] = v2;
				NewPoly.Vertex[2] = v2 + (SelectedPolys(x).Normal * Depth);
				NewPoly.Vertex[3] = v1 + (SelectedPolys(x).Normal * Depth);

				new(ActorList(x)->Brush->Polys->Element)FPoly( NewPoly );
			}

			// Create the cap polys.
			for( INT pl = 0 ; pl < PolyList.Num() ; pl++ )
			{
				FPoly* PolyFromList = &PolyList(pl);

				for( INT poly = 0 ; poly < ActorList(x)->Brush->Polys->Element.Num() ; poly++ )
					if( *PolyFromList == ActorList(x)->Brush->Polys->Element(poly) )
					{
						FPoly* Poly = &(ActorList(x)->Brush->Polys->Element(poly));
						for( INT vtx = 0 ; vtx < Poly->NumVertices ; vtx++ )
							Poly->Vertex[vtx] += (SelectedPolys(x).Normal * Depth);
						break;
					}
			}

			// Clean up the polys.
			for( INT poly = 0 ; poly < ActorList(x)->Brush->Polys->Element.Num() ; poly++ )
			{
				FPoly* Poly = &(ActorList(x)->Brush->Polys->Element(poly));
				Poly->iLink = poly;
				Poly->Normal = FVector(0,0,0);
				Poly->Finalize(0);
				Poly->Base = Poly->Vertex[0];
			}

			ActorList(x)->Brush->BuildBound();
		}

		EdCallback( EDC_RedrawAllViewports, 0, 0 );
		Trans->End();
	}
	else if( ParseCommand(&Str,TEXT("BEVEL")) )	// POLY BEVEL DEPTH=<value> BEVEL=<value>
	{
		Trans->Begin( TEXT("Poly Bevel") );

		INT Depth, Bevel;
		Parse( Str, TEXT("DEPTH="), Depth );
		Parse( Str, TEXT("BEVEL="), Bevel );

		Level->Modify();

		// Get a list of all the selected polygons.
		TArray<FPoly> SelectedPolys;	// The selected polygons.
		TArray<AActor*> ActorList;		// The actors that own the polys (in synch with SelectedPolys)

		for( INT x = 0 ; x < Level->Model->Surfs.Num() ; x++ )
		{
			FBspSurf* Surf = &(Level->Model->Surfs(x));
			check(Surf->Actor);
			if( Surf->PolyFlags & PF_Selected )
			{
				FPoly Poly;
				if( polyFindMaster( Level->Model, x, Poly ) )
				{
					new( SelectedPolys )FPoly( Poly );
					ActorList.AddItem( Surf->Actor );
				}
			}
		}

		for( INT x = 0 ; x < SelectedPolys.Num() ; x++ )
		{
			ActorList(x)->Brush->Polys->Element.ModifyAllItems();

			// Find all the polys which are linked to create this surface.
			TArray<FPoly> PolyList;
			polyGetLinkedPolys( (ABrush*)ActorList(x), &SelectedPolys(x), &PolyList );

			// Get a list of the outer edges of this surface.
			TArray<FEdge> EdgeList;
			polyGetOuterEdgeList( &PolyList, &EdgeList );

			// Figure out where the center of the poly is.
			FVector PolyCenter = FVector(0,0,0);
			for( INT edge = 0 ; edge < EdgeList.Num() ; edge++ )
				PolyCenter += EdgeList(edge).Vertex[0];
			PolyCenter /= EdgeList.Num();

			// Create new polys from the edges of the selected surface.
			for( INT edge = 0 ; edge < EdgeList.Num() ; edge++ )
			{
				FEdge* Edge = &EdgeList(edge);

				FVector v1 = Edge->Vertex[0],
					v2 = Edge->Vertex[1];

				FPoly NewPoly;
				NewPoly.Init();
				NewPoly.NumVertices = 4;
				NewPoly.Vertex[0] = v1;
				NewPoly.Vertex[1] = v2;

				FVector CenterDir = PolyCenter - v2;
				CenterDir.Normalize();
				NewPoly.Vertex[2] = v2 + (SelectedPolys(x).Normal * Depth) + (CenterDir * Bevel);

				CenterDir = PolyCenter - v1;
				CenterDir.Normalize();
				NewPoly.Vertex[3] = v1 + (SelectedPolys(x).Normal * Depth) + (CenterDir * Bevel);

				new(ActorList(x)->Brush->Polys->Element)FPoly( NewPoly );
			}

			// Create the cap polys.
			for( INT pl = 0 ; pl < PolyList.Num() ; pl++ )
			{
				FPoly* PolyFromList = &PolyList(pl);

				for( INT poly = 0 ; poly < ActorList(x)->Brush->Polys->Element.Num() ; poly++ )
					if( *PolyFromList == ActorList(x)->Brush->Polys->Element(poly) )
					{
						FPoly* Poly = &(ActorList(x)->Brush->Polys->Element(poly));
						for( INT vtx = 0 ; vtx < Poly->NumVertices ; vtx++ )
						{
							FVector CenterDir = PolyCenter - Poly->Vertex[vtx];
							CenterDir.Normalize();
							Poly->Vertex[vtx] += (CenterDir * Bevel);

							Poly->Vertex[vtx] += (SelectedPolys(x).Normal * Depth);
						}
						break;
					}
			}

			// Clean up the polys.
			for( INT poly = 0 ; poly < ActorList(x)->Brush->Polys->Element.Num() ; poly++ )
			{
				FPoly* Poly = &(ActorList(x)->Brush->Polys->Element(poly));
				Poly->iLink = poly;
				Poly->Normal = FVector(0,0,0);
				Poly->Finalize(0);
				Poly->Base = Poly->Vertex[0];
			}

			ActorList(x)->Brush->BuildBound();
		}

		EdCallback( EDC_RedrawAllViewports, 0, 0 );
		Trans->End();
	}
	else if( ParseCommand(&Str,TEXT("SETTEXTURE")) )
	{
		Trans->Begin( TEXT("Poly SetTexture") );
		Level->Model->ModifySelectedSurfs(1);
		for( INT Index1=0; Index1<Level->Model->Surfs.Num(); Index1++ )
		{
			if( Level->Model->Surfs(Index1).PolyFlags & PF_Selected )
			{
				Level->Model->Surfs(Index1).Material = CurrentMaterial;
				polyUpdateMaster( Level->Model, Index1, 0 );
			}
		}
		Level->Model->ClearRenderData(GRenDev);
		Trans->End();
		RedrawLevel(Level);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SET")) ) // POLY SET <variable>=<value>...
	{
		Trans->Begin( TEXT("Poly Set") );
		Level->Model->ModifySelectedSurfs( 1 );
		DWORD Ptr;
		if( !Parse(Str,TEXT("TEXTURE="),Ptr) )   Ptr = 0;
		UTexture *Texture = (UTexture*)Ptr;
		if( Texture )
		{
			for( INT x = 0 ; x < Level->Model->Surfs.Num() ; ++x )
			{
				if( Level->Model->Surfs(x).PolyFlags & PF_Selected )
				{
					Level->Model->Surfs(x).Material = Texture;
					polyUpdateMaster( Level->Model, x, 0 );
				}
			}
		}
		Word4  = 0;
		INT DWord1 = 0;
		INT DWord2 = 0;
		if (Parse(Str,TEXT("SETFLAGS="),DWord1))   Word4=1;
		if (Parse(Str,TEXT("CLEARFLAGS="),DWord2)) Word4=1;
		if (Word4)  polySetAndClearPolyFlags (Level->Model,DWord1,DWord2,1,1); // Update selected polys' flags
		Level->Model->ClearRenderData(GRenDev);
		Trans->End();
		RedrawLevel(Level);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("TEXSCALE")) ) // POLY TEXSCALE [U=..] [V=..] [UV=..] [VU=..]
	{
		Trans->Begin( TEXT("Poly Texscale") );
		Level->Model->ModifySelectedSurfs( 1 );

		Word2 = 1; // Scale absolute
		if( ParseCommand(&Str,TEXT("RELATIVE")) )
			Word2=0;

		TexScale:

		// Ensure each polygon has unique texture vector indices.

		for(INT SurfaceIndex = 0;SurfaceIndex < Level->Model->Surfs.Num();SurfaceIndex++)
		{
			FBspSurf&	Surf = Level->Model->Surfs(SurfaceIndex);

			if(Surf.PolyFlags & PF_Selected)
			{
				FVector	TextureU = Level->Model->Vectors(Surf.vTextureU),
						TextureV = Level->Model->Vectors(Surf.vTextureV);

				Surf.vTextureU = Level->Model->Vectors.AddItem(TextureU);
				Surf.vTextureV = Level->Model->Vectors.AddItem(TextureV);
			}
		}

		FLOAT UU,UV,VU,VV;
		UU=1.0; Parse (Str,TEXT("UU="),UU);
		UV=0.0; Parse (Str,TEXT("UV="),UV);
		VU=0.0; Parse (Str,TEXT("VU="),VU);
		VV=1.0; Parse (Str,TEXT("VV="),VV);

		polyTexScale( Level->Model, UU, UV, VU, VV, Word2 );

		Trans->End();
		RedrawLevel( Level );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("TEXMULT")) ) // POLY TEXMULT [U=..] [V=..]
	{
		Trans->Begin( TEXT("Poly Texmult") );
		Level->Model->ModifySelectedSurfs( 1 );
		Word2 = 0; // Scale relative;
		goto TexScale;
	}
	else if( ParseCommand(&Str,TEXT("TEXPAN")) ) // POLY TEXPAN [RESET] [U=..] [V=..]
	{
		Trans->Begin( TEXT("Poly Texpan") );
		Level->Model->ModifySelectedSurfs( 1 );

		// Ensure each polygon has a unique base point index.

		for(INT SurfaceIndex = 0;SurfaceIndex < Level->Model->Surfs.Num();SurfaceIndex++)
		{
			FBspSurf&	Surf = Level->Model->Surfs(SurfaceIndex);

			if(Surf.PolyFlags & PF_Selected)
			{
				FVector	Base = Level->Model->Points(Surf.pBase);

				Surf.pBase = Level->Model->Points.AddItem(Base);
			}
		}

		if( ParseCommand (&Str,TEXT("RESET")) )
			polyTexPan( Level->Model, 0, 0, 1 );
		INT	PanU = 0; Parse (Str,TEXT("U="),PanU);
		INT	PanV = 0; Parse (Str,TEXT("V="),PanV);
		polyTexPan( Level->Model, PanU, PanV, 0 );
		Trans->End();
		RedrawLevel( Level );
		return 1;
	}

	return 0;

	unguard;
}

UBOOL UEditorEngine::Exec_Texture( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_Texture);

	if(ParseCommand(&Str,TEXT("PUBLICHACK")))
	{
		//
		// Hack #3: Make sure all materials in memory have the RF_Public flag.
		// The first material versions artists started working with had a bug
		// which caused objects constructed for editinline variables in the
		// property window to not have RF_Public.
		//

		for(TObjectIterator<UMaterial> It;It;++It)
		{
			if(!(It->GetFlags() & RF_Public))
			{
				UObject*	Package = *It;

				while(Package->GetOuter())
					Package = Package->GetOuter();

				if(Package != GetTransientPackage())
				{
					CastChecked<UPackage>(Package)->bDirty = 1;
					It->SetFlags(RF_Public);
				}
			}
		}
	}
	else if(ParseCommand(&Str,TEXT("PALETTEHACK")))
	{
		//
		// Hack to copy the red channel to the alpha channel in a texture's palette
		//
		UTexture* Texture;
		if(ParseObject<UTexture>(Str,TEXT("NAME="),Texture,ANY_PACKAGE))
		{
			Ar.Logf(TEXT("Hacking alpha channel in palette for texture %s."), Texture->GetName());
			UPalette* P = Texture->Palette;
			for( INT i=0;i<256;i++ )
				P->Colors(i).A = P->Colors(i).R;
		}

		return 1;
	}
	else if(ParseCommand(&Str,TEXT("DETAILHACK")))
	{
		//
		// Hack to clear all mipmaps of a texture to grey.
		// Used to make detail textures that fade with distance.
		//

		UTexture*	Texture;

		if(ParseObject<UTexture>(Str,TEXT("NAME="),Texture,ANY_PACKAGE))
		{
			Texture->CreateMips(1,1);

			for(INT MipIndex = 1;MipIndex < Texture->Mips.Num();MipIndex++)
			{
				Texture->Mips(MipIndex).DataArray.Load();

				if(Texture->Format == TEXF_RGBA8)
				{
					FColor*	DataPtr = (FColor*) &Texture->Mips(MipIndex).DataArray(0);
					FLOAT	GreyFactor = (FLOAT) (MipIndex * MipIndex);
					INT		Width = Texture->USize >> MipIndex,
							Height = Texture->VSize >> MipIndex;

					for(INT Y = 0;Y < Height;Y++)
					{
						for(INT X = 0;X < Width;X++)
						{
							FPlane	Color = DataPtr->Plane();

							*DataPtr++ = FColor((Color + FPlane(0.5f,0.5f,0.5f,0.5f) * GreyFactor) / (GreyFactor + 1.0f));
						}
					}

					Texture->bRealtimeChanged = 1;
				}
				else
					Ar.Logf(TEXT("Unsupported texture format."));
			}
		}
		else
			Ar.Logf(TEXT("Unknown texture."));

		return 1;
	}
	else if(ParseCommand(&Str,TEXT("COMPRESS")))
	{
		guard(Exec_Texture COMPRESS);
		UTexture*	Texture;
		UCubemap*	Cubemap;
		
		if(ParseObject<UCubemap>(Str,TEXT("NAME="),Cubemap,ANY_PACKAGE))
		{
			FString FormatStr;
			if( Parse( Str, TEXT("FORMAT="), FormatStr ) )
			{
				ETextureFormat Format = TEXF_DXT1;
				if( FormatStr == TEXT("DXT3") )		Format = TEXF_DXT3;
				else if( FormatStr == TEXT("DXT5") )	Format = TEXF_DXT5;

				for( INT i=0; i<6; i++ )
				{
					UTexture* Texture = Cubemap->Faces[i];
					if( Texture )
					{
						if( !Texture->Compress( Format, 1 ) )				
							Ar.Logf(TEXT("Compression on texture '%s' failed."), Texture->GetPathName());
					}
					else
						Ar.Logf(TEXT("Missing face in cubemap '%s'."), Cubemap->GetPathName());
				}
			}
		}
		else
		if(ParseObject<UTexture>(Str,TEXT("NAME="),Texture,ANY_PACKAGE))
		{			
			FString FormatStr;
			if( Parse( Str, TEXT("FORMAT="), FormatStr ) )
			{
				ETextureFormat Format = TEXF_DXT1;
				if( FormatStr == TEXT("DXT3") )		Format = TEXF_DXT3;
				else if( FormatStr == TEXT("DXT5") )	Format = TEXF_DXT5;

				if( !Texture->Compress( Format, 1 ) )				
					Ar.Logf(TEXT("Compression on texture '%s' failed."), Texture->GetPathName());									
			}
		}
		else
			Ar.Logf(TEXT("Unknown texture."));

		return 1;
		unguard;
	}
	else if( ParseCommand(&Str,TEXT("CLEAR")) )
	{
		UTexture* Texture;
		if( ParseObject<UTexture>(Str,TEXT("NAME="),Texture,ANY_PACKAGE) )
			Texture->Clear( TCLEAR_Temporal );

		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SCALE")) )
	{
		FLOAT DeltaScale;
		Parse( Str, TEXT("DELTA="), DeltaScale );
		if( DeltaScale <= 0 )
		{
			Ar.Logf( NAME_ExecWarning, TEXT("Invalid DeltaScale setting") );
			return 1;
		}

		// get the current viewport
		UViewport* CurrentViewport = GetCurrentViewport();
		if( CurrentViewport == NULL )
		{
			Ar.Logf( NAME_ExecWarning, TEXT("Current viewport not found") );
			return 1;
		}

		// get the selected texture package
		UObject* Pkg = CurrentViewport->MiscRes;
		if( Pkg && CurrentViewport->Group!=NAME_None )
			Pkg = FindObject<UPackage>( Pkg, *CurrentViewport->Group );

		// Make the list.
		FMemMark Mark(GMem);
		enum {MAX=16384};
		UTexture** List = new(GMem,MAX)UTexture*;
		INT n = 0;
		for( TObjectIterator<UTexture> It; It && n<MAX; ++It )
			if( It->IsIn(Pkg) )
				List[n++] = *It;

		// scale the textures in the list relative to their old values
		for( INT i=0; i<n; i++ )
		{
			//!!MAT
			// what did this used to do?

			//UTexture* Texture = List[i];
			//Texture->Scale *= DeltaScale;
		}
		Mark.Pop();
		return 1;
	}
	// Texture Culling added by Legend on 4/12/2000
	//
	// Editor Command: TEXTURE CULL
	//
	// Build a "ReferencedTextures" list of all textures referenced on surfaces 
	// (Surfs and Mover Polys).  This is the visible texture list.
	//
	// Then, traverse all polys in the level, eliminating textures that
	// are not contained in the ReferencedTextures list.
	//
	// When the level is saved, all back-facing textures (textures that were
	// beling loaded -- consuming memory -- but never visible to the player,
	// will have been removed.)
	//

	//!!MAT
	else if( ParseCommand(&Str,TEXT("CULL")) )
	{
		TArray<UMaterial*> ReferencedTextures;
		TArray<UMaterial*> CulledTextures;

		for( TArray<AActor*>::TIterator It1(Level->Actors); It1; ++It1 )
		{
			AActor* Actor = *It1;
			if( Actor )
			{
				UModel* M = Actor->IsA(ALevelInfo::StaticClass()) ? Actor->GetLevel()->Model : Actor->Brush;
				if( M )
				{
					for( TArray<FBspSurf>::TIterator ItS(M->Surfs); ItS; ++ItS )
					{
						if( ItS->Material )
						{
							ReferencedTextures.AddUniqueItem( ItS->Material );
						}
					}

					if( M->Polys && Actor->IsA(AMover::StaticClass()) )
					{
						for( TArray<FPoly>::TIterator ItP(M->Polys->Element); ItP; ++ItP )
						{
							if( ItP->Material )
							{
								ReferencedTextures.AddUniqueItem( ItP->Material );
							}
						}
					}
				}
			}
		}
		for( TArray<AActor*>::TIterator It2(Level->Actors); It2; ++It2 )
		{
			AActor* Actor = *It2;
			if( Actor )
			{
				UModel* M = Actor->IsA(ALevelInfo::StaticClass()) ? Actor->GetLevel()->Model : Actor->Brush;
				if( M && M->Polys )
				{
					for( TArray<FPoly>::TIterator ItP(M->Polys->Element); ItP; ++ItP )
					{
						if( ItP->Material )
						{
							// if poly isn't in the list, kill it
							if( ReferencedTextures.FindItemIndex( ItP->Material ) == INDEX_NONE )
							{
								CulledTextures.AddUniqueItem( ItP->Material );
								ItP->Material = 0;
							}
						}
					}
				}
			}
		}
		GLog->Logf( TEXT("TEXTURE CULLING SUMMARY") );
		GLog->Logf( TEXT("  REFERENCED") );
		for( TArray<UMaterial*>::TIterator ItR(ReferencedTextures); ItR; ++ItR )
		{
			GLog->Logf( TEXT("    %s"), (*ItR)->GetFullName() );
		}
		GLog->Logf( TEXT("  CULLED") );
		for( TArray<UMaterial*>::TIterator ItC(CulledTextures); ItC; ++ItC )
		{
			GLog->Logf( TEXT("    %s"), (*ItC)->GetFullName() );
		}
		return 1;
	}
	// Batch Detail Texture Editing added by Legend on 4/12/2000
	//
	// Editor Command: TEXTURE CLEARDETAIL
	//
	//		Clear the "current detail texture"
	//
	// Editor Command: TEXTURE SETDETAIL
	//
	//		Set the "current detail texture" to the current the texture browser selection
	//
	// Editor Command: TEXTURE APPLYDETAIL [OVERRIDE]
	//
	//		Apply the "current detail texture" to the texture selected in the texture browser
	//
	// Editor Command: TEXTURE REPLACEDETAIL
	//
	//		Search through all texture packages for occurrences of detail textures that
	//		match the texture currently selected in the texture browser.  If a match
	//		is found, replace the texture's detail texture with the "current detail texture."
	//
	// Editor Command: TEXTURE BATCHAPPLY DETAIL=[DetailTextureName | None]
	//                 [PREFIX=TextureNameMatchingPrefix] [OVERRIDE=[TRUE | FALSE]]
	//
	//		Search through all texture packages optionally searching for matches
	//		against the "TextureNameMatchingPrefix" for all textures found, apply
	//		DetailTextureName as the new detail texture.  If a detail texture already
	//		exists and OVERRIDE=FALSE, then skip the texture.
	//
	//!!MAT
	/*
	else if( ParseCommand(&Str,TEXT("CLEARDETAIL")) )
	{
		CurrentDetailTexture = 0;
		debugf( NAME_Log, TEXT("Detail texture cleared") );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SETDETAIL")) )
	{
		CurrentDetailTexture = CurrentMaterial;
		debugf( NAME_Log, TEXT("Detail texture set to %s"), CurrentMaterial ? CurrentMaterial->GetFullName() : TEXT("None") );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("APPLYDETAIL")) )
	{
		if( CurrentMaterial != 0 )
		{
			if( CurrentMaterial->DetailTexture == 0 || ParseCommand(&Str,TEXT("OVERRIDE")) )
			{
				CurrentMaterial->DetailTexture = CurrentDetailTexture;
				debugf( NAME_Log, TEXT("Detail texture %s applied to %s"), CurrentMaterial->DetailTexture->GetFullName(), CurrentMaterial->GetFullName() );
			}
			else
			{
				debugf( NAME_Log, TEXT("Detail texture for %s ALREADY set to %s"), CurrentMaterial->GetFullName(), CurrentMaterial->DetailTexture->GetFullName() );
			}
		}
		else
		{
			debugf( NAME_Log, TEXT("No texture selected") );
		}
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("REPLACEDETAIL")) )
	{
		for( TObjectIterator<UTexture> It; It; ++It )
		{
			if( It->DetailTexture == CurrentMaterial )
			{
				It->DetailTexture = CurrentDetailTexture;
				debugf( NAME_Log, TEXT("Detail texture %s replaced with %s on %s"), CurrentMaterial->DetailTexture->GetFullName(), CurrentDetailTexture->GetFullName(), It->GetFullName() );
			}
		}
	}
	else if( ParseCommand(&Str,TEXT("BATCHAPPLY")) )
	{
		UTexture* DetailTexture = 0;
		ParseObject<UTexture>(Str,TEXT("DETAIL="),DetailTexture,ANY_PACKAGE);
		debugf( NAME_Log, TEXT("Detail=%s"), DetailTexture ? DetailTexture->GetFullName() : TEXT("<None>") );

		FString TexturePrefix;
		UBOOL bNoPrefix = !Parse( Str, TEXT("PREFIX="), TexturePrefix );
		debugf( NAME_Log, TEXT("Prefix=%s"), bNoPrefix ? TEXT("<None>") : TexturePrefix );

		UBOOL bOverride = 0;
		Parse( Str,TEXT("OVERRIDE="), bOverride );
		debugf( NAME_Log, TEXT("bOverride=%d"), bOverride );

		for( TObjectIterator<UTexture> It; It; ++It )
		{
			if( bNoPrefix || appStrstr( It->GetName(), *TexturePrefix ) == It->GetName() )
			{
				if( bOverride || It->DetailTexture == 0 )
				{
					It->DetailTexture = DetailTexture;
					debugf( NAME_Log, TEXT("Detail texture %s applied to %s"), It->DetailTexture->GetFullName(), It->GetFullName() );
				}
				else
				{
					debugf( NAME_Log, TEXT( "Detail texture for %s ALREADY set to %s"), It->GetFullName(), It->DetailTexture->GetFullName() );
				}
			}
		}
		return 1;
	}*/
	else if( ParseCommand(&Str,TEXT("NEW")) )
	{
		FName GroupName=NAME_None;
		FName PackageName;
		UClass* TextureClass;
		INT USize, VSize;
		if
		(	Parse( Str, TEXT("NAME="),    TempName, NAME_SIZE )
		&&	ParseObject<UClass>( Str, TEXT("CLASS="), TextureClass, ANY_PACKAGE )
		&&	Parse( Str, TEXT("USIZE="),   USize )
		&&	Parse( Str, TEXT("VSIZE="),   VSize )
		&&	Parse( Str, TEXT("PACKAGE="), PackageName )
		&&	TextureClass->IsChildOf( UMaterial::StaticClass() ) 
		&&	PackageName!=NAME_None )
		{
			UPackage* Pkg = CreatePackage(NULL,*PackageName);
			Pkg->bDirty = 1;
			if( Parse( Str, TEXT("GROUP="), GroupName ) && GroupName!=NAME_None )
				Pkg = CreatePackage(Pkg,*GroupName);
			if( !StaticFindObject( TextureClass, Pkg, TempName ) )
			{
				// Create new texture object.
				UMaterial* Material = Cast<UMaterial>(StaticConstructObject( TextureClass, Pkg, TempName, RF_Public|RF_Standalone ));
				UTexture* Texture = Cast<UTexture>(Material);

				if( Texture )
				{
					if( !Texture->Palette )
					{
						Texture->Palette = new( Texture->GetOuter(), NAME_None, RF_Public )UPalette;
						Texture->Palette->Colors.Add( 256 );
					}
					Texture->Init( USize, VSize );
				}
				Material->PostLoad();
				if( Texture )
					Texture->Clear( TCLEAR_Temporal | TCLEAR_Bitmap );

				CurrentMaterial = Material;
			}
			else Ar.Logf( NAME_ExecWarning, TEXT("Texture exists") );
		}
		else Ar.Logf( NAME_ExecWarning, TEXT("Bad TEXTURE NEW") );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("DUPLICATE")) )	// TEXTURE DUPLICATE PACKAGE=x GROUP=x NAME=x
	{
		if( !CurrentMaterial )
			return 0;

		FString PkgName, Group, Name;
		Parse( Str, TEXT("PACKAGE="), PkgName );
		UPackage* Pkg = CreatePackage(NULL,*PkgName);
		Pkg->bDirty = 1;
		if( Parse(Str,TEXT("GROUP="),Group) && Group!=NAME_None )
			Pkg = CreatePackage( Pkg, *Group );
		if( Parse(Str,TEXT("NAME="),Name) )
		{
			if( !StaticFindObject( UMaterial::StaticClass(), Pkg, *Name ) )
			{	
				GUglyHackFlags		|= 8;
				INT SavedLazyLoad	= GLazyLoad;
				GLazyLoad			= 0;
				UMaterial* Material = CastChecked<UMaterial>(UMaterial::StaticConstructObject( CurrentMaterial->GetClass(), Pkg, *Name, RF_Public|RF_Standalone, CurrentMaterial, GError ));

				TArray<BYTE> Buffer;

				FMemoryWriter Writer( Buffer );
				FMemoryReader Reader( Buffer );

				CurrentMaterial->Serialize(Writer);
				Material->Serialize(Reader);

				UTexture* Texture			= Cast<UTexture>(Material);
				UTexture* CurrentTexture	= Cast<UTexture>(CurrentMaterial);
				if( Texture )
				{
					// Don't share palettes.
					if( CurrentTexture->Palette )
					{
						Texture->Palette = new( Texture->GetOuter(), NAME_None, RF_Public )UPalette;
						Texture->Palette->Colors.Add( 256 );
						for( INT i=0; i<CurrentTexture->Palette->Colors.Num(); i++ )
							Texture->Palette->Colors(i) = CurrentTexture->Palette->Colors(i);
					}
				}

				Material->PostLoad();
				Material->PostEditChange();

				GUglyHackFlags		&= ~8;	
				GLazyLoad			= SavedLazyLoad;
				CurrentMaterial		= Material;
			}
			else Ar.Logf( NAME_ExecWarning, TEXT("Texture exists") );
		}
		
		EdCallback( EDC_RedrawAllViewports, 0, 0 );
		EdCallback( EDC_RefreshEditor, 1, ERefreshEditor_Misc | ERefreshEditor_AllBrowsers );

		return 1;
	}

	return 0;

	unguard;
}


UBOOL UEditorEngine::Exec_Transaction( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_Transaction);

	if( ParseCommand(&Str,TEXT("UNDO")) )
	{
		if( Trans->Undo() )
		{
			Level->Model->ClearRenderData(GRenDev);
			RedrawLevel( Level );
		}
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("REDO")) )
	{
		if( Trans->Redo() )
		{
			Level->Model->ClearRenderData(GRenDev);
			RedrawLevel(Level);
		}
		return 1;
	}

	return 0;

	unguard;
}

UBOOL UEditorEngine::Exec_Obj( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_Obj);

	if( ParseCommand(&Str,TEXT("EXPORT")) )//oldver
	{
		FName Package=NAME_None;
		UClass* Type;
		UObject* Res;
		Parse( Str, TEXT("PACKAGE="), Package );
		if
		(	ParseObject<UClass>( Str, TEXT("TYPE="), Type, ANY_PACKAGE )
		&&	Parse( Str, TEXT("FILE="), TempFname, 256 )
		&&	ParseObject( Str, TEXT("NAME="), Type, Res, ANY_PACKAGE ) )
		{
			for( FObjectIterator It; It; ++It )
				It->ClearFlags( RF_TagImp | RF_TagExp );
			UExporter* Exporter = UExporter::FindExporter( Res, appFExt(TempFname) );
			if( Exporter )
			{
				Exporter->ParseParms( Str );
				UExporter::ExportToFile( Res, Exporter, TempFname );
				delete Exporter;
			}
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing file, name, or type") );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SavePackage")) )
	{
		UPackage* Pkg;
		if
		(	Parse( Str, TEXT("FILE="), TempFname, 256 ) 
		&&	ParseObject<UPackage>( Str, TEXT("Package="), Pkg, NULL ) )
		{
			GWarn->BeginSlowTask( TEXT("Saving package"), 1);

			UBOOL bWarnOverwrite = true, bWarnParam = true;			
			if( Parse( Str, TEXT("WARN="), bWarnParam ) )
				bWarnOverwrite = bWarnParam;

			if( LoadEntirePackageWhenSaving )
			{
				GWarn->StatusUpdatef( 1, 1, TEXT("Loading entire package...") );
				GEditor->LoadPackage( NULL, TempFname, LOAD_NoWarn );
			}

			GWarn->StatusUpdatef( 1, 1, TEXT("Saving package...") );

			INT DiskSize = GFileManager->FileSize( TempFname );
			FArchiveCountMem Count( Pkg );

			// Save the package out to a temporary name to get the filesize.
			FString TmpFilename = TempFname;
			TmpFilename += TEXT(".tmp");
			SavePackage( Pkg, NULL, RF_Standalone, *TmpFilename, GWarn );
			INT NewSize = GFileManager->FileSize( *TmpFilename );

			// If the file existing on the disk is larger than the new one we're going to overwrite it with, warn the user.
			if( bWarnOverwrite && DiskSize > -1 && DiskSize - NewSize > 0 )	// Use a buffer of 100 bytes to offset stupid differences that sometimes come up
			{
				if( !appMsgf( 1, TEXT("The file on the disk (%s) is larger than the file in memory (%s).  Are you sure you want to overwrite it?"),
					*FString::FormatAsNumber(DiskSize), *FString::FormatAsNumber( NewSize ) ) )
				{
					GFileManager->Delete(*TmpFilename);

					GWarn->EndSlowTask();
					return 0;
				}
			}

			if( !GFileManager->Move(TempFname,*TmpFilename) )
				appMsgf( 0, TEXT("Couldn't save package - maybe file is read-only?") );

			Pkg->bDirty = 0;
			GWarn->EndSlowTask();
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("Rename")) )
	{
		UObject* Object=NULL;
		UObject* OldPackage=NULL, *OldGroup=NULL;
		FString NewName, NewGroup, NewPackage;
		ParseObject<UObject>( Str, TEXT("OLDPACKAGE="), OldPackage, NULL );
		ParseObject<UObject>( Str, TEXT("OLDGROUP="), OldGroup, OldPackage );
		Cast<UPackage>(OldPackage)->bDirty = 1;
		if( OldGroup )
			OldPackage = OldGroup;
		ParseObject<UObject>( Str, TEXT("OLDNAME="), Object, OldPackage );
		Parse( Str, TEXT("NEWPACKAGE="), NewPackage );
		UPackage* Pkg = CreatePackage(NULL,*NewPackage);
		Pkg->bDirty = 1;
		if( Parse(Str,TEXT("NEWGROUP="),NewGroup) && NewGroup!=NAME_None )
			Pkg = CreatePackage( Pkg, *NewGroup );
		Parse( Str, TEXT("NEWNAME="), NewName );
		if( Object )
		{
			Object->Rename( *NewName, Pkg );
			Object->SetFlags(RF_Public|RF_Standalone);
		}
		return 1;
	}

	return 0;

	unguard;
}

UBOOL UEditorEngine::Exec_Class( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_Class);

	if( ParseCommand(&Str,TEXT("SPEW")) )
	{
		GWarn->BeginSlowTask( TEXT("Exporting scripts"), 0);

		UBOOL All = ParseCommand(&Str,TEXT("ALL"));
		UObject* Package=NULL;
		if( ParseObject( Str, TEXT("PACKAGE="), Package, ANY_PACKAGE ) )
			All = 1;
		for( TObjectIterator<UClass> It; It; ++It )
		{
			if( It->ScriptText && (All || (It->GetFlags() & RF_SourceModified)) )
			{
				// Check package
				if( Package )
				{
					UObject* Outer = It->GetOuter();
					while( Outer && Outer->GetOuter() )
						Outer = Outer->GetOuter();
					if( Outer != Package )
						continue;
				}
				// Make package directory.
				appStrcpy( TempFname, TEXT("..") PATH_SEPARATOR );
				appStrcat( TempFname, It->GetOuter()->GetName() );
				GFileManager->MakeDirectory( TempFname, 0 );

				// Make package\Classes directory.
				appStrcat( TempFname, PATH_SEPARATOR TEXT("Classes") );
				GFileManager->MakeDirectory( TempFname, 0 );

				// Save file.
				appStrcat( TempFname, PATH_SEPARATOR );
				appStrcat( TempFname, It->GetName() );
				appStrcat( TempFname, TEXT(".uc") );
				debugf( NAME_Log, TEXT("Spewing: %s"), TempFname );
				UExporter::ExportToFile( *It, NULL, TempFname );
			}
		}

		GWarn->EndSlowTask();
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("LOAD")) ) // CLASS LOAD FILE=..
	{
		if( Parse( Str, TEXT("FILE="), TempFname, 256 ) )
		{
			Ar.Logf( TEXT("Loading class from %s..."), TempFname );
			if( appStrfind(TempFname,TEXT("UC")) )
			{
				FName PkgName, ObjName;
				if
				(	Parse(Str,TEXT("PACKAGE="),PkgName)
				&&	Parse(Str,TEXT("NAME="),ObjName) )
				{
					// Import it.
					ImportObject<UClass>( Level, CreatePackage(NULL,*PkgName), ObjName, RF_Public|RF_Standalone, TempFname );
				}
				else Ar.Log(TEXT("Missing package name"));
			}
			else if( appStrfind( TempFname, TEXT("U")) )
			{
				// Load from Unrealfile.
				UPackage* Pkg = Cast<UPackage>(LoadPackage( NULL, TempFname, LOAD_Forgiving ));
				if( Pkg && (Pkg->PackageFlags & PKG_BrokenLinks) )
				{
					debugf( TEXT("Some classes were broken; a recompile is required") );
					for( TObjectIterator<UClass> It; It; ++It )
					{
						if( It->IsIn(Pkg) )
						{
							It->Dependencies.Empty();
							It->Script.Empty();
						}
					}
				}
			}
			else Ar.Log( NAME_ExecWarning, TEXT("Unrecognized file type") );
		}
		else Ar.Log(NAME_ExecWarning,TEXT("Missing filename"));
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("NEW")) ) // CLASS NEW
	{
		UClass *Parent;
		FName PackageName;
		if
		(	ParseObject<UClass>( Str, TEXT("PARENT="), Parent, ANY_PACKAGE )
			&&	Parse( Str, TEXT("PACKAGE="), PackageName )
			&&	Parse( Str, TEXT("NAME="), TempStr, NAME_SIZE ) )
		{
			UPackage* Pkg = CreatePackage(NULL,*PackageName); 
			Pkg->bDirty = 1;
			UClass* Class = new( Pkg, TempStr, RF_Public|RF_Standalone )UClass( Parent );
			if( Class )
				Class->ScriptText = new( Class->GetOuter(), TempStr, RF_NotForClient|RF_NotForServer )UTextBuffer;
			else
				Ar.Log( NAME_ExecWarning, TEXT("Class not found") );
		}
		return 1;
	}

	return 0;

	unguard;
}

UBOOL UEditorEngine::Exec_Camera( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_Camera);

	UBOOL DoUpdate = ParseCommand(&Str,TEXT("UPDATE"));
	UBOOL DoOpen   = ParseCommand(&Str,TEXT("OPEN"));
	if( (DoUpdate || DoOpen) && Level )
	{
		UViewport* Viewport;
		UBOOL Temp=0;
		TCHAR TempStr[NAME_SIZE];
		if( Parse( Str, TEXT("NAME="), TempStr, NAME_SIZE ) )
		{
			Viewport = FindObject<UViewport>( Client, TempStr );
			if( !Viewport )
			{
				Viewport = Client->NewViewport( TempStr );
				Level->SpawnViewActor( Viewport );
				Viewport->Input->Init( Viewport );
				DoOpen = 1;
			}
			else Temp=1;
		}
		else
		{
			Viewport = Client->NewViewport( NAME_None );
			Level->SpawnViewActor( Viewport );
			Viewport->Input->Init( Viewport );
			DoOpen = 1;
		}
		check(Viewport->Actor);

		DWORD hWndParent=0;
		Parse( Str, TEXT("HWND="), hWndParent );

		INT NewX=Viewport->SizeX, NewY=Viewport->SizeY;
		Parse( Str, TEXT("XR="), NewX ); if( NewX<0 ) NewX=0;
		Parse( Str, TEXT("YR="), NewY ); if( NewY<0 ) NewY=0;
		Viewport->Actor->FovAngle = FovAngle;

		Viewport->Actor->Misc1=0;
		Viewport->Actor->Misc2=0;
		Viewport->MiscRes=NULL;
		Parse(Str,TEXT("FLAGS="),Viewport->Actor->ShowFlags);
		Parse(Str,TEXT("REN="),  Viewport->Actor->RendMap);
		Parse(Str,TEXT("MISC1="),Viewport->Actor->Misc1);
		Parse(Str,TEXT("MISC2="),Viewport->Actor->Misc2);
		FName GroupName=NAME_None;
		if( Parse(Str,TEXT("GROUP="),GroupName) )
			Viewport->Group = GroupName;
		if( appStricmp(*Viewport->Group,TEXT("(All)"))==0 )
			Viewport->Group = NAME_None;

		switch( Viewport->Actor->RendMap )
		{
			case REN_TexView:
			{
				//!!MAT - this correct?
				ParseObject<UMaterial>(Str,TEXT("TEXTURE="),*(UMaterial **)&Viewport->MiscRes,ANY_PACKAGE); 
				if( !Viewport->MiscRes )
					Viewport->MiscRes = Viewport->Actor->Level->DefaultTexture;
			}
			break;

			case REN_MeshView:
			case REN_Animation: //#SKEL
#ifdef WITH_LIPSINC
			case REN_LIPSinc:
#endif
			{
				if( !Temp )
				{
					Viewport->Actor->Location = FVector(100.0f,100.0f,+60.0f);
					Viewport->Actor->Rotation.Yaw=0x6000;
				}
				ParseObject<UMesh>( Str, TEXT("MESH="), *(UMesh**)&Viewport->MiscRes, ANY_PACKAGE ); 
			}
			break;

			case REN_TexBrowser:
			{
				ParseObject<UPackage>(Str,TEXT("PACKAGE="),*(UPackage**)&Viewport->MiscRes,NULL);
			}
			break;

			case REN_StaticMeshBrowser:
			{
				ParseObject<UPackage>(Str,TEXT("PACKAGE="),*(UPackage**)&Viewport->MiscRes,NULL);
			}
			break;
		}
		if( DoOpen )
		{
			INT OpenX = INDEX_NONE;
			INT OpenY = INDEX_NONE;
			Parse( Str, TEXT("X="), OpenX );
			Parse( Str, TEXT("Y="), OpenY );
			Viewport->OpenWindow( hWndParent, 0, NewX, NewY, OpenX, OpenY );
			if( appStricmp(Viewport->GetName(),TEXT("U2Viewport0"))==0 )
				ResetSound();
		}
		else Draw( Viewport, 1 );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("HIDESTANDARD")) )
	{
		Client->ShowViewportWindows( SHOW_StandardView, 0 );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("CLOSE")) )
	{
		UViewport* Viewport;
		if( ParseCommand(&Str,TEXT("ALL")) )
		{
			for( INT i=Client->Viewports.Num()-1; i>=0; i-- )
				delete Client->Viewports(i);
		}
		else if( ParseCommand(&Str,TEXT("FREE")) )
		{
			for( INT i=Client->Viewports.Num()-1; i>=0; i-- )
				if( appStrstr( Client->Viewports(i)->GetName(), TEXT("STANDARD") )==0 )
					delete Client->Viewports(i);
		}
		else if( ParseObject<UViewport>(Str,TEXT("NAME="),Viewport,GetTransientPackage()) )
		{
			delete Viewport;
		}
		else Ar.Log( TEXT("Missing name") );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("ALIGN") ) )
	{
		// select the named actor
		if( Parse( Str, TEXT("NAME="), TempStr, NAME_SIZE ) )
		{
			AActor* Actor = NULL;
			for( INT i=0; i<Level->Actors.Num(); i++ )
			{
				Actor = Level->Actors(i);
				if( Actor && appStricmp( Actor->GetName(), TempStr ) == 0 )
				{
					SelectActor( Level, Actor, 1, 0 );
					break;
				}
			}
		}

		FVector NewLocation;
		if( Parse( Str, TEXT("X="), NewLocation.X ) )
		{
			Parse( Str, TEXT("Y="), NewLocation.Y );
			Parse( Str, TEXT("Z="), NewLocation.Z );

			for( INT i = 0; i < Client->Viewports.Num(); i++ )
			{
				AActor* Camera = Client->Viewports(i)->Actor;
				Camera->Location = NewLocation;
			}
		}
		else
		{
			// find the first selected actor as the target for the viewport cameras
			AActor* Target = NULL;
			for( INT i = 0; i < Level->Actors.Num(); i++ )
			{
				if( Level->Actors(i) && Level->Actors(i)->bSelected )
				{
					Target = Level->Actors(i);
					break;
				}
			}
			// if no actor was selected, find the camera for the current viewport
			if( Target == NULL )
			{
				for( INT i = 0; i < Client->Viewports.Num(); i++ )
				{
					if( Client->Viewports(i)->Current )
					{
						Target = Client->Viewports(i)->Actor;
						break;
					}
				}
			}
			if( Target == NULL )
			{
				Ar.Log( TEXT("Can't find target (viewport or selected actor)") );
				return 0;
			}

			// move all viewport cameras to the target actor, offset if the target isn't a camera (PlayerPawn)
			for( INT i = 0 ; i < Client->Viewports.Num() ; i++ )
			{
				AActor* Camera = Client->Viewports(i)->Actor;

				if( Client->Viewports(i)->Actor->RendMap != REN_StaticMeshBrowser )
				{
					if( Target->IsA( APawn::StaticClass() ) )
						Camera->Location = Target->Location;
					else
						Camera->Location = Target->Location - Camera->Rotation.Vector() * 48;
					Camera->Rotation = FRotator( Target->Rotation.Pitch, Target->Rotation.Yaw, 0 );	// we don't want the roll
				}
			}
		}
		Ar.Log( TEXT("Aligned camera on the current target.") );
		NoteSelectionChange( Level );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SELECT") ) )
	{
		if( Parse( Str, TEXT("NAME="), TempStr,NAME_SIZE ) )
		{
			AActor* Actor = NULL;
			for( INT i=0; i<Level->Actors.Num(); i++ )
			{
				Actor = Level->Actors(i);
				if( Actor && appStrcmp( Actor->GetName(), TempStr ) == 0 )
				{
					SelectActor( Level, Actor, 1, 0 );
					break;
				}
			}
			if( Actor == NULL )
			{
				Ar.Log( TEXT("Can't find the specified name.") );
				return 0;
			}

			for( INT i = 0; i < Client->Viewports.Num(); i++ )
			{
				AActor* Camera = Client->Viewports(i)->Actor;
				Camera->Location = Actor->Location - Camera->Rotation.Vector() * 48;
			}
			Ar.Log( TEXT("Aligned camera on named object.") );
			NoteSelectionChange( Level );
			return 1;
		}
	}

	return 0;

	unguard;
}

UBOOL UEditorEngine::Exec_Level( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_Level);

	if( ParseCommand(&Str,TEXT("REDRAW")) )
	{
		RedrawLevel(Level);
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("LINKS")) )
	{
		Results->Text.Empty();
		INT Internal=0,External=0;
		Results->Logf( TEXT("Level links:\r\n") );
		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			if( Cast<ATeleporter>(Level->Actors(i)) )
			{
				ATeleporter& Teleporter = *(ATeleporter *)Level->Actors(i);
				Results->Logf( TEXT("   %s\r\n"), *Teleporter.URL );
				if( appStrchr(*Teleporter.URL,'//') )
					External++;
				else
					Internal++;
			}
		}
		Results->Logf( TEXT("End, %i internal link(s), %i external.\r\n"), Internal, External );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("VALIDATE")) )
	{
		// Validate the level.
		Results->Text.Empty();
		Results->Log( TEXT("Level validation:\r\n") );

		// Make sure it's not empty.
		if( Level->Model->Nodes.Num() == 0 )
		{
			Results->Log( TEXT("Error: Level is empty!\r\n") );
			return 1;
		}

		// Find playerstart.
		INT i;
		for( i=0; i<Level->Actors.Num(); i++ )
			if( Cast<APlayerStart>(Level->Actors(i)) )
				break;
		if( i == Level->Actors.Num() )
		{
			Results->Log( TEXT("Error: Missing PlayerStart actor!\r\n") );
			return 1;
		}

		// Make sure PlayerStarts are outside.
		for( i=0; i<Level->Actors.Num(); i++ )
		{
			if( Cast<APlayerStart>(Level->Actors(i)) )
			{
				FCheckResult Hit(0.0f);
				if( !Level->Model->PointCheck( Hit, NULL, Level->Actors(i)->Location, FVector(0.f,0.f,0.f), 0 ) )
				{
					Results->Log( TEXT("Error: PlayerStart doesn't fit!\r\n") );
					return 1;
				}
			}
		}

		// Check scripts.
		if( GEditor && !GEditor->CheckScripts( GWarn, UObject::StaticClass(), *Results ) )
		{
			Results->Logf( TEXT("\r\nError: Scripts need to be rebuilt!\r\n") );
			return 1;
		}

		// Check level title.
		if( Level->GetLevelInfo()->Title==TEXT("") )
		{
			Results->Logf( TEXT("Error: Level is missing a title!") );
			return 1;
		}
		else if( Level->GetLevelInfo()->Title==TEXT("Untitled") )
		{
			Results->Logf( TEXT("Warning: Level is untitled\r\n") );
		}

		// Check actors.
		for( i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor = Level->Actors(i);
			if( Actor )
			{
				guard(CheckingActors);
				check(Actor->GetClass()!=NULL);
				check(Actor->GetStateFrame());
				check(Actor->GetStateFrame()->Object==Actor);
				check(Actor->Level!=NULL);
				check(Actor->GetLevel()!=NULL);
				check(Actor->GetLevel()==Level);
				check(Actor->GetLevel()->Actors(0)!=NULL);
				check(Actor->GetLevel()->Actors(0)==Actor->Level);
				unguardf(( TEXT("(%i %s)"), i, Actor->GetFullName() ));
			}
		}

		// Success.
		Results->Logf( TEXT("Success: Level validation succeeded!\r\n") );
		return 1;
	}

	return 0;

	unguard;
}

UBOOL UEditorEngine::Exec_Terrain( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_Terrain);

	if( ParseCommand(&Str,TEXT("SOFTSELECT") ) )
	{
		if( GEditor->Mode == EM_TerrainEdit && GTerrainTools.GetCurrentTerrainInfo() )
			GTerrainTools.GetCurrentTerrainInfo()->SoftSelect( GTerrainTools.GetInnerRadius(), GTerrainTools.GetOuterRadius() );
	}
	else if( ParseCommand(&Str,TEXT("SOFTDESELECT") ) )
	{
		for( INT i=0;i<Level->Actors.Num();i++ )
		{
			AActor* A = Level->Actors(i);
			if( A && A->IsA(ATerrainInfo::StaticClass()) )
				Cast<ATerrainInfo>(A)->SoftDeselect();
		}
	}
	else if( ParseCommand(&Str,TEXT("DESELECT") ) )
	{
		for( INT i=0;i<Level->Actors.Num();i++ )
		{
			AActor* A = Level->Actors(i);
			if( A && A->IsA(ATerrainInfo::StaticClass()) )
				Cast<ATerrainInfo>(A)->SelectedVertices.Empty();
		}
	}
	else if( ParseCommand(&Str,TEXT("RESETMOVE") ) )
	{
		if( GEditor->Mode == EM_TerrainEdit && GTerrainTools.GetCurrentTerrainInfo() )
			GTerrainTools.GetCurrentTerrainInfo()->ResetMove();
	}
	else if( ParseCommand(&Str,TEXT("SHOWGRID") ) )
	{
		if( GEditor->Mode == EM_TerrainEdit && GTerrainTools.GetCurrentTerrainInfo() )
		{		
			INT LayerMask;
			if( Parse( Str, TEXT("MASK="), LayerMask) )
			{
				GTerrainTools.GetCurrentTerrainInfo()->ShowGrid = LayerMask;
			}
			else
			{
				LayerMask = 0xFF;
				INT Layer;
				if( Parse( Str, TEXT("LAYER="), Layer ) )
					LayerMask = 1<<Layer;

				GTerrainTools.GetCurrentTerrainInfo()->ShowGrid ^= LayerMask;
			}
		}
	}
	else if( ParseCommand(&Str,TEXT("TOOLRADIUS") ) )	// TERRAIN TOOLRADIUS INNER=# OUTER=#
	{
		INT IRadius, ORadius;
		if( Parse( Str, TEXT("INNER="), IRadius ) )
			GTerrainTools.SetInnerRadius( IRadius );
		if( Parse( Str, TEXT("OUTER="), ORadius ) )
			GTerrainTools.SetOuterRadius( ORadius );

		// Make sure the radii stay valid
		if( GTerrainTools.GetInnerRadius() < 0 ) GTerrainTools.SetInnerRadius( 0 );
		if( GTerrainTools.GetOuterRadius() < 0 ) GTerrainTools.SetOuterRadius( 0 );
		if( GTerrainTools.GetOuterRadius() < GTerrainTools.GetInnerRadius() ) GTerrainTools.SetOuterRadius( GTerrainTools.GetInnerRadius() );
		if( GTerrainTools.GetInnerRadius() > GTerrainTools.GetOuterRadius() ) GTerrainTools.SetInnerRadius( GTerrainTools.GetOuterRadius() );

		EdCallback( EDC_RefreshEditor, 1, ERefreshEditor_Terrain );
	}
	else if( ParseCommand(&Str,TEXT("BRUSH") ) )	// TERRAIN BRUSH <name>
	{
		if( ParseCommand(&Str,TEXT("VERTEXEDIT")) )			TerrainEditBrush = TB_VertexEdit;
		else if( ParseCommand(&Str,TEXT("PAINT")) )			TerrainEditBrush = TB_Paint;
		else if( ParseCommand(&Str,TEXT("SMOOTH")) )		TerrainEditBrush = TB_Smooth;
		else if( ParseCommand(&Str,TEXT("NOISE")) )			TerrainEditBrush = TB_Noise;
		else if( ParseCommand(&Str,TEXT("FLATTEN")) )		TerrainEditBrush = TB_Flatten;
		else if( ParseCommand(&Str,TEXT("TEXPAN")) )		TerrainEditBrush = TB_TexturePan;
		else if( ParseCommand(&Str,TEXT("TEXROTATE")) )		TerrainEditBrush = TB_TextureRotate;
		else if( ParseCommand(&Str,TEXT("TEXSCALE")) )		TerrainEditBrush = TB_TextureScale;
		else if( ParseCommand(&Str,TEXT("SELECT")) )		TerrainEditBrush = TB_Select;
		else if( ParseCommand(&Str,TEXT("VISIBILITY")) )	TerrainEditBrush = TB_Visibility;
		else if( ParseCommand(&Str,TEXT("EDGETURN")) )		TerrainEditBrush = TB_EdgeTurn;
		else if( ParseCommand(&Str,TEXT("COLOR")) )			TerrainEditBrush = TB_Color;

		GTerrainTools.SetCurrentBrush( TerrainEditBrush );

		RedrawLevel( Level );
		//EdCallback( EDC_RedrawAllViewports, 0, 0 );
	}
    // amb ---
	else if( ParseCommand(&Str,TEXT("ADDCONST") ) )	// TERRAIN ADDCONST=<_WORD>
    {
        INT addAmount = 0;
        ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
        if (Parse(Str, TEXT("="), addAmount) && TI && TI->TerrainMap)
        {
            debugf(TEXT("Add constant %d to TerrainMap: %s"), addAmount, TI->TerrainMap->GetFullName());
            TI->TerrainMap->Mips(0).DataArray.Load();
            
            for (int x=0; x<TI->TerrainMap->USize; x++)
            {
                for (int y=0; y<TI->TerrainMap->VSize; y++)
                {
                    _WORD oldH = TI->GetHeightmap(x, y);
                    _WORD newH = Clamp(oldH + addAmount, 0, 65535);
                    TI->SetHeightmap(x, y, newH);
                }
            }

	        TI->Update(0.f);
            TI->TerrainMap->bRealtimeChanged = 1;
            RedrawLevel( Level );
        }
    }
    // --- amb

	return 1;

	unguard;
}

UBOOL UEditorEngine::Exec_Audio( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_Audio);

	if( ParseCommand(&Str,TEXT("PLAY")) )
	{
		Exec( TEXT("AUDIO FINDVIEWPORT") );

		if( Audio )
		{
			UViewport* Viewport = Audio->GetViewport();
			if( Viewport )
			{
				USound* Sound;
				if( ParseObject<USound>( Str, TEXT("NAME="), Sound, ANY_PACKAGE ) )
				{
					if ( Sound )
					{
						UBOOL bLooping = ParseCommand(&Str,TEXT("LOOPING"));

						Audio->StopAllSounds();
						Audio->PlaySound( Viewport->Actor, 2*SLOT_Misc, Sound, Viewport->Actor->Location, 1.0, 4096.0, 1.0, (bLooping?SF_Looping:0) | SF_No3D, 0.f );
					}
					else
						Audio->StopAllSounds();
				}
			}
		}
		else Ar.Logf( TEXT("Can't find viewport for sound") );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("FINDVIEWPORT")) )
	{
		UViewport* Viewport = NULL;
		for( INT vp = 0 ; vp < dED_MAX_VIEWPORTS && !Viewport ; vp++ )
		{
			Viewport = FindObject<UViewport>( ANY_PACKAGE, *FString::Printf(TEXT("U2Viewport%d"), vp) );
			if( Viewport && !Viewport->IsPerspective() )
				Viewport = NULL;
		}

		if( Audio && Audio->GetViewport() != Viewport )
		{
			GWarn->BeginSlowTask( TEXT("Setting up OpenAL Audio viewport"), 1);
			Audio->SetViewport( Viewport );
			GWarn->EndSlowTask();
		}
	}

	return 0;

	unguard;
}

UBOOL UEditorEngine::Exec_BrushClip( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_BrushClip);

	// Locates the first 2 ClipMarkers in the world and flips their locations, which
	// effectively flips the normal of the clipping plane.
	if( ParseCommand(&Str,TEXT("FLIP")) )			// BRUSHCLIP FLIP
	{
		AActor *pActor1, *pActor2;
		pActor1 = pActor2 = NULL;
		for( INT i = 0 ; i < Level->Actors.Num() ; i++ )
		{
			AActor* pActor = Level->Actors(i);
			if( pActor && pActor->IsA(AClipMarker::StaticClass()) )
			{
				if( !pActor1 )
					pActor1 = pActor;
				else
					if( !pActor2 )
						pActor2 = pActor;

				// Once we have 2 valid actors, break out...
				if( pActor2 ) break;
			}
		}

		if( pActor1 && pActor2 )
			Exchange( pActor1->Location, pActor2->Location );

		RedrawLevel( Level );
	}
	// Locate any existing clipping markers and delete them.
	else if( ParseCommand(&Str,TEXT("DELETE")) )	// BRUSHCLIP DELETE
	{
		brushclipDeleteMarkers();
	}
	// Execute the clip based on the current marker positions.
	else
	{
		// Get the current viewport.
		UViewport* CurrentViewport = GetCurrentViewport();
		if( !CurrentViewport )
		{
			debugf(TEXT("BRUSHCLIP : No current viewport - make sure a viewport has the focus before trying this operation."));
			return 1;
		}

		// Gather a list of all the ClipMarkers in the level.
		TArray<AActor*> ClipMarkers;

		for( INT actor = 0 ; actor < Level->Actors.Num() ; actor++ )
		{
			AActor* pActor = Level->Actors(actor);
			if( pActor && pActor->IsA(AClipMarker::StaticClass()) )
				ClipMarkers.AddItem( pActor );
		}

		if( (CurrentViewport->IsOrtho() && ClipMarkers.Num() < 2)
			|| (!CurrentViewport->IsOrtho() && ClipMarkers.Num() < 3))
		{
			debugf(TEXT("BRUSHCLIP : You don't have enough ClipMarkers to perform this operation."));
			return 1;
		}

		// Create a clipping plane based on ClipMarkers present in the level.
		FVector vtx1, vtx2, vtx3;
		FPoly ClippingPlanePoly;

		vtx1 = ClipMarkers(0)->Location;
		vtx2 = ClipMarkers(1)->Location;

		if( ClipMarkers.Num() == 3 )
		{
			// If we have 3 points, just grab the third one to complete the plane.
			vtx3 = ClipMarkers(2)->Location;
		}
		else
		{
			// If we only have 2 points, we will assume the third based on the viewport.
			// (With just 2 points, we can only use ortho viewports)
			vtx3 = vtx1;
			if( CurrentViewport->IsOrtho() )
				switch( CurrentViewport->Actor->RendMap )
				{
					case REN_OrthXY:	vtx3.Z -= 64;	break;
					case REN_OrthXZ:	vtx3.Y -= 64;	break;
					case REN_OrthYZ:	vtx3.X -= 64;	break;
				}
		}

		UBOOL bSplit = ParseCommand(&Str,TEXT("SPLIT"));	// BRUSHCLIP [SPLIT]

		// If we've gotten this far, we're good to go.  Do the clip.
		Trans->Begin( TEXT("Brush Clip") );

		Level->Modify();

		for( INT actor = 0; actor < Level->Actors.Num() ; actor++ )
		{
			AActor* SrcActor = Level->Actors(actor);
			if( SrcActor && SrcActor->bSelected && SrcActor->IsBrush() )
			{
				ABrush* SrcBrush = (ABrush*)SrcActor;
				UBOOL bBuilderBrush = (SrcBrush == Level->Brush());

				FCoords BrushW(SrcBrush->ToWorld()),
					BrushL(SrcBrush->ToLocal());

				// Create a clipping plane for this brushes coordinate system.
				ClippingPlanePoly.NumVertices = 3;
				ClippingPlanePoly.Vertex[0] = vtx1.TransformVectorBy( BrushL );
				ClippingPlanePoly.Vertex[1] = vtx2.TransformVectorBy( BrushL );
				ClippingPlanePoly.Vertex[2] = vtx3.TransformVectorBy( BrushL );

				if( ClippingPlanePoly.CalcNormal(1) )
				{
					debugf(TEXT("BRUSHCLIP : Unable to compute normal!  Try moving the clip markers further apart."));
					return 1;
				}

				ClippingPlanePoly.Base = ClippingPlanePoly.Vertex[0];
				ClippingPlanePoly.Base -= ( SrcBrush->Location.TransformVectorBy( BrushL ) - SrcBrush->PrePivot.TransformVectorBy( BrushL ) );
				FPlane ClippingPlane( ClippingPlanePoly.Base, ClippingPlanePoly.Normal );

				ClipBrushAgainstPlane( ClippingPlane, SrcBrush, 1 );

				// If we're doing a split instead of just a plain clip.
				// NOTE : You can't do split operations against the builder brush.
				if( bSplit && !bBuilderBrush )
				{
					// Flip the clipping plane first.
					ClippingPlane = ClippingPlane.Flip();

					ClipBrushAgainstPlane( ClippingPlane, SrcBrush, 0 );
				}

				// Clean up
				if( !bBuilderBrush )	// Don't destroy the builder brush!
					Level->DestroyActor( SrcBrush );

				// Option to remove the clip markers after the clip operation is complete.
				if( ParseCommand(&Str,TEXT("DELMARKERS")) )		// BRUSHCLIP [DELMARKERS]
					brushclipDeleteMarkers();
			}
		}

		Trans->End();
	}

	return 1;

	unguard;
}


//
// IMPORT of raw animation data.
//
UBOOL UEditorEngine::Exec_Anim( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_Anim);
	// Hack to comform bone hierarchy of one mesh to another.
	if(ParseCommand(&Str,TEXT("CONFORMMESH")))
	{
		USkeletalMesh *ModMesh, *SrcMesh;
		if( ParseObject<USkeletalMesh>(Str,TEXT("MESH="), ModMesh, ANY_PACKAGE) )
		{
			if( ParseObject<USkeletalMesh>(Str,TEXT("SRC="),SrcMesh, ANY_PACKAGE) )
			{
				if( ModMesh && SrcMesh )
				{
					INT DoReplace = 0;
					Parse( Str,TEXT("REPLACE="), DoReplace );
					debugf(TEXT("Conforming Mesh %s to Mesh %s "),ModMesh->GetName(), SrcMesh->GetName() );
					ModMesh->ConformSkeletonTo( SrcMesh, DoReplace );
				}
			}
			else
			{				
				debugf(TEXT("SRC= mesh not found."));
			}
		}
		else
		{
			debugf(TEXT("MESH= mesh not found."));
		}
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("IMPORT")) )
	{
		static TCHAR TempMeshName[NAME_SIZE];

		if( Parse( Str,TEXT("FILE="),TempFname,256 ) )
		{	
			appStrupr(TempFname); //Convert to upper case.

			// .PSA part of TempFname ?
			if( appStrstr(TempFname,TEXT(".PSK") ) )
			{
				GWarn->BeginSlowTask( TEXT("Importing Skeletal Mesh File"), 1);
				Trans->Begin( TEXT("Anim Import") );

				// #SKEL -> change to use command line's package and group, just like textures.
				FName PkgName; // = ParentContext ? ParentContext->GetFName() : NAME_None;
				PkgName = FName(TEXT("TempAnimPackage"));
				Parse( Str, TEXT("PACKAGE="), PkgName );
				UPackage* Pkg = CreatePackage(NULL,*PkgName);		
				FName GroupName = TEXT("Default"); // NAME_None; 				

				INT DoWeldVerts = 0;
				Parse( Str,TEXT("WELDING="), DoWeldVerts);

				// Does it need to be deleted before we load a new one ?
				USkeletalMesh* NewMesh = NULL;
				
				FName NewMeshName(TEXT("_NewMesh"));
				Parse( Str, TEXT("NAME="), NewMeshName );				
			
				NewMesh = (USkeletalMesh*) UObject::StaticFindObject( USkeletalMesh::StaticClass(), ANY_PACKAGE, *NewMeshName);

				// Existing mesh ? Delete it first!
				if( NewMesh ) 
				{										
					USkeletalMeshInstance* MInst = (USkeletalMeshInstance*) NewMesh->MeshGetInstance(NULL);
					if( MInst)
					{					
						MInst->SetStatus(MINST_DeleteMe);
						delete (MInst);
					}
					delete( NewMesh );
					Pkg->bDirty = 1;
				}
				
				// Actual load as in the #exec methods.
				// MODEL boned mesh object import.
				if( 1 ) //
				{
					UBOOL Unmirror=0, ZeroTex=0;
					FLODProcessInfo LODInfo;
					LODInfo.LevelOfDetail = true;
					LODInfo.ApplySmoothingGroups = false;
					LODInfo.Style = 10; // Good default style.
					LODInfo.SampleFrame = 0;
					LODInfo.NoUVData = false;
					LODInfo.Specify = 0;
					LODInfo.WeldVerts = DoWeldVerts;
					
					appStrcpy(TempMeshName,*NewMeshName);
					
					// meshSkelImport( TempMeshName, ParentContext, TempFname, Unmirror, ZeroTex, LinkMaterials, &LODInfo );
					meshSkelImport( TempMeshName, Pkg, TempFname, Unmirror, ZeroTex, 1, &LODInfo );
				}
				
				// Point NewMesh to the mesh name from the PSK....
				NewMesh = (USkeletalMesh*) UObject::StaticFindObject( USkeletalMesh::StaticClass(), ANY_PACKAGE, *NewMeshName);
				
				// Scale..
				FVector Scale(1.f,1.f,1.f);
				NewMesh->MeshGetInstance(NULL)->SetScale( Scale );

				// Set any rotation passed in with the command line
				GetFROTATOR ( Str, NewMesh->RotOrigin, 256 );
				
								
				// Try to assign textures to the names as extracted from RawData->Materials().				
				if( NewMesh )
				{
					Pkg->bDirty = 1;
					debugf(TEXT("New USkeletalMesh created: Name: [%s] Group: [%s] Package: [%s] "),NewMesh->GetName(), NewMesh->GetOuter()->GetName(), NewMesh->GetOuter()->GetFullName() );					
				}
				else
				{					
					debugf(TEXT("ERROR New mesh not created"));
				}
				
				Trans->End();
				GWarn->EndSlowTask();
			}
			else 
			if( appStrstr(TempFname,TEXT(".PSA") ) )
			{
				GWarn->BeginSlowTask( TEXT("Importing Animation File"), 1);
				Trans->Begin( TEXT("Anim Import") );
				// Create animation object
				debugf(TEXT("Generating ANIMATION object: cmd: [%s]"),Str);

				// #SKEL -> change to use command line's package and group, just like textures.
				FName PkgName; // = ParentContext ? ParentContext->GetFName() : NAME_None;
				PkgName = FName(TEXT("TempAnimPackage"));
				Parse( Str, TEXT("Package="), PkgName );
				UPackage* Pkg = CreatePackage(NULL,*PkgName);		
				FName GroupName = TEXT("Default"); // NAME_None; 

				UMeshAnimation* NewAnim = NULL;

				//#SKEL - cleanup string mess.
				TCHAR TempNewName[64];
				TempNewName[0] = 0;				
				FName NewAnimName(TEXT("_NewAnim"));
				Parse( Str, TEXT("NAME="), NewAnimName );

				// Animation compression factor. Optional on this commandline.
				FLOAT CompDefault = 1.0f;  
				FLOAT CompFactor = 1.0f;
				if( Parse( Str, TEXT("COMP="), CompFactor ) )
					CompDefault = CompFactor;
				
				appStrcpy( TempNewName,*NewAnimName); 
				
				NewAnim = (UMeshAnimation*) UObject::StaticFindObject( UMeshAnimation::StaticClass(), ANY_PACKAGE, *NewAnimName);

				TArray<USkeletalMesh*> DependentMeshes;

				// Existing Anim object ? Delete it first - but take care of meshes that had it as their default animation.
				if( NewAnim ) 
				{									
					// Record any meshes using it.
					for( TObjectIterator<USkeletalMesh> It ; It ; ++It )
					{
						USkeletalMesh* TestMesh = *It;
						if( TestMesh->DefaultAnim == NewAnim )
						{
							DependentMeshes.AddItem(TestMesh); // Mark for restoration
							TestMesh->DefaultAnim = NULL;							
						}
					}

					delete( NewAnim );
					Pkg->bDirty = 1;
				}
				
				UBOOL Unmirror=0;								
				animationImport( TempNewName, Pkg, TempFname, Unmirror, CompDefault );

				// Find it again by name once generated.
				NewAnim = (UMeshAnimation*) UObject::StaticFindObject( UMeshAnimation::StaticClass(), ANY_PACKAGE, *NewAnimName);

				// Link back any meshes that depended on the animation object just replaced.
				if( NewAnim )
				{
					for( INT i=0; i<DependentMeshes.Num(); i++ )
						DependentMeshes(i)->DefaultAnim = NewAnim;
				}
								
				//
				if( NewAnim && NewAnim->DigestHelper )
				{

					// Write debugging info to log if verbose mode requested.
					UBOOL bVerbose = true;												
					if( bVerbose )
					{
						debugf(TEXT("Skeletal animation digest: raw animation key memory: %i Bytes."), NewAnim->DigestHelper->RawAnimKeys.Num() * sizeof(VQuatAnimKey));
					}		

					// Digest and compress the movements.					
					digestMovementRepertoire(NewAnim);

					// Erase the raw data.
					NewAnim->DigestHelper->RawAnimKeys.Empty();
					NewAnim->DigestHelper->MovesInfo.Empty();
					if( bVerbose )
					{
						// Make into popup window..
						debugf(TEXT("Skeletal animation digest: final animation key memory: %i Bytes."),NewAnim->MemFootprint());
					}

					// Link up this animation to the LAST-imported PSK skeletal mesh as its default-animation object - if NOT called from the browser.
					FString TempStr;
					if( !Parse(Str, TEXT("BROWSER"),TempStr) )
					{
						FName NewMeshName = FName( TempMeshName ); 
						USkeletalMesh* NewMesh = (USkeletalMesh*) UObject::StaticFindObject( USkeletalMesh::StaticClass(), ANY_PACKAGE, *NewMeshName);
						if (NewMesh) NewMesh->DefaultAnim = NewAnim;
					}					

					Pkg->bDirty = 1;
				}
				else
				{
					debugf(TEXT(" NewAnim not found after animationImport ! name(%s) file(%s)"),*NewAnimName,Str);
				}

				Trans->End();
				GWarn->EndSlowTask();
			}
			else 
			{
				debugf(TEXT("ERROR - do not know how to digest [%s]"),TempFname ); 
			}
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );

		return 1;
	}

	return 0;
	unguard;
}


UBOOL UEditorEngine::Exec_Fluid( const TCHAR* Str, FOutputDevice& Ar )
{
	guard(UEditorEngine::Exec_Fluid);

	debugf(TEXT("EXEC_FLUID: command line: [%s] "),Str);

	// Rebuild fluid actors.
	// This updates the 'clamped' map for the fluid, so water bounces properly
	// off terrain/meshes.
	if( ParseCommand( &Str, TEXT("REBUILD")) )
	{
		for (INT i=0; i<Level->Actors.Num(); i++)
		{
			if(Level->Actors(i) && Level->Actors(i)->IsA(AFluidSurfaceInfo::StaticClass()))
			{
				AFluidSurfaceInfo *Fluid = Cast<AFluidSurfaceInfo>(Level->Actors(i));
				Fluid->RebuildClampedBitmap(); 
			}
		}
	}

	return 0;
	unguard;
}

//
// Process an incoming network message meant for the editor server
//
UBOOL UEditorEngine::Exec( const TCHAR* Stream, FOutputDevice& Ar )
{
	//debugf("GEditor Exec: %s",Stream);
	TCHAR CommandTemp[MAX_EDCMD];
	TCHAR ErrorTemp[256]=TEXT("Setup: ");
	guard(UEditorEngine::Exec);
	UBOOL Processed=0;

	// Echo the command to the log window
	if( appStrlen(Stream)<200 )
	{
		appStrcat( ErrorTemp, Stream );
		debugf( NAME_Cmd, Stream );
	}

	GStream = Stream;
	GBrush = Level ? Level->Brush()->Brush : NULL;

	appStrncpy( CommandTemp, Stream, 512 );
	const TCHAR* Str = &CommandTemp[0];

	appStrncpy( ErrorTemp, Str, 79 );
	ErrorTemp[79]=0;

	if( SafeExec( Stream, Ar ) )
	{
		return 1;
	}
	//------------------------------------------------------------------------------------
	// MISC
	//
	else if( ParseCommand(&Str,TEXT("EDCALLBACK")) )
	{
		if( ParseCommand(&Str,TEXT("SURFPROPS")) )
			EdCallback( EDC_SurfProps, 0, 0 );
	}
	else if( ParseCommand(&Str,TEXT("POLYGON")) )
	{
		if( ParseCommand(&Str,TEXT("DELETE")) )
		{
			polygonDeleteMarkers();
		}
	}
	else if( ParseCommand(&Str,TEXT("BRUSHCLIP")) )		// BRUSHCLIP
	{
		if( Exec_BrushClip( Str, Ar ) )
			return 1;
	}
	else if(ParseCommand(&Str,TEXT("STATICMESH")))
	{
		// amb, jij ---
        if (Exec_StaticMeshImport(Str, Ar))
            return 1;
		if( Exec_StaticMesh( Str, Ar ))
			return 1;
	}
	//------------------------------------------------------------------------------------
	// BRUSH
	//
	else if( ParseCommand(&Str,TEXT("BRUSH")) )
	{
		if( Exec_Brush( Str, Ar ) )
			return 1;
	}
	//----------------------------------------------------------------------------------
	// PATHS
	//
	else if( ParseCommand(&Str,TEXT("PATHS")) )
	{
		if( Exec_Paths( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// BSP
	//
	else if( ParseCommand( &Str, TEXT("BSP") ) )
	{
		if( Exec_BSP( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// LIGHT
	//
	else if( ParseCommand( &Str, TEXT("LIGHT") ) )
	{
		if( Exec_Light( Str, Ar ) )
			return 1;
	}
#if 1 // toggle showing inventory spots
	//------------------------------------------------------------------------------------
	// DEBUGGING
	//
	else if (ParseCommand(&Str,TEXT("SHOWINV")))
	{
		for (INT i=0; i<Level->Actors.Num(); i++)
		{
			AActor *Actor = Level->Actors(i); 
			if ( Actor && Actor->IsA(AInventorySpot::StaticClass()) )
			{
				Actor->bHiddenEd = !Actor->bHiddenEd;
			}
		}

		RedrawLevel( Level );
	}
#endif
	//------------------------------------------------------------------------------------
	// MAP
	//
	else if (ParseCommand(&Str,TEXT("MAP")))
	{
		if( Exec_Map( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// SELECT: Rerouted to mode-specific command
	//
	else if( ParseCommand(&Str,TEXT("SELECT")) )
	{
		if( Exec_Select( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// DELETE: Rerouted to mode-specific command
	//
	else if (ParseCommand(&Str,TEXT("DELETE")))
	{
		return Exec( TEXT("ACTOR DELETE") );
	}
	//------------------------------------------------------------------------------------
	// DUPLICATE: Rerouted to mode-specific command
	//
	else if (ParseCommand(&Str,TEXT("DUPLICATE")))
	{
		return Exec( TEXT("ACTOR DUPLICATE") );
	}
	//------------------------------------------------------------------------------------
	// POLY: Polygon adjustment and mapping
	//
	else if( ParseCommand(&Str,TEXT("POLY")) )
	{
		if( Exec_Poly( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// TEXTURE management:
	//
	else if( ParseCommand(&Str,TEXT("Texture")) )
	{
		if( Exec_Texture( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// ANIM: All mesh/animation management.
	//
	else if( ParseCommand(&Str,TEXT("NEWANIM")) )
	{
		if( Exec_Anim( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// Transaction tracking and control
	//
	else if( ParseCommand(&Str,TEXT("TRANSACTION")) )
	{
		if( Exec_Transaction( Str, Ar ) )
		{
			NoteSelectionChange( Level );
			EdCallback( EDC_MapChange, 0, 1 );

			return 1;
		}
	}
	//------------------------------------------------------------------------------------
	// General objects
	//
	else if( ParseCommand(&Str,TEXT("OBJ")) )
	{
		if( Exec_Obj( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// CLASS functions
	//
	else if( ParseCommand(&Str,TEXT("CLASS")) )
	{
		if( Exec_Class( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// CAMERA: cameras
	//
	else if( ParseCommand(&Str,TEXT("CAMERA")) )
	{
		if( Exec_Camera( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// LEVEL
	//
	if( ParseCommand(&Str,TEXT("LEVEL")) )
	{
		if( Exec_Level( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// Terrain
	//
	else if( ParseCommand(&Str,TEXT("TERRAIN")) )
	{
		if( Exec_Terrain( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// Fluid Surfaces
	//
	else if( ParseCommand(&Str,TEXT("FLUID")) )
	{
		if( Exec_Fluid( Str, Ar ) )
			return 1;
	}
	//------------------------------------------------------------------------------------
	// Other handlers.
	//
	else if( ParseCommand(&Str,TEXT("FIX")) )
	{
		for( INT i=0; i<Level->Actors.Num(); i++ )
			if( Level->Actors(i) )
				Level->Actors(i)->SoundRadius = Clamp(4*(INT)Level->Actors(i)->SoundRadius,0,255);
	}
	else if( ParseCommand(&Str,TEXT("AUDIO")) )
	{
		if( Exec_Audio( Str, Ar ) )
			return 1;
	}
	else if( ParseCommand(&Str,TEXT("SETCURRENTCLASS")) )
	{
		ParseObject<UClass>( Str, TEXT("CLASS="), CurrentClass, ANY_PACKAGE );
		Ar.Logf( TEXT("CurrentClass=%s"), CurrentClass->GetName() );
		return 1;
	}
	else if( Level && Level->Exec(Stream,Ar) )
	{
		// The level handled it.
		Processed = 1;
	}
	else if( UEngine::Exec(Stream,Ar) )
	{
		// The engine handled it.
		Processed = 1;
	}
	else if( ParseCommand(&Str,TEXT("SELECTNAME")) )
	{
		FName FindName=NAME_None;
		Parse( Str, TEXT("NAME="), FindName );
		for( INT i=0; i<Level->Actors.Num(); i++ )
			if( Level->Actors(i) )
				SelectActor( Level, Level->Actors(i), Level->Actors(i)->GetFName()==FindName, 0 );

		Processed = 1;
	}
    // gam ---
	else if( ParseCommand(&Str,TEXT("DUMPINT")) )
	{
        FString PackageName, IntName;

        UBOOL UseLevelAsPackage;
        UBOOL ExportFresh = 0;

        UBOOL gotFile = Parse (Str, TEXT("FILE="), IntName);
        UBOOL gotPackage = Parse (Str, TEXT("PACKAGE="), PackageName);
        
        Parse (Str, TEXT("FRESH="), ExportFresh);
        
        if (gotFile && gotPackage)
        {
            UseLevelAsPackage = false;
        }
		else if (gotFile && !gotPackage)
        {
            UseLevelAsPackage = true;
        }
        else if (gotPackage && !gotFile)
        {
            UseLevelAsPackage = false;
            IntName = PackageName + TEXT(".int");            
        }
		else
        {
			TCHAR Tmp[256],Loc[256];

		    while( *Str==' ' )
			    Str++;

            PackageName = Str;

			appStrcpy( Tmp, Str );
			if( appStrchr(Tmp,'.') )
				*appStrchr(Tmp,'.') = 0;
			appStrcat( Tmp, TEXT(".int") );
			appStrcpy( Loc, appBaseDir() );
			appStrcat( Loc, Tmp );

            IntName = Loc;

            UseLevelAsPackage = false;
        }

        if( !UseLevelAsPackage )
        {
            IntExport( *PackageName, *IntName, ExportFresh, 1 );
        }
        else
        {
            check (Level);
            check (Level->GetOuter());

            IntExport( Level->GetOuter(), *IntName, ExportFresh, 1 );
        }

		return 1;
	}
    // -- gam
	else if( ParseCommand(&Str,TEXT("JUMPTO")) )
	{
		TCHAR A[32], B[32], C[32];
		ParseToken( Str, A, ARRAY_COUNT(A), 0 );
		ParseToken( Str, B, ARRAY_COUNT(B), 0 );
		ParseToken( Str, C, ARRAY_COUNT(C), 0 );
		for( INT i=0; i<Client->Viewports.Num(); i++ )
			Client->Viewports(i)->Actor->Location = FVector(appAtoi(A),appAtoi(B),appAtoi(C));
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("LSTAT")) )
	{
		TArray<FVector> Sizes;
		for( INT i=0; i<Level->Model->LightMaps.Num(); i++ )
			new(Sizes)FVector(Level->Model->LightMaps(i).SizeX,Level->Model->LightMaps(i).SizeY,0);
		/*for( i=0; i<Sizes.Num(); i++ )
			for( INT j=0; j<i; j++ )
				if
				(	(Sizes(j).X>Sizes(i).X)
				||	(Sizes(j).X==Sizes(i).X && Sizes(j).Y>Sizes(i).Y) )
					Exchange( Sizes(i), Sizes(j) );*/
		debugf( TEXT("LightMap Sizes: ") );
		INT DX[17], DY[17], Size=0, Under32=0, Under64=0;
		for( INT i=0; i<9; i++ )
			DX[i]=DY[i]=0;
		for( INT i=0; i<Sizes.Num(); i++ )
		{
			DX[appCeilLogTwo((INT) Sizes(i).X)]++;
			DY[appCeilLogTwo((INT) Sizes(i).Y)]++;
			Size += (INT) (Sizes(i).X*Sizes(i).Y);
			if( Sizes(i).X<=32 && Sizes(i).Y<=32 )
				Under32++;
			if( Sizes(i).X<=64 && Sizes(i).Y<=64 )
				Under64++;
		}
		debugf( TEXT("Size=%iK elements"), Size/1024);
		debugf( TEXT("Under32=%f%% Under64=%f%%"), 100.0*Under32/Sizes.Num(), 100.0*Under64/Sizes.Num() );
		for( INT i=0; i<9; i++ )
		{
			debugf
			(
				TEXT("Distribution (%i..%i) X=%f%% Y=%f%%"),
				(1<<i)/2+1,
				(1<<i),
				100.0*DX[i]/Sizes.Num(),
				100.0*DY[i]/Sizes.Num()
			);
		}
		debugf( TEXT("Collision hulls=%i"), Level->Model->LeafHulls.Num() );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("REND")) )
    {
        UBOOL bProcessed = 0;
        for (TObjectIterator<UViewport> It; It; ++It)
        {
            bProcessed = It->Exec(Stream, Ar);
        }
        return bProcessed;
    }
	else if (ParseCommand(&Str,TEXT("NEWPICKUPBASES")))
	{
		INT Count = 0;
		for (INT i = 0; i < Level->Actors.Num(); i++)
			if (Level->Actors(i))
			{
				AxPickUpBase* PickupBase = Cast<AxPickUpBase>(Level->Actors(i));
				if (PickupBase && PickupBase->NewStaticMesh && PickupBase->StaticMesh == PickupBase->GetClass()->GetDefaultActor()->StaticMesh)
				{
					PickupBase->SetStaticMesh(PickupBase->NewStaticMesh);
					PickupBase->Skins.Empty();
					PickupBase->SetDrawScale(PickupBase->NewDrawScale);
					PickupBase->PrePivot = PickupBase->NewPrePivot;
					debugf(TEXT("Successfully set new static mesh for pickup base: %s"), Level->Actors(i)->GetName());
					Count++;
				}
			}
		GWarn->Logf(NAME_Log, TEXT("%i pickup bases converted"), Count);

		return 1;
	}
	return Processed;
	unguardf(( TEXT("(%s)%s"), ErrorTemp, appStrlen(ErrorTemp)>=69 ? TEXT("..") : TEXT("") ));
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

