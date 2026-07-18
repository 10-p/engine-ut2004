/*==================================================================================================
	UnMeshLP.cpp: Unreal Mesh LODProcess. Level-of-detail mesh preprocessing code.
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Erik de Neve.
        * Textured vertex 'wedges' concept inspired by Hugues Hoppe's papers (Microsoft Research).
		* Edge collapse processing code based on article: 'A Simple, Fast, and Effective Polygon 
		  Reduction Algorithm' by Stan Melax, Game Developer Magazine Nov. 1998.
		* Jan 2000 - skeletal mesh (LOD) processing code. Duplicates part of the regular mesh processing but starts
		  from different binary/raw format data.
	    * Jul 2001 - wedge sort makes more sense now - coherent, sorted by material then 3d point index.

	NOTES:
	[Mesh lod processing] This code takes a regular Unreal1 mesh & animation data *internally* at content-compile
	time, after it has been imported as a regular UMesh, and re-digests it into new structures 
	fit for real-time LOD collapse and morphing. 

	TODO:
	Currently there's a number of collapsing and noncollapsing arrays, the latter are
	(but not all) inside the Mesh-> hierarchy, but some in TModel too - a bit messy.

    !!SMOOTHING Groups: almost work but the vertex duplication still clashes with the LOD system:
	non-synchronous collapse causes holes, and not all normals are properly precalculated (too many
	duplicate vertices are still generated )

======================================================================================================*/

#include "EditorPrivate.h"
#include "UnRender.h"
    
#undef  NOTE
#define NOTE(func)  {}
//#define NOTE(func) func


#undef  NLOG
#define NLOG(func) {}
//#define NLOG(func) func

// Is the UV difference for two textured vertices small enough to not duplicate them ?
// Assume a 1.0 or smaller pixel difference in a 1024x1024 texture can be merged. 
#define ISDIFFERENT_UV( x, y ) (Square( x - y ) > Square( 1.0f/1024.f ))

// Throw out bone influences that are < .5 percent.
#define MINBONEWEIGHT (0.005f)

/*-----------------------------------------------------------------------------
	Level of Detail processing.
-----------------------------------------------------------------------------*/

// Temporary LOD processing classes ( never serialized - for intermediate use only )
// Analogous to the UVertMesh structures, this is a setup with vertices, textured
// vertices and Faces.

// Globals
INT LODStyle;

// Forward declarations.
struct TVertex;
struct TFace;		
struct TWedge;

// Textured vertex ('wedge').
struct TWedge
{
	INT             OriginalIndex; 
	INT             VertexIndex;    
	INT             DestinationIndex;
	INT             MaterialID;
	FLOAT           U,V;
	DWORD           SmoothFlags;
	DWORD           StatusFlags;

	TWedge& operator=( const TWedge& Other )
	{
		OriginalIndex = Other.OriginalIndex;
		VertexIndex = Other.VertexIndex;
		DestinationIndex = Other.DestinationIndex;
		MaterialID = Other.MaterialID;
		U = Other.U;
		V = Other.V;
		SmoothFlags = Other.SmoothFlags;
		StatusFlags = Other.StatusFlags;
		return *this;
	}

	// Criterium for 'AddUnique' to determine shared vertices(and normals) according to smoothing group masks.
	UBOOL operator==( const TWedge& Other )
	{		
		return ( VertexIndex == Other.VertexIndex ) &&
			   ( MaterialID == Other.MaterialID ) &&
			   ( U == Other.U ) &&
			   ( V == Other.V ) &&
			   ( SmoothFlags == Other.SmoothFlags );
	}

};

// Temp structure for digesting bone-vertex weights.
struct TWorkWeight
{
	INT PointIndex;
	INT BoneIndex;
	INT Multiplicity;
	FLOAT Weight;
};

// Temp structure for smoothing-group vertex duplication.
struct TSmoothVert
{
	INT   PointIndex;
	INT   OriginalIndex;
	DWORD SmoothFlags;	
	DWORD StatusFlags;

	// Criterium for 'AddUnique' to determine shared vertices(and normals) according to smoothing group masks.
	UBOOL operator==( const TSmoothVert& Other )
	{		
		return ( PointIndex == Other.PointIndex ) && 
			   ( ( (SmoothFlags & Other.SmoothFlags) != 0 ) || ( (SmoothFlags == 0) && (Other.SmoothFlags == 0) ) );
	}
};

QSORT_RETURN CDECL CompareSmoothVertKey( const TSmoothVert* A, const TSmoothVert* B )
{
	if     ( A->PointIndex > B->PointIndex ) return  1;
	else if( A->PointIndex < B->PointIndex ) return -1;
	else if( A->OriginalIndex > B->OriginalIndex ) return  1;
	else if( A->OriginalIndex < B->OriginalIndex ) return -1;	
	else if( A->StatusFlags > B->StatusFlags ) return  1;	
	else if( A->StatusFlags < B->StatusFlags ) return -1;	
	else  return 0;
}

// Structure used for sorting wedges.
struct TWedgeSort
{
	INT		SortedWedgeIndex;
	TWedge  Wedge;
};

QSORT_RETURN CDECL CompareWedgeKey( const TWedgeSort* A, const TWedgeSort* B )
{
	if     ( A->Wedge.MaterialID  > B->Wedge.MaterialID  ) return  1;		
	else if( A->Wedge.MaterialID  < B->Wedge.MaterialID  ) return -1;		
	else if( A->Wedge.VertexIndex > B->Wedge.VertexIndex ) return  1;		
	else if( A->Wedge.VertexIndex < B->Wedge.VertexIndex ) return -1;		
	else if( A->Wedge.U > B->Wedge.U ) return  1;
	else if( A->Wedge.U < B->Wedge.U ) return -1;
	else if( A->Wedge.V > B->Wedge.V ) return  1;
	else if( A->Wedge.V < B->Wedge.V ) return -1;
	else  return 0;
}


// Vertex
struct TVertex
{
	FVector             Point3D;        //  Our 3d point.
	INT					OriginalIndex;  //  place of vertex in original list.	

	TArray<TVertex*>    Verts;		    //  all directly connected vertices  (TransientVtxes)
	TArray<TFace*>		Faces;		    //  all directly connected triangles (Mesh->Faces)
	TArray<TWedge*>     Wedges;         //  a wedge* for each Face: ie, these give the UV of a Face at _this_ vertex.

	INT                 Flag;           //  
	FLOAT				MergeCost;	    //  Cost of collapsing to ColTarget
	TVertex*			ColTarget;      //  candidate vertex for collapse

	                 TVertex(FVector v, INT OrigIndex, INT FlagInit);  
	                 ~TVertex();        
	INT              RemoveIfNonNeighbor(TVertex *n);
	INT              IsBorder();
};


// Face
struct TFace
{
	INT					OriginalIndex;    // 
	TVertex*	        Verts[3];         // Nontextured vertices 
	TWedge*             Wedges[3];        // The same, textured vertices. 

	INT                 MaterialID;       // Material Index.
	FVector             Normal;           // Normalized normal vector.

	UBOOL	HasVertex( TVertex *U );
	int     GetVertexIndex(TVertex *U);
			TFace( INT MatID, TWedge *v0, TWedge *v1,TWedge *v2, INT StartIndex);
			~TFace();
	void    ComputeNormal();
	void    ReplaceVertex( TVertex *VOld, TVertex *VNew );
};


// Global mesh for LOD processing.
class TMesh
{
public:

	TArray<TFace*>      GFaces;      // Faces (allocated individually)
	TArray<TVertex*>    GVerts;      // Vertices (allocated individually)
	TArray<TWedge>      GWedges;     // Wedges 
	TArray<TWedge>      GAuxWedges;  // Auxiliary wedge copies.
	TArray<INT>         GOriginalVertIdx; // Original vertex indices mirroring GVerts(x).OriginalIndex;

	TArray<_WORD>         FaceLevel;    // mirrors Mesh's facelevel
	TArray<FMeshMaterial> Materials;    // mirrors Mesh's materials

	// Vertex based evaluation and collapse methods
	FLOAT	GetCollapseCost( TVertex *U, TVertex *V );
	INT     IsBorderVertex ( TVertex *U );
	void    UpdateEdgeCost ( TVertex *U );
	void	Collapse( TVertex *U );		

	void Reset()
	{
		GFaces.Empty();
		GVerts.Empty();
		GWedges.Empty();
		GAuxWedges.Empty();
		GOriginalVertIdx.Empty();
		FaceLevel.Empty();
		Materials.Empty();
	}

}TModel;


// Methods.

// By making the TMesh TModel a global, TFace and TVertex methods can update 
// the arrays that they're part of, themselves.

TFace::TFace(INT MatID, TWedge *v0, TWedge *v1,TWedge *v2, INT StartIndex)
{
	guard(TFace::TFace);

	MaterialID = MatID;
	OriginalIndex = StartIndex;

	Wedges[0] = v0;
	Wedges[1] = v1;
	Wedges[2] = v2;
	//! kludgy ! GVerts must have been set up completely.
	Verts[0] = TModel.GVerts(v0->VertexIndex);
	Verts[1] = TModel.GVerts(v1->VertexIndex);
	Verts[2] = TModel.GVerts(v2->VertexIndex);

	// Discard triangles with non-unique vertices.
	if ( Verts[0]==Verts[1] || Verts[1]==Verts[2] || Verts[2]==Verts[0] )
	{
		debugf(TEXT("Mesh LOD processing warning - non-unique vertices for face %5i are: %5i %5i %5i"),
			   this->OriginalIndex,
			   Verts[0]->OriginalIndex,
			   Verts[1]->OriginalIndex,
			   Verts[2]->OriginalIndex );
		return; // Don't add this face to the big array then !!!
	}

	ComputeNormal();
	TModel.GFaces.AddItem( this );

	for( INT i=0; i<3; i++) 
	{
		// Update bordering vertices with Face.
		Verts[i]->Faces.AddItem( this );
		// Back-link Wedge to its vert.
		Verts[i]->Wedges.AddItem( Wedges[i] ); 

		// Update neighboring vertices with these vertices.
		for( INT j=0; j<3; j++ ) 
		{
			if ( i!=j ) Verts[i]->Verts.AddUniqueItem( Verts[j] );
		}
	}
	unguard;
}

TFace::~TFace()
{
	guard(TFace::~TFace);
	TModel.GFaces.RemoveItem( this );

	// Record the vertex count at which this face became obsolete.
	TModel.FaceLevel(this->OriginalIndex) = TModel.GVerts.Num();
	NOTE(debugf(TEXT("%% Original Face %i  Collapsed when there were %i vertices left."),this->OriginalIndex,TModel.GVerts.Num());)
	NOTE(debugf(TEXT("%% And its original wedges were %i %i %i "),Wedges[0]->OriginalIndex,Wedges[1]->OriginalIndex,Wedges[2]->OriginalIndex );)

	for( INT i=0; i<3; i++ ) 
	{
		if( Verts[i] ) 
		{
			//debugf(TEXT("Removing face %i from vertex %i"),this->OriginalIndex,Verts[i]->OriginalIndex);
		    INT FaceIndex = Verts[i]->Faces.FindItemIndex(this);
			// remove the face and the wedge with the same index as the face just removed.
		    Verts[i]-> Faces.Remove(FaceIndex);
			Verts[i]->Wedges.Remove(FaceIndex);
		}
	}

	for( INT i=0; i<3; i++) 
	{
		INT i2 = (i+1)%3;
		if( ! Verts[i] || ! Verts[i2] ) continue;

		Verts[i ]->RemoveIfNonNeighbor( Verts[i2] );
		//debugf(TEXT("Removing vertex %i if nonneighbor vertex %i "),Verts[i]->OriginalIndex,Verts[i2]->OriginalIndex); 

		Verts[i2]->RemoveIfNonNeighbor( Verts[i ] );
		//debugf(TEXT("Removing vertex %i if nonneighbor vertex %i "),Verts[i2]->OriginalIndex,Verts[i]->OriginalIndex);
	}
	unguard;
}

