//=============================================================================
// AxProcMesh - implementation
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#include "EnginePrivate.h"
#include "UnRenDev.h"
#include <math.h>

#if MACOSX  // !!! FIXME: Is there a better way to do this?  --ryan.
#define sinf sin
#define asinf asin
#define cosf cos
#define acosf acos
#endif

const DWORD ProcMesh_VF_Flags = VF_Position | VF_Normal | VF_Diffuse | VF_Tex1;
const float RIGID_WEIGHT = -1.0f;
const float StandardWeight = 4.0f;

IMPLEMENT_CLASS(AxProcMesh);

struct FProcMeshFace
{
    _WORD   verts[3];
};

struct FProcMeshLink
{
    _WORD   v0;
    _WORD   v1;
    float   weight;
};

struct FProcMeshShared
{
    _WORD   v0;
    _WORD   v1;
};

struct FProcVertData
{
    float   offset;
    float   force;
    float   invWtSum;
    float   wt;
    FVector restPosition;
    FVector restNormal;
};

struct MeshData
{
    TArray<FProcVertData>   vertData;
    TArray<FProcMeshFace>   faces;
    TArray<FProcMeshLink>   links;
    TArray<FProcMeshShared> shares; // verts who live on a texture seam, but are the same for iteration
    // cached values, changes in these require recomputing mesh data
    UStaticMesh*            StaticMesh;
    UBOOL                   bRigidEdges;
};

// Create refraction table - from d3d sdk
struct WaterRefract
{
    // Vrefract = (V + refract * N) * norm
    float fRefract;
    float fRefractNorm; 
    DWORD dwDiffuse;
};

#if defined(_X86) && !defined(_WIN64)
inline static int f2i(float flt) 
{
	volatile int n; 

	__asm 
	{
		fld flt
		fistp n
	}

	return n;
}
#else
inline static int f2i(float flt) 
{
    // !!! FIXME: There's like 18 million functions that do this!
    // !!! FIXME:  Grep for "FloatToInt".  --ryan.
	return (int) flt;
}
#endif

WaterRefract RefractTable[512];
bool         RefractInitialized = false;

static void InitRefraction()
{
    if ( RefractInitialized )
        return;
    RefractInitialized = true;

    for(DWORD u = 0; u < 256; u++)
    {        
        float fCos0 = (float) u / (float) 256.0f;
        float f0 = acosf(fCos0);
        float fSin0 = sinf(f0);

        float fSin1 = fSin0 / 1.333f; // water
        float f1 = asinf(fSin1);
        float fCos1 = cosf(f1);
    
        RefractTable[u].fRefract = fSin0 / fSin1 * fCos1 - fCos0;
        RefractTable[u].fRefractNorm = - fSin1 / fSin0;
        RefractTable[u].dwDiffuse = ((((0xff - u)*(0xff - u)*(0xff - u)) << 8) & 0xff000000);
        RefractTable[u+256] = RefractTable[u];
    }
}

static bool AddUniqueLink(TArray<FProcMeshLink>& linkData, _WORD v0, _WORD v1, float wt)
{
    for ( int i=0; i<linkData.Num(); i++ )
    {
        if ( linkData(i).v0 == v0 && linkData(i).v1 == v1 )
        {
            return false;
        }
        if ( linkData(i).v1 == v0 && linkData(i).v0 == v1 )
        {
            return false;
        }
    }

    FProcMeshLink l;
    l.v0 = v0;
    l.v1 = v1;
    l.weight = wt;
    linkData.AddItem(l);
	return true;
}

// !! - assumes Vertex normals are zeroed for accumulation here
static void CalcProcNormals( TArray<FProcMeshVertex>& Vertices, TArray<FProcMeshFace>& faces )
{
    int numFaces = faces.Num();
    FProcMeshFace* pFace = &faces(0);
    FVector faceNorm;
    for( int i=0; i<numFaces; i++ )
    {
        FProcMeshVertex* pV0 = &Vertices( pFace->verts[0] );
        FProcMeshVertex* pV1 = &Vertices( pFace->verts[1] );
        FProcMeshVertex* pV2 = &Vertices( pFace->verts[2] );
        faceNorm = (pV0->Position - pV1->Position) ^ (pV2->Position - pV1->Position);
        pV0->Normal += faceNorm;
        pV1->Normal += faceNorm;
        pV2->Normal += faceNorm;
        pFace++;
    }
}

