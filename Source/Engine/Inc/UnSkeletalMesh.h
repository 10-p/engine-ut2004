/*=============================================================================
	UnSkeletalMesh.h: Unreal mesh objects.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	UMeshAnimation: Unreal Animation object
	Objects containing skeletal or hierarchical animation keys.
	(Classic vertex animation is stored inside UMesh object.)

	Copyright 1999,200,2001,2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* ULodMesh, USkeletalMesh subclassing - Erik 
		* Updated to use Chris Hargrove's mesh instancing concepts.
		* Optimized and uses discrete LOD levels, feb 2002 - Erik

=============================================================================*/

#define MESHANIM_VERSION 1

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

#ifdef WITH_LIPSINC
#include "LIPSincSupport.h"
#endif

// Forward decl.
class USkeletalMesh;
class USkeletalMeshInstance;
class UMeshAnimation;
class MotionChunk;

//
// Skeletal Mesh stream for smoothly skinned sections, with compute-skin-at-GetStreamData-callback time.
//
class ENGINE_API FSkinVertexStream : public FVertexStream
{
public:

	UMesh*						AnimMesh;    
	UMeshInstance*				MeshInstance;

	QWORD						CacheId;
	INT							Revision;
	UBOOL                       bPartial;  

	UBOOL                       bStreamCallback;
	TArray<FAnimMeshVertex>		Vertices;               

	// Constructor.
	FSkinVertexStream()
	{
		AnimMesh = NULL;
		MeshInstance=NULL;
		CacheId = MakeCacheID(CID_RenderVertices);
		Revision = 1;
		bPartial = 0;
		bStreamCallback = 0;
	}

	// Non-inlines:
	virtual void GetStreamData(void* Dest);
	virtual void GetRawStreamData(void ** Dest, INT FirstVertex );
	virtual INT GetSize();
	virtual INT GetStride();

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

	// Serialization.
	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FSkinVertexStream& V)
	{
		if( !Ar.IsPersistent() )
		{
			Ar << V.AnimMesh; 
			Ar << V.MeshInstance;
		}
		Ar << V.Revision;
		Ar << V.bPartial;
		Ar << V.bStreamCallback;
		Ar << V.Vertices;
		return Ar;
	}
};


// 16-byte FSkinVert with 3d vertex and packed normal.
struct FSkinPoint
{
	FVector   Point;
	FMeshNorm Normal;

	// Serialization.
	friend FArchive& operator<<(FArchive& Ar,FSkinPoint& S)
{
		Ar		<< S.Point
				<< S.Normal;				
		return Ar;
	}	
};


// Defines
#define MAXSKELANIMCHANNELS (256)    // Arbitrary maxima.
#define MAXSKELBONESCALERS  (256)    //
#define MAXSKELBONEMOVERS   (256)    //


// Mesh section structure - for both animating and static parts.
struct FSkelMeshSection
{
	_WORD       MaterialIndex,  // Material (texture) used for this section. Usually an actor's multi-skins.
	     		FirstIndex,		// The offset of the first index in the index buffer.
				MinIndex,		// The smallest vertex index used by this section.
				MaxIndex,		// The largest vertex index used by this section.
				TotalVerts,     // The total number of vertices 
				TotalWedges,    //
				MaxInfluences,  // Largest number of per-vertex bone influences
				ShaderIndex,    // This section might need a custom shader.
				FirstFace,      // First face for this section.
				TotalFaces;     // Faces - at full LOD - for this section

	// Serialization.
	friend FArchive& operator<<(FArchive& Ar,FSkelMeshSection& S)
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
		return Ar;
	}	
};


/*-----------------------------------------------------------------------------
	Skeletal Mesh animation controllers
-----------------------------------------------------------------------------*/

struct MeshAnimLinkup
{
	UMeshAnimation* Anim;  // Animation as was linked.
	UMesh*          Mesh;  // Mesh as was linked.
	UMesh*          RefMesh;     // Optional mesh to get refpose adjustments from - to help make non-alike skeletons' animations work.
	TArray <INT>    CachedLinks; // The named-bone links between the animation and mesh.
	//TArray <FCoords>RefBoneAdjusts; // If a reference mesh was set, these adjusts may have to be applied.  At some cost, but only O(bones).

	friend FArchive &operator<<( FArchive& Ar, MeshAnimLinkup& M )
	{
		if( !Ar.IsPersistent() )
		{
			//Ar << M.Anim;
			Ar << M.Mesh;
			Ar << M.RefMesh;
			//Ar << M.CachedLinks; 
			//Ar << RefBoneAdjusts;
		}
		return Ar;
	}

};

struct MeshAnimChannel
{	
	FName ChannelID;             
	INT   MeshAnimIndex;    // Index into the instance's linked animations in AnimLinkups.

	// Classic variables	
	FName AnimSequence;     //
	FLOAT AnimRate;         //
	FLOAT AnimFrame;        //
	FLOAT AnimLast;         //
	FLOAT TweenRate;        //
	FLOAT OldAnimRate;		// Animation rate of previous animation (= AnimRate until animation completes).
	FLOAT OrigRate;         // 'Native' sequence rate.
	FLOAT AnimRangeStart;   // Optional non-zero Start and non-1.0f End within a sequence - to allows sub-ranges to 
	FLOAT AnimRangeEnd;     // be treated as a 'new' animation sequence, and even implied reversed playing.

	INT   bAnimFinished;    //
	INT   bAnimLoop;        //
	INT   bAnimNotify;      // Whether a notify is applied to the current sequence
	INT   bTimedBlend;      // Blend to a destination alpha over a certain time.
	INT   bTweening;        //
	INT   bAltRange;        // Play animation from AnimRangeStart to AnimRangeEnd if set - otherwise default 0-1 range.
	INT   bNotifyIgnore;    // Ignore notifies 
	INT   bGlobalPose;      // Evaluate entire skeleton even for partial blends, then use the global rotations..
	
	// Blending
	FLOAT BlendAlpha;       // Alpha for entire blend
	FLOAT BlendInTime;      // Lead-in time (relative to entire animation duration)	
	FLOAT BlendOutTime;     // lead-out rate (relative to entire animation )

	FLOAT BlendTargetInterval; // Remaining interval to blend from current to target alpha (starts whenver this was set.)
	FLOAT BlendTargetAlpha;    //
	FLOAT TweenProgress;    // Value tweens along with animation to create a linear alpha.

    INT   StartBone;        // start bone number

	FLOAT CachedFrame;      // AnimFrame of the last stored skeletal pose..
	FName CachedSeq;	    // Cached animation sequence

	// FPlane    SkelSimAnim[4]; // replication to simulated proxies.

	// Ctor defaults: rely on AddZeroed.
	MeshAnimChannel()
	{
	}
	~MeshAnimChannel()
	{
	}	

