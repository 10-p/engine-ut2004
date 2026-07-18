/*=============================================================================
	UnSkeletalTools.cpp: Misc Unreal mesh and animation code for 
	editing, importing and other non-game-time processing.	
	Mostly internal mesh data management.

	Copyright 2001,2002   Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Erik de Neve	 

	Todo: 
	Create cleaner division of processing/sorting/normal-calculation for rigid and smooth vertices.

=============================================================================*/ 

#include "EnginePrivate.h"
#include "UnRender.h"


// Forwards..
class TTempModel;
void CoherenceFaceSort( FStaticLODModel* Model );
void ModelDataReSort( FStaticLODModel* Model );
void RecalculateSmoothNormals( FStaticLODModel* Model );
void RecalculateLODNormals( FStaticLODModel* NewLOD, TArray<FVector>& TempNormals );


/*-----------------------------------------------------------------------------
	USkeletalMesh preprocessing: LOD/normals/hardware rendering chunking.
-----------------------------------------------------------------------------*/

QSORT_RETURN CDECL CompareBoneWeight( const FVertInfluence* A, const FVertInfluence* B )
{
	if		( A->VertIndex  > B->VertIndex ) return  1;
	else if ( A->VertIndex  < B->VertIndex ) return -1;
	else if ( A->Weight     < B->Weight    ) return  1;
	else if ( A->Weight     > B->Weight    ) return -1;
	else if ( A->BoneIndex  > B->BoneIndex ) return  1;
	else if ( A->BoneIndex  < B->BoneIndex ) return -1;
	else                                     return  0;	
}


//
// Temporary LOD model, used to reorganize rigid, smooth parts etc.
//

class TTempModel
{	
public:

	TArray<FVertInfluence> Influences;
	TArray<FMeshWedge>     Wedges;
	TArray<FMeshFace>      Faces;
	TArray<FVector>        Points;	
	TArray<FVector>        Normals;

	TTempModel(  FStaticLODModel* Model )
	{ 
		Influences.Empty();
		Influences.Add( Model->Influences.Num() );
		for( INT i=0; i< Model->Influences.Num(); i++)
			Influences(i) = Model->Influences(i);

		Wedges.Empty();
		Wedges.Add( Model->Wedges.Num() );
		for( INT i=0; i< Model->Wedges.Num(); i++)
			Wedges(i) = Model->Wedges(i);

		Faces.Empty();
		Faces.Add( Model->Faces.Num() );
		for( INT i=0; i< Model->Faces.Num(); i++)
			Faces(i) = Model->Faces(i);

		Points.Empty();
		Points.Add( Model->Points.Num() );
		for( INT i=0; i< Model->Points.Num(); i++)
			Points(i) = Model->Points(i);
	}

	void CopyInto( FStaticLODModel* Model )
	{
		Model->Influences.Empty();
		Model->Influences.Add( Influences.Num() );
		for( INT i=0; i< Influences.Num(); i++)
			Model->Influences(i) = Influences(i);

		Model->Wedges.Empty();
		Model->Wedges.Add( Wedges.Num() );
		for( INT i=0; i< Wedges.Num(); i++)
			Model->Wedges(i) = Wedges(i);

		Model->Faces.Empty();
		Model->Faces.Add( Faces.Num() );
		for( INT i=0; i< Faces.Num(); i++)
			Model->Faces(i) = Faces(i);

		Model->Points.Empty();
		Model->Points.Add( Points.Num() );
		for( INT i=0; i< Points.Num(); i++)
			Model->Points(i) = Points(i);
	}

	void AddNormals( TArray<FVector>& Normals )
	{
		Normals.Empty();
		Normals.Add( Normals.Num() );
		for( INT i=0; i< Normals.Num(); i++)
			Normals(i) = Normals(i);
	}
};

