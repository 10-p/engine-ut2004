/*=============================================================================
	ID3TagV2.h: Describes an ID3v2 tag associated with a music file.
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision History:
	Created by Ron Prestenback.
=============================================================================*/

#ifndef NAMES_ONLY

#ifndef __ID3TAG_H__
#define __ID3TAG_H__
#include "ID3TagGlobals.h"

// =====================================================================================================================
// =====================================================================================================================
//  ID3 version 2.3
// =====================================================================================================================
// =====================================================================================================================

struct ID3HeaderSize
{
	BYTE Size[4];

	INT Get() const { return (Size[0]<<24) + (Size[1]<<16) + (Size[2]<<8) + Size[3]; }

	friend FArchive& operator<<( FArchive& Ar, ID3HeaderSize& S )
	{
		INT i;
		SERIALIZEARRAY(S.Size);
		return Ar;
	}

	ID3HeaderSize() { MEMZEROARRAY(Size); }
};

struct ENGINE_API FID3V2TagHeader
{
	BYTE Identifier [3] GCC_PACK(4);
	FID3V2TagVersion Version;
	struct TagHeaderFlags
	{
		BYTE Unsynchronized : 1;
		BYTE ExtendedHeader : 1;
		BYTE Experimental   : 1;
		BYTE                : 5;

		FID3V2TagHeader::TagHeaderFlags& operator=( const BYTE InFlags )
		{
			Unsynchronized = InFlags & THFLAG_Unsynch;
			ExtendedHeader = InFlags & THFLAG_Extended;
			Experimental   = InFlags & THFLAG_Experimental;

			return *this;
		}
		friend FArchive& operator<<( FArchive& Ar, FID3V2TagHeader::TagHeaderFlags& Flags )
		{
			BYTE& Value = (BYTE&)Flags;
			Ar << Value;
			return Ar;
		}

		TagHeaderFlags( BYTE InFlags = 0 )
			: Unsynchronized(InFlags&THFLAG_Unsynch),
			ExtendedHeader(InFlags&THFLAG_Extended),
			Experimental(InFlags&THFLAG_Experimental)
		{ }
	} Flags;

private:
	ID3HeaderSize Size;

public:
	INT GetSize() const { return Size.Get(); }
	friend FArchive& operator<<( FArchive& Ar, FID3V2TagHeader& Header )
	{
		INT i;
		SERIALIZEARRAY(Header.Identifier);
		Ar << Header.Version << Header.Flags << Header.Size;
		return Ar;
	}

	FID3V2TagHeader() { MEMZEROARRAY(Identifier); }
};

struct ENGINE_API FID3V2ExtendedTagHeader
{
private:
	ID3HeaderSize ExtendedSize;

public:
	INT GetSize() const { return ExtendedSize.Get(); }

	struct ExtendedHeaderFlags
	{
		_WORD CRCPresent : 1;
		_WORD            : 15;

		friend FArchive& operator<<( FArchive& Ar, FID3V2ExtendedTagHeader::ExtendedHeaderFlags& Flags )
		{
			_WORD& Value = (_WORD&)Flags;
			Ar << Value;
			return Ar;
		}

		ExtendedHeaderFlags( _WORD InFlags = 0 ) : CRCPresent(InFlags&ETHFlag_CRCExists) { }
	} ExtendedFlags;

private:
	ID3HeaderSize   ExtendedPaddingSize;
public:
	INT GetPaddingSize() const { return ExtendedPaddingSize.Get(); }
	DWORD           CRCData;            // Only if FID3V2ExtendedTagHeader.ExtendedHeaderFlags.CRCPresent
	friend FArchive& operator<<( FArchive& Ar, FID3V2ExtendedTagHeader& Header )
	{
		Ar << Header.ExtendedSize << Header.ExtendedFlags;
		if ( Header.ExtendedFlags.CRCPresent ) Ar << Header.CRCData;
		return Ar;
	}

	FID3V2ExtendedTagHeader( DWORD CRC = 0 ) : CRCData(CRC) { }

};

struct ENGINE_API FFrameHeader
{
	union
	{
		BYTE   IDText[4];
		DWORD  IDValue;
	};

private:
	ID3HeaderSize    Size;

public:
	const TCHAR* GetID( TCHAR* OutID ) const
	{
		if ( OutID )
			return appFromAnsi( (ANSICHAR*)IDText, OutID, 5 );

		else return appFromAnsi( (ANSICHAR*)IDText, NULL, 5 );
	}

	INT GetSize() const { return Size.Get(); }

