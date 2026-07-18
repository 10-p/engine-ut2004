/*=============================================================================
	UnLightwaveImporter.cpp: Lightwave object importer code
	Copyright 2001-2002 Digital Extremes - All Rights Reserved.

	Revision history:
		* Created by Jeff Jam
=============================================================================*/

#include "EditorPrivate.h"

#define UV2SUPPORT
//#define XFADESUPPORT
//#define DECOSUPPORT
#define MSASUPPORT

UBOOL GImportedBrush = false;

//=================================================================================================
// Material Option Processing

bool ParseMaterialOption(const TCHAR **Str, FString &Option)
{
    TCHAR *tokenEnd;

    if (**Str == '\0')
        return (false);

    tokenEnd = appStrchr (*Str, '.');

    if (!tokenEnd)
    {
        Option = FString (*Str);
        *Str += Option.Len ();
    }
    else
    {
        int tokenLength;

        tokenLength = tokenEnd - *Str;
        Option = FString (*Str).Left (tokenLength);
        // eat up the trailing '.'
        *Str = tokenEnd + 1;
    }

    return (true);
}


//=================================================================================================
// Data Reads from Big Endian

INT GrabIntFromArray(TArray<BYTE> &Data, INT &indexptr)
{
    INT index = indexptr;

    indexptr += 4;

    if (index+3 < Data.Num())
    {        
        return (INT)((Data(index) << 24) | (Data(index+1) << 16) | (Data(index+2) << 8) | (Data(index+3)));
    }
    return 0;
}

_WORD GrabWordFromArray(TArray<BYTE> &Data, INT &indexptr)
{
    INT index = indexptr;

    indexptr += 2;

    if (index+1 < Data.Num())
    {
        return (_WORD)((Data(index) << 8) | (Data(index+1)));
    }
    return 0;
}

FLOAT GrabFloatFromArray(TArray<BYTE> &Data, INT &indexptr)
{
    INT index = indexptr;

    indexptr += 4;
    
    if (index+3 < Data.Num())
    {
        INT c = (INT)((Data(index) << 24) | (Data(index+1) << 16) | (Data(index+2) << 8) | (Data(index+3)));
        FLOAT *fptr = (FLOAT*)((void*)&c);
        return *fptr;
    }
    return 0.f;
}

const TCHAR* GrabStringFromArray(TArray<BYTE> &Data, INT &indexptr)
{
    static char ascstring[256] = "";  
    
    INT index = indexptr;

    indexptr += 2;                
               
    while(Data(indexptr - 1) != 0) indexptr += 2;
    
    for(INT j = 0; j < (indexptr - index); j++)
    {
        ascstring[j] = Data(index + j);
        
        // downcase the character (A-Z)->(a-z)
        if (ascstring[j] >= 65 && ascstring[j] <= 90)
            ascstring[j] |= 32;
    }
    
    return appFromAnsi(ascstring);
}

//=================================================================================================
// Chunk Processing

void ProcessVertexChunk(TArray<BYTE> &Data, TArray<FStaticMeshVertex> &Vertices, FVector *Pivot, INT ChunkLength, INT ReadPtr)
{
    INT ChunkReadPtr = ReadPtr;
    INT vertexcount = ChunkLength/12;
    
    debugf( NAME_Log, TEXT("Processing PNTS, Vertex Count = %d."), vertexcount );

    // allocate memory for the vertices
    Vertices.Add(vertexcount);        

    // read in the vertices
    for(INT j = 0; j < vertexcount; j++)
    {
        FStaticMeshVertex *V = &Vertices(j);
    
        // read the vertex
        V->Position.X = GrabFloatFromArray(Data,ChunkReadPtr);
        V->Position.Z = GrabFloatFromArray(Data,ChunkReadPtr);
        V->Position.Y = -GrabFloatFromArray(Data,ChunkReadPtr);

        // adjust the vertex based on the current layer's pivot
        V->Position.X -= Pivot->X;
        V->Position.Y -= Pivot->Y;
        V->Position.Z -= Pivot->Z;
    }
}

void ProcessVertexMappingChunk
(
    TArray<BYTE> &Data, 
    TArray<FStaticMeshVertex> &Vertices,
    TArray<FVector> &PriTexCoords,
    TArray<FVector> &SecTexCoords,
    INT ChunkLength, 
    INT ReadPtr
)
{
    INT ChunkReadPtr = ReadPtr;
    INT ChunkTag = GrabIntFromArray(Data,ChunkReadPtr);

    // make sure this vertex-mapping is for UV coords
    if (ChunkTag != (INT)(('T' << 24) + ('X' << 16) + ('U' << 8) + 'V'))
        return;

    // read dimension (assume it's 2, it's difficult to cleanly error out of multi-layer LWO importing)
    _WORD dimension = GrabWordFromArray(Data,ChunkReadPtr);
    
    if (dimension != 2)
    {
        GWarn->Logf( NAME_Error, TEXT("VMAP dimension is %d (should be 2!)."), dimension );
        return;
    }

    // read the name of this vertex-mapping
    const TCHAR* Ch = GrabStringFromArray(Data,ChunkReadPtr);        
    debugf( NAME_DevLoad, TEXT("Processing VMAP TXUV, Name = %s."), Ch );
            
    // texture coord count
    INT texturecoordcount = (ChunkLength - (ChunkReadPtr - ReadPtr)) / 10;                
            
    // is this vertex-mapping the secondary one?
    if (appStrstr(Ch,TEXT("uv2")))
    {
        debugf( NAME_Log, TEXT("Processing secondary UV map.") );

        // allocate memory for the secondary texture coords, if we haven't already
        if (SecTexCoords.Num() == 0)
            SecTexCoords.AddZeroed(Vertices.Num());    
       
        // read in the secondary texture coords
        for(INT j = 0; j < texturecoordcount; j++)
        {
            // read index into vertex array
            _WORD index = GrabWordFromArray(Data,ChunkReadPtr);
            
            // read the texture coordinate
            FVector *TC = &SecTexCoords(index);                        
            TC->X = GrabFloatFromArray(Data,ChunkReadPtr);
            TC->Y = 1.f - GrabFloatFromArray(Data,ChunkReadPtr);
        }
    }
    else
    {
        debugf( NAME_Log, TEXT("Processing primary UV map: %s."), Ch );
        
        // allocate memory for the primary texture coords, if we haven't already
        if (PriTexCoords.Num() == 0)
            PriTexCoords.AddZeroed(Vertices.Num());

        // read in the texture coords
        for(INT j = 0; j < texturecoordcount; j++)
        {
            // read index into vertex array
            _WORD index = GrabWordFromArray(Data,ChunkReadPtr);
            
            // read the texture coordinate
            FVector *TC = &PriTexCoords(index);                        
            TC->X = GrabFloatFromArray(Data,ChunkReadPtr);
            TC->Y = 1.f - GrabFloatFromArray(Data,ChunkReadPtr);
        }
    }
}

