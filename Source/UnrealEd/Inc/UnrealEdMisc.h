/*=============================================================================
	UnrealEdMisc.h: Misc UnrealEd helper functions.
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

    Revision history:
		* Created by Jack Porter.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Not sure this is the best place for this yet.
-----------------------------------------------------------------------------*/

enum eBROWSER {
	eBROWSER_TEXTURE	= 0,
	eBROWSER_ACTOR		= 1,
	eBROWSER_MESH		= 2,
	eBROWSER_ANIMATION	= 3,
	eBROWSER_STATICMESH	= 4,
	eBROWSER_PREFAB		= 5,
	eBROWSER_GROUP		= 6,
	eBROWSER_SOUND		= 7,
	eBROWSER_MUSIC		= 8,

#ifdef WITH_LIPSINC
	eBROWSER_LIPSINC	= 9,
	eBROWSER_MAX		= 10
#else
	eBROWSER_MAX		= 9
#endif

};

enum eLASTDIR {
	eLASTDIR_UNR	= 0,
	eLASTDIR_UTX	= 1,
	eLASTDIR_PCX	= 2,
	eLASTDIR_UAX	= 3,
	eLASTDIR_WAV	= 4,
	eLASTDIR_BRUSH	= 5,
	eLASTDIR_2DS	= 6,
	eLASTDIR_UPX	= 7,
	eLASTDIR_T3D	= 8,
	eLASTDIR_USX	= 9,
	eLASTDIR_ASE	= 10,	
	eLASTDIR_UKX    = 11,
	eLASTDIR_PSA    = 12,
	eLASTDIR_PSK    = 13,

#ifdef WITH_LIPSINC
	eLASTDIR_LBP	= 14,
	eLASTDIR_LTF	= 15,
	eLASTDIR_IEX    = 16,
	eLASTDIR_MAX	= 17
#else
	eLASTDIR_MAX	= 14 
#endif

};

extern FString GLastDir[];

#define SAFEDELETENULL(a)\
{\
  if( a )\
  {\
      DestroyWindow (*a);\
      a = NULL;\
  }\
}

/*-----------------------------------------------------------------------------
	FPolyBreaker.
-----------------------------------------------------------------------------*/

//
// Breaks a list of vertices into a set of convex FPolys.  The only requirement
// is the vertices are wound in edge order ... so that each vertex connects to the next.
// It can't be a random pool of vertices.  The winding direction doesn't matter.
//
class FPolyBreaker
{
public:
	TArray<FVector>* PolyVerts;
	FVector PolyNormal;
	TArray<FPoly> FinalPolys;	// The resulting polygons.

	// tor's
	FPolyBreaker();
	~FPolyBreaker();

	// FPolyBreaker interface
	void Process( TArray<FVector>* InPolyVerts, FVector InPolyNormal );
	UBOOL IsPolyConvex( FPoly* InPoly );
	UBOOL IsPolyConvex( TArray<FVector>* InVerts );
	void MakeConvexPoly( TArray<FVector>* InVerts );
	INT TryToMerge( FPoly *Poly1, FPoly *Poly2 );
	// Looks at the resulting polygons and tries to put polys with matching edges
	// together.  This reduces the total number of polys in the final shape.
	void Optimize();
	// Returns 1 if any polys were merged
	UBOOL OptimizeList( TArray<FPoly>* PolyList );
	// This is basically the same function as FPoly::SplitWithPlane, but modified
	// to work with this classes data structures.
	INT SplitWithPlane
	(
		TArray<FVector>			*Vertex,
		int						NumVertices,
		const FVector			&PlaneBase,
		const FVector			&PlaneNormal,
		TArray<FVector>			*FrontPoly,
		TArray<FVector>			*BackPoly
	) const;
};

/*-----------------------------------------------------------------------------
	The end.
-----------------------------------------------------------------------------*/

