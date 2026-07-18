//=============================================================================
// xObjExporters - Wavefront OBJ format exporters.
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#include "EditorPrivate.h"

/*------------------------------------------------------------------------------
	ULevelExporterOBJ implementation.
------------------------------------------------------------------------------*/

void ULevelExporterOBJ::StaticConstructor()
{
	guard(ULevelExporterOBJ::StaticConstructor);
	SupportedClass = ULevel::StaticClass();
	bText = 1;
	new(Formats)FString(TEXT("OBJ"));
	unguard;
}

static const DWORD OBJSupportedPolyFlags = 0
    | PF_Masked
    | PF_Modulated
    | PF_Flat
    | PF_TwoSided
    | PF_Translucent
    | PF_Environment
    | PF_NoSmooth
    | PF_Unlit
    | PF_AlphaTexture
    | PF_SpecialLit
;

static void PolyFlagsToMaterialName( FString &MaterialName, DWORD PolyFlags )
{
    guard(PolyFlagsToMaterialName)

    DWORD TempPolyFlags = PolyFlags & OBJSupportedPolyFlags;

    if (TempPolyFlags & PF_Masked)
    {
        MaterialName += TEXT (".mask");
        TempPolyFlags ^= PF_Masked;
    }

    if (TempPolyFlags & PF_Modulated)
    {
        MaterialName += TEXT (".modulate");
        TempPolyFlags ^= PF_Modulated;
    }

    if (TempPolyFlags & PF_Flat)
    {
        MaterialName += TEXT (".flat");
        TempPolyFlags ^= PF_Flat;
    }

    if (TempPolyFlags & PF_TwoSided)
    {
        MaterialName += TEXT (".two-sided");
        TempPolyFlags ^= PF_TwoSided;
    }

    if (TempPolyFlags & PF_Translucent)
    {
        MaterialName += TEXT (".translucent");
        TempPolyFlags ^= PF_Translucent;
    }

    if (TempPolyFlags & PF_Environment)
    {
        MaterialName += TEXT (".enviro");
        TempPolyFlags ^= PF_Environment;
    }

    if (TempPolyFlags & PF_NoSmooth)
    {
        MaterialName += TEXT (".no-Smooth");
        TempPolyFlags ^= PF_NoSmooth;
    }

    if (TempPolyFlags & PF_Unlit)
    {
        MaterialName += TEXT (".unlit");
        TempPolyFlags ^= PF_Unlit;
    }

    if (TempPolyFlags & PF_AlphaTexture)
    {
        MaterialName += TEXT (".alpha");
        TempPolyFlags ^= PF_AlphaTexture;
    }

    if (TempPolyFlags & PF_SpecialLit)
    {
        MaterialName += TEXT (".special-lit");
        TempPolyFlags ^= PF_SpecialLit;
    }

    check (!TempPolyFlags);

    unguard;
}

static void ExportPolys( UPolys* Polys, INT &PolyNum, INT TotalPolys, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	guard (ExportPolys);

    UMaterial *DefaultMaterial = (Cast<UMaterial>(UMaterial::StaticClass()->GetDefaultObject())->DefaultMaterial);

    UMaterial *CurrentMaterial;
    DWORD CurrentPolyFlags;

    INT i;

    CurrentMaterial = DefaultMaterial;
    CurrentPolyFlags = 0;

	for (i = 0; i < Polys->Element.Num(); i++)
	{
        Warn->StatusUpdatef( PolyNum++, TotalPolys, TEXT("Exporting Level To OBJ") );

		FPoly *Poly = &Polys->Element(i);

        int j;

        if (
            (!Poly->Material && (CurrentMaterial != DefaultMaterial)) ||
            (Poly->Material && (Poly->Material != CurrentMaterial)) ||
            ((Poly->PolyFlags & OBJSupportedPolyFlags) != CurrentPolyFlags)
           )
        {
            FString Material;

            CurrentMaterial = Poly->Material;
            CurrentPolyFlags = Poly->PolyFlags & OBJSupportedPolyFlags;

            if( CurrentMaterial )
        	    Material = FString::Printf (TEXT("usemtl %s"), CurrentMaterial->GetName());
            else
        	    Material = FString::Printf (TEXT("usemtl DefaultMaterial"));

            PolyFlagsToMaterialName( Material, CurrentPolyFlags );

            Ar.Logf (TEXT ("%s\n"), *Material );
        }

		for (j = 0; j < Poly->NumVertices; j++)
        {
            // Transform to Lightwave's coordinate system
			Ar.Logf (TEXT("v %f %f %f\n"), Poly->Vertex[j].X, Poly->Vertex[j].Z, Poly->Vertex[j].Y);
        }

		FVector	TextureBase = Poly->Base;

        FVector	TextureX, TextureY;
        if( CurrentMaterial && CurrentMaterial->IsA( UBitmapMaterial::StaticClass() ) )
        {
            UBitmapMaterial* BitmapMaterial = (UBitmapMaterial*)CurrentMaterial;
		    TextureX = Poly->TextureU / (FLOAT) BitmapMaterial->USize;
		    TextureY = Poly->TextureV / (FLOAT) BitmapMaterial->VSize;
        }
        else
        {
		    TextureX = Poly->TextureU / (FLOAT) 128;
		    TextureY = Poly->TextureV / (FLOAT) 128;
        }

		for (j = 0; j < Poly->NumVertices; j++)
        {
            // Invert the y-coordinate (Lightwave has their bitmaps upside-down from us).
    		Ar.Logf (TEXT("vt %f %f\n"),
			    (Poly->Vertex[j] - TextureBase) | TextureX, -((Poly->Vertex[j] - TextureBase) | TextureY));
        }

		Ar.Logf (TEXT("f "));

        // Reverse the winding order so Lightwave generates proper normals:
		for (j = Poly->NumVertices - 1; j >= 0; j--)
			Ar.Logf (TEXT("%i/%i "), (j - Poly->NumVertices), (j - Poly->NumVertices));

		Ar.Logf (TEXT("\n"));
	}

    unguard;
}