//
// Create all render-specific (but serializable) data for this LOD : the 'compiled' rendering stream, 
// mesh sections, and index buffer.
//
void CreateSkinningStream( FStaticLODModel* NewLOD, USkeletalMesh* ParentMesh )
{
	guard(CreateSkinningStream);
	// v 2.0 SkinningStream:
	//
	// 1xxxxxxxx - same as others, but copy result TO the buffer (on top)
	// 1111nnnnn - copy result FROM the buffer at index n
	//
	// - preshifted the dupe-buffer-index and bone index
	// 
	// - Safeguard that influences are limited to 7 max.....
	//	
	UBOOL bVerbose = ( GIsEditor || GIsUCC );

	TArray<INT> VertMarkers;
	TArray<INT> WedgeMarkers;
	TArray<INT> VertDupeIdx;

	// Clean up. Allow multiple calls to CreateSkinningStream for same model.
	NewLOD->SmoothSections.Empty();
	NewLOD->SmoothVerts.Empty();
	NewLOD->SkinningStream.Empty();	
	NewLOD->SmoothIndexBuffer.Indices.Empty();	

	// Set SmoothVerts... 
	NewLOD->SmoothVerts.Add( NewLOD->Points.Num() ); 

	for( INT i=0; i<NewLOD->Points.Num(); i++ ) 
	{			
		NewLOD->SmoothVerts(i).Point = NewLOD->Points(i); 
	}	
	// Recalculate packed normals.
	RecalculateSmoothNormals( NewLOD );

	VertMarkers.AddZeroed( NewLOD->SmoothVerts.Num());
	VertDupeIdx.AddZeroed( NewLOD->SmoothVerts.Num());
	WedgeMarkers.AddZeroed( NewLOD->Wedges.Num());

	DWORD UniqueDuplicates = 0;	

	// Predigest loop to detect where points for multiple wedges are first calculated.
	for( INT w=0; w<NewLOD->Wedges.Num(); w++)
	{				
		INT VertIdx = NewLOD->Wedges(w).iVertex; 
		VertMarkers(VertIdx)++;
		// Mark first occurrence of a vertex in a wedge..
		if( VertMarkers(VertIdx) == 1)
		{
			WedgeMarkers(w)= -1; 
		}
	}

	// Now: Total buffer size known, and which vertices need to go in a buffer/come from a buffer...
	// Wedges that need to store a vertex are ( -1)
	// Now mark wedges that need to retrieve one.
	for( INT w=0; w<NewLOD->Wedges.Num(); w++)
	{				
		INT VertIdx = NewLOD->Wedges(w).iVertex; 

		if (VertMarkers(VertIdx) > 1) // non unique vertex: wedge needs store or load special-case.
		{
			// Wedge has unique first vertex occurrence: store..-> 0
			if( WedgeMarkers(w) == -1) 
			{
				WedgeMarkers(w) = 0; // Indicate first-of-multiple - otherwise, -1 (unique vertex+wedge).
				//debugf(TEXT("Storage command for wedge %i vertex %i into top of buffer = %i "),w,VertIdx,UniqueDuplicates);
				UniqueDuplicates++;
				VertDupeIdx(VertIdx) = UniqueDuplicates; // location in buffer (+1)				
			}
			else if( WedgeMarkers(w) == 0 ) // retrieval needed
			{
				WedgeMarkers(w) = VertDupeIdx(VertIdx);
			}
		}
	}

	VertDupeIdx.Empty();
	NewLOD->DupVertCount = UniqueDuplicates;

	// debugf(TEXT("## Duplicates-to-store total : %i"),NewLOD->DupVertCount);
	
	// Marker arrays:
	// - know which wedge marks first occurrence of a point
	// - for recurring points, know where off the temp stack to get them.
	
	INT InfIdx = 0;
	INT BufSizeSim = 0; 

	// Wedges already sorted in render order.
	// Process wedges to create transformation stream (section-independent)
	for( INT w=0; w<NewLOD->Wedges.Num(); w++)
	{		
		FMeshWedge& ThisWedge = NewLOD->Wedges(w);
		//     Add either single bone + UV  
		//  or Add multiple weights+indices
		//  or Add back-index + UV  if vertex alread processed
		INT VertIdx = ThisWedge.iVertex; //ThisWedge.iVertex;
	
		if( WedgeMarkers(w) > 0 ) // 3d Point already computed - get it from the dupe buffer; index premultiplied with 6 (normal+vertex) floats
		{			
			NewLOD->SkinningStream.AddItem( (DWORD)(( WedgeMarkers(w)-1 )*6) | (DWORD)0xF0000000 );			
			// Add U and V.
			NewLOD->SkinningStream.AddItem(*((DWORD*)&ThisWedge.TexUV.U));
			NewLOD->SkinningStream.AddItem(*((DWORD*)&ThisWedge.TexUV.V));
		}
		else   
		{
			// Count influences: 
			INT LookIdx = InfIdx;

			INT InfluenceCount = 0;
			while ( (NewLOD->Influences(LookIdx).VertIndex == VertIdx) && ( NewLOD->Influences.Num() > LookIdx) )
			{			
				InfluenceCount++;
				LookIdx++;
			}	

			// debugf(TEXT(" Influence count for vertex %i  is %i "),VertIdx ,InfluenceCount);
			if( InfluenceCount == 0 ) 
			{				
				debugf(TEXT("NULL VERTEX INFLUENCE DETECTED."));				
			}	

			if( InfluenceCount > 7 )
			{
				debugf(TEXT("INFLUENCES OVERFLOW: [%i]"),InfluenceCount);
				InfluenceCount = 7;
			}

			// if( ForceSingleInfluence ) InfluenceCount = 1;
			
			// First in any influence seqeuence: indicator bits to store the number of influences, or a copy-back instruction.			
			DWORD IndicatorBits = ((DWORD)(Min(8,InfluenceCount)-1)) << (DWORD)(32-4);			

			// Set high bit IF this one has to go into the duplicate-vertex-buffer at render time.
			if( WedgeMarkers(w) == 0 )
			{
				IndicatorBits |= 0x80000000;				
				BufSizeSim++;				
			}
	
			for( INT i=0; i<InfluenceCount; i++)
			{
				FLOAT  Weight = 0;
				INT    BoneIndex = 0;
				
				BoneIndex = NewLOD->Influences(InfIdx+i).BoneIndex;

				// Safety check (esp. for when importing non-compatible meshes as static LOD models.)
				if( BoneIndex >= ParentMesh->RefSkeleton.Num() )
					BoneIndex = 0;

				if( InfluenceCount > 0)
				{
					Weight = NewLOD->Influences(InfIdx+i).Weight;					
				}	

				// assumes bone maximum of 1024
				DWORD CompiledInfluence =  IndicatorBits |  (((DWORD)( Weight * 65535.f )) << 12 )  |  (DWORD)(BoneIndex * 6);
				
				// Store.
				NewLOD->SkinningStream.AddItem( CompiledInfluence );
				
				// Subsequent influences have no need for indicator tags.
				IndicatorBits = 0;
			}

			// Special fixup case of missing influences...
			if( InfluenceCount == 0)
			{
				if( WedgeMarkers(w) == 0 )
				{
					IndicatorBits |= 0x80000000;				
					BufSizeSim++;				
				}
				InfluenceCount = 1;
				FLOAT Weight = 1.0f;
				INT   BoneIndex = 0;
				DWORD IndicatorBits = (DWORD)( 0 ) << (DWORD)(32-4);			
				DWORD CompiledInfluence =  IndicatorBits |  (((DWORD)( Weight * 65535.f )) << 12 )  |  (DWORD)(BoneIndex * 6);
				NewLOD->SkinningStream.AddItem( CompiledInfluence );				
			}

			InfIdx = LookIdx; // Advance influences.
					
			// Add U and V.
			NewLOD->SkinningStream.AddItem(*((DWORD*)&ThisWedge.TexUV.U));
			NewLOD->SkinningStream.AddItem(*((DWORD*)&ThisWedge.TexUV.V));

			// Mark as processed with location in output stream.
			VertMarkers( VertIdx ) = -1; 
		}
	}
	NewLOD->SkinningStream.AddItem( (DWORD)0xFFFFFFFF ); // End-Of-Stream marker.

	// debugf(TEXT("Created SkinningStream, size: %i    Temp vertex dupes: %i"),NewLOD->SkinningStream.Num(),BufSizeSim);	

	// Tests-interpret entire stream for debugging.
	if( 0 )
	{
		INT BufSizeSim = 0;
		INT RSIndex=0;
		INT SimVertex = 0;
		while( RSIndex < NewLOD->SkinningStream.Num() )
		{
			DWORD R = NewLOD->SkinningStream(RSIndex);

			// Trailing DWORD>..
			debugf(TEXT(">> STREAM [%5i]  hex: %8X  Indicator [%3i]  - boneindex: [%4i] "),
				RSIndex,
				R,			
				(INT)((DWORD)R >> (DWORD)(32-4)),
				(INT)(R & (DWORD)0x0FFF)/6 
			);
			
			INT WeightNum = (INT)((DWORD)R >> (DWORD)(32-4));

			if( (R & 0x80000000) && (R < 0xF0000000 ) ) // high bit set but no 0xF: calculate-and-store .
				WeightNum = (INT)((DWORD)(R & 0x7FFFFFFF) >> (DWORD)(32-4));

			if( (R & 0xF0000000 ) != 0xF0000000 )
			{
				if( R & 0x80000000 ) 
				{
					BufSizeSim++;
					debugf(TEXT("[ Copy to buffer top - total: %i]"),BufSizeSim);					
				}

				for( INT i=0; i<WeightNum+1; i++)
				{									
					DWORD R=NewLOD->SkinningStream(RSIndex);
					debugf(TEXT("     WEIGHTS(%i)  [%5i]  hex: %8X  Indicator [%3i]  Weight [%f] BoneIndex [%4i] "),
						i,
						RSIndex,
						R,			
						(INT)(R >> (32-4)),
						(1.f/65535.f) * (FLOAT)(((DWORD)R >> (DWORD)(12)) & (DWORD)0xFFFF), 
						(INT)(R & (DWORD)0x0FFF)/6 
					);
					
					RSIndex++;
				}			
			}
			else // Copy from duplicate buffer.
			{
				debugf(TEXT(" Copy back from buffer index [%i]  Curr. Vertex: %i "),( R & 0x0fffffff )/6,  SimVertex );
				RSIndex++;
			}

			RSIndex+=2; //skip U&V
			SimVertex++;
		}
	}
	
	// Fill SmoothIndexBuffer from entire face array.	
	for( INT f=0; f<NewLOD->Faces.Num(); f++)
	{
		for( INT i=0; i<3; i++)
		{
			NewLOD->SmoothIndexBuffer.Indices.AddItem( NewLOD->Faces(f).iWedge[i] );
		}
	}
        	
	if( ++ NewLOD->SmoothIndexBuffer.Revision < 1 ) 
		NewLOD->SmoothIndexBuffer.Revision = 1; 

	// Number of wedges the SkinningStream will produce at render time, as reported back via the vertex stream's GetSize (without having
	// to load the LazyArray Wedges !)
	NewLOD->SmoothStreamWedges = NewLOD->Wedges.Num(); 	
	
	// Digest the sections.
	FSkelMeshSection* Section = NULL;
	for(INT FaceIndex = 0;FaceIndex < NewLOD->Faces.Num(); FaceIndex++)
	{
		FMeshFace ThisFace = NewLOD->Faces(FaceIndex);
		INT  ThisMatIndex = ThisFace.MeshMaterialIndex;			
		if( !Section || ( ThisMatIndex != Section->MaterialIndex ) )
		{
			// Create a new static mesh section.
			Section = new( NewLOD->SmoothSections)FSkelMeshSection;
			Section->FirstFace = FaceIndex;
			Section->MaterialIndex = ThisFace.MeshMaterialIndex;						
			Section->TotalFaces = 0;			
			Section->MinIndex = NewLOD->Wedges.Num();
			Section->MaxIndex = 0;
		}
		Section->TotalFaces++;
		for( INT i=0; i<3; i++)
		{
			Section->MinIndex = Min(Section->MinIndex,ThisFace.iWedge[i]);
			Section->MaxIndex = Max(Section->MaxIndex,ThisFace.iWedge[i]);
		}
	}	

	for(INT s=0; s<NewLOD->SmoothSections.Num(); s++)
	{		
		NewLOD->SmoothSections(s).TotalVerts = ( NewLOD->SmoothSections(s).MaxIndex - NewLOD->SmoothSections(s).MinIndex )+1; 
		
		if( bVerbose) 
		{
			debugf(TEXT(" Smooth section constructed: [%3i], MinIndex  %3i, MaxIndex %3i, Material %3i Faces %3i FirstFace %3i Wedges: %i"),
			s,
			NewLOD->SmoothSections(s).MinIndex,
			NewLOD->SmoothSections(s).MaxIndex,
			NewLOD->SmoothSections(s).MaterialIndex,
			NewLOD->SmoothSections(s).TotalFaces,
			NewLOD->SmoothSections(s).FirstFace,
			NewLOD->SmoothStreamWedges
			);
		}
	}

	unguard;
}


//
// Resort and prepare the skinning stream for this LOD model
//
void FinalizeLodModel( FStaticLODModel* NewModel, USkeletalMesh* ParentMesh, UBOOL bSlowTaskAllowed )
{	
	guard( FinalizeLodModel);

	// Coherence-sort the (raw) faces.
	if( bSlowTaskAllowed ) CoherenceFaceSort( NewModel );  
	
	// Draw-order-sort the raw wedges and vertices as they are encountered through the face order.
	ModelDataReSort( NewModel );
	// NOTE: from here on, the raw mesh data is out of sync with LODModel data.	

	// Standard rigidparts extraction could be done here.

	// Prepare skinning stream for ComputeSkinVerts.
	CreateSkinningStream( NewModel , ParentMesh );

	unguard;
}
	

void LODModelErase( FStaticLODModel* Model )
{
	guard( LODModelErase );

	Model->SkinningStream.Empty();
	Model->SmoothVerts.Empty();	
	Model->SmoothSections.Empty();
	Model->RigidSections.Empty();

	Model->SmoothIndexBuffer.Indices.Empty();
	Model->RigidIndexBuffer.Indices.Empty();
	Model->RigidVertexStream.Vertices.Empty();

	Model->ActiveBoneIndices.Empty();
	Model->Influences.Empty();
	Model->Wedges.Empty();
	Model->Faces.Empty();
	Model->Points.Empty();

	Model->DisplayFactor = 0.0f; // Lod factor at which this LOD (or a lower one) kicks in.
	Model->LODHysteresis = 0.0f; // Flexible DisplayFactor range delta allowed to prevent flickering between lods.
	Model->DupVertCount = 0;  // Required duplicate vertices buffer size during skinning.	
	Model->MaxInfluences = 0; // For the smooth sections; if 0, they can use simpler rendering scheme.
	Model->bUniqueSubset = false; // Whether this LOD originated from a standalone, artist-defined mesh.
	Model->bUseSmoothing = false; // Flag: construct/redigest itself with smoothing groups.

	unguard;
}