	friend FArchive &operator<<( FArchive& Ar, MeshAnimChannel& M )
	{
		if( !Ar.IsPersistent() )
		{
			Ar << M.ChannelID;
			Ar << M.MeshAnimIndex;
			Ar << M.AnimSequence;
			Ar << M.AnimRate;
			Ar << M.AnimFrame;
			Ar << M.AnimLast;
			Ar << M.TweenRate;
			Ar << M.OldAnimRate;
			Ar << M.OrigRate;
			Ar << M.AnimRangeStart;
			Ar << M.AnimRangeEnd;
			Ar << M.bAnimFinished;
			Ar << M.bAnimLoop;
			Ar << M.bAnimNotify;
			Ar << M.bTimedBlend;
			Ar << M.bTweening;
			Ar << M.bAltRange;
			Ar << M.bNotifyIgnore;
			Ar << M.bGlobalPose;
			Ar << M.BlendAlpha;
			Ar << M.BlendInTime;
			Ar << M.BlendOutTime;
			Ar << M.BlendTargetAlpha;
			Ar << M.BlendTargetInterval;
			Ar << M.TweenProgress;
			Ar << M.StartBone;
			Ar << M.CachedFrame;
			Ar << M.CachedSeq;
		}
		return Ar;
	}
};

struct MeshBoneScaler
{
	INT		Bone;
	FName	BoneName;
	FLOAT   ScaleUniform;
	FCoords	ScalerTrafo;

	// Serialization.
	friend FArchive& operator<<(FArchive& Ar,MeshBoneScaler& S)
	{
		return Ar
				<< S.Bone
				<< S.BoneName
				<< S.ScaleUniform
				<< S.ScalerTrafo;				
	}	
};

struct MeshBoneDirector
{
	INT		Bone;
	FName	BoneName;
	FRotator	Turn;
	FVector		Trans;
	DWORD       Flags;
	FLOAT       TurnAlpha;
	FLOAT       TransAlpha;
	// #debug add movement restrictors?

	friend FArchive& operator<<(FArchive& Ar, MeshBoneDirector& S)
	{
		return Ar
			<< S.Bone
			<< S.BoneName
			<< S.Turn
			<< S.Trans
			<< S.TurnAlpha
			<< S.TransAlpha;
	}
};





/*-----------------------------------------------------------------------------
	USkeletalMesh.
-----------------------------------------------------------------------------*/

// Note: uses old-style resource loading, then saves it as an USkeletalMesh in the
// .u building phase.  That's why some of these structs are serializable and others aren't.

// A bone: an orientation, and a position, all relative to their parent.
struct VJointPos
{
	FQuat   	Orientation;  //
	FVector		Position;     //  needed or not ?

	FLOAT       Length;       //  For collision testing / debugging drawing...
	FLOAT       XSize;
	FLOAT       YSize;
	FLOAT       ZSize;

	friend FArchive &operator<<( FArchive& Ar, VJointPos& V )
	{
		return Ar << V.Orientation << V.Position << V.Length << V.XSize << V.XSize << V.ZSize;
	}
};


//
// 16-byte helper structure (PSX2) VBoneInfluence + Bone index, up to 4, for each vertex.
//
struct VertInfIndex
{
	_WORD InfIndex[4];
	_WORD BoneIndex[4];

	// For garbage collection / objlist size only
	friend FArchive &operator<<( FArchive& Ar, VertInfIndex& V )
	{
		return Ar << V.InfIndex[0] << V.InfIndex[1] << V.InfIndex[2] << V.InfIndex[3]
			      << V.BoneIndex[0]<< V.BoneIndex[1]<< V.BoneIndex[2]<< V.BoneIndex[3];
	}
};

// Reference-skeleton bone, the package-serializable version.
struct FMeshBone
{
	FName 		Name;		  // Bone's name.
	DWORD		Flags;        // reserved
	VJointPos	BonePos;      // reference position
	INT         ParentIndex;  // 0/NULL if this is the root bone.  
	INT 		NumChildren;  // children  // only needed in animation ?
	INT         Depth;        // Number of steps to root in the skeletal hierarcy; root=0.
	friend FArchive &operator<<( FArchive& Ar, FMeshBone& F)
	{
		return Ar << F.Name << F.Flags << F.BonePos << F.NumChildren << F.ParentIndex;
	}
};

// Named bone for the animating skeleton data. 
struct FNamedBone
{
	FName	   Name;  // Bone's fname (== single 32-bit index to name)
	DWORD      Flags; // reserved
	INT        ParentIndex;  // 0/NULL if this is the root bone.  
	friend FArchive &operator<<( FArchive& Ar, FNamedBone& F)
	{
		return Ar << F.Name << F.Flags << F.ParentIndex;
	}
};

// Binary bone format to deal with raw animations as generated by various exporters.
struct FNamedBoneBinary
{
	ANSICHAR   Name[64];	// Bone's name
	DWORD      Flags;		// reserved
	INT        NumChildren; //
	INT		   ParentIndex;	// 0/NULL if this is the root bone.  
	VJointPos  BonePos;	    //
};


// Binary animation info format - used to organize raw animation keys into FAnimSeqs on rebuild
// Similar to MotionChunkDigestInfo..
struct AnimInfoBinary
{
	ANSICHAR Name[64];     // Animation's name
	ANSICHAR Group[64];    // Animation's group name	

	INT TotalBones;           // TotalBones * NumRawFrames is number of animation keys to digest.

	INT RootInclude;          // 0 none 1 included 		
	INT KeyCompressionStyle;  // Reserved: variants in tradeoffs for compression.
	INT KeyQuotum;            // Max key quotum for compression	
	FLOAT KeyReduction;       // desired 
	FLOAT TrackTime;          // explicit - can be overridden by the animation rate
	FLOAT AnimRate;           // frames per second.
	INT StartBone;            // - Reserved: for partial animations.
	INT FirstRawFrame;        //
	INT NumRawFrames;         // NumRawFrames and AnimRate dictate tracktime...
};



// File header structure. 
struct VChunkHeader
{
	ANSICHAR	ChunkID[20];  // string ID of up to 19 chars (usually zero-terminated)
	INT			TypeFlag;     // Flags/reserved
    INT         DataSize;     // size per struct following;
	INT         DataCount;    // number of structs/
};

// Raw data material.
struct VMaterial
{
	ANSICHAR            MaterialName[64];
	INT					TextureIndex;  // texture index ('multiskin index')
	DWORD				PolyFlags;     // ALL poly's with THIS material will have this flag.
	INT				    AuxMaterial;   // reserved: index into another material, eg. detailtexture/shininess/whatever.
	DWORD				AuxFlags;      // reserved: auxiliary flags 
	INT					LodBias;       // material-specific lod bias
	INT					LodStyle;      // material-specific lod style
};