static int CountEdges( TArray<FProcMeshFace>& faces, _WORD v0, _WORD v1 )
{
    int edges = 0;
    for( int i=0; i<faces.Num(); i++ )
    {
        for( int j=0; j<3; j++ )
        {
            if ( faces(i).verts[j] == v0 && faces(i).verts[(j+1)%3] == v1 )
            {
                edges++;
                break;
            }
            else if ( faces(i).verts[j] == v1 && faces(i).verts[(j+1)%3] == v0 )
            {
                edges++;
                break;
            }
        }
    }
    return edges;
}

//
//	CalculateLighting
//

static void CalculateLighting(UStaticMesh* StaticMesh,UStaticMeshInstance* StaticMeshInstance,AActor* Owner)
{
	guard(CalculateLighting);

	// Update the static lighting stream.

    FMatrix LocalToWorld = Owner->LocalToWorld();

	appMemzero(&StaticMeshInstance->ColorStream.Colors(0),StaticMeshInstance->ColorStream.Colors.Num() * sizeof(FColor));
	StaticMeshInstance->ColorStream.Revision++;

	for(INT LightIndex = 0;LightIndex < StaticMeshInstance->Lights.Num();LightIndex++)
	{
		FStaticMeshLightInfo*	LightInfo = &StaticMeshInstance->Lights(LightIndex);
		FDynamicLight*			Light = LightInfo->LightActor->GetLightRenderData();

		if(Light && !Light->Changed && !Light->Dynamic)
		{
			FStaticMeshVertex*	VertexPtr = &StaticMesh->VertexStream.Vertices(0);
			FColor*				ColorPtr = &StaticMeshInstance->ColorStream.Colors(0);
			BYTE*				BitPtr = LightInfo->VisibilityBits.Num() ? &LightInfo->VisibilityBits(0) : NULL;
			BYTE				BitMask = 1;

			for(INT VertexIndex = 0;VertexIndex < StaticMesh->VertexStream.Vertices.Num();VertexIndex++)
			{
				FVector	SamplePoint = LocalToWorld.TransformFVector(VertexPtr->Position),
						SampleNormal = LocalToWorld.TransformNormal(VertexPtr->Normal).SafeNormal();

				if(*BitPtr & BitMask)
					*ColorPtr += FColor(
									0.5f * Light->Color * Light->SampleIntensity(
															SamplePoint,
															SampleNormal
															)
									);

				ColorPtr++;
				VertexPtr++;
				BitMask <<= 1;

				if(!BitMask)
				{
					BitPtr++;
					BitMask = 1;
				}
			}

			LightInfo->Applied = 1;
		}
		else
			LightInfo->Applied = 0;
	}

    for( int i=0; i<((AxProcMesh*)Owner)->Vertices.Num(); i++ )
    {
        if( i >= StaticMeshInstance->ColorStream.Colors.Num()-1 )
            break;
        ((AxProcMesh*)Owner)->Vertices(i).Color = StaticMeshInstance->ColorStream.Colors(i);
    }

#if 0
	// Blend the vertex color/alpha into the color stream.
	FColor*	SrcPtr = &StaticMesh->ColorStream.Colors(0);
	FColor*	DestPtr = &StaticMeshInstance->ColorStream.Colors(0);
	for(INT VertexIndex = 0;VertexIndex < StaticMesh->VertexStream.Vertices.Num();VertexIndex++)
	{
		if(StaticMesh->UseVertexColor)
		{
			DestPtr->R = (BYTE) ((INT)SrcPtr->R * (INT)DestPtr->R / (INT)255);
			DestPtr->G = (BYTE) ((INT)SrcPtr->G * (INT)DestPtr->G / (INT)255);
			DestPtr->B = (BYTE) ((INT)SrcPtr->B * (INT)DestPtr->B / (INT)255);
		}

		DestPtr->A = SrcPtr->A;
		SrcPtr++;
		DestPtr++;
	}
#endif
	unguard;
}

