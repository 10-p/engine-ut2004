/*=============================================================================
	UnMeshEd.cpp: Unreal editor mesh code
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EditorPrivate.h"
#include "UnRender.h"

/*-----------------------------------------------------------------------------
	Data types for importing James' creature meshes.
-----------------------------------------------------------------------------*/

/* debug logging */
#undef  NLOG                
//#define NLOG(func) func
#define NLOG(func) {}



// James mesh info.
struct FJSDataHeader
{
	_WORD	NumPolys;
	_WORD	NumVertices;
	_WORD	BogusRot;
	_WORD	BogusFrame;
	DWORD	BogusNormX,BogusNormY,BogusNormZ;
	DWORD	FixScale;
	DWORD	Unused1,Unused2,Unused3;
};

// James animation info.
struct FJSAnivHeader
{
	_WORD	NumFrames;		// Number of animation frames.
	_WORD	FrameSize;		// Size of one frame of animation.
};


// Mesh triangle.
struct FJSMeshTri
{
	_WORD		iVertex[3];		// Vertex indices.
	BYTE		Type;			// James' mesh type.
	BYTE		Color;			// Color for flat and Gouraud shaded.
	FMeshByteUV	Tex[3];			// Texture UV coordinates.
	BYTE		TextureNum;		// Source texture offset.
	BYTE		Flags;			// Unreal mesh flags (currently unused).
};


/*-----------------------------------------------------------------------------
	Import functions.
-----------------------------------------------------------------------------*/

// Mesh sorting function.
static QSORT_RETURN CDECL CompareTris( const FMeshTri* A, const FMeshTri* B )
{
	if     ( (A->PolyFlags&PF_Translucent) > (B->PolyFlags&PF_Translucent) ) return  1;
	else if( (A->PolyFlags&PF_Translucent) < (B->PolyFlags&PF_Translucent) ) return -1;
	else if( (A->PolyFlags&PF_AlphaTexture)> (B->PolyFlags&PF_AlphaTexture)) return  1;
	else if( (A->PolyFlags&PF_AlphaTexture)< (B->PolyFlags&PF_AlphaTexture)) return -1;
	else if( A->MaterialIndex              > B->MaterialIndex              ) return  1;
	else if( A->MaterialIndex              < B->MaterialIndex              ) return -1;
	else if( A->PolyFlags                  > B->PolyFlags                  ) return  1;
	else if( A->PolyFlags                  < B->PolyFlags                  ) return -1;
	else                                                                     return  0;
}