INT ProcessPolygonChunk
(
    TArray<BYTE> &Data,
    TArray<FStaticMeshVertex> &Vertices,
    TArray<FVector> &PriTexCoords,
    TArray<FPoly> &Polygons,
    INT ChunkLength, 
    INT ReadPtr
)
{
    FVector Dummy0(0.f,0.f,0.f);
    FVector Dummy1(0.f,1.f,0.f);
    FVector Dummy2(0.f,0.f,1.f);

    INT ChunkReadPtr = ReadPtr;
    INT ChunkTag = GrabIntFromArray(Data,ChunkReadPtr);

    // make sure this polygon chunk is actual face data
    if (ChunkTag != (INT)(('F' << 24) + ('A' << 16) + ('C' << 8) + 'E'))
        return 0;

    // read in the polygon faces
    while(ChunkReadPtr < ((ReadPtr + ChunkLength)-4))
    {
        // add a new polygon to the brush
        Polygons.Add();        
        FPoly *Poly = &Polygons(Polygons.Num()-1);
        Poly->Init();
        
        _WORD vertexcount = GrabWordFromArray(Data,ChunkReadPtr);
        vertexcount &= 0x03FFFFFF;
        
        Poly->NumVertices = vertexcount;
        
        // ignore polygons with less than 3 verts!
        if (vertexcount < 3)
        {
            GWarn->Logf( NAME_Warning, TEXT("Degenerate polygon found, ignoring it! (%d verts)"), vertexcount );

            // make a dummy triangle out of it so the rest of the file parsing isn't affected
            Poly->Vertex[0] = Dummy0;
            Poly->Vertex[1] = Dummy1;
            Poly->Vertex[2] = Dummy2;

            for(INT j = 0; j < 3; j++)
            {
                Poly->UV[j].X = 1.0f+j;
                Poly->UV[j].Y = 0.0f+j;
                Poly->UV[j].Z = 0.0f;
            }            
        }
        else
        {        
            // read the vertex indices
            for(INT j = 0; j < vertexcount; j++)
            {
                _WORD index;
                index = GrabWordFromArray(Data,ChunkReadPtr);                        
            
                Poly->Vertex[j] = Vertices(index).Position;

                // store just the first three vertex texture coords for texture matrix generation
                if (j < 3)
                {
                    Poly->UV[j].X = PriTexCoords(index).X;
                    Poly->UV[j].Y = PriTexCoords(index).Y;
                    Poly->UV[j].Z = 0.f;
                }
            }                    
        }
        
		Poly->Base = Poly->Vertex[0];
		Poly->Finalize(0);
    }

    return Polygons.Num();
}

INT ProcessTrianglesOnlyChunk
(
    TArray<BYTE> &Data, 
    TArray<FStaticMeshVertex> &Vertices,
    TArray<FVector> &PriTexCoords,
    TArray<FVector> &SecTexCoords,
    TArray<FStaticMeshTriangle> &Triangles,
    TArray<INT> &PolySurfs,
    UMaterial *DefaultMaterial,
    INT ChunkLength, 
    INT ReadPtr
) 
{
    INT ChunkReadPtr = ReadPtr;
    INT ChunkTag = GrabIntFromArray(Data,ChunkReadPtr);
    INT facecount = 0;
    FVector Dummy0(0.f,0.f,0.f);
    FVector Dummy1(0.f,1.f,0.f);
    FVector Dummy2(0.f,0.f,1.f);

    // make sure this polygon chunk is actual face data
    if (ChunkTag != (INT)(('F' << 24) + ('A' << 16) + ('C' << 8) + 'E'))
        return 0;

    // read in the polygon faces
    while(ChunkReadPtr < ((ReadPtr + ChunkLength)-4))
    {
        Triangles.Add();
        PolySurfs.Add();     
        
        FStaticMeshTriangle *Tri = &Triangles(facecount);
        Tri->NumUVs = 1;
        
        // default the polygon to the first surface definition in the Lightwave file
        PolySurfs(facecount) = 0;            
        
        facecount++;

        _WORD vertexcount = GrabWordFromArray(Data,ChunkReadPtr);
        vertexcount &= 0x03FFFFFF;

        // ignore polygons that aren't triangles!
        if (vertexcount != 3)
        {
            GWarn->Logf( NAME_Warning, TEXT("Non-triangular polygon found, ignoring it! (%d verts)"), vertexcount );

            // make a dummy triangle out of it so the rest of the file parsing isn't affected
            {
                Tri->Vertices[0] = Dummy0;
                Tri->Vertices[1] = Dummy1;
                Tri->Vertices[2] = Dummy2;

                for(INT j = 0; j < 3; j++)
                {
                    Tri->UVs[j][0].U = 1.0f;
                    Tri->UVs[j][0].V = 1.0f;
				#ifdef UV2SUPPORT
                    Tri->UVs[j][1].U = 1.0f;
                    Tri->UVs[j][1].V = 1.0f;
				#endif
                }

				Tri->NumUVs = Max(1,Tri->NumUVs);            

                // initialize polyflags and default texture (they will be set properly by the polygon-tag mapping later on)
                Tri->LegacyPolyFlags = 0;
                Tri->LegacyMaterial = DefaultMaterial;

                // default vertex colors to full white
                Tri->Colors[0] = 0xFFFFFFFF;
                Tri->Colors[1] = 0xFFFFFFFF;
                Tri->Colors[2] = 0xFFFFFFFF;
            }
            
            ChunkReadPtr += 2*vertexcount;
            continue;
        }
        
        // read the vertex indices
        for(INT j = 0; j < 3; j++)
        {
            _WORD index;
            index = GrabWordFromArray(Data,ChunkReadPtr);                        
            
            Tri->Vertices[j] = Vertices(index).Position;
            
            if (PriTexCoords.Num() > 0)
            {                        
                Tri->UVs[j][0].U = PriTexCoords(index).X;
                Tri->UVs[j][0].V = PriTexCoords(index).Y;                    
				Tri->NumUVs = Max(1,Tri->NumUVs);
            }

            #ifdef UV2SUPPORT                
            if (SecTexCoords.Num() > 0)
            {
                Tri->UVs[j][1].U = SecTexCoords(index).X;
                Tri->UVs[j][1].V = SecTexCoords(index).Y;
				Tri->NumUVs = Max(2,Tri->NumUVs);
            }
            #endif
        }                    
        
        // re-order the vertices, winding order was changed when we mirrored in y...
		Exchange( Tri->Vertices[0], Tri->Vertices[2] );
        
        // ...and swap the texture coords as well
		Exchange( Tri->UVs[0][0], Tri->UVs[2][0] );

        #ifdef UV2SUPPORT
		Exchange( Tri->UVs[0][1], Tri->UVs[2][1] );
        #endif

        // initialize polyflags and default texture (they will be set properly by the polygon-tag mapping later on)
        Tri->LegacyPolyFlags = 0;
        Tri->LegacyMaterial = DefaultMaterial;

        // default vertex colors to full white
        Tri->Colors[0] = 0xFFFFFFFF;
        Tri->Colors[1] = 0xFFFFFFFF;
        Tri->Colors[2] = 0xFFFFFFFF;
    }

    debugf( NAME_Log, TEXT("Processing POLS FACE, polygon count = %d."), facecount );    
    return facecount;
}

