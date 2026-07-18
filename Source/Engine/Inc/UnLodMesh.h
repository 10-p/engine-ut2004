/*=============================================================================

	UnLodMesh.h: Unreal Warfare mesh instance / pseudo-abstract base object for LOD meshes
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

    Pseudo-abstract LOD mesh base.


	Internal LODMESH_VERSION update significance :
		1 - Ancestral LOD/Skeletal meshes - start of UKX package usage.
		2 - Skeletal meshes have distinct LOD levels.
		3 - Impostor sprites for meshes.
		4 - Added SkinTesselationFactor.

=============================================================================*/

#define LODMESH_VERSION 4

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

// Forward declarations.
class ULodMesh;
class ULodMeshInstance;

 // LodMesh specific structs  

struct FAnimMeshVertex
{
	FVector	Position,
			Normal;
	FLOAT	U,
			V;

	inline UBOOL operator==(FAnimMeshVertex& InOther)
	{
		if(Position == InOther.Position && Normal == InOther.Normal && U == InOther.U && V == InOther.V)
			return true;
		else
			return false;
	}

	friend FArchive& operator<<(FArchive& Ar,FAnimMeshVertex& V)
	{
		return Ar	<< V.Position
					<< V.Normal
					<< V.U
					<< V.V;
	}
};


/*
	FAnimMeshVertexStream
*/

class ENGINE_API FAnimMeshVertexStream : public FVertexStream
{
public:
	ULodMesh*				    AnimMesh;    

	TArray<FAnimMeshVertex>		Vertices;
	QWORD						CacheId;
	INT							Revision;
	UBOOL                       bPartial;  //LOD may require partial upload
	UBOOL                       bStreamCallback;
	INT                         PartialVerts;   
	
	// Constructor.
	FAnimMeshVertexStream()
	{
		AnimMesh = NULL;
		CacheId = MakeCacheID(CID_RenderVertices);
		Revision = 0;
		bPartial = 0;
		bStreamCallback = 0;
	}

	// GetPartialSize / SetPartialSize
	virtual INT GetPartialSize()
	{
		return bPartial? Min(Vertices.Num(),PartialVerts): Vertices.Num();
	}

	virtual INT SetPartialSize(INT Size)
	{
		if( Size >= 0)
		{
			PartialVerts = Min( Size, Vertices.Num());
			bPartial = true;
			return( PartialVerts );
		}
		else
		{
			PartialVerts = 0;
			bPartial = false;
			return Vertices.Num();
		}
	}


	// GetCacheId

	virtual QWORD GetCacheId()
	{
		return CacheId;
	}

	// GetRevision

	virtual INT GetRevision()
	{
		return Revision;
	}

	// GetSize

	virtual INT GetSize()
	{
		return GetPartialSize() * sizeof(FAnimMeshVertex);
	}

	// GetStride

	virtual INT GetStride()
	{
		return sizeof(FAnimMeshVertex);
	}

	// GetComponents

	virtual INT GetComponents(FVertexComponent* OutComponents)
	{
		OutComponents[0].Type = CT_Float3;
		OutComponents[0].Function = FVF_Position;
		OutComponents[1].Type = CT_Float3;
		OutComponents[1].Function = FVF_Normal;
		OutComponents[2].Type = CT_Float2;
		OutComponents[2].Function = FVF_TexCoord0;

		return 3;
	}

	// GetStreamData
	virtual void GetStreamData(void* Dest)
	{
		appMemcpy(Dest,&Vertices(0),GetPartialSize() * sizeof(FAnimMeshVertex));
	}

	virtual void GetRawStreamData(void ** Dest, INT FirstVertex )
	{
		*Dest = &Vertices(FirstVertex);
	}
	// Serialization.

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FAnimMeshVertexStream& V)
	{
		return Ar << V.Vertices << V.Revision;
	}
};


/*-----------------------------------------------------------------------------
	FMeshVert.
-----------------------------------------------------------------------------*/

// Packed mesh vertex point for vertex-animated meshes.
#define GET_MESHVERT_DWORD(mv) (*(DWORD*)&(mv))
struct FMeshVert
{
	// Variables.
	INT X:11; INT Y:11; INT Z:10;

	// Constructor.
	FMeshVert()
	{}
	FMeshVert( const FVector& In )
	: X((INT)In.X), Y((INT)In.Y), Z((INT)In.Z)
	{}

	// Functions.
	FVector Vector() const
	{
		return FVector( X, Y, Z );
	}

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FMeshVert& V )
	{
		#if __INTEL_BYTE_ORDER__
		return Ar << GET_MESHVERT_DWORD(V);
		#else
		DWORD val = (V.X & 0x7FF) | ((V.Y & 0x7FF) << 11) | ((V.Z & 0x3FF) << 22);
		FArchive &retval = Ar << val;
		V.X = (val >> 00) & 0x7FF;
		V.Y = (val >> 11) & 0x7FF;
		V.Z = (val >> 22) & 0x3FF;
		return retval;
		#endif
	}
};