//
// Total memory footprint; done explicitly, because we need to distinguish 
// specifically between Lazy arrays vs render-time data.
//
INT USkeletalMesh::MemFootprint( UBOOL RenderDataOnly )
{ 
	guard( USkeletalMesh::MemFootprint );
	INT MemTotal = 0;

	// Count all the on-disk lazyarrays too if requested.
	if( !RenderDataOnly )
	{
		// Base lazyarrays.
		RawVerts.Load();
		RawNormals.Load();
		RawWedges.Load();
		RawFaces.Load();
		RawInfluences.Load();
		RawCollapseWedges.Load();
		RawFaceLevel.Load();

		MemTotal += RawVerts.Num()          * sizeof( FVector );
		MemTotal += RawNormals.Num()        * sizeof( FMeshNorm );
		MemTotal += RawWedges.Num()         * sizeof( FMeshWedge );
		MemTotal += RawFaces.Num()          * sizeof( VTriangle );
		MemTotal += RawInfluences.Num()     * sizeof( FVertInfluence );
		MemTotal += RawCollapseWedges.Num() * sizeof( _WORD );
		MemTotal += RawFaceLevel.Num()      * sizeof( _WORD );

		// Dynamic arrays themselves take up 12 bytes; lazy arrays take 24.
		MemTotal += 24 * 7;
		
		// Lazy raw data from the LOD models.
		for( INT i=0; i< LODModels.Num(); i++)
		{
			LODModels(i).Influences.Load();
			LODModels(i).Wedges.Load();
			LODModels(i).Faces.Load();
			LODModels(i).Points.Load();

			MemTotal += LODModels(i).Influences.Num() * sizeof(FVertInfluence);
			MemTotal += LODModels(i).Wedges.Num() * sizeof(FMeshWedge);
			MemTotal += LODModels(i).Faces.Num() * sizeof(FMeshFace);
			MemTotal += LODModels(i).Points.Num() * sizeof(FVector);

			// Dynamic arrays 
			MemTotal += 24 * 4;
		}
	}

	//
	// Rendertime data.
	//

	for( INT i=0; i< LODModels.Num(); i++)
	{
		MemTotal += LODModels(i).SkinningStream.Num() * sizeof( DWORD );
		MemTotal += LODModels(i).SmoothVerts.Num() * sizeof( FSkinPoint );
		MemTotal += LODModels(i).SmoothSections.Num() * sizeof( FSkelMeshSection );
		MemTotal += LODModels(i).RigidSections.Num() * sizeof( FSkelMeshSection );
		MemTotal += LODModels(i).ActiveBoneIndices.Num() * sizeof( _WORD );		

		// Vertex and index buffers.
		MemTotal += sizeof( FRawIndexBuffer );
		MemTotal += sizeof( FRawIndexBuffer );
		MemTotal += sizeof( FSkinVertexStream );
		MemTotal += LODModels(i).SmoothIndexBuffer.Indices.Num() * sizeof( _WORD );
		MemTotal += LODModels(i).RigidIndexBuffer.Indices.Num() * sizeof( _WORD );
		MemTotal += LODModels(i).RigidVertexStream.Vertices.Num() * sizeof( FAnimMeshVertex );

		// Additional variables..
		MemTotal += 7 * sizeof ( DWORD );

		// Dynamic arrays 
		MemTotal += 12 * 5;
	}

	// General non-lazyarray data.
	MemTotal += RefBasesInverse.Num() * sizeof( FCoords );
	MemTotal += TagAliases.Num() * sizeof( FName );
	MemTotal += TagNames.Num() * sizeof( FName );
	MemTotal += TagCoords.Num() * sizeof( FCoords );

	// Dynamic arrays (including obsolete empty ones)
	MemTotal += 12 * (4 +  8);
	
	return MemTotal; 

	unguard;
}
	
// 
// Footprint for one specific LOD.
//
INT USkeletalMesh::LODFootprint( INT LODIndex, UBOOL RenderDataOnly )
{    
	guard(USkeletalMesh::LODFootprint);

	if( LODIndex < 0 || LODIndex >= LODModels.Num() )
		return 0;

	INT MemTotal = 0;	
	FStaticLODModel* LODModel = &LODModels(LODIndex);

	if( !RenderDataOnly )
	{
		LODModel->Influences.Load();
		LODModel->Wedges.Load();
		LODModel->Faces.Load();
		LODModel->Points.Load();

		MemTotal += LODModel->Influences.Num() * sizeof(FVertInfluence);
		MemTotal += LODModel->Wedges.Num() * sizeof(FMeshWedge);
		MemTotal += LODModel->Faces.Num() * sizeof(FMeshFace);
		MemTotal += LODModel->Points.Num() * sizeof(FVector);

		// Dynamic arrays 
		MemTotal += 24 * 4;
	}

	MemTotal += LODModel->SkinningStream.Num() * sizeof( DWORD );
	MemTotal += LODModel->SmoothVerts.Num() * sizeof( FSkinPoint );
	MemTotal += LODModel->SmoothSections.Num() * sizeof( FSkelMeshSection );
	MemTotal += LODModel->RigidSections.Num() * sizeof( FSkelMeshSection );
	MemTotal += LODModel->ActiveBoneIndices.Num() * sizeof( _WORD );		

	// Vertex and index buffers.
	MemTotal += sizeof( FRawIndexBuffer );
	MemTotal += sizeof( FRawIndexBuffer );
	MemTotal += sizeof( FSkinVertexStream );
	MemTotal += LODModel->SmoothIndexBuffer.Indices.Num() * sizeof( _WORD );
	MemTotal += LODModel->RigidIndexBuffer.Indices.Num() * sizeof( _WORD );
	MemTotal += LODModel->RigidVertexStream.Vertices.Num() * sizeof( FAnimMeshVertex );

	// Additional variables..
	MemTotal += 7 * sizeof ( DWORD );

	// Dynamic arrays 
	MemTotal += 12 * 5;

	return MemTotal;

	unguard;
}

//
// Generate a LOD level; hark back to older LOD data as our source if necessary for backward compatibility.
// Note, 0th LOD level needs to be generated explicitly also.
// TODO: more options needed for creation...
//

#define MAXLODLEVELS 8

void USkeletalMesh::GenerateLodModel( INT LodLevel, FLOAT ReductionFactor, FLOAT DisplayFactor, INT MaxInfluences, UBOOL bSlowTaskAllowed )
{
	guard(USkeletalMesh::GenerateLodModel);

	if (( LodLevel > MAXLODLEVELS ) || (LodLevel < 0 ))
		return;
	
	RawVerts.Load();
	RawNormals.Load();
	RawWedges.Load();
	RawFaces.Load();
	RawInfluences.Load();
	RawCollapseWedges.Load();
	RawFaceLevel.Load();

	// Don't waste memory..
	RawNormals.Empty();

	// Special-case deletion of model.. works only if it's the highest level.
	if( ReductionFactor == 0.f )
	{
		if ( LodLevel == LODModels.Num()-1 )
		{
			LODModels.Remove(LodLevel); //#SKEL
		}
		return;		
	}	

	// Make sure requested slot is valid. Empty LOD levels are allowed.
	while( LodLevel >= LODModels.Num() )
	{
		new(LODModels)FStaticLODModel(); // 'new' necessary - constructor sets up cacheID and revision.
	}

	FStaticLODModel* NewModel = &LODModels( LodLevel );

	// Clear any existing data.
	LODModelErase( NewModel );
	
	NewModel->DisplayFactor = DisplayFactor;

	if( (ReductionFactor != 1.0f) )
	{
		// Check valid collapse data.
		if( RawCollapseWedges.Num() )
		{		
			INT VertexSubset = Points.Num(); 
			FLOAT TargetSubset    = Max( (FLOAT)LODMinVerts, (FLOAT)RawVerts.Num() * ReductionFactor  );
			VertexSubset          = Min( appRound(TargetSubset), RawVerts.Num() );
			
			// 'Render' NewModel data from the ref.pose into this NewModel.
			for( INT FaceIdx = 0; FaceIdx < RawFaces.Num(); FaceIdx++)
			{							
				if( RawFaceLevel( FaceIdx ) <= VertexSubset ) 
				{
					VTriangle OldFace = RawFaces(FaceIdx);
					VTriangle NewFace;				
					NewFace.WedgeIndex[0] = NewFace.WedgeIndex[1] = NewFace.WedgeIndex[2] = 0;

					for( INT v=0; v<3; v++ )
					{
						INT WedgeIdx = OldFace.WedgeIndex[v];
						INT LODVertIndex = RawWedges(WedgeIdx).iVertex;
						// Go down LOD wedge collapse list until below the current LOD vertex count.
						while( LODVertIndex >= VertexSubset && WedgeIdx )
						{
							WedgeIdx = RawCollapseWedges(WedgeIdx);
							// Paranoid safeguard - this must not crash even with questionable old mesh data.
							if( WedgeIdx < 0 || WedgeIdx >= RawWedges.Num() )
							{
								debugf(TEXT("WrongCollapse.. wedgeIdx %i"),WedgeIdx);
								WedgeIdx = 0;
							}							
							LODVertIndex = RawWedges(WedgeIdx).iVertex;
						}
						NewFace.WedgeIndex[v] = WedgeIdx;
					}		
					NewFace.MatIndex = OldFace.MatIndex;				
					
					if( ( NewFace.WedgeIndex[0] != NewFace.WedgeIndex[1] ) && ( NewFace.WedgeIndex[0] != NewFace.WedgeIndex[2] ) ) 
					{
						FMeshFace ModelFace;
						ModelFace.iWedge[0] = NewFace.WedgeIndex[0];
						ModelFace.iWedge[1] = NewFace.WedgeIndex[1];
						ModelFace.iWedge[2] = NewFace.WedgeIndex[2];
						ModelFace.MeshMaterialIndex = NewFace.MatIndex;
						NewModel->Faces.AddItem( ModelFace );
					}
				}
			}
		}
	}
	else // Special case 0th LOD - copy uncollapsed faces.
	{
		NewModel->Faces.Add( RawFaces.Num() );
		// Copy face information.
		for( INT f=0; f<RawFaces.Num(); f++)
		{
			NewModel->Faces(f).MeshMaterialIndex = RawFaces(f).MatIndex;
			for( INT i=0; i<3; i++)
			{
				NewModel->Faces(f).iWedge[i] = RawFaces(f).WedgeIndex[i];
			}			
		}
	}


	// Copy rest of information ( all will get properly culled in ModelDataResort. )

	// Copy Wedge information.
	NewModel->Wedges.Add(RawWedges.Num());
	for( INT i=0; i<RawWedges.Num(); i++)
	{
		NewModel->Wedges(i) = RawWedges(i);
	} 	

	NewModel->Points.Add(RawVerts.Num());	
	for( INT i=0; i<RawVerts.Num(); i++)
	{			
		NewModel->Points(i) = RawVerts(i);		
	}

	// Copy influences information.
	NewModel->Influences.Add( RawInfluences.Num());
	for( INT i=0; i<RawInfluences.Num(); i++)
	{
		NewModel->Influences(i) = RawInfluences(i);		
	}		

	// Good default (just enough to avoid multi-frame flicker).				
	NewModel->LODHysteresis= 0.02f; 

	// Set allowed max number of bone influences per vertex.
	NewModel->MaxInfluences = Min( Max(MaxInfluences,1),7 );

	// Sort data and create predigested skinning stream.
	FinalizeLodModel( NewModel, this, bSlowTaskAllowed );

	unguard;	
}


