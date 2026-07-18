/*=============================================================================
	KTypes.h
=============================================================================*/

#include "UnForcePacking_begin.h"

#define KTRILIST_SIZE (1024)

/////////////////  Script Structs /////////////////

typedef struct _FKRBVec
{
	FLOAT		X;
	FLOAT		Y;
	FLOAT		Z;
} FKRBVec;

typedef struct _FKRigidBodyState
{
	FKRBVec		Position;
	FQuat		Quaternion;
	FKRBVec		LinVel;
	FKRBVec		AngVel;
} FKRigidBodyState;

typedef struct _FKSimParams
{
	FLOAT	GammaPerSec;
	FLOAT	Epsilon;
	FLOAT	PenetrationOffset;
	FLOAT	PenetrationScale;
	FLOAT	ContactSoftness;
	FLOAT	MaxPenetration;
	FLOAT	MaxTimestep;
} FKSimParams;

#ifdef WITH_KARMA

///////////////// Internal Classes /////////////////

class KarmaModelUserData
{
public:
	KarmaModelUserData() {}

	AActor* actor;
	TArray<McdModelID>	OverlapModels;
	TArray<McdModelID>	GoodbyeModels; // 'Goodbye Pending' Used temporarily during KUpdateContact
	TArray<FSphere>		SafeTimeSpheres; // Sphere used for line checks when doing safe-time check (Karma scale)
};

typedef struct _KarmaTriUserData
{
	// Friction derived from the material of the triangle
	MeReal			localFriction;

	// Restitution derived from the material of the triangle
	MeReal			localRestitution;

	// Pointer to material used on this triangle
	UMaterial*		localMaterial;
}KarmaTriUserData;

/*  Temporary storage used by tri-list collision. */
typedef struct _KarmaTriListData
{
	McdUserTriangle triangles[KTRILIST_SIZE];
    int             num_tri;

    // At the moment we copy the vertices, rather than just pointing, for safety.
    MeVector3       vertex_memory[KTRILIST_SIZE * 3];
    int             num_vert;
    
    // Memory to store computed triangle normals (if necessary).
	// We assume if we share normals, we can share tri userdata
    MeVector3       normal_memory[KTRILIST_SIZE];
	KarmaTriUserData tri_list_udata[KTRILIST_SIZE];
    int             num_norm;
}KarmaTriListData;

void ENGINE_API KarmaTriListDataInit(KarmaTriListData* list);

#endif

// ---------------------------------------------
//	Karma-relevant StaticMesh extensions
//	These have to be included in any build, preserve binary package compatibility.
// ---------------------------------------------


// --- COLLISION ---
class ENGINE_API FKSphereElem
{
public:
	FMatrix TM;
	FLOAT Radius;

	FKSphereElem() {}

	FKSphereElem( FLOAT r ) 
	: Radius(r) {}

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FKSphereElem& S )
	{
		return Ar << S.TM << S.Radius;
	}
};

class ENGINE_API FKBoxElem
{
public:
	FMatrix TM;
	FLOAT X, Y, Z; // Length (not radius) in each dimension

	FKBoxElem() {}

	FKBoxElem( FLOAT s ) 
	: X(s), Y(s), Z(s) {}

	FKBoxElem( FLOAT InX, FLOAT InY, FLOAT InZ ) 
	: X(InX), Y(InY), Z(InZ) {}	

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FKBoxElem& B )
	{
		return Ar << B.TM << B.X << B.Y << B.Z;
	}
};

class ENGINE_API FKCylinderElem
{
public:
	FMatrix TM;
	FLOAT Radius;
	FLOAT Height; // total height of cylinder

	FKCylinderElem() {}

	FKCylinderElem( FLOAT InRadius, FLOAT InHeight ) 
	: Radius(InRadius), Height(InHeight) {}	

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FKCylinderElem& C )
	{
		return Ar << C.TM << C.Radius << C.Height;
	}
};

class ENGINE_API FKConvexElem
{
public:
	FMatrix TM;
	TArray<FVector> VertexData;
	TArray<FPlane>  PlaneData;
	
	FKConvexElem() {}

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FKConvexElem& C )
	{
		return Ar << C.TM << C.VertexData;
	}
};

// A static mesh can have a pointer to one of these things, that describes
// the collision geometry used by Karma. This a collection of primitives, each with a
// transformation matrix from the mesh origin.
class ENGINE_API FKAggregateGeom
{
public:
	TArray<FKSphereElem>	SphereElems;
	TArray<FKBoxElem>		BoxElems;
	TArray<FKCylinderElem>	CylinderElems;
	TArray<FKConvexElem>	ConvexElems;