UBOOL ULevelExporterOBJ::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	guard (ULevelExporterOBJ::ExportText);

	ULevel* Level = CastChecked<ULevel> (Object);

	GEditor->bspBuildFPolys( Level->Model, 0, 0 );
	UPolys* Polys = Level->Model->Polys;

    UMaterial *DefaultMaterial = (Cast<UMaterial>(UMaterial::StaticClass()->GetDefaultObject())->DefaultMaterial);

    UMaterial *CurrentMaterial;
    DWORD CurrentPolyFlags;

    INT i, j, TotalPolys;
    INT PolyNum;
    INT iActor;

    // Calculate the total number of polygons to export:

    PolyNum = 0;
    TotalPolys = Polys->Element.Num();

	for( iActor=0; iActor<Level->Actors.Num(); iActor++ )
	{
		AActor* Actor = Level->Actors(iActor);

        if( !Actor )
            continue;
        
        if( !Actor->StaticMesh )
            continue;
         
        UStaticMesh* StaticMesh = CastChecked<UStaticMesh> (Actor->StaticMesh);

        TotalPolys += StaticMesh->RawTriangles.Num();
    }

    // Export the BSP

	Ar.Logf (TEXT("# OBJ File Generated by UnrealEd\n"));

	Ar.Logf (TEXT("o MyLevel\n"));
    Ar.Logf (TEXT("g BSP\n") );

    ExportPolys( Polys, PolyNum, TotalPolys, Ar, Warn );

    // Export the static meshes

    CurrentMaterial = DefaultMaterial;
    CurrentPolyFlags = 0;

	for( iActor=0; iActor<Level->Actors.Num(); iActor++ )
	{
		AActor* Actor = Level->Actors(iActor);

        if( !Actor )
            continue;

        if( Actor->IsA(ATerrainInfo::StaticClass()) )
        {
            ATerrainInfo* pTerrain = (ATerrainInfo*)Actor;
            // Generate texture coordinates
            FCoords TexCoords = ( GMath.UnitCoords / FRotator( 0.f, pTerrain->Layers[0].TextureRotation, 0.f) );
            TexCoords *= pTerrain->ToHeightmap.Transpose();
            TexCoords.XAxis /= pTerrain->Layers[0].UScale;
            TexCoords.YAxis /= pTerrain->Layers[0].VScale;
            TexCoords.Origin += FVector( pTerrain->Layers[0].UPan*pTerrain->Layers[0].UScale, pTerrain->Layers[0].VPan*pTerrain->Layers[0].VScale, 0.f ).TransformVectorBy(pTerrain->ToWorld);

            switch( pTerrain->Layers[0].TextureMapAxis )
            {
            case TEXMAPAXIS_XY:
	            break;
            case TEXMAPAXIS_XZ:
	            Exchange( TexCoords.Origin.Y, TexCoords.Origin.Z );
	            Exchange( TexCoords.Origin.X, TexCoords.Origin.Z );
	            Exchange( TexCoords.XAxis.X, TexCoords.XAxis.Z );
	            Exchange( TexCoords.YAxis.X, TexCoords.YAxis.Y );
	            Exchange( TexCoords.ZAxis.Y, TexCoords.ZAxis.Z );
	            break;
            case TEXMAPAXIS_YZ:
	            Exchange( TexCoords.Origin.X, TexCoords.Origin.Z );
	            Exchange( TexCoords.XAxis.X, TexCoords.XAxis.Z );
	            Exchange( TexCoords.ZAxis.X, TexCoords.ZAxis.Z );
	            break;
            }
            TexCoords = TexCoords * ( GMath.UnitCoords / pTerrain->Layers[0].LayerRotation );
            
            Ar.Logf (TEXT("g %s\n"), pTerrain->GetName() );
                
            FString Material;
            UMaterial* CurrentMaterial = pTerrain->Layers[0].Texture;
            if( CurrentMaterial )
        	    Material = FString::Printf (TEXT("usemtl %s"), CurrentMaterial->GetName());
            else
        	    Material = FString::Printf (TEXT("usemtl DefaultMaterial"));

            Ar.Logf (TEXT ("%s\n"), *Material);
	        
	        for( INT y=0; y<pTerrain->HeightmapY; y++ )
	        {
		        for( INT x=0; x<pTerrain->HeightmapX; x++ )
		        {
			        if( x>0 && y>0 )
			        {
                        FVector v[3];
				        if( pTerrain->GetEdgeTurnBitmap( x-1, y-1 ) )
				        {
					        // 124, 423
                            v[0] = pTerrain->Vertices(pTerrain->GetGlobalVertex(x-1, y-1));
                            v[1] = pTerrain->Vertices(pTerrain->GetGlobalVertex(x, y-1));
                            v[2] = pTerrain->Vertices(pTerrain->GetGlobalVertex(x-1, y));

                            for( j=0; j<3; j++ )
                            {
                                Ar.Logf( TEXT("v %f %f %f\n"), v[j].X, v[j].Z, v[j].Y );
                            }

                            for( j=0; j<3; j++ )
                            {
                                v[j] = v[j].TransformPointBy(TexCoords);
                                Ar.Logf( TEXT("vt %f %f\n"), v[j].X, -v[j].Y );
                            }

                            Ar.Logf (TEXT("f "));
		                    for( j=2; j>-1; j-- )
			                    Ar.Logf (TEXT("%i/%i "), (j - 3), (j - 3));
		                    Ar.Logf (TEXT("\n"));

                            v[0] = pTerrain->Vertices(pTerrain->GetGlobalVertex(x-1, y));
                            v[1] = pTerrain->Vertices(pTerrain->GetGlobalVertex(x, y-1));
                            v[2] = pTerrain->Vertices(pTerrain->GetGlobalVertex(x, y));

                            for( j=0; j<3; j++ )
                            {
                                Ar.Logf( TEXT("v %f %f %f\n"), v[j].X, v[j].Z, v[j].Y );
                            }

                            for( j=0; j<3; j++ )
                            {
                                v[j] = v[j].TransformPointBy(TexCoords);
                                Ar.Logf( TEXT("vt %f %f\n"), v[j].X, -v[j].Y );
                            }

                            Ar.Logf (TEXT("f "));
		                    for( j=2; j>-1; j-- )
			                    Ar.Logf (TEXT("%i/%i "), (j - 3), (j - 3));
		                    Ar.Logf (TEXT("\n"));

				        }
				        else
				        {
                            v[0] = pTerrain->Vertices(pTerrain->GetGlobalVertex(x-1, y-1));
                            v[1] = pTerrain->Vertices(pTerrain->GetGlobalVertex(x, y-1));
                            v[2] = pTerrain->Vertices(pTerrain->GetGlobalVertex(x, y));

                            for( j=0; j<3; j++ )
                            {
                                Ar.Logf( TEXT("v %f %f %f\n"), v[j].X, v[j].Z, v[j].Y );
                            }

                            for( j=0; j<3; j++ )
                            {
                                v[j] = v[j].TransformPointBy(TexCoords);
                                Ar.Logf( TEXT("vt %f %f\n"), v[j].X, -v[j].Y );
                            }

                            Ar.Logf (TEXT("f "));
		                    for( j=2; j>-1; j-- )
			                    Ar.Logf (TEXT("%i/%i "), (j - 3), (j - 3));
		                    Ar.Logf (TEXT("\n"));

                            v[0] = pTerrain->Vertices(pTerrain->GetGlobalVertex(x-1, y-1));
                            v[1] = pTerrain->Vertices(pTerrain->GetGlobalVertex(x, y));
                            v[2] = pTerrain->Vertices(pTerrain->GetGlobalVertex(x-1, y));

                            for( j=0; j<3; j++ )
                            {
                                Ar.Logf( TEXT("v %f %f %f\n"), v[j].X, v[j].Z, v[j].Y );
                            }

                            for( j=0; j<3; j++ )
                            {
                                v[j] = v[j].TransformPointBy(TexCoords);
                                Ar.Logf( TEXT("vt %f %f\n"), v[j].X, -v[j].Y );
                            }

                            Ar.Logf (TEXT("f "));
		                    for( j=2; j>-1; j-- )
			                    Ar.Logf (TEXT("%i/%i "), (j - 3), (j - 3));
		                    Ar.Logf (TEXT("\n"));
				        }
			        }
		        }
	        }      
        }
        
        if( !Actor->StaticMesh )
            continue;

        FMatrix LocalToWorld = Actor->LocalToWorld();
         
    	Ar.Logf (TEXT("g %s\n"), Actor->GetName() );

        UStaticMesh* StaticMesh = CastChecked<UStaticMesh> (Actor->StaticMesh);

        StaticMesh->RawTriangles.Load();

	    for (i = 0; i < StaticMesh->RawTriangles.Num(); i++)
	    {
            Warn->StatusUpdatef( PolyNum++, TotalPolys, TEXT("Exporting Level To OBJ") );

            const FStaticMeshTriangle &Triangle = StaticMesh->RawTriangles(i);

            if (
                (!Triangle.LegacyMaterial && (CurrentMaterial != DefaultMaterial)) ||
                (Triangle.LegacyMaterial && (Triangle.LegacyMaterial != CurrentMaterial)) ||
                ((Triangle.LegacyPolyFlags & OBJSupportedPolyFlags) != CurrentPolyFlags)
               )
            {
                FString Material;

                CurrentMaterial = StaticMesh->GetSkin(Actor,Triangle.MaterialIndex); // sjs
                CurrentPolyFlags = Triangle.LegacyPolyFlags & OBJSupportedPolyFlags;

                if( CurrentMaterial )
        	        Material = FString::Printf (TEXT("usemtl %s"), CurrentMaterial->GetName());
                else
        	        Material = FString::Printf (TEXT("usemtl DefaultMaterial"));

                PolyFlagsToMaterialName( Material, CurrentPolyFlags );

                Ar.Logf (TEXT ("%s\n"), *Material);
            }

		    for( j = 0; j < ARRAY_COUNT(Triangle.Vertices); j++ )
            {
                FVector V = Triangle.Vertices[j];

                V = LocalToWorld.TransformFVector( V );

                // Transform to Lightwave's coordinate system
			    Ar.Logf( TEXT("v %f %f %f\n"), V.X, V.Z, V.Y );
            }

		    for( j = 0; j < ARRAY_COUNT(Triangle.Vertices); j++ )
            {
                // Invert the y-coordinate (Lightwave has their bitmaps upside-down from us).
    		    Ar.Logf( TEXT("vt %f %f\n"), Triangle.UVs[j][0].U, -Triangle.UVs[j][0].V );
            }

		    Ar.Logf (TEXT("f "));

		    for( j = 0; j < ARRAY_COUNT(Triangle.Vertices); j++ )
			    Ar.Logf (TEXT("%i/%i "), (j - ARRAY_COUNT(Triangle.Vertices)), (j - ARRAY_COUNT(Triangle.Vertices)));

		    Ar.Logf (TEXT("\n"));
	    }
	}

	Level->Model->Polys->Element.Empty();

	Ar.Logf (TEXT("# dElaernU yb detareneG eliF JBO\n"));
	return 1;
	unguard;
}

IMPLEMENT_CLASS(ULevelExporterOBJ);

/*------------------------------------------------------------------------------
	UPolysExporterOBJ implementation.
------------------------------------------------------------------------------*/

void UPolysExporterOBJ::StaticConstructor()
{
	guard(UPolysExporterOBJ::StaticConstructor);
	SupportedClass = UPolys::StaticClass();
	bText = 1;
	new(Formats)FString(TEXT("OBJ"));
	unguard;
}

UBOOL UPolysExporterOBJ::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	guard (ULevelExporterOBJ::ExportText);

    UPolys* Polys = CastChecked<UPolys> (Object);

    INT PolyNum = 0;
    INT TotalPolys = Polys->Element.Num();

	Ar.Logf (TEXT("# OBJ File Generated by UnrealEd\n"));

    ExportPolys( Polys, PolyNum, TotalPolys, Ar, Warn );

	Ar.Logf (TEXT("# dElaernU yb detareneG eliF JBO\n"));

	return 1;
	unguard;
}

IMPLEMENT_CLASS(UPolysExporterOBJ);