//
// Use a raw NewModel mesh's data as a lod level for this mesh. Requirements: compatible material indices, compatible skeleton.
//
void USkeletalMesh::InsertLodModel( INT LodLevel, USkeletalMesh* RawMesh, FLOAT DisplayFactor, UBOOL bSlowTaskAllowed )
{
	guard( USkeletalMesh::InsertLodModel );

	if (( LodLevel > MAXLODLEVELS ) || (LodLevel < 0 ))
		return;

	// Make sure requested slot is valid. Empty LOD levels are allowed.
	while( LodLevel >= LODModels.Num() )
	{
		new(LODModels)FStaticLODModel(); // 'new' necessary - constructor sets up cacheID and revision.
	}
	FStaticLODModel* NewModel = &LODModels( LodLevel );

	// 'Draw' mesh's full LOD into the (raw) structures - including (smooth) sections/materials, material indices assumed compatible.
	// Section/material info: inferred from face's material indices.

	RawMesh->RawVerts.Load();
	RawMesh->RawFaces.Load();
	RawMesh->RawWedges.Load();
	RawMesh->RawNormals.Load();		
	RawMesh->RawInfluences.Load();
	RawMesh->RawCollapseWedges.Load();
	RawMesh->RawFaceLevel.Load();
	
	if( !RawMesh->RawVerts.Num() || 
		!RawMesh->RawFaces.Num() || 
		!RawMesh->RawWedges.Num() || 
		!RawMesh->RawInfluences.Num() 
		)
		return; // Avoid invalid input.

	// Clean out any existing data.
	LODModelErase(NewModel);

	// Safety - ensure we don't run out of material slots with the new mesh's material indices.
	INT NewMaxMatIndex = 0;
	for( INT i=0; i<RawMesh->RawFaces.Num(); i++)
	{
		if( RawMesh->RawFaces(i).MatIndex <0 )
			RawMesh->RawFaces(i).MatIndex = 0;

		NewMaxMatIndex = Max( NewMaxMatIndex, (INT)RawMesh->RawFaces(i).MatIndex );
	}

	// Expand slots if necessary..
	while( MeshMaterials.Num() <= NewMaxMatIndex )
	{
		  MeshMaterials.AddZeroed();
	}

	// Copy raw data.
	NewModel->Points.Add( RawMesh->RawVerts.Num());
	for( INT i=0; i<RawMesh->RawVerts.Num(); i++)
	{
		NewModel->Points(i) = RawMesh->RawVerts(i);
	}

	NewModel->Wedges.Add( RawMesh->RawWedges.Num());
	for( INT i=0; i<RawMesh->RawWedges.Num(); i++)
	{
		NewModel->Wedges(i) = RawMesh->RawWedges(i);
	}

	NewModel->Influences.Add( RawMesh->RawInfluences.Num());
	for( INT i=0; i<RawMesh->RawInfluences.Num(); i++)
	{
		NewModel->Influences(i) = RawMesh->RawInfluences(i);
	}

	NewModel->Faces.Add( RawMesh->RawFaces.Num());
	for( INT i=0; i<RawMesh->RawFaces.Num(); i++)
	{
		FMeshFace ModelFace;
		ModelFace.iWedge[0] = RawMesh->RawFaces(i).WedgeIndex[0];
		ModelFace.iWedge[1] = RawMesh->RawFaces(i).WedgeIndex[1];
		ModelFace.iWedge[2] = RawMesh->RawFaces(i).WedgeIndex[2];
		ModelFace.MeshMaterialIndex = RawMesh->RawFaces(i).MatIndex;
		NewModel->Faces(i) = ModelFace;
	}			

	// Allow maximum number of influences; with explicitly imported LOD levels,
	// any reduction is assumed to have been done in the source art.
	NewModel->MaxInfluences = 7; 

	// Indicate this LOD is made with external data and not to be 
	// automatically updated with the auto-generated LOD sets.
	NewModel->bUniqueSubset = true;
	
	// Sort data and create predigested skinning stream.
	FinalizeLodModel( NewModel, this, true );

	unguard;
}


// 
// Recalculate normals for the smoothskinned vertices.
//
void RecalculateSmoothNormals( FStaticLODModel* NewLOD )
{
	guard( UnSkeletalTools_CalculateNormals );

	if( NewLOD->Faces.Num()==0 ) return;

	TArray <FVector> TempNormals;

	TempNormals.AddZeroed( NewLOD->SmoothVerts.Num() );
		
	// Go over triangles and create face normals (in 'actor' space) by using dot products and accumulate them into the vertices.
	for( INT i=0; i<NewLOD->Faces.Num(); i++ )
	{
		INT VIdx[3]; // Vertex indices.
		for( INT v=0; v<3; v++)
		{
			VIdx[v]= NewLOD->Wedges( NewLOD->Faces(i).iWedge[v] ).iVertex;
		}

		FVector FaceNormal = ( NewLOD->SmoothVerts( VIdx[0] ).Point - NewLOD->SmoothVerts( VIdx[1] ).Point ) 
			               ^ ( NewLOD->SmoothVerts( VIdx[2] ).Point - NewLOD->SmoothVerts( VIdx[0] ).Point );
		// Don't normalize at this point - the surface of the triangle is factored in due to the cross product and lends appropriate bias to the normal.
		// Accumulate into normals of all vertices that make up this face.
		TempNormals(VIdx[0]) += FaceNormal;
		TempNormals(VIdx[1]) += FaceNormal;
		TempNormals(VIdx[2]) += FaceNormal;
	}	

	// Store normalized per-vertex normal.
	for( INT i=0; i<TempNormals.Num(); i++)
	{   			
		TempNormals(i) = TempNormals(i) / appSqrt( TempNormals(i).SizeSquared() + 0.0001f);		
		// Pack and assign.
		NewLOD->SmoothVerts(i).Normal = FMeshNorm( appRound( 511.f+TempNormals(i).X * 511.f),
				  				 				   appRound( 511.f+TempNormals(i).Y * 511.f),
												   appRound( 511.f+TempNormals(i).Z * 511.f));
	}
	unguard;
}


//
// Calculate temporary normals for this LOD model only.
//
void RecalculateLODNormals( FStaticLODModel* NewLOD, TArray<FVector>& TempNormals )
{
	guard( UnSkeletalTools_CalculateNormals );

	if( NewLOD->Faces.Num()==0 ) return;

	TempNormals.Empty();
	TempNormals.AddZeroed( NewLOD->Points.Num() );

	// Go over triangles and create face normals (in 'actor' space) by using dot products and accumulate them into the vertices.
	for( INT i=0; i<NewLOD->Faces.Num(); i++ )
	{
		INT VIdx[3]; // Vertex indices.
		for( INT v=0; v<3; v++)
		{
			VIdx[v]= NewLOD->Wedges( NewLOD->Faces(i).iWedge[v] ).iVertex;
		}

		FVector FaceNormal = ( NewLOD->Points( VIdx[0] ) - NewLOD->Points( VIdx[1] ) ) ^ 
			                 ( NewLOD->Points( VIdx[2] ) - NewLOD->Points( VIdx[0] ) );
		// Don't normalize at this point - the surface of the triangle is factored in due to the cross product and lends appropriate bias to the normal.
		// Accumulate into normals of all vertices that make up this face.
		TempNormals(VIdx[0]) += FaceNormal;
		TempNormals(VIdx[1]) += FaceNormal;
		TempNormals(VIdx[2]) += FaceNormal;
	}	

	// Store normalized per-vertex normal.
	for( INT i=0; i<TempNormals.Num(); i++)
	{   			
		TempNormals(i) = TempNormals(i) / appSqrt( TempNormals(i).SizeSquared() + 0.0001f);				
	}
	unguard;
}

// Force limits N influences (hardware preprocessing)
#define BONEINFLUENCELIMIT (3) 

void USkeletalMesh::NormalizeInfluences( INT LodLevel )
{
	guard(USkeletalMesh::NormalizeInfluences);

/*
	// Influences match the 3D points, not the wedges.
	InfluenceIndex.Empty();
	InfluenceIndex.Add(Points.Num());
	
	//Mark as uninitialized.
	for( INT i=0; i<InfluenceIndex.Num(); i++ )
	{   		
		InfluenceIndex(i).InfIndex[0] = 0xFFFF;
		InfluenceIndex(i).InfIndex[1] = 0xFFFF;
		InfluenceIndex(i).InfIndex[2] = 0xFFFF;
		InfluenceIndex(i).InfIndex[3] = 0xFFFF;	

		InfluenceIndex(i).BoneIndex[0] = 0;
		InfluenceIndex(i).BoneIndex[1] = 0;
		InfluenceIndex(i).BoneIndex[2] = 0;
		InfluenceIndex(i).BoneIndex[3] = 0;	
	}

	// Initialize the Influence-index array (for each POINT / vertex , knows whick bones influence it with what weight).
	for( INT w=0; w< BoneWeightIdx.Num(); w++ )
	{
		INT Index  = BoneWeightIdx(w).WeightIndex;
		INT Number = BoneWeightIdx(w).Number;
		if( Number > 0 )
		{
			for( INT b=Index; b<(Index+Number); b++ )
			{
				INT VertIndex = BoneWeights(b).PointIndex;

				// if (VertIndex < FrameVerts )   // LOD check...
				// FLOAT Weight = (FLOAT)BoneWeights(b).BoneWeight * ( 1.f/65535.f );

				// Store known weights and influence index per vertex, up to 4.
				INT InfCounter = 0;
				while( ( InfluenceIndex(VertIndex).InfIndex[InfCounter] != 0xFFFF) && InfCounter < BONEINFLUENCELIMIT )
					InfCounter++;
				
				// Use first noninitialized slot.
				if( InfCounter < BONEINFLUENCELIMIT ) // < 4 to obey the Playstation limit...
				{
					InfluenceIndex(VertIndex).BoneIndex[InfCounter] = w ; // bone index.
					InfluenceIndex(VertIndex).InfIndex[InfCounter] = b ;  // boneweights/influence index
				}

				// 4 influence slots were already full ?
				if( InfCounter >= BONEINFLUENCELIMIT ) // >= 4? : Zero out this bone's influence if we ran out of slots.
				{
					BoneWeights(b).BoneWeight = 0; 
				}
			}
		}
	}
	
	//  Restriction of max influences per vertex;:make sure these all add up to 1 (65536)
	for( i=0; i<InfluenceIndex.Num(); i++)
	{
		DWORD TotalWeight = 0;
		INT WeightCount = 0;

		// Find total
		for( INT j=0; j<4; j++) 
		{
			INT Index = InfluenceIndex(i).InfIndex[j];
			if( Index !=0xFFFF )
			{
				TotalWeight += BoneWeights( Index ).BoneWeight;
				WeightCount++;
			}			
		}

		// Ensure the influences add up to 65535 ( with some tolerance... )
		if( (TotalWeight > 0xFFFF) || (TotalWeight < 0xFFF0) ) // Rescaling needed ?
		{			
			FLOAT WeightScale = 65535.f/((FLOAT)TotalWeight + 0.01f);
			for( INT j=0; j<WeightCount; j++)
			{
				INT Index = InfluenceIndex(i).InfIndex[j];				
				BoneWeights( Index ).BoneWeight = (INT) ( WeightScale * (FLOAT)BoneWeights( Index ).BoneWeight);
			}
		}
	}
	//debugf(TEXT("Generated %i InfIndices"),InfluenceIndex.Num());

*/
	unguard;
}


