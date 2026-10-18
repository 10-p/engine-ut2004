/*=============================================================================
	UnStaticMesh.h: Static mesh class definition.
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

// sjs - moved to cpp #define STATICMESH_VERSION 8

//
//	FStaticMeshVertex
//

struct ENGINE_API FStaticMeshVertex
{
	FVector	Position,
			Normal;

	// Serializer.

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FStaticMeshVertex& V)
	{
		Ar	<< V.Position
			<< V.Normal;

		if(Ar.Ver() < 112)
		{
			FLOAT	U,
					V;
			Ar << U << V;
		}

		// sjs porting ---
        if ( Ar.LicenseeVer() >= 5 && Ar.Ver() < 112 )
		{
			FLOAT U2, V2;
            Ar << U2 << V2;
		}
		// --- sjs

		if(Ar.Ver() == 111)
		{
			FColor	Color;
			Ar << Color;
		}

		return Ar;
	}
};

//
//	FStaticMeshVertexStream
//

class ENGINE_API FStaticMeshVertexStream : public FVertexStream
{
public:

	TArray<FStaticMeshVertex>	Vertices;
	QWORD						CacheId;
	INT							Revision;

	// Constructor.

	FStaticMeshVertexStream()
	{
		CacheId = MakeCacheID(CID_RenderVertices);
		Revision = 0;
	}

	// FVertexStream interface.

	virtual INT GetSize() { return Vertices.Num() * sizeof(FStaticMeshVertex); }
	virtual INT GetStride() { return sizeof(FStaticMeshVertex); }
	virtual INT GetComponents(FVertexComponent* OutComponents)
	{
		OutComponents[0].Type = CT_Float3;
		OutComponents[0].Function = FVF_Position;
		OutComponents[1].Type = CT_Float3;
		OutComponents[1].Function = FVF_Normal;

		return 2;
	}
	virtual void GetStreamData(void* Dest)
	{
		appMemcpy(Dest,&Vertices(0),Vertices.Num() * sizeof(FStaticMeshVertex));
	}
	virtual void GetRawStreamData(void ** Dest, INT FirstVertex )
	{
		*Dest = &Vertices(FirstVertex);
	}

	// FRenderResource interface.

	virtual QWORD GetCacheId() { return CacheId; }
	virtual INT GetRevision() { return Revision; }

	// Serializer.

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FStaticMeshVertexStream& S)
	{
		return Ar << S.Vertices << S.Revision;
	}
};

//
//	FStaticMeshColorStream
//

class ENGINE_API FStaticMeshColorStream : public FRawColorStream
{
public:

	// FVertexStream interface.

	virtual INT GetComponents(FVertexComponent* OutComponents)
	{
		OutComponents[0].Type = CT_Color;
		OutComponents[0].Function = FVF_Specular;

		return 1;
	}
};

//
//	FStaticMeshUV
//

struct ENGINE_API FStaticMeshUV
{
	FLOAT	U,
			V;

	// Serializer.

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FStaticMeshUV& UV)
	{
		return Ar	<< UV.U
					<< UV.V;
	}
};

//
//	FStaticMeshUVStream
//

class ENGINE_API FStaticMeshUVStream : public FVertexStream
{
public:

	TArray<FStaticMeshUV>	UVs;
	INT						CoordinateIndex;

	QWORD					CacheId;
	INT						Revision;

	// Constructor.

	FStaticMeshUVStream()
	{
		CacheId = MakeCacheID(CID_RenderVertices);
		CoordinateIndex = 0;
		Revision = 0;
	}

	// FVertexStream interface.

	virtual INT GetSize() { return UVs.Num() * sizeof(FStaticMeshUV); }
	virtual INT GetStride() { return sizeof(FStaticMeshUV); }
	virtual INT GetComponents(FVertexComponent* OutComponents)
	{
		OutComponents[0].Type = CT_Float2;
		OutComponents[0].Function = (EFixedVertexFunction) (FVF_TexCoord0 + CoordinateIndex);

		return 1;
	}
	virtual void GetStreamData(void* Dest)
	{
		appMemcpy(Dest,&UVs(0),UVs.Num() * sizeof(FStaticMeshUV));
	}
	virtual void GetRawStreamData(void ** Dest, INT FirstVertex )
	{
		*Dest = &UVs(FirstVertex);
	}

	// FRenderResource interface.

	virtual QWORD GetCacheId() { return CacheId; }
	virtual INT GetRevision() { return Revision; }

	// Serializer.

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FStaticMeshUVStream& S)
	{
		return Ar << S.UVs << S.CoordinateIndex << S.Revision;
	}
};

//
//	FStaticMeshSection
//	A set of triangles in a static mesh.
//

class ENGINE_API FStaticMeshSection
{
public:

	UMaterial*	LegacyMaterial;	// Moved to the UStaticMesh::Materials array.  Stored for converting old static meshes.

	UBOOL		IsStrip;		// Whether this section is stripped.
	_WORD		FirstIndex,		// The first index in the index buffer used by this section.
				MinVertexIndex,	// The smallest vertex index used by this section.
				MaxVertexIndex,	// The largest vertex used by this section.
				NumTriangles,	// The number of raw triangles in this section.
				NumPrimitives;	// The number of primitives to render, including degenerate triangles used in stripping.

#ifdef __PSX2_EE__
	// PS2 specific runtime data
	void*		PS2Data;
#endif

	// Legacy variables.
	_WORD	LegacyFirstIndex;

	// Constructor.
	FStaticMeshSection()
	{
		LegacyMaterial = NULL;
		IsStrip = 0;
		FirstIndex = NumTriangles = NumPrimitives = 0;
		MinVertexIndex = MaxVertexIndex = (_WORD) INDEX_NONE;
	}

	// Serialization.
	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FStaticMeshSection& S)
	{
		if(Ar.Ver() < 92)
		{
			DWORD		PolyFlags;
			_WORD		MinIndex,
						MaxIndex;

			if(Ar.Ver() >= 74)
				Ar << S.IsStrip;
			else if(Ar.IsLoading())
				S.IsStrip = 0;

			Ar	<< S.LegacyMaterial
				<< PolyFlags
				<< S.LegacyFirstIndex
				<< S.NumTriangles
				<< MinIndex
				<< MaxIndex;
		}
		else if(Ar.Ver() < 112)
		{
			FStaticMeshVertexStream		VertexStream;
			FRawIndexBuffer				IndexBuffer,
										WireframeIndexBuffer;
			TArray<FStaticMeshUVStream>	UVStreams;
			DWORD						PolyFlags;

			Ar	<< S.IsStrip
				<< S.NumTriangles
				<< S.NumPrimitives
				<< S.LegacyMaterial
				<< PolyFlags
				<< VertexStream
				<< IndexBuffer
				<< WireframeIndexBuffer;

			if(Ar.Ver() >= 111)
				Ar	<< UVStreams;
		}
		else
		{
			Ar	<< S.IsStrip
				<< S.FirstIndex
				<< S.MinVertexIndex
				<< S.MaxVertexIndex
				<< S.NumTriangles
				<< S.NumPrimitives;
		}

		// sjs ---
        if(Ar.LicenseeVer() >= 5 && Ar.Ver() < 111) 
        {
			BYTE oldAreaColorId;
            Ar << oldAreaColorId;
        }
		// --- sjs

		return Ar;
	}
};

//
//	FStaticMeshTriangle
//

struct ENGINE_API FStaticMeshTriangle
{
public:

	FVector			Vertices[3];
	FStaticMeshUV	UVs[3][8];
	FColor			Colors[3];
	INT				MaterialIndex;
	DWORD			SmoothingMask;
	INT				NumUVs;

	UMaterial*		LegacyMaterial;
	DWORD			LegacyPolyFlags;

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FStaticMeshTriangle& T);
};

//
//	FStaticMeshCollisionTriangle
//

struct ENGINE_API FStaticMeshCollisionTriangle
{
public:

	FPlane		Plane;
	FPlane		SidePlanes[3];
	INT			VertexIndices[3];
	INT			MaterialIndex;
	INT			CollisionTag;

	// Serializer.

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FStaticMeshCollisionTriangle& T)
	{
		return Ar	<< T.Plane
					<< T.SidePlanes[0]
					<< T.SidePlanes[1]
					<< T.SidePlanes[2]
					<< AR_INDEX(T.VertexIndices[0])
					<< AR_INDEX(T.VertexIndices[1])
					<< AR_INDEX(T.VertexIndices[2])
					<< AR_INDEX(T.MaterialIndex);
	}
};

//
//	FStaticMeshCollisionNode
//

struct ENGINE_API FStaticMeshCollisionNode
{
	INT		TriangleIndex,
			CoplanarIndex,
			ChildIndices[2];

	FBox	BoundingBox;

	// Serializer.

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FStaticMeshCollisionNode& N)
	{
		return Ar	<< AR_INDEX(N.TriangleIndex)
					<< AR_INDEX(N.CoplanarIndex)
					<< AR_INDEX(N.ChildIndices[0])
					<< AR_INDEX(N.ChildIndices[1])
					<< N.BoundingBox;
	}
};

//
//	FStaticMeshMaterial
//

class ENGINE_API FStaticMeshMaterial
{
public:

	UMaterial*	Material;
	UBOOL		EnableCollision,
				OldEnableCollision;	// Used to detect when the user has changed EnableCollision. :)

	// Constructor.

	FStaticMeshMaterial(UMaterial* InMaterial)
	{
		Material = InMaterial;
		EnableCollision = OldEnableCollision = 1;
	}

	// Serializer.

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FStaticMeshMaterial& M)
	{
		return Ar	<< M.Material
					<< AR_INDEX(M.EnableCollision)
					<< AR_INDEX(M.OldEnableCollision);
	}
};


#include "UnkDOP.h"

//
//	UStaticMesh
//

class ENGINE_API UStaticMesh : public UPrimitive
{
	DECLARE_CLASS(UStaticMesh,UPrimitive,CLASS_SafeReplace|CLASS_CollapseCategories,Engine);

public:

	// Rendering data.

	TArray<FStaticMeshSection>				Sections;

	FStaticMeshVertexStream					VertexStream;
	FStaticMeshColorStream					ColorStream,
											AlphaStream;
	TArray<FStaticMeshUVStream>				UVStreams;

	FRawIndexBuffer							IndexBuffer,
											WireframeIndexBuffer;

	TArray<FStaticMeshMaterial>				Materials;

	// Collision data.
	UModel*									CollisionModel;
	FkDOPTree								kDOPTree;

	// Artist-accessible options.

	UBOOL									UseSimpleLineCollision,
											UseSimpleBoxCollision,
											UseVertexColor,
											UseSimpleKarmaCollision;

	// Source data.

	TLazyArray<FStaticMeshTriangle>			RawTriangles;
	INT										InternalVersion;
    TArray<FLOAT>                           MaxSmoothingAngles; // jij

	// Content authentication key.

	DWORD                                   AuthenticationKey;

	// Legacy variables.

	UVertexBuffer*							LegacyVertexBuffer;
	UIndexBuffer*							LegacyIndexBuffer;
	UIndexBuffer*							LegacyWireframeIndexBuffer;

	// Karma properties

	UKMeshProps*							KPhysicsProps;
#ifdef WITH_KARMA
	TArray<McdGeometryID>					KCollisionGeom;
	TArray<FVector>							KCollisionGeomScale3D;
#endif

	// Constructor.

	UStaticMesh();

	// UObject interface.

	void StaticConstructor();
	virtual void PostEditChange();
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();
	virtual void Destroy();
	virtual void Rename( const TCHAR* NewName=NULL, UObject* NewOuter=NULL );

	// UPrimitive interface.

	virtual FBox GetRenderBoundingBox(const AActor* Owner);
	virtual FSphere GetRenderBoundingSphere(const AActor* Owner);
	virtual FBox GetCollisionBoundingBox(const AActor* Owner) const;
	virtual UBOOL UseCylinderCollision( const AActor* Owner );

	virtual void Illuminate(AActor* Owner,UBOOL ChangedOnly);

	virtual UBOOL PointCheck(FCheckResult& Result,AActor* Owner,const FVector& Location,const FVector& Extent,DWORD ExtraNodeFlags);
	virtual UBOOL LineCheck(FCheckResult &Result,AActor* Owner,const FVector& End,const FVector& Start,const FVector& Extent,DWORD ExtraNodeFlags, DWORD TraceFlags);

	virtual FVector GetEncroachExtent(AActor* Owner);
	virtual FVector GetEncroachCenter(AActor* Owner);

	// UStaticMesh interface.

	FORCEINLINE UMaterial* GetSkin(AActor* Owner,INT Index)
	{
		UMaterial*	Material = Owner->GetSkin(Index);

		if(!Material)
		{
			Material = Materials(Index).Material;

			if(!Material)
				Material = CastChecked<UMaterial>(UMaterial::StaticClass()->GetDefaultObject())->DefaultMaterial;
		}

		return Material;
	}

	void Build( UBOOL OnlyCollision = 0, FVector* InNormals = NULL );

	// Authentication key generation.
	DWORD CreateAuthenticationKey( INT AuxCode = 0 );
	UBOOL ValidateAuthenticationKey();	

    // gam ---
	UBOOL NeededRebuild();
    virtual void CheckForErrors();
    // --- gam

	// UT2004 PLE-import specific
	void BuildWithNormals( const TArray< FVector > & normalList );	

};

//
//	FStaticMeshLightInfo
//

class FStaticMeshLightInfo
{
public:

	AActor*			LightActor;
	TArray<BYTE>	VisibilityBits;
	UBOOL			Applied;

	// Constructors.

	FStaticMeshLightInfo() {}
	FStaticMeshLightInfo(AActor* InActor)
	{
		LightActor = InActor;
		Applied = 0;
	}

	// Serializer.

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FStaticMeshLightInfo& L)
	{
		return Ar	<< L.LightActor
					<< L.VisibilityBits
					<< L.Applied;
	}
};

//
//	UStaticMeshInstance
//

class ENGINE_API UStaticMeshInstance : public UObject
{
	DECLARE_CLASS(UStaticMeshInstance,UObject,0,Engine);

public:

	TArray<FStaticMeshLightInfo>	Lights;
	FRawColorStream					ColorStream;

#ifdef __PSX2_EE__
	// memory to hold the PS2 lighting data for this mesh instance
	void* PS2Data;
#endif

	// Constructor.
	UStaticMeshInstance() {}

	// Serialize
	virtual void Serialize(FArchive& Ar);
};

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