int TFace::HasVertex(TVertex *U) 
{
	return ( U==Verts[0] || U==Verts[1] || U==Verts[2] );
}

int TFace::GetVertexIndex(TVertex *U)
{
	if (U==Verts[0]) return 0;
	if (U==Verts[1]) return 1;
	if (U==Verts[2]) return 2;

	debugf(TEXT("LOD preprocessing error: Vertex index not found in face."));
	return 0;
}

void TFace::ComputeNormal()
{
	guard(TFace::ComputeNormal);

	FVector v0 = Verts[0]->Point3D;
	FVector v1 = Verts[1]->Point3D;
	FVector v2 = Verts[2]->Point3D;

	Normal = (v1-v0) ^ (v2-v1);
	Normal /= (FLOAT)appSqrt(Normal.SizeSquared());

	unguard;
}


//  Replace a collapsing vertex in a triangle.
//  Might create a new wedge if UV texture coordinate morphing 
//  is desirable for this collapse.

void TFace::ReplaceVertex(TVertex *VOld, TVertex *VNew) 
{
	guard(TFace::ReplaceVertex);
	check(VOld && VNew);
	check(VOld==Verts[0] || VOld==Verts[1] || VOld==Verts[2]);
	check(VNew!=Verts[0] && VNew!=Verts[1] && VNew!=Verts[2]);

	NOTE(debugf(TEXT("Replacevertex  %i  by %i"),VOld->OriginalIndex,VNew->OriginalIndex));

	INT NewVertexIndex;

	if( VOld==Verts[0] )
	{
		NewVertexIndex = 0;
	}
	else if( VOld==Verts[1] )
	{
		NewVertexIndex = 1;
	}
	else 
	{
		check(VOld==Verts[2]);
		NewVertexIndex = 2;
	}
	Verts[NewVertexIndex] = VNew;

	// Kludgy wedge save/restore to make them match the faces array at each vertex.

	// Remove corresponding face and wedge connections from collapsing vertex.
	INT RemoveIndex = VOld->Faces.FindItemIndex(this);
	VOld->Faces.Remove(RemoveIndex);

	TWedge *OldWedge = VOld->Wedges(RemoveIndex); //Save...
	VOld->Wedges.Remove(RemoveIndex);

	check( VNew->Faces.FindItemIndex(this) == INDEX_NONE );
	// Add new face and wedge connectivity info to this new vertex.
	VNew->Faces.AddItem(this);
	VNew->Wedges.AddItem(OldWedge); // Insert old one again.

	for( INT i=0; i<3; i++ )
	{
		VOld->RemoveIfNonNeighbor(Verts[i]);
		Verts[i]->RemoveIfNonNeighbor(VOld);
	}

	for( INT i=0; i<3; i++ )
	{
		check( Verts[i]->Faces.FindItemIndex(this) != INDEX_NONE );
		for( INT j=0; j<3; j++ ) if( i!=j )
		{
			Verts[i]->Verts.AddUniqueItem(Verts[j]);
		}
	}

	ComputeNormal();

	unguard;
}


TVertex::TVertex(FVector V,INT OrigIdx, INT FlagInit) 
{
	guard(TVertex::TVertex);
	Point3D = V;
	OriginalIndex = OrigIdx;
	Flag = FlagInit;
	TModel.GVerts.AddItem( this );
	unguard;
}

TVertex::~TVertex()
{
	guard(TVertex::~TVertex);

	// The vertex should have been detached from any face before deletion.
	check( Faces.Num()==0 );

	// Remove all mutual references between neighbors and us.
	while( Verts.Num() )
	{
		Verts(0)->Verts.RemoveItem(this);
		Verts.RemoveItem(Verts(0));
	}

	// Remove all Wedges references.
	Wedges.Empty();
	
	TModel.GVerts.RemoveItem(this);
	unguard;
}


int TVertex::RemoveIfNonNeighbor(TVertex *n) 
{
	guard(TVertex::RemoveIfNonNeighbor);
	// removes n from neighbor list if n isn't a neighbor.
	if( Verts.FindItemIndex(n) == INDEX_NONE ) return 0;

	for( INT i=0;i<Faces.Num();i++ )
	{
		if( Faces(i)->HasVertex(n) ) return 0;
	}
	Verts.RemoveItem(n);
	return 1;

	unguard;
}

//
// Edge detection.
//

int TVertex::IsBorder() 
{
	guard(TVertex::IsBorder);
    INT i,j;
    for( i=0; i < Verts.Num(); i++ ) 
	{
        INT Count=0;
        for( j=0; j < Faces.Num(); j++ ) 
		{
			if( Faces(j)->HasVertex( Verts(i) ) ) 
			{
				Count++;
            }
        }
        check( Count>0 );
        if( Count==1 ) return 1;
    }
    return 0;
	unguard;
}




//
//  Collapsing vertex A to B : Any triangle that has a wedge in point A, when that wedge is also present
//  as part of one of 1-2 triangles along the edge to be collapsed, then it should lerp it's UV's as it
//  collapses.  
//
//  Future refinements?
//  1: only if those common wedges are adjacent, or indirectly adjacent. 
//  2: lerping shouldn't make previously unused parts of textures visible if at all avoidable
//  3: there should be a lot more merging if at all possible, to get greater visual consistency
//     especially for body parts that are 'sort of' projection-mapped.
//  4: Texture fitness? Any two vertices that merge, the more wedges they DON't have in common _WITH THE wedges
//	   around the two bordering tris_ , the more expensive the collapse.
//

//
// Cost of collapsing u -> v
//
FLOAT ComputeEdgeCollapseCost( TVertex *u, TVertex *v ) 
{
	guard(ComputeEdgeCollapseCost);
	INT i;
	FLOAT EdgeLength; 

	guard(FDist);
	EdgeLength = FDist(v->Point3D, u->Point3D); 
	unguard;

	FLOAT Curvature=0.0f;
	FLOAT TexRespect = 1.0f;
	FLOAT TransBias = 1.0f;
	INT   Translucent = 0;
	INT   Planar = 1;

	// Find the "sides" triangles that are on the edge uv.
	TArray<TFace*>    SideFaces;
	TArray<TWedge*> SideWedges;
	TArray<TWedge*> NonSideWedges;
	TArray<TWedge*> UniqueWedges;

	// Detect side wedges, materials.
	for( i=0; i<u->Faces.Num(); i++ ) 
	{
		if(u->Faces(i)->HasVertex(v))
		{
			SideFaces.AddItem(u->Faces(i));		
			// check if not a double-sided poly
			if( ! (TModel.Materials( u->Faces(i)->MaterialID).PolyFlags & PF_TwoSided) )
			{
				Planar = 0;
			}

			if( (TModel.Materials( u->Faces(i)->MaterialID).PolyFlags & PF_Translucent) )
			{
				Translucent = 1;
			}

			SideWedges.AddItem(u->Wedges(i));
		}
		else
		{
			NonSideWedges.AddItem(u->Wedges(i));
		}
	}

	//    Gauge texture 'collapsability' ease: perfect if there's only one wedge (one texture
	//    UV pair shared by all bordering triangles.)
	for ( i=0; i<u->Wedges.Num(); i++)
	{
		UniqueWedges.AddUniqueItem(u->Wedges(i));
	}

	// This vertex is on a material/smoothing group SEAM: increase the 'texture respect' factor.
	if( UniqueWedges.Num() > 1 )
	{
		// Extra texture seam protection
		if( LODStyle & 4 ) 
		{
			TexRespect = 2.1f * UniqueWedges.Num();
		}
		else
		{
			// Default seam protection
			TexRespect = 0.7f * UniqueWedges.Num(); 
		}
	}

	// Extra translucent-material protection bit.
	if( LODStyle & 16 ) 
	{
		if( Translucent )
		{
			TransBias = 1.7f;
		}
	}

	// Planar (edge only borders doublesided polys): 'fixes' minimum curvature.
	if( ( LODStyle & 2 ) && ( Planar > 0 ) )
	{
		Curvature = 0.35f;  // Was 0.7f in 224v.
	}
	

	// But fix minimum Curvature if u is on a border but v isn't.
	if( u->IsBorder() && (SideFaces.Num() > 1) ) 
	{
		Curvature = 1.f; 
	}
	else
	{
		// Use the triangle facing most away from the sides 
		// to determine our Curvature term.
		// The more coplanar the lower the Curvature term 
		for( i=0; i<u->Faces.Num(); i++ ) 
		{
			FLOAT Mincurv=1.f; // Curve for face i and closer side to it.
			for( INT j=0; j<SideFaces.Num(); j++ ) 
			{
				// Use dot product of face normals.
				FLOAT Dotprod = u->Faces(i)->Normal | SideFaces(j)->Normal;
				Mincurv = Min(Mincurv,(1-Dotprod)/2.0f);
			}
			Curvature = Max(Curvature,Mincurv);
		}
	}

	// Emphasize lenght by disregarding curvature. Cool for blocky low-poly picups, boxes.
	if( LODStyle & 8 ) 
	{
		Curvature = 1.0f;
	}

	// Emphasize curvature by disregarding lengths.
	if( LODStyle & 1 )
	{
		EdgeLength = 1.0f;
	}

	// Make edge length twice as important.
	if( LODStyle & 32 )
	{
		EdgeLength = EdgeLength*EdgeLength;
	}

	return EdgeLength * Curvature * TexRespect * TransBias;

	unguard;
}

void ComputeEdgeCostAtVertex(TVertex *v) 
{
	guard(ComputeEdgeCostAtVertex);

	if( v->Verts.Num()==0 ) 
	{
		// v Doesn't have neighbors so it costs nothing to collapse.
		v->ColTarget = NULL;
		v->MergeCost= -0.01f;
		return;
	}

	v->MergeCost = 1000000.f;
	v->ColTarget= NULL;

	// Search all neighboring edges for "least cost" edge.
	for( INT i=0; i<v->Verts.Num(); i++ ) 
	{
		FLOAT dist;
		dist = ComputeEdgeCollapseCost( v, v->Verts(i) );
		if( dist<v->MergeCost ) 
		{
			v->ColTarget= v->Verts(i);  // Candidate for edge collapse.
			v->MergeCost = dist;        // Cost of the collapse.
		}
	}
	unguard;
}