void AxProcMesh::execProcPling( FFrame& Stack, RESULT_DECL )
{
    guard(AxProcMesh::execProcPling);
	P_GET_VECTOR(Position);
	P_GET_FLOAT(Strength);
	P_GET_FLOAT(Radius);
    P_GET_VECTOR_REF(EffectLocation);
    P_GET_VECTOR_REF(EffectNormal);
	P_FINISH;

    *EffectNormal = FVector(0,0,1);
    *EffectLocation = Position;

    MeshData* pData = (MeshData*)pProcData;
    if( !pData )
        return;
  
    Position = WorldToLocal().TransformFVector(Position);
    float radiusSqr = Square(Radius);
    float invRadiusSqr = 1.0f / radiusSqr;

    FProcMeshVertex* pVert = &Vertices(0);
    FProcVertData* pVData = &pData->vertData(0);
    int numVerts = Vertices.Num();
    int k;

    if( Radius == 0.0f )
    {
        float best = 9999999.9f;
        int bestIndex = 0;
        for ( k=0; k<numVerts; k++ )
        {
            float distSqr = (Position - pVert->Position).SizeSquared();
            if ( distSqr < best )
            {
                best = distSqr;
                bestIndex = k;

            }
            pVert++;
            pVData++;
        }
        pData->vertData(bestIndex).offset += (-Strength * ForceAttenuation);
    }
    else
    {
	    for ( k=0; k<numVerts; k++ )
        {
            float distSqr = (Position - pVert->Position).SizeSquared();
            if ( distSqr < radiusSqr )
            {
                pVData->offset += (-Strength * ForceAttenuation * distSqr * invRadiusSqr);
            }
            pVert++;
            pVData++;
        }
    }

    unguard;
}

static inline void FetchTextureColor( FPlane& r, INT USize, INT VSize, float u, float v, BYTE* pBitmap, FColor* pColors )
{
	int SrcXIndex = Clamp( (int)(u * USize), 0, USize-1 );
	int SrcYIndex = Clamp( (int)(v * VSize), 0, VSize-1 );
	BYTE b = (BYTE)pBitmap[ SrcXIndex + SrcYIndex * USize ];
	r = (*(FColor*)(&pColors[ b ])).Plane();
}