void ProcessDiscontinuousPolygonVertexMappingChunk
(
    TArray<BYTE> &Data, 
    TArray<FStaticMeshVertex> &Vertices,
    TArray<FPoly> &Polygons,
    INT ChunkLength, 
    INT ReadPtr
)
{
    INT ChunkReadPtr = ReadPtr;
    INT ChunkTag = GrabIntFromArray(Data,ChunkReadPtr);
    INT k = 0;
    
    // make sure this vertex-mapping is for UV coords
    if (ChunkTag != (INT)(('T' << 24) + ('X' << 16) + ('U' << 8) + 'V'))
        return;

    // read dimension (assume it's 2, it's difficult to cleanly error out of multi-layer LWO importing)
    _WORD dimension = GrabWordFromArray(Data,ChunkReadPtr);
    
    if (dimension != 2)
    {
        GWarn->Logf( NAME_Warning, TEXT("VMAD dimension is %d (should be 2!)."), dimension );
        return;
    }
               
    // read the name of this discontinuous vertex-mapping
    const TCHAR* Ch = GrabStringFromArray(Data,ChunkReadPtr);        
    debugf( NAME_Log, TEXT("Processing VMAD TXUV, Name = %s."), Ch );
                
    // texture coord count
    INT texturecoordcount = (ChunkLength - (ChunkReadPtr - ReadPtr)) / 12;                

    debugf( NAME_Log, TEXT("Processing discontinuous UV map.") );

    // read in the over-riding texture coords
    for(INT j = 0; j < texturecoordcount; j++)
    {
        // read index into vertex array
        _WORD vertex_index = GrabWordFromArray(Data,ChunkReadPtr);

        // read index into face (triangle) array
        _WORD face_index = GrabWordFromArray(Data,ChunkReadPtr);
                    
        // read the over-riding texture coord                        
        FLOAT newU = GrabFloatFromArray(Data,ChunkReadPtr);
        FLOAT newV = 1.f - GrabFloatFromArray(Data,ChunkReadPtr);

        FPoly *Poly = &Polygons(face_index);

        // update the related face
        for(k = 0; k < 3; k++)
        {
            if (Poly->Vertex[k] == Vertices(vertex_index).Position)
            {
                Poly->UV[k].X = newU;
                Poly->UV[k].Y = newV;                               
                break;
            }
        }
        
        if (k < 3)
        {
		    FVector	ST1 = Poly->UV[0];
			FVector	ST2 = Poly->UV[1];
			FVector	ST3 = Poly->UV[2];
            INT USize = Poly->Material->MaterialUSize();
            INT VSize = Poly->Material->MaterialVSize();

            FTexCoordsToVectors
            (
			    Poly->Vertex[0],FVector(ST1.X*USize,ST1.Y*VSize,ST1.Z),
			    Poly->Vertex[1],FVector(ST2.X*USize,ST2.Y*VSize,ST2.Z),
			    Poly->Vertex[2],FVector(ST3.X*USize,ST3.Y*VSize,ST3.Z),
			    &Poly->Base,
			    &Poly->TextureU,
			    &Poly->TextureV
		    );
        }
    }
}

void ProcessDiscontinuousTriangleVertexMappingChunk
(
    TArray<BYTE> &Data, 
    TArray<FStaticMeshVertex> &Vertices,
    TArray<FStaticMeshTriangle> &Triangles,
    INT ChunkLength, 
    INT ReadPtr
)
{
    INT ChunkReadPtr = ReadPtr;
    INT ChunkTag = GrabIntFromArray(Data,ChunkReadPtr);
    INT k = 0;
    
    // make sure this vertex-mapping is for UV coords
    if (ChunkTag != (INT)(('T' << 24) + ('X' << 16) + ('U' << 8) + 'V'))
        return;

    // read dimension (assume it's 2, it's difficult to cleanly error out of multi-layer LWO importing)
    _WORD dimension = GrabWordFromArray(Data,ChunkReadPtr);
    
    if (dimension != 2)
    {
        GWarn->Logf( NAME_Warning, TEXT("VMAD dimension is %d (should be 2!)."), dimension );
        return;
    }
               
    // read the name of this discontinuous vertex-mapping
    const TCHAR* Ch = GrabStringFromArray(Data,ChunkReadPtr);        
    debugf( NAME_DevLoad, TEXT("Processing VMAD TXUV, Name = %s."), Ch );
                
    // texture coord count
    INT texturecoordcount = (ChunkLength - (ChunkReadPtr - ReadPtr)) / 12;                

    // is this discontinuous vertex-mapping the secondary one?
    if (appStrstr(Ch,TEXT("uv2")))
    {
        debugf( NAME_Log, TEXT("Processing secondary discontinuous UV map.") );

        // read in the over-riding secondary texture coords
        for(INT j = 0; j < texturecoordcount; j++)
        {
            // read index into vertex array
            _WORD vertex_index = GrabWordFromArray(Data,ChunkReadPtr);
            
            // read index into face (triangle) array
            _WORD face_index = GrabWordFromArray(Data,ChunkReadPtr);
            
            // read the over-riding secondary texture coord                        
            FLOAT newU = GrabFloatFromArray(Data,ChunkReadPtr);
            FLOAT newV = 1.f - GrabFloatFromArray(Data,ChunkReadPtr);

            FStaticMeshTriangle *Tri = &Triangles(face_index);

            // update the related face
            for(k = 0; k < 3; k++)
            {
                if (Tri->Vertices[k] == Vertices(vertex_index).Position)
                {
                    #ifdef UV2SUPPORT
                    Tri->UVs[k][1].U = newU;
                    Tri->UVs[k][1].V = newV;
                    #endif
                    break;
                }
            }

			Tri->NumUVs = Max(2,Tri->NumUVs);
            
            if (k == 3)
            {
                GWarn->Logf( NAME_Warning, TEXT("Unmatched secondary VMAD coord!"));
            }
        }
    }
    else
    {    
        debugf( NAME_Log, TEXT("Processing primary discontinuous UV map.") );

        // read in the over-riding texture coords
        for(INT j = 0; j < texturecoordcount; j++)
        {
            // read index into vertex array
            _WORD vertex_index = GrabWordFromArray(Data,ChunkReadPtr);

            // read index into face (triangle) array
            _WORD face_index = GrabWordFromArray(Data,ChunkReadPtr);
                        
            // read the over-riding texture coord                        
            FLOAT newU = GrabFloatFromArray(Data,ChunkReadPtr);
            FLOAT newV = 1.f - GrabFloatFromArray(Data,ChunkReadPtr);

            FStaticMeshTriangle *Tri = &Triangles(face_index);

            // update the related face
            for(k = 0; k < 3; k++)
            {
                if (Tri->Vertices[k] == Vertices(vertex_index).Position)
                {
					Tri->UVs[k][0].U = newU;
					Tri->UVs[k][0].V = newV;
                    break;
                }
            }

			Tri->NumUVs = Max(1,Tri->NumUVs);

            if (k == 3)
            {
                GWarn->Logf( NAME_Warning, TEXT("Unmatched primary VMAD coord!"));
            }
        }
    }
}