	FKAggregateGeom() {}

public:

	INT GetElementCount()
	{
		return SphereElems.Num() + CylinderElems.Num() + BoxElems.Num() + ConvexElems.Num();
	}

	void EmptyElements()
	{
		BoxElems.Empty();
		ConvexElems.Empty();
		CylinderElems.Empty();
		SphereElems.Empty();
	}

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FKAggregateGeom& G )
	{
		// There was a bug in <113 (spheres instead of convex!)
		if(Ar.Ver() < 113)
		{
			Ar << G.SphereElems;
			Ar << G.BoxElems;
			Ar << G.SphereElems;
			Ar << G.CylinderElems;
		}
		else
		{
			Ar << G.SphereElems;
			Ar << G.BoxElems;
			Ar << G.CylinderElems;
			Ar << G.ConvexElems;
		}

		return Ar;
	}
};

// This is the mass data (inertia tensor, centre-of-mass offset) optionally saved along with the graphics.
// This applies to the mesh at default scale, and with a mass of 1.
// This is in KARMA scale.

class ENGINE_API UKMeshProps : public UObject
{
	DECLARE_CLASS(UKMeshProps,UObject,0,Engine);

	// symmetric, so only store half
	// (0 1 2)
	// (1 3 4)
	// (2 4 5) 
	FLOAT				InertiaTensor[6];
	FVector				COMOffset;
	FKAggregateGeom		AggGeom;

	UKMeshProps()
	: COMOffset(0, 0, 0)
	{
		InertiaTensor[0] = InertiaTensor[1] = InertiaTensor[2] = 0;
		InertiaTensor[3] = InertiaTensor[4] = InertiaTensor[5] = 0;
	}
	
#ifdef WITH_KARMA
	void Draw(FRenderInterface* RI, INT ShowFlags);
#endif

	virtual void Serialize(FArchive& Ar)
	{
		guard(UKMeshProps::Serialize);
		Super::Serialize(Ar);
		Ar << InertiaTensor[0] << InertiaTensor[1] << InertiaTensor[2];
		Ar << InertiaTensor[3] << InertiaTensor[4] << InertiaTensor[5];
		Ar << COMOffset;
		Ar << AggGeom;
		unguard;
	}
};

#ifdef WITH_KARMA
/* NOTE!: These also scale from ME to Unreal (using K_ME2UMassScale/K_U2MEMassScale) */
void    ENGINE_API KU2METransform(MeMatrix4 tm, const FVector pos, const FRotator rot);
void    ENGINE_API KME2UTransform(FVector* pos, FRotator* rot, const MeMatrix4 tm);

void    ENGINE_API KME2UCoords(FCoords* coords, const MeMatrix4 tm);

void    ENGINE_API KU2MEPosition(MeVector3 mv, const FVector fv);
void    ENGINE_API KME2UPosition(FVector* fv, const MeVector3 mv);

/* Just copy (dont scale) */
void	ENGINE_API KU2MEMatrixCopy(MeMatrix4 out, FMatrix* in);
void	ENGINE_API KME2UMatrixCopy(FMatrix* out, MeMatrix4 in);
void    ENGINE_API KU2MEVecCopy(MeVector3 out, const FVector in);
void    ENGINE_API KME2UVecCopy(FVector* out, const MeVector3 in);

void			ENGINE_API KModelToHulls(FKAggregateGeom* outGeom, UModel* inModel, FVector prePivot);
McdGeometryID	ENGINE_API KAggregateGeomInstance(FKAggregateGeom* uGeom, FVector scale3D, McdGeomMan* geomMan, const TCHAR* name);
void			ENGINE_API KUpdateMassProps(UKMeshProps* mp);
void			ENGINE_API KCopyMassPropsToClipboard(UKMeshProps* mp);

#elif defined(WITH_PHYSICS)
/* Jolt versions use void* instead of Karma types (see JoltGlobals.h) */
void    ENGINE_API KU2MEPosition(void* mv, const FVector fv);
void    ENGINE_API KME2UPosition(FVector* fv, const void* mv);
void    ENGINE_API KU2METransform(void* tm, const FVector pos, const FRotator rot);
void    ENGINE_API KME2UTransform(FVector* pos, FRotator* rot, const void* tm);
void*   ENGINE_API KAggregateGeomInstance(FKAggregateGeom* uGeom, FVector scale3D, void* geomMan, const TCHAR* name);

#endif

#include "UnForcePacking_end.h"