// Raw data bone.
struct VBone
{
	ANSICHAR    Name[64];     //
	DWORD		Flags;        // reserved / 0x02 = bone where skin is to be attached...	
	INT 		NumChildren;  // children  // only needed in animation ?
	INT         ParentIndex;  // 0/NULL if this is the root bone.  
	VJointPos	BonePos;      // reference position
};

// Bone influence blending
struct VBoneInfIndex // ,, ,, contains Index, number of influences per bone (+ N detail level sizers! ..)
{
	_WORD WeightIndex;
	_WORD Number;  // how many to process 
	_WORD DetailA;  // how many to process if we're up to 2 max influences
	_WORD DetailB;  // how many to process if we're up to full 3 max influences 

	friend FArchive &operator<<( FArchive& Ar, VBoneInfIndex& V )
	{
		return Ar << V.WeightIndex << V.Number << V.DetailA << V.DetailB;
	}
};

struct VBoneInfluence // Weight and bone number
{
	_WORD BoneWeight; 
	_WORD BoneIndex; 
	friend FArchive &operator<<( FArchive& Ar, VBoneInfluence& V )
	{
		return Ar << V.BoneWeight << V.BoneIndex;
	}
};

// 3d vertex point index into a VBoneBlend array.
struct VWeightIndex
{
	TArray<_WORD> PointIndices;
	INT  WeightBase;
	friend FArchive &operator<<( FArchive& Ar, VWeightIndex& V )
	{
		return Ar << V.PointIndices << V.WeightBase;
	}
};

// Raw data bone influence.
struct VRawBoneInfluence // just weight, vertex, and Bone, sorted later....
{
	FLOAT Weight;
	INT   PointIndex;
	INT   BoneIndex;
};



// An animation key.
struct VQuatAnimKey
{
	FVector		Position;           // relative to parent.
	FQuat       Orientation;        // relative to parent.
	FLOAT       Time;				// The duration until the next key (end key wraps to first...)

	friend FArchive &operator<<( FArchive& Ar, VQuatAnimKey& V )
	{
		return Ar << V.Position << V.Orientation << V.Time;
	}
};

//
// 'Analog' animation key track (for single bone/element.)
// Either KeyPos or KeyQuat can be single/empty? entries to signify no movement at all;
// for N>1 entries there's always N keytimers available.
//
struct AnalogTrack
{	
	DWORD Flags;       // reserved 
	TArray <FQuat>   KeyQuat;   // Orientation key track
	TArray <FVector> KeyPos;    // Position key track
	TArray <FLOAT>   KeyTime;  // For each key, time when next key takes effect (measured from start of track.)
	friend FArchive &operator<<( FArchive& Ar, AnalogTrack& A )
	{
		return Ar << A.Flags << A.KeyQuat << A.KeyPos << A.KeyTime;
	}

	AnalogTrack& operator=( const AnalogTrack& Other)
	{
		guard(AnalogTrack::operator=);
		this->Flags   = Other.Flags;	
		this->KeyQuat = Other.KeyQuat;
		this->KeyPos  = Other.KeyPos;
		this->KeyTime = Other.KeyTime;		
		return *this;
		unguard;
	}

	void Erase()
	{
		guard(AnalogTrack::Erase);
		KeyQuat.Empty();
		KeyPos.Empty();
		KeyTime.Empty();
		unguard;
	}	
};

//
// Motion chunks as defined by Animsequences in script and/or exported raw data.
// Used during digestion phase only.
//
struct MotionChunkDigestInfo
{	
	FName	Name;		 // Sequence's name.
	FName	Group;		 // Group.	
	INT     RootInclude; // 0=none, 1=include, 2=store only root motion/
	INT     KeyCompressionStyle;
	INT     KeyQuotum;
	FLOAT   KeyReduction;
	FLOAT   TrackTime;
	FLOAT   AnimRate;
	INT     StartBone;
	INT     FirstRawFrame;
	INT     NumRawFrames;	
};

// Individual animation;  subgroup of bones with compressed animation.
class MotionChunk
{
public:
	FVector RootSpeed3D;  // Net 3d speed.
	FLOAT   TrackTime;    // Total time (Same for each track.)
	INT     StartBone;    // If we're a partial-hierarchy-movement, this is the lowest bone.
	DWORD   Flags;        // Reserved 

	TArray<INT>           BoneIndices;    // Refbones number of Bone indices (-1 or valid one) to fast-find tracks for a particular bone.
	// Frame-less, compressed animation tracks. NumBones times NumAnims tracks in total 
	TArray<AnalogTrack>   AnimTracks;     // Compressed key tracks (one for each bone)
	AnalogTrack           RootTrack;      // May or may not be used; actual traverse-a-scene root tracks for use
	// with cutscenes / special physics modes, in addition to the regular skeletal root track.

	friend FArchive &operator<<( FArchive& Ar, MotionChunk& M)
	{
		return Ar << M.RootSpeed3D << M.TrackTime << M.StartBone << M.Flags << M.BoneIndices << M.AnimTracks << M.RootTrack;
	}

	MotionChunk& operator=( const MotionChunk& Other )
	{
		guard(MotionChunk::operator =);
		this->RootSpeed3D = Other.RootSpeed3D;
		this->TrackTime   = Other.TrackTime;
		this->StartBone   = Other.StartBone;
		this->Flags       = Other.Flags;
		this->RootTrack   = Other.RootTrack;
		this->BoneIndices = Other.BoneIndices;	
		this->AnimTracks  = Other.AnimTracks;			
		return *this;
		unguard;
	}

	void Erase()
	{
		guard(MotionChunk::Erase);
		BoneIndices.Empty();
		//#SKEL
		for( INT i=0; i<AnimTracks.Num(); i++)
			AnimTracks(i).Erase();

		AnimTracks.Empty();
		unguard;
	}

	INT CalculateMemFootprint( UBOOL RenderDataOnly=0 );
};


// Temp error structure for compression of animation key tracks.
struct TrackDiffs
{
	FLOAT  BoneBias;         // Bias in error tolerance for this bone (may depend on size, place in hierarchy)
	TArray <FLOAT> QuatErr;  // Error in orientation between this key and next
	TArray <FLOAT> PosErr;   // Error in position between this key and next
};

// Vertex with texturing info, akin to Hoppe's 'Wedge' concept - import only.
struct VVertex
{
	_WORD	PointIndex;	 // Index to a point.
	FLOAT   U,V;         // Scaled to BYTES, rather...-> Done in digestion phase, on-disk size doesn't matter here.
	BYTE    MatIndex;    // At runtime, this one will be implied by the face that's pointing to us.
	BYTE    Reserved;    // Top secret.
};

