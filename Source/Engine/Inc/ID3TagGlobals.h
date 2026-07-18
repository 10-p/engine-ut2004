/*=============================================================================
	ID3TagGlobals.h: Enums, lists, types, etc. for ID3 tags
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision History:
	Created by Ron Prestenback.
=============================================================================*/

#ifndef NAMES_ONLY

#ifndef __ID3TAGGLOBALS_H__
#define __ID3TAGGLOBALS_H__

#define SERIALIZEARRAY( name ) for ( i = 0; i < ARRAY_COUNT(name); i++ ) Ar << name[i];
#define MEMZEROARRAY( name ) appMemzero( name, ARRAY_COUNT(name) )

enum ETagHeaderFlagType
{
	THFLAG_Unsynch           = (1 << 15),
	THFLAG_Extended          = (1 << 14),
	THFLAG_Experimental      = (1 << 13),
};

enum EExtendedHeaderFlag
{
	ETHFlag_CRCExists        = (1 << 15),
};

enum EFrameHeaderFlagType
{
	FHFLAG_PreserveTagAlter  = (1 << 15),
	FHFLAG_PreserveFileAlter = (1 << 14),
	FHFLAG_ReadOnly          = (1 << 13),
	FHFLAG_Compressed        = (1 << 7),
	FHFLAG_Encrypted         = (1 << 6),
	FHFLAG_GroupingInfo      = (1 << 5),

	FHFLAG_PreserveAlways    = FHFLAG_PreserveTagAlter | FHFLAG_PreserveFileAlter,
};