// Packed mesh vertex normal for vertex-animated meshes.
#define GET_MESHNORM_DWORD(mn) (*(DWORD*)&(mn))
struct FMeshNorm
{
	// Variables.
	unsigned int X:10; unsigned int Y:10; unsigned int Z:10;

	// Constructor.
	FMeshNorm()
	{}
	FMeshNorm( const FVector& In )
	: X((unsigned int)In.X), Y((unsigned int)In.Y), Z((unsigned int)In.Z)
	{}

	FMeshNorm( INT iX, INT iY, INT iZ)
	: X((unsigned int)iX), Y((unsigned int)iY), Z((unsigned int)iZ)
	{}	

	// Functions.
	FVector Vector() const
	{
		return FVector(
		(INT)(X - 511),
		(INT)(Y - 511),				
		(INT)(Z - 511)
		);
	}

	// Serialize as DWORDs.
	friend FArchive& operator<<( FArchive& Ar, FMeshNorm& N )
	{
		#if __INTEL_BYTE_ORDER__
		return Ar << GET_MESHNORM_DWORD(N);
		#else
		DWORD val = (N.X & 0x3FF) | ((N.Y & 0x3FF) << 10) | ((N.Z & 0x3FF) << 20);
		FArchive &retval = Ar << val;
		N.X = (val >> 00) & 0x3FF;
		N.Y = (val >> 10) & 0x3FF;
		N.Z = (val >> 20) & 0x3FF;
		return retval;
		#endif
	}

};

/*-----------------------------------------------------------------------------
	FMeshTri.
-----------------------------------------------------------------------------*/

// Texture coordinates associated with a vertex and one or more mesh triangles.
// All triangles sharing a vertex do not necessarily have the same texture
// coordinates at the vertex.

struct FMeshByteUV
{
	BYTE U;
	BYTE V;
	friend FArchive &operator<<( FArchive& Ar, FMeshByteUV& M )
		{return Ar << M.U << M.V;}
};

struct FMeshUV
{
	FLOAT U;
	FLOAT V;
	friend FArchive &operator<<( FArchive& Ar, FMeshUV& M )
		{return Ar << M.U << M.V;}
};

// One triangular polygon in a mesh, which references three vertices,
// and various drawing/texturing information.
struct FMeshTri
{
	_WORD		iVertex[3];		// Vertex indices.
	FMeshUV		Tex[3];			// Texture UV coordinates.
	DWORD		PolyFlags;		// Surface flags.
	INT			MaterialIndex;	// Source texture index.
	friend FArchive &operator<<( FArchive& Ar, FMeshTri& T )
	{
		Ar << T.iVertex[0] << T.iVertex[1] << T.iVertex[2];
		Ar << T.Tex[0] << T.Tex[1] << T.Tex[2];
		Ar << T.PolyFlags << T.MaterialIndex;
		return Ar;
	}
};

/*-----------------------------------------------------------------------------
	LOD Mesh structs.
-----------------------------------------------------------------------------*/

// Temporary structure to send processing info to mesh digestion at import time.
struct FLODProcessInfo
{
	UBOOL LevelOfDetail;
	UBOOL NoUVData;
	UBOOL ApplySmoothingGroups;
	INT   SampleFrame;
	INT   MinVerts;
	INT   Style;
	INT   Specify;
	FLOAT MaterialBias;
	FLOAT MaterialMinVerts;
	INT  WeldVerts;

	friend FArchive &operator<<( FArchive& Ar, FLODProcessInfo& L )
	{
		Ar << L.LevelOfDetail;
		Ar << L.NoUVData;
		Ar << L.ApplySmoothingGroups;
		Ar << L.SampleFrame;
		Ar << L.MinVerts;
		Ar << L.Style;
		Ar << L.Specify;
		Ar << L.MaterialBias;
		Ar << L.MaterialMinVerts;
		Ar << L.WeldVerts;
		return Ar;
	}
};


//
// LOD-style Textured vertex struct. references one vertex, and 
// contains texture U,V information. 4 bytes.
// One triangular polygon in a mesh, which references three vertices,
// and various drawing/texturing information. 
//
struct FMeshWedge
{
	_WORD			iVertex;		// Vertex index.
	FMeshUV			TexUV;			// Texture UV coordinates. 
	friend FArchive &operator<<( FArchive& Ar, FMeshWedge& T )
	{
		Ar << T.iVertex << T.TexUV;
		return Ar;
	}