void ProcessSurfaceNames
(
    TArray<BYTE> &Data, 
    TArray<DWORD> &PolyFlags,
    TArray<UMaterial*> &BaseMaterials,
    TArray<FString> &SurfaceNames,
    UMaterial *DefaultMaterial,
    INT ChunkLength, 
    INT ReadPtr
)
{
    INT ChunkReadPtr = ReadPtr;
    INT surfindex = 0;
    INT CollisionSurfaceCount = 0;
    bool CollisionSurface = false;


    while(ChunkReadPtr < (ReadPtr + ChunkLength))
    {
        FString Texture;
        
        const TCHAR* Ch = GrabStringFromArray(Data,ChunkReadPtr);
        debugf( NAME_DevLoad, TEXT("TAG Surface name = %s."), Ch );

        // create and initialize texture for this new surface             
        BaseMaterials.Add();
        BaseMaterials(surfindex) = DefaultMaterial;
                
        new(SurfaceNames)FString(Ch);

        if (ParseMaterialOption(&Ch, Texture))
        {
            // if we have a collision surface, then do not attempt to find a corresponding texture for it
            if (!appStricmp(*Texture,TEXT("col")))
            {
                debugf( NAME_DevLoad, TEXT("Found collision surface."));
                CollisionSurface = true;
                CollisionSurfaceCount++;

                if (CollisionSurfaceCount > 1)
                {
                    // for now, just throw an error message, if we want we could say CollisionSurface = false...
                    GWarn->Logf( NAME_Error, TEXT("Extra collision surface found!"));
                }
            }
            else
            {
                BaseMaterials(surfindex) = FindObject<UMaterial>(ANY_PACKAGE, *Texture);

                if (!BaseMaterials(surfindex))
                {
                    BaseMaterials(surfindex) = DefaultMaterial;
                    GWarn->Logf( NAME_Warning, TEXT("Couldn't find texture for surface name = %s."), *Texture );
                }
                else
                    debugf( NAME_DevLoad, TEXT("Found texture for surface name = %s."), *Texture );
            }
        }               
                
        // create and initialize polygon flags for this new surface
        PolyFlags.Add();
        PolyFlags(surfindex) = 0; // ie. PF_Normal
                
        if (CollisionSurface)
        {
            PolyFlags(surfindex) |= PF_Semisolid; // ride this flag (means "collision polygon" for our purposes)
            CollisionSurface = false;
        }
        
        // parse the surface name to obtain our poly flags
        if (appStrstr(Ch,TEXT("mask")))
        {
            PolyFlags(surfindex) |= PF_Masked;
        }
        else if (appStrstr(Ch,TEXT("translucent")))
        {
            PolyFlags(surfindex) |= PF_Translucent;
        }
        else if (appStrstr(Ch,TEXT("modulate")))
        {
            PolyFlags(surfindex) |= PF_Modulated;
        }
        else if (appStrstr(Ch,TEXT("two-sided")))
        {
            PolyFlags(surfindex) |= PF_TwoSided;
        }
        // otherwise, normal is default

        // attributes
        if (appStrstr(Ch,TEXT("flat")))
        {                        
            PolyFlags(surfindex) = PF_Flat | PolyFlags(surfindex);
        }                        
        if (appStrstr(Ch,TEXT("enviro")))
        {
            PolyFlags(surfindex) = PF_Environment | PolyFlags(surfindex);
        }                        
        if (appStrstr(Ch,TEXT("unlit")))
        {
            PolyFlags(surfindex) = PF_Unlit | PolyFlags(surfindex);
        }                                        
        if (appStrstr(Ch,TEXT("alpha")))
        {
            PolyFlags(surfindex) = PF_AlphaTexture | PolyFlags(surfindex);
        }                
        if (appStrstr(Ch,TEXT("no-Smooth")))
        {
            PolyFlags(surfindex) = PF_NoSmooth | PolyFlags(surfindex);
        }                
        if (appStrstr(Ch,TEXT("special-lit")))
        {
            PolyFlags(surfindex) = PF_SpecialLit | PolyFlags(surfindex);
        }                
        // otherwise none of these are set
                
        // move on to the next surfindex
        surfindex++;
    }
}

void ProcessPolygonSurfaces
(
    TArray<BYTE> &Data,
    TArray<FPoly> &Polygons,
    TArray<DWORD> &PolyFlags,
    TArray<UMaterial*> &BaseMaterials,
    INT ChunkLength, 
    INT ReadPtr
)
{
    INT ChunkReadPtr = ReadPtr;
    INT ChunkTag = GrabIntFromArray(Data,ChunkReadPtr);

    // make sure this polygon-mapping is for surfaces (indirected through the tags structure)
    if (ChunkTag != (INT)(('S' << 24) + ('U' << 16) + ('R' << 8) + 'F'))
        return;

    debugf( NAME_DevLoad, TEXT("Processing PTAG SURF polygon-surface mapping.") );
    INT mappingcount = (ChunkLength - 4)/4;

    for(INT j = 0; j < mappingcount; j++)
    {
        _WORD polyindex = GrabWordFromArray(Data,ChunkReadPtr);
        _WORD surfindex = GrabWordFromArray(Data,ChunkReadPtr);

        FPoly *Poly = &Polygons(polyindex);        

        Poly->PolyFlags = PolyFlags(surfindex);
        Poly->Material = BaseMaterials(surfindex);

		FVector	ST1 = Poly->UV[0];
		FVector	ST2 = Poly->UV[1];
		FVector	ST3 = Poly->UV[2];
        INT USize = Poly->Material->MaterialUSize();
        INT VSize = Poly->Material->MaterialVSize();

        FTexCoordsToVectors
        (
			Poly->Vertex[0],FVector(ST1.X*USize,ST1.Y*VSize,ST1.Z),
			Poly->Vertex[1],FVector(ST2.X*USize,ST2.Y*VSize,ST2.Z),
			Poly->Vertex[2],FVector(ST3.X*USize,ST3.Y*VSize,ST3.Z),
			&Poly->Base,
			&Poly->TextureU,
			&Poly->TextureV
		);		
    }
}