enum EFrameID
{
  /* ???? */ FRAME_NOFRAME = 0,       /**< No known frame */
  ///* AENC */ FRAME_AUDIOCRYPTO,       /**< Audio encryption */
  ///* APIC */ FRAME_PICTURE,           /**< Attached picture */
  ///* ASPI */ FRAME_AUDIOSEEKPOINT,    /**< Audio seek point index */
  ///* COMM */ FRAME_COMMENT,           /**< Comments */
  ///* COMR */ FRAME_COMMERCIAL,        /**< Commercial frame */
  ///* ENCR */ FRAME_CRYPTOREG,         /**< Encryption method registration */
  ///* EQU2 */ FRAME_EQUALIZATION2,     /**< Equalisation (2) */
  ///* EQUA */ FRAME_EQUALIZATION,      /**< Equalization */
  ///* ETCO */ FRAME_EVENTTIMING,       /**< Event timing codes */
  ///* GEOB */ FRAME_GENERALOBJECT,     /**< General encapsulated object */
  ///* GRID */ FRAME_GROUPINGREG,       /**< Group identification registration */
  ///* IPLS */ FRAME_INVOLVEDPEOPLE,    /**< Involved people list */
  ///* LINK */ FRAME_LINKEDINFO,        /**< Linked information */
  ///* MCDI */ FRAME_CDID,              /**< Music CD identifier */
  ///* MLLT */ FRAME_MPEGLOOKUP,        /**< MPEG location lookup table */
  ///* OWNE */ FRAME_OWNERSHIP,         /**< Ownership frame */
  ///* PRIV */ FRAME_PRIVATE,           /**< Private frame */
  ///* PCNT */ FRAME_PLAYCOUNTER,       /**< Play counter */
  ///* POPM */ FRAME_POPULARIMETER,     /**< Popularimeter */
  ///* POSS */ FRAME_POSITIONSYNC,      /**< Position synchronisation frame */
  ///* RBUF */ FRAME_BUFFERSIZE,        /**< Recommended buffer size */
  ///* RVA2 */ FRAME_VOLUMEADJ2,        /**< Relative volume adjustment (2) */
  ///* RVAD */ FRAME_VOLUMEADJ,         /**< Relative volume adjustment */
  ///* RVRB */ FRAME_REVERB,            /**< Reverb */
  ///* SEEK */ FRAME_SEEKFRAME,         /**< Seek frame */
  ///* SIGN */ FRAME_SIGNATURE,         /**< Signature frame */
  ///* SYLT */ FRAME_SYNCEDLYRICS,      /**< Synchronized lyric/text */
  ///* SYTC */ FRAME_SYNCEDTEMPO,       /**< Synchronized tempo codes */
  /* TALB */ FRAME_ALBUM,             /**< Album/Movie/Show title */
  /* TBPM */ FRAME_BPM,               /**< BPM (beats per minute) */
  /* TCOM */ FRAME_COMPOSER,          /**< Composer */
  /* TCON */ FRAME_CONTENTTYPE,       /**< Content type */
  /* TCOP */ FRAME_COPYRIGHT,         /**< Copyright message */
  /* TDAT */ FRAME_DATE,              /**< Date */
  /* TDEN */ FRAME_ENCODINGTIME,      /**< Encoding time */
  /* TDLY */ FRAME_PLAYLISTDELAY,     /**< Playlist delay */
  /* TDOR */ FRAME_ORIGRELEASETIME,   /**< Original release time */
  /* TDRC */ FRAME_RECORDINGTIME,     /**< Recording time */
  /* TDRL */ FRAME_RELEASETIME,       /**< Release time */
  /* TDTG */ FRAME_TAGGINGTIME,       /**< Tagging time */
  /* TIPL */ FRAME_INVOLVEDPEOPLE2,   /**< Involved people list */
  /* TENC */ FRAME_ENCODEDBY,         /**< Encoded by */
  /* TEXT */ FRAME_LYRICIST,          /**< Lyricist/Text writer */
  /* TFLT */ FRAME_FILETYPE,          /**< File type */
  /* TIME */ FRAME_TIME,              /**< Time */
  /* TIT1 */ FRAME_CONTENTGROUP,      /**< Content group description */
  /* TIT2 */ FRAME_TITLE,             /**< Title/songname/content description */
  /* TIT3 */ FRAME_SUBTITLE,          /**< Subtitle/Description refinement */
  /* TKEY */ FRAME_INITIALKEY,        /**< Initial key */
  /* TLAN */ FRAME_LANGUAGE,          /**< Language(s) */
  /* TLEN */ FRAME_SONGLEN,           /**< Length */
  /* TMCL */ FRAME_MUSICIANCREDITLIST,/**< Musician credits list */
  /* TMED */ FRAME_MEDIATYPE,         /**< Media type */
  /* TMOO */ FRAME_MOOD,              /**< Mood */
  /* TOAL */ FRAME_ORIGALBUM,         /**< Original album/movie/show title */
  /* TOFN */ FRAME_ORIGFILENAME,      /**< Original filename */
  /* TOLY */ FRAME_ORIGLYRICIST,      /**< Original lyricist(s)/text writer(s) */
  /* TOPE */ FRAME_ORIGARTIST,        /**< Original artist(s)/performer(s) */
  /* TORY */ FRAME_ORIGYEAR,          /**< Original release year */
  /* TOWN */ FRAME_FILEOWNER,         /**< File owner/licensee */
  /* TPE1 */ FRAME_LEADARTIST,        /**< Lead performer(s)/Soloist(s) */
  /* TPE2 */ FRAME_BAND,              /**< Band/orchestra/accompaniment */
  /* TPE3 */ FRAME_CONDUCTOR,         /**< Conductor/performer refinement */
  /* TPE4 */ FRAME_MIXARTIST,         /**< Interpreted, remixed, or otherwise modified by */
  /* TPOS */ FRAME_PARTINSET,         /**< Part of a set */
  /* TPRO */ FRAME_PRODUCEDNOTICE,    /**< Produced notice */
  /* TPUB */ FRAME_PUBLISHER,         /**< Publisher */
  /* TRCK */ FRAME_TRACKNUM,          /**< Track number/Position in set */
  /* TRDA */ FRAME_RECORDINGDATES,    /**< Recording dates */
  /* TRSN */ FRAME_NETRADIOSTATION,   /**< Internet radio station name */
  /* TRSO */ FRAME_NETRADIOOWNER,     /**< Internet radio station owner */
  /* TSIZ */ FRAME_SIZE,              /**< Size */
  /* TSOA */ FRAME_ALBUMSORTORDER,    /**< Album sort order */
  /* TSOP */ FRAME_PERFORMERSORTORDER,/**< Performer sort order */
  /* TSOT */ FRAME_TITLESORTORDER,    /**< Title sort order */
  /* TSRC */ FRAME_ISRC,              /**< ISRC (international standard recording code) */
  /* TSSE */ FRAME_ENCODERSETTINGS,   /**< Software/Hardware and settings used for encoding */
  /* TSST */ FRAME_SETSUBTITLE,       /**< Set subtitle */
  /* TXXX */ FRAME_USERTEXT,          /**< User defined text information */
  /* TYER */ FRAME_YEAR,              /**< Year */
  ///* UFID */ FRAME_UNIQUEFILEID,      /**< Unique file identifier */
  ///* USER */ FRAME_TERMSOFUSE,        /**< Terms of use */
  ///* USLT */ FRAME_UNSYNCEDLYRICS,    /**< Unsynchronized lyric/text transcription */
  ///* WCOM */ FRAME_WWWCOMMERCIALINFO, /**< Commercial information */
  ///* WCOP */ FRAME_WWWCOPYRIGHT,      /**< Copyright/Legal infromation */
  ///* WOAF */ FRAME_WWWAUDIOFILE,      /**< Official audio file webpage */
  ///* WOAR */ FRAME_WWWARTIST,         /**< Official artist/performer webpage */
  ///* WOAS */ FRAME_WWWAUDIOSOURCE,    /**< Official audio source webpage */
  ///* WORS */ FRAME_WWWRADIOPAGE,      /**< Official internet radio station homepage */
  ///* WPAY */ FRAME_WWWPAYMENT,        /**< Payment */
  ///* WPUB */ FRAME_WWWPUBLISHER,      /**< Official publisher webpage */
  ///* WXXX */ FRAME_WWWUSER,           /**< User defined URL link */
  ///*      */ FRAME_METACRYPTO,        /**< Encrypted meta frame (id3v2.2.x) */
  ///*      */ FRAME_METACOMPRESSION,   /**< Compressed meta frame (id3v2.2.1) */
  /* >>>> */ FRAME_LASTFRAMEID        /**< Last field placeholder */
};