//
// Import a vertex-animated mesh. Uses file commands instead of object manager.
//
void UEditorEngine::meshVertImport
(
	const TCHAR*		MeshName,
	UObject*			InParent,
	const TCHAR*		AnivFname, 
	const TCHAR*		DataFname,
	UBOOL				Unmirror,
	UBOOL				ZeroTex,
	INT					UnMirrorTex,
	FLODProcessInfo*	LODInfo
	
)
{
	guard(UEditorEngine::meshVertImport);
	UVertMesh*		Mesh;
	FArchive*		AnivFile;
	FArchive*		DataFile;
	FJSDataHeader	JSDataHdr;
	FJSAnivHeader	JSAnivHdr;
	INT				i;
	INT				Ok = 0;
	INT				MaxTextureIndex = 0;

	TArray<FMeshTri> RawTris;

	debugf( NAME_Log, TEXT("Importing mesh [%s]"), MeshName );

	// Open James' animation vertex file and read header.
	AnivFile = GFileManager->CreateFileReader( AnivFname, 0, GLog );
	if( !AnivFile )
	{
		debugf( NAME_Log, TEXT("Error opening file %s"), AnivFname );
		goto Out1;
	}
	AnivFile->Serialize( &JSAnivHdr, sizeof(FJSAnivHeader) );
	if( AnivFile->IsError() )
	{
		debugf( NAME_Log, TEXT("Error reading %s"), AnivFname );
		goto Out2;
	}

	// Open James' mesh data file and read header.
	DataFile = GFileManager->CreateFileReader( DataFname, 0, GLog );
	if( !DataFile )
	{
		debugf( NAME_Log, TEXT("Error opening file %s"), DataFname );
		goto Out2;
	}
	DataFile->Serialize( &JSDataHdr, sizeof(FJSDataHeader) );
	if( DataFile->IsError() )
	{
		debugf( NAME_Log, TEXT("Error reading %s"), DataFname );
		goto Out3;
	}


	// Allocate 
	Mesh = new( InParent, MeshName, RF_Public|RF_Standalone )UVertMesh; 

	// Initialize mesh defaults. TODO: - move into general LOD mesh initialization?
	// Default LOD settings.
	Mesh->LODMinVerts	  = 10;		// Minimum number of vertices with which to draw a model. (Minimum for a cube = 8...)
	Mesh->LODStrength	  = 1.00f;	// Scales the (not necessarily linear) falloff of vertices with distance.
	Mesh->LODMorph        = 0.30f;	// Morphing range. 0.0f = no morphing.
	Mesh->LODZDisplace    = 0.00f;    // Z-displacement (in world units) for falloff function tweaking.
	Mesh->LODHysteresis	  = 0.00f;	// Controls LOD-level change delay/morphing. (unused)
	// Set counts.
	Mesh->FrameVerts	= JSDataHdr.NumVertices;
	Mesh->AnimFrames	= JSAnivHdr.NumFrames;
	// Allocate TArrays.
	RawTris.Add(JSDataHdr.NumPolys);
	Mesh->Verts.Add(Mesh->FrameVerts * Mesh->AnimFrames);

	Mesh->BoundingBoxes.Add(Mesh->AnimFrames);
	Mesh->BoundingSpheres.Add(Mesh->AnimFrames);

	// Init textures.
	for( i=0; i<Mesh->Materials.Num(); i++ )
		Mesh->Materials(i) = NULL;
	
	
	// Display summary info.
	debugf(NAME_Log,TEXT(" * Triangles  %i"),RawTris.Num());
	debugf(NAME_Log,TEXT(" * Vertices   %i"),Mesh->FrameVerts);
	debugf(NAME_Log,TEXT(" * AnimFrames %i"),Mesh->AnimFrames);
	debugf(NAME_Log,TEXT(" * FrameSize  %i"),JSAnivHdr.FrameSize);
	debugf(NAME_Log,TEXT(" * AnimSeqs   %i"),Mesh->AnimSeqs.Num());

	// Import mesh triangles.
	debugf( NAME_Log, TEXT("Importing triangles") );
	DataFile->Seek( DataFile->Tell() + 12 );

	for( i=0; i<RawTris.Num(); i++ )
	{
		guard(Importing triangles);

		// Load triangle.
		FJSMeshTri Tri;
		DataFile->Serialize( &Tri, sizeof(Tri) );
		if( DataFile->IsError() )
		{
			debugf( NAME_Log, TEXT("Error processing %s"), DataFname );
			goto Out4;
		}
		if( Unmirror )
		{
			Exchange( Tri.iVertex[1], Tri.iVertex[2] );
			Exchange( Tri.Tex    [1], Tri.Tex    [2] );
			if( Tri.TextureNum == UnMirrorTex )
			{
				Tri.Tex[0].U = 255 - Tri.Tex[0].U;
				Tri.Tex[1].U = 255 - Tri.Tex[1].U;
				Tri.Tex[2].U = 255 - Tri.Tex[2].U;
			}
		}
		if( ZeroTex )
		{
			Tri.TextureNum = 0;
		}

		// Copy to Unreal structures.
		for( INT v=0;v<3;v++ )
		{
			RawTris(i).iVertex[v]	= Tri.iVertex[v];
			RawTris(i).Tex[v].U		= (FLOAT)Tri.Tex[v].U / 255.f;
			RawTris(i).Tex[v].V		= (FLOAT)Tri.Tex[v].V / 255.f;
		}
		RawTris(i).MaterialIndex	= Tri.TextureNum;
		MaxTextureIndex = Max<INT>(MaxTextureIndex,Tri.TextureNum);		


		// Set style based on triangle type.
		DWORD PolyFlags=0;
		if     ( (Tri.Type&15)==MTT_Normal         ) PolyFlags |= 0;
		else if( (Tri.Type&15)==MTT_NormalTwoSided ) PolyFlags |= PF_TwoSided;
		else if( (Tri.Type&15)==MTT_Modulate       ) PolyFlags |= PF_TwoSided | PF_Modulated;
		else if( (Tri.Type&15)==MTT_Translucent    ) PolyFlags |= PF_TwoSided | PF_Translucent;
		else if( (Tri.Type&15)==MTT_Masked         ) PolyFlags |= PF_TwoSided | PF_Masked;
		else if( (Tri.Type&15)==MTT_Placeholder    ) PolyFlags |= PF_TwoSided | PF_Invisible;

		// Handle effects.
		if     ( Tri.Type&MTT_Unlit             ) PolyFlags |= PF_Unlit;
		if     ( Tri.Type&MTT_Flat              ) PolyFlags |= PF_Flat;
		if     ( Tri.Type&MTT_Environment       ) PolyFlags |= PF_Environment;
		if     ( Tri.Type&MTT_NoSmooth          ) PolyFlags |= PF_NoSmooth;

		// per-pixel Alpha flag ( Reuses Flatness triangle tag and PF_AlphaTexture engine tag...)
		if     ( Tri.Type&MTT_Flat				) PolyFlags |= PF_AlphaTexture; 

		// Set flags.
		RawTris(i).PolyFlags = PolyFlags;

		unguard;
	}

	// Sort triangles by texture and flags.
	appQsort( &RawTris(0), RawTris.Num(), sizeof(RawTris(0)), (QSORT_COMPARE)CompareTris );

	while( MaxTextureIndex >= Mesh->Materials.Num() )
		Mesh->Materials.AddItem( NULL );

	debugf(TEXT(" Mesh Textures: %i "),Mesh->Materials.Num());

	// Import mesh vertices.
	debugf( NAME_Log, TEXT("Importing vertices") );
	for( i=0; i<Mesh->AnimFrames; i++ )
	{
		guard(Importing animation frames);
		AnivFile->Serialize( &Mesh->Verts(i * Mesh->FrameVerts), sizeof(FMeshVert) * Mesh->FrameVerts );
		if( AnivFile->IsError() )
		{
			debugf( NAME_Log, TEXT("Vertex error in mesh %s, frame %i: expecting %i verts"), AnivFname, i, Mesh->FrameVerts );
			break;
		}
		if( Unmirror )
			for( INT j=0; j<Mesh->FrameVerts; j++ )
				Mesh->Verts(i * Mesh->FrameVerts + j).X *= -1;
		AnivFile->Seek( AnivFile->Tell() + JSAnivHdr.FrameSize - Mesh->FrameVerts * sizeof(FMeshVert) );
		unguard;
	}

	
	// Setup mesh and process automatic LOD.
	meshVertLODProcess( Mesh, RawTris, LODInfo );
	// Compute per-frame bounding volumes plus overall bounding volume.
	Mesh->MeshGetInstance(NULL)->MeshBuildBounds();


	// Exit labels.
	Ok = 1;
	Out4: if (!Ok) {delete Mesh;}
	Out3: delete DataFile;
	Out2: delete AnivFile;
	Out1: ;
	unguard;
}

void UEditorEngine::meshDropFrames
(
	UVertMesh*		Mesh,
	INT				StartFrame,
	INT				NumFrames
)
{
	guard(UEditorEngine::meshDropFrames);
	Mesh->Verts.Remove( StartFrame*Mesh->FrameVerts, NumFrames*Mesh->FrameVerts );
	Mesh->AnimFrames -= NumFrames;
	unguard;
}


/*-----------------------------------------------------------------------------
	Special importers for skeletal data.
-----------------------------------------------------------------------------*/

//
// To do: Need much better error and version handling on all skeletal file reading - Erik
//