void ProcessTriangleSurfaces
(
    TArray<BYTE> &Data,
    TArray<FStaticMeshTriangle> &Triangles,
    TArray<DWORD> &PolyFlags,
    TArray<UMaterial*> &BaseMaterials,
    TArray<INT> &PolySurfs,
    INT ChunkLength, 
    INT ReadPtr
)
{
    INT ChunkReadPtr = ReadPtr;
    INT ChunkTag = GrabIntFromArray(Data,ChunkReadPtr);

    // make sure this polygon-mapping is for surfaces (indirected through the tags structure)
    if (ChunkTag != (INT)(('S' << 24) + ('U' << 16) + ('R' << 8) + 'F'))
        return;

    debugf( NAME_DevLoad, TEXT("Processing PTAG SURF triangle-surface mapping.") );
    INT mappingcount = (ChunkLength - 4)/4;

    for(INT j = 0; j < mappingcount; j++)
    {
        _WORD polyindex = GrabWordFromArray(Data,ChunkReadPtr);
        _WORD surfindex = GrabWordFromArray(Data,ChunkReadPtr);

        Triangles(polyindex).LegacyPolyFlags = PolyFlags(surfindex);
        Triangles(polyindex).LegacyMaterial = BaseMaterials(surfindex);
        
        // smoothing group analogy in Lightwave is to treat each surface as its own smoothing group (for now)
        if (surfindex < 32)
        {
            Triangles(polyindex).SmoothingMask = 1 << surfindex;
        }
        else
        {
            GWarn->Logf( NAME_Error, TEXT("Too many surfaces! Smoothing groups require less than 32 surface definitions!") );
        }

        // disable smoothing where requested
        if (PolyFlags(surfindex) & PF_NoSmooth)
            Triangles(polyindex).SmoothingMask = 0;
        
        PolySurfs(polyindex) = surfindex;
    }
}

void ProcessSurfaceProperties
(
    TArray<BYTE> &Data,
    TArray<FString> &SurfaceNames,
    TArray<FLOAT> &MaxSmoothingAngles,
    INT ChunkLength,
    INT ReadPtr
)
{
    INT ChunkReadPtr = ReadPtr;
    FLOAT SmoothingThreshold;
			
    // read the surface name    
    const TCHAR* Ch = GrabStringFromArray(Data,ChunkReadPtr);

    // skip the surface source name
    GrabStringFromArray(Data,ChunkReadPtr);
			
	// keep skipping subchunks until we encounter the smoothing threshold subchunk			
    INT SubChunkTag = 0;
    INT SubChunkLength = 0;
    do
	{
        SubChunkTag = GrabIntFromArray(Data,ChunkReadPtr);
        SubChunkLength = GrabWordFromArray(Data,ChunkReadPtr);

        if (SubChunkTag == (INT)(('S' << 24) + ('M' << 16) + ('A' << 8) + 'N'))
        {
            // we only need to create the smoothing angles if a maximum has been set in the LWO file for any surface
            // so this shouldn't be removed from this loop
            if (MaxSmoothingAngles.Num() == 0)
            {
                MaxSmoothingAngles.Add(SurfaceNames.Num());
        
                for(INT k = 0; k < SurfaceNames.Num(); k++)
                {
                    MaxSmoothingAngles(k) = appCos(89.0f*0.0174532925199433f);
                }
            }                    
            
            // read the smoothing threshold, it's in radians
            SmoothingThreshold = GrabFloatFromArray(Data,ChunkReadPtr);

            // attempt to find the matching surface from the TAGS (was used as index for smoothing groups)
            INT matchsurfindex = 0;
            INT k;
            for(k = 0; k < SurfaceNames.Num(); k++)
            {
                if (SurfaceNames(k) == Ch)
                {
                    matchsurfindex = k;
                    break;
                }
            }
            
            if (k == SurfaceNames.Num())
            {
                GWarn->Logf( NAME_Warning, TEXT("Found surface name with no matching TAGS entry!") );
            }
            else
            {
                MaxSmoothingAngles(matchsurfindex) = appCos(SmoothingThreshold);
            }
            
            break;
        }			
        
        ChunkReadPtr += SubChunkLength;
    }
    while(ChunkReadPtr < (ReadPtr+ChunkLength));
}			

UBOOL ProcessLayer
(
    TArray<BYTE> &Data,
    FVector *Pivot,
    UBOOL *IsMover,
    UBOOL *IsBrush,
    UBOOL *MergeCoplanars,
    INT ReadPtr
)
{
    UBOOL hidden = false;

    INT ChunkReadPtr = ReadPtr + 2;
    _WORD flags = GrabWordFromArray(Data,ChunkReadPtr);

    // is this layer supposed to be hidden?
    if (flags & 1)
    {
        debugf( NAME_DevLoad, TEXT("Ignoring hidden layer.") );
        hidden = true;
    }

    Pivot->X = GrabFloatFromArray(Data,ChunkReadPtr);
    Pivot->Z = GrabFloatFromArray(Data,ChunkReadPtr);
    Pivot->Y = -GrabFloatFromArray(Data,ChunkReadPtr);
            
    const TCHAR* Ch = GrabStringFromArray(Data,ChunkReadPtr);
    debugf( NAME_DevLoad, TEXT("Layer name = %s."), Ch );            
            
    // is this layer a mover?
    if (appStrnicmp(Ch,TEXT("mover"),5) == 0)
    {
        *IsMover = true;
        debugf( NAME_DevLoad, TEXT("Processing layer as a mover."));
    }
    else
    {
        *IsMover = false;
    }

    // is this layer a brush?
    if (appStrnicmp(Ch,TEXT("brush"),5) == 0)
    {
        *IsBrush = true;
        debugf( NAME_DevLoad, TEXT("Processing layer as the new builder brush."));

        if (appStrstr(Ch,TEXT(".merge")))
        {
            *MergeCoplanars = true;
            debugf( NAME_Log, TEXT("Merge faces requested."));
        }
        else
        {
            *MergeCoplanars = false;
        }
    }
    else
    {
        *IsBrush = false;
    }

    return hidden;
}


//=================================================================================================
// Static Mesh Actor & Mover Generation