// Points: regular FVectors (for now..)
struct VPoint
{	
	FVector			Point; // Change into packed integer later IF necessary, for 3x size reduction...
};

// Textured triangle.
struct VTriangle
{
	_WORD   WedgeIndex[3];	 // Point to three vertices in the vertex list.
	BYTE    MatIndex;	     // Materials can be anything.
	BYTE    AuxMatIndex;     // Second material from exporter (unused)
	DWORD   SmoothingGroups; // 32-bit flag for smoothing groups.

	friend FArchive &operator<<( FArchive& Ar, VTriangle& V )
	{
		Ar << V.WedgeIndex[0] << V.WedgeIndex[1] << V.WedgeIndex[2];
		Ar << V.MatIndex << V.AuxMatIndex;
		Ar << V.SmoothingGroups;
		return Ar;
	}

	VTriangle& operator=( const VTriangle& Other)
	{
		guard(VTriangle::operator=);
		this->AuxMatIndex = Other.AuxMatIndex;
		this->MatIndex        =  Other.MatIndex;
		this->SmoothingGroups =  Other.SmoothingGroups;
		this->WedgeIndex[0]   =  Other.WedgeIndex[0];
		this->WedgeIndex[1]   =  Other.WedgeIndex[1];
		this->WedgeIndex[2]   =  Other.WedgeIndex[2];
		return *this;
		unguard;
	}
};

struct FVertInfluence 
{
	FLOAT Weight;
	_WORD VertIndex;
	_WORD BoneIndex;
	friend FArchive &operator<<( FArchive& Ar, FVertInfluence& F )
	{
		return Ar << F.Weight << F.VertIndex << F.BoneIndex;
	}
};

// Bundle of raw data - from our 3DSMax plugin output.
struct USkelImport 
{
	TArray <VMaterial>   Materials; // Materials
	TArray <FVector>	 Points;    // 3D Points
	TArray <VVertex>     Wedges;    // Wedges
	TArray <VTriangle>   Faces;     // Faces
	TArray <VBone>       RefBonesBinary;   // reference skeleton
	TArray <VRawBoneInfluence> Influences; //
	//TArray <> AnimationKeys              //	
};



//
// Skeletal (render-ready) wedge.
//
struct FSkelWedge
{
	FLOAT  			TexU;			// Texture UV coordinates. 
	FLOAT           TexV;			//
	_WORD           VertexIndex;    // 
	                                
	friend FArchive &operator<<( FArchive& Ar, FSkelWedge& T )
	{
		Ar << T.TexU << T.TexV << T.VertexIndex;
		return Ar;
	}

	FSkelWedge& operator=( const FSkelWedge& Other )
	{		
		this->TexU = Other.TexU;
		this->TexV = Other.TexV;
		this->VertexIndex = Other.VertexIndex;
		return *this;
	}
};


//
// Colored normals for debug display as set by ComputeSkinVerts.
//
struct FVisNormal
{
	FVector Vertex;
	DWORD   InfluenceCount;
	FVector Normal;
	DWORD   MainBoneIndex;
	
	friend FArchive &operator<<( FArchive& Ar, FVisNormal& V )
	{
		Ar << V.Vertex << V.InfluenceCount << V.Normal << V.MainBoneIndex;
		return Ar;
	}
};

//
// Skeletal Bone Collision Primitive - Sphere
//
struct FSkelBoneSphere
{
	FName		BoneName;
	FVector		Offset;
	FLOAT		Radius;
	UBOOL		bBlockKarma;
	UBOOL		bBlockZeroExtent;
	UBOOL		bBlockNonZeroExtent;

	friend FArchive &operator<<( FArchive& Ar, FSkelBoneSphere& V )
	{
		Ar << V.BoneName << V.Offset << V.Radius;

		if( Ar.Ver() < 124 )
			V.bBlockKarma = true;
		else
			Ar << V.bBlockKarma;

		if( Ar.Ver() < 125 )
		{
			V.bBlockNonZeroExtent = true;
			V.bBlockZeroExtent = true;
		}
		else
		{
			Ar << V.bBlockNonZeroExtent;
			Ar << V.bBlockZeroExtent;
		}

		return Ar;
	}
};

//
// Skeletal Bone Collision Primitive - Sphere
//
struct FSkelBoneBox
{
	FName		BoneName;
	FVector		Offset;
	FVector		Radii;
	UBOOL		bBlockKarma;
	UBOOL		bBlockZeroExtent;
	UBOOL		bBlockNonZeroExtent;

	friend FArchive &operator<<( FArchive& Ar, FSkelBoneBox& V )
	{
		Ar << V.BoneName << V.Offset << V.Radii;

		if( Ar.Ver() < 124 )
			V.bBlockKarma = true;
		else
			Ar << V.bBlockKarma;

		if( Ar.Ver() < 125 )
		{
			V.bBlockNonZeroExtent = true;
			V.bBlockZeroExtent = true;
		}
		else
		{
			Ar << V.bBlockNonZeroExtent;
			Ar << V.bBlockZeroExtent;
		}

		return Ar;
	}
};

#ifdef WITH_KARMA
class FKBoneLifter
{
public:
	FInterpCurve		LiftVel;
	FInterpCurve		Softness;

	DWORD				BoneIndex;
	FLOAT				LateralFriction;
	FLOAT				CurrentTime;
	MdtContactGroupID	LiftContact;

	FKBoneLifter(DWORD InBoneIndex , FInterpCurve* InLiftVel, FLOAT InLateralFriction, FInterpCurve* InSoftness);
};
#endif

// All data to define a certain LOD model for a mesh.
class FStaticLODModel
{
public:
	//
	// All necessary data to render smooth-parts is in SkinningStream, SmoothVerts, SmoothSections and SmoothIndexbuffer.
	// For rigid parts: RigidVertexStream, RigidIndexBuffer, and RigidSections.
	//

	// Essential direct-render data.	
	TArray<DWORD>        SkinningStream; // UV's, bone indices, weights.
	TArray<FSkinPoint>   SmoothVerts;        // Vertices and normals.
	INT					 SmoothStreamWedges; // # of wedges the SkinningStream will produce.

	// Sections.
	TArray<FSkelMeshSection> SmoothSections;
	TArray<FSkelMeshSection> RigidSections;

	// Pre-filled buffers. 
	FRawIndexBuffer      SmoothIndexBuffer;
	FRawIndexBuffer      RigidIndexBuffer;
	FSkinVertexStream    RigidVertexStream;	

	// Bone hierarchy subset active for this chunk.
	TArray<_WORD> ActiveBoneIndices;  
		