void UEditorEngine::meshSkelImport//modelImport
(
	const TCHAR*		MeshName,
	UObject*			InParent,
	const TCHAR*		SkinFname, 
	UBOOL				Unmirror,
	UBOOL				ZeroTex,
	UBOOL				LinkMaterials,
	FLODProcessInfo*	LODInfo
)
{
	guard(UEditorEngine::meshSkelImport);

	/*	 
	// File header structure. 
	struct VChunkHeader
	{
		ANSICHAR	ChunkID[20];  // string ID of up to 19 chars (usually zero-terminated..)
		INT			TypeFlag;     // Flags/reserved
		INT         DataSize;     // size per struct following;
		INT         DataCount;    // number of structs/
	};
	*/
	
	// Temp structs for importing
	USkelImport RawData; // local struct

	//
	// Alternate loading and construction of skeletal mesh object 
	//
	// 2nd draft: loads animation data with a SEPARATE script #exec, as a separate
	// object type, and all animations and bones link up by name.
	//

	USkeletalMesh*			Mesh;
	FArchive*		SkinFile;
	VChunkHeader	ChunkHeader;

	debugf( NAME_Log, TEXT("Importing skeletal mesh/skeleton: [%s] package: [%s] "), MeshName, InParent->GetName()  );	
	// Allocate skeletal mesh object.
	Mesh = new( InParent, MeshName, RF_Public|RF_Standalone )USkeletalMesh();

	// Open skeletal skin file and read header.
	SkinFile = GFileManager->CreateFileReader( SkinFname, 0, GLog );
	if( !SkinFile )
	{
		appErrorf( NAME_Log, TEXT("Error opening skin file %s"), SkinFname );
		//goto Out1; &&
	}

	SkinFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );
	if( SkinFile->IsError() )
	{
		appErrorf( NAME_Log, TEXT("Error reading skin file %s"), SkinFname );
		//goto Out2; &&
	}

	guard(ReadPointData);
	// Read the temp skin structures..
	// 3d points "vpoints" datasize*datacount....
	SkinFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );
	RawData.Points.Add(ChunkHeader.DataCount);
	SkinFile->Serialize( &RawData.Points(0), sizeof(VPoint) * ChunkHeader.DataCount);	
	unguard;

	guard(ReadWedgeData);
	//  Wedges (VVertex)
	SkinFile->Serialize(&ChunkHeader, sizeof(VChunkHeader) );
	RawData.Wedges.Add(ChunkHeader.DataCount);
	SkinFile->Serialize( &RawData.Wedges(0), sizeof(VVertex) * ChunkHeader.DataCount);
	unguard;

	guard(ReadFaceData);
	// Faces (VTriangle)
	SkinFile->Serialize(&ChunkHeader, sizeof(VChunkHeader) );
	RawData.Faces.Add(ChunkHeader.DataCount);
	SkinFile->Serialize( &RawData.Faces(0), sizeof(VTriangle) * ChunkHeader.DataCount);
	unguard

	guard(ReadMaterialData);
	// Materials (VMaterial)
	SkinFile->Serialize(&ChunkHeader, sizeof(VChunkHeader) );
	RawData.Materials.Add(ChunkHeader.DataCount);
	SkinFile->Serialize( &RawData.Materials(0), sizeof(VMaterial) * ChunkHeader.DataCount);
	unguard;

	guard(ReadRefSkeleton);
	// Reference skeleton (VBones)
	SkinFile->Serialize(&ChunkHeader, sizeof(VChunkHeader) );
	RawData.RefBonesBinary.Add(ChunkHeader.DataCount);
	SkinFile->Serialize( &RawData.RefBonesBinary(0), sizeof(VBone) * ChunkHeader.DataCount);
	unguard;
	
	guard(ReadBoneInfluences);
	// Raw bone influences (VRawBoneInfluence)
	SkinFile->Serialize(&ChunkHeader, sizeof(VChunkHeader) );
	RawData.Influences.Add(ChunkHeader.DataCount);
	SkinFile->Serialize( &RawData.Influences(0), sizeof(VRawBoneInfluence) * ChunkHeader.DataCount);
	unguard;

	delete SkinFile;

	// Y-flip quaternions and translations from Max/Maya/etc space into Unreal space.
	for( INT b=0; b<RawData.RefBonesBinary.Num(); b++)
	{
		FQuat Bone = RawData.RefBonesBinary(b).BonePos.Orientation;
		Bone.W = - Bone.W;
		Bone.Y = - Bone.Y;
		RawData.RefBonesBinary(b).BonePos.Orientation = Bone;

		FVector Pos = RawData.RefBonesBinary(b).BonePos.Position;
		Pos.Y = - Pos.Y;
		RawData.RefBonesBinary(b).BonePos.Position = Pos;
	}

	// Y-flip skin, and adjust handedness
	for( INT p=0; p<RawData.Points.Num(); p++ )
	{
		RawData.Points(p).Y =-RawData.Points(p).Y;
	}
	for( INT f=0; f<RawData.Faces.Num(); f++)
	{
		INT WIdx = RawData.Faces(f).WedgeIndex[2];
		RawData.Faces(f).WedgeIndex[2] = RawData.Faces(f).WedgeIndex[1];
		RawData.Faces(f).WedgeIndex[1] = WIdx;
	}	

	// Initialize mesh defaults. TODO: - move into general LOD mesh initialization?
	// Default LOD settings.
	Mesh->LODMinVerts	  = 10;		// Minimum number of vertices with which to draw a model. (Minimum for a cube = 8...)
	Mesh->LODStrength	  = 1.00f;	// Scales the (not necessarily linear) falloff of vertices with distance.
	Mesh->LODMorph        = 0.30f;	// Morphing range. 0.0f = no morphing.
	Mesh->LODZDisplace    = 0.00f;    // Z-displacement (in world units) for falloff function tweaking.
	Mesh->LODHysteresis	  = 0.00f;	// Controls LOD-level change delay/morphing. (unused)
	
	
	// If direct linkup of materials (textures for now) is requested, try to find them here - 
	// to get a texture name from a material name, cut off anything in front of the dot (beyond are special flags.)
	if( LinkMaterials )
	{
		Mesh->Materials.Empty();
		for( INT m=0; m< RawData.Materials.Num(); m++)
		{			
			TCHAR RawMatName[128];
			appStrcpy( RawMatName, appFromAnsi( RawData.Materials(m).MaterialName ) );

			// Terminate string at the dot, or at any double underscore (Maya doesn't allow 
			// anything but underscores in a material name..) Beyond that, the materialname 
			// had tags that are now already interpreted by the exporter to go into flags
			// or order the materials for the .PSK refrence skeleton/skin output.
			TCHAR* TagsCutoff = appStrstr( RawMatName , TEXT(".") );
			if(  !TagsCutoff )
				TagsCutoff = appStrstr( RawMatName, TEXT("__"));
			if( TagsCutoff ) *TagsCutoff = 0; 

			FName NewMaterialName = FName(RawMatName);				

			UMaterial* NewMaterial;
			NewMaterial = FindObject<UMaterial>( ANY_PACKAGE, *NewMaterialName );
			Mesh->Materials.AddItem(NewMaterial);
			RawData.Materials(m).TextureIndex = m; // Force 'skin' index to point to the exact named material.

			if (NewMaterial) 
				debugf(TEXT(" Found texture for material %i: [%s] skinindex:%i "),m, NewMaterial->GetName(), RawData.Materials(m).TextureIndex );
			else
				debugf(TEXT(" Mesh material not found among currently loaded ones: %s"),*NewMaterialName );
		}
	}
	else
	{
		// Init textures.
		for(INT i=0; i<Mesh->Materials.Num(); i++ )
			Mesh->Materials(i) = NULL;
	}

	// Pad the texture pointers.. TODO: clean all this up when using materials.
	while( RawData.Materials.Num() > Mesh->Materials.Num() )
			Mesh->Materials.AddItem( NULL );

	// display summary info
	debugf(NAME_Log,TEXT(" * Skeletal skin VPoints: %i "),RawData.Points.Num());
	debugf(NAME_Log,TEXT(" * Skeletal skin VVertices: %i "),RawData.Wedges.Num());
	debugf(NAME_Log,TEXT(" * Skeletal skin VTriangles: %i "),RawData.Faces.Num());
	debugf(NAME_Log,TEXT(" * Skeletal skin VMaterials: %i "),RawData.Materials.Num());
	debugf(NAME_Log,TEXT(" * Skeletal skin VBones: %i "),RawData.RefBonesBinary.Num());
	debugf(NAME_Log,TEXT(" * Skeletal skin VRawBoneInfluences: %i "),RawData.Influences.Num());
	
	// Setup mesh and process automatic LOD.
	meshSkelLODProcess( Mesh, LODInfo, &RawData );  
	// Compute per-frame bounding volumes plus overall bounding volume.
	Mesh->MeshGetInstance(NULL)->MeshBuildBounds(); 

	//// Default scalings.
	//Mesh->MeshGetInstance(NULL)->SetScale(FVector(1.0f,1.0f,1.0f));
		
	debugf(NAME_Log,TEXT(" * Total materials: %i "),((USkeletalMesh*)Mesh)->MeshMaterials.Num());
	
	// display summary info.	
	debugf(NAME_Log,TEXT(" * Skeletal skin Points: %i size %i "),Mesh->RawVerts.Num(), sizeof(FVector) );
	debugf(NAME_Log,TEXT(" * Skeletal skin Wedges: %i size %i "),Mesh->RawWedges.Num(), sizeof(FMeshWedge) );
	debugf(NAME_Log,TEXT(" * Skeletal skin Triangles: %i size %i "),Mesh->RawFaces.Num(), sizeof(VTriangle) );
	debugf(NAME_Log,TEXT(" * Skeletal skin Influences: %i size %i "),Mesh->RawInfluences.Num(), sizeof(FVertInfluence) );
	debugf(NAME_Log,TEXT(" * Skeletal skin Bones: %i size %i "),Mesh->RefSkeleton.Num(), sizeof(FMeshBone) );

	unguard;
}