UStaticMesh *GenerateStaticMesh
(
    TArray<FStaticMeshTriangle> &Triangles,
    TArray<INT> &PolySurfs,
    TArray<FStaticMeshVertex> &Vertices,
    TArray<FVector> &PriTextureCoords,
    TArray<FVector> &SecTextureCoords,
    TArray<FLOAT> &MaxSmoothingAngles,
    TArray<DWORD> &PolyFlags,
    UObject* InOuter, 
    FName InName, 
    ULevel *Level, 
    UClass* Class,
    UBOOL IsMover,
    INT *StaticMeshActorCount,
    INT *MoverActorCount,
    FVector Pivot
)
{
    UStaticMesh *LWOMesh = 0;
    TArray<FStaticMeshTriangle> CollisionTriangles;
    TArray<FStaticMeshTriangle> StaticMeshTriangles;
    
    // is there any data to process?
    if (!(Triangles.Num () > 0))
        return 0;

	TArray<FStaticMeshMaterial> MeshMaterials; // sjs - fill in this new material struct
    TArray<DWORD> MeshPolyFlags;

	for( int i=0; i<Triangles.Num(); i++ )
	{
		FStaticMeshMaterial meshMat(Triangles(i).LegacyMaterial);
		meshMat.EnableCollision = GBuildStaticMeshCollision;
		int j = 0;
		for( j=0; j<MeshMaterials.Num(); j++ )
		{
			if( MeshMaterials(j).Material == meshMat.Material && Triangles(i).LegacyPolyFlags == MeshPolyFlags(j))
			{
				break;
			}
		}
		if( j == MeshMaterials.Num() )
		{
            //debugf(TEXT("Added %s mesh material to %s"), meshMat.Material->GetName(), *InName );
			MeshMaterials.AddItem( meshMat );
            MeshPolyFlags.AddItem( Triangles(i).LegacyPolyFlags );
		}
		Triangles(i).MaterialIndex = j;

		if( Triangles(i).NumUVs < 0 || Triangles(i).NumUVs > 8 )
		{
			appErrorf(TEXT("Triangles(i).NumUVs invalid for %s!"), *InName);
		}
	}

    // remove any collision triangles before passing the data off for static mesh creation
    for(int i = 0; i < Triangles.Num(); i++)
    {
        if (PolyFlags(PolySurfs(i)) & PF_Semisolid)
        {
            CollisionTriangles.Add();
            CollisionTriangles(CollisionTriangles.Num()-1) = Triangles(i);
        }
        else
        {
            StaticMeshTriangles.Add();
            StaticMeshTriangles(StaticMeshTriangles.Num()-1) = Triangles(i);
        }
    }

    if (CollisionTriangles.Num() > 0)
        debugf(TEXT("Collision Triangle Count: %d"), CollisionTriangles.Num());
    
    LWOMesh = CreateStaticMesh(StaticMeshTriangles,MeshMaterials,InOuter,InName);

    // if there are any collision triangles, then create a collision model from them for this static mesh
    if (CollisionTriangles.Num() && GEditor)
    {
        LWOMesh->CollisionModel = new(LWOMesh->GetOuter()) UModel(NULL,1);
		
        for(int i = 0; i < CollisionTriangles.Num(); i++)
		{
			FPoly*	Poly = new(LWOMesh->CollisionModel->Polys->Element) FPoly();
			Poly->Init();
			Poly->Vertex[0] = CollisionTriangles(i).Vertices[0];
			Poly->Vertex[1] = CollisionTriangles(i).Vertices[2]; // sjs - jeff sucks!
			Poly->Vertex[2] = CollisionTriangles(i).Vertices[1]; // sjs - jeff sucks!
			Poly->iLink = INDEX_NONE;
			Poly->NumVertices = 3;
			Poly->CalcNormal(1);
            Poly->Finalize(0);
		}

		LWOMesh->CollisionModel->BuildBound();
	    //GEditor->bspMergeCoplanars( LWOMesh->CollisionModel, 0, 0 );
		GEditor->bspBuild(LWOMesh->CollisionModel,BSP_Good,15,70,1,0);
		GEditor->bspRefresh(LWOMesh->CollisionModel,1);
		GEditor->bspBuildBounds(LWOMesh->CollisionModel);
        LWOMesh->UseSimpleBoxCollision = 1;
	}
                
    #ifdef MSASUPPORT
    // copy maximum smoothing angles array
    LWOMesh->MaxSmoothingAngles = MaxSmoothingAngles;

    for( int i=0; i<MaxSmoothingAngles.Num(); i++ )
    {
        debugf(TEXT("Mesh %s smoothing[%d] = %f"), *InName, i, MaxSmoothingAngles(i));
    }
    LWOMesh->Modify();
    LWOMesh->Build();
    #endif    
    
    if (LWOMesh && (Level != NULL))
    {
        AActor* Actor = 0;

        if (IsMover)
        {
            Actor = Level->SpawnActor( AMover::StaticClass(), NAME_None, (Level->Brush()->Location + Pivot));
        }
        else
        {
            Actor = Level->SpawnActor( Class, NAME_None, (Level->Brush()->Location + Pivot) );
        }

        Actor->DrawType = DT_StaticMesh;
		Actor->StaticMesh = LWOMesh;
                    
        Actor->PostEditChange();
                    
        if (IsMover)
        {
            (*MoverActorCount)++;
        }
        else
        {            
            (*StaticMeshActorCount)++;
        }
    }

    debugf( NAME_Log, TEXT("Imported %i vertices, %i texture coordinates, %i triangles."),
        Vertices.Num (), PriTextureCoords.Num (), Triangles.Num ());

    Vertices.Empty();
    PriTextureCoords.Empty();
    SecTextureCoords.Empty();
    Triangles.Empty();
    PolySurfs.Empty();

    return LWOMesh;
}


//=================================================================================================
// Brush Actor Generation

void GenerateBrush
(
    TArray<FPoly> &Polygons,
    INT *BrushCount,
    UBOOL MergeCoplanars
)
{
    UModel* Brush = GEditor->Level ? GEditor->Level->Brush()->Brush : NULL;
	
    if (Brush == NULL)
    {
        GWarn->Logf( NAME_Error, TEXT("Couldn't create brush from layer.") );
        return;
    }
	
	Brush->Modify();
	GEditor->Level->Brush()->Modify();
	GEditor->Level->Brush()->Group = NAME_None;
	FRotator Temp(0.0f,0.0f,0.0f);
	GEditor->Constraints.Snap( NULL, GEditor->Level->Brush()->Location, FVector(0,0,0), Temp );
	FModelCoords TempCoords;
	GEditor->Level->Brush()->BuildCoords( &TempCoords, NULL );
	GEditor->Level->Brush()->Location -= GEditor->Level->Brush()->PrePivot.TransformVectorBy( TempCoords.PointXform );
	GEditor->Level->Brush()->PrePivot = FVector(0,0,0);
	
    Brush->Polys->Element.Empty();
	for(INT i = 0; i < Polygons.Num(); i++)
    {
        new(Brush->Polys->Element)FPoly(Polygons(i));
    }
    
    if (MergeCoplanars)
	{
		GEditor->bspMergeCoplanars( Brush, 0, 1 );
		GEditor->bspValidateBrush( Brush, 1, 1 );
	}
	
    Brush->Linked = 1;
	
    GEditor->bspValidateBrush( Brush, 0, 1 );
	
    Brush->BuildBound();
	
	GEditor->RedrawLevel( GEditor->Level );
	GEditor->NoteSelectionChange( GEditor->Level );

    (*BrushCount)++;
}