void Collapse(TVertex *u, TVertex *v)
{
	guard(Collapse);

	// Collapse the edge uv by moving vertex u onto v
	// Actually remove tris on uv, then update tris that
	// have u to have v, and then remove u.
	INT i;

	if(!v) 
	{
		// u is a vertex all by itself so just delete it.
		if( u )
		{
			// Delete lone triangles.
			for( i=u->Faces.Num()-1; i>=0; i--) 
			{				
				delete u->Faces(i);
			}
			delete u;
		}
		return;
	}

	// Before deleting ANY faces, decide what to do with
	// each wedge involved: it can either morph to a wedge at the destination,
	// or we need to make a new wedge with the adapted vertex index.

	// Check to see if we kept wedges and faces matched.
	check( u->Faces.Num() == u->Wedges.Num() );
	check( v->Faces.Num() == v->Wedges.Num() );

	TArray<TFace*>  SideFaces;
	TArray<TWedge>  SideWedges; // No pointers - because content changes during loop.

	TArray<TWedge*> NonSideWedges;
	TArray<TWedge*> UniqueNonSideWedges;

	SideFaces.Empty();
	SideWedges.Empty();
	NonSideWedges.Empty();
	UniqueNonSideWedges.Empty();

	for( i=0; i<u->Faces.Num(); i++ ) 
	{
		if( u->Faces(i)->HasVertex(v) )
		{
			SideFaces.AddItem(u->Faces(i));
			SideWedges.AddItem(*u->Wedges(i));
		}
		else
		{
			NonSideWedges.AddItem(u->Wedges(i));
		}
	}

	for( i=0; i<NonSideWedges.Num(); i++ )
	{
		UniqueNonSideWedges.AddUniqueItem(NonSideWedges(i));		
	}


	// Update wedges per unique non-side wedge.
	for( i=0; i<UniqueNonSideWedges.Num(); i++ )
	{
		// Does it match any of the side wedges - if so,
		// prepare a new wedge for UV-morphing.

		INT MatchedWedgeIdx = INDEX_NONE;
		// MatchedWedgeIdx = SideWedges.FindItemIndex( UniqueNonSideWedges(i)); //NO! we want to compare UV only.	
		// Vertex match is implied.
		for( INT w=0; w< SideWedges.Num(); w++)
		{
			NOTE(debugf(TEXT("matchtry UVunsw %i %i with UVSide %i %i"),UniqueNonSideWedges(i)->U,UniqueNonSideWedges(i)->V,SideWedges(w).U,SideWedges(w).V);)

			//#TODO - any UV tolerance in preprocessing necessary ?
			if( (UniqueNonSideWedges(i)->U == SideWedges(w).U) && (UniqueNonSideWedges(i)->V == SideWedges(w).V) )// == compares only U V
				MatchedWedgeIdx = w;
		}	

		if ( MatchedWedgeIdx == INDEX_NONE ) // We can't morph, therefore make a new wedge in this place.
		{
			// With each wedge's 'originalindex' intact, and the (to be sorted in collapse order)
			// Vertex it points to, the whole collapse history is retrievable.
			// Add copy of current wedge to auuxiliary wedge array.
			TModel.GAuxWedges.AddItem( *UniqueNonSideWedges(i) ); 

			/*
			if ( UniqueNonSideWedges(i)->VertexIndex == v->OriginalIndex ) debugf(TEXT(" DOUBLE COLLAPSE DETECTED:UNSW %i  U %i  V  %i"), UniqueNonSideWedges(i)->VertexIndex,u->OriginalIndex,v->OriginalIndex);
			if ( UniqueNonSideWedges(i)->VertexIndex != u->OriginalIndex ) debugf(TEXT(" VERTEX IDENTITY CRISIS:UNSW %i  U %i  V  %i"), UniqueNonSideWedges(i)->VertexIndex,u->OriginalIndex,v->OriginalIndex);
			*/

			NOTE(debugf(TEXT("Update wedge oix %i from vertex %i to vertex %i"),UniqueNonSideWedges(i)->OriginalIndex,UniqueNonSideWedges(i)->VertexIndex,v->OriginalIndex ));
			NOTE(debugf(TEXT(" Keep UV %i %i  "), UniqueNonSideWedges(i)->U,UniqueNonSideWedges(i)->V );)

			// Stretch wedge to the destination vertex:
			// Construct the new wedge.
			UniqueNonSideWedges(i)->VertexIndex = v->OriginalIndex; 
			UniqueNonSideWedges(i)->StatusFlags = 1; // Non-original wedge - does not need to be duplicated for smoothing groups (?) #SKEL
			
		}
		else // We can morph, therefore copy the destination wedge content to this wedge.
		{
			// Add copy of current wedge to auxiliary wedge array.
			TModel.GAuxWedges.AddItem( *UniqueNonSideWedges(i) );  

			// Find the exact wedge we're collapsing to.
			// It's he wedge of the SideFaces face(MatchedWedgeIdx) that has the same index as vertex V.
			TWedge *DestinationWedge;
			DestinationWedge = SideFaces(MatchedWedgeIdx)->Wedges[ SideFaces(MatchedWedgeIdx)->GetVertexIndex(v) ];

			NOTE(debugf(TEXT("Morphed wedgeoix %i to oix %i , from vertex %i to vertex %i"),UniqueNonSideWedges(i)->OriginalIndex,DestinationWedge->OriginalIndex,UniqueNonSideWedges(i)->VertexIndex,v->OriginalIndex ));
			NOTE(debugf(TEXT("Morph from UV %i %i to %i %i"), UniqueNonSideWedges(i)->U,UniqueNonSideWedges(i)->V,DestinationWedge->U,DestinationWedge->V );)

			// We morphed to another wedge:
			// Construct that wedge (but keep our originalindex.)
			// UniqueNonSideWedges(i)->OriginalIndex = DestinationWedge->OriginalIndex; 
			UniqueNonSideWedges(i)->U			= DestinationWedge->U; 
			UniqueNonSideWedges(i)->V			= DestinationWedge->V; 
			UniqueNonSideWedges(i)->VertexIndex = DestinationWedge->VertexIndex; 			
		}
	}


	// Make tmp a list of all the neighbors of u.
	TArray <TVertex*> Tmp;

	for( i=0; i<u->Verts.Num(); i++ ) 
	{
		Tmp.AddItem(u->Verts(i));
	}

	// Delete triangles on edge uv:
	for( i=u->Faces.Num()-1; i>=0; i--) 
	{
		if( u->Faces(i)->HasVertex(v) ) 
		{
			delete(u->Faces(i));
		}
	}

	// Update remaining triangles to have v instead of u.
	for( i=u->Faces.Num()-1; i>=0; i-- ) 
	{
		u->Faces(i)->ReplaceVertex(u,v);
	}
	delete u; 


	// Recompute the edge collapse costs for neighboring vertices.
	for( i=0; i<Tmp.Num(); i++ ) 
	{
		ComputeEdgeCostAtVertex( Tmp(i) );
	}
	
	unguard;
}



// Find the edge that if collapsed will affect model the least.
// Returns the second vertex of the edge (collapse candidate) 
// as stored in the first vertex' data.	

TVertex *MinimumCostEdge()
{
	guard(MinimumCostEdge);
	
	TVertex *MinCostVtx = TModel.GVerts(0);

	for( INT i=0; i<TModel.GVerts.Num(); i++) 
	{
		if( TModel.GVerts(i)->MergeCost < MinCostVtx->MergeCost) 
		{
			MinCostVtx = TModel.GVerts(i);
		}
	}
	return MinCostVtx;
	unguard;
}


/////////////////////////////////////////////////////////////////////
//
// Build the Level-Of-Detail collapse sequences.
//
/////////////////////////////////////////////////////////////////////