	struct HeaderFlags
	{
		FFrameHeader::HeaderFlags& operator=( _WORD InValue )
		{
			PreserveIfAlterTag  = InValue & FHFLAG_PreserveTagAlter;
			PreserveIfAlterFile = InValue & FHFLAG_PreserveFileAlter;
			ReadOnly            = InValue & FHFLAG_ReadOnly;
			Compressed          = InValue & FHFLAG_Compressed;
			Encrypted           = InValue & FHFLAG_Encrypted;
			GroupingInfo        = InValue & FHFLAG_GroupingInfo;

			return *this;
		}
		friend FArchive& operator<<( FArchive& Ar, FFrameHeader::HeaderFlags& Flags )
		{
			_WORD& Value = (_WORD&)Flags;
			Ar << Value;
			return Ar;
		}

		_WORD  PreserveIfAlterTag : 1 GCC_PACK(4);
		_WORD  PreserveIfAlterFile: 1;
		_WORD  ReadOnly           : 1;
		_WORD                     : 5;
		_WORD  Compressed         : 1;
		_WORD  Encrypted          : 1;
		_WORD  GroupingInfo       : 1;
		_WORD                     : 5;

		HeaderFlags( _WORD InValue = 0 ) 
			: PreserveIfAlterTag(InValue&FHFLAG_PreserveTagAlter),
			PreserveIfAlterFile(InValue&FHFLAG_PreserveFileAlter),
			ReadOnly(InValue&FHFLAG_ReadOnly),
			Compressed(InValue&FHFLAG_Compressed),
			Encrypted(InValue&FHFLAG_Encrypted),
			GroupingInfo(InValue&FHFLAG_GroupingInfo)
		{ }

	} Flags;

	friend FArchive& operator<<( FArchive& Ar, FFrameHeader& Header )
	{
		INT i;
		SERIALIZEARRAY(Header.IDText);
		Ar << Header.Size << Header.Flags;
		return Ar;
	};

	FFrameHeader( DWORD InValue = 0 )            : IDValue(InValue)  { }
	FFrameHeader( const FFrameHeader& InHeader ) : Size(InHeader.Size), IDValue(InHeader.IDValue), Flags(InHeader.Flags) { }
};
struct ENGINE_API FID3V2Frame
{
	FFrameHeader Header;
	DWORD        DecompressedSize;
	BYTE         EncryptionMethod;

	EID3TextEncoding Encoding;
	FString          Data;


	const TCHAR* GetID( TCHAR* OutID ) const { return Header.GetID(OutID); }
	INT GetSize() const { return Header.GetSize(); }

	friend FArchive& operator<<( FArchive& Ar, FID3V2Frame& Frame )
	{
		if ( Frame.Header.Flags.Compressed ) Ar << Frame.DecompressedSize;
		if ( Frame.Header.Flags.Encrypted )  Ar << Frame.EncryptionMethod;
		if ( Frame.GetSize() <= 1 )          Ar << Frame.Data;
		else
		{
			Ar << Frame.Encoding;
			INT cnt = Frame.GetSize()-1;
			TArray<TCHAR>& cArray = Frame.Data.GetCharArray();

			if( Ar.IsLoading() )
			{

				cArray.SetSize(cnt+1);
				if( Frame.Encoding == ID3ENC_UNICODE )
					for( INT i=0; i<cnt; i++ )
						{UNICHAR UCh; Ar << UCh; cArray(i)=FromUnicode(UCh);}
				else
					for( INT i=0; i<cnt; i++ )
						{ANSICHAR ACh; Ar << *(BYTE*)&ACh; cArray(i)=FromAnsi(ACh);}

				if( cArray.Num()==1 )
					Frame.Data.Empty();
				else cArray(cnt) = 0;
			}
			else
			{
				if( Frame.Encoding == ID3ENC_UNICODE )
					for( INT i=0; i<cnt; i++ )
						{UNICHAR UCh=ToUnicode(cArray(i)); Ar << UCh;}
				else
					for( INT i=0; i<cnt; i++ )
						{ANSICHAR ACh=ToAnsi(cArray(i)); Ar << *(BYTE*)&ACh;}
			}
		}

		return Ar;
	}

	void DumpFrame()
	{
		TCHAR str [5];
		INT Index = INDEX_NONE;
		FString Description;

		GetID(str);
		if ( !str[0] )
		{
			debugf(TEXT(" FrameID: INVALID"));
			return;
		}

		FrameTypeList* List = UStreamInteraction::GetFrameTypes();
		Index = List->FindFrameTypeIndex(str);
		check(Index > INDEX_NONE && Index < ARRAY_COUNT(List->FrameTypes));

		debugf( TEXT("     Frame: %s %s"), str, *List->FrameTypes[Index].Description );
		debugf( TEXT("            %s"), *Data);
	}