// Calculate normals - only for PS2.
void USkeletalMesh::CalculateNormals(TArray <FVector>& Normals, UBOOL Displace )
{
	guard(USkeletalMesh::CalculateNormals);

	// Already filled?
	if( Normals.Num() ) 
		return;

	TArray <FVector> TempNormals;
	TempNormals.AddZeroed(Points.Num());
	
	// Go over triangles and create face normals (in 'actor' space) by using dot products and accumulate them into the vertices.
	for( INT i=0; i<Faces.Num(); i++ )
	{
		INT V[3]; // Vertex indices.
		for( INT v=0; v<3; v++)
		{
			V[v]= Wedges(Faces(i).iWedge[v]).iVertex;
		}
		FVector FaceNormal = (Points(V[0])-Points(V[1])) ^ (Points(V[2])-Points(V[0]));

		// Don't normalize at this point - the surface of the triangle is factored in due to the cross product and
		// lends appropriate bias to the normal.		
		// Accumulate into normals of all vertices that make up this face.
		TempNormals(V[0]) += FaceNormal;
		TempNormals(V[1]) += FaceNormal;
		TempNormals(V[2]) += FaceNormal;
	}

	Normals.Add(Points.Num());

	// Store normalized per-vertex normal.
	for( INT i=0; i<Normals.Num(); i++)
	{   			
		FVector NewNormal = TempNormals(i) / appSqrt(TempNormals(i).SizeSquared() + 0.001f);
		// Store displaced point when software-skinning.
		// NOTE - Keep displacement relatively large, otherwise float accuracy problems cause shimmering
		// lighting despite the fact that DX8 auto-normalizes the final normal lengths in hardware.
		if( Displace ) NewNormal = ( NewNormal * 1.0f) + Points(i); 
		Normals(i) = NewNormal;
	}

	unguard;
}


//
// Retrieve raw data from mesh either from data on import, or from pre-existing older-version .UKX or even .U mesh data.
//

void USkeletalMesh::ReconstructRawMesh()
{
	guard(ReconstructRawMesh);

	UBOOL bVerbose = ( GIsEditor || GIsUCC );

	// Fast reconstruction of full (non-LOD) data into 'raw' arrays.	

	RawVerts.Load();	
	RawFaces.Load();
	RawWedges.Load();
	RawNormals.Load();
	RawInfluences.Load();
	RawCollapseWedges.Load();
	RawFaceLevel.Load();

	RawVerts.Empty();
	RawWedges.Empty();
	RawFaces.Empty();
	RawInfluences.Empty();
	RawCollapseWedges.Empty();
	RawFaceLevel.Empty();
	
	INT BlendsLimit = Min( MultiBlends.Num(), 7);
	// Unwind the raw weights; then, after NewPointIndexes has been filled, remap 'em all.
	for( INT m=0; m< BlendsLimit ; m++)
	{
		if( m==0) // Single influence special case. 
		{		
			INT WeightIdx = MultiBlends(m).WeightBase;
			for( INT v=0; v<MultiBlends(m).PointIndices.Num(); v++) // All points with a single influence.
			{
				FVertInfluence NewWeight;
				NewWeight.VertIndex =  MultiBlends(m).PointIndices(v);
				NewWeight.BoneIndex =  Weights(WeightIdx).BoneIndex;					
				NewWeight.Weight = 1.0f; // Special-case single weight...

				// Safeguard bone bounds.
				if( NewWeight.BoneIndex >= RefSkeleton.Num())
				{
					debugf(TEXT("Spurious bone index on single influence bone: %i weight %f "),NewWeight.BoneIndex,NewWeight.Weight);
					NewWeight.BoneIndex=0;
					NewWeight.Weight = 0.0f; 
				}			
				
				RawInfluences.AddItem( NewWeight );
				WeightIdx++;							
			}
		}
		else if( m>0 ) // Multiple influences (can be more than 4...)
		{								
			INT WeightIdx = MultiBlends(m).WeightBase;
			for( INT v=0; v<MultiBlends(m).PointIndices.Num(); v++)
			{					
				INT VIndex = MultiBlends(m).PointIndices(v);
				// Multiple influences for Vertex  VIndex:
				for(INT n=0; n<(m+1); n++) 
				{
					FVertInfluence NewWeight;
					NewWeight.VertIndex =  VIndex;
					NewWeight.BoneIndex =  Weights(WeightIdx).BoneIndex;
					NewWeight.Weight = ( 1.0f/65535.f )*(FLOAT)Weights(WeightIdx).BoneWeight;

					// Safeguard against spurious bone indices.
					if( NewWeight.BoneIndex >= RefSkeleton.Num())
					{
						debugf(TEXT("Spurious bone index on %i th influence bone: %i weight %f "),v,NewWeight.BoneIndex,NewWeight.Weight);
						NewWeight.BoneIndex=0;
						NewWeight.Weight = 0.0f; 
						
					}
					
					RawInfluences.AddItem( NewWeight );
					WeightIdx++;
				}												
			}
		}
	}

	/*
	if( bVerbose) 
	{
		debugf(TEXT("Raw influences: %i  extracted from [%i] multiblends."),RawInfluences.Num(),MultiBlends.Num());	
		for( INT m=0; m< MultiBlends.Num(); m++)
		{
			debugf(TEXT("[%i]-bone influences: %i "),m+1,MultiBlends(m).PointIndices.Num() );
		}		
	}
	*/

	TArray<INT> NewWedgeIndexes;
	NewWedgeIndexes.AddZeroed(Wedges.Num());
	TArray<INT> NewPointIndexes;
	NewPointIndexes.AddZeroed(Points.Num());

	// Accumulate new wedge and point arrays.
	for( INT f=0; f<Faces.Num(); f++)
	{
		VTriangle NewFace;
		NewFace.MatIndex = Faces(f).MeshMaterialIndex;
		NewFace.SmoothingGroups = 1; // Smoothing information is lost when retrieving raw mesh from digested USkeletalMesh.

		for( INT v=0; v<3; v++)
		{
			NewFace.WedgeIndex[v] = Faces(f).iWedge[v];
		}
		RawFaces.AddItem(NewFace);
	}

	for( INT i=0; i<Wedges.Num(); i++)
	{
		FMeshWedge NewWedge;
		NewWedge.TexUV = Wedges(i).TexUV;
		NewWedge.iVertex = Wedges(i).iVertex; 
		RawWedges.AddItem(NewWedge);		
	}

	for( INT i=0; i<Points.Num(); i++)
	{
		FVector NewVert;
		NewVert = Points(i);		
		RawVerts.AddItem( NewVert );
	}

	for( INT i=0; i<CollapseWedgeThus.Num(); i++)
	{
		RawCollapseWedges.AddItem( CollapseWedgeThus(i) );
	}

	for( INT i=0; i<FaceLevel.Num(); i++)
	{
		RawFaceLevel.AddItem( FaceLevel(i) );
	}

	if( bVerbose) debugf(TEXT(" Raw data: Faces %i  Wedges %i Verts %i Collapses %i OldFaces %i OldWedges %i OldPoints %i "), 
		RawFaces.Num(),
		RawWedges.Num(),
		RawVerts.Num(),
		RawCollapseWedges.Num(),
		Faces.Num(),
		Wedges.Num(),
		Points.Num()		
	);

	// Old data, no longer needed.	
	Faces.Empty();
	Wedges.Empty();
	Points.Empty();
	CollapseWedgeThus.Empty();
	MultiBlends.Empty();
	Weights.Empty();
	InfluenceIndex.Empty(); 
	Weights.Empty();

	unguard;
}



//
// Preprocess: generate vertex buffers and sections for hardware rendering.
// Note: some of these are better moved to PostLoad ?
// 

INT USkeletalMesh::RenderPreProcess()
{
	guard(USkeletalMesh::RenderPreProcess);

#ifdef __PSX2_EE__
//	void HardwarePreProcess( USkeletalMesh& Mesh );
//	HardwarePreProcess(*this);
#endif

	return 1;
	unguard;
}


//
// Warning: unlike vertex animation, these bounds only reflect the size 
// of the reference pose. Actual animations are linked in at runtime
// so we can never be exactly sure of the effective bounding box.
// 
// NOTE: We can adjust the bounding box interactively in the editor
// when using .UKX package based content.
//

void USkeletalMeshInstance::MeshBuildBounds()
{
	guard(USkeletalMeshInstance::MeshBuildBounds);

	USkeletalMesh* Mesh= (USkeletalMesh*)GetMesh();

	GWarn->StatusUpdatef( 0, 0, TEXT("Bounding skeletal mesh") );

	// We'll need to build the extends with reference-pose vertices.
	Mesh->RawVerts.Load();
	TArray<FVector>RawPoints;
	for( INT i=0; i<Mesh->RawVerts.Num(); i++)
	{
		RawPoints.AddItem(Mesh->RawVerts(i));
	}

	Mesh->BoundingBox    = FBox   ( &RawPoints(0), RawPoints.Num() );
	Mesh->BoundingSphere = FSphere( &RawPoints(0), RawPoints.Num() );

	FBox Temp = Mesh->BoundingBox;

	// Extend by 2 to compensate for the fact that the skeletal bounds reflect the reference pose only...
	// Skeletal meshes - don't have frames beyond 0.
	/*
	Mesh->BoundingBox.Min = 2.0f*Temp.Min-(Temp.Min + Temp.Max)*0.5f;
    Mesh->BoundingBox.Max = 2.0f*Temp.Max-(Temp.Min + Temp.Max)*0.5f;
	*/

	// Tighter: scale up by + S x the distance from the midpoint; conservative bounds for psx2 players/models.
	FVector MidMesh = 0.5f*(Temp.Min + Temp.Max);
	Mesh->BoundingBox.Min = Temp.Min + 1.0f*(Temp.Min - MidMesh);  // 0.5 ?
    Mesh->BoundingBox.Max = Temp.Max + 1.0f*(Temp.Max - MidMesh);  //

	// Tuck up the bottom. - this rarely extends lower than a reference pose's ( having its feet on the floor.)
	Mesh->BoundingBox.Min.Z = Temp.Min.Z + 0.1f*(Temp.Min.Z - MidMesh.Z); //#SKEL

	Mesh->BoundingSphere.W *= 1.4f;
	
	unguard;
}