void UEditorEngine::meshVertLODProcess( UVertMesh* Mesh, TArray<FMeshTri>& RawTris, FLODProcessInfo* LODInfo)
{
	guard(UEditorEngine::meshVertLODProcess);

	FMemMark Mark(GMem);

	NOTE( GWarn->Logf( NAME_Log, TEXT("Vertex-animated mesh LOD processing: %s"), Mesh->GetName() ); )

	// The global temporary collapsible helper mesh is your friend.
	TModel.Reset();

	LODStyle = LODInfo->Style;

	// Build the materials, and vertices-with-UV list.
	// Each unique Wedge has unique UV, unique 3d vertex index, and
	// a unique material index, thought that last one is iplicitly
	// inherited from the Face that points to it.

	TArray<INT> TempMatArray;

	Mesh->MeshMaterials.Empty();

	// if No UV data required, erase it here..
	if( LODInfo->NoUVData )
	{
		for ( INT t=0; t< RawTris.Num(); t++)
		{
			RawTris(t).Tex[0].U = 0;
			RawTris(t).Tex[0].V = 0;
			RawTris(t).Tex[1].U = 0;
			RawTris(t).Tex[1].V = 0;
			RawTris(t).Tex[2].U = 0;
			RawTris(t).Tex[2].V = 0;
		}
	}

	// Duplicate TModel vertices for sampling. Use frame # SampleFrame.
	INT FrameOffset = Mesh->FrameVerts * LODInfo->SampleFrame;
	for( INT t=0; t< Mesh->FrameVerts; t++ )
	{
		new TVertex( Mesh->Verts(t + FrameOffset).Vector(), t, 0 );
	}   


	// Go backwards so we can immediately delete any special-coordinate faces.
	for(INT p=RawTris.Num()-1; p>=0; p-- )
	{
		// Always exclude 'special coordinates': meaning N triangles with the invisible
		// flag set, and record their vertices.
		// Count number of 'special coordinates' tri's, skip these faces for LOD.

		if( ( RawTris(p).PolyFlags & PF_Invisible ) ) 
		{
			// Accumulate unique vertices.
			// Store the invisible faces directly with local vertex indices (no wedges.)
			
			// 'Special-face' stuff: removed for UW.
		
			NOTE( debugf(TEXT(" WARNING- weapon-poly vertices no longer supported.")); )

			NOTE( debugf(TEXT(" Weapon orig verts are: %i %i %i tri-index: %i"), RawTris(p).iVertex[0], RawTris(p).iVertex[1], RawTris(p).iVertex[2],p);)
			
			RawTris.Remove(p);
			continue;
		}
		// Mark these vertices as being used by a visible triangle.
		TModel.GVerts(RawTris(p).iVertex[0])->Flag = 0xFFFF;
		TModel.GVerts(RawTris(p).iVertex[1])->Flag = 0xFFFF;
		TModel.GVerts(RawTris(p).iVertex[2])->Flag = 0xFFFF;	
	}

	// Materials building.
	// A material is any unique combination of a texture and the flags.
	for( INT p=0; p<RawTris.Num(); p++ )
	{
		UBOOL Unique = true;
		INT ThisMatIndex = 0;
		FMeshFace NewFace;
	

		// Test for unique materials.
		for( INT m=0; m<Mesh->MeshMaterials.Num(); m++ )
		{
			if(  ( Mesh->MeshMaterials(m).PolyFlags == RawTris(p).PolyFlags )
			   &&( Mesh->MeshMaterials(m).MaterialIndex == RawTris(p).MaterialIndex) )
			{
				ThisMatIndex = m;
				Unique = false;
				break;
			}
		}

		//NOTE( debugf(TEXT("MATERIAL for triangle %i is %i, textureindex %i"),p,m,RawTris(p).TextureIndex);)

		// Add new material.
		if( Unique )
		{
			ThisMatIndex = Mesh->MeshMaterials.Num();
			FMeshMaterial NewStuff;
			NewStuff.PolyFlags    = RawTris(p).PolyFlags;
			NewStuff.MaterialIndex = RawTris(p).MaterialIndex;
			Mesh->MeshMaterials.AddItem(NewStuff);
		}

		NOTE( debugf(TEXT("NEW face added, material index: %i, Materialtotal: %i, Polyflags: %i, TextureIndex: %i"),
			    ThisMatIndex,Mesh->MeshMaterials.Num(),Mesh->MeshMaterials(Mesh->MeshMaterials.Num()-1).PolyFlags, Mesh->MeshMaterials(Mesh->MeshMaterials.Num()-1).TextureIndex );)

		// Add new Face, no vertex indices yet.
		NewFace.MeshMaterialIndex = ThisMatIndex; 

		TModel.FaceLevel.AddItem(0xFFFF);    // The LOD bound.
		Mesh->Faces.AddItem(NewFace);		 // Note the Faces array now precisely mirrors the Tri's.
	}


	// Record Wedges.
	Mesh->Wedges.Empty();
	for( INT p=0; p<RawTris.Num(); p++ )
	{
		for( INT t=0; t<3; t++)
		{
			// Accumulate unique textured vertices, and 
			// create a vertex->Wedge remapping array?
			FMeshWedge NewWedge;

			UBOOL Unique = true;
			// Test for unique Wedges.
			for( INT v=0; v<Mesh->Wedges.Num(); v++ )
			{
				if( ( Mesh->Wedges(v).iVertex == RawTris(p).iVertex[t] )
					&&
					( Mesh->Wedges(v).TexUV.U == RawTris(p).Tex[t].U ) 
					&&
					( Mesh->Wedges(v).TexUV.V == RawTris(p).Tex[t].V ) 
					&&
					( TempMatArray(v) == Mesh->Faces(p).MeshMaterialIndex )
					)
				{
					// Now point a Face's iVertex INT o the already existing Wedge.
					Mesh->Faces(p).iWedge[t] = v;
					Unique = false;
					break;					
				}
			}

			if( Unique ) // Add new Wedge: unique UV, Material, and vertex index.
			// Still, should be significantly lower than the number of Faces.
			{
				NewWedge.iVertex   = RawTris(p).iVertex[t];
				NewWedge.TexUV.U   = RawTris(p).Tex[t].U;
				NewWedge.TexUV.V   = RawTris(p).Tex[t].V;

				// New unique textured vertex.
				Mesh->Wedges.AddItem( NewWedge );
				// Temporary associated material for that Wedge
				TempMatArray.AddItem( Mesh->Faces(p).MeshMaterialIndex );

				// now point a Face's iVertex INT o the new Wedge.
				Mesh->Faces(p).iWedge[t] = Mesh->Wedges.Num() - 1;
			}			
		}
	}

	TempMatArray.Empty();



	// Full vertex set for LOD now known.
	INT TrueFrameVertNum = TModel.GVerts.Num();
	// Backup original indices.
	for( INT t=0; t<TModel.GVerts.Num(); t++)
	{
		TModel.GOriginalVertIdx.AddItem(TModel.GVerts(t)->OriginalIndex);
		TModel.GVerts(t)->OriginalIndex = t; // Create permutation using CURRENT index set.
	}

	// From now on we'll use only our TModel.GVerts array, and only at the
	// end we'll remap the vertices in our actual animation frames.

	NOTE(debugf(TEXT("## Tris Verts Faces Wedges Materials Textures FaceLevels:  %i %i %i  %i %i %i  %i"),RawTris.Num(),Mesh->FrameVerts,Mesh->Faces.Num(),Mesh->Wedges.Num(),Mesh->MeshMaterials.Num(),Mesh->Textures.Num(),TModel.FaceLevel.Num());)

	TModel.GWedges.Empty();
	TModel.GFaces.Empty();

	// Fill more of the TModel.
	
	// Wedges
	for( INT t=0; t<Mesh->Wedges.Num(); t++ )
	{
		TWedge NewWedge;
		// NewWedge.MaterialID
		NewWedge.U = Mesh->Wedges(t).TexUV.U;
		NewWedge.V = Mesh->Wedges(t).TexUV.V;
		NewWedge.VertexIndex = Mesh->Wedges(t).iVertex; 
		NewWedge.DestinationIndex = -1;
		NewWedge.OriginalIndex = t;
		NewWedge.StatusFlags = 0;

		NOTE(debugf(TEXT("# Wedges setup:  Wedge %i  Vertex  %i  Via TModel: %i"),t, NewWedge.VertexIndex, TModel.GVerts(Mesh->Wedges(t).iVertex)->OriginalIndex  ));

		TModel.GWedges.AddItem(NewWedge); 
	}
	
	// Faces, Wedge materials.
	for( INT t=0; t<Mesh->Faces.Num(); t++)
	{
		TWedge* TV[3];
		for( int j=0; j<3; j++ )
		{
			TV[j] = &TModel.GWedges( Mesh->Faces(t).iWedge[j] );
		}
		new TFace( Mesh->Faces(t).MeshMaterialIndex, TV[0], TV[1], TV[2], t );
	}

	NOTE(debugf(TEXT("# TModel vertices: %i  Triangles: %i Wedges+AuxVerts: %i "),TModel.GVerts.Num(),TModel.GFaces.Num(),TModel.GWedges.Num()+TModel.GAuxWedges.Num()));

	// Copy materials to TModel array.
	for( INT m=0; m<Mesh->MeshMaterials.Num(); m++ )
	{
		TModel.Materials.AddItem(Mesh->MeshMaterials(m));
	}

	// Precompute all the collapse costs.
	for( INT i=0; i<TModel.GVerts.Num(); i++ ) 
	{
		ComputeEdgeCostAtVertex( TModel.GVerts(i) );
	}

	// The permutation array.
	TArray<INT> Permutation;
	Permutation.AddZeroed(TrueFrameVertNum);

	// Reduce the object down to no triangles.
	while( TModel.GVerts.Num() > 0) 
	{
		NOTE(debugf(TEXT(" #reducing mesh - vertices: %i  Triangles: %i Wedges: %i "),TModel.GVerts.Num(),TModel.GFaces.Num(),TModel.GWedges.Num()));
		// Get the next vertex to collapse.
		TVertex *mn = MinimumCostEdge();
		// Keep track of this vertex, i.e. the collapse ordering.
		Permutation(mn->OriginalIndex)= TModel.GVerts.Num()-1;
		// Collapse this edge.
		Collapse(mn,mn->ColTarget);
	}

	NOTE(debugf(TEXT("##reduced mesh - vertices: %i  Triangles: %i Wedges: %i "),TModel.GVerts.Num(),TModel.GFaces.Num(),TModel.GWedges.Num()+TModel.GAuxWedges.Num()));

	// Debugging
	for ( INT t=0; t<Permutation.Num(); t++ )
	{
		NOTE( debugf(TEXT(" Permutation # %5i    %5i "), t, Permutation(t) );)
	}

	//  Vertex collapse list preparation .
	NOTE( debugf(TEXT(" Permutation array size: %i"),Permutation.Num());)

	for (INT v=0; v<Mesh->FrameVerts; v++)
	{
		NOTE(debugf(TEXT(" Vertex: %4i  XYZ: %5i %5i %5i"), v, Mesh->Verts(v).X, Mesh->Verts(v).Y, Mesh->Verts(v).Z );)
	}

	// Move all our extra wedge copies over to the full GWedges list.
	for( INT w=0; w<TModel.GAuxWedges.Num(); w++ )
	{
		TModel.GWedges.AddItem(TModel.GAuxWedges(w));
	}
	TModel.GAuxWedges.Empty();

	// Reorder all vertex indices in the Wedges accordingly, and prepare its internal resort array.
	TWedgeSort* WedgePermutation = New<TWedgeSort>(GMem,TModel.GWedges.Num());

	for( INT w=0; w<TModel.GWedges.Num(); w++ )
	{
		// Permutate the vertices inside all our wedges.
		TModel.GWedges(w).VertexIndex = Permutation(TModel.GWedges(w).VertexIndex);

		WedgePermutation[w].SortedWedgeIndex = w;
		WedgePermutation[w].Wedge = TModel.GWedges(w);
	}
	// Sort entire Wedges array: by vertex (collapse) index, secondary by UVs.
	appQsort( WedgePermutation, TModel.GWedges.Num(), sizeof(WedgePermutation[0]),(QSORT_COMPARE)CompareWedgeKey);

	// Wedge array: how to trace the collapse sequence *trees* and build:
	// - The collapse links list.
	// - A sorted wedge list & update the faces accordingly.
	// Set up a special permutation map needed to remap all Wedge indices inside the faces
	// to only their highest LOD-level wedge. WedgePermutation needed (has OriginalIndices)

	// Build our final Wedge collapse list - !!!!!!!!! use .OriginalIndex....
	// Mesh->CollapseWedgeThus.Add(TModel.GWedges.Num());  // The collapse list
	NOTE( debugf(TEXT("Applying the permutation..") ));
	
	// Apply the sorted WedgePermutation.
	TArray<TWedge>  WorkWedges;
	for( INT w=0; w<TModel.GWedges.Num(); w++ )
	{
		INT PermIdx = WedgePermutation[w].SortedWedgeIndex;
		WorkWedges.AddItem( TModel.GWedges(PermIdx) );

		NOTE(debugf(TEXT(" WorkWedge: # %4i Wedge UV %3i %3i   Vertex: %4i Origindex: %4i "),w,WorkWedges(w).U,WorkWedges(w).V,WorkWedges(w).VertexIndex,WorkWedges(w).OriginalIndex ));
	}
	TModel.GWedges.Empty();
	
	// Digest duplicate wedges, create the wedge remap list, and the wedge LOD collapse list.
	TArray<INT> WedgeRemapList;
	TArray<INT> WedgeRemapToUnique;
	TArray<TWedge> UniqueWedges;

	WedgeRemapList.Add(WorkWedges.Num()); 

	for ( INT w=0; w<WorkWedges.Num(); w++)
	{
		// Build the condensed version.
		if ( w==0 )
		{
			UniqueWedges.AddItem(WorkWedges(0));
		}
		else
		{
			// Only store if UV or 3d Vertex differ.
			// Any non-unique wedges will be adjacent ( due to the sorting process.)
			if(   ( WorkWedges(w).VertexIndex != WorkWedges(w-1).VertexIndex ) 
				||( WorkWedges(w).MaterialID  != WorkWedges(w-1).MaterialID  ) 
				|| ISDIFFERENT_UV( WorkWedges(w).U, WorkWedges(w-1).U )
				|| ISDIFFERENT_UV( WorkWedges(w).V, WorkWedges(w-1).V ) )
				UniqueWedges.AddItem(WorkWedges(w));
		}
		WedgeRemapToUnique.AddItem( UniqueWedges.Num()-1 ); 

		NOTE(debugf(TEXT(" WorkWedge %4i remaps to unique wedge %4i"), w, UniqueWedges.Num()-1 );)

		// Make sure the unique wedge with highest lod gets used for the WedgeRemapList ???
		WedgeRemapList( WorkWedges(w).OriginalIndex ) = UniqueWedges.Num()-1;

		NOTE(debugf(TEXT(" OriginalIndex of WorkWedge %4i remaps to unique wedge %4i"), WorkWedges(w).OriginalIndex, UniqueWedges.Num()-1 );)
	}

	NOTE(debugf(TEXT("## Wedges after condensation: %4i "),UniqueWedges.Num()));

	TArray<INT> WedgeFlagger;
	WedgeFlagger.Add(WorkWedges.Num());
	for ( INT w=0; w<WorkWedges.Num(); w++) 
	{
		WedgeFlagger(w)=-1;
	}
	
	//
	// Create the wedge LOD collapse list. Mesh->CollapseWedgeThus...
	//
	Mesh->CollapseWedgeThus.Add(UniqueWedges.Num());
	for ( INT w=0; w<UniqueWedges.Num(); w++) 
	{
		Mesh->CollapseWedgeThus(w)= 0; //#SKEL - infinite-loop bug in renderer: this _should_ work with =w instead of =0; collapses in these cases should never be encountered.
	}
	for( INT w=0; w<WorkWedges.Num(); w++)
	{
		INT WOrig = WorkWedges(w).OriginalIndex;  
		INT UIndex = WedgeRemapToUnique(w);
		INT WFOld = WedgeFlagger(WOrig);
		WedgeFlagger(WOrig) = UIndex; // Get current position into Unique.
		// Store Unique (but not highest-LOD) vertex destination for any Wedge.
		if (WFOld != -1) // An original index already encountered: try and make a backlink.
		{
			 NOTE(debugf(TEXT("CCWorkwedge %4i  WRTU(W) %4i  WFOldbacklink:  %4i , OriginalIndex %4i, Old CWT[worigdest] %4i "),w,WedgeRemapToUnique(w), WFOld,WorkWedges(w).OriginalIndex, Mesh->CollapseWedgeThus(UIndex) );)
				 Mesh->CollapseWedgeThus(UIndex) = WFOld;  // put into collapse at current unique pos.		
		}
	}


	//
	// Move final wedges to the Mesh->Wedges wedge array.
	//
	Mesh->Wedges.Empty();
	Mesh->Wedges.Add(UniqueWedges.Num());
	for( INT w=0; w<UniqueWedges.Num(); w++ )
	{
		Mesh->Wedges(w).TexUV.U = UniqueWedges(w).U;
		Mesh->Wedges(w).TexUV.V = UniqueWedges(w).V;
		Mesh->Wedges(w).iVertex = UniqueWedges(w).VertexIndex;

		NOTE(debugf(TEXT(" Finalwedge: # %4i Wedge UV %3i %3i   Vertex: %4i Origindex: %4i "),w,UniqueWedges(w).U,UniqueWedges(w).V,UniqueWedges(w).VertexIndex,UniqueWedges(w).OriginalIndex ));  
	}

	//for ( w=0; w<UniqueWedges.Num(); w++) 
	//{
	//	 NOTE(debugf(TEXT(" CollapseWedgeThus ( %4i ) = %4i , vertex: %i"), w, Mesh->CollapseWedgeThus(w),UniqueWedges(w).VertexIndex ););
	//}

	//
	// Remap wedges inside the Mesh's Tri's.
	//
	for( INT f=(Mesh->Faces.Num()-1); f>=0; f--)
	{
		// Indicates a triangle that didn't get processed because of 2-3 identical verts.
		if( TModel.FaceLevel(f) == 0xFFFF )
		{
			NOTE(debugf(TEXT("%% RRemoving face number %i which has wedges %i %i %i"),f,Mesh->Faces(f).iWedge[0],Mesh->Faces(f).iWedge[1],Mesh->Faces(f).iWedge[2]);)
			Mesh->Faces.Remove(f); 
			TModel.FaceLevel.Remove(f);
			
		}
		else
		{
			Mesh->Faces(f).iWedge[0] = WedgeRemapList( Mesh->Faces(f).iWedge[0]);
			Mesh->Faces(f).iWedge[1] = WedgeRemapList( Mesh->Faces(f).iWedge[1]);
			Mesh->Faces(f).iWedge[2] = WedgeRemapList( Mesh->Faces(f).iWedge[2]);
		}				
	}

	// Copy TModel's Facelevel array over to Mesh
	for( INT f=0; f<TModel.FaceLevel.Num(); f++)
	{
		Mesh->FaceLevel.AddItem(TModel.FaceLevel(f));
	}

	NOTE(debugf(TEXT("## Tris Verts Faces Wedges Materials Textures FaceLevels:  %i %i %i  %i %i %i  %i"),RawTris.Num(),Mesh->FrameVerts,Mesh->Faces.Num(),Mesh->Wedges.Num(),Mesh->MeshMaterials.Num(),Mesh->Textures.Num(),Mesh->FaceLevel.Num()));

	//
	// Finally : sort vertices throughout all the animation frames,
	// Needs to be done both with Permutation and also with the OriginalIndex parts
	// of TModel.GVerts.
	//

	NOTE(debugf(TEXT("## FrameVertNum %i   TrueFrameVertNum %i "),Mesh->FrameVerts, TrueFrameVertNum);)
	
	INT FullFrameVertNum = TrueFrameVertNum; // + SpecialCoordVerts.Num();

	TArray<INT> FullRemap; 
	FullRemap.Add(FullFrameVertNum);

	guard(Bigremap);
	
	for( INT t=0; t<TrueFrameVertNum; t++)
	{
		// Map all regular used verts to their sorted position after the special ones.
		FullRemap(Permutation(t)) =  TModel.GOriginalVertIdx(t);  
	}
	
	// Big new array.
	TArray<FMeshVert> NewVerts;
	NewVerts.Add( FullFrameVertNum * Mesh->AnimFrames);

	for( INT f=0; f<Mesh->AnimFrames; f++ )
	{
		NOTE(debugf(TEXT(" fullremap AnimFrames %i "),f);)
		INT FrameStart = f * Mesh->FrameVerts;
		INT FullFrameStart = f * FullFrameVertNum;
		// Copy permutated.
		for( INT v=0; v<FullFrameVertNum; v++ )
		{
			NewVerts(FullFrameStart + v) = Mesh->Verts( FullRemap(v) + FrameStart); // If necessary this duplicates verts also.
		}
	}
	
	NOTE(debugf(TEXT("## FrameVertNum %i   TrueFrameVertNum %i "),Mesh->FrameVerts, TrueFrameVertNum);)

	// Kludgy array exchange - maybe cool to have the =operator: Mesh->Verts = NewVerts ?
	Mesh->Verts.Empty();
	Mesh->Verts.Add(NewVerts.Num());
	ExchangeArray(Mesh->Verts, NewVerts);
	NewVerts.Empty();


	// BUT we now also have to update the indices inside the Tri's.....
	// even though they're not used for the LODMesh rendering any more, they're used
	// for the texture LOD gauging !
	TArray<INT> UnFullRemap; 
	UnFullRemap.Add(Mesh->FrameVerts);
	for( INT t=0; t<FullFrameVertNum; t++)
	{
		UnFullRemap(FullRemap(t))= t;
	}
	for( INT t=0; t<RawTris.Num(); t++)
	{
		RawTris(t).iVertex[0] = UnFullRemap( RawTris(t).iVertex[0] );
		RawTris(t).iVertex[1] = UnFullRemap( RawTris(t).iVertex[1] );
		RawTris(t).iVertex[2] = UnFullRemap( RawTris(t).iVertex[2] );
	}
	// OldFrameVerts is needed in the texture LOD gauging.
	// Mesh->OldFrameVerts = FullFrameVertNum;
	
	Mesh->FrameVerts = FullFrameVertNum;
	Mesh->ModelVerts = TrueFrameVertNum;

	TModel.Reset();

	
	//
	// Pre-process the animated, compressed normals.
	//
	TArray<FVector> NewNormals;
	NewNormals.AddZeroed( Mesh->ModelVerts * Mesh->AnimFrames );

	for( INT f=0; f < Mesh->AnimFrames; f++ )
	{
		for( INT t=0; t< Mesh->Faces.Num(); t++)
		{
			FVector V[3];
			INT Idx[3];
			// Accumulate normals over faces
			for( INT i=0; i<3; i++)
			{
				// Get single vertex 
				Idx[i] = Mesh->Wedges(Mesh->Faces(t).iWedge[i]).iVertex + f * Mesh->ModelVerts;		
				FMeshVert MeshVertex = Mesh->Verts( Idx[i] );
				V[i]= FVector( MeshVertex.X, MeshVertex.Y, MeshVertex.Z );
			}				
			FVector FaceNormal = (V[0]-V[1]) ^ (V[0]- V[2]);
			FaceNormal *= 1.0f/ appSqrt(FaceNormal.SizeSquared()+0.0001f);
			// Accumulate into normals of all vertices that make up this face.
			NewNormals(Idx[0]) += FaceNormal;
			NewNormals(Idx[1]) += FaceNormal;
			NewNormals(Idx[2]) += FaceNormal;
		}
	}
	// Normalize, then compress normals into 15-bit FMeshNorms
	for( INT i=0; i<NewNormals.Num(); i++)
	{
		// normalize 
		NewNormals(i) *= 1.0f/ appSqrt(NewNormals(i).SizeSquared()+0.001f);

		//
		// 'un'-normalize: make it stretch to the edges of the -1..1 cube in order
		//  to get more out of quantization.....
		//
		// FLOAT MaxComponent = Max( Max( Abs(NewNormals(i).X),Abs(NewNormals(i).Y)),Abs(NewNormals(i).Z));
		// NewNormals(i) *= 1.0f/MaxComponent;
		//

		DWORD FNew = (DWORD) 
		(
			( (DWORD)(NewNormals(i).Z*511.0f + 512.0f) << 20) +
			( (DWORD)(NewNormals(i).Y*511.0f + 512.0f) << 10) +
			( (DWORD)(NewNormals(i).X*511.0f + 512.0f) << 00)
		);

		Mesh->AnimNormals.AddItem( *(FMeshNorm*)&FNew );
	}
	

	Mark.Pop();
	unguard;

	unguard;
}