	FMeshWedge& operator=( const FMeshWedge& Other )
	{
		this->iVertex = Other.iVertex;
		this->TexUV = Other.TexUV;
		return *this;
	}
};
	


// LOD-style triangular polygon in a mesh, which references three textured vertices.  8 bytes.
struct FMeshFace
{
	_WORD		iWedge[3];		// Textured Vertex indices.
	_WORD		MeshMaterialIndex;	// Source Material (= texture plus unique flags) index.

	friend FArchive &operator<<( FArchive& Ar, FMeshFace& F )
	{
		Ar << F.iWedge[0] << F.iWedge[1] << F.iWedge[2];
		Ar << F.MeshMaterialIndex;
		return Ar;
	}

	FMeshFace& operator=( const FMeshFace& Other )
	{
		guardSlow(FMeshFace::operator=);
		this->iWedge[0] = Other.iWedge[0];
		this->iWedge[1] = Other.iWedge[1];
		this->iWedge[2] = Other.iWedge[2];
		this->MeshMaterialIndex = Other.MeshMaterialIndex;
		return *this;
		unguardSlow;
	}	
};

// LOD-style mesh material.
struct FMeshMaterial
{
	DWORD		PolyFlags;		// Surface flags.
	INT			MaterialIndex;	// Source texture index.
	friend FArchive &operator<<( FArchive& Ar, FMeshMaterial& M )
	{
		return Ar << M.PolyFlags << M.MaterialIndex;
	}
};




// Mesh section structure - for both animating or static parts.
struct FLODMeshSection
{
	_WORD       MaterialIndex,  // Material (texture) used for this section. Usually an actor's multi-skins.
	     		FirstIndex,		// The offset of the first index in the index buffer.
				MinIndex,		// The smallest vertex index used by this section.
				MaxIndex,		// The largest vertex index used by this section.
				TotalVerts,     // The total number of vertices (as later stored in VertexStream)
				MaxInfluences,  // Largest number of per-vertex bone influences (to optimize the shader...)                  				
				ShaderIndex,    // This section might need a custom shader.
				FirstFace,      // First face for this section.
				TotalFaces;     // Faces - at full LOD - for this section

	FAnimMeshVertexStream VertexStream;
	FRawIndexBuffer       IndexBuffer;

	//TArray<_WORD> BoneIndices;  // Bones active for this chunk - used by PSX2/hardware rendering

	// Serialization.
	friend FArchive& operator<<(FArchive& Ar,FLODMeshSection& S)
	{		
		Ar		<< S.MaterialIndex
				<< S.FirstIndex
				<< S.MinIndex
				<< S.MaxIndex
				<< S.TotalVerts
				<< S.MaxInfluences
				<< S.ShaderIndex
				<< S.FirstFace
				<< S.TotalFaces;

		if( !Ar.IsPersistent() )
		{
			Ar << S.VertexStream << S.IndexBuffer;
		}

		return Ar;
	}	
};

// Fixed-level LOD skeletal mesh resources for hardware rendering.
struct FMeshLODChunk
{
	// Hardware rendering ( analogous to UnStaticMesh's 4xbuffer, \uffer, and sections.
	FLOAT                       LODFactor;
	
	FRawIndexBuffer				IndexBuffer;
	TArray<FLODMeshSection>		Sections;      
	

	// Serialization.
	friend FArchive& operator<<(FArchive& Ar,FMeshLODChunk& S)
	{
		return Ar
			<< S.LODFactor			
			<< S.IndexBuffer
			<< S.Sections;
	}

};

// Impostor data
struct FImpostorProps
{		
	UMaterial* Material;
	FVector    Scale3D;
	FRotator   RelativeRotation;
	FVector    RelativeLocation;
	FColor     ImpColor;		
	INT  ImpSpaceMode;   
	INT  ImpDrawMode;
	INT  ImpLightMode;
	
	// Serialization.
	friend FArchive& operator<<(FArchive& Ar,FImpostorProps& F)
	{		
		Ar		<< F.Material
			    << F.RelativeLocation
				<< F.RelativeRotation
				<< F.Scale3D
				<< F.ImpColor				
				<< F.ImpSpaceMode
				<< F.ImpDrawMode
				<< F.ImpLightMode;
		return Ar;
	}	
};

//
// Mesh instance for LOD Meshes
//

class ENGINE_API ULodMeshInstance : public UMeshInstance
{
	DECLARE_CLASS(ULodMeshInstance,UMeshInstance,CLASS_Transient,Engine)

	ULodMesh*  OurMesh;	      // Associated mesh.	
	AActor*	   OurActor;      // Associated actor.
	INT        StatusFlags;   //
	QWORD      LastGTicks;    // Determines freshness of the skeleton as set up in GetFrame.
	FLOAT      LastAnimFrame; //   ,,       