INT UEditorEngine::animGetBoneIndex
( 
	UMeshAnimation* Anim,
	FName TempFname 
)
{
	guard(UEditorEngine::animGetBoneIndex);
	for( INT b=0; b< Anim->RefBones.Num(); b++)
	{
		if ( Anim->RefBones(b).Name == TempFname )
		{
			return b;
		}
	}
	return 0;
	unguard;
}

void UEditorEngine::animationImport
(
	const TCHAR*		AnimName,
	UObject*			InParent,
	const TCHAR*		DataFname,
	UBOOL				Unmirror,
	FLOAT				CompDefault
)
{
	guard(UEditorEngine::animationImport);

	UMeshAnimation* NewAnimation;
	FArchive*		AnimationFile;
	VChunkHeader	ChunkHeader;

	debugf( NAME_Log, TEXT("Importing skeletal animation [%s]"), AnimName );
	
	// Allocate skeletal mesh object.
	NewAnimation = new( InParent, AnimName, RF_Public|RF_Standalone )UMeshAnimation();

	// Allocate/initialize temp import helper structure.
	NewAnimation->InitForDigestion();
	
	// Open skeletal animation key file and read header.
	AnimationFile = GFileManager->CreateFileReader( DataFname, 0, GLog );
	if( !AnimationFile )
	{
		appErrorf( NAME_Log, TEXT("Error opening animation file %s"), DataFname );
	}

	// Read main header
	AnimationFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );
	if( AnimationFile->IsError() )
	{
		appErrorf( NAME_Log, TEXT("Error reading animation file %s"), DataFname );
	}

	// Read the header and bone names.
	AnimationFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );

	TArray<FNamedBoneBinary> RawBoneNames;
	RawBoneNames.Add(ChunkHeader.DataCount);	
	NewAnimation->RefBones.Add(ChunkHeader.DataCount);
	AnimationFile->Serialize( &RawBoneNames(0), sizeof(FNamedBoneBinary) * ChunkHeader.DataCount);
	// INT NumBones = ChunkHeader.DataCount;

	// Translate the raw data from the bones to Animation->RefBones FNames
	for( INT n=0; n<RawBoneNames.Num(); n++)
	{
		appTrimSpaces(&RawBoneNames(n).Name[0]);
		NewAnimation->RefBones(n).Name  = FName(appFromAnsi(&RawBoneNames(n).Name[0]));
		NewAnimation->RefBones(n).Flags = RawBoneNames(n).Flags;
		NewAnimation->RefBones(n).ParentIndex = RawBoneNames(n).ParentIndex;
	}

	guard(SeqInfoRaw);
	// Read the header and the animation sequence info if present...
	AnimationFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );
	NewAnimation->DigestHelper->RawAnimSeqInfo.Add(ChunkHeader.DataCount);
	AnimationFile->Serialize( &NewAnimation->DigestHelper->RawAnimSeqInfo(0), sizeof(AnimInfoBinary) * ChunkHeader.DataCount);
	// Remember to change  the raw animation name and group to FNames... =  FName(appFromAnsi( &name ));
	unguard;
	
	// Read the header and beta keys.
	AnimationFile->Serialize( &ChunkHeader, sizeof(VChunkHeader) );
	NewAnimation->DigestHelper->RawAnimKeys.Add(ChunkHeader.DataCount);
	AnimationFile->Serialize( &NewAnimation->DigestHelper->RawAnimKeys(0), sizeof(VQuatAnimKey) * ChunkHeader.DataCount);	
	NewAnimation->DigestHelper->RawNumFrames = ChunkHeader.DataCount / NewAnimation->RefBones.Num();
	NewAnimation->DigestHelper->CompFactor = CompDefault;

	delete AnimationFile;

	// Y-flip quaternions and translations from Max/Maya/etc space into Unreal space.
	for( INT i=0; i<NewAnimation->DigestHelper->RawAnimKeys.Num(); i++)
	{
		FQuat Bone = NewAnimation->DigestHelper->RawAnimKeys(i).Orientation;
		Bone.W = - Bone.W;
		Bone.Y = - Bone.Y;
		NewAnimation->DigestHelper->RawAnimKeys(i).Orientation = Bone;

		FVector Pos = NewAnimation->DigestHelper->RawAnimKeys(i).Position;
		Pos.Y = - Pos.Y;
		NewAnimation->DigestHelper->RawAnimKeys(i).Position = Pos;
	}
	unguard;
}


// Quick inter-key error evaluation. Assumes Key+1 and Key-1 indices are valid.
void GetInterKeyError( AnalogTrack& Track, const INT Key, TrackDiffs& Dev)
{
	FLOAT IntervalSize = Track.KeyTime(Key+1) - Track.KeyTime(Key-1);
	FLOAT Alpha = IntervalSize > 0.000001f ? (Track.KeyTime(Key)- Track.KeyTime(Key-1))/ IntervalSize : 0.0f;  
				
	FQuat LerpedQuat = SlerpQuat( Track.KeyQuat(Key-1),Track.KeyQuat(Key+1), Alpha);
	FLOAT QuatError = FQuatError( LerpedQuat, Track.KeyQuat(Key));
	Dev.QuatErr(Key) = QuatError;

	FVector LerpedPos = ( Track.KeyPos(Key-1)*Alpha + Track.KeyPos(Key+1)*(1.f-Alpha) );
	FLOAT PosError = (LerpedPos - Track.KeyPos(Key)).Size();				
	Dev.PosErr(Key) = PosError;
}



//
// Compress single animation from the raw data ( bones * frames ) 
// as found in RawAnimKeys()
// Anim->Moves(MoveIndex)
// Anim->DigestHelper->MovesInfo(MoveIndex)
// Anim->AnimSeqs(MoveIndex)
//
void UEditorEngine::movementDigest(	UMeshAnimation* Anim, INT MoveIndex )
{
	#define MAXANGLEDIFF 0.0001f   // angle difference below which positions are considered identical   0.0001 
	#define MAXPOSDIFF   0.0003f   // spatial difference below which positions are considered identical 0.0001  
	#define MAXERRX      0.0004f    // max 'key error' (posdiff+anglediff) below which keys are considered identical 
	#define MAXLERPERRX  0.1000f    // max angle difference below which an interpolation is considered feasible 
	
	guard(movementDigest);

	MotionChunk* ThisMove = &Anim->Moves(MoveIndex);
	MotionChunkDigestInfo* ThisMoveInfo = &Anim->DigestHelper->MovesInfo(MoveIndex);

	// Builds a FMeshAnimSeq and associated animation data for each new Move.
	Anim->AnimSeqs.AddZeroed();
	//FMeshAnimSeq* Seq = &Anim->AnimSeqs(MoveIndex);
	FMeshAnimSeq* Seq = &Anim->AnimSeqs(Anim->AnimSeqs.Num()-1);
	
	debugf(TEXT("Digesting movement number %i"),MoveIndex); 

	// !Nonzero startbone not supported by GetFrame yet.
	// Refill the remap array according to Startbone
	
	// Fill array with reduced hierarchy bone indices.
	TArray <INT> Hierarchy;
	TArray <INT> MarkParent;

	MarkParent.AddZeroed(Anim->RefBones.Num());
	
	ThisMove->StartBone = ThisMoveInfo->StartBone;
	// Rip out the sub-hierarchy.
	if( ThisMove->StartBone )
		for(INT i=ThisMove->StartBone; i<Anim->RefBones.Num(); i++)
		{		
			if( i==ThisMove->StartBone || MarkParent( Anim->RefBones(i).ParentIndex ) ) MarkParent(i)=1;
			if (MarkParent(i)) Hierarchy.AddItem(i);
		}
	else
		for(INT i=0; i<Anim->RefBones.Num(); i++)
		{
			Hierarchy.AddItem(i);
		}

	debugf(TEXT(" movement Digestion: Sub-hierarchy startbone: %i Total nodes: %i "),ThisMove->StartBone, Anim->RefBones.Num());

	// AnimTracks workspace.
	ThisMove->AnimTracks.Empty();
	ThisMove->AnimTracks.AddZeroed(Hierarchy.Num()); // AddZeroed needed - these contain dynamic arrays.

	////////////////////////////////////////////////////////////////////////////////

	// To do: Actually the KeyReduction factor should apply AFTER we threw out all trivial keys.
	
	INT KeyMaximum = (INT)Abs( (ThisMove->AnimTracks.Num() * ThisMoveInfo->NumRawFrames)*ThisMoveInfo->KeyReduction); 
	
	if (ThisMoveInfo->KeyQuotum > 0)
		KeyMaximum = Min(ThisMoveInfo->KeyQuotum,KeyMaximum);

	ThisMove->AnimTracks.Empty();
	ThisMove->AnimTracks.AddZeroed(Hierarchy.Num()); 

	debugf(TEXT("Processing uAnimation: %s - number of Bones: %i  KeyReduction %f NumRawFrames %i"),Anim->GetName(),Anim->RefBones.Num(), ThisMoveInfo->KeyReduction, ThisMoveInfo->NumRawFrames );

	ThisMove->TrackTime = ThisMoveInfo->TrackTime;

	// Fill in the backward-compatible sequence data.
	// Adding notifys: only done AFTER the digestion.
	Seq->AddGroup( ThisMoveInfo->Group ); //Seq->Group = ThisMoveInfo->Group;
	Seq->Name =  ThisMoveInfo->Name;
	Seq->NumFrames = ThisMoveInfo->NumRawFrames;
	Seq->Rate = ThisMoveInfo->AnimRate;
	Seq->StartFrame = 0; // Always the start of a compressed skeletal move.

	// Does the range overrun the actual amount of keys ?
	if( (ThisMoveInfo->FirstRawFrame + ThisMoveInfo->NumRawFrames) * Anim->RefBones.Num() > Anim->DigestHelper->RawAnimKeys.Num() )
		debugf(TEXT("Skeletal frame number overrun warning for sequence %s : Total %i Requested: %i to %i"),Anim->GetName(),Anim->DigestHelper->RawAnimKeys.Num()/Anim->RefBones.Num(),  ThisMoveInfo->FirstRawFrame, ThisMoveInfo->FirstRawFrame+ThisMoveInfo->NumRawFrames ) ;

	// Reorder raw data into the appropriate tracks - Full bones.
	for(INT i=0; i<ThisMove->AnimTracks.Num(); i++)
	{		
		INT b= Hierarchy(i);		
		NLOG( debugf(TEXT(" Bone B:%i for hierarchy I: %i"),b,i);)
		ThisMove->AnimTracks(i).Flags = 0;
		ThisMove->AnimTracks(i).KeyPos.Empty();
		ThisMove->AnimTracks(i).KeyQuat.Empty();
		ThisMove->AnimTracks(i).KeyTime.Empty();

		for( INT f=0; f< ThisMoveInfo->NumRawFrames; f++ )
		{						
			if( Anim->DigestHelper->RawAnimKeys.Num() )
			{
				// Min() makes sure no illegal key index is used.
				INT KeyIdx = Min(Anim->DigestHelper->RawAnimKeys.Num()-1,( ThisMoveInfo->FirstRawFrame + f ) * Anim->RefBones.Num() + b);
				//debugf(TEXT("Raw frame %i KeyIndex %i rawanimkeystotal %i "), f, KeyIdx, Anim->RawAnimKeys.Num());
				ThisMove->AnimTracks(i).KeyPos.AddItem( Anim->DigestHelper->RawAnimKeys(KeyIdx).Position );
				ThisMove->AnimTracks(i).KeyQuat.AddItem( Anim->DigestHelper->RawAnimKeys(KeyIdx).Orientation );
				ThisMove->AnimTracks(i).KeyTime.AddItem( (FLOAT)f/(FLOAT) ThisMoveInfo->NumRawFrames * ThisMove->TrackTime );  
			}
		}
	}

	// Nothing to eliminate if there are < 3 keys in all tracks. => To do:  except for static position tracks.
	if( ThisMoveInfo->NumRawFrames < 3 ) return; 
	
	// Find largest bone size (seems to range from 10 to 40..) -> to factor into the error.
	FLOAT BoneMax = 0.0f;

	guard(FindMax);
	for( INT i=0; i<ThisMove->AnimTracks.Num(); i++ ) 
	{
		INT b=Hierarchy(i);
		// Bone size(pivot offset) may vary over time.
		if( b != 0 ) // Ignore root track offset which does not represent a bone.
		{
			for( INT p=0; p< ThisMove->AnimTracks(i).KeyPos.Num(); p++ )
			{
				FLOAT BoneSize = ThisMove->AnimTracks(i).KeyPos(p).Size();
				if( BoneSize > BoneMax ) 
				BoneMax = BoneSize;
			}
		}
	}
	NLOG( debugf(TEXT("Max bone size for this animation: %f"),BoneMax) );
	unguard;
	
	// Create a hierarchy-depth number for each bone to scale errors properly by keeping in mind cumulative inaccuracies.
	TArray <INT> BoneDepth;
	BoneDepth.Add(Anim->RefBones.Num());

	TArray <FLOAT> ErrorBias;
	ErrorBias.Add(ThisMove->AnimTracks.Num());
	
	INT MaxDepth = 0;
	
	for( INT b=0; b<Anim->RefBones.Num(); b++ )
	{
		INT Parent = Anim->RefBones(b).ParentIndex;
		BoneDepth(b) = 1.0f;
		if( Parent != b )
		{
			BoneDepth(b) += BoneDepth(Parent);
		}
		//NLOG( debugf(TEXT("Parent of %i  is %i - hierarchy depth: %i"),b,Parent,BoneDepth(b)) );

		if( MaxDepth < BoneDepth(b))
		{
			MaxDepth = BoneDepth(b);
		}
	}
	for( INT i=0; i<ThisMove->AnimTracks.Num(); i++ )
	{
		
		INT b = Hierarchy(i);
		// conservative error scaling: root is about 2x as important as end bones ?
		ErrorBias(i) =  1.0f + (MaxDepth+1-BoneDepth(b))/MaxDepth; 
		//if (b < 5) ErrorBias(b) = 3.0f;
		
	}
	BoneDepth.Empty();

	
	// Scale bone errors according depending on max bone size.
	// Arbitrary scaler. 20/BoneMax seems reasonable; in compression, a higher PosFactor will result in
	// more accurate bone _offsets_, lower will conserve more accurate _angles_.
	FLOAT PosFactor = 10.0f / BoneMax;
	NLOG( debugf(TEXT("Keys before culling: %i  Target: %i"), ThisMove->AnimTracks.Num() * ThisMoveInfo->NumRawFrames,KeyMaximum) );

	// Stats keeping
	INT TotalKeys = ThisMove->AnimTracks.Num() * ThisMoveInfo->NumRawFrames;
	INT RemovedMatched = 0;
	INT RemovedLerped = 0;	

	// First culling step.
	for( INT b=0; b<ThisMove->AnimTracks.Num(); b++)
	{
		AnalogTrack& Track = ThisMove->AnimTracks(b);
	
		INT QuatNum = Track.KeyQuat.Num();

		// From tail on down. Immediately throw away all keys that are lerp-able with negligible error.
		for( INT i=QuatNum-2; i>0; i--)  
		{
			FLOAT KeyErr;
			KeyErr  = FQuatError( Track.KeyQuat(i), Track.KeyQuat(i-1) );
			KeyErr += FQuatError( Track.KeyQuat(i), Track.KeyQuat(i+1) );
			if( (KeyErr*0.5f) < MAXANGLEDIFF )
			{
				// Throw away only if positions match also
				FLOAT PosErr;
				PosErr  = ( Track.KeyPos(i-1) - Track.KeyPos(i) ).Size();
				PosErr += ( Track.KeyPos(i+1) - Track.KeyPos(i) ).Size();
				if( (PosFactor*PosErr*0.5f) < MAXPOSDIFF ) //###
				{
					if (1) //(b >= 0 ) 
					{
						Track.KeyQuat.Remove(i);
						Track.KeyPos.Remove(i);
						Track.KeyTime.Remove(i);
						RemovedMatched++;
						TotalKeys--;
					}
				}
			}
		}
	} 

	
	
	/*
	  ### Note:
	  Hierarchy.Num() instead of Anim->RefBones.Num() usually;
	  Error arrays etc all size Hierarchy.Num(), or Thismove->animTracks.Num()....
	  Unless you need actual full skeleton bone info keep to this size/index otherwise b=Hierarchy(i)

	*/

	// Allocate error arrays
	TArray <FLOAT> MinTrackError;
	TArray <FLOAT> MinTrackErrIdx;
	MinTrackError.AddZeroed( ThisMove->AnimTracks.Num());
	MinTrackErrIdx.AddZeroed( ThisMove->AnimTracks.Num());

	TArray <TrackDiffs> DevTrack;
	DevTrack.AddZeroed(ThisMove->AnimTracks.Num());

	// Main interpolation/compression loop

	// Precaculate per-track smallest lerp error.
	for( INT b=0; b<ThisMove->AnimTracks.Num(); b++)
	{
		AnalogTrack& Track = ThisMove->AnimTracks(b);

		DevTrack(b).QuatErr.AddZeroed( Track.KeyQuat.Num() );
		DevTrack(b).PosErr.AddZeroed( Track.KeyQuat.Num() );

		if ( Track.KeyQuat.Num() > 2)
		{
			// Fill error arrays, and min-error index & error;
			MinTrackErrIdx(b) = -1;
			MinTrackError(b) = 1000000.0f;

			for( INT i=1; i<Track.KeyQuat.Num()-1; i++)
			{
				GetInterKeyError(Track, i, DevTrack(b) );
				FLOAT ThisError = DevTrack(b).QuatErr(i)+ PosFactor*DevTrack(b).PosErr(i);
				// Why posFactor: QuatErr and PosErr need to be scaled to equivalent units -> Position relative to mesh-size.
				if (ThisError < MinTrackError(b))
				{
					MinTrackError(b) = ThisError;
					MinTrackErrIdx(b) = i;
				}
			}			
		}
	}


	debugf(TEXT("Start: Keymax %i Totalkeys %i "),KeyMaximum, TotalKeys );


	guard(While);
	while ( KeyMaximum < TotalKeys ) // TotalKeys must keep track of all keys
	{
		// Find the smallest overall error
		INT SmallestIdx = -1;
		FLOAT SmallestError = 100000000.f;

		//debugf(TEXT("Keymax %i Totalkeys %i "),KeyMaximum, TotalKeys );

		guard(Errortest);
		for( INT i=0; i<ThisMove->AnimTracks.Num(); i++)
		{
			if ( (MinTrackError(i) < SmallestError) && ThisMove->AnimTracks(i).KeyQuat.Num() > 2 )
			{
				SmallestIdx = i;
				SmallestError = ErrorBias(i) * MinTrackError(i);
			}
		}
		unguard;

		guard(Deletelerp);
		if( SmallestIdx == -1) break; // Less than 3 keys left in all tracks, so exit.
		// Check whether we need to exit because the smallest overall error is too big to lerp
		// if( SmallestError > MAXLERPERRX ) break;

		// Delete the most lerp-able key.
		INT i = MinTrackErrIdx(SmallestIdx);
		AnalogTrack& Track = ThisMove->AnimTracks(SmallestIdx);

		//debugf(TEXT("Removing %i from keys totals %i %i %i "),i,Track.KeyQuat.Num(),Track.KeyPos.Num(),Track.KeyTime.Num());
		//debugf(TEXT("Removing %i from  err totals %i %i smallestidx %i "), i,DevTrack(SmallestIdx).QuatErr.Num(),DevTrack(SmallestIdx).PosErr.Num(),SmallestIdx);
		Track.KeyQuat.Remove(i);
		Track.KeyPos.Remove(i);
		Track.KeyTime.Remove(i);
		DevTrack(SmallestIdx).QuatErr.Remove(i);
		DevTrack(SmallestIdx).PosErr.Remove(i);

		RemovedLerped++;
		TotalKeys--;		

		// Update the error for all (bordering) keys in this track,
		// And the MinTrackError for this track.
		if( Track.KeyQuat.Num() > 2)
		{			
			if (i < Track.KeyQuat.Num() - 1)
			{
				GetInterKeyError( Track, i, DevTrack(SmallestIdx) );
			}
			if (i > 1) 
			{
				GetInterKeyError( Track, i-1, DevTrack(SmallestIdx) );
			}

			MinTrackError(SmallestIdx)  = 1000000.f;
			MinTrackErrIdx(SmallestIdx) = -1;

			// update MinTrackError and MinTrackErrIdx for this track.
			for( INT i=1; i<Track.KeyQuat.Num()-1; i++)
			{				
				FLOAT ThisError = DevTrack(SmallestIdx).QuatErr(i) + PosFactor*DevTrack(SmallestIdx).PosErr(i);
				// QuatErr and PosErr need to be scaled appropriately -> Position is mesh-size dependent !
				if (ThisError < MinTrackError(SmallestIdx))
				{
					MinTrackError(SmallestIdx) = ThisError;
					MinTrackErrIdx(SmallestIdx) = i;
				}
			}			
		}		
		unguard;
	}
	unguard;

	DevTrack.Empty();

	NLOG( debugf(TEXT(" QuatKeys after lossy culling 1: %i Duplicates: %i LerpMatches: %i "),TotalKeys,RemovedMatched,RemovedLerped) );

	// Turn any 2-key tracks into 1 key tracks if possible.
	for( INT b=0; b<ThisMove->AnimTracks.Num(); b++)
	{
		AnalogTrack& Track = ThisMove->AnimTracks(b);
		
		if( Track.KeyQuat.Num() == 2 )
		{
			// Collapse 2nd key only if both Quat and Pos difference fall within the max delta limits.
			FLOAT QuatDiff = FQuatError( Track.KeyQuat(0),Track.KeyQuat(1) );
			FLOAT PosDiff  = ( Track.KeyPos(0) - Track.KeyPos(1) ).Size();

			if ( (QuatDiff + PosFactor*PosDiff) < MAXERRX )
			{
				Track.KeyQuat.Remove(1);
				Track.KeyPos.Remove(1);
				Track.KeyTime.Remove(1);
				RemovedMatched++;
				TotalKeys--;
			}		
		}
	
		if( ThisMove->AnimTracks(b).KeyQuat.Num() > 1 )
			NLOG( debugf(TEXT("#-> [%5i] QuatKeys in track %20s [%5i]"),ThisMove->AnimTracks(b).KeyQuat.Num(),*Anim->RefBones(b).Name, b ) );
	}
		
	// Compress positions: Now that tracks have been compressed, decide for each track
	// if it only needs to hold one static position or remain a full position track ( usually 
	// the root bone Pos track remains )

	INT RemovedPos = 0;
	
	for( INT b=0; b<ThisMove->AnimTracks.Num(); b++ )
	{
		FLOAT MaxDelta = 0.0f;
		for(INT i=0; i<ThisMove->AnimTracks(b).KeyPos.Num(); i++)
		{
			FVector DiffPos = ThisMove->AnimTracks(b).KeyPos(i) - ThisMove->AnimTracks(b).KeyPos(0);
			FLOAT LocalDiff;
			LocalDiff = DiffPos.Size();
			if( LocalDiff>MaxDelta ) MaxDelta = LocalDiff;
		}
		if( MaxDelta < MAXPOSDIFF) // No significant deviation in all local positions => turn it into a static track.
		{
			FVector SinglePos = ThisMove->AnimTracks(b).KeyPos(0);
			ThisMove->AnimTracks(b).KeyPos.Empty();
			ThisMove->AnimTracks(b).KeyPos.AddItem(SinglePos);
			RemovedPos++;
		}
		if( ThisMove->AnimTracks(b).KeyPos.Num() > 1 )
			NLOG( debugf(TEXT("#-> [%5i] PosKeys in track %20s [%5i]"),ThisMove->AnimTracks(b).KeyPos.Num(),*Anim->RefBones(b).Name, b ) );
	}
	
	debugf(TEXT("QuatKeys after lossy culling 2:%i  Duplicates: %i LerpMatches: %i Removed Pos tracks: %i"),TotalKeys,RemovedMatched,RemovedLerped,RemovedPos);
	
	// Align quats. The first and last quats in looping animations can only be aligned at runtime...
	for( INT b=0; b<ThisMove->AnimTracks.Num(); b++)
	{
		for(INT i=1; i< ThisMove->AnimTracks(b).KeyQuat.Num(); i++)
		{
			AlignFQuatWith( ThisMove->AnimTracks(b).KeyQuat(i), ThisMove->AnimTracks(b).KeyQuat(i-1));		
		}
	}

	// When limited to a bone subset, copy the hierarchy bonemapping array into our ThisMove
	ThisMove->BoneIndices.Empty();
	for( INT t=0; t<Anim->RefBones.Num(); t++)
	{
		INT NodeIdx =  Hierarchy.FindItemIndex(t);
		if( NodeIdx!=INDEX_NONE )
			ThisMove->BoneIndices.AddItem(NodeIdx);
		else
			ThisMove->BoneIndices.AddItem(-1);
	}

	unguard;
}

//
// Digest the raw frame data into a movement repertoire.
//
void UEditorEngine::digestMovementRepertoire( UMeshAnimation* Anim)
{	
	debugf(TEXT("Digesting %i movements for animation %s "),Anim->DigestHelper->MovesInfo.Num(),Anim->GetName());
	debugf(TEXT(" Additional imported move data: %i "), Anim->DigestHelper->RawAnimSeqInfo.Num());
	
	// Convert any animation info as defined in the input file RawAnimSeqInfo into MovesInfo...
	for( INT i=0; i<Anim->DigestHelper->RawAnimSeqInfo.Num(); i++)
	{
		MotionChunkDigestInfo NewMoveInfo;
		// Explicit copying - we need to convert names to FNames...
		NewMoveInfo.Name = FName(appFromAnsi( Anim->DigestHelper->RawAnimSeqInfo(i).Name));
		NewMoveInfo.Group = FName(appFromAnsi( Anim->DigestHelper->RawAnimSeqInfo(i).Group));		
		NewMoveInfo.FirstRawFrame = Anim->DigestHelper->RawAnimSeqInfo(i).FirstRawFrame;
		NewMoveInfo.KeyCompressionStyle = Anim->DigestHelper->RawAnimSeqInfo(i).KeyCompressionStyle;
		NewMoveInfo.KeyQuotum = Anim->DigestHelper->RawAnimSeqInfo(i).KeyQuotum;
		NewMoveInfo.KeyReduction = Anim->DigestHelper->CompFactor; //Anim->DigestHelper->RawAnimSeqInfo(i).KeyReduction;  //#SKEL
		NewMoveInfo.NumRawFrames = Anim->DigestHelper->RawAnimSeqInfo(i).NumRawFrames;
		NewMoveInfo.RootInclude = Anim->DigestHelper->RawAnimSeqInfo(i).RootInclude;
		NewMoveInfo.StartBone = Anim->DigestHelper->RawAnimSeqInfo(i).StartBone;
		NewMoveInfo.TrackTime =	Anim->DigestHelper->RawAnimSeqInfo(i).TrackTime;	
		NewMoveInfo.AnimRate = Anim->DigestHelper->RawAnimSeqInfo(i).AnimRate;

		// Addunique - based on name ? 
		Anim->DigestHelper->MovesInfo.AddItem(NewMoveInfo);
	}

	// Discard raw sequence info.
	Anim->DigestHelper->RawAnimSeqInfo.Empty();

	// Allocate moves.
	Anim->Moves.Empty();
	Anim->Moves.AddZeroed(Anim->DigestHelper->MovesInfo.Num());

	for( INT i=0; i<Anim->DigestHelper->MovesInfo.Num(); i++)
	{		
		debugf(TEXT("Digesting motion [%s] number %i  raw keys: %i reduction: %f "), (*Anim->DigestHelper->MovesInfo(i).Name), i, Anim->DigestHelper->MovesInfo(i).NumRawFrames * Anim->RefBones.Num(),Anim->DigestHelper->MovesInfo(i).KeyReduction);
		debugf(TEXT("          motion params: Group: [%s] Rate: [%f]"),(*Anim->DigestHelper->MovesInfo(i).Group),Anim->DigestHelper->MovesInfo(i).AnimRate); 
		movementDigest( Anim, i ); 
		// debugf(TEXT("  digested motion : %i keytracks:  %i rate: %f "),Anim->Moves.Num(), Anim->Moves(i).AnimTracks.Num(),Anim->DigestHelper->MovesInfo(i).AnimRate );
	}

	Anim->AnimSeqs.Shrink(); 
	Anim->Moves.Shrink();
	Anim->RefBones.Shrink();
	
}


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