/*-----------------------------------------------------------------------------
	Special skeletal skin&bones LOD processing.
	Tasks: 
	Standard mesh LOD sequence determination for the mesh, 
	except specific bone links can now play a part in the level-of-detail bias.
-----------------------------------------------------------------------------*/


QSORT_RETURN CDECL CompareBoneWeight( const TWorkWeight* A, const TWorkWeight* B )
{
	if		( A->PointIndex > B->PointIndex ) return  1;
	else if ( A->PointIndex < B->PointIndex ) return -1;
	else if ( A->BoneIndex  > B->BoneIndex  ) return  1;
	else if ( A->BoneIndex  < B->BoneIndex  ) return -1;
	else                                      return  0;	
}



// #TODO: Sort with translucency drawn last !
QSORT_RETURN CDECL CompareFaces( const VTriangle* A, const VTriangle* B )
{
	if     ( A->SmoothingGroups > B->SmoothingGroups ) return 1;		
	else if( A->SmoothingGroups < B->SmoothingGroups ) return -1;		
	else if( A->MatIndex    > B->MatIndex    ) return 1;
	else if( A->MatIndex    < B->MatIndex    ) return -1;		
	else if( A->AuxMatIndex > B->AuxMatIndex ) return 1;
	else if( A->AuxMatIndex < B->AuxMatIndex ) return -1;
	else if( A->WedgeIndex  > A->WedgeIndex  ) return 1;
	else if( A->WedgeIndex  < A->WedgeIndex  ) return -1;	
	else                                       return 0;
}

// Helper struct to determine vertex uniqueness.
struct InfluencesForVertex
{
	TArray<VRawBoneInfluence> Influences;

	UBOOL operator==( const InfluencesForVertex& OtherInfs ) const
	{	
		if( Influences.Num() != OtherInfs.Influences.Num() )
			return false;

		for( INT i=0; i<Influences.Num(); i++)
		{
			if( ( Influences(i).Weight != OtherInfs.Influences(i).Weight) ||				
				( Influences(i).BoneIndex != OtherInfs.Influences(i).BoneIndex)	)
				return false;
		}
		return true;
	}
};