	FID3V2Frame( EID3TextEncoding InEncoding = ID3ENC_ANSI ) : Encoding(InEncoding) { }
	FID3V2Frame( FFrameHeader InHeader, EID3TextEncoding InEncoding = ID3ENC_ANSI  ) : Header(InHeader), Encoding(InEncoding) { }
};

struct ENGINE_API FID3TagV2 : public FID3Tag
{
	FID3V2TagHeader         Header;
	FID3V2ExtendedTagHeader ExtendedHeader;
	TArray<FID3V2Frame>     Frames;

	INT GetSize() const { return Header.GetSize(); }

	friend FArchive& operator<<( FArchive& Ar, FID3TagV2& Tag )
	{
		Ar.Seek(0);

		Ar << Tag.Header;
		if ( Tag.Header.Flags.ExtendedHeader ) Ar << Tag.ExtendedHeader;

		if ( Ar.IsLoading() )
		{
			INT cnt = Tag.GetSize() + sizeof(FID3V2TagHeader);

			// Get header
			while ( Ar.Tell() < cnt )
			{
				FFrameHeader Header;
				Ar << Header;
				if ( Header.GetSize() <= 0 )
					break;

				if ( Tag.AddFrame(Ar, Header.IDValue) )
				{
					FID3V2Frame* Frame = new(Tag.Frames) FID3V2Frame(Header);
					Ar << *Frame;
				}
				else Ar.Seek( Ar.Tell() + Header.GetSize() );
			}
		}

		else if ( Ar.IsSaving() )
		{
			for ( INT i = 0; i < Tag.Frames.Num(); i++ )
				Ar << Tag.Frames(i);
		}

		return Ar;
	}

	UBOOL AddFrame( FArchive& Ar, DWORD FrameID )
	{
		if ( (FrameID & 0xFF) != 84 ) // first letter of frameID is T
			return 0;

		return 1;
	}

	void DumpTag()
	{
		debugf( TEXT("  == V2 Tag ==  ") );
		for ( INT i = 0; i < Frames.Num(); i++ )
			Frames(i).DumpFrame();
	}

// Some versions of GNU C++ seem to need a little nudging here. --ryan.
#ifdef __GNUC__
    void *operator new(size_t x) { return ::operator new(x); }
#endif

};

// =====================================================================================================================
// =====================================================================================================================
//  ID3 version 1.1
// =====================================================================================================================
// =====================================================================================================================

struct ENGINE_API FID3TagV1 : public FID3Tag
{
	BYTE Identifier [3] GCC_PACK(4);
	BYTE SongTitle [30];
	BYTE Artist    [30];
	BYTE Album     [30];
	BYTE Year      [4];
	BYTE Comment   [28];
	BYTE Padding;
	BYTE TrackNum;
	ETagGenre Genre;

	friend FArchive& operator<<( FArchive& Ar, FID3TagV1& Tag )
	{
		INT i;

		if ( Ar.TotalSize() < 128 )
			return Ar;

		Ar.Seek( Ar.TotalSize() - 128 );
		SERIALIZEARRAY(Tag.Identifier);
		SERIALIZEARRAY(Tag.SongTitle);
		SERIALIZEARRAY(Tag.Artist);
		SERIALIZEARRAY(Tag.Album);
		SERIALIZEARRAY(Tag.Year);
		SERIALIZEARRAY(Tag.Comment);
		Ar << Tag.Padding << Tag.TrackNum << Tag.Genre;
		return Ar;
	}

	void DumpTag()
	{
		debugf( TEXT("  == V1 Tag ==  ") );
		debugf( TEXT("     Title: %s"), appFromAnsi((ANSICHAR*)SongTitle,NULL,30) );
		debugf( TEXT("    Artist: %s"), appFromAnsi((ANSICHAR*)Artist,NULL,30) );
		debugf( TEXT("     Album: %s"), appFromAnsi((ANSICHAR*)Album,NULL,30) );
		debugf( TEXT("      Year: %s"), appFromAnsi((ANSICHAR*)Year,NULL,4) );
		debugf( TEXT("     Track: %i"), TrackNum );
	}

	FID3TagV1( BYTE InPadding = 0, BYTE InTrackNum = 0, ETagGenre InGenre = GENRE_Blues) 
		: Padding(InPadding),TrackNum(InTrackNum),Genre(InGenre)
	{
		MEMZEROARRAY(Identifier);
		MEMZEROARRAY(SongTitle);
		MEMZEROARRAY(Artist);
		MEMZEROARRAY(Album);
		MEMZEROARRAY(Year);
		MEMZEROARRAY(Comment);
	}

	UBOOL RenderFields( class UStreamTag* Tag ) const;
	void ByteToString( const BYTE* Text, FString& Str, INT Count ) const;
};

#endif // __ID3Tag_H__

#endif // NAMES_ONLY