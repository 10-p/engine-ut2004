//=============================================================================
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#include "Engine.h"
#include "XGame.h"
#include "UnNet.h"

#define RECORDLOG(typ) debugf(NAME_RecordCache, TEXT("Filling %sList From Cache"), TEXT(#typ))
#define DECO_TEXT_EOL_MARKER '|'

static void ParseDecoText( UDecoText* DecoText, const TCHAR* Text, INT ColumnCount )
{
    guard( ParseDecoText );

    TCHAR Line[DECO_TEXT_MAX_COLUMNS + 1];

    const TCHAR* LineStart;
    const TCHAR* WordStart;
    const TCHAR* WordEnd;
    const TCHAR* PrevWordEnd;
    if( ColumnCount > DECO_TEXT_MAX_COLUMNS )
    {
        debugf( NAME_Error, TEXT("Couldn't parse DecoText into %d columns (only %d are supported)"), ColumnCount, DECO_TEXT_MAX_COLUMNS );
        ColumnCount = DECO_TEXT_MAX_COLUMNS;
    }
    else if( ColumnCount <= 0 )
    {
        debugf( NAME_Error, TEXT("Couldn't parse DecoText into %d columns"), ColumnCount );
        return;
    }

    LineStart = Text;
    WordStart = Text;
    WordEnd = Text;
    PrevWordEnd = Text;

    for(;;)
    {
        // Get the next word:

        while( appIsSpace( *WordEnd ) )
            WordEnd++;

        while( !appIsSpace( *WordEnd ) && ( *WordEnd != '\0' ) && ( *WordEnd != DECO_TEXT_EOL_MARKER ) )
            WordEnd++;

        if( (WordEnd - WordStart) > ColumnCount ) // The word itself is too long:
        {
            // Flush line so-far:
            if( (PrevWordEnd - LineStart) > 0 )
            {
                appStrncpy( Line, LineStart, PrevWordEnd - LineStart + 1 );
                Line[PrevWordEnd - LineStart] = '\0';
                new(DecoText->Rows)FString(Line);
            }

            // Flush the word:
            appStrncpy( Line, WordStart, WordEnd - WordStart + 1 );
            Line[WordEnd - WordStart] = '\0';
            new(DecoText->Rows)FString(Line);

            // Start a new line:

            while( appIsSpace( *WordEnd ) )
                WordEnd++;

            LineStart = WordEnd;
        }
        else if( ( WordEnd - LineStart ) > ColumnCount )
        {
            // Flush line so-far:
            if( (PrevWordEnd - LineStart) > 0 )
            {
                appStrncpy( Line, LineStart, PrevWordEnd - LineStart + 1 );
                Line[PrevWordEnd - LineStart] = '\0';
                new(DecoText->Rows)FString(Line);
            }

            WordEnd = PrevWordEnd;

            while( appIsSpace( *WordEnd ) )
                WordEnd++;

            // Start a new line:
            LineStart = WordEnd;
        }
        else if( ( *WordEnd == DECO_TEXT_EOL_MARKER ) || ( *WordEnd == '\0' ) ) 
        {
            // Flush line to word end:

            appStrncpy( Line, LineStart, WordEnd - LineStart + 1 );
            Line[WordEnd - LineStart] = '\0';
            new(DecoText->Rows)FString(Line);

            if( *WordEnd == '\0' )
                break;

            WordEnd++;

            // Start a new line:
            LineStart = WordEnd;
        }

        PrevWordEnd = WordEnd;
        WordStart = WordEnd;
        WordEnd = WordEnd;
    }

    unguard;
}