// Optional welding of vertices.    
INT WeldRawVertices( USkelImport* RawData )
{
	guard( UnMeshLP.cpp::WeldRawVertices);
	INT WeldCount = 0;
	INT MergedInfluences = 0;
	INT ZeroedInfluences = 0;

#define MINIMALBONEWEIGHT (0.001f)

	//Gather weight data for each vertex. Used to distinguish vertices that may share the same 3d position but different bone associations......
	TArray<InfluencesForVertex> VertInfTracker;
	VertInfTracker.AddZeroed( RawData->Points.Num());
	for( INT i=0; i< RawData->Influences.Num(); i++)
	{
		if( ( RawData->Influences(i).Weight > MINIMALBONEWEIGHT ) 
			&&	(RawData->Influences(i).PointIndex < RawData->Points.Num() ) )
		{
			VRawBoneInfluence NewWeight;
			NewWeight.PointIndex = RawData->Influences(i).PointIndex;
			NewWeight.BoneIndex = RawData->Influences(i).BoneIndex;
			NewWeight.Weight = RawData->Influences(i).Weight;
			VertInfTracker( RawData->Influences(i).PointIndex).Influences.AddItem( NewWeight);
		}
	}

	TArray<INT> CollapseArray;
	CollapseArray.Add(RawData->Points.Num());	
	for( INT i=0; i<RawData->Points.Num(); i++)
	{	
		CollapseArray(i)=i;
	}

	INT TotalPoints = RawData->Points.Num();		
	
	// Check duplicates.
	for( INT PointIdx = 0; PointIdx < RawData->Points.Num()-1; PointIdx++)
	{
		if(  CollapseArray(PointIdx) == PointIdx )  //If not already remapped itself...
		{		
			for( INT i=PointIdx+1; i< RawData->Points.Num(); i++)
			{			
				if( CollapseArray(i) ==i )// Not already remapped..
				{								
					if( ( FDistSquared( RawData->Points(PointIdx) , RawData->Points(i)) < Square( 0.0000001f)  )   && // Arbitrarily small welding constant.    //if(  RawData->Points(PointIdx) ==  RawData->Points(i)  ) 
						 VertInfTracker(PointIdx) == VertInfTracker(i)  )  // Check for bone influence correspondence.
					{
						CollapseArray(i)=PointIdx; //Mark it for remapping.  PointIdx always < i.
						WeldCount++;
					}
				}
			}
		}
	}	

	for( INT i=0; i<VertInfTracker.Num(); i++)
	{
		VertInfTracker(i).Influences.Empty();
	}

	TArray<INT> UniquePointsTracking;	
	TArray<INT> FinalPointRemap;
	UniquePointsTracking.Add(TotalPoints);
	FinalPointRemap.Add(TotalPoints);
	for( INT i=0; i<TotalPoints;i++ )
	{
		UniquePointsTracking(i)=i;	
		FinalPointRemap(i)=-1;
	}
		
	// Remove actual remapped points. 
	for( INT PointIdx = TotalPoints-1; PointIdx >= 0; PointIdx--)
	{
		if( CollapseArray(PointIdx) < PointIdx) // Any destination to collapse to ?
		{			
			RawData->Points.Remove( PointIdx);			
			UniquePointsTracking.Remove(PointIdx); // track  remains with only unique points.			
		}
	}

	// Prepare net remapping array.
	for( INT PointIdx = 0; PointIdx < UniquePointsTracking.Num(); PointIdx++)
	{
		FinalPointRemap( UniquePointsTracking(PointIdx)) = PointIdx; // UniquePointsTracking for uncollapsed points only holds the original index.
	}
	
	for( INT PointIdx = 0; PointIdx < CollapseArray.Num(); PointIdx++)
	{
		if( CollapseArray(PointIdx)  < PointIdx ) // All points that actually collapsed.
		{
			INT CollapseDest = FinalPointRemap(  CollapseArray(PointIdx) ); // Remap the collapse destination.   That destination may have shifted but it should definitely exist in FinalPointRemap.
			FinalPointRemap( PointIdx ) = CollapseDest; 
		}		
	}

	// Remap wedge point indices.
	for( INT WedgeIdx = 0; WedgeIdx < RawData->Wedges.Num(); WedgeIdx++)
	{		
		RawData->Wedges(WedgeIdx).PointIndex = FinalPointRemap( RawData->Wedges(WedgeIdx).PointIndex );
	}
	
	// Remap influences.
	for( INT InfluenceIdx = RawData->Influences.Num() -1; InfluenceIdx>=0; InfluenceIdx--)
	{	
		// Clean up all nearly-null bone influences while we're at it...						
		if( RawData->Influences(InfluenceIdx).Weight <= MINIMALBONEWEIGHT )
		{
			RawData->Influences.Remove(InfluenceIdx);
			ZeroedInfluences++;
		}
		else
		{
			RawData->Influences(InfluenceIdx).PointIndex = FinalPointRemap(  RawData->Influences(InfluenceIdx).PointIndex );				
		}
	}	

	// MERGE influences when they share both  POINT+BONE, average the weights together (should be identical!) and delete one.   Rely on later LOD processing code to calibrate weights.
	TArray<INT> MergeWeightInto;
	MergeWeightInto.Add( RawData->Influences.Num());
	for( INT i=0; i<MergeWeightInto.Num(); i++)
	{
		MergeWeightInto(i)=-1;
	}
	for( INT i=0; i<RawData->Influences.Num()-1;i++)
	{
		if( MergeWeightInto(i) == -1 )// unmerged influence.
		for( INT j=i+1; j< RawData->Influences.Num(); j++)
		{
			if( MergeWeightInto(j) == -1) //unmerged influence.
			{	
				if( (RawData->Influences(i).PointIndex == RawData->Influences(j).PointIndex) && (RawData->Influences(i).BoneIndex == RawData->Influences(j).BoneIndex) )
				{
					// Merge weight immediately, and mark influence for deletion. 
					RawData->Influences(i).Weight = 0.5f * ( RawData->Influences(i).Weight + RawData->Influences(j).Weight ); 					
					MergeWeightInto(j)=i;
				}
			}			
		}
	}
	// Delete merged influences. 
	for( INT i=MergeWeightInto.Num()-1; i>=0; i--)
	{
		if( MergeWeightInto(i) != -1 )
		{
			RawData->Influences.Remove(i);
			MergedInfluences++;
		}
	}
	debugf(TEXT(" While vertex welding, %i near-zero bone influences were removed, and %i influences got merged. "),ZeroedInfluences, MergedInfluences);

	// Note that any remaining duplicate wedges (that now share vertices because of the welding) get merged automatically as the LOD models are constructed and coherence-sorted further.
		
	return WeldCount;
	unguard;
}