	// Raw data. Never loaded unless converting / updating.
	TLazyArray<FVertInfluence> Influences;
	TLazyArray<FMeshWedge>     Wedges;
	TLazyArray<FMeshFace>      Faces;
	TLazyArray<FVector>        Points;	

	// Rendering characteristics.
	FLOAT DisplayFactor; // Lod factor at which this LOD (or a lower one) kicks in.
	FLOAT LODHysteresis; // Flexible DisplayFactor range delta allowed to prevent flickering between lods.
	INT   DupVertCount;  // Required duplicate vertices buffer size during skinning.	
	INT   MaxInfluences; // For the smooth sections; if 0, they can use simpler rendering scheme.
	UBOOL bUniqueSubset; // Whether this LOD originated from a standalone, artist-defined mesh.
	UBOOL bUseSmoothing; // Flag: construct/redigest itself with smoothing groups.	
		
	friend FArchive &operator<<( FArchive& Ar, FStaticLODModel& F )
	{	
		Ar << F.SkinningStream;
		Ar << F.SmoothVerts;
		Ar << F.SmoothStreamWedges;

		Ar << F.SmoothSections;
		Ar << F.RigidSections;		

		Ar << F.SmoothIndexBuffer;				
		Ar << F.RigidIndexBuffer;			
		Ar << F.RigidVertexStream;			

		Ar << F.Influences;
		Ar << F.Wedges;
		Ar << F.Faces;
		Ar << F.Points;		

		Ar << F.DisplayFactor;
		Ar << F.LODHysteresis;
		Ar << F.DupVertCount;
		Ar << F.MaxInfluences;
		Ar << F.bUniqueSubset;
		Ar << F.bUseSmoothing;								
		
		if( !Ar.IsPersistent() )
		{					
			Ar << F.ActiveBoneIndices;
		}
		return Ar;
	}
};


//
// Skeletal mesh instance.
//

class ENGINE_API USkeletalMeshInstance : public ULodMeshInstance
{
	DECLARE_CLASS(USkeletalMeshInstance, ULodMeshInstance, CLASS_Transient, Engine)

    virtual class USkeletalMeshInstance* GetUSkeletalMeshInstance() { return this; } // sjs

	// 'Direct-write-callback' vertex buffer for smooth-skinned parts ( doesn't contain a TArray. )	
	FSkinVertexStream     SkinStream; // All other buffers - index- and rigid-parts vertex buffers - don't change so can reside with the mesh.
	
	TArray<MeshAnimLinkup>  AnimLinkups; // Animations along with bone-to-bone linkup arrays.		
	
	// State of bone bases - set up in local mesh space.
	TArray <FCoords>   SpaceBases; 
	
	// Temporary storage of intermediate FCoords.
	FCoords            CachedMeshTrafo; // Transforms CachedFCoords into world space

	// LOD	
	INT CurrentLODLevel;     // Index into LODModels.
	FLOAT LastLodFactor;     // last computed LOD factor ( using which, LOD model was chosen )
	
	// Animation blending.
	TArray <MeshAnimChannel>  Blends;       // Animation blending.
	TArray <MeshBoneScaler>   Scalers;      // Non-uniform bone scaling
	TArray <MeshBoneDirector> Directors;    // rotation/translation controllers - local space
	TArray <MeshBoneDirector> WorldSpacers; // ,, - in world space.

	INT LastNotifyStage;  // Remembers channel for notify events. 
	INT LastAnimEndStage; //
	
	// Cached vertex frame for tweening
	TArray <FQuat>    CachedOrientations;
	TArray <FVector>  CachedPositions;
	TArray <INT>      CachedLinks;
	
	FLOAT			  CachedFrame;
	FName			  CachedSeq;	
	USkeletalMesh*    CachedMesh;	
	FLOAT		      TweenIndicator;
	UBOOL             CacheActive;
	FLOAT             PrevFrame; // Loop detection
	FName             PrevSeq;   // Loop detection

	// For rendering bone/influence debugging graphics only.
	TArray <FVector> DebugPivots;
	TArray <INT>	 DebugParents;
	TArray <FVisNormal> DebugInfluences;

	// Editor/debugging rendering mode flags.
	INT              ForcedLodModel;  // Force drawing of a specific lodmodel -1    if > 0.
	UBOOL            bForceRefpose;	  // Force reference pose.
	UBOOL            bPrintBoneNames; // Force printing of bone names.
	UBOOL            bDisplayNormals; // Force drawing of influences-count-colored normals.
	
	// Root motion control.
	INT      RootMotionMode;   // >0 : Lock root motion, letting script handle it.
	FVector  RootLocation;     // Last updated root position that we reported
	FRotator RootRotation;     // ,, ,,
	FVector  LastRootLocation;
	FRotator LastRootRotation;
	
	FCoords  LockedLocalTrafo; // Orientation/size/position at last lock from which the root animation plays out.
	INT      bRootTrafoStale;  // Set if LockedLocalTrafo needs refreshing.
	INT      bRelockRoot;      // When animation wraps or changes - relock root so no jump-back occurs.

	FCoords LockedRootTrafo; // Locked root bone memory	
	FCoords PrevRootTrafo;   //
	FCoords NewRootRaw;      // raw bone 0 fcoords at time of lock
	FCoords NewRootTrafo;    //

#ifdef WITH_LIPSINC
	INT m_nActiveLIPSincAnim;
	TArray<DOUBLE> CachedLIPSincValues;
	TArray<TLIPSincIndexedBone> CachedLIPSincBones;

	TLIPSincLookAtController LookAtController;

	TLIPSincBlendInfo LIPSincBlendInfo;

	UBOOL HasLIPSincAnim( FName LIPSincAnimName );
	void PlayLIPSincAnim( FName LIPSincAnimName, FLOAT Volume, FLOAT Radius, FLOAT Pitch );
	UBOOL StopLIPSincAnim( void );
	UBOOL PauseLIPSincAnim( void );
	UBOOL ResumeLIPSincAnim( void );
	UBOOL IsPlayingLIPSincAnim( void );

	// Force an update for this mesh, whether it is visible or not.
	void ForceUpdate( void );

	// NOTE:
	// For now, just expose an FString that is the name of the currently playing LIPSinc animation.
	// When we do LIPSinc anim properties, we might want to return a TLIPSincAnimation* here and
	// expose that to script, etc.

	FString CurrentLIPSincAnim( void );
#endif	

#ifdef WITH_KARMA
    /*
    This means the 'Reference' relative transform (with BBONE_RefPose flag)
    for this bone is nothing, and that ALL relative transforms come from a Director.
    */
    TArray <UBOOL>              NoRefPose;

    /* Physics for rag-doll. */
    TArray <McdModelID>         KSkelModels;
    TArray <MdtConstraintID>    KSkelJoints;
#endif

#ifdef WITH_KARMA
    UBOOL                       KSkelIsInitialised;