//=================================================================================================
// Format Verification

// insures input file is actually a LWO file
UBOOL VerifyLWOFile(TArray<BYTE> &Data, INT &ReadPtr)
{
    INT ChunkTag = GrabIntFromArray(Data,ReadPtr);

    if (ChunkTag != (INT)(('F' << 24) + ('O' << 16) + ('R' << 8) + 'M'))
    {
        GWarn->Logf( NAME_Error, TEXT("Lightwave file may be damaged or corrupt.") );
        return false;
    }
    
    INT ChunkLength = GrabIntFromArray(Data,ReadPtr);
    debugf( NAME_DevLoad, TEXT("Lightwave file size = %d."), ChunkLength+8 );
    
    ChunkTag = GrabIntFromArray(Data,ReadPtr);
    if (ChunkTag != (INT)(('L' << 24) + ('W' << 16) + ('O' << 8) + '2'))
    {
        GWarn->Logf( NAME_Error, TEXT("Lightwave file may be damaged or corrupt.") );
        return false;
    }

    return true;
}

//=================================================================================================
// File Importing

// parses and loads a Newtek Lightwave model file
static UStaticMesh *ParseLWO(TArray<BYTE> &Data, UObject* InOuter, FName InName, ULevel *Level, UClass* Class)
{
    // for file parsing
    INT ReadPtr = 0, CachedReadPtr;
    INT ChunkTag;
    INT ChunkLength;

    // for static mesh loading       
    TArray<FStaticMeshVertex> Vertices;
    TArray<FStaticMeshTriangle> Triangles;
    TArray<FVector> PriTextureCoords;
    TArray<FVector> SecTextureCoords;
    TArray<DWORD> PolyFlags;
    TArray<UMaterial*> BaseMaterials;    
    TArray<INT> PolySurfs;
    TArray<FString> SurfaceNames;
    TArray<FLOAT> MaxSmoothingAngles;
    
    FVector Pivot(0.f,0.f,0.f);
    UStaticMesh *LWOMesh = NULL;
    UBOOL HiddenLayer = false;
    INT StaticMeshActorCount = 0;       
    INT PolygonCount = 0;
    INT LayerCount = 0; 

    // for mover loading
    UBOOL IsMover = false;
    INT MoverActorCount = 0;

    // for brush loading
    TArray<FPoly> Polygons;
    UBOOL IsBrush = false;
    UBOOL MergeCoplanars = false;
    INT BrushCount = 0;

    // are we dealing with a valid Lightwave data file?
    if (!VerifyLWOFile(Data,ReadPtr)) 
        return NULL;
    
    // initialize a pointer to our default material to be used when specified textures can't be loaded
    UMaterial *DefaultMaterial = (Cast<UMaterial>(UMaterial::StaticClass()->GetDefaultObject())->DefaultMaterial);
    check(DefaultMaterial);    
   
    // reset global error counts
    GWarn->ErrorCount = 0;
    GWarn->WarningCount = 0;
    
    // before we start parsing in earnest, we need to grab the maximum smoothing angles and surface names first
    CachedReadPtr = ReadPtr;
    while(ReadPtr < Data.Num())
    {
        // read a chunk header
        ChunkTag = GrabIntFromArray(Data,ReadPtr);
        ChunkLength = GrabIntFromArray(Data,ReadPtr);        
        
        // process the chunk accordingly
        switch(ChunkTag)
        {
            case ((INT)(('T' << 24) + ('A' << 16) + ('G' << 8) + 'S')):        

                ProcessSurfaceNames(Data,PolyFlags,BaseMaterials,SurfaceNames,DefaultMaterial,ChunkLength,ReadPtr);
                break;

            case ((INT)(('S' << 24) + ('U' << 16) + ('R' << 8) + 'F')):
                
                ProcessSurfaceProperties(Data,SurfaceNames,MaxSmoothingAngles,ChunkLength,ReadPtr);
                break;
            
            default:

                // unrecognized chunk encountered...
                break;               
        }        
        
        // move to the next chunk
        ReadPtr += ChunkLength;    
    }
    ReadPtr = CachedReadPtr;
    
    // parse the LWO file data from the remainder of the array, ignoring unrecognized chunks
    while(ReadPtr < Data.Num())
    {
        // read a chunk header
        ChunkTag = GrabIntFromArray(Data,ReadPtr);
        ChunkLength = GrabIntFromArray(Data,ReadPtr);

        // display chunk name and chunk length
        debugf( NAME_DevLoad, TEXT("Chunk Name: %c%c%c%c ChunkLength: %d."), 
                ((char*)&ChunkTag)[3],
                ((char*)&ChunkTag)[2],
                ((char*)&ChunkTag)[1],
                ((char*)&ChunkTag)[0],
                ChunkLength );
        
        // process the chunk accordingly
        switch(ChunkTag)
        {
            case ((INT)(('P' << 24) + ('N' << 16) + ('T' << 8) + 'S')):

                // if the current layer is 'hidden' then ignore this chunk
                if (HiddenLayer)
                    break;
                
                ProcessVertexChunk(Data,Vertices,&Pivot,ChunkLength,ReadPtr);
                break;

            case ((INT)(('V' << 24) + ('M' << 16) + ('A' << 8) + 'P')):
                
                // if the current layer is 'hidden', or we are loading the new builder brush, then ignore this chunk
                if (HiddenLayer)
                    break;

                ProcessVertexMappingChunk(Data,Vertices,PriTextureCoords,SecTextureCoords,ChunkLength,ReadPtr);
                break;

            case ((INT)(('P' << 24) + ('O' << 16) + ('L' << 8) + 'S')):
                
                // if the current layer is 'hidden' then ignore this chunk
                if (HiddenLayer)
                    break;

                if (IsBrush)
                {
                    PolygonCount = ProcessPolygonChunk(Data,Vertices,PriTextureCoords,Polygons,ChunkLength,ReadPtr);
                }
                else                
                {
                    PolygonCount = ProcessTrianglesOnlyChunk(Data,Vertices,PriTextureCoords,SecTextureCoords,Triangles,PolySurfs,DefaultMaterial,ChunkLength,ReadPtr);
                }
                
                break;

            case ((INT)(('V' << 24) + ('M' << 16) + ('A' << 8) + 'D')):

                // if the current layer is 'hidden' then ignore this chunk
                if (HiddenLayer)
                    break;

                if (IsBrush)
                {
                    ProcessDiscontinuousPolygonVertexMappingChunk(Data,Vertices,Polygons,ChunkLength,ReadPtr);
                }
                else
                {
                    ProcessDiscontinuousTriangleVertexMappingChunk(Data,Vertices,Triangles,ChunkLength,ReadPtr);
                }
                
                break;

            case ((INT)(('T' << 24) + ('A' << 16) + ('G' << 8) + 'S')):        

                // done in first pass above: ProcessSurfaceNames(Data,PolyFlags,BaseMaterials,SurfaceNames,DefaultMaterial,ChunkLength,ReadPtr);
                break;

            case ((INT)(('P' << 24) + ('T' << 16) + ('A' << 8) + 'G')):

                // if the current layer is 'hidden', or we are loading the new builder brush, then ignore this chunk
                if (HiddenLayer)
                    break;
    
                if (IsBrush)
                {
                    ProcessPolygonSurfaces(Data,Polygons,PolyFlags,BaseMaterials,ChunkLength,ReadPtr);
                }                
                else
                {
                    ProcessTriangleSurfaces(Data,Triangles,PolyFlags,BaseMaterials,PolySurfs,ChunkLength,ReadPtr);
                }
                
                break;

            case ((INT)(('S' << 24) + ('U' << 16) + ('R' << 8) + 'F')):
                
                // done in first pass above: ProcessSurfaceProperties(Data,SurfaceNames,MaxSmoothingAngles,ChunkLength,ReadPtr);
                break;

            case ((INT)(('L' << 24) + ('A' << 16) + ('Y' << 8) + 'R')):

                // generate a static mesh actor or brush from the last layer's data
                if (IsBrush)
                {
                    GenerateBrush(Polygons,&BrushCount,MergeCoplanars);
                }
                else
                {
                    LWOMesh = GenerateStaticMesh(Triangles,PolySurfs,Vertices,PriTextureCoords,SecTextureCoords,MaxSmoothingAngles,PolyFlags,
                                                 InOuter,InName,Level,Class,IsMover,&StaticMeshActorCount,&MoverActorCount,Pivot);
                }

                // process layer information
                HiddenLayer = ProcessLayer(Data,&Pivot,&IsMover,&IsBrush,&MergeCoplanars,ReadPtr);
                
                // increment layer count
                LayerCount++;
                
                break;
 
            default:

                // unrecognized chunk encountered...
                break;               
        }        
        
        // move to the next chunk
        ReadPtr += ChunkLength;
    }    
    
    // generate a static mesh actor or brush from the last layer's data    
    if (IsBrush)
    {
        GenerateBrush(Polygons,&BrushCount,MergeCoplanars);
    }
    else
    {
        LWOMesh = GenerateStaticMesh(Triangles,PolySurfs,Vertices,PriTextureCoords,SecTextureCoords,MaxSmoothingAngles,PolyFlags,
                                     InOuter,InName,Level,Class,IsMover,&StaticMeshActorCount,&MoverActorCount,Pivot);
    }
    
    // empty remaining TArrays
    PolyFlags.Empty();
    BaseMaterials.Empty();    
    SurfaceNames.Empty();
        
    // log how many static mesh actors were imported from the LWO file, if any (if outside of UnrealEd, and
    // importing from script, this will be zero)
    debugf( NAME_Log, TEXT("Imported %i Static Mesh Actors."), StaticMeshActorCount);
    debugf( NAME_Log, TEXT("Imported %i Mover Actors."), MoverActorCount);
    debugf( NAME_Log, TEXT("Imported %i Brushes."), BrushCount);

    if ( LWOMesh )
    {
        debugf( NAME_Log, TEXT("Analyzing StaticMesh:") );
        LWOMesh->CheckForErrors();
    }
    
    if (BrushCount)
    {
        GImportedBrush = true;
    }
    
    if( GWarn->ErrorCount )
		debugf( NAME_Log, TEXT("LWO Import Failed - %d error(s), %d warning(s)"), GWarn->ErrorCount, GWarn->WarningCount );
    else
		debugf( NAME_Log, TEXT("LWO Import Successful - %d error(s), %d warning(s)"), GWarn->ErrorCount, GWarn->WarningCount );
    
    // return a pointer to the last (and possibly only or even none) static mesh generated from the import process
    return LWOMesh;
}

