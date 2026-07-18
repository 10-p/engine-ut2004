/*=============================================================================
	UnEditorOptions.h: Options for various editor components
	Copyright 2001 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

enum eMaterialTypeFilter
{
	MTF_Textures			= 0x00000001, 	// Show textures
	MTF_Shaders				= 0x00000002, 	// Show shaders
	MTF_Modifiers			= 0x00000004, 	// Show texture coordinate modifiers
	MTF_Combiners			= 0x00000008, 	// Show combiners
	MTF_FinalBlends			= 0x00000010, 	// Show final blends
};

enum eInUseTypeFilter
{
	IUF_Actors				= 0x00000001,
	IUF_Brushes				= 0x00000002,
	IUF_Sprites				= 0x00000004,
	IUF_StaticMeshes		= 0x00000008,
	IUF_Terrain				= 0x00000010,
	IUF_Emitters			= 0x00000020,
};

enum eTexViewSize
{
	TVS_200_PCT				= 1,
	TVS_100_PCT				= 2,
	TVS_50_PCT				= 4,
	TVS_25_PCT				= 8,
	TVS_32_FIXED			= 16,
	TVS_64_FIXED			= 32,
	TVS_128_FIXED			= 64,
	TVS_256_FIXED			= 128,
	TVS_512_FIXED			= 256,
	TVS_VARIABLE			= TVS_200_PCT | TVS_100_PCT | TVS_50_PCT | TVS_25_PCT,
	TVS_FIXED				= TVS_32_FIXED | TVS_64_FIXED | TVS_128_FIXED | TVS_256_FIXED | TVS_512_FIXED,
};

// Options for the texture browser.
class UNREALED_API FTBOptions
{
public:
	FTBOptions()
	{
		TypeFilter = MTF_Textures | MTF_Shaders |  MTF_Modifiers | MTF_Combiners | MTF_FinalBlends;
		IUFilter = IUF_Actors | IUF_Brushes | IUF_StaticMeshes | IUF_Terrain | IUF_Emitters;
		TexViewSize = TVS_100_PCT;
		LastScroll = LastScrollUsed = LastScrollMRU = 0;
		NameFilter = TEXT("");
	}
	~FTBOptions()
	{}

	// All tabs
	FString NameFilter;
	INT TypeFilter,				// Bitfield : eMaterialTypeFilter
		IUFilter,				// Bitfield : eInUseTypeFilter
		TexViewSize;			// How to display the textures in the browser.

	FLOAT GetURatio( UMaterial* InMaterial )
	{
		FLOAT Sz = 1.f;
		switch( TexViewSize )
		{
			case TVS_200_PCT:
			case TVS_100_PCT:
			case TVS_50_PCT:
			case TVS_25_PCT:	return 1.f;
			case TVS_32_FIXED:	Sz = 32.f;		break;
			case TVS_64_FIXED:	Sz = 64.f;		break;
			case TVS_128_FIXED:	Sz = 128.f;		break;
			case TVS_256_FIXED:	Sz = 256.f;		break;
			case TVS_512_FIXED:	Sz = 512.f;		break;
		}

		FLOAT USz = InMaterial->MaterialUSize(),
			VSz = InMaterial->MaterialVSize();

		FLOAT Ratio = ( USz > VSz
					? 1.f
					: (VSz / USz) );

		if( USz < Sz )
			Ratio = 1.f / Ratio;

		return Ratio;
	}
	FLOAT GetVRatio( UMaterial* InMaterial )
	{
		FLOAT Sz = 1.f;
		switch( TexViewSize )
		{
			case TVS_200_PCT:
			case TVS_100_PCT:
			case TVS_50_PCT:
			case TVS_25_PCT:	return 1.f;
			case TVS_32_FIXED:	Sz = 32.f;		break;
			case TVS_64_FIXED:	Sz = 64.f;		break;
			case TVS_128_FIXED:	Sz = 128.f;		break;
			case TVS_256_FIXED:	Sz = 256.f;		break;
			case TVS_512_FIXED:	Sz = 512.f;		break;
		}

		FLOAT USz = InMaterial->MaterialUSize(),
			VSz = InMaterial->MaterialVSize();

		FLOAT Ratio = ( VSz > USz
					? 1.f 
					: (USz / VSz) );

		if( VSz < Sz )
			Ratio = 1.f / Ratio;

		return Ratio;
	}
	FLOAT GetScale( UMaterial* InMaterial )
	{
		switch( TexViewSize )
		{
			case TVS_200_PCT:	return 2.f;
			case TVS_100_PCT:	return 1.f;
			case TVS_50_PCT:	return .5f;
			case TVS_25_PCT:	return .25f;
			case TVS_32_FIXED:	return 32.f / (FLOAT)(InMaterial->MaterialUSize() > InMaterial->MaterialVSize() ? InMaterial->MaterialUSize() : InMaterial->MaterialVSize() );
			case TVS_64_FIXED:	return 64.f / (FLOAT)(InMaterial->MaterialUSize() > InMaterial->MaterialVSize() ? InMaterial->MaterialUSize() : InMaterial->MaterialVSize() );
			case TVS_128_FIXED:	return 128.f / (FLOAT)(InMaterial->MaterialUSize() > InMaterial->MaterialVSize() ? InMaterial->MaterialUSize() : InMaterial->MaterialVSize() );
			case TVS_256_FIXED:	return 256.f / (FLOAT)(InMaterial->MaterialUSize() > InMaterial->MaterialVSize() ? InMaterial->MaterialUSize() : InMaterial->MaterialVSize() );
			case TVS_512_FIXED:	return 512.f / (FLOAT)(InMaterial->MaterialUSize() > InMaterial->MaterialVSize() ? InMaterial->MaterialUSize() : InMaterial->MaterialVSize() );
		}

		check(0);
		return 0.f;
	}
	INT GetMaterialWidth( UMaterial* InMaterial )
	{
		switch( TexViewSize )
		{
			case TVS_200_PCT:	return InMaterial->MaterialUSize()*2.f;
			case TVS_100_PCT:	return InMaterial->MaterialUSize()*1.f;
			case TVS_50_PCT:	return InMaterial->MaterialUSize()*.5f;
			case TVS_25_PCT:	return InMaterial->MaterialUSize()*.25f;
			case TVS_32_FIXED:	return 32;
			case TVS_64_FIXED:	return 64;
			case TVS_128_FIXED:	return 128;
			case TVS_256_FIXED:	return 256;
			case TVS_512_FIXED:	return 512;
		}

		check(0);
		return 0.f;
	}
	INT GetMaterialHeight( UMaterial* InMaterial )
	{
		switch( TexViewSize )
		{
			case TVS_200_PCT:	return InMaterial->MaterialVSize()*2.f;
			case TVS_100_PCT:	return InMaterial->MaterialVSize()*1.f;
			case TVS_50_PCT:	return InMaterial->MaterialVSize()*.5f;
			case TVS_25_PCT:	return InMaterial->MaterialVSize()*.25f;
			case TVS_32_FIXED:	return 32;
			case TVS_64_FIXED:	return 64;
			case TVS_128_FIXED:	return 128;
			case TVS_256_FIXED:	return 256;
			case TVS_512_FIXED:	return 512;
		}

		check(0);
		return 0.f;
	}
	INT GetTileSize( UMaterial* InMaterial )
	{
		switch( TexViewSize )
		{
			case TVS_200_PCT:
			case TVS_100_PCT:
			case TVS_50_PCT:
			case TVS_25_PCT:	return 0;
			case TVS_32_FIXED:	return 32;
			case TVS_64_FIXED:	return 64;
			case TVS_128_FIXED:	return 128;
			case TVS_256_FIXED:	return 256;
			case TVS_512_FIXED:	return 512;
		}

		check(0);
		return 0.f;
	}

	// "Full" tab
	INT LastScroll;

	// "Used" tab
	INT LastScrollUsed;

	// "MRU" tab
	INT LastScrollMRU;
	enum {MRU_MAX_ITEMS=16};				// The number of materials to keep in the MRU array
	TArray<UMaterial*> MRUMaterials;		// List of the 20 most recently used materials
	void DeleteMRU( UMaterial* InMaterial )
	{
		// Find this material and remove it from the list.
		INT idx;
		if( MRUMaterials.FindItem( InMaterial, idx ) )
			MRUMaterials.Remove( idx );
	}
	void AddMRU( UMaterial* InMaterial )
	{
		// If this material already exists in the array, remove it and insert it at the start.
		DeleteMRU( InMaterial );

		// Add the material at the start of the array.
		MRUMaterials.Insert( 0 );
		MRUMaterials(0) = InMaterial;

		// Remove any extra materials at the end of the array.
		while( MRUMaterials.Num() > FTBOptions::MRU_MAX_ITEMS )
			MRUMaterials.Pop();
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