void AxProcMesh::CalcMeshData(void)
{
	guard(AxProcMesh::CalcMeshData);
    NoiseCounter = 0.0;

	if ( !StaticMesh )
    {
        Vertices.Empty();
        if( pProcData )
        {
            MeshData* pData = (MeshData*)pProcData;
            pData->vertData.Empty();
            pData->faces.Empty();
            pData->links.Empty();
            pData->StaticMesh = NULL;
        }
		return;
    }

    if ( pProcData )
    {
        MeshData* pData = (MeshData*)pProcData;
        if ( pData->StaticMesh == StaticMesh && pData->bRigidEdges == bRigidEdges )
        {
            return;
        }
    }

    if ( !pProcData )
        pProcData = (PTRINT)(new MeshData);

    MeshData* pData = (MeshData*)pProcData;
    pData->StaticMesh = StaticMesh;
    pData->bRigidEdges = bRigidEdges;

    InitRefraction();

    int i=0;
    int j=0;

    Vertices.Empty();
    pData->vertData.Empty();
    pData->faces.Empty();
    pData->links.Empty();
    NoiseCounter = 0.0;

    if(StaticMeshInstance)
	{
		// Determine whether the static lighting stream needs to be updated.

		UBOOL	UpdateStaticLighting = 0;

		for(INT LightIndex = 0;LightIndex < StaticMeshInstance->Lights.Num();LightIndex++)
		{
			FStaticMeshLightInfo*	LightInfo = &StaticMeshInstance->Lights(LightIndex);
			FDynamicLight*			Light = LightInfo->LightActor->GetLightRenderData();

			if((!Light || Light->Dynamic || Light->Changed) == LightInfo->Applied)
			{
				UpdateStaticLighting = 1;
				break;
			}
		}

		// Update the static lighting.

		if(UpdateStaticLighting)
			CalculateLighting(StaticMesh,StaticMeshInstance,this);
	}

	Vertices.AddZeroed(StaticMesh->VertexStream.Vertices.Num());
	pData->vertData.AddZeroed(StaticMesh->VertexStream.Vertices.Num());
	for( i=0; i<StaticMesh->VertexStream.Vertices.Num(); i++ )
	{
		Vertices(i).Position = StaticMesh->VertexStream.Vertices(i).Position;
        Vertices(i).Normal = FVector(0,0,0);// this is accumulated in CalcProcNormals
        Vertices(i).Color = FColor(255,255,255,255);
        if( StaticMeshInstance && StaticMeshInstance->ColorStream.Colors.Num()>i )
            Vertices(i).Color = StaticMeshInstance->ColorStream.Colors(i);
        Vertices(i).U = StaticMesh->UVStreams(0).UVs(i).U;
        Vertices(i).V = StaticMesh->UVStreams(0).UVs(i).V;
        pData->vertData(i).force = 0.0f;
        pData->vertData(i).offset = 0.0f;
        pData->vertData(i).wt = 1.0f;
        pData->vertData(i).restPosition = Vertices(i).Position;
        pData->vertData(i).restNormal = StaticMesh->VertexStream.Vertices(i).Normal;
        if ( qFRand() <= Noise ) // initial noise
            pData->vertData(i).force += (NoiseForce.Min + qFRand() * (NoiseForce.Min - NoiseForce.Max)); // initial force for noise
        pData->vertData(i).invWtSum = 0.0f;
	}

    for( i=0; i<Vertices.Num(); i++ )
	{
		pData->vertData(i).invWtSum = 1.0f;//StandardWeight;
	}

    // look for ident verts
    pData->shares.Empty();
    for( i=0; i<Vertices.Num(); i++ )
	{
        for( int j=i+1; j<Vertices.Num(); j++ )
        {
            if( Vertices(i).Position == Vertices(j).Position &&
                pData->vertData(i).restNormal == pData->vertData(j).restNormal )
            {
                FProcMeshShared s;
                s.v0 = i;
                s.v1 = j;
                pData->shares.AddItem(s);

                pData->vertData(i).invWtSum = RIGID_WEIGHT;
            }
        }
    }
    debugf(TEXT("ProcMesh %d shared verts."), pData->shares.Num() );

	for( i=0; i<StaticMesh->Sections.Num(); i++ )
    {
		int offset = pData->faces.Num();
		pData->faces.AddZeroed( StaticMesh->Sections(i).NumTriangles );
		_WORD* pIndices = &StaticMesh->IndexBuffer.Indices(StaticMesh->Sections(i).FirstIndex);
        if( StaticMesh->IndexBuffer.Indices.Num() == 0 )
            break;
		for( j=0; j<StaticMesh->Sections(i).NumPrimitives; j++ )
        {
            pData->faces(offset+j).verts[0] = pIndices[0];
            pData->faces(offset+j).verts[1] = pIndices[1];
            pData->faces(offset+j).verts[2] = pIndices[2];
			pIndices += 3;
        }
	}

    if( bRigidEdges )
    {
	    int numRigidVerts = 0;
	    for( i=0; i<pData->faces.Num(); i++ )
	    {
		    for( int j=0; j<3; j++ )
		    {
			    if ( CountEdges( pData->faces, pData->faces(i).verts[j], pData->faces(i).verts[(j+1)%3] ) == 1 )
			    {
				    numRigidVerts++;
				    pData->vertData(pData->faces(i).verts[j]).invWtSum = RIGID_WEIGHT;
				    pData->vertData(pData->faces(i).verts[(j+1)%3]).invWtSum = RIGID_WEIGHT;
			    }
		    }
	    }
	    debugf(TEXT("xProcMesh numRigidVerts=%d"), numRigidVerts );
    }

    if( 0 )
    {
        float UsedRadiusSqr = Square(InfluenceRadius);
        // guess a good influence radius! simply using triangulation fails in some cases
        if( UsedRadiusSqr == 0.0f ) 
        {
            float longestEdge = 0.0f;
            int numSamples = pData->faces.Num() / 10;
            for( i=0; i<numSamples; i++ )
            {
                int randFace = appRand() % pData->faces.Num();
                for( int j=0; j<3; j++ )
                {
                    _WORD v0 = pData->faces(randFace).verts[j];
                    _WORD v1 = pData->faces(randFace).verts[(j+1)%3];

                    float edgeLen = (Vertices(v0).Position - Vertices(v1).Position).Size();
                    if( edgeLen > longestEdge )
                        longestEdge = edgeLen;
                }
            }
            longestEdge *= 1.2f;
            UsedRadiusSqr = Square(longestEdge);
        }

        for( i=0; i<Vertices.Num(); i++ )
        {
            float wt = 1.0f;
            for( int j=i+1; j<Vertices.Num(); j++ )
            {
                float dot = pData->vertData(i).restNormal | pData->vertData(j).restNormal;
                if( dot <= 0.001f )
                    continue;

                float distSqr = (Vertices(i).Position - Vertices(j).Position).SizeSquared();
                if( distSqr < UsedRadiusSqr )
                {
                    wt = dot * distSqr / UsedRadiusSqr;
                    if( AddUniqueLink(pData->links, i, j, wt) )
			        {
				        if( pData->vertData(i).invWtSum != RIGID_WEIGHT )
					        pData->vertData(i).invWtSum += wt;
				        if( pData->vertData(j).invWtSum != RIGID_WEIGHT )
					        pData->vertData(j).invWtSum += wt;
			        }
                }
            }
        }
    }
    else
    {
        // use trianglation for influences
        for( i=0; i<pData->faces.Num(); i++ )
	    {
		    for( int j=0; j<3; j++ )
		    {
			    float wt = 1.0f;

			    if( pData->vertData(pData->faces(i).verts[j]).invWtSum == RIGID_WEIGHT && pData->vertData(pData->faces(i).verts[(j+1)%3]).invWtSum == RIGID_WEIGHT )
				    continue;

                _WORD v0 = pData->faces(i).verts[j];
                _WORD v1 = pData->faces(i).verts[(j+1)%3];

			    if( AddUniqueLink(pData->links, v0, v1, wt) )
			    {
				    if( pData->vertData(v0).invWtSum != RIGID_WEIGHT )
					    pData->vertData(v0).invWtSum += wt;
				    if( pData->vertData(v1).invWtSum != RIGID_WEIGHT )
					    pData->vertData(v1).invWtSum += wt;
			    }
		    }
	    }
    }

	for( i=0; i<Vertices.Num(); i++ )
	{
        if( pData->vertData(i).invWtSum != RIGID_WEIGHT )
			pData->vertData(i).invWtSum = 1.0f / pData->vertData(i).invWtSum;
	}

    // use weight map
    UTexture* weightMap = Cast<UTexture>(UV2Texture);
    if( weightMap && weightMap->Format == TEXF_P8 )
    {
		BYTE*	Data		= NULL;
		INT		MipLevel	= 0,
				USize		= 0,
				VSize		= 0;

		while( !Data && (MipLevel < weightMap->Mips.Num()) )
		{
			weightMap->Mips(MipLevel).DataArray.Load();
			Data = (BYTE*) &weightMap->Mips(MipLevel).DataArray(0);
			USize = weightMap->Mips(MipLevel).USize;
			VSize = weightMap->Mips(MipLevel).VSize;
			MipLevel++;
		}
		check(Data);

        FPlane clr;
        for( i=0; i<Vertices.Num(); i++ )
	    {
            float tu = Vertices(i).U;
            float tv = Vertices(i).V;
            if( 0 )//StaticMesh->UVStreams.Num() > 1 && StaticMesh->UVStreams(1).UVs.Num() == Vertices.Num() )
            {
                tu = StaticMesh->UVStreams(1).UVs(i).U;
                tv = StaticMesh->UVStreams(1).UVs(i).V;
            }
            FetchTextureColor( clr, USize, VSize, tu, tv, Data, &weightMap->Palette->Colors(0) );
            if( (clr.X + clr.Y + clr.Z) < 0.1f )
            {
                pData->vertData(i).invWtSum = RIGID_WEIGHT;
            }
            else
            {
                pData->vertData(i).wt = clr.X;
                //pData->vertData(i).invWtSum *= clr.X;
            }
        }
        //debugf( NAME_Warning, TEXT("P8 Weightmap integrated into forces!"));
	}

	//debugf(TEXT("pData->links count: %d"), pData->links.Num());

    CalcProcNormals( Vertices, pData->faces );
	unguard;
}