UStaticMesh *CreateStaticMeshFromLWO(UObject* InOuter, FName InName, const FString &InFileName, ULevel *Level, UClass* Class)
{
    guard(CreateStaticMeshFromLWO);
	GWarn->BeginSlowTask(TEXT("Loading LWO file..."),1);
    
    UStaticMesh  *LWOMesh = NULL;
    TArray<BYTE>  LWOData;

    if( !appLoadFileToArray( LWOData, *InFileName ) )
    {
        GWarn->Logf( NAME_Error, TEXT("Could not open %s"), *InFileName );
	    GWarn->EndSlowTask();
        return NULL;
    }

    GImportedBrush = false;

    // If the staticmesh already exists, save its collision model!
    UStaticMesh* PrevStaticMesh = Cast<UStaticMesh>(UObject::StaticFindObject( UStaticMesh::StaticClass(), InOuter, *InName ));
    UModel* PrevUModel = PrevStaticMesh ? PrevStaticMesh->CollisionModel : NULL;

    if ((LWOMesh = ParseLWO( LWOData, InOuter, InName, Level, Class )) == NULL )
    {
        if (!GImportedBrush)
            GWarn->Logf( NAME_Error, TEXT("Error reading from %s."), *InFileName );
    }

    if( LWOMesh && !LWOMesh->CollisionModel && PrevUModel )
	{
        LWOMesh->CollisionModel = PrevUModel;
        debugf( NAME_Log, TEXT("Assigned previous collision model to newly imported mesh: %s"), LWOMesh->GetFullName() );
	}

    if( GIsRunning )
    {
        if( GWarn->ErrorCount )
            appMsgf( 0, TEXT("LWO Import Failed - %d error(s), %d warning(s)! Fix the file in Lightwave and re-import!\n\nPossible causes:\n1. Non-tripled input data for static meshes.\n2. Misspelled texture names.\n3. Check the log for help!"), GWarn->ErrorCount, GWarn->WarningCount );
        else if( GWarn->WarningCount )
            appMsgf( 0, TEXT("LWO Import Successful - %d error(s), %d warning(s)! Fix the file in Lightwave and re-import!\n\nPossible causes:\n1. Non-tripled input data for static meshes.\n2. Misspelled texture names.\n3. Check the log for help!"), GWarn->ErrorCount, GWarn->WarningCount );
    }

    GWarn->EndSlowTask();
    return LWOMesh;
    unguard;
}