struct FrameIDType
{
	TCHAR IDCode[5] GCC_PACK(4);

	EFrameID Type;
	FString  Description;

	FrameIDType() { }
	FrameIDType( const TCHAR InCode[5], const EFrameID InType, const FString& InDescription ) : Type(InType), Description(InDescription)
	{
		appStrncpy( IDCode, InCode, 5 );
		IDCode[4] = 0;
	}

	UBOOL TestFrameID( const TCHAR* FrameIDText, BYTE& OutId, FString& OutDescription )
	{
		if ( !appStrncmp(FrameIDText, IDCode, 5) )
		{
			OutId = Type;
			OutDescription = Description;

			return 1;
		}

		return 0;
	}
};

struct FrameTypeList
{
	static bool bInitialized;
	FrameIDType FrameTypes[50];
	INT FindFrameTypeIndex( const TCHAR* Test )
	{
		guard(FindFrameTypeIndex);

		for ( INT i = 0; i < ARRAY_COUNT(FrameTypes); i++ )
			if ( !appStrncmp(Test, FrameTypes[i].IDCode, 5) )
				return i;

		return INDEX_NONE;

		unguard;
	}

	FrameTypeList()
	{
		if ( !bInitialized )
		{
			bInitialized = true;
			FrameTypes[0] = FrameIDType( TEXT("TALB"), FRAME_ALBUM,	             FString(TEXT("Album/Movie/Show title")) );
			FrameTypes[1] = FrameIDType( TEXT("TBPM"), FRAME_BPM,                FString(TEXT("BPM (beats per minute)")) );
			FrameTypes[2] = FrameIDType( TEXT("TCOM"), FRAME_COMPOSER,           FString(TEXT("Composer")) );
			FrameTypes[3] = FrameIDType( TEXT("TCON"), FRAME_CONTENTTYPE,        FString(TEXT("Content type")) );
			FrameTypes[4] = FrameIDType( TEXT("TCOP"), FRAME_COPYRIGHT,          FString(TEXT("Copyright message")) );
			FrameTypes[5] = FrameIDType( TEXT("TDAT"), FRAME_DATE,               FString(TEXT("Date")) );
			FrameTypes[6] = FrameIDType( TEXT("TDEN"), FRAME_ENCODINGTIME,       FString(TEXT("Encoding time ")) );
			FrameTypes[7] = FrameIDType( TEXT("TDLY"), FRAME_PLAYLISTDELAY,	     FString(TEXT("Playlist delay")) );
			FrameTypes[8] = FrameIDType( TEXT("TDOR"), FRAME_ORIGRELEASETIME,    FString(TEXT("Original release time")) );
			FrameTypes[9] = FrameIDType( TEXT("TDRC"), FRAME_RECORDINGTIME,	     FString(TEXT("Recording time")) );
			FrameTypes[10] = FrameIDType( TEXT("TDRL"), FRAME_RELEASETIME,       FString(TEXT("Release time")) );
			FrameTypes[11] = FrameIDType( TEXT("TDTG"), FRAME_TAGGINGTIME,       FString(TEXT("Tagging time")) );
			FrameTypes[12] = FrameIDType( TEXT("TIPL"), FRAME_INVOLVEDPEOPLE2,   FString(TEXT("Involved people list")) );
			FrameTypes[13] = FrameIDType( TEXT("TENC"), FRAME_ENCODEDBY,         FString(TEXT("Encoded by")) );
			FrameTypes[14] = FrameIDType( TEXT("TEXT"), FRAME_LYRICIST,          FString(TEXT("Lyricist/Text writer")) );
			FrameTypes[15] = FrameIDType( TEXT("TFLT"), FRAME_FILETYPE,          FString(TEXT("File type")) );
			FrameTypes[16] = FrameIDType( TEXT("TIME"), FRAME_TIME,              FString(TEXT("Time")) );
			FrameTypes[17] = FrameIDType( TEXT("TIT1"), FRAME_CONTENTGROUP,      FString(TEXT("Content group description")) );
			FrameTypes[18] = FrameIDType( TEXT("TIT2"), FRAME_TITLE,             FString(TEXT("Title/songname/content description")) );
			FrameTypes[19] = FrameIDType( TEXT("TIT3"), FRAME_SUBTITLE,          FString(TEXT("Subtitle/Description refinement")) );
			FrameTypes[20] = FrameIDType( TEXT("TKEY"), FRAME_INITIALKEY,        FString(TEXT("Initial key")) );
			FrameTypes[21] = FrameIDType( TEXT("TLAN"), FRAME_LANGUAGE,          FString(TEXT("Language(s)")) );
			FrameTypes[22] = FrameIDType( TEXT("TLEN"), FRAME_SONGLEN,           FString(TEXT("Length")) );
			FrameTypes[23] = FrameIDType( TEXT("TMED"), FRAME_MEDIATYPE,         FString(TEXT("Media type")) );
			FrameTypes[24] = FrameIDType( TEXT("TMOO"), FRAME_MOOD,              FString(TEXT("Mood")) );
			FrameTypes[25] = FrameIDType( TEXT("TOAL"), FRAME_ORIGALBUM,         FString(TEXT("Original album/movie/show title")) );
			FrameTypes[26] = FrameIDType( TEXT("TOFN"), FRAME_ORIGFILENAME,      FString(TEXT("Original filename")) );
			FrameTypes[27] = FrameIDType( TEXT("TOLY"), FRAME_ORIGLYRICIST,      FString(TEXT("Original lyricist(s)/text writer(s)")) );
			FrameTypes[28] = FrameIDType( TEXT("TOPE"), FRAME_ORIGARTIST,        FString(TEXT("Original artist(s)/performer(s)")) );
			FrameTypes[29] = FrameIDType( TEXT("TORY"), FRAME_ORIGYEAR,          FString(TEXT("Original release year")) );
			FrameTypes[30] = FrameIDType( TEXT("TOWN"), FRAME_FILEOWNER,         FString(TEXT("File owner/licensee")) );
			FrameTypes[31] = FrameIDType( TEXT("TPE1"), FRAME_LEADARTIST,        FString(TEXT("Lead performer(s)/Soloist(s)")) );
			FrameTypes[32] = FrameIDType( TEXT("TPE2"), FRAME_BAND,              FString(TEXT("Band/orchestra/accompaniment")) );
			FrameTypes[33] = FrameIDType( TEXT("TPE3"), FRAME_CONDUCTOR,         FString(TEXT("Conductor/performer refinement")) );
			FrameTypes[34] = FrameIDType( TEXT("TPE4"), FRAME_MIXARTIST,         FString(TEXT("Interpreted, remixed, or otherwise modified by")) );
			FrameTypes[35] = FrameIDType( TEXT("TPOS"), FRAME_PARTINSET,         FString(TEXT("Part of a set")) );
			FrameTypes[36] = FrameIDType( TEXT("TPRO"), FRAME_PRODUCEDNOTICE,    FString(TEXT("Produced notice")) );
			FrameTypes[37] = FrameIDType( TEXT("TPUB"), FRAME_PUBLISHER,         FString(TEXT("Publisher")) );
			FrameTypes[38] = FrameIDType( TEXT("TRCK"), FRAME_TRACKNUM,          FString(TEXT("Track number/Position in set")) );
			FrameTypes[39] = FrameIDType( TEXT("TRDA"), FRAME_RECORDINGDATES,    FString(TEXT("Recording dates")) );
			FrameTypes[40] = FrameIDType( TEXT("TRSN"), FRAME_NETRADIOSTATION,   FString(TEXT("Internet radio station name")) );
			FrameTypes[41] = FrameIDType( TEXT("TRSO"), FRAME_NETRADIOOWNER,     FString(TEXT("Internet radio station owner")) );
			FrameTypes[42] = FrameIDType( TEXT("TSIZ"), FRAME_SIZE,              FString(TEXT("Size")) );
			FrameTypes[43] = FrameIDType( TEXT("TSOA"), FRAME_ALBUMSORTORDER,    FString(TEXT("Album sort order")) );
			FrameTypes[44] = FrameIDType( TEXT("TSOP"), FRAME_PERFORMERSORTORDER,FString(TEXT("Performer sort order")) );
			FrameTypes[45] = FrameIDType( TEXT("TSOT"), FRAME_TITLESORTORDER,    FString(TEXT("Title sort order")) );
			FrameTypes[46] = FrameIDType( TEXT("TSRC"), FRAME_ISRC,              FString(TEXT("ISRC (international standard recording code)")) );
			FrameTypes[47] = FrameIDType( TEXT("TSSE"), FRAME_ENCODERSETTINGS,   FString(TEXT("Software/Hardware and settings used for encoding")) );
			FrameTypes[48] = FrameIDType( TEXT("TSST"), FRAME_SETSUBTITLE,       FString(TEXT("Set subtitle")) );
			FrameTypes[49] = FrameIDType( TEXT("TYER"), FRAME_YEAR,              FString(TEXT("Year")) );

			for ( INT i = 0; i < ARRAY_COUNT(FrameTypes); i++ )
				FrameTypes[i].Description.Shrink();
		}
	}
};