UBOOL AxProcMesh::Tick( FLOAT DeltaTime, enum ELevelTick TickType )
{
	guard(AxProcMesh::Tick);
    int		i;
	bool	recalcNormals = false;

    if ( !pProcData || Level->TimeSeconds - LastRenderTime > 1 )
        return Super::Tick( DeltaTime, TickType );

    INT SetupStartTime = appCycles();

    MeshData* pData = (MeshData*)pProcData;

    //float t = Clamp( DeltaTime, 0.016f, 0.03f );
    float t = DeltaTime;

    NoiseCounter -= t;
    if ( NoiseCounter <= 0.0f && Noise > 0.0f )
    {
		int numRands = Noise * Vertices.Num();
        NoiseCounter = NoiseTimer.Min + qFRand() * (NoiseTimer.Min - NoiseTimer.Max);
        for( i=0; i<numRands; i++ )
        {
			int vert = qRand() % Vertices.Num();
            pData->vertData(vert).force += (NoiseForce.Min + qFRand() * (NoiseForce.Min - NoiseForce.Max)) * pData->vertData(vert).wt;
        }
    }

    for( i=0; i<Touching.Num(); i++ )
    {
        AActor* pActor = Touching(i);
        if( !pActor || pActor->bDeleteMe )
            continue;

        FBox bbox = pActor->GetPrimitive()->GetCollisionBoundingBox( pActor );
        bbox = bbox.TransformBy( WorldToLocal() );
        //bbox = bbox.ExpandBy(0.1f);
        FSphere BoundingSphere = FSphere(&bbox.Min,2);
        float radiusSqr = Square(BoundingSphere.W);
        float invRadiusSqr = 1.0f / radiusSqr;

        FProcMeshVertex* pVert = &Vertices(0);
        FProcVertData* pVData = &pData->vertData(0);
        FVector actorVelocity = pActor->Velocity;

        actorVelocity.Z -= actorVelocity.Size() * 0.2f; // argh
        
        int numVerts = Vertices.Num();
		for ( int k=0; k<numVerts; k++ )
        {
            float distSqr = (BoundingSphere - pVert->Position).SizeSquared();
            if ( distSqr < radiusSqr )
            {
				recalcNormals = true;
                FVector force =  actorVelocity * (ForceAttenuation * TouchStrength * (distSqr * invRadiusSqr));
                pVData->force += Clamp( (pVData->restNormal | force), -TouchStrength, TouchStrength ) * pVData->wt;
            }
            pVert++;
            pVData++;
        }
    }

    // update pData->links - !! this assume that pData->vertData(i).desire == Vertices(i).Position before entering
    int numLinks = pData->links.Num();
    FProcMeshLink* pLink = &pData->links(0);
    for (i=0; i<numLinks; i++)
    {
        FProcVertData* pVData0 = &pData->vertData(pLink->v0);
        FProcVertData* pVData1 = &pData->vertData(pLink->v1);
        pVData0->force += (pVData1->offset - pVData0->offset) * pVData0->invWtSum;
        pVData1->force += (pVData0->offset - pVData1->offset) * pVData1->invWtSum;
        pLink++;
    }

	if ( ProcType == MT_Water )
	{
        float maxStep = 1.0f/40.0f;
        float secs = Clamp(DeltaTime, 0.001f, 0.1f);
        while( secs > 0.0001f )
        {
            t = secs;
            if( t > maxStep)
            {
                t = maxStep;
            }
            secs -= t;
		    recalcNormals = true;
		    // calc desired acceleration based on tension in mesh, update velocity
            FProcMeshVertex* pVert = &Vertices(0);
            FProcVertData* pVData = &pData->vertData(0);
            int numVerts = Vertices.Num();
		    for ( i=0; i<numVerts; i++ )
		    {
                if ( pVData->invWtSum != RIGID_WEIGHT )
                {
                    pVData->force += -pVData->offset * t * RestTension * pVData->wt;
                    pVData->force *= 1.0f - (t * Dampening);
                    pVData->offset += pVData->force * t * Tension;

                    // update
                    pVData->offset = Clamp( pVData->offset, MovementClamp.Min, MovementClamp.Max );
			        pVert->Position = pVData->restPosition + (pVData->restNormal * pVData->offset);
                }

			    // since we're here, lets prepare the vertex normal for averaging in CalcProcNormals
			    pVert->Normal = FVector(0,0,0);

                pVert++;
                pVData++;
		    }
        }
	}

	if ( ProcType == MT_Deform && recalcNormals==true )
	{
        /*
		// collision occured so lets move verts
        FProcMeshVertex* pVert = &Vertices(0);
        FProcVertData* pVData = &pData->vertData(0);
        int numVerts = Vertices.Num();
		for ( i=0; i<numVerts; i++ )
		{    
            float inplaneVel = (pVData->velocity | pVData->restNormal) * t;
			pVert->Position += pVData->restNormal * inplaneVel;
			pVData->velocity = FVector(0,0,0);
            float planeDist = FPointPlaneDist(pVert->Position, pVData->restPosition, pVData->restNormal);
			if( planeDist > MovementClamp.Max )
			{
				pVert->Position = pVData->restPosition + (pVData->restNormal * MovementClamp.Max);
			}
            else if ( planeDist < MovementClamp.Min )
			{
				pVert->Position = pVData->restPosition + (pVData->restNormal * MovementClamp.Min);
			}
            pVert++;
            pVData++;
		}*/
	}

	if ( recalcNormals )
    {
		CalcProcNormals( Vertices, pData->faces );
        FProcMeshShared* pShare = &pData->shares(0);
        int numShare = pData->shares.Num();
        for( int i=0; i<numShare; i++ )
        {
            Vertices(pShare->v1).Position = Vertices(pShare->v0).Position;
            Vertices(pShare->v1).Normal = Vertices(pShare->v0).Normal;
            pShare++;
        }
    }

    GStats.DWORDStats( GEngineStats.STATS_Particle_Particles ) += Vertices.Num();
    GStats.DWORDStats( GEngineStats.STATS_Particle_SpriteSetupCycles ) += appCycles() - SetupStartTime;
    

	return Super::Tick( DeltaTime, TickType );
	unguard;
}