void USkeletalMesh::FlipFaces()
{
	guard(USkeletalMesh::FlipFaces);
	// Physique flipping ('bones') doesn't always correctly flip the faces - so do it here specifically if needed.	
	for(INT i=0; i<Faces.Num(); i++)
	{
		// Change handedness on faces.
		INT Wedge0 = Faces(i).iWedge[0];
		Faces(i).iWedge[0]=Faces(i).iWedge[1];
		Faces(i).iWedge[1]=Wedge0;
	}
	debugf(TEXT("Flipping all faces for model %s"),GetName());
	unguard;
}



/*
void DetermineSections( USkeletalMesh* Mesh)
{
	//
	// Create 'material' sections, with proper max/min & material indices, but most of the rest will be set dynamically;
	//

	FMeshSection* Section = NULL;
	for(INT FaceIndex = 0;FaceIndex < Mesh->Faces.Num(); FaceIndex++)
	{
		FMeshFace ThisFace = Mesh->Faces(FaceIndex);
		INT  ThisMatIndex = ThisFace.MeshMaterialIndex;
			
		if( !Section || ( ThisMatIndex != Section->MaterialIndex ) )
		{
			// debugf(TEXT("New section %i for material index %i"),SmoothSections.Num(),ThisMatIndex );
		
			// Create a new static mesh section.
			Section = new( Mesh->SmoothSections) FMeshSection;
			Section->FirstFace = FaceIndex;
			Section->MaterialIndex = ThisFace.MeshMaterialIndex;
			// Indices filled later, dynamically
			// Section->FirstIndex = LODChunks[LodIdx].Indexbuffer.Indices.Num();
			Section->MinIndex = Mesh->Wedges.Num()-1; // Updated below.
			Section->MaxIndex = 0; // Updated below.
			Section->TotalFaces = 0;
		}

		// For each vertex in the triangle determine the minimum/maximum index;
		// Not compatible with LOD since extra vertex-buffer vertices have been created
		// for new collapsed faces.
		//for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
		//{
		//	INT ThisVertIdx = ThisFace.iWedge[VertexIndex]; 
		//	Update the current section's minimum/maximum index hints.
		//	Section->MinIndex = Min<_WORD>(Section->MinIndex, ThisVertIdx );
		//	Section->MaxIndex = Max<_WORD>(Section->MaxIndex, ThisVertIdx );
		//}
		
		// Update the section's triangle count.
		Section->TotalFaces++;
	}
	
	//
	// Min/MaxIndex: must encompass all of a material, not assuming any ordering.
	//
	for(INT s=0; s<Mesh->SmoothSections.Num(); s++)
	{
		INT ThisMat = Mesh->SmoothSections(s).MaterialIndex;
		INT NewMin =  Mesh->SmoothSections(s).MinIndex;
		INT NewMax =  Mesh->SmoothSections(s).MaxIndex;
		
		for(INT w=0; w<Mesh->Wedges.Num(); w++)
		{
			if( ThisMat == WedgeMaterials(w) )
			{
				if( NewMin > w )
					NewMin = w;
				if( NewMax < w )
					NewMax = w;				
			}
		}
		
		Mesh->SmoothSections(s).FirstIndex = 0;
		Mesh->SmoothSections(s).MinIndex   = NewMin;
		Mesh->SmoothSections(s).MaxIndex   = NewMax;
		Mesh->SmoothSections(s).TotalVerts = (NewMax-NewMin)+1;
	}
	
	// Section report. 
	
	//{for(INT s=0;s<Mesh->SmoothSections.Num();s++)
	//{
		//debugf(TEXT("Min, maxindex for section %i   %i  %i mat:%i "),(INT)s,(INT)Mesh->SmoothSections(s).MinIndex, (INT)Mesh->SmoothSections(s).MaxIndex,(INT)Mesh->SmoothSections(s).MaterialIndex);	//}}
	
}

*/


//
// Mesh Faces: Sort then anew, for a specific LOD, by 'stripping'
//
void CoherenceFaceSort( FStaticLODModel* Model )
{
	guard(CoherenceFaceSort);

	UBOOL bVerbose = ( GIsEditor || GIsUCC );

	//
	// Super simple but effective: sort faces in order of most-vertices (wedges)-in-common, and to 'snake' around
	// triangles that were already hooked up.
	//
	// Slow O(n^2) triangle search is sufficient for now..
	//
	// Material order will be conserved.
	//
	// Note: can be made more hardware-vertex-buffer friendly by also evaluating 2, 3+ faces back for matching vertices 
	//
	// 

	TArray<INT> FaceStrip;
	TArray<INT> FaceMark;
	TArray<INT> VertMark;
	VertMark.AddZeroed(Model->Wedges.Num());
	FaceMark.AddZeroed(Model->Faces.Num());
	INT NextFace = 0; 
	
	FaceStrip.AddItem(NextFace);

	{for( INT i=0; i<(Model->Faces.Num()-1); i++ )
	{	
		FaceMark(NextFace) = 1;	
		INT StripMaterial = Model->Faces(NextFace).MeshMaterialIndex;

		INT FaceWedges[3];
		for( INT f=0; f<3; f++)
		{
			FaceWedges[f] = Model->Faces(NextFace).iWedge[f];
			VertMark(FaceWedges[f])=1;
		}

		INT BestWedgeMatch = -1;		
		INT BestCommon = -1;

		// Any others with wedges in common ?
		for( INT c=0; c<Model->Faces.Num(); c++)
		{			
			if( FaceMark(c) == 0 ) // unlinked face?
			{
				INT VertsCommon = 0;
				for( INT m=0; m<3; m++)
				{
					for( INT n=0; n<3; n++)
					{
						// Wedges in common with previous triangle is cool
						if( FaceWedges[n] == Model->Faces(c).iWedge[m] )
							VertsCommon+=16;
						// Favoring touching triangles that have already been done, for 'flood'-filling.
						VertsCommon += VertMark( Model->Faces(c).iWedge[m] );
					}
				}
				if( Model->Faces(c).MeshMaterialIndex != StripMaterial)
				{
					VertsCommon = 0;
				}
				// Will settle for this face if we found nothing else at all.
				if( (VertsCommon > BestCommon) || (BestWedgeMatch == -1) )
				{
					BestCommon = VertsCommon;
					BestWedgeMatch = c;
				}
			}			
		}

		// Assign it.
		FaceStrip.AddItem(BestWedgeMatch);	
		NextFace = BestWedgeMatch;		
	}}

	// Apply the FaceStrip sort order.
	TArray<FMeshFace> NewFaces;

	for( INT i=0; i<Model->Faces.Num(); i++ )
	{
		NewFaces.AddItem(Model->Faces(FaceStrip(i)));
	}

	for( INT i=0; i<Model->Faces.Num(); i++ )
	{
		Model->Faces(i) = NewFaces(i);
	}

	NewFaces.Empty();
	FaceStrip.Empty();
	FaceMark.Empty();

	if( bVerbose ) debugf(TEXT(" Coherence sorting completed."));

	unguard;
}