bool FrameTypeList::bInitialized = false;

/*
FrameIDType FrameTypeList::FrameTypes[50] =
{
	FrameIDType( TEXT("TALB"), FRAME_ALBUM,	            FString(TEXT("Album/Movie/Show title")) ),
	FrameIDType( TEXT("TBPM"), FRAME_BPM,               FString(TEXT("BPM (beats per minute)")) ),
	FrameIDType( TEXT("TCOM"), FRAME_COMPOSER,          FString(TEXT("Composer")) ),
	FrameIDType( TEXT("TCON"), FRAME_CONTENTTYPE,       FString(TEXT("Content type")) ),
	FrameIDType( TEXT("TCOP"), FRAME_COPYRIGHT,         FString(TEXT("Copyright message")) ),
	FrameIDType( TEXT("TDAT"), FRAME_DATE,              FString(TEXT("Date")) ),
	FrameIDType( TEXT("TDEN"), FRAME_ENCODINGTIME,      FString(TEXT("Encoding time ")) ),
	FrameIDType( TEXT("TDLY"), FRAME_PLAYLISTDELAY,	    FString(TEXT("Playlist delay")) ),
	FrameIDType( TEXT("TDOR"), FRAME_ORIGRELEASETIME,   FString(TEXT("Original release time")) ),
	FrameIDType( TEXT("TDRC"), FRAME_RECORDINGTIME,	    FString(TEXT("Recording time")) ),
	FrameIDType( TEXT("TDRL"), FRAME_RELEASETIME,       FString(TEXT("Release time")) ),
	FrameIDType( TEXT("TDTG"), FRAME_TAGGINGTIME,       FString(TEXT("Tagging time")) ),
	FrameIDType( TEXT("TIPL"), FRAME_INVOLVEDPEOPLE2,   FString(TEXT("Involved people list")) ),
	FrameIDType( TEXT("TENC"), FRAME_ENCODEDBY,         FString(TEXT("Encoded by")) ),
	FrameIDType( TEXT("TEXT"), FRAME_LYRICIST,          FString(TEXT("Lyricist/Text writer")) ),
	FrameIDType( TEXT("TFLT"), FRAME_FILETYPE,          FString(TEXT("File type")) ),
	FrameIDType( TEXT("TIME"), FRAME_TIME,              FString(TEXT("Time")) ),
	FrameIDType( TEXT("TIT1"), FRAME_CONTENTGROUP,      FString(TEXT("Content group description")) ),
	FrameIDType( TEXT("TIT2"), FRAME_TITLE,             FString(TEXT("Title/songname/content description")) ),
	FrameIDType( TEXT("TIT3"), FRAME_SUBTITLE,          FString(TEXT("Subtitle/Description refinement")) ),
	FrameIDType( TEXT("TKEY"), FRAME_INITIALKEY,        FString(TEXT("Initial key")) ),
	FrameIDType( TEXT("TLAN"), FRAME_LANGUAGE,          FString(TEXT("Language(s)")) ),
	FrameIDType( TEXT("TLEN"), FRAME_SONGLEN,           FString(TEXT("Length")) ),
	FrameIDType( TEXT("TMED"), FRAME_MEDIATYPE,         FString(TEXT("Media type")) ),
	FrameIDType( TEXT("TMOO"), FRAME_MOOD,              FString(TEXT("Mood")) ),
	FrameIDType( TEXT("TOAL"), FRAME_ORIGALBUM,         FString(TEXT("Original album/movie/show title")) ),
	FrameIDType( TEXT("TOFN"), FRAME_ORIGFILENAME,      FString(TEXT("Original filename")) ),
	FrameIDType( TEXT("TOLY"), FRAME_ORIGLYRICIST,      FString(TEXT("Original lyricist(s)/text writer(s)")) ),
	FrameIDType( TEXT("TOPE"), FRAME_ORIGARTIST,        FString(TEXT("Original artist(s)/performer(s)")) ),
	FrameIDType( TEXT("TORY"), FRAME_ORIGYEAR,          FString(TEXT("Original release year")) ),
	FrameIDType( TEXT("TOWN"), FRAME_FILEOWNER,         FString(TEXT("File owner/licensee")) ),
	FrameIDType( TEXT("TPE1"), FRAME_LEADARTIST,        FString(TEXT("Lead performer(s)/Soloist(s)")) ),
	FrameIDType( TEXT("TPE2"), FRAME_BAND,              FString(TEXT("Band/orchestra/accompaniment")) ),
	FrameIDType( TEXT("TPE3"), FRAME_CONDUCTOR,         FString(TEXT("Conductor/performer refinement")) ),
	FrameIDType( TEXT("TPE4"), FRAME_MIXARTIST,         FString(TEXT("Interpreted, remixed, or otherwise modified by")) ),
	FrameIDType( TEXT("TPOS"), FRAME_PARTINSET,         FString(TEXT("Part of a set")) ),
	FrameIDType( TEXT("TPRO"), FRAME_PRODUCEDNOTICE,    FString(TEXT("Produced notice")) ),
	FrameIDType( TEXT("TPUB"), FRAME_PUBLISHER,         FString(TEXT("Publisher")) ),
	FrameIDType( TEXT("TRCK"), FRAME_TRACKNUM,          FString(TEXT("Track number/Position in set")) ),
	FrameIDType( TEXT("TRDA"), FRAME_RECORDINGDATES,    FString(TEXT("Recording dates")) ),
	FrameIDType( TEXT("TRSN"), FRAME_NETRADIOSTATION,   FString(TEXT("Internet radio station name")) ),
	FrameIDType( TEXT("TRSO"), FRAME_NETRADIOOWNER,     FString(TEXT("Internet radio station owner")) ),
	FrameIDType( TEXT("TSIZ"), FRAME_SIZE,              FString(TEXT("Size")) ),
	FrameIDType( TEXT("TSOA"), FRAME_ALBUMSORTORDER,    FString(TEXT("Album sort order")) ),
	FrameIDType( TEXT("TSOP"), FRAME_PERFORMERSORTORDER,FString(TEXT("Performer sort order")) ),
	FrameIDType( TEXT("TSOT"), FRAME_TITLESORTORDER,    FString(TEXT("Title sort order")) ),
	FrameIDType( TEXT("TSRC"), FRAME_ISRC,              FString(TEXT("ISRC (international standard recording code)")) ),
	FrameIDType( TEXT("TSSE"), FRAME_ENCODERSETTINGS,   FString(TEXT("Software/Hardware and settings used for encoding")) ),
	FrameIDType( TEXT("TSST"), FRAME_SETSUBTITLE,       FString(TEXT("Set subtitle")) ),
	FrameIDType( TEXT("TYER"), FRAME_YEAR,              FString(TEXT("Year")) ),
};
*/
enum ETagGenre
{
	GENRE_Blues = 0,
	GENRE_ClassicRock,
	GENRE_Country,
	GENRE_Dance,
	GENRE_Disco,
	GENRE_Funk,
	GENRE_Grunge,
	GENRE_HipHop,
	GENRE_Jazz,
	GENRE_Metal,
	GENRE_NewAge,
	GENRE_Oldies,
	GENRE_Other,
	GENRE_Pop,
	GENRE_RB,
	GENRE_Rap,
	GENRE_Reggae,
	GENRE_Rock,
	GENRE_Techno,
	GENRE_Industrial,
	GENRE_Alternative,
	GENRE_Ska,
	GENRE_DeathMetal,
	GENRE_Pranks,
	GENRE_Soundtrack,
	GENRE_EuroTechno,
	GENRE_Ambient,
	GENRE_TripHop,
	GENRE_Vocal,
	GENRE_JazzFunk,
	GENRE_Fusion,
	GENRE_Trance,
	GENRE_Classical,
	GENRE_Instrumental,
	GENRE_Acid,
	GENRE_House,
	GENRE_Game,
	GENRE_SoundClip,
	GENRE_Gospel,
	GENRE_Noise,
	GENRE_AlternRock,
	GENRE_Bass,
	GENRE_Soul,
	GENRE_Punk,
	GENRE_Space,
	GENRE_Meditative,
	GENRE_InstrumentalPop,
	GENRE_InstrumentalRock,
	GENRE_Ethnic,
	GENRE_Gothic,
	GENRE_Darkwave,
	GENRE_TechnoIndustrial,
	GENRE_Electronic,
	GENRE_PopFolk,
	GENRE_Eurodance,
	GENRE_Dream,
	GENRE_SouthernRock,
	GENRE_Comedy,
	GENRE_Cult,
	GENRE_Gangsta,
	GENRE_Top40,
	GENRE_ChristianRap,
	GENRE_PopFunk,
	GENRE_Jungle,
	GENRE_NativeAmerican,
	GENRE_Cabaret,
	GENRE_NewWave,
	GENRE_Psychadelic,
	GENRE_Rave,
	GENRE_Showtunes,
	GENRE_Trailer,
	GENRE_LoFi,
	GENRE_Tribal,
	GENRE_AcidPunk,
	GENRE_AcidJazz,
	GENRE_Polka,
	GENRE_Retro,
	GENRE_Musical,
	GENRE_RockRoll,
	GENRE_HardRock,
	GENRE_Folk,   	// Following Are WinAmp Extentions,
	GENRE_FolkRock,
	GENRE_NationalFolk,
	GENRE_Swing,
	GENRE_FastFusion,
	GENRE_Bebob,
	GENRE_Latin,
	GENRE_Revival,
	GENRE_Celtic,
	GENRE_Bluegrass,
	GENRE_Avantgarde,
	GENRE_GothicRock,
	GENRE_ProgressiveRock,
	GENRE_PsychedelicRock,
	GENRE_SymphonicRock,
	GENRE_SlowRock,
	GENRE_BigBand,
	GENRE_Chorus,
	GENRE_EasyListening,
	GENRE_Acoustic,
	GENRE_Humour,
	GENRE_Speech,
	GENRE_Chanson,
	GENRE_Opera,
	GENRE_ChamberMusic,
	GENRE_Sonata,
	GENRE_Symphony,
	GENRE_BootyBass,
	GENRE_Primus,
	GENRE_PornGroove,
	GENRE_Satire,
	GENRE_SlowJam,
	GENRE_Club,
	GENRE_Tango,
	GENRE_Samba,
	GENRE_Folklore,
	GENRE_Ballad,
	GENRE_PowerBallad,
	GENRE_RhythmicSoul,
	GENRE_Freestyle,
	GENRE_Duet,
	GENRE_PunkRock,
	GENRE_DrumSolo,
	GENRE_Acapella,
	GENRE_EuroHouse,
	GENRE_DanceHall,
	GENRE_Goa,
	GENRE_DrumBass,
	GENRE_ClubHouse,
	GENRE_Hardcore,
	GENRE_Terror,
	GENRE_Indie,
	GENRE_Britpop,
	GENRE_Negerpunk,
	GENRE_PolskPunk,
	GENRE_Beat,
	GENRE_ChristianGangstaRap,
	GENRE_HeavyMetal,
	GENRE_BlackMetal,
	GENRE_Crossover,
	GENRE_ContemporaryChristian,
	GENRE_ChristianRock,
	GENRE_Merengue,
	GENRE_Salsa,
	GENRE_TrashMetal,
	GENRE_Anime,
	GENRE_JPop,
	GENRE_Synthpop,
};

enum  EID3TextEncoding
{
	ID3ENC_ANSI    = 0,
	ID3ENC_UNICODE = 1,
};


struct ENGINE_API FID3V2TagVersion
{
	BYTE MajorVersion;
	BYTE MinorVersion;

	friend FArchive& operator<<( FArchive& Ar, FID3V2TagVersion& Version )
	{
		Ar << Version.MajorVersion << Version.MinorVersion;
		return Ar;
	}
};

FArchive& operator<<( FArchive& Ar, EID3TextEncoding& Encoding );
FArchive& operator<<( FArchive& Ar, ETagGenre& Genre );

struct ENGINE_API FID3Tag
{
	virtual void DumpTag() = 0;
};

#endif // __ID3TAGGLOBALS_H__
#endif // NAMES_ONLY