	// Editor/debugging rendering mode flags.
	UBOOL            bForceWireframe;
	UBOOL            bForceBackfaceCulling;
	UBOOL            bForceRawOffset;

	// Another 'massage'. During rag-doll, we want the actor origin to be on the 
	// physics root. This is so it doesn't pass outside the world and cause problems with
	// lighting etc. This bool says whether to ignore Offset/RotOffset for this instance.
	UBOOL			bIgnoreMeshOffset;

	
	//  Object interface.
	void Serialize( FArchive& Ar );	
	ULodMeshInstance(){};

	// Status queries
	virtual void SetStatus(INT Flags)
	{
		StatusFlags = Flags;
	}
	virtual int GetStatus() 
	{
		return StatusFlags;
	}

	// Get/set the mesh associated with the mesh instance.	
	virtual void  SetMesh(UMesh* InMesh ) { OurMesh = (ULodMesh*) InMesh;}
	virtual UMesh* GetMesh() { return (class UMesh*)OurMesh; }

	// Get or assign the owner for this meshinstance.
	AActor* GetActor() { return OurActor; }
	void SetActor(AActor* InActor) { OurActor = InActor; }	

	// Animation getframe..
	virtual void GetFrame(AActor* Owner, FLevelSceneNode* SceneNode, FVector* ResultVerts, INT Size, INT& LODRequest, DWORD TaskFlag){};		
	// Retrieve mesh vertices for conversion purposes.
	virtual void GetMeshVerts(AActor* Owner, FVector* ResultVerts, INT Size, INT& LODRequest){};			

	// UMesh interface.
	FMeshAnimSeq* GetAnimSeq( FName SeqName ){return NULL;};
	UMaterial* GetMaterial( INT Count, AActor* Owner ){return NULL;}; 
};

//
// LodMesh.Base of all the UW skeletal/vertex animated meshes.
//

class ENGINE_API ULodMesh : public UMesh
{
	DECLARE_CLASS(ULodMesh,UMesh,CLASS_SafeReplace, Engine)

	// Internal version number. Important now that we allow management of 
	// backward compatibility of meshes/animations inside .ukx packages.
	INT 	InternalVersion; 

	// General LOD mesh data
	INT                             ModelVerts;
	TArray<FMeshVert>				Verts;
	TArray<UMaterial*>				Materials;	
	
	// Scaling.
	FVector					Scale;		// Mesh scaling.
	FVector 				Origin;		// Origin in original coordinate system.
	FRotator				RotOrigin;	// Amount to rotate when importing (mostly for yawing).

	// LOD-specific objects.
	// Make lazy arrays where useful.	
	TArray<_WORD>           FaceLevel;          // Minimum lod-level indicator for each face.
	TArray<FMeshFace>       Faces;              // Faces 
	TArray<_WORD>			CollapseWedgeThus;  // Lod-collapse single-linked list for the wedges.
	TArray<FMeshWedge>		Wedges;             // 'Hoppe-style' textured vertices.
	TArray<FMeshMaterial>   MeshMaterials;      // Materials

	// Max of x/y/z mesh scale for LOD gauging (works on top of drawscale).
	FLOAT  MeshScaleMax;

	// Script-settable LOD controlling parameters.
	FLOAT  LODStrength;    // Scales the (not necessarily linear) falloff of vertices with distance.
	INT    LODMinVerts;    // Minimum number of vertices with which to draw a model.
	FLOAT  LODMorph;       // >0.0 = allow morphing ; 0.0-1.0 = range of vertices to morph.
	FLOAT  LODZDisplace;   // Z displacement for LOD distance-dependency tweaking.
	FLOAT  LODHysteresis;  // Controls LOD-level change delay and morphing.   

	// Impostor support.
	UBOOL			bImpostorPresent;
	FImpostorProps  ImpostorProps;

	// Hardware specific.
	FLOAT           SkinTesselationFactor; // Hardware triangle subdivision factor ( <=1.0 means inactive )

	// Multi-purpose content authentication key.
	DWORD            AuthenticationKey;

	// Object interface.
	void Serialize( FArchive& Ar );	
	ULodMesh(){};	
	UClass* MeshGetInstanceClass() { return ULodMeshInstance::StaticClass(); }

	// Authentication key generation.
	virtual DWORD CreateAuthenticationKey( INT AuxCode = 0 ){ return 0; }
	virtual UBOOL ValidateAuthenticationKey(){ return true; }

	// Estimates memory footprint in bytes, for digested data.
	virtual INT MemFootprint(UBOOL RenderDataOnly=0){ return 0; }	
	
};

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif
