/*=============================================================================
	UStreamInterface.cpp: Streaming music file manager / ID3 Tag reader/writer
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Ron Prestenback
=============================================================================*/
#include "UStreamPrivate.h"

IMPLEMENT_CLASS(UStreamBase);
IMPLEMENT_CLASS(UStreamInteraction);	// handles input & stream notifications
IMPLEMENT_CLASS(UStreamInterface);		// file manager
IMPLEMENT_CLASS(UStream);               // contains information regarding a single song file
IMPLEMENT_CLASS(UStreamTag);            // contains ID3 tag information

FArchive& operator<<( FArchive& Ar, EID3TextEncoding& Encoding )
{
	BYTE* B = (BYTE*)&Encoding;
	Ar << *B;
	return Ar;
}

FArchive& operator<<( FArchive& Ar, ETagGenre& Genre )
{
	BYTE* G = (BYTE*)&Genre;
	Ar << *G;
	return Ar;
}

FFilePath::FFilePath()
{ }

FFilePath::FFilePath( const TCHAR* FullPathName )
: FullPath(FullPathName)
{
	INT pos = FullPath.InStr(PATH_SEPARATOR,1);
	if ( pos != INDEX_NONE )
		pos++;

	Directory = FullPath.Left(pos);
	INT extpos = FullPath.InStr(TEXT("."),1);
	if ( extpos == INDEX_NONE )
	{
		extpos = FullPath.Len();
		FullPath += TEXT(".mp3");
	}

	Extension = FullPath.Mid(extpos+1);
	if ( Extension != TEXT("mp3") && Extension != TEXT("ogg") )
		debugf(TEXT("Invalid extension %s"), *Extension);

	Filename = FullPath.Left(extpos).Mid(pos);
}

FFilePath::FFilePath( const TCHAR* InDir, const TCHAR* InFileName, const TCHAR* InExtension )
: Directory(InDir), Filename(InFileName), Extension(InExtension)
{
	if ( Directory.Right(1) != PATH_SEPARATOR )
		Directory += PATH_SEPARATOR;
	FullPath = Directory + Filename + TEXT(".") + Extension;
}

FFilePath::FFilePath( TArray<FString>& InDirectories, const TCHAR* InDir, const TCHAR* InFileName, const TCHAR* InExtension )
: DirectoryParts(InDirectories), Directory(InDir), Filename(InFileName), Extension(InExtension)
{ }

// =======================================================================================================================================================
// =======================================================================================================================================================
// UStreamBase
// =======================================================================================================================================================
// =======================================================================================================================================================

void UStreamBase::execGetPathSeparator( FFrame& Stack, RESULT_DECL )
{
	guard(UStreamBase::execGetPathSeparator);

	P_FINISH;

	*(FString*)Result = PATH_SEPARATOR;
	unguardexec;
}

void UStreamBase::execIsCaseSensitive( FFrame& Stack, RESULT_DECL )
{
	guard(UStreamBase::execIsCaseSensitive);

	P_FINISH;

	*(UBOOL*)Result = GFileManager ? GFileManager->IsCaseSensitive() : 1;

	unguardexec;
}

void UStreamBase::execMultiply_StrStr( FFrame& Stack, RESULT_DECL )
{
	guard(UStreamInterface::execMultiply_StrStr);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(FString*)Result = A * B;

	unguardexec;
}