UDecoText* UxUtil::LoadDecoText( const TCHAR* TextName, INT ColumnCount )
{
    guard( UxUtil::LoadDecoText );

    TCHAR PackageName[MAX_NAME_SIZE];
    TCHAR KeyName[MAX_NAME_SIZE];

    TCHAR FileNames[2][MAX_NAME_SIZE];

    TCHAR Buffer[DECO_TEXT_MAX_SIZE];
    const TCHAR *BufferP;

    const TCHAR* p;
    TCHAR* q;
    INT i;

    UDecoText* DecoText = NULL;
	
	debugf(NAME_RecordCache, TEXT("LoadDecoText::TextName = [%s]"),TextName);
    // Parse out the package and key name from TextName

    p = TextName;
    q = PackageName;
    i = ARRAY_COUNT(PackageName) - 1;

    while( *p != '.' )
    {
        if( *p == '\0' )
            return( NULL );

        if( i <= 0 )
            return( NULL );

        *q = *p;

        p++;
        q++;
        i--;
    }

    *q = '\0';
    p++;

    appStrncpy( KeyName, p, ARRAY_COUNT(KeyName) );

    if( !appStrlen( KeyName ) )
        return( NULL );
    
    // Find the key:
	
    appSprintf( FileNames[0], TEXT("%s.%s"), PackageName, UObject::GetLanguage() );
    appSprintf( FileNames[1], TEXT("%s.%s"), PackageName, TEXT("int") );

	debugf(NAME_RecordCache, TEXT("LoadDecoText::All = [%s] [%s] [%s] [%s]"),PackageName, KeyName, FileNames[0], FileNames[1]);

    if( !GConfig->GetString( TEXT("DecoText"), KeyName, Buffer, ARRAY_COUNT( Buffer), FileNames[0] ) &&
        !GConfig->GetString( TEXT("DecoText"), KeyName, Buffer, ARRAY_COUNT( Buffer), FileNames[1] ) )
    {
        debugf( NAME_RecordCache, TEXT("Couldn't load DecoText %s.%s from %s"), PackageName, KeyName, FileNames[0]);
		BufferP = TextName;
    }
	else
	    BufferP = Buffer;
    
    DecoText = CastChecked<UDecoText>( UObject::StaticConstructObject( UDecoText::StaticClass(), UObject::GetTransientPackage(), NAME_None, RF_Public | RF_Transient ) );
    DecoText->TextName = TextName;

    ParseDecoText( DecoText, BufferP, ColumnCount );

    return( DecoText );

    unguard;
}

template<class CacheClass> void CacheLoad( CacheClass& Cache )
{
    TCHAR Filename[1024];
    TArray<FString> Files;
    DOUBLE Time;

    guard(UxUtil::CacheLoad);

	UGameEngine* GameEngine = FindObject<UGameEngine>(ANY_PACKAGE, *FString::Printf(TEXT("GameEngine")) );
	UPackageMap* PackageMap = NULL;
	TArray<FString> Packages;
	if ( GameEngine && GameEngine->GLevel && GameEngine->GLevel->NetDriver && GameEngine->GLevel->NetDriver->ServerConnection )
	{
		PackageMap = GameEngine->GLevel->NetDriver->ServerConnection->PackageMap;
		for (INT i=0;i<PackageMap->List.Num();i++)
		{
			FString P = PackageMap->List(i).URL;
			INT Pos = P.InStr(TEXT("."));
			new(Packages)FString( FString::Printf(TEXT("%s"),*P.Left(Pos)) );
		}
	}

	Time = appSeconds();

    Cache.Records.Empty();

	UBOOL bFoundUPL=false;
//    for( INT i = 0; i < GSys->Paths.Num(); i++ )
    for( INT i = GSys->Paths.Num()-1; i >= 0; i-- )
    {
        Files.Empty();
        appSprintf( Filename, TEXT("%s"), *GSys->Paths(i) );
        TCHAR* Tmp = appStrstr( Filename, TEXT("*.") );

        if( !Tmp )
            continue;

        appSprintf( Tmp, TEXT("*.%s"), *Cache.FileExtension );
		if ( appStrlen(GModPath)==0 || appStricmp(Filename,TEXT("../system/*.upl")) || !bFoundUPL )
			Files = GFileManager->FindFiles(Filename, 1, 0 );


	    TCHAR *Buffer = 0;

		INT buffersize = 0;
        for( INT j = 0; j < Files.Num(); j++ )
        {
            Files(j) = Files(j).LeftChop(4);
            appSprintf( Tmp, *Files(j) );
            TCHAR* End = Tmp + appStrlen( Tmp );
            appSprintf( End, TEXT(".%s"), *Cache.FileExtension );

			buffersize = GFileManager->FileSize(Filename) + 1;
			if(Buffer)
				Buffer = (TCHAR*)appRealloc(Buffer, sizeof(TCHAR) * buffersize, TEXT("CacheLoad Buffer"));
			else
				Buffer = (TCHAR*)appMalloc(sizeof(TCHAR) * buffersize, TEXT("CacheLoad Buffer"));

			// note, buffer is at least as large as buffersize * sizeof(TCHAR)
            UBOOL Success = GConfig->GetSection( *Cache.SectionName, Buffer, buffersize, Filename );
            if( !Success )
			{
                continue;
			}
	
			bFoundUPL=true;

            TCHAR* Next;
            for( TCHAR* Key = Buffer; *Key; Key=Next )
            {
                Next = Key + appStrlen(Key) + 1;
                TCHAR* Value = appStrstr(Key,TEXT("="));

                if( !Value )
                    continue;

                *Value++ = 0;
                if( *Value=='(' )
                    *Value++ = 0;
                if( *Value && Value[appStrlen(Value)-1]==')' )
                    Value[appStrlen(Value)-1] = 0;

                CacheParseLine( Cache, Key, Value, *Files(j), Packages );
            }
        }
		if (Buffer) 
			appFree(Buffer);

    }

    Time = appSeconds() - Time;
    
    debugf( NAME_RecordCache, TEXT("xUtil::CacheLoad %s (%f seconds)"), Cache.GetName(), Time );

    unguard;
};


template<class CacheClass> CacheClass* CacheGet()
{
    guard(UxUtil::CacheGet);

    CacheClass* Cache = CastChecked<CacheClass>( UObject::StaticConstructObject( CacheClass::StaticClass(), UObject::GetTransientPackage(), NAME_None, RF_Public | RF_Transient ) );
    CacheLoad( *Cache );

    return( Cache );

    unguard;
}

static void CacheParseLine( UCachePlayers& Cache, const TCHAR* Key, const TCHAR* Value, const TCHAR* FileName, TArray<FString> Packages )
{
	guard( CacheParseLine::Players );

    if( appStricmp( Key, TEXT("Player") ) != 0 )
        return;

    FPlayerRecord Record;
	appMemzero( &Record, sizeof(FPlayerRecord) );

    TCHAR tmp [MAX_NAME_SIZE];
    if( !Parse( Value, TEXT("DefaultName="), tmp, ARRAY_COUNT(tmp) ) )
        appStrcpy( tmp, TEXT("Player") );
    Record.DefaultName = tmp;

    if( Parse( Value, TEXT("Species="), tmp, ARRAY_COUNT(tmp) ) )
	{

		TCHAR Pack[128];
		TCHAR *p;

		appStrcpy(Pack,tmp);
		p = appStrstr(Pack,TEXT("."));
		p[0]=0;

		if ( Packages.Num()>0 && appStricmp(p,TEXT("xgame")) )
		{
			INT i;
			for (i=0;i<Packages.Num();i++)
			{
				if ( !appStricmp(*Packages(i),Pack) )
					break;
			}

			if (i==Packages.Num() )
			{
				debugf(NAME_Warning, TEXT("Attempted to load a Species (%s) that's not configured on the server"), tmp);
				return;
			}
		}
		Record.Species = UObject::StaticLoadClass( USpeciesType::StaticClass(), NULL, tmp, NULL, 0, NULL );
	}

    Parse( Value, TEXT("Mesh="), Record.MeshName );
    Parse( Value, TEXT("BodySkin="), Record.BodySkinName );
    Parse( Value, TEXT("FaceSkin="), Record.FaceSkinName );
    Parse( Value, TEXT("Voice="), Record.VoiceClassName );
    Parse( Value, TEXT("Skeleton="), Record.Skeleton );
    Parse( Value, TEXT("Sex="), Record.Sex );

	UMaterial* mat = NULL;
    if( Parse( Value, TEXT("Portrait="), tmp, ARRAY_COUNT(tmp) ) )
        mat = Cast<UMaterial>( UObject::StaticLoadObject( UMaterial::StaticClass(), NULL, tmp, NULL, 0, NULL ) );
    if( !mat )
        mat = Cast<UMaterial>( UObject::StaticLoadObject( UMaterial::StaticClass(), NULL, TEXT("PlayerPictures.cDefault"), NULL, LOAD_NoWarn, NULL ) );
    Record.Portrait = mat;

    Parse( Value, TEXT("Accuracy="), Record.Accuracy );
    Parse( Value, TEXT("Aggressiveness="), Record.Aggressiveness );
    Parse( Value, TEXT("StrafingAbility="), Record.StrafingAbility );
    Parse( Value, TEXT("CombatStyle="), Record.CombatStyle );
    Parse( Value, TEXT("Tactics="), Record.Tactics );
    Parse( Value, TEXT("ReactionTime="), Record.ReactionTime );
    Parse( Value, TEXT("Jumpiness="), Record.Jumpiness );
    Parse( Value, TEXT("Race="), Record.Race );
    Parse( Value, TEXT("FavoriteWeapon="), Record.FavoriteWeapon );
    Parse( Value, TEXT("Menu="), Record.Menu );
	if ( Parse(Value, TEXT("BotUse="), tmp, ARRAY_COUNT(tmp)) )
	{
		Record.BotUse = appAtoi( tmp );
	}

	if ( Parse(Value, TEXT("UseSpecular="), tmp, ARRAY_COUNT(tmp)) && !appStrcmp(tmp,TEXT("1")) )
		Record.UseSpecular = true;
    if ( Parse(Value, TEXT("TeamFace="), tmp, ARRAY_COUNT(tmp)) && !appStrcmp(tmp,TEXT("1")) )
		Record.TeamFace = true;
    if ( Parse(Value, TEXT("ZeroWeaponOffsets="), tmp, ARRAY_COUNT(tmp)) && !appStrcmp(tmp,TEXT("1")) )
		Record.ZeroWeaponOffsets = true;
    if( !Parse( Value, TEXT("Text="), tmp, ARRAY_COUNT(tmp) ) )
        appStrcpy( tmp, TEXT("XPlayers.Default") );
    Record.TextName = tmp;

    Parse( Value, TEXT("Ragdoll="), Record.Ragdoll );

    INT index = Cache.Records.AddZeroed();
    Record.RecordIndex = index;
    Cache.Records( index ) = Record;

    unguard;
}