//
// Resorts wedges and vertices in first-come order according to faces, and also gets rid of unused wedges/vertices.
//
void ModelDataReSort( FStaticLODModel* Model )
{
	guard( ModelDataReSort );

	UBOOL bVerbose = ( GIsEditor || GIsUCC );

	// Resort RawWedges and RawVerts according to RawFaces.

	TArray<INT> NewWedgeIndexes;
	NewWedgeIndexes.AddZeroed( Model->Wedges.Num());
	TArray<INT> NewPointIndexes;
	NewPointIndexes.AddZeroed( Model->Points.Num());

	TArray<FMeshWedge> NewWedges;	
	TArray<FVector> NewVerts;	

	INT WedgeCounter = 0;
	INT PointCounter = 0;

	// Accumulate new wedge and point arrays on the fly.
	for( INT f=0; f<Model->Faces.Num(); f++)
	{
		// Add vertices/wedges in Face order. 
		for( INT v=0; v<3; v++)
		{
			INT OldWIdx = Model->Faces(f).iWedge[v];

			// If 0, first use; otherwise, already in our new list at (Index-1).
			if (NewWedgeIndexes( OldWIdx ) == 0)
			{								
				NewWedgeIndexes( OldWIdx ) = ++WedgeCounter;
				// New wedge- but is it a new vertex ?
				INT OldVIdx = Model->Wedges(OldWIdx ).iVertex;				
				// If 0, first use; otherwise, already in our new list at (Index-1).
				if( NewPointIndexes( OldVIdx )== 0)
				{										
					NewPointIndexes( OldVIdx ) = ++PointCounter;
					// Add this new point.					
					FVector NewVert = Model->Points( OldVIdx );
					NewVerts.AddItem( NewVert );
				}
				// Add this new wedge.
				FMeshWedge NewWedge;
				NewWedge.TexUV = Model->Wedges( OldWIdx  ).TexUV;
				NewWedge.iVertex = NewPointIndexes(OldVIdx)-1; // New vertex index... not: NewWedgeIndexes( OldWIdx ) -1;
				NewWedges.AddItem( NewWedge);
			}		

			// Ensure wedge index is updated.
			Model->Faces(f).iWedge[v] = NewWedgeIndexes(OldWIdx)-1;
		}				
	}

	// Copy the new Wedges and Verts.
	Model->Wedges.Empty();
	for( INT i=0; i<NewWedges.Num(); i++)
	{
		new(Model->Wedges)FMeshWedge(NewWedges(i));
	}
	
	Model->Points.Empty();   
	for( INT i=0; i<NewVerts.Num(); i++)
	{	
		new(Model->Points)FVector( NewVerts(i));
	}
	
	// Remap weights' indices to the new vertices.
	for( INT i=0; i<Model->Influences.Num(); i++)
	{
		Model->Influences(i).VertIndex = NewPointIndexes( Model->Influences(i).VertIndex )-1;
	}

	// Sort the weights by Point index....
	appQsort( &Model->Influences(0), Model->Influences.Num(), sizeof(FVertInfluence), (QSORT_COMPARE)CompareBoneWeight );

	if( 1 ) // influence cleanup 
	{
		// Remove runs of more than allowed number of weights...
		INT LastVert = -1;
		INT InfRun = 0;
		for(  INT i=0; i<Model->Influences.Num(); i++ )
		{		
			if( ( LastVert != Model->Influences(i).VertIndex ) )
			{
				InfRun = 0;
			}
			InfRun++;

			LastVert = Model->Influences(i).VertIndex;
			
			// More than allowed number of runs - or if a vertex is out of bounds: mark weight for deletion.
			if( ( InfRun >  Model->MaxInfluences ) || ( LastVert >= Model->Points.Num()) )
			{
				Model->Influences(i).Weight = 0.0f;
			}
		}

		INT RemovedInfs = 0;
		#define MINWEIGHT (0.01f) // 1% is the minimal allowed influence....	
		// Delete.
		for( INT i=Model->Influences.Num()-1; i>=0; i-- )
		{
			if( Model->Influences(i).Weight < MINWEIGHT )
			{
				Model->Influences.Remove(i);
				RemovedInfs++;
			}
		}

		if( bVerbose) debugf(TEXT("Redundant influences removed for LOD : %i  Influences-per-vertex limit: %i "),RemovedInfs,Model->MaxInfluences); 
	}
	
	//
	// Renormalize all weights, regardless of removed influences.
	//

	if( bVerbose ) debugf(TEXT("Recalibrate %i weights...."),Model->Influences.Num());

	if( 1 ) 
	{		
		INT InfRun = 0;
		INT LastRun = 0;
		INT LastVert = -1;
		FLOAT TotalWeight = 0;
		for( INT i=0; i<Model->Influences.Num(); i++ )
		{
			if(LastVert != Model->Influences(i).VertIndex)
			{
				LastRun = InfRun;
				InfRun = 0;
				// Normalize the run...
				if( LastRun && (TotalWeight != 1.0f) )
				{				
					FLOAT WeightAdjust = 1.f/TotalWeight;
					for( int r=0; r<LastRun; r++)
					{
						Model->Influences(i-r-1).Weight *=WeightAdjust;						
					}
				}
				TotalWeight = 0.f;				
				LastVert = Model->Influences(i).VertIndex;							
			}
			InfRun++;
			TotalWeight+= Model->Influences(i).Weight;			
		}
	}
		
	unguard;
}


//
// Extract rigid parts from already digested (in UKX..) mesh.
//
// Issue a 'stream' redigest and make that use only the remaining/duplicated smooth-skinned verts.
// Can assume raw data already properly stored.
//

//
class TempRawChunk
{
	public:
	INT BoneIndex;
	INT Flag;
	TArray<FMeshFace> Faces;
};
class BoneInfluence
{
	public:
	INT Bone;
	FLOAT Weight;
};
class VertForces
{
	public:
	TArray<BoneInfluence> Influences; 
};

enum RigidFaceStates
{
	RFS_Raw            =  0,
	RFS_Smooth         = -1,
	RFS_Rigid          = -2,
	RFS_ConfirmedRigid = -3,
};

void USkeletalMesh::ExtractRigidParts( INT LODModelIndex, INT PartMinVerts, INT MaxParts, INT SectionMethod )
{
	guard(USkeletalMesh::ExtractRigidParts);	

	UBOOL bVerbose = ( GIsEditor || GIsUCC );

	FStaticLODModel& Model = this->LODModels(LODModelIndex);
	
	// Model raw data.
	Model.Wedges.Load();
	Model.Faces.Load();
	Model.Points.Load();
	Model.Influences.Load();

	Model.RigidVertexStream.Vertices.Empty();
	Model.RigidSections.Empty();	
	if( ++ Model.RigidVertexStream.Revision < 1 )
		Model.RigidVertexStream.Revision = 1;
	if( ++ Model.RigidIndexBuffer.Revision < 1 )
		Model.RigidIndexBuffer.Revision = 1;			

	//
	// Recalculate normals.
	//
	TArray<FVector> TempNormals;
	RecalculateLODNormals( &Model, TempNormals );
	
	//
    // Extracting rigid parts to individual pools of faces/verts.	
	//

	// Fill complete rigid vertex stream ( #TODO: CULL ?! This one duplicates ALL verts - not just rigid ones ... )
	for(INT v = 0; v < Model.Wedges.Num(); v++)
	{
		FAnimMeshVertex NewVert;		
		NewVert.U = Model.Wedges(v).TexUV.U;
		NewVert.V = Model.Wedges(v).TexUV.V;
		NewVert.Position = Model.Points( Model.Wedges(v).iVertex );
		NewVert.Normal   = TempNormals( Model.Wedges(v).iVertex ); 		
		NewVert.Normal.Normalize();
		Model.RigidVertexStream.Vertices.AddItem(NewVert);
	}

	// No software skinning callback for rigid segments.
	Model.RigidVertexStream.bStreamCallback = false; 

	//
	// Main bone index for each vertex; -1 if multiple indices...
	// TArray<INT> SingleBoneIndexes; 
	// SingleBoneIndexes.AddZeroed( Model.Points.Num() );
	//

	TArray<TempRawChunk> SolidChunks;
	TArray<INT> FaceMarkers;  // Flags faces' digestion status.
	FaceMarkers.AddZeroed( Model.Faces.Num() );

	// Accumulate bone forces for each vertex.
	TArray<VertForces> PointBones;
	PointBones.AddZeroed( Model.Points.Num() );	
	
	if( SectionMethod == MSM_SinglePiece )
	{
		// Single-piece-forcing: all bone influences become single bone 0 influences.
		for( INT i=0; i<PointBones.Num(); i++)
		{
			BoneInfluence NewBone;
			NewBone.Bone = 0;
			NewBone.Weight = 1.0f;
			PointBones(i).Influences.AddItem( NewBone );
		}
	}
	else
	{
		for( INT i=0; i<Model.Influences.Num(); i++)
		{
			BoneInfluence NewBone;
			NewBone.Bone = Model.Influences(i).BoneIndex;
			NewBone.Weight = Model.Influences(i).Weight;
			PointBones(Model.Influences(i).VertIndex).Influences.AddItem(NewBone);		
		}
	}

	//
	// TODO: optionally cull/sort influences for more tolerant to-rigid conversion...
	//


	// Mark only truly rigidizable tris.
	for( INT f=0; f< Model.Faces.Num(); f++)
	{
		INT CommonBone = -1;
		for( INT v=0; v<3; v++)
		{
			INT PointIdx = Model.Wedges( Model.Faces( f ).iWedge[v] ).iVertex;		
			// Only single influence of same bone for all three verts counts as rigid.
			if( PointBones( PointIdx ).Influences.Num()==1 )
			{
				INT VertBone = PointBones( PointIdx ).Influences(0).Bone;
				if( CommonBone == -1) 
					CommonBone = VertBone;
				else if( CommonBone != VertBone )
					CommonBone = -2; // Discard.
			}
			else
			{
				CommonBone = -2; //Discard.
			}			
		}		

		if( CommonBone >= 0 )		
			FaceMarkers(f) = CommonBone;
		else
			FaceMarkers(f) = RFS_Smooth; // Face stretches between mult. bones or is dangling with no influences.
	}

	//
	// Accumulate all truly rigid chunks - flood-fill style - until exhausted.
	//
	INT AllFaceCount = Model.Faces.Num();		
	INT TotalRigidFaces = 0;

	TArray<INT> NewChunkFaces;

	while( AllFaceCount )
	{	
		INT CurrentFace = -1;
		INT CommonBone = -1;
		INT CommonMaterial = -1;

		// Get first unprocessed face.
		for( INT f=0; f< Model.Faces.Num(); f++)
		{
			if( FaceMarkers(f) >= 0)
			{
				CurrentFace = f;
				break;
			}
		}

		// All eligible faces processed already ?
		if( CurrentFace < 0 )
			break;
		
		// Add first face..
		CommonBone = FaceMarkers( CurrentFace );
		CommonMaterial = Model.Faces( CurrentFace ).MeshMaterialIndex;
		FaceMarkers( CurrentFace ) = RFS_Rigid; // Mark as processed.
		NewChunkFaces.AddItem( CurrentFace);
		AllFaceCount--;		
		
		// Find ALL connecting faces with the common bone - add these to chunk.
		for( INT f=0; f< Model.Faces.Num(); f++)
		{
			if( FaceMarkers(f) >= 0)
			{
				if( FaceMarkers(f) == CommonBone &&  Model.Faces(f).MeshMaterialIndex == CommonMaterial )
				{
					FaceMarkers(f) = RFS_Rigid;
					NewChunkFaces.AddItem(f);					
					AllFaceCount--;
				}
			}
		}	
		
		// Save the newly accumulated chunk: only if: "MinFaces" faces or bigger.
		if( CommonBone >= 0 && ( NewChunkFaces.Num()*3 > PartMinVerts) ) // Kludgy vertex criterium.
		{			
			INT NewIdx = SolidChunks.AddZeroed();
			SolidChunks(NewIdx).BoneIndex = CommonBone;
			for( INT i=0; i< NewChunkFaces.Num(); i++)
			{
				// Mark new added face as succesfully rigid.
				FaceMarkers( NewChunkFaces(i) ) = RFS_ConfirmedRigid;

				FMeshFace NewFace;
				NewFace.iWedge[0] = Model.Faces(NewChunkFaces(i)).iWedge[0];
				NewFace.iWedge[1] = Model.Faces(NewChunkFaces(i)).iWedge[1]; 
				NewFace.iWedge[2] = Model.Faces(NewChunkFaces(i)).iWedge[2];
				NewFace.MeshMaterialIndex = Model.Faces( NewChunkFaces(i) ).MeshMaterialIndex;
				SolidChunks(NewIdx).Faces.AddItem( NewFace );				
			}

			TotalRigidFaces += NewChunkFaces.Num();
						
			if( bVerbose ) debugf(TEXT(" Solid chunk %i has %i faces, BONE: %i Meshmaterialindex %i "),SolidChunks.Num(),SolidChunks(NewIdx).Faces.Num(), SolidChunks(NewIdx).BoneIndex, SolidChunks(NewIdx).Faces(0).MeshMaterialIndex );
		}
		NewChunkFaces.Empty();
	}
	
	INT FaceIndex = 0;


	// Save the data before processing.
	TTempModel TempModel( &Model );	

	// Dump indices into index buffer and establish the sections.
	Model.RigidSections.AddZeroed(SolidChunks.Num());

	for( INT c=0; c<SolidChunks.Num(); c++)
	{
		FSkelMeshSection* Section = &Model.RigidSections(c);	
		Section->FirstIndex = Model.RigidIndexBuffer.Indices.Num();				
		Section->MinIndex = Model.RigidVertexStream.Vertices.Num();		
		Section->MaxIndex = 0;
		
		// Dump rigid indices
		for( INT f=0; f<SolidChunks(c).Faces.Num(); f++)
		{
			for( INT i=0; i<3; i++)
			{
				// Add indices
				_WORD ThisWedgeIdx = SolidChunks(c).Faces(f).iWedge[i];
				Model.RigidIndexBuffer.Indices.AddItem( ThisWedgeIdx );				

				// keep track of min/max wedge index for section.
				Section->MinIndex = Min( Section->MinIndex, ThisWedgeIdx );
				Section->MaxIndex = Max( Section->MaxIndex, ThisWedgeIdx );				
			}
		}
				
		// Find the min & maxindex (vertex index) for this section.				
		Section->TotalVerts = SolidChunks(c).Faces.Num()*3; // The total number of vertices // or the span  of vertices used ?!?!?
		Section->TotalWedges = 0; // Not relevant.
		Section->MaxInfluences = SolidChunks(c).BoneIndex;  // Re-using MaxInfluences as the bone index...
		Section->MaterialIndex = SolidChunks(c).Faces.Num() ? SolidChunks(c).Faces(0).MeshMaterialIndex : 0; // Material index.
		Section->FirstFace = FaceIndex;                       // First face (pointing into index buffer) for the section.
		Section->TotalFaces = SolidChunks(c).Faces.Num();     // Total primitives to render.		

		FaceIndex += SolidChunks(c).Faces.Num();
	}	

	// Digestion 'SectionMethod' options:
	//	MSM_SmoothOnly,    // Smooth (software transformed) sections only.
	//	MSM_RigidOnly,     // Only draw rigid parts, throw away anything that's not rigid.
	//	MSM_Mixed,         // Convert suitable mesh parts to rigid and draw remaining sections smoothly (software transformation).
	//  MSM_SinglePiece,   // Freeze all as a single static piece just as in the refpose.
	//	MSM_ForcedRigid,   // Force smooth parts into rigid along main bones etc.
			
	// Blank out smooth sections - ONLY if the conversion was complete AND not too many parts.
	if( SolidChunks.Num() <= MaxParts && (SolidChunks.Num() > 0) ) 
	{
		if( bVerbose ) debugf(TEXT("Rigid conversion: Total faces: %i  Rigid faces: %i  Total rigid sections: %i "),Model.Faces.Num(), TotalRigidFaces, SolidChunks.Num() );		

		// There are valid rigid sections, so discard previous smooth sections always; if we want any they'll be regenerated.
		Model.SmoothStreamWedges = 0;
		Model.SmoothSections.Empty();			
		Model.SmoothVerts.Empty();
		Model.SkinningStream.Empty();	
		Model.SmoothIndexBuffer.Indices.Empty();			
	
		if( SectionMethod == MSM_Mixed )
		{						
			if( bVerbose ) debugf(TEXT("LODModel total faces: %i"),Model.Faces.Num() );

			// Cull SmoothVerts/points/faces to only do the partial smooth mesh left.
			for( INT f=Model.Faces.Num()-1; f>=0; f--)
			{
				if( FaceMarkers(f) == RFS_ConfirmedRigid )
				{
					Model.Faces.Remove(f);
				}
			}						

			if( bVerbose ) debugf(TEXT("Smooth-skinned faces left after culling rigid faces: %i"),Model.Faces.Num() );

			if( Model.Faces.Num() ) // Any left? Create software sections.
			{			
				// Re-cohere the remaining faces as well as possible.
				CoherenceFaceSort( &Model );

				// Based on faces, cull all points and wedges, and reindexes everything accordingly.
				ModelDataReSort( &Model ); 

				// Create a new skinning stream with just the remaining smooth data.
				CreateSkinningStream( &Model, this );

				if( bVerbose ) debugf(TEXT("Size of partial smooth data after skinstream creation: Faces %i Verts %i Wedges %i Infs %i "),Model.Faces.Num(),Model.Points.Num(),Model.Wedges.Num(),Model.Influences.Num() );
			}
		}
	}
	else // Rigid part collection deemed too fragmented:
	{						
		Model.RigidVertexStream.Vertices.Empty();
		Model.RigidIndexBuffer.Indices.Empty();
		Model.RigidSections.Empty();	
	}

	// Restore lodmodel data regardless of any rigidparts processing/culling.
	TempModel.CopyInto( &Model );

	unguard;
}