    //  BIG FAT HACK! - theres no easy way to tell from a Trace which Bone got hit,
    //  so we just remember which bone hit last time a linetrace was run...
    INT                         KLastTraceHit;

	// This is the world-space AABB to use when the skel mesh is in rag-doll.
	FBox						KSkelBox;

	// Used for 'freeze-ing' ragdoll
	UBOOL						KFrozen;
#endif

#ifdef WITH_KARMA
	// Used for contacts that lift the skeleton up.
	TArray <FKBoneLifter>		KBoneLifters;
#endif

#ifdef WITH_KARMA
	// First and last bones in the array which have physics.
	INT							KPhysRootIndex;
	INT							KPhysLastIndex;
#endif

#ifdef WITH_KARMA
	// Contains pairs of models that have no collision between them.
	TMap<KModelPairType, UBOOL>			KSkelDisableTable;
#endif

#ifdef WITH_KARMA
	// How long this ragdoll has been physical for.
	FLOAT						KRagdollAge;

	// Time (KRagdollAge) at which we need to relax our limits again.
	FLOAT						KRelaxLimitTime;
	UBOOL						KLimitsAreReduced;

	// Time that the next convulsion will start.
	FLOAT						KNextConvulsionTime;
#endif
	
	// Object interface.
	void Serialize( FArchive& Ar );
	// Constructor.
	USkeletalMeshInstance()
	{
		CachedMesh = NULL;

#ifdef WITH_LIPSINC
		m_nActiveLIPSincAnim = -1;
#endif

#ifdef WITH_KARMA
        KSkelIsInitialised = 0;
        KLastTraceHit = -1;
#endif
	}

#ifdef WITH_KARMA
	virtual void Destroy();
#endif

	UBOOL UpdateAnimation(FLOAT DeltaSeconds);
	UBOOL PlayAnim(INT Channel,FName SequenceName, FLOAT InRate, FLOAT InTweenTime, UBOOL InLooping);
	UBOOL SetBlendParams(INT Channel, FLOAT BlendAlpha, FLOAT InTime, FLOAT OutTime, FName StartBoneName, UBOOL bGlobalPose );
	UBOOL SetSkelAnim( UMeshAnimation* NewAnimation, USkeletalMesh* NewMesh ); // Link in a new animation object to the meshinstance.	
	void  ClearSkelAnims(); // Clear linked animation objects & their precached bone match lists.

	// Internal blending playanim.
	UBOOL BlendAnim(HMeshAnim InAnim, FLOAT InRate, FLOAT InTweenTime, UBOOL InLooping);

	virtual void  ActualizeAnimLinkups();

	void  SetMesh(UMesh* InMesh );

	// Return number of animations supported by the mesh instance.
	INT GetAnimCount();
	// Return animation for a given index.
	HMeshAnim GetAnimIndexed(INT InIndex);
	// Return animation for a given name
	HMeshAnim GetAnimNamed(FName InName);
	// Get the name of a given animation
	FName AnimGetName(HMeshAnim InAnim);
	// Get the group of a given animation
	FName AnimGetGroup(HMeshAnim InAnim);
	// See if an animation has this particular group tag.
	UBOOL AnimIsInGroup(HMeshAnim InAnim, FName Group);
	// Get the number of frames in an animation
	FLOAT AnimGetFrameCount(HMeshAnim InAnim);
	// Get the play rate of the animation in frames per second
	FLOAT AnimGetRate(HMeshAnim InAnim);
	// Get the number of notifications associated with this animation.
	INT AnimGetNotifyCount(HMeshAnim InAnim);
	// Get the time of a particular notification.
	FLOAT AnimGetNotifyTime(HMeshAnim InAnim, INT InNotifyIndex);
	// Get text associated with a given notify.
	const TCHAR* AnimGetNotifyText(HMeshAnim InAnim, INT InNotifyIndex);
	// Get function associated with given notify.
	UAnimNotify* AnimGetNotifyObject(HMeshAnim InAnim, INT InNotifyIndex);
	// change to void* => AnimGetNotifyObject() ? #debug

	virtual UMeshAnimation* FindAnimObjectForSequence( FName SeqName );
	virtual UBOOL AnimForcePose( FName SeqName, FLOAT AnimFrame, FLOAT Delta, INT Channel = 0 );
	virtual void ForceBoneRefresh();

	virtual UMeshAnimation* CurrentSkelAnim( INT channel );

	UBOOL IsAnimTweening( INT Channel=0);	
	UBOOL IsAnimLooping( INT Channel=0);
	UBOOL IsAnimPastLastFrame( INT Channel=0);
	UBOOL AnimStopLooping( INT Channel=0);

	FName GetActiveAnimSequence(INT Channel=0);
	FLOAT GetActiveAnimRate(INT Channel=0);
	FLOAT GetActiveAnimFrame(INT Channel=0);
	
	// TODO -clean up texture retrieval logic - some sources obsolete?
	UMaterial* GetMaterial( INT Count, AActor* Owner )
	{
		guardSlow(UMesh::GetMaterial);
		if( Owner && Owner->GetSkin( Count ) )
			return Owner->GetSkin( Count );
		else if( Count!=0 && ((ULodMesh*)GetMesh())->Materials.Num() > Count )
			return ((ULodMesh*)GetMesh())->Materials(Count);
		else if( Owner && Owner->Skins.Num() )
			return Owner->Skins(0);
		else if( Count>=0 &&((ULodMesh*)GetMesh())->Materials.Num() > Count )
			return ((ULodMesh*)GetMesh())->Materials(Count);
		else if( Count >= ((ULodMesh*)GetMesh())->Materials.Num() && ((ULodMesh*)GetMesh())->Materials.Num()>0 )
			return ((ULodMesh*)GetMesh())->Materials(0);
		else
			return NULL;
		unguardSlow;
	}

	void GetFrame( AActor* Owner, FLevelSceneNode* SceneNode, FVector*	ResultVerts, INT Size, INT& LODRequest, DWORD TaskFlag);
	void GetMeshVerts( AActor* Owner, FVector*	ResultVerts, INT Size, INT& LODRequest);
	FLOAT GetAnimRateOnChannel(INT Channel);

	// Get Verts & Normals or Bones & Normals for the current pose (interleaved.)
	INT GetMeshVertsAndNormals( AActor* Owner, TArray<FVector>* Result );
	INT GetMeshJointsAndNormals( AActor* Owner, TArray<FVector>* Result, INT BoneDepth=0 );

	// Render support.
	FMatrix MeshToWorld();
	void Render(FDynamicActor* Owner,FLevelSceneNode* SceneNode,TList<FDynamicLight*>* Lights,TList<FProjectorRenderInfo*>* Projectors,FRenderInterface* RI);
	
	virtual void MeshSkinVertsCallback( void* Dest );
	virtual INT ActiveVertStreamSize();
	
	
	void MeshBuildBounds();
	void SetScale( FVector NewScale );

	FBox GetRenderBoundingBox( const AActor* Owner );
	FSphere GetRenderBoundingSphere( const AActor* Owner );
	
	// Additional skeletal interface functions.
	void ForceGetFrame();
	FCoords  GetBoneCoords(   DWORD BoneIdx );
	FCoords  GetTagCoords( FName TagAlias );
	FRotator GetBoneRotation( FName BoneName, INT Space );
	FRotator GetBoneRotation( DWORD BoneIdx, INT Space );
	FRotator GetRootRotation();
	FVector  GetRootLocation();
	FRotator GetRootRotationDelta();
	FVector  GetRootLocationDelta();

	UBOOL	 LockRootMotion( INT Lock );
	UBOOL    EnableChannelNotify( INT Channel, INT Switch );

	INT		MatchRefBone( FName StartBoneName);
	UBOOL	ValidateAnimChannel( INT TestChannel );
	INT		GetAnimChannelCount();
	UBOOL   IsAnimating(INT Channel);
	UBOOL	StopAnimating( UBOOL ClearAll = false );
	UBOOL   FreezeAnimAt( FLOAT Time, INT Channel );

	UBOOL	SetBoneScale( INT Slot, FLOAT BoneScale, FName BoneName );
	        
	UBOOL   SetBoneLocation( FName BoneName, FVector  BoneTrans, FLOAT Alpha );
	UBOOL   SetBoneRotation( FName BoneName, FRotator BoneTurn, INT Space, FLOAT Alpha );
	UBOOL   SetBoneDirection( FName BoneName, FRotator BoneTurn, FVector BoneTrans, FLOAT Alpha, INT Space );

	void SetAnimRate(INT Channel, FLOAT NewRate);
	void ForceAnimRate(INT Channel, FLOAT NewRate);
	FLOAT GetBlendAlpha(INT Channel);
	void SetBlendAlpha(INT Channel, FLOAT NewAlpha);
	FLOAT GetAnimFrame(INT Channel);	
	void SetAnimFrame(INT Channel, FLOAT NewFrame, INT UnitFlag=0 );
	void CopyAnimation(INT SrcChannel, INT DestChannel);
	void UpdateBlendAlpha(INT Channel, FLOAT GoalAlpha, FLOAT StepSize);
	FName GetAnimSequence(INT Channel);
	void SetAnimSequence(INT Channel, FName NewSeq);
	void BlendToAlpha(INT Channel, FLOAT DestAlpha, FLOAT TimeInterval);
};
	

//
// Skeletal mesh.
//

class ENGINE_API USkeletalMesh : public ULodMesh
{
	DECLARE_CLASS(USkeletalMesh, ULodMesh, CLASS_SafeReplace, Engine)

    virtual class USkeletalMesh* GetUSkeletalMesh() { return this; } // sjs
	// Full lazy-loadable mesh data + wedge-collapses ( Verts are implicitly LOD-sorted. )
	TLazyArray<FVector>			RawVerts;  
	TLazyArray<FMeshNorm>       RawNormals;  // Temporary, redigest-time only.
	TLazyArray<FMeshWedge>		RawWedges;
	TLazyArray<VTriangle>		RawFaces;
	TLazyArray<FVertInfluence>  RawInfluences;
	TLazyArray<_WORD>           RawCollapseWedges;
	TLazyArray<_WORD>           RawFaceLevel;

	// Skeletal specific data
	TArray<FMeshBone>       RefSkeleton;   // Reference skeleton.
	INT SkeletalDepth;  // The max hierarchy depth.

	// Static LOD models
	TArray<FStaticLODModel> LODModels;

	// Legacy-skeletal data structures: 
	TArray<FVector>         Points;        // Floating point vectors directly from our skin vertex 3d points.	

	// OBSOLETE:    Single, double, etc indices - MultiWeights(0).PointIndex(n) = pointindex, weights implied from _WORD array
	TArray<VWeightIndex>	MultiBlends;  // Nested TArray.
	TArray<VBoneInfluence>  Weights;        // Weight/Boneindex array.

	UMeshAnimation* DefaultAnim; // Link this up when no other animation is available - for backwards compatibility.
	USkeletalMesh*  DefaultRefMesh; // Optional non-similar mesh for animation adjustment.

	// All LOD/Vertex buffers that aren't unique per instance.
	FMeshLODChunk LODChunks[4]; // For non-smooth-lod hardware rendering schemes (PSX2)
	
	// Reference skeleton precomputed bases #DEBUG wasteful ?!
	TArray <FCoords> RefBasesInverse;

	// Runtime influence index helper array.
	TArray <VertInfIndex> InfluenceIndex; // pre-processed index of bone influences per vertex.

	// Attachments.
	TArray <FName>   TagAliases; // Attachment bone aliases
	TArray <FName>   TagNames;	 // real bone indices into reference skeleton
	TArray <FCoords> TagCoords;	 // reserved

	// Bone collision primitives.
	// This info is BEFORE ULodMesh::DrawScale is applied.
	TArray <FSkelBoneSphere>	BoneCollisionSpheres;
	TArray <FSkelBoneBox>		BoneCollisionBoxes;

	// Set of UModels that correspond to the BoneCollisionBoxes array.
	// These have ULodMesh::DrawScale baked in.
	TArray <UModel*>			BoneCollisionBoxModels; 

	// Static mesh used for traces for actors with bUseCollisionProxyForStaticMeshTraces.
	UStaticMesh*				CollisionStaticMesh;

	// Karma version of collision data (taken from reference pose). 
	// Used for PHYS_Karma with skeletal meshes (not PHYS_KarmaRagDoll!)
	// This has the ULodMesh::DrawScale baked in.
	UKMeshProps*				KPhysicsProps;	

#ifdef __PSX2_EE__
	void* PS2Data;
#endif

	// Object interface.
	void Serialize( FArchive& Ar );
	virtual void PostLoad();
	virtual void Destroy();

	// Constructor.
	USkeletalMesh()
	{
#ifdef WITH_LIPSINC
#ifdef _XBOX
		bAlreadyLookedForController = 0;
#endif
#endif
		DefaultAnim = NULL;
	}

	UClass* MeshGetInstanceClass() { return USkeletalMeshInstance::StaticClass(); }

	// Setup-only routines - not concerned with the instance.
	void ReconstructRawMesh();
	void GenerateLodModel( INT LodLevel, FLOAT ReductionFactor, FLOAT DistanceFactor, INT MaxInfluences, UBOOL bSlowTaskAllowed );	 
	// Use the raw NewModel data as a lod level for this mesh.	
	void InsertLodModel( INT LodLevel, USkeletalMesh* RawMesh, FLOAT DistanceFactor, UBOOL bSlowTaskAllowed );

	// Convert -existing- smooth skin to rigid parts + smooth parts.	
	void ExtractRigidParts( INT LODModelIndex, INT PartMinVerts, INT MaxParts, UBOOL bDuplicateVerts );
	
	void CalculateNormals(TArray <FVector>& Normals, UBOOL Displace );
	void NormalizeInfluences( INT LodLevel );
	INT  RenderPreProcess();

	// Defined in both, but points to meshinstance implementation.
	FBox GetRenderBoundingBox( const AActor* Owner );
	FSphere GetRenderBoundingSphere( const AActor* Owner );

	// Attachment aliases.
	UBOOL SetAttachAlias( FName TagName, FName BoneName, FCoords& AdjustCoords );

	UBOOL SetAttachmentLocation(AActor *Actor, AActor *AttachedActor);

	// Forced handedness fix.
	void FlipFaces();
	UBOOL ConformSkeletonTo( USkeletalMesh* SrcMesh, UBOOL Replace );
    
	// Reports memory footprint in bytes, for digested data.
	virtual INT MemFootprint(UBOOL RenderDataOnly=0);	
	INT LODFootprint( INT LODIndex, UBOOL RenderDataOnly );

	// Authentication key generation.
	virtual DWORD CreateAuthenticationKey( INT AuxCode = 0 );
	virtual UBOOL ValidateAuthenticationKey();

	INT		MatchRefBone( FName StartBoneName);

	FSkelBoneSphere* FindBoneCollisionSphere(FName BoneName);
   

	virtual UBOOL LineCheck(FCheckResult &Result,AActor* Owner,const FVector& End,const FVector& Start,const FVector& Extent,DWORD ExtraNodeFlags, DWORD TraceFlags);
	virtual FBox GetCollisionBoundingBox( const AActor* Owner ) const;
	virtual UBOOL UseCylinderCollision( const AActor* Owner );	
	virtual UBOOL PointCheck(FCheckResult&	Result, AActor*	Owner, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags);

#ifdef WITH_LIPSINC
#ifdef _XBOX
	UBOOL	bAlreadyLookedForController;
#endif
	TLIPSincController *LIPSincController;
	UBOOL LinkupLIPSincBones( void );
#endif

};


/*-----------------------------------------------------------------------------
	UMeshAnimation definition.
-----------------------------------------------------------------------------*/
// STUB LEGACY CLASS
class ENGINE_API UAnimation : public UObject 
{
	DECLARE_CLASS(UAnimation,UObject, CLASS_SafeReplace, Engine)
	void Serialize( FArchive& Ar );
};


//
//  Per-Animation-Object (re-)digest data and info.
//
struct  AnimImportHelpObject
{
	TArray<MotionChunkDigestInfo>  MovesInfo;		  // Moves info from file or script, instructions to build the AnimSeqs.
	INT                     RawNumFrames;			  // Raw number of frames.
	TArray<VQuatAnimKey>    RawAnimKeys;			  // Raw keys (bones * frames), ordered by frames.
	TArray<AnimInfoBinary>  RawAnimSeqInfo;	          // Moves info from file (optional)
	FLOAT                   CompFactor;               // Default global compression factor when digesting animations.

	// Serialization - #skel - incomplete - but never used in-game.
	friend FArchive& operator<<(FArchive& Ar,AnimImportHelpObject& A)
	{	
		return	Ar	
			    //<< A.MovesInfo
				<< A.RawNumFrames
				<< A.RawAnimKeys
				//<< A.RawAnimSeqInfo
				<< A.CompFactor;
	}	
};

//
// UnAnimation, the base class of animating skeletal bones which are linked up by name dynamically to 
// USkeletalMesh skins/reference skeletons. 
//
class ENGINE_API UMeshAnimation : public UObject
{
	DECLARE_CLASS(UMeshAnimation,UObject, CLASS_SafeReplace, Engine)

	// Internal version number. Important now that we allow management of 
	// backward compatibility of meshes/animations inside .ukx packages.
	INT 	InternalVersion; 

	// Skeletal animation data. Linked up to a reference skeleton at runtime.
	TArray<FNamedBone>    RefBones;        // Name.
	TArray<MotionChunk>   Moves;           // One for every animation - has hierarchy starting point,
	                                       // speed, flags, compression, and actual animation.

	// MeshAnimSeq information; per-animation names/data/notifies.
	TArray<FMeshAnimSeq>  AnimSeqs;        // Classic AnimSeqs.

	// Helper object containing raw uncompressed animation keys, etc - not to be serialized.	
	AnimImportHelpObject*  DigestHelper;       

	// Object interface.
	void Serialize( FArchive& Ar );
	void PostLoad();
	// Constructor.
	UMeshAnimation();		

	virtual FMeshAnimSeq* GetAnimSeq( FName SeqName )
	{
		guardSlow(UMeshAnimation::GetAnimSeq);
		for( INT i=0; i<AnimSeqs.Num(); i++ )
		{
			if( SeqName == AnimSeqs(i).Name )
				return &AnimSeqs(i);
		}
		return NULL;
		unguardSlow;
	}

	// Initialize import/digestion helper object.
	virtual void InitForDigestion()
	{	
		if( !DigestHelper )
		{
			DigestHelper = new AnimImportHelpObject; 
			appMemzero( DigestHelper,sizeof(AnimImportHelpObject));
			DigestHelper->CompFactor = 1.0f;
		}
	}

	// Get the motion tracks for a sequence name, if present.
	virtual MotionChunk* GetMovement( FName SeqName ) 
	{
		guardSlow(UMeshAnimation::GetMovement);
		for( INT i=0; i<AnimSeqs.Num(); i++ )
		{
			if( SeqName == AnimSeqs(i).Name )
				return &Moves(i);
		}
		return NULL;
		unguardSlow;
	}

	// Modify the root orientation/translation for a certain sequence in this UMeshAnimation
	virtual UBOOL AdjustMovement( FName SeqName, FCoords AdjustCoords );	
       
        // Conform bones from non-matching skeletons when merging PSA animation data in the editor.
	virtual void ConformBones( UMeshAnimation* DestAnimObject, USkeletalMesh* ReferenceMesh );

	// Estimates memory footprint in bytes, for digested data.
	virtual INT MemFootprint();
	virtual INT SequenceMemFootprint( FName SeqName);

	// Sequence list order management. 
	virtual INT SortSequencesByName();
	virtual INT MoveSequenceByIndex( INT SourceIdx, INT DestIdx );

};

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/