void UEditorEngine::meshSkelLODProcess( USkeletalMesh* Mesh, FLODProcessInfo* LODInfo, USkelImport* RawData)
{
	guard(UEditorEngine::meshSkelLODProcess);

	FMemMark Mark(GMem); 

	//
	// * Mesh and bones delivered in raw form in USkelImport.
	//
	// * Bone influence on LOD will be through a few conditional tests in the collapse cost
	//   calculation routines, to keep the whole LOD collapse process
	//   compatible with non-skeletal meshes.
	//
	// * Bone influences need LOD sorting for best results also.
	//
	// Input:  old style: 
	// Tris ( Triangles with UV on each corner )
	// Vertices: sample mesh from certain frame's packed 3d points.
	//	
	
	LODStyle = LODInfo->Style;
	debugf(TEXT("Skeletal mesh processing. LODStyle: %i"),LODStyle); 

	// Weld any duplicate vertices first, if requested.
	if( LODInfo->WeldVerts )
	{
		INT WeldCount = WeldRawVertices(RawData);
		debugf(TEXT(" Vertex welding: %i duplicate vertices eliminated."),WeldCount);
	}

	// The global temporary collapsible helper mesh is your friend.
	TModel.Reset();

	// Build the materials, and vertices-with-UV list.
	// Each unique Wedge has unique UV, unique 3d vertex index, and
	// a unique material index, thought that last one is iplicitly
	// inherited from the Face that points to it.

	TArray<INT> TempMatArray;

	Mesh->MeshMaterials.Empty();

	// Erase UV data if not required (entirely envmapped things)
	if( LODInfo->NoUVData )
	{
		for ( INT t=0; t< RawData->Wedges.Num(); t++)
		{
			RawData->Wedges(t).U = 0;
			RawData->Wedges(t).V = 0;
		}
	}

	//
	// Initialize TModel vertices for sampling. Uses reference skin pose only.
	// Fills the TModel.GVerts array automatically. Full FVectors.
	//
	for( INT t=0; t< RawData->Points.Num(); t++ )
	{
		new TVertex( RawData->Points(t), t, 0 );
	}

	NLOG( debugf(TEXT("Raw points %i "), RawData->Points.Num() ));

	// Necessary: Fixup face material index from wedge 0. RawData's faces don't always have the proper material index (exporter's task..)
	{for( INT i=0; i<RawData->Faces.Num(); i++)
	{
		RawData->Faces(i).MatIndex = RawData->Wedges(RawData->Faces(i).WedgeIndex[0]).MatIndex;
		RawData->Faces(i).AuxMatIndex = 0; // Insure unused data doesn't throw off our sorting.
	}}


	//  Mesh Materials
	for( INT t=0; t< RawData->Materials.Num(); t++)
	{
		FMeshMaterial NewStuff;
	
		DWORD RawFlags = RawData->Materials(t).PolyFlags;

		// Set style based on triangle type. Classic flag conversion as in UnMeshEd.cpp
		DWORD PolyFlags=0;
		if     ( (RawFlags&15)==MTT_Normal         ) PolyFlags |= 0;
		else if( (RawFlags&15)==MTT_NormalTwoSided ) PolyFlags |= PF_TwoSided;
		else if( (RawFlags&15)==MTT_Modulate       ) PolyFlags |= PF_TwoSided | PF_Modulated;
		else if( (RawFlags&15)==MTT_Translucent    ) PolyFlags |= PF_TwoSided | PF_Translucent;
		else if( (RawFlags&15)==MTT_Masked         ) PolyFlags |= PF_TwoSided | PF_Masked;
		else if( (RawFlags&15)==MTT_Placeholder    ) PolyFlags |= PF_TwoSided | PF_Invisible;
		// Handle effects.
		if     ( RawFlags&MTT_Unlit             ) PolyFlags |= PF_Unlit;
		if     ( RawFlags&MTT_Flat              ) PolyFlags |= PF_Flat;
		if     ( RawFlags&MTT_Environment       ) PolyFlags |= PF_Environment;
		if     ( RawFlags&MTT_NoSmooth          ) PolyFlags |= PF_NoSmooth;

		// per-pixel Alpha flag ( Reuses Flatness triangle tag and PF_AlphaTexture engine tag...)
		if     ( RawFlags&MTT_Flat				) PolyFlags |= PF_AlphaTexture; 

		NewStuff.PolyFlags    = PolyFlags; 
		NewStuff.MaterialIndex = RawData->Materials(t).TextureIndex;
		NLOG( debugf(TEXT("Raw Material %i texture index %i PolyFlags 0x%15x"),t,RawData->Materials(t).TextureIndex,NewStuff.PolyFlags));

		Mesh->MeshMaterials.AddItem(NewStuff);		
	}

	NLOG( debugf(TEXT("Raw materials %i "), RawData->Materials.Num() ));

	
	// Aid in sorting by spatial coherence: abuse 'smoothinggroups' and AuxMatIndex for translucency and spatial(highest Y refpose coordinate) sorting.	
	{for( INT i=0; i<RawData->Faces.Num(); i++)
	{
		FLOAT LowPoint = 99999.0f;
		for( INT p=0; p<3; p++)
		{
			INT PointIdx = RawData->Wedges(RawData->Faces(i).WedgeIndex[p]).PointIndex;
			LowPoint = Min(LowPoint, -RawData->Points(PointIdx).Z);
		}
		// Abuse the Aux variable to force spatial sorting.
		RawData->Faces(i).AuxMatIndex = appRound(LowPoint*5.0f);
		// Abuse the smoothinggroup variable to force translucency -sorting : any non-occluding material should always be drawn last.
		RawData->Faces(i).SmoothingGroups = (( Mesh->MeshMaterials( RawData->Faces(i).MatIndex ).PolyFlags & PF_NoOcclude ) > 0)? 1:0;
	}}
		
	// Sort raw data mesh faces by material index.
	appQsort( &RawData->Faces(0),RawData->Faces.Num(), sizeof(RawData->Faces(0)), (QSORT_COMPARE)CompareFaces );
	
	// Mesh faces from RawDataFaces ->   Mesh->Faces; also size TModel.FaceLevel.AddItem(0xFFFF) !!
	for( INT t=0; t< RawData->Faces.Num(); t++ )
	{
		FMeshFace NewFace;

		NewFace.iWedge[0] = RawData->Faces(t).WedgeIndex[0];
		NewFace.iWedge[1] = RawData->Faces(t).WedgeIndex[1];
		NewFace.iWedge[2] = RawData->Faces(t).WedgeIndex[2];
		NewFace.MeshMaterialIndex = RawData->Wedges(NewFace.iWedge[0]).MatIndex;

		NLOG(debugf(TEXT("# Making new face nr %i Wedges123 %i %i %i  Matidx %i"),t,NewFace.iWedge[0],NewFace.iWedge[1],NewFace.iWedge[2], NewFace.MaterialIndex ) );

		NOTE( debugf(TEXT("MATERIAL for triangle %i is %i, textureindex %i"),t,NewFace.MaterialIndex);)

		Mesh->Faces.AddItem(NewFace);
		TModel.FaceLevel.AddItem(0xFFFF);
	}   
	
	//
	// Full vertex set for LOD now known.
	//
	INT TrueFrameVertNum = TModel.GVerts.Num();

	// Backup original Point indices = needed to get wedges pointing properly after points
	// are sorted in collapse order.
	for( INT t=0; t<TModel.GVerts.Num(); t++)
	{
		TModel.GOriginalVertIdx.AddItem(TModel.GVerts(t)->OriginalIndex);
		TModel.GVerts(t)->OriginalIndex = t; // Create permutation using CURRENT index set.
	}

	NLOG(debugf(TEXT("# Tmodel.GVerts number: %i \n"),TModel.GVerts.Num() ));

	//
	// From now on we'll use only our TModel.GVerts array. Copied back at the very end.
	//
	NLOG(debugf(TEXT("## Faces Wedges Materials Textures FaceLevels:  %i %i %i  %i %i %i  %i"),Mesh->Faces.Num(),Mesh->Wedges.Num(),Mesh->MeshMaterials.Num(),Mesh->Textures.Num(),TModel.FaceLevel.Num());)

	TModel.GWedges.Empty();
	TModel.GFaces.Empty(); 

	// Fill more of the TModel.
	
	// Wedges
	for( INT t=0; t<RawData->Wedges.Num(); t++ )
	{
		TWedge NewWedge;
		// NewWedge.MaterialID
		NewWedge.U = RawData->Wedges(t).U;
		NewWedge.V = RawData->Wedges(t).V;
		NewWedge.VertexIndex = RawData->Wedges(t).PointIndex;
		NewWedge.DestinationIndex = -1;
		NewWedge.OriginalIndex = t;
		NewWedge.MaterialID = RawData->Wedges(t).MatIndex;

		//NLOG(debugf(TEXT("# Wedges setup:  Wedge %i  Vertex  %i  Via TModel: %i"),t, NewWedge.VertexIndex, TModel.GVerts(Mesh->Wedges(t).iVertex)->OriginalIndex  ));
		TModel.GWedges.AddItem(NewWedge); 
	}
	
	// Faces, Wedge materials.
	for( INT t=0; t<Mesh->Faces.Num(); t++)
	{
		TWedge* TV[3];
		for( int j=0; j<3; j++ )
		{
			TV[j] = &TModel.GWedges( Mesh->Faces(t).iWedge[j] );
  			NLOG(debugf(TEXT("# Making new face nr %i GWedgeidx %i Matidx %i"),t,Mesh->Faces(t).iWedge[j],Mesh->Faces(t).MaterialIndex ) );
		}
		new TFace( Mesh->Faces(t).MeshMaterialIndex, TV[0], TV[1], TV[2], t );
	}

	NLOG(debugf(TEXT("# TModel vertices: %i  Triangles: %i Wedges+AuxVerts: %i "),TModel.GVerts.Num(),TModel.GFaces.Num(),TModel.GWedges.Num()+TModel.GAuxWedges.Num()));

	// Copy materials to TModel array.
	for( INT m=0; m<Mesh->MeshMaterials.Num(); m++ )
	{
		TModel.Materials.AddItem(Mesh->MeshMaterials(m));
	}

	// Precompute all the collapse costs.
	for( INT i=0; i<TModel.GVerts.Num(); i++ ) 
	{
		ComputeEdgeCostAtVertex( TModel.GVerts(i) );
	}

	
	// The permutation array.
	TArray<INT> Permutation;
	Permutation.AddZeroed(TrueFrameVertNum);
	// The collapse list.

	// Reduce the object down to no triangles.
	while( TModel.GVerts.Num() > 0) 
	{
		NLOG(debugf(TEXT(" #reducing mesh - vertices: %i  Triangles: %i Wedges: %i "),TModel.GVerts.Num(),TModel.GFaces.Num(),TModel.GWedges.Num()));
		// Get the next vertex to collapse.
		TVertex *mn = MinimumCostEdge();
		// Keep track of this vertex, i.e. the collapse ordering.
		Permutation(mn->OriginalIndex)= TModel.GVerts.Num()-1;
		// Collapse this edge.
		Collapse(mn,mn->ColTarget);
	}

	NLOG(debugf(TEXT("##reduced mesh - vertices: %i  Triangles: %i Wedges: %i "),TModel.GVerts.Num(),TModel.GFaces.Num(),TModel.GWedges.Num()+TModel.GAuxWedges.Num()));

	// Debugging
	for ( INT t=0; t<Permutation.Num(); t++ )
	{
		NLOG(debugf(TEXT(" Permutation # %5i    %5i "), t, Permutation(t) );)
	}

	//  Vertex collapse list preparation .
	NLOG(debugf(TEXT(" Permutation array size: %i"),Permutation.Num());)

	// Move all our extra wedge copies over to the full GWedges list.
	for( INT w=0; w<TModel.GAuxWedges.Num(); w++ )
	{
		TModel.GWedges.AddItem(TModel.GAuxWedges(w));
	}
	TModel.GAuxWedges.Empty();

	// Reorder all vertex indices in the Wedges accordingly, and prepare its internal resort array.
	TWedgeSort* WedgePermutation = New<TWedgeSort>(GMem,TModel.GWedges.Num());

	for( INT w=0; w<TModel.GWedges.Num(); w++ )
	{
		// Permutate the vertices inside all our wedges.
		TModel.GWedges(w).VertexIndex = Permutation(TModel.GWedges(w).VertexIndex);

		WedgePermutation[w].SortedWedgeIndex = w;		
		WedgePermutation[w].Wedge = TModel.GWedges(w);												  
	}
	// Sort entire Wedges array: by vertex (collapse) index, secondary by original index.
	appQsort( WedgePermutation, TModel.GWedges.Num(), sizeof(WedgePermutation[0]),(QSORT_COMPARE)CompareWedgeKey);

	// Wedge array: how to trace the collapse sequence *trees* and build:
	// - The collapse links list.
	// - A sorted wedge list & update the faces accordingly.
	// Set up a special permutation map needed to remap all Wedge indices inside the faces
	// to only their highest LOD-level wedge. WedgePermutation needed (has OriginalIndices)

	// Build our final Wedge collapse list - !!!!!!!!! use .OriginalIndex....
	// Mesh->CollapseWedgeThus.Add(TModel.GWedges.Num());  // The collapse list
	NLOG(debugf(TEXT("Applying the permutation..")) );

	
	// Apply the sorted WedgePermutation.
	TArray<TWedge>  WorkWedges;
	for( INT w=0; w<TModel.GWedges.Num(); w++ )
	{
		INT PermIdx = WedgePermutation[w].SortedWedgeIndex;
		WorkWedges.AddItem( TModel.GWedges(PermIdx) );

		NLOG(debugf(TEXT(" WorkWedge: # %4i Wedge UV %3i %3i   Vertex: %4i Origindex: %4i "),w,WorkWedges(w).U,WorkWedges(w).V,WorkWedges(w).VertexIndex,WorkWedges(w).OriginalIndex ));
	}
	TModel.GWedges.Empty();
	
	// Digest duplicate wedges, create the wedge remap list, and the wedge LOD collapse list.
	TArray<INT> WedgeRemapList;
	TArray<INT> WedgeRemapToUnique;
	TArray<TWedge> UniqueWedges;

	WedgeRemapList.Add(WorkWedges.Num()); 

	for ( INT w=0; w<WorkWedges.Num(); w++)
	{
		// Build the condensed version.
		if ( w==0 )
		{
			UniqueWedges.AddItem(WorkWedges(w));
		}
		else
		{
			// Only store if UV or 3d Vertex differ.
			// Any non-unique wedges will be adjacent ( due to the sorting process.)
			if(   ( WorkWedges(w).VertexIndex != WorkWedges(w-1).VertexIndex ) 
				||( WorkWedges(w).MaterialID  != WorkWedges(w-1).MaterialID  ) 
				|| ISDIFFERENT_UV( WorkWedges(w).U, WorkWedges(w-1).U )  
				|| ISDIFFERENT_UV( WorkWedges(w).V, WorkWedges(w-1).V )  
				)
				UniqueWedges.AddItem(WorkWedges(w));			
		}
		WedgeRemapToUnique.AddItem( UniqueWedges.Num()-1 ); 

		NLOG(debugf(TEXT(" WorkWedge %4i remaps to unique wedge %4i"), w, UniqueWedges.Num()-1 );)

		// Make sure the unique wedge with highest lod gets used for the WedgeRemapList ???
		WedgeRemapList( WorkWedges(w).OriginalIndex ) = UniqueWedges.Num()-1;

		NLOG(debugf(TEXT(" OriginalIndex of WorkWedge %4i remaps to unique wedge %4i"), WorkWedges(w).OriginalIndex, UniqueWedges.Num()-1 );)
	}

	NLOG(debugf(TEXT("## Wedges after condensation: %4i "),UniqueWedges.Num()));

	TArray<INT> WedgeFlagger;
	WedgeFlagger.Add(WorkWedges.Num());
	for ( INT w=0; w<WorkWedges.Num(); w++) 
	{
		WedgeFlagger(w)=-1;
	}
	
	// Create the wedge LOD collapse list. Mesh->CollapseWedgeThus...
	Mesh->CollapseWedgeThus.Add(UniqueWedges.Num());
	for ( INT w=0; w<UniqueWedges.Num(); w++) 
	{
		Mesh->CollapseWedgeThus(w)= 65535; 
	}
	for( INT w=0; w<WorkWedges.Num(); w++)
	{
		INT WOrig = WorkWedges(w).OriginalIndex;
		INT UIndex = WedgeRemapToUnique(w);
		INT WFOld = WedgeFlagger(WOrig);
		WedgeFlagger(WOrig) = UIndex; // Get current position into Unique.
		// Store Unique (but not highest-LOD) vertex destination for any Wedge.
		if (WFOld != -1) // An original index already encountered: try and make a backlink.
		{
			 NLOG(debugf(TEXT("CCWorkwedge %4i  WRTU(W) %4i  WFOldbacklink:  %4i , OriginalIndex %4i, Old CWT[worigdest] %4i "),w,WedgeRemapToUnique(w), WFOld,WorkWedges(w).OriginalIndex, Mesh->CollapseWedgeThus(UIndex) );)
			if( Mesh->CollapseWedgeThus(UIndex) == 65535 )
			{
				 Mesh->CollapseWedgeThus(UIndex) = WFOld;  // put into collapse at current unique pos.		
			}		
		}
	}

	// Materials-index list ( necessary for mesh-sections determination below.. )
	TArray<INT> WedgeMaterials;

	// Move final wedges to the Mesh->Wedges wedge array.
	Mesh->Wedges.Empty();
	Mesh->Wedges.Add(UniqueWedges.Num());
	for( INT w=0; w<UniqueWedges.Num(); w++ )
	{
		Mesh->Wedges(w).TexUV.U = UniqueWedges(w).U;
		Mesh->Wedges(w).TexUV.V = UniqueWedges(w).V;
		Mesh->Wedges(w).iVertex = UniqueWedges(w).VertexIndex;
		WedgeMaterials.AddItem(UniqueWedges(w).MaterialID);
		
		NLOG( debugf(TEXT(" Finalwedge: # %4i Wedge UV %3f %3f   Vertex: %4i Origindex: %4i matId %4i "),w,UniqueWedges(w).U,UniqueWedges(w).V,UniqueWedges(w).VertexIndex,UniqueWedges(w).OriginalIndex,UniqueWedges(w).MaterialID );)
	}
		
	// Remap wedges inside the Mesh's Tri's.
	for( INT f=(Mesh->Faces.Num()-1); f>=0; f--)
	{
		// Indicates a triangle that didn't get processed because of 2-3 identical verts.
		if( TModel.FaceLevel(f) == 0xFFFF )
		{
			NLOG(debugf(TEXT("%% RRemoving face number %i which has wedges %i %i %i"),f,Mesh->Faces(f).iWedge[0],Mesh->Faces(f).iWedge[1],Mesh->Faces(f).iWedge[2]);)
			Mesh->Faces.Remove(f); 
			TModel.FaceLevel.Remove(f);	
		}
		else
		{
			Mesh->Faces(f).iWedge[0] = WedgeRemapList( Mesh->Faces(f).iWedge[0]);
			Mesh->Faces(f).iWedge[1] = WedgeRemapList( Mesh->Faces(f).iWedge[1]);
			Mesh->Faces(f).iWedge[2] = WedgeRemapList( Mesh->Faces(f).iWedge[2]);
		}				
	}

	// Copy TModel's Facelevel array over to Mesh
	for( INT f=0; f<TModel.FaceLevel.Num(); f++)
	{
		Mesh->FaceLevel.AddItem(TModel.FaceLevel(f));		
	}

	NLOG(debugf(TEXT("## Faces Wedges Materials Textures FaceLevels:  %i %i %i %i %i"),Mesh->Faces.Num(),Mesh->Wedges.Num(),Mesh->MeshMaterials.Num(),Mesh->Textures.Num(),Mesh->FaceLevel.Num()));

	// 
	// Finally : reorder vertices ; FullRemap is the inverted Permutation...
	// 

	TArray<INT> FullRemap; 
	FullRemap.Add(TrueFrameVertNum);

	for( INT t=0; t<TrueFrameVertNum; t++)
	{
		// Map all regular used verts to their sorted position 
		NLOG(debugf(TEXT(" # %5i FR DestIndex %5i  NewIn %5i "),t, Permutation(t),TModel.GOriginalVertIdx(t));)
		FullRemap(Permutation(t)) =  TModel.GOriginalVertIdx(t); 
		NLOG(debugf(TEXT(" Fullremap index %5i  content %5i "),t, FullRemap(t));)
	}

	NLOG(debugf(TEXT("Start rawdata remap - Number: %i"),RawData->Influences.Num()));

	// Remap  point indices inside raw bone influences also...
	for( INT t=0; t<RawData->Influences.Num(); t++)
	{
		RawData->Influences(t).PointIndex = Permutation( RawData->Influences(t).PointIndex );
		
	}

	NLOG(debugf(TEXT("End rawdata remap")));

	// Temp new vertex array
	TArray<FVector> NewPoints;

	Mesh->Points.Empty(); 
	Mesh->Points.Add( TrueFrameVertNum );

	guard(1stp);
	// Copy permutated.
	for( INT v=0; v<TrueFrameVertNum; v++ )
	{
		Mesh->Points(v) = RawData->Points( FullRemap(v) ); 
	}
	unguard;

	NLOG(debugf(TEXT("## MeshPoints %i   TrueFrameVertNum %i Rawdatapoints %i "), Mesh->Points.Num(), TrueFrameVertNum, RawData->Points.Num());)

	Mesh->ModelVerts = TrueFrameVertNum;

	TModel.Reset();

	TArray<TWorkWeight> WorkWeights;
	INT AllWeights = 0;
	INT SmallWeights = 0;

	TArray<INT> InfluencesPerPoint;
	InfluencesPerPoint.AddZeroed(Mesh->Points.Num());

	// Accumulate all influences.
	for( INT w=0; w<RawData->Influences.Num(); w++ )
	{	
		FLOAT FWeight  = RawData->Influences(w).Weight;
		// Legal index, nonzero weight ?
		if ( (FLOAT)FWeight >= (MINBONEWEIGHT) )
		{
			TWorkWeight NewWeight;
			NewWeight.Weight = FWeight;
			NewWeight.PointIndex = RawData->Influences(w).PointIndex;	// Points already remapped in collapse order.			
			NewWeight.BoneIndex  = RawData->Influences(w).BoneIndex; 							
			NewWeight.Multiplicity = 0;
			WorkWeights.AddItem(NewWeight);	
			AllWeights++;

			if( NewWeight.PointIndex < InfluencesPerPoint.Num() )
				InfluencesPerPoint(NewWeight.PointIndex)++;
		}		
		else
		{
			SmallWeights++;
		}		
	}
	debugf(TEXT(" Total vertex influences: %i  Discarded weights:%i"),AllWeights,SmallWeights );

	// Ensure all vertices to have at least one weight - add a 0-bone weight if none other present. 
	// Known to happen with certain MAX skeletal setups.
	INT PatchedSceneWeights = 0;	
	for( INT p=0; p<InfluencesPerPoint.Num(); p++)
	{
		if( InfluencesPerPoint(p) == 0 )
		{
			TWorkWeight NewWeight;
			NewWeight.Weight = 1.0f;
			NewWeight.PointIndex = p;	
			NewWeight.BoneIndex  = 0;	
			NewWeight.Multiplicity = 0;
			WorkWeights.AddItem(NewWeight);	
			PatchedSceneWeights++;
		}
	}
    if( PatchedSceneWeights )
		debugf(TEXT(" Patched vertex influences to bone 0: %i "),PatchedSceneWeights );
				
	// Sort influences per point.
	appQsort( &WorkWeights(0), WorkWeights.Num(), sizeof(TWorkWeight),(QSORT_COMPARE)CompareBoneWeight );

	
	// Then collapse the multiby multiplicity into Weights and the MultiBlends array.
	// Possible recalibration done here too.
	INT LastPointIndex = -1;
	// Determine multiplicity in each first weight of a group
	INT LastSameWeight = 0;
	INT MaxMultiple = 0;
	for( INT w=0; w<WorkWeights.Num(); w++)
	{
		INT ThisPointIndex = WorkWeights(w).PointIndex;
		if( LastPointIndex == ThisPointIndex)
		{
			WorkWeights(LastSameWeight).Multiplicity++;
			MaxMultiple = Max(MaxMultiple, WorkWeights(LastSameWeight).Multiplicity );
		}	
		else
		{
			LastSameWeight = w;
			LastPointIndex = ThisPointIndex;
		}
	}
	// Store all arrays. 
	Mesh->MultiBlends.AddZeroed(MaxMultiple+1);
	for( INT m=0; m<=MaxMultiple; m++)
	{
		Mesh->MultiBlends(m).WeightBase = Mesh->Weights.Num();
		// Extract all the 1-influence verts, 2 influence verts etc.
		for(INT w=0; w<WorkWeights.Num(); w++  )
		{
			INT Mult = WorkWeights(w).Multiplicity;
			if( Mult==m )
			{
				
				Mesh->MultiBlends(m).PointIndices.AddItem(WorkWeights(w).PointIndex);

				for(INT i=0; i<=m; i++)
				{
					VBoneInfluence NewInf;
					NewInf.BoneWeight = Clamp( appRound( WorkWeights(w+i).Weight * 65535.f ),0,65535 );
					NewInf.BoneIndex  = WorkWeights(w+i).BoneIndex;
					Mesh->Weights.AddItem( NewInf );
				}
			}
			w+=Mult; // Skip to next PointIndex' weights.
		}
		debugf(TEXT(" [%i]-bone-influenced vertices: %i weightindex: %i total: %i"),m+1,Mesh->MultiBlends(m).PointIndices.Num(),Mesh->MultiBlends(m).WeightBase, Mesh->Weights.Num());
	}
	


	// Setup skeletal hierarchy + names structure.
	Mesh->RefSkeleton.Add( RawData->RefBonesBinary.Num() );

	// Digest bones to the serializable format.
    for( INT b=0; b<RawData->RefBonesBinary.Num(); b++ )
	{
		FMeshBone& Bone = Mesh->RefSkeleton(b);
		// Bone = Mesh->RefSkeleton(b);
		// GWarn->Logf( NAME_Log, TEXT("1") );
		
		Bone.Flags = 0;
		Bone.BonePos.Position =    RawData->RefBonesBinary(b).BonePos.Position;     // FVector - Origin of bone relative to parent, or root-origin.
		Bone.BonePos.Orientation = RawData->RefBonesBinary(b).BonePos.Orientation;  // FQuat - orientation of bone in parent's Trans.
		Bone.BonePos.Length=0;
		Bone.BonePos.XSize=0;
		Bone.BonePos.YSize=0;
		Bone.BonePos.ZSize=0;

		Bone.NumChildren = RawData->RefBonesBinary(b).NumChildren;
		Bone.ParentIndex = RawData->RefBonesBinary(b).ParentIndex;		
		appTrimSpaces( &RawData->RefBonesBinary(b).Name[0] );
		Bone.Name = FName( appFromAnsi(&RawData->RefBonesBinary(b).Name[0]) );		
	}

	// Add hierarchy index to each bone and detect max depth.
	Mesh->SkeletalDepth = 0;
	for( INT b=0; b<Mesh->RefSkeleton.Num(); b++)
	{
		INT Parent = Mesh->RefSkeleton(b).ParentIndex;
		INT Depth = 1.0f;
		Mesh->RefSkeleton(b).Depth = 1.0f;
		if( Parent != b )
		{
			Depth += Mesh->RefSkeleton(Parent).Depth;
		}
		if( Mesh->SkeletalDepth < Depth )
		{
			Mesh->SkeletalDepth = Depth;
		}
		Mesh->RefSkeleton(b).Depth = Depth;
	}
	debugf( TEXT("Bones digested - %i  Depth of hierarchy - %i"), Mesh->RefSkeleton.Num(), Mesh->SkeletalDepth );

	//
	// Convert to 'raw' mesh data (should be done explicitly/directly here as we construct the mesh?) and Activate 0th LOD by default:	
	// 
	Mesh->ReconstructRawMesh();

	//
	// Generate default LOD models unless 'specify' was defined - meaning,
	// LOD's are explicitly created in script (or editor) with Generate calls.	
	// 
	if( ! LODInfo->Specify )
	{		
		debugf(TEXT("Generating standard LOD models for mesh [%s] "),this->GetName());		
		Mesh->GenerateLodModel( 0, 1.00f, 1.00f, 4, true );  
		Mesh->GenerateLodModel( 1, 0.70f, 0.50f, 1, true );
		Mesh->GenerateLodModel( 2, 0.35f, 0.30f, 1, true );
		Mesh->GenerateLodModel( 3, 0.10f, 0.17f, 1, true );
		for( int i=0; i< Mesh->LODModels.Num(); i++)
		{
			//Default hysteresis.. enough to not make it flicker back and forth.
			Mesh->LODModels(i).LODHysteresis = 0.02f; 
		}		
	}		
	
	Mark.Pop();
	unguard; 
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