void AxProcMesh::Spawned()
{
	guard(AxProcMesh::Spawned);
	Super::Spawned();
	CalcMeshData();
	unguard;
}

void AxProcMesh::PostLoad()
{
	guard(AxProcMesh);
	Super::PostLoad();
	CalcMeshData();	
	unguard;
}

void AxProcMesh::Destroy()
{
	guard(AxProcMesh);
    delete (MeshData*)pProcData;
	Super::Destroy();
	unguard;
}

void AxProcMesh::PostEditChange()
{
	guard(AxProcMesh::PostEditChange);
	Super::PostEditChange();
	CalcMeshData();
	unguard;
}


class FProcVertStream : public FVertexStream
{
public:
	QWORD		    CacheId;
    int             Revision;
	AxProcMesh*		pProcMesh;

    void Init(AxProcMesh* inProcMesh)
    {
		pProcMesh = inProcMesh;
        Revision++;
    }

	FProcVertStream()
	{
		pProcMesh = NULL;
		CacheId = MakeCacheID(CID_RenderVertices);
        Revision = 0;
	}

	virtual QWORD GetCacheId()
	{
		return CacheId;
	}

	virtual INT GetRevision()
	{
		return Revision;
	}

	virtual INT GetSize()
	{
		if(!pProcMesh)
			return 0;
		return pProcMesh->Vertices.Num() * sizeof(FProcMeshVertex);
	}

