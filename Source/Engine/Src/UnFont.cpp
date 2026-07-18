/*=============================================================================
	UnFont.cpp: Unreal font code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
//#include "UnRender.h"

/*------------------------------------------------------------------------------
	OLD font page support.
------------------------------------------------------------------------------*/

struct FOLDFontCharacter
{
	// Variables.
	INT StartU, StartV;
	INT USize, VSize;

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FOLDFontCharacter& Ch )
	{
		guard(FOLDFontCharacter<<);
		return Ar << Ch.StartU << Ch.StartV << Ch.USize << Ch.VSize;
		unguard;
	}
};

//
// A font page.
//
struct FOLDFontPage
{
	// Variables.
	UTexture* Texture;
	TArray<FOLDFontCharacter> Characters;

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FOLDFontPage& Ch )
	{
		guard(FOLDFontPage<<);
		return Ar << Ch.Texture << Ch.Characters;
		unguard;
	}
};

/*------------------------------------------------------------------------------
	UFont implementation.
------------------------------------------------------------------------------*/

UFont::UFont()
{}

void UFont::Serialize( FArchive& Ar )
{
	guard(UFont::Serialize);
	Super::Serialize( Ar );
	UBOOL GSavedLazyLoad = GLazyLoad;
#ifdef _XBOX
	GLazyLoad = 0;
#else
	GLazyLoad = 1;
#endif

	if( Ar.LicenseeVer() < 0x1d )
	{
		TArray<FOLDFontPage> OldPages;
		INT CharactersPerPage;
		Ar << OldPages << CharactersPerPage;

		// convert old format.
		for( INT i=0;i<OldPages.Num();i++ )
		{
			Textures.AddItem( OldPages(i).Texture );
			for( INT c=0;c<OldPages(i).Characters.Num();c++ )
			{
                INT j = Characters.AddZeroed();
				Characters(j).TextureIndex = i;
				Characters(j).StartU = OldPages(i).Characters(c).StartU;
				Characters(j).StartV = OldPages(i).Characters(c).StartV;
				Characters(j).USize  = OldPages(i).Characters(c).USize;
				Characters(j).VSize  = OldPages(i).Characters(c).VSize;
			}	
		}
	}
	else
	{
		Ar << Characters << Textures;
	}

	Ar << Kerning;

	if( !GLazyLoad )
    {
		for( INT t=0;t<Textures.Num();t++ )
			if( Textures(t) )
				for( INT i=0; i<Textures(t)->Mips.Num(); i++ )
					Textures(t)->Mips(i).DataArray.Load();
    }
    // --- gam
	GLazyLoad = GSavedLazyLoad;

	if( Ar.Ver() >= 69 )
		Ar << CharRemap << IsRemapped;
	else
		IsRemapped = 0;

	ScaleFactor = 1.0f;

	unguardobj;
}
IMPLEMENT_CLASS(UFont);

/*------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------*/