//
// Sequence list order management. 
//
struct SortSeqTag
{
	FName SequenceName;
	INT   OriginalIndex;	
};


static QSORT_RETURN CDECL CompareSequenceName( const SortSeqTag* A, const SortSeqTag* B )
{
	FString NameA( *(A->SequenceName) );
	FString NameB( *(B->SequenceName) );

	     if( NameA > NameB ) return  1;
	else if( NameA < NameB ) return -1;
	else return  0;	
}

//
// Sort all sequences by sequence name - affects Moves and Animseqs.
//
INT UMeshAnimation::SortSequencesByName()
{
	// Sort FNAMES to obtain ReorderIndices, copy into new array, erase old array, copy into old array with new order.	
	
	if( (AnimSeqs.Num() < 2 ) || (AnimSeqs.Num() != Moves.Num() ) )
		return 0;	

	TArray<MotionChunk> TempMoves;
	TempMoves.AddZeroed(Moves.Num());
	TArray<FMeshAnimSeq> TempSeqData;
	TempSeqData.AddZeroed(Moves.Num());
	TArray<SortSeqTag> ReorderSeqTags;
	ReorderSeqTags.AddZeroed(Moves.Num());	
	for( INT n=0; n<Moves.Num(); n++)
	{
		// Setup sort data.
		ReorderSeqTags(n).SequenceName = AnimSeqs(n).Name;
		ReorderSeqTags(n).OriginalIndex = n;

		//if( n > 0)
		//debugf(TEXT("SortSeq outcome:  %i to %i is %i  from names %s %s "),n-1,n, (INT) CompareSequenceName( &ReorderSeqTags(n-1), &ReorderSeqTags(n) ),  *ReorderSeqTags(n-1).SequenceName,  *ReorderSeqTags(n).SequenceName  );
				
		// Copy all data.				                
		TempMoves(n) = Moves(n);
		Moves(n).Erase();      		
		// Store the animseqs.
		TempSeqData(n) = AnimSeqs(n);
		AnimSeqs(n).Erase();								
	}	

	appQsort( &ReorderSeqTags(0), ReorderSeqTags.Num(), sizeof(SortSeqTag), (QSORT_COMPARE) CompareSequenceName );

	for( INT n=0; n<Moves.Num(); n++)
	{				
		INT OrigIndex = ReorderSeqTags(n).OriginalIndex;
		//debugf(TEXT("Original Index for %i  was %i "),n, OrigIndex  ); //#SKEL!!!
		Moves(n)   = TempMoves( OrigIndex );
		TempMoves( OrigIndex ).Erase();
		AnimSeqs(n)= TempSeqData( OrigIndex );
		TempSeqData( OrigIndex ).Erase();
	}

	for( INT n=0; n<TempMoves.Num(); n++)
	{
		TempMoves(n).Erase();
	}

	return Moves.Num();
}

//
// If indices are valid, moves around a single animation sequence.
//
INT UMeshAnimation::MoveSequenceByIndex( INT SourceIdx, INT DestIdx )
{
	if(( SourceIdx == DestIdx ) ||
	   ( SourceIdx < 0 || SourceIdx >= Moves.Num()) ||
	   ( DestIdx < 0   || DestIdx   >= Moves.Num()) ||
	   ( AnimSeqs.Num() != Moves.Num() ) )
		return -1;

	// Special (only practical) case:  move a sequence up or down. 
	// Just swap both the Moves and AnimSeqs.
	if( abs( SourceIdx-DestIdx) == 1 )
	{
		MotionChunk TempMoveDest;
		appMemzero( (void*)&TempMoveDest,sizeof(MotionChunk));		
		TempMoveDest = Moves(DestIdx);

		// Lazy arrays need to be loaded before they can be shuffled.
		
		//Moves( DestIdx ).RawTracks.Load();
		//Moves( SourceIdx ).RawTracks.Load();		

		Moves( DestIdx ).Erase();
		Moves( DestIdx ) = Moves( SourceIdx );		

		Moves( SourceIdx ).Erase();
		Moves( SourceIdx ) = TempMoveDest;  

		TempMoveDest.Erase();

		FMeshAnimSeq  TempSeqData = AnimSeqs(DestIdx);
		appMemzero( (void*)&TempSeqData,sizeof(FMeshAnimSeq));

		TempSeqData = AnimSeqs( DestIdx );

		AnimSeqs( DestIdx ).Erase();
		AnimSeqs( DestIdx ) = AnimSeqs( SourceIdx );

		AnimSeqs( SourceIdx ).Erase();
		AnimSeqs( SourceIdx ) = TempSeqData;

		TempSeqData.Erase();

		return DestIdx;
	}

	// TODO: Implement general case.
	// ...

	return -1;	
}