	virtual INT GetStride()
	{
		return sizeof(FProcMeshVertex);
	}

	virtual INT GetComponents(FVertexComponent* OutComponents)
	{
		OutComponents[0].Type = CT_Float3;
		OutComponents[0].Function = FVF_Position;
		OutComponents[1].Type = CT_Float3;
		OutComponents[1].Function = FVF_Normal;
		OutComponents[2].Type = CT_Color;
		OutComponents[2].Function = FVF_Diffuse;
        OutComponents[3].Type = CT_Float2;
		OutComponents[3].Function = FVF_TexCoord0;
        return 4;
	}

	virtual void GetStreamData(void* Dest)
	{
		if(!pProcMesh)
			return;
        appMemcpy( Dest, &pProcMesh->Vertices(0), pProcMesh->Vertices.Num()*sizeof(FProcMeshVertex) );
	}

	virtual void GetRawStreamData(void ** Dest, INT FirstVertex )
	{
		*Dest = NULL;
	}
};

FProcVertStream procVertStream;

void AxProcMesh::Render(FLevelSceneNode* SceneNode,TList<class FDynamicLight*>* Lights,FRenderInterface* RI)
{
	guard(AxProcMesh::Render);
	if ( Texture == NULL || Vertices.Num()==0 )
		return;

    //if(SceneNode->Viewport->IsOrtho())
    //    return;

    INT RenderStartTime = appCycles();

	RI->SetTransform(TT_LocalToWorld,LocalToWorld());

	// setup lights ---
    INT LightsLimit = MaxLights; 
	INT	NumHardwareLights = 0;
	RI->EnableLighting(1,0,1,NULL,SceneNode->Viewport->Actor->RendMap == REN_LightingOnly,StaticMesh->GetRenderBoundingSphere(this));
	for(TList<FDynamicLight*>* LightList = Lights;LightList && NumHardwareLights < LightsLimit;LightList = LightList->Next)
		RI->SetLight(NumHardwareLights++,LightList->Element);			
	RI->SetAmbientLight(FColor(AmbientGlow,AmbientGlow,AmbientGlow));
	// --- lights

	DWORD extraPolyFlags = 0;
	if ( SceneNode->Viewport->IsWire() )
		extraPolyFlags |= (PF_Wireframe | PF_FlatShaded);

    UBOOL doRefraction = 0;
    if ( doRefraction )
    {
        for( int i=0; i<Vertices.Num(); i++ )
        {
            // Update texture coords and diffuse based upon refraction
            FVector v = (SceneNode->ViewOrigin - Vertices(i).Position).UnsafeNormal();
            FVector n = Vertices(i).Normal.UnsafeNormal();
            int refractIdx = 256 + f2i( (v | n) * 255.0f);
            FColor clr = RefractTable[refractIdx].dwDiffuse;
            clr.R = 255;
            clr.G = 255;
            clr.B = 255;
            Vertices(i).Color = clr;//FColor(255,255,255,255);//clr;
        }
    }

	// lock and fill
	procVertStream.Init(this);
	int firstVert = RI->SetDynamicStream(VS_FixedFunction, &procVertStream);

	if(SceneNode->Viewport->IsWire())
	{
		// Determine the wireframe color.

		UEngine*	Engine = SceneNode->Viewport->GetOuterUClient()->Engine;
		FColor		WireColor = Engine->C_StaticMesh;

		RI->EnableLighting(1,0,0,NULL,0,StaticMesh->GetRenderBoundingSphere(this));

		if(bSelected && (SceneNode->Viewport->Actor->ShowFlags & SHOW_SelectionHighlight))
			RI->SetAmbientLight(WireColor);
		else
			RI->SetAmbientLight(FColor(WireColor.Plane() * 0.5f));

		static FSolidColorTexture	WhiteTexture(FColor(255,255,255));

		DECLARE_STATIC_UOBJECT( UProxyBitmapMaterial, HACKGAH, { HACKGAH->SetTextureInterface(&WhiteTexture); } );
		DECLARE_STATIC_UOBJECT( UFinalBlend, LineMaterial, { LineMaterial->Material = HACKGAH; } );

		RI->SetMaterial(LineMaterial);
		RI->SetIndexBuffer(&StaticMesh->WireframeIndexBuffer,firstVert);

		// Draw the wireframe static mesh.

		RI->DrawPrimitive(
			PT_LineList,
			0,
			StaticMesh->WireframeIndexBuffer.Indices.Num() / 2,
			0,
			StaticMesh->VertexStream.Vertices.Num() - 1
			);
	}
	else
	{
        RI->SetIndexBuffer(&StaticMesh->IndexBuffer,firstVert);

        // set states and draw
	    UMaterial* pMat = Texture;
	    if ( Skins.Num() && Skins(0) )
	    {
		    pMat = Skins(0);
	    }
	    if ( pMat )
	    {
            RI->SetMaterial(pMat);
	    }

		for( int i=0; i<StaticMesh->Sections.Num(); i++ )
		{
			FStaticMeshSection&	Section = StaticMesh->Sections(i);
			//RI->SetMaterial(pMat);
			if(Section.NumPrimitives > 0)
			{
			#ifdef __PSX2_EE__
				extern void PSX2Render_RenderStaticMesh(UStaticMesh* StaticMesh, UStaticMeshInstance* Instance, INT SectionIndex);
				PSX2Render_RenderStaticMesh(StaticMesh, (Section.PolyFlags & PF_Unlit) ? NULL : Owner->Actor->StaticMeshInstance, SectionIndex);
			#else
				RI->DrawPrimitive(
					Section.IsStrip ? PT_TriangleStrip : PT_TriangleList,
					Section.FirstIndex,
					Section.NumPrimitives,
					Section.MinVertexIndex,
					Section.MaxVertexIndex
					);
			#endif
			}
		}
	}

	RI->EnableLighting(0);
    for(INT LightIndex = 0;LightIndex < NumHardwareLights;LightIndex++)
        RI->SetLight(LightIndex,NULL);

    GStats.DWORDStats( GEngineStats.STATS_Particle_RenderCycles ) += (appCycles() - RenderStartTime);

	unguard;
}