void UStreamBase::execMultiplyEqual_StrStr( FFrame& Stack, RESULT_DECL )
{
	guard(UStreamInterface::execMultiplyEqual_StrStr);

	P_GET_STR_REF(A);
	P_GET_STR(B);
	P_FINISH;

	*(FString*)Result = ( *A *= B );

	unguardexec;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UStreamInteraction
// =======================================================================================================================================================
// =======================================================================================================================================================

void UStreamInteraction::execIsPaused( FFrame& Stack, RESULT_DECL )
{
	guard(UStreamInteraction::execIsPaused);

	P_GET_INT_OPTX(SongHandle,CurrentSongHandle);
	P_FINISH;

	if ( SongHandle != UCONST_INVALIDSONGHANDLE && ViewportOwner && ViewportOwner->Actor && ViewportOwner->Actor->GetLevel()->Engine->Audio )
		*(UBOOL*)Result = ViewportOwner->Actor->GetLevel()->Engine->Audio->IsPaused(SongHandle);

	unguardexec;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UStreamInterface
// =======================================================================================================================================================
// =======================================================================================================================================================

void UStreamInterface::execGetBaseDirectory( FFrame& Stack, RESULT_DECL )
{
	guard(UStreamInterface::execGetBaseDirectory);

	P_FINISH;

	FString OldBaseDir = GFileManager->GetDefaultDirectory();
	if ( GFileManager->SetDefaultDirectory(TEXT("..\\Music")) )
		*(FString*)Result = *GFileManager->GetDefaultDirectory();
	else *(FString*)Result = OldBaseDir;

	GFileManager->SetDefaultDirectory(*OldBaseDir);

	unguardexec;
}

void UStreamInterface::execCreateDirectory( FFrame& Stack, RESULT_DECL )
{
	guard(UStreamInterface::execCreateDirectory);

	P_GET_STR(DirectoryName);
	P_FINISH;

	FString& StringResult = *(FString*)Result = TEXT("");
	if ( !GFileManager || DirectoryName == TEXT("") )
		return;

	if ( GFileManager->MakeDirectory(*DirectoryName) )
		StringResult = GFileManager->ExpandPath(*DirectoryName);

	unguardexec;
}

void UStreamInterface::execRemoveDirectory( FFrame& Stack, RESULT_DECL )
{
	guard(UStreamInterface::execRemoveDirectory);

	P_GET_STR(DirectoryName);
	P_FINISH;

	*(UBOOL*)Result = GFileManager && GFileManager->DeleteDirectory(*DirectoryName);
	unguardexec;
}

void UStreamInterface::execValidDirectory( FFrame& Stack, RESULT_DECL )
{
	guard(UStreamInterface::execValidDirectory);

	P_GET_STR_OPTX(DirectoryName,CurrentDirectory);
	P_FINISH;

	if ( !GFileManager )
	{
		*(UBOOL*)Result = 0;
		return;
	}

	FString OldDirectory = GFileManager->GetDefaultDirectory();
	*(UBOOL*)Result = GFileManager->SetDefaultDirectory(*DirectoryName);
	GFileManager->SetDefaultDirectory(*OldDirectory);

	unguardexec;
}

void UStreamInterface::execGetDriveLetters( FFrame& Stack, RESULT_DECL )
{
	guard(UStreamInterface::execGetDriveLetters);

	P_GET_TARRAY_REF(Letters,FString);
	P_FINISH;

	FString Drives;

	if ( GFileManager->GetDriveLetters( Drives ) )
		Drives.ParseIntoArray( TEXT(";"), Letters );

	*(UBOOL*)Result = Letters->Num();
	unguard;
}

void UStreamInterface::execGetDirectoryContents( FFrame& Stack, RESULT_DECL )
{
	guard(UStreamInterface::execGetDirectoryContents);

	P_GET_TARRAY_REF(Files,FString);
	P_GET_STR_OPTX(DirectoryName,TEXT("."));
	P_GET_BYTE_OPTX(FileType,1);
	P_FINISH;

	UBOOL& BoolResult = *(UBOOL*)Result;
	TArray<FString>& ArrayResult = *Files;

	if ( GFileManager )
	{
//		debugf(NAME_MusicPlayer, TEXT("GetDirectoryContents  Type:%i %s"), FileType, *DirectoryName);

		if ( FileType == FILE_Directory )
		{
			if ( DirectoryName.Right(1) == PATH_SEPARATOR )
				DirectoryName += TEXT("*.*");

			else if ( eventParseExtension(DirectoryName) == TEXT("") )
				DirectoryName += FString::Printf(TEXT("%s%s"), PATH_SEPARATOR, TEXT("*"));

			ArrayResult += GFileManager->FindFiles( *DirectoryName, 0, 1 );
		}
		else
		{
			INT dotpos = DirectoryName.InStr(TEXT("."),1);
			INT pathpos = DirectoryName.InStr(PATH_SEPARATOR,1);
			if ( dotpos != INDEX_NONE && (pathpos == INDEX_NONE || pathpos < dotpos) )
				DirectoryName = DirectoryName.Left(dotpos);

			else if ( DirectoryName.Right(1) == PATH_SEPARATOR )
				DirectoryName += TEXT("*");

			FString Extension = eventConvertToFileExtension(FileType);
			if ( Extension == TEXT("") )
			{
				debugf(NAME_MusicPlayer, TEXT("StreamInterface.GetDirectoryContents() - Invalid filetype requested %i"), FileType);
				return;
			}

			dotpos = Extension.InStr(TEXT(";"));
			if ( dotpos != INDEX_NONE )
			{
				// Handle multiple file extensions for this type
				TArray<FString> Extensions;
				Extension.ParseIntoArray(TEXT(";"), &Extensions);
				for ( INT i = 0; i < Extensions.Num(); i++ )
					ArrayResult += GFileManager->FindFiles( *(DirectoryName + Extensions(i)), 1, 0 );
			}
			else ArrayResult += GFileManager->FindFiles( *(DirectoryName + Extension), 1, 0 );
		}
	}

	BoolResult = Files->Num() > 0;
	unguardexec;
}

void UStreamInterface::execValidFile( FFrame& Stack, RESULT_DECL )
{
	guard(UStreamInterface::execValidFile);

	P_GET_STR(FileName);
	P_FINISH;

	*(UBOOL*)Result = GFileManager && GFileManager->FileSize(*FileName) > 0;

	unguardexec;
}

void UStreamInterface::execCreateStream( FFrame& Stack, RESULT_DECL )
{
	guard(UStreamInterface::execCreateStream);

	P_GET_STR(FileName);
	P_GET_UBOOL_OPTX(bStrict,0);
	P_FINISH;

	*(UObject**)Result = CreateStream(FileName,bStrict);

	unguardexec;
}

UStream* UStreamInterface::CreateStream( FString& FileName, UBOOL bStrict )
{
	guard(UStreamInterface::CreateStream);

	if ( FileName == TEXT("") || !GFileManager )
		return NULL;

TryAgain:
	if ( GFileManager->FileSize(*FileName) <= 0 )
	{
		if ( !bStrict && CurrentDirectory != TEXT("*") )
		{
			FileName = CurrentDirectory + FileName;
			bStrict = 1;
			goto TryAgain;
		}

		debugf(NAME_MusicPlayer, TEXT("StreamInterface.CreateStream() - file not found '%s'"), *FileName);
		return NULL;
	}

	BYTE Type = eventConvertToFileType(FileName);
	if ( Type != FILE_Stream )
	{
		debugf(NAME_MusicPlayer, TEXT("StreamInterface.CreateStream() - invalid file type specified '%s'"), *FileName);
		return NULL;
	}

	UStream* NewStream = Cast<UStream>( StaticConstructObject(UStream::StaticClass(), UObject::GetTransientPackage(), NAME_None, 0, NULL) );
    if ( !NewStream )
		return NULL;

    NewStream->eventCreated();
	if ( eventParsePath( FileName, NewStream->PathName ) )
		NewStream->LoadID3Tag();
	else debugf(NAME_MusicPlayer, TEXT("StreamInterface.CreateStream() - Error parsing path while attempting to link tag to '%s'"), *FileName);

	NewStream->bReadOnly = GFileManager->IsReadOnly(*FileName);
	NewStream->Type = Type;

	return NewStream;

	unguard;
}

void UStreamInterface::execLoadPlaylist( FFrame& Stack, RESULT_DECL )
{
	guard(UStreamInterface::execLoadPlaylist);

	P_GET_STR(FileName);
	P_GET_TARRAY_REF(Lines,FString);
	P_GET_UBOOL_OPTX(bStrict,0);
	P_FINISH;

	TArray<FString>& Playlist = *Lines;

	if ( eventConvertToFileType(FileName) != FILE_Playlist )
	{
		debugf(NAME_MusicPlayer, TEXT("StreamInterface.LoadPlaylist() - invalid filename specified '%s'"), *FileName);
		*(UBOOL*)Result = 0;
		return;
	}

	Playlist.Empty();
	LoadPlaylist( FileName, Playlist );
	*(UBOOL*)Result = Playlist.Num() > 0;

	unguardexec;
}

void UStreamInterface::LoadPlaylist( FString& FileName, TArray<FString>& Lines, UBOOL bStrict ) const
{
	guard(UStreamInterface::LoadPlaylist);

	FString Result;

	if ( !bStrict && CurrentDirectory != TEXT("*") && GFileManager->FileSize(*FileName) <= 0 )
		FileName = CurrentDirectory + *FileName;

	if ( !appLoadFileToString( Result, *FileName, GFileManager ) )
	{
		debugf(NAME_MusicPlayer, TEXT("StreamInterface.LoadPlaylist() - error attempting to read contents of playlist file '%s'"), *FileName);
		return;
	}

	Result.ParseIntoArray( LINE_TERMINATOR, &Lines );

	unguard;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UStream
// =======================================================================================================================================================
// =======================================================================================================================================================

void UStream::execLoadID3Tag( FFrame& Stack, RESULT_DECL )
{
	guard(UStream::execLoadID3Tag);

	P_FINISH;

	*(UBOOL*)Result = LoadID3Tag();
	unguardexec
}

void UStream::execSaveID3Tag( FFrame& Stack, RESULT_DECL )
{
	guard(UStream::execSaveID3Tag);
	P_FINISH;

	*(UBOOL*)Result = SaveID3Tag();
	unguardexec;
}

UBOOL UStream::LoadID3Tag()
{
	guard(UStream::LoadIDTag);

	if ( PathName.FullPath == TEXT("") )
		return 0;

	if ( !IDTag )
		IDTag = CastChecked<UStreamTag>(StaticConstructObject(UStreamTag::StaticClass(), this));

	return IDTag->LinkTag();
	unguard;
}

UBOOL UStream::SaveID3Tag()
{
	guard(UStream::SaveID3Tag);

	check(IDTag != NULL);
	return IDTag->SaveTag();
	unguard;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UStreamTag
// =======================================================================================================================================================
// =======================================================================================================================================================

UBOOL UStreamTag::HasV2Tag( FArchive* InReader ) const
{
	guard(UStreamTag::HasV2Tag);

	if ( !InReader )
		return 0;

	if ( InReader->TotalSize() < sizeof(FID3TagV2) )
		return 0;

	BYTE Test[3], Compare[3] = { 'I','D','3' };

	InReader->Seek(0);
	InReader->Serialize( Test, 3 );
	InReader->Seek(0);

	for ( INT i = 0; i < 3; i++ )
		if ( Test[i] != Compare[i] )
			return 0;

	return 1;
	unguard;
}

UBOOL UStreamTag::HasV1Tag( FArchive* InReader ) const
{
	guard(UStreamTag::HasV1Tag);

	if ( !InReader )
		return false;

	INT size = InReader->TotalSize();
	if ( size < 128 )
		return false;

	BYTE Test[3], Compare[3] = { 'T','A','G' };

	InReader->Seek( size - 128 );
	InReader->Serialize( &Test, 3 );
	InReader->Seek(0);

	for ( INT i = 0; i < 3; i++ )
		if ( Test[i] != Compare[i] )
			return false;

	return true;
	unguard;
}

UBOOL UStreamTag::LinkTag()
{
	guard(UStreamTag::LinkTag);

	V1Tag TagV1 = (V1Tag)NativeID3Tag[0];
	V2Tag TagV2 = (V2Tag)NativeID3Tag[1];

	FArchive* Reader = GFileManager->CreateFileReader( *(Cast<UStream>(GetOuter())->PathName.FullPath) );
	if ( !Reader ) return 0;
	if ( Reader->IsError() )
	{
		delete Reader;
		return 0;
	}

	if ( HasV2Tag(Reader) )
	{
		if ( !TagV2 ) TagV2 = new FID3TagV2;
		*Reader << *TagV2;
	}

	if ( HasV1Tag(Reader) )
	{
		if ( !TagV1 ) TagV1 = new FID3TagV1;
		*Reader << *TagV1;
	}

	delete Reader;

	if ( TagV2 && TagV2->Frames.Num() )
		for ( INT i = 0; i < TagV2->Frames.Num(); i++ )
			new(Fields) FID3Field(&TagV2->Frames(i));

	else if ( TagV1 )
		TagV1->RenderFields(this);

	if ( TagV1 || TagV2 )
	{
		RefreshShortcuts();
		return 1;
	}

	return 0;

	unguard;
}

UBOOL UStreamTag::SaveTag()
{
	guard(UStreamTag::SaveTag);

	return 0;

	V1Tag TagV1 = (V1Tag)NativeID3Tag[0];
	V2Tag TagV2 = (V2Tag)NativeID3Tag[1];

	if ( !TagV1 && !TagV2 )
		return 0;

	FArchive* Writer = GFileManager->CreateFileWriter( *(Cast<UStream>(GetOuter())->PathName.FullPath), FILEWRITE_AllowRead|FILEWRITE_Unbuffered );
	checkSlow(Writer);

	if ( TagV2 ) *Writer << *TagV2;
	if ( TagV1 ) *Writer << *TagV1;

	delete Writer;
	RefreshShortcuts();

	return 1;

	unguard;
}


/*
TCHAR* GetFrameIDText( ID3_FrameID ID, TCHAR* Text )
{
	switch ( ID )
	{
	case FRAME_NOFRAME:
		appStrcpy(Text, TEXT("FRAME_NOFRAME"));
		break;
	case FRAME_AUDIOCRYPTO:
		appStrcpy(Text, TEXT("FRAME_AUDIOCRYPTO"));
		break;
	case FRAME_PICTURE:
		appStrcpy(Text,TEXT("FRAME_PICTURE"));
		break;
	case FRAME_AUDIOSEEKPOINT:
		appStrcpy(Text, TEXT("FRAME_AUDIOSEEKPOINT"));
		break;
	case FRAME_COMMENT:
		appStrcpy(Text,TEXT("FRAME_COMMENT"));
		break;
	case FRAME_COMMERCIAL:
		appStrcpy(Text,TEXT("FRAME_COMMERCIAL"));
		break;
	case FRAME_CRYPTOREG:
		appStrcpy(Text,TEXT("FRAME_CRYPTOREG"));
		break;
	case FRAME_EQUALIZATION2:
		appStrcpy(Text,TEXT("FRAME_EQUALIZATION2"));
		break;
	case FRAME_EQUALIZATION:
		appStrcpy(Text,TEXT("FRAME_EQUALIZATION"));
		break;
	case FRAME_EVENTTIMING:
		appStrcpy(Text,TEXT("FRAME_EVENTTIMING"));
		break;
	case FRAME_GENERALOBJECT:
		appStrcpy(Text,TEXT("FRAME_GENERALOBJECT"));
		break;

	case FRAME_GROUPINGREG:
		appStrcpy(Text,TEXT("FRAME_GROUPINGREG"));
		break;

	case FRAME_INVOLVEDPEOPLE:
		appStrcpy(Text,TEXT("FRAME_INVOLVEDPEOPLE"));
		break;

	case FRAME_LINKEDINFO:
		appStrcpy(Text,TEXT("FRAME_LINKEDINFO"));
		break;

	case FRAME_CDID:
		appStrcpy(Text,TEXT("FRAME_CDID"));
		break;

	case FRAME_MPEGLOOKUP:
		appStrcpy(Text,TEXT("FRAME_MPEGLOOKUP"));
		break;

	case FRAME_OWNERSHIP:
		appStrcpy(Text,TEXT("FRAME_OWNERSHIP"));
		break;

	case FRAME_PRIVATE:
		appStrcpy(Text,TEXT("FRAME_PRIVATE"));
		break;

	case FRAME_PLAYCOUNTER:
		appStrcpy(Text,TEXT("FRAME_PLAYCOUNTER"));
		break;

	case FRAME_POPULARIMETER:
		appStrcpy(Text,TEXT("FRAME_POPULARIMETER"));
		break;

	case FRAME_POSITIONSYNC:
		appStrcpy(Text,TEXT("FRAME_POSITIONSYNC"));
		break;

	case FRAME_BUFFERSIZE:
		appStrcpy(Text,TEXT("FRAME_BUFFERSIZE"));
		break;

	case FRAME_VOLUMEADJ2:
		appStrcpy(Text,TEXT("FRAME_VOLUMEADJ2"));
		break;

	case FRAME_VOLUMEADJ:
		appStrcpy(Text,TEXT("FRAME_VOLUMEADJ"));
		break;

	case FRAME_REVERB:
		appStrcpy(Text,TEXT("FRAME_REVERB"));
		break;

	case FRAME_SEEKFRAME:
		appStrcpy(Text,TEXT("FRAME_SEEKFRAME"));
		break;
	
	
	case FRAME_SIGNATURE:
		appStrcpy(Text,TEXT("FRAME_SIGNATURE"));
		break;
	
	case FRAME_SYNCEDLYRICS:
		appStrcpy(Text,TEXT("FRAME_SYNCEDLYRICS"));
		break;
		
	case FRAME_SYNCEDTEMPO:
		appStrcpy(Text,TEXT("FRAME_SYNCEDTEMPO"));
		break;
		
	case FRAME_ALBUM:
		appStrcpy(Text,TEXT("FRAME_ALBUM"));
		break;
	
	case FRAME_BPM:
		appStrcpy(Text,TEXT("FRAME_BPM"));
		break;

	case FRAME_COMPOSER:
		appStrcpy(Text,TEXT("FRAME_COMPOSER"));
		break;

	case FRAME_CONTENTTYPE:
		appStrcpy(Text,TEXT("FRAME_CONTENTTYPE"));
		break;

	case FRAME_COPYRIGHT:
		appStrcpy(Text,TEXT("FRAME_COPYRIGHT"));
		break;

	case FRAME_DATE:
		appStrcpy(Text,TEXT("FRAME_DATE"));
		break;

	case FRAME_ENCODINGTIME:
		appStrcpy(Text,TEXT("FRAME_ENCODINGTIME"));
		break;

	case FRAME_PLAYLISTDELAY:
		appStrcpy(Text,TEXT("FRAME_PLAYLISTDELAY"));
		break;

	case FRAME_ORIGRELEASETIME:
		appStrcpy(Text,TEXT("FRAME_ORIGRELEASETIME"));
		break;

	case FRAME_RECORDINGTIME:
		appStrcpy(Text,TEXT("FRAME_RECORDINGTIME"));
		break;

	case FRAME_RELEASETIME:
		appStrcpy(Text,TEXT("FRAME_RELEASETIME"));
		break;

	case FRAME_TAGGINGTIME:
		appStrcpy(Text,TEXT("FRAME_TAGGINGTIME"));
		break;

	case FRAME_INVOLVEDPEOPLE2:
		appStrcpy(Text,TEXT("FRAME_INVOLVEDPEOPLE2"));
		break;

	case FRAME_ENCODEDBY:
		appStrcpy(Text,TEXT("FRAME_ENCODEDBY"));
		break;

	case FRAME_LYRICIST:
		appStrcpy(Text,TEXT("FRAME_LYRICIST"));
		break;

	case FRAME_FILETYPE:
		appStrcpy(Text,TEXT("FRAME_FILETYPE"));
		break;

	case FRAME_TIME:
		appStrcpy(Text,TEXT("FRAME_TIME"));
		break;

	case FRAME_CONTENTGROUP:
		appStrcpy(Text,TEXT("FRAME_CONTENTGROUP"));
		break;

	case FRAME_TITLE:
		appStrcpy(Text,TEXT("FRAME_TITLE"));
		break;

	case FRAME_SUBTITLE:
		appStrcpy(Text,TEXT("FRAME_SUBTITLE"));
		break;

	case FRAME_INITIALKEY:
		appStrcpy(Text,TEXT("FRAME_INITIALKEY"));
		break;

	case FRAME_LANGUAGE:
		appStrcpy(Text,TEXT("FRAME_LANGUAGE"));
		break;

	case FRAME_SONGLEN:
		appStrcpy(Text,TEXT("FRAME_SONGLEN"));
		break;

	case FRAME_MUSICIANCREDITLIST:
		appStrcpy(Text,TEXT("FRAME_MUSICIANCREDITLIST"));
		break;

	case FRAME_MEDIATYPE:
		appStrcpy(Text,TEXT("FRAME_MEDIATYPE"));
		break;

	case FRAME_MOOD:
		appStrcpy(Text,TEXT("FRAME_MOOD"));
		break;

	case FRAME_ORIGALBUM:
		appStrcpy(Text,TEXT("FRAME_ORIGALBUM"));
		break;

	case FRAME_ORIGFILENAME:
		appStrcpy(Text,TEXT("FRAME_ORIGFILENAME"));
		break;

	case FRAME_ORIGLYRICIST:
		appStrcpy(Text,TEXT("FRAME_ORIGLYRICIST"));
		break;

	case FRAME_ORIGARTIST:
		appStrcpy(Text,TEXT("FRAME_ORIGARTIST"));
		break;

	case FRAME_ORIGYEAR:
		appStrcpy(Text,TEXT("FRAME_ORIGYEAR"));
		break;

	case FRAME_FILEOWNER:
		appStrcpy(Text,TEXT("FRAME_FILEOWNER"));
		break;

	case FRAME_LEADARTIST:
		appStrcpy(Text,TEXT("FRAME_LEADARTIST"));
		break;

	case FRAME_BAND:
		appStrcpy(Text,TEXT("FRAME_BAND"));
		break;

	case FRAME_CONDUCTOR:
		appStrcpy(Text,TEXT("FRAME_CONDUCTOR"));
		break;

	case FRAME_MIXARTIST:
		appStrcpy(Text,TEXT("FRAME_MIXARTIST"));
		break;

	case FRAME_PARTINSET:
		appStrcpy(Text,TEXT("FRAME_PARTINSET"));
		break;

	case FRAME_PRODUCEDNOTICE:
		appStrcpy(Text,TEXT("FRAME_PRODUCEDNOTICE"));
		break;

	case FRAME_PUBLISHER:
		appStrcpy(Text,TEXT("FRAME_PUBLISHER"));
		break;

	case FRAME_TRACKNUM:
		appStrcpy(Text,TEXT("FRAME_TRACKNUM"));
		break;

	case FRAME_ENCODERSETTINGS:
		appStrcpy(Text, TEXT("FRAME_ENCODERSETTINGS"));
		break;

	case FRAME_SETSUBTITLE:
		appStrcpy(Text, TEXT("FRAME_SETSUBTITLE"));
		break;

	case FRAME_USERTEXT:
		appStrcpy(Text, TEXT("FRAME_USERTEXT"));
		break;

	case FRAME_YEAR:
		appStrcpy(Text, TEXT("FRAME_YEAR"));
		break;

	case FRAME_RECORDINGDATES:
		appStrcpy(Text,TEXT("FRAME_RECORDINGDATES"));
		break;

	case FRAME_NETRADIOSTATION:
		appStrcpy(Text,TEXT("FRAME_NETRADIOSTATION"));
		break;

	case FRAME_NETRADIOOWNER:
		appStrcpy(Text,TEXT("FRAME_NETRADIOOWNER"));
		break;

	case FRAME_SIZE:
		appStrcpy(Text,TEXT("FRAME_SIZE"));
		break;

	case FRAME_ALBUMSORTORDER:
		appStrcpy(Text,TEXT("FRAME_ALBUMSORTORDER"));
		break;

	case FRAME_PERFORMERSORTORDER:
		appStrcpy(Text,TEXT("FRAME_PERFORMERSORTORDER"));
		break;

	case FRAME_TITLESORTORDER:
		appStrcpy(Text,TEXT("FRAME_TITLESORTORDER"));
		break;

	case FRAME_ISRC:
		appStrcpy(Text,TEXT("FRAME_ISRC"));
		break;

	case FRAME_UNIQUEFILEID:
		appStrcpy(Text, TEXT("FRAME_UNIQUEFILEID"));
		break;

	case FRAME_TERMSOFUSE:
		appStrcpy(Text, TEXT("FRAME_TERMSOFUSE"));
		break;

	case FRAME_UNSYNCEDLYRICS:
		appStrcpy(Text, TEXT("FRAME_UNSYNCEDLYRICS"));
		break;

	case FRAME_WWWCOMMERCIALINFO:
		appStrcpy(Text, TEXT("FRAME_WWWCOMMERCIALINFO"));
		break;

	case FRAME_WWWCOPYRIGHT:
		appStrcpy(Text, TEXT("FRAME_WWWCOPYRIGHT"));
		break;

	case FRAME_WWWAUDIOFILE:
		appStrcpy(Text, TEXT("FRAME_WWWAUDIOFILE"));
		break;

	case FRAME_WWWARTIST:
		appStrcpy(Text, TEXT("FRAME_WWWARTIST"));
		break;

	case FRAME_WWWAUDIOSOURCE:
		appStrcpy(Text, TEXT("FRAME_WWWAUDIOSOURCE"));
		break;

	case FRAME_WWWRADIOPAGE:
		appStrcpy(Text, TEXT("FRAME_WWWRADIOPAGE"));
		break;

	case FRAME_WWWPAYMENT:
		appStrcpy(Text, TEXT("FRAME_WWWPAYMENT"));
		break;

	case FRAME_WWWPUBLISHER:
		appStrcpy(Text, TEXT("FRAME_WWWPUBLISHER"));
		break;

 	case FRAME_WWWUSER:
		appStrcpy(Text, TEXT("FRAME_WWWUSER"));
		break;

	case FRAME_METACRYPTO:
		appStrcpy(Text, TEXT("FRAME_METACRYPTO"));
		break;

	case FRAME_METACOMPRESSION:
		appStrcpy(Text, TEXT("FRAME_METACOMPRESSION"));
		break;

	case FRAME_LASTFRAMEID:
		appStrcpy(Text, TEXT("FRAME_LASTFRAMEID"));
		break;
	}

	return Text;
}
*/
void UStreamTag::execDumpTag( FFrame& Stack, RESULT_DECL )
{
	guard(UStreamTag::execDumpTag);

	P_FINISH;

	V1Tag TagV1 = (V1Tag)NativeID3Tag[0];
	V2Tag TagV2 = (V2Tag)NativeID3Tag[1];

	if ( !TagV1 && !TagV2 )
	{
		debugf(TEXT("No tags!"));
		return;
	}

	debugf(NAME_MusicPlayer, TEXT("TAGDUMP: %s"), *(CastChecked<UStream>(GetOuter())->PathName.FullPath) );
	if ( TagV1 ) TagV1->DumpTag();
	if ( TagV2 ) TagV2->DumpTag();

	unguardexec;
}

void UStreamTag::RefreshShortcuts()
{
	guard(UStreamTag::RefreshShortcuts);

	if ( Fields.Num() == 0 )
	{
		debugf(NAME_MusicPlayer, TEXT("ID3 tag for %s has no fields!"), *(CastChecked<UStream>(GetOuter())->PathName.FullPath));
		return;
	}

	for ( INT i = 0; i < Fields.Num(); i++ )
	{
		switch ( Fields(i).FieldID )
		{
		case FRAME_TITLE:             Title = Fields(i); break;
		case FRAME_TRACKNUM:    TrackNumber = Fields(i); break;
		case FRAME_LEADARTIST:       Artist = Fields(i); break;
		case FRAME_ALBUM:             Album = Fields(i); break;
		case FRAME_YEAR:               Year = Fields(i); break;
		case FRAME_CONTENTTYPE:       Genre = Fields(i); break;
		case FRAME_SONGLEN:        Duration = Fields(i); break;
		}
	}

	if ( DELEGATE_IS_SET(OnRefresh) )
		delegateOnRefresh();

	unguard;
}

FID3Field::FID3Field( PTRINT Ref, BYTE ID ) : Reference(Ref), FieldID(ID) 
{
	appMemzero( IDCode, 4 );
}

FID3Field::FID3Field( const FID3V2Frame* frame )            // Conversion constructor
{
	guard(FID3Field::FID3Field);

	if ( frame )
	{
		Reference = (PTRINT) &frame;

		TCHAR CodeText[5];
		INT i;

		appMemcpy( IDCode, frame->Header.IDText, 4 );

		FrameTypeList* List = UStreamInteraction::GetFrameTypes();
		for ( i = 0; i < ARRAY_COUNT(List->FrameTypes); i++ )
		{
			if ( List->FrameTypes[i].TestFrameID(appFromAnsi((ANSICHAR*)IDCode,NULL,5), FieldID, FieldName) )
				break;
		}
		
		if ( i == ARRAY_COUNT(List->FrameTypes) )
			debugf(NAME_MusicPlayer, TEXT("Couldn't find a matching frametype for %s"), CodeText);

		FieldValue = frame->Data;
	}

	unguard;
}

 // Copy constructor
FID3Field::FID3Field( const FID3Field& Other )
: FieldID(Other.FieldID), Reference(Other.Reference), FieldName(Other.FieldName), FieldValue(Other.FieldValue)
{
	appMemcpy( IDCode, Other.IDCode, 4 );
}


UBOOL FID3TagV1::RenderFields( UStreamTag* Tag ) const
{
	{
		guard(FID3TagV1::RenderFields);

		if ( !Tag ) return 0;
/*
		case FRAME_TITLE:             Title = Fields(i); break;
		case FRAME_TRACKNUM:    TrackNumber = Fields(i); break;
		case FRAME_LEADARTIST:       Artist = Fields(i); break;
		case FRAME_ALBUM:             Album = Fields(i); break;
		case FRAME_YEAR:               Year = Fields(i); break;
		case FRAME_CONTENTTYPE:       Genre = Fields(i); break;
		case FRAME_SONGLEN:        Duration = Fields(i); break;
*/
		FrameTypeList* List = UStreamInteraction::GetFrameTypes();
		check(List);

		// SongTitle
		if ( SongTitle[0] )
		{
			FID3Field* field = new(Tag->Fields) FID3Field;
			field->FieldID = FRAME_TITLE;

			for ( INT i = 0; i < ARRAY_COUNT(List->FrameTypes); i++ )
			{
				if ( List->FrameTypes[i].Type == field->FieldID )
				{
					field->FieldName = List->FrameTypes[i].Description;
					break;
				}
			}

			ByteToString( SongTitle, field->FieldValue, ARRAY_COUNT(SongTitle) );
		}

		// Artist
		if ( Artist[0] )
		{
			FID3Field* field = new(Tag->Fields) FID3Field;
			field->FieldID = FRAME_LEADARTIST;

			for ( INT i = 0; i < ARRAY_COUNT(List->FrameTypes); i++ )
			{
				if ( List->FrameTypes[i].Type == field->FieldID )
				{
					field->FieldName = List->FrameTypes[i].Description;
					break;
				}
			}

			ByteToString( SongTitle, field->FieldValue, ARRAY_COUNT(SongTitle) );
		}

		// Album
		if ( Album[0] )
		{
			FID3Field* field = new(Tag->Fields) FID3Field;
			field->FieldID = FRAME_ALBUM;
			for ( INT i = 0; i < ARRAY_COUNT(List->FrameTypes); i++ )
			{
				if ( List->FrameTypes[i].Type == field->FieldID )
				{
					field->FieldName = List->FrameTypes[i].Description;
					break;
				}
			}

			ByteToString( Album, field->FieldValue, ARRAY_COUNT(Album) );
		}

		// Year
		if ( Year[0] )
		{
			FID3Field* field = new(Tag->Fields) FID3Field;
			field->FieldID = FRAME_YEAR;
			for ( INT i = 0; i < ARRAY_COUNT(List->FrameTypes); i++ )
			{
				if ( List->FrameTypes[i].Type == field->FieldID )
				{
					field->FieldName = List->FrameTypes[i].Description;
					break;
				}
			}

			ByteToString( SongTitle, field->FieldValue, ARRAY_COUNT(Year) );
		}

		// TrackNum
		FID3Field* field = new(Tag->Fields) FID3Field;
		field->FieldID = FRAME_TRACKNUM;
		for ( INT i = 0; i < ARRAY_COUNT(List->FrameTypes); i++ )
		{
			if ( List->FrameTypes[i].Type == field->FieldID )
			{
				field->FieldName = List->FrameTypes[i].Description;
				break;
			}
		}

		field->FieldValue = appItoa( (INT)TrackNum );

		// Genre
		field = new(Tag->Fields) FID3Field;
		field->FieldID = FRAME_CONTENTTYPE;
		for ( INT i = 0; i < ARRAY_COUNT(List->FrameTypes); i++ )
		{
			if ( List->FrameTypes[i].Type == field->FieldID )
			{
				field->FieldName = List->FrameTypes[i].Description;
				break;
			}
		}

		field->FieldValue = appItoa((INT)Genre);
		return 1;
		unguard;
	}
}

void FID3TagV1::ByteToString( const BYTE* Text, FString& Str, INT Count ) const
{
	guard(FID3TagV1::ByteToString);

	TArray<TCHAR>& cArray = Str.GetCharArray();
	cArray.SetSize(Count+1);
	for( INT i=0; i<Count; i++ )
		cArray(i)=FromAnsi( (ANSICHAR)Text[i] );

	if( cArray.Num()==1 )
		Str.Empty();
	else cArray(Count) = 0;

	unguard;
}