void UxUtil::GetPlayerList()
{
    if (!CachedPlayerList)
        CachedPlayerList = CacheGet<UCachePlayers>();
	else RECORDLOG(Player);
}

void UxUtil::execGetPlayerList( FFrame& Stack, RESULT_DECL )
{
    guard(UxUtil::execGetPlayerList);

    P_GET_TARRAY_REF( PlayerRecords, FPlayerRecord );
    P_FINISH;

    GetPlayerList();
    *PlayerRecords = CachedPlayerList->Records;

    unguardexec;
}

void UxUtil::execGetPlayerRecord( FFrame& Stack, RESULT_DECL )
{
    guard(UxUtil::execGetPlayerRecord);

    P_GET_INT(prIdx);
    P_FINISH;

    GetPlayerList();
    if( CachedPlayerList->Records.Num()==0 || prIdx >= CachedPlayerList->Records.Num() )
    {
        FPlayerRecord empty;
	 	appMemzero( &empty, sizeof(FPlayerRecord) );
       *(FPlayerRecord*)Result = empty;
        return;
    }

    *(FPlayerRecord*)Result = CachedPlayerList->Records(prIdx);

    unguardexec;
}

void UxUtil::execFindUPLPlayerRecord( FFrame& Stack, RESULT_DECL )
{
    guard(UxUtil::execFindUPLPlayerRecord);
    P_GET_STR(charName);
    P_FINISH;

    GetPlayerList();
    for (INT i=0; i<CachedPlayerList->Records.Num(); i++)
    {
        if (charName == CachedPlayerList->Records(i).DefaultName)
        {
            *(FPlayerRecord*)Result = CachedPlayerList->Records(i);
            return;
        }
    }
    
    FPlayerRecord empty;
	appMemzero( &empty, sizeof(FPlayerRecord) );
    *(FPlayerRecord*)Result = empty;

    unguardexec;
}

void UxUtil::execLoadDecoText( FFrame& Stack, RESULT_DECL )
{
	guard(UxUtil::execLoadDecoText);

	P_GET_STR(PackageName);
	P_GET_STR(DecoTextName);
	P_GET_INT_OPTX(ColumnCount,DECO_TEXT_MAX_COLUMNS);
	P_FINISH;

	*(UDecoText**)Result = LoadDecoText(*(PackageName + TEXT(".") + DecoTextName), ColumnCount);
	unguardexec;
}
