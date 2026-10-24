/*=============================================================================
	FConfigCacheIni.h: Unreal config file reading/writing.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

/*-----------------------------------------------------------------------------
	Config cache.
-----------------------------------------------------------------------------*/

// One section in a config file.
class FConfigSection : public TMultiMap<FString,FString>
{
public:
	// rjp --
	UBOOL HasQuotes( const FString& Test ) const
	{
		guardSlow(FConfigSection::HasQuotes);
		return Test.Left(1) == TEXT("\"") && Test.Right(1) == TEXT("\"");
		unguardSlow;
	}
	UBOOL operator==( const FConfigSection& Other ) const
	{
		guardSlow(FConfigSection::operator==);
		if ( Pairs.Num() != Other.Pairs.Num() )
			return 0;

		TMapBase<FString,FString>::TConstIterator My( (const TMapBase<FString,FString>&)*this ), Their((const TMapBase<FString,FString>&)Other);
		while ( My && Their )
		{
			if ( My.Key() != Their.Key() )
				return 0;

			const FString& MyValue = My.Value(), &TheirValue = Their.Value();
			if ( MyValue != TheirValue &&
				(!HasQuotes(MyValue) || TheirValue != MyValue.Mid(1,MyValue.Len()-2)) &&
				(!HasQuotes(TheirValue) || MyValue != TheirValue.Mid(1,TheirValue.Len()-2)) )
				return 0;

			++My, ++Their;
		}
		return 1;
		unguardSlow;
	}
	UBOOL operator!=( const FConfigSection& Other ) const
	{
		guardSlow(FConfigSection::operator!=);
		return ! (FConfigSection::operator==(Other));
		unguardSlow;
	}
	// -- rjp
};

// One config file.
class FConfigFile : public TMap<FString,FConfigSection>
{
public:
	UBOOL Dirty, NoSave, Quotes;
	// Keys set for this session only (FConfigCacheIni::SetSessionString), named "<Section>]<Key>": SessionOld holds the
	// value the file had, which Write saves in place of the session value; SessionNew the keys the file did not have,
	// which Write leaves out. CommitSessionStrings, or SetString of another value, makes the value a saved one.
	TMap<FString,FString> SessionOld, SessionNew;
	FConfigFile()
	: Dirty( 0 )
	, NoSave( 0 )
	, Quotes( 0 )
	{}
	void Combine( const TCHAR* Filename)
	{
		guard(FConfigFile::Combine);

		FString Text;
		if( appLoadFileToString( Text, Filename ) )
		{
			TCHAR* Ptr = const_cast<TCHAR*>( *Text );
			FConfigSection* CurrentSection = NULL;
			UBOOL Done = 0;
			while( !Done )
			{
				while( *Ptr=='\r' || *Ptr=='\n' )
					Ptr++;
				TCHAR* Start = Ptr;
				while( *Ptr && *Ptr!='\r' && *Ptr!='\n' )
					Ptr++;				
				if( *Ptr==0 )
					Done = 1;
				*Ptr++ = 0;
				if( *Start=='[' && Start[appStrlen(Start)-1]==']' )
				{
					Start++;
					Start[appStrlen(Start)-1] = 0;
					CurrentSection = Find( Start );
					if( !CurrentSection )
						CurrentSection = &Set( Start, FConfigSection() );
				}
				else if( CurrentSection && *Start )
				{
					TCHAR* Value = appStrstr(Start,TEXT("="));
					if( Value )
					{
						TCHAR Cmd = Start[0];
						if ( Cmd=='+' || Cmd=='-' )
							Start++;
						else
							Cmd=' ';

						*Value++ = 0;
						debugf(TEXT("Cmd=%c Start=%s Value=%s"),Cmd, Start,Value);
						// strip trailing spaces.
						while( *Value && Value[appStrlen(Value)-1]==' ' )
							Value[appStrlen(Value)-1] = 0;

						// decode quotes if they're present.
						if( *Value=='\"' && Value[appStrlen(Value)-1]=='\"' )
						{
							Value++;
							Value[appStrlen(Value)-1]=0;
						}

				        FString* Str = NULL;	// Holds the value pointer

						if (Cmd=='+') 
						{
							Str = CurrentSection->FindPair(Start,Value );		
							if (!Str)
								CurrentSection->Add(Start,Value);
						}
						else if (Cmd=='-')
						{
							Str = CurrentSection->FindPair(Start,Value);
							if (Str)
								CurrentSection->RemovePair(Start,Value);
						}
						else
						{
							Str = CurrentSection->Find(Start);
							if( !Str )
								CurrentSection->Add( Start, Value );
							else
								*Str = Value;
						}
					}
				}
			}
		}
		unguard;
	}
	void Read( const TCHAR* Filename )
	{
		guard(FConfigFile::Read);

		Empty();

		FString Text;
		if( appLoadFileToString( Text, Filename ) )
		{
			TCHAR* Ptr = const_cast<TCHAR*>( *Text );
			FConfigSection* CurrentSection = NULL;
			UBOOL Done = 0;
			while( !Done )
			{
				while( *Ptr=='\r' || *Ptr=='\n' )
					Ptr++;
				TCHAR* Start = Ptr;
				while( *Ptr && *Ptr!='\r' && *Ptr!='\n' )
					Ptr++;				
				if( *Ptr==0 )
					Done = 1;
				*Ptr++ = 0;
				if( *Start=='[' && Start[appStrlen(Start)-1]==']' )
				{
					Start++;
					Start[appStrlen(Start)-1] = 0;
					CurrentSection = Find( Start );
					if( !CurrentSection )
						CurrentSection = &Set( Start, FConfigSection() );
				}
				else if( CurrentSection && *Start )
				{
					TCHAR* Value = appStrstr(Start,TEXT("="));
					if( Value )
					{
						*Value++ = 0;

						// strip trailing spaces.
						while( *Value && Value[appStrlen(Value)-1]==' ' )
							Value[appStrlen(Value)-1] = 0;

						// decode quotes if they're present.
						if( *Value=='\"' && Value[appStrlen(Value)-1]=='\"' )
						{
							Value++;
							Value[appStrlen(Value)-1]=0;
						}
						CurrentSection->Add( Start, Value );
					}
				}
			}
		}
		unguard;
	}
	UBOOL Write( const TCHAR* Filename )
	{
		guard(FConfigFile::Write);
		if( !Dirty || NoSave )
			return 1;
		Dirty = 0;
		FString Text;
		for( TIterator It(*this); It; ++It )
		{
            #if WIN32
                TCHAR *newline = TEXT("\r\n");
            #else
                TCHAR *newline = TEXT("\n");
            #endif
			Text += FString::Printf( TEXT("[%s]%s"), *It.Key(), newline );
			for( FConfigSection::TIterator It2(It.Value()); It2; ++It2 )
			{
				const FString* Value = &It2.Value();
				if( SessionOld.Num() || SessionNew.Num() )
				{
					const FString Id = It.Key() + TEXT("]") + It2.Key();
					if( SessionNew.Find( Id ) )
						continue;
					if( const FString* Old = SessionOld.Find( Id ) )
						Value = Old;
				}
				Text += FString::Printf( TEXT("%s=%s%s%s%s"), *It2.Key(), Quotes ? TEXT("\"") : TEXT(""), **Value, Quotes ? TEXT("\"") : TEXT(""), newline );
			}
			Text += FString::Printf( newline );
		}
		return appSaveStringToFile( Text, Filename, GFileManager, 0, 1 );
		unguard;
	}
};

// Set of all cached config files.
class FConfigCacheIni : public FConfigCache, public TMap<FString,FConfigFile>
{
public:
	// Basic functions.
	FString SystemIni, UserIni;
	FConfigCacheIni()
	{}
	~FConfigCacheIni()
	{
		guard(FConfigCacheIni::~FConfigCacheIni);
		Flush( 1 );
		unguard;
	}
	FConfigFile* Find( const TCHAR* InFilename, UBOOL CreateIfNotFound )
	{
		guard(FConfigCacheIni::Find);

		// If filename not specified, use default.
		FString Filename = InFilename ? InFilename : *SystemIni;	// a path of any length

		// Add .ini extension.
		INT Len = Filename.Len();
		if( Len<5 || ((*Filename)[Len-4]!='.' && (*Filename)[Len-5]!='.') )
			Filename += TEXT(".ini");

		// Automatically translate generic filenames.
		if( Filename==TEXT("User.ini") )
			Filename = UserIni;
		else if( Filename==TEXT("System.ini") )
			Filename = SystemIni;

		// Get file.
		FConfigFile* Result = TMap<FString,FConfigFile>::Find( Filename );
		if( !Result && (CreateIfNotFound || GFileManager->FileSize(*Filename)>=0)  )
		{
			Result = &Set( *Filename, FConfigFile() );
			Result->Read( *Filename );
		}
		return Result;

		unguard;
	}
	void Flush( UBOOL Read, const TCHAR* Filename=NULL )
	{
		guard(FConfigCacheIni::Flush);

		for( TIterator It(*this); It; ++It )
		{
			FString& Key = It.Key();
			if( !Filename || (Key == Filename || Key.Left(Key.InStr(TEXT(".")))==Filename) )
				It.Value().Write( *Key );
		}
		if( Read )
		{
			if( Filename )
				Remove(Filename);
			else
				Empty();
		}
		unguard;
	}
	void UnloadFile( const TCHAR* Filename )
	{
		guard(FConfigCacheIni::UnloadFile);
		FConfigFile* File = Find( Filename, 1 );
		if( File )
			Remove( Filename );
		unguard;
	}
	void Detach( const TCHAR* Filename )
	{
		guard(FConfigCacheIni::Detach);
		FConfigFile* File = Find( Filename, 1 );
		if( File )
			File->NoSave = 1;
		unguard;
	}
	UBOOL GetString( const TCHAR* Section, const TCHAR* Key, TCHAR* Value, INT Size, const TCHAR* Filename )
	{
		guard(FConfigCacheIni::GetString);
		*Value = 0;
		FConfigFile* File = Find( Filename, 0 );
		if( !File )
			return 0;
		FConfigSection* Sec = File->Find( Section );
		if( !Sec )
			return 0;
		FString* PairString = Sec->Find( Key );
		if( !PairString )
			return 0;
		appStrncpy( Value, **PairString, Size );
		return 1;
		unguard;
	}
	UBOOL GetSection( const TCHAR* Section, TCHAR* Result, INT Size, const TCHAR* Filename )
	{
		guard(FConfigCacheIni::GetSection);
		*Result = 0;
		FConfigFile* File = Find( Filename, 0 );
		if( !File )
			return 0;
		FConfigSection* Sec = File->Find( Section );
		if( !Sec )
			return 0;
		TCHAR* End = Result;
		// Whole "key=value" entries while they fit, with the final terminator.
		for( FConfigSection::TIterator It(*Sec); It && End-Result+It.Key().Len()+1+It.Value().Len()+1<Size; ++It )
			End += appSprintf( End, TEXT("%s=%s"), *It.Key(), *It.Value() ) + 1;
		*End++ = 0;
		return 1;
		unguard;
	}
	TMultiMap<FString,FString>* GetSectionPrivate( const TCHAR* Section, UBOOL Force, UBOOL Const, const TCHAR* Filename )
	{
		guard(FConfigCacheIni::GetSectionPrivate);
		FConfigFile* File = Find( Filename, Force );
		if( !File )
			return NULL;
		FConfigSection* Sec = File->Find( Section );
		if( !Sec && Force )
			Sec = &File->Set( Section, FConfigSection() );
		if( Sec && (Force || !Const) )
			File->Dirty = 1;
		return Sec;
		unguard;
	}
	void SetString( const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const TCHAR* Filename, UBOOL UniqueKey = 1 ) // gam
	{
		guard(FConfigCacheIni::SetString);
		FConfigFile* File = Find( Filename, 1 );
		if( File->SessionOld.Num() || File->SessionNew.Num() )
		{
			// A value other than the session one is a setting of its own: saved from now on.
			const FString Id = FString(Section) + TEXT("]") + Key;
			const FString* Now = File->Find( Section ) ? File->Find( Section )->Find( Key ) : NULL;
			if( (File->SessionOld.Find( Id ) || File->SessionNew.Find( Id )) && !(Now && appStrcmp( **Now, Value )==0) )
			{
				File->SessionOld.Remove( *Id );
				File->SessionNew.Remove( *Id );
				File->Dirty = 1;
			}
		}
		FConfigSection* Sec  = File->Find( Section );
		if( !Sec )
			Sec = &File->Set( Section, FConfigSection() );
        // gam ---
        FString* Str = NULL;

        if( UniqueKey )			
		    Str = Sec->Find( Key );
        // --- gam
		if( !Str )
		{
			Sec->Add( Key, Value );
			File->Dirty = 1;
		}
		else if( appStricmp(**Str,Value)!=0 )
		{
			File->Dirty = (appStrcmp(**Str,Value)!=0);
			*Str = Value;
		}
		unguard;
	}
	void EmptySection( const TCHAR* Section, const TCHAR* InFilename )
	{
		guard(FConfigCacheIni::EmptySection);

		// If filename not specified, use default.
		TCHAR Filename[256];
		appStrcpy( Filename, InFilename ? InFilename : *SystemIni );

		// Add .ini extension.
		INT Len = appStrlen(Filename);
		if( Len<5 || (Filename[Len-4]!='.' && Filename[Len-5]!='.') )
			appStrcat( Filename, TEXT(".ini") );

		FConfigFile* File = Find( Filename, 0 );
		if( File )
		{
			FConfigSection* Sec = File->Find( Section );
		// rjp --
			if( Sec )
			{
				if ( FConfigSection::TIterator(*Sec) )
					Sec->Empty();

				File->Remove(Section);
				if (File->Num())
				{
					File->Dirty = 1;
					Flush(0, Filename);
				}
				else GFileManager->Delete(Filename);
					
			}	// -- rjp
		}
		unguard;
	}
	UBOOL GetSectionNames( TArray<FString>& Results, const TCHAR* InFilename )
	{
		guard(FConfigCacheIni::EmptySection);

		Results.Empty();

		// If filename not specified, use default.
		TCHAR Filename[256];
		appStrncpy( Filename, 
			InFilename && appStricmp(InFilename, TEXT("System")) 
			? (appStricmp(InFilename, TEXT("User")) ? InFilename : *UserIni) 
			: *SystemIni, ARRAY_COUNT(Filename) );

		// Add .ini extension.
		INT Len = appStrlen(Filename);
		if( Len<5 || (Filename[Len-4]!='.' && Filename[Len-5]!='.') )
			appStrncat( Filename, TEXT(".ini"), ARRAY_COUNT(Filename) );

		FConfigFile* File = Find( Filename, 0 );
		if( File )
		{
			for ( FConfigFile::TIterator It(*File); It; ++It )
				new(Results) FString(It.Key());
		}

		return Results.Num() > 0;

		unguard;
	}
	void Init( const TCHAR* InSystem, const TCHAR* InUser, UBOOL RequireConfig )
	{
		guard(FConfigCacheIni::Init);
		SystemIni = InSystem;
		UserIni   = InUser;
		unguard;
	}
	// A value for this session only: read back at once, while the file keeps the value it had (or not the key) until
	// CommitSessionStrings, whatever ends the process before that.
	void SetSessionString( const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const TCHAR* Filename )
	{
		guard(FConfigCacheIni::SetSessionString);
		FConfigFile* File = Find( Filename, 1 );
		const FString Id = FString(Section) + TEXT("]") + Key;
		// The file's value: the one recorded by an earlier call, else the one there now.
		FString Old;
		UBOOL Had = 0;
		if( const FString* Saved = File->SessionOld.Find( Id ) )
			{ Old = *Saved; Had = 1; }
		else if( !File->SessionNew.Find( Id ) && File->Find( Section ) && File->Find( Section )->Find( Key ) )
			{ Old = *File->Find( Section )->Find( Key ); Had = 1; }
		SetString( Section, Key, Value, Filename );
		File->SessionOld.Remove( *Id );
		File->SessionNew.Remove( *Id );
		if( Had )
			File->SessionOld.Set( *Id, *Old );
		else
			File->SessionNew.Set( *Id, TEXT("") );
		unguard;
	}
	// Every session value becomes a saved one.
	void CommitSessionStrings()
	{
		guard(FConfigCacheIni::CommitSessionStrings);
		for( TIterator It(*this); It; ++It )
			if( It.Value().SessionOld.Num() || It.Value().SessionNew.Num() )
			{
				It.Value().SessionOld.Empty();
				It.Value().SessionNew.Empty();
				It.Value().Dirty = 1;
			}
		unguard;
	}
	void Exit()
	{
		guard(FConfigCacheIni::Exit);
		Flush( 1 );
		unguard;
	}
	void Dump( FOutputDevice& Ar )
	{
		guard(FConfigCacheIni::Dump);
		Ar.Log( TEXT("Files map:") );
		TMap<FString,FConfigFile>::Dump( Ar );

#ifdef _DEBUG
		// rjp --
		for ( TMapBase<FString,FConfigFile>::TIterator It(*this); It; ++It )
		{
			Ar.Logf(TEXT("FileName: %s"), *It.Key());
			FConfigFile& File = It.Value();
			for ( TMapBase<FString,FConfigSection>::TIterator FileIt(File); FileIt; ++FileIt )
			{
				FConfigSection& Sec = FileIt.Value();
				Ar.Logf(TEXT("   [%s]"), *FileIt.Key());
				for ( TMapBase<FString,FString>::TIterator SecIt(Sec); SecIt; ++SecIt )
					Ar.Logf(TEXT("   %s=%s"), *SecIt.Key(), *SecIt.Value());

				Ar.Log(LINE_TERMINATOR);
			}
		}
		// -- rjp
#endif

		unguard;
	}

	// Derived functions.
	UBOOL GetString
	(
		const TCHAR* Section,
		const TCHAR* Key,
		FString&     Str,
		const TCHAR* Filename
	)
	{
		guard(FConfigCacheIni::GetString);
		TCHAR Temp[4096]=TEXT("");
		UBOOL Result = GetString( Section, Key, Temp, ARRAY_COUNT(Temp), Filename );
		Str = Temp;
		return Result;
		unguard;
	}
	const TCHAR* GetStr( const TCHAR* Section, const TCHAR* Key, const TCHAR* Filename )
	{
		guard(FConfigCacheIni::GetStr);
		TCHAR* Result = appStaticString1024();
		GetString( Section, Key, Result, 1024, Filename );
		return Result;
		unguard;
	}
	UBOOL GetInt
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		INT&			Value,
		const TCHAR*	Filename
	)
	{
		guard(FConfigCacheIni::GetInt);
		TCHAR Text[80]; 
		if( GetString( Section, Key, Text, ARRAY_COUNT(Text), Filename ) )
		{
			Value = appAtoi(Text);
			return 1;
		}
		return 0;
		unguard;
	}
	UBOOL GetFloat
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		FLOAT&			Value,
		const TCHAR*	Filename
	)
	{
		guard(FConfigCacheIni::GetFloat);
		TCHAR Text[80]; 
		if( GetString( Section, Key, Text, ARRAY_COUNT(Text), Filename ) )
		{
			Value = appAtof(Text);
			return 1;
		}
		return 0;
		unguard;
	}
	UBOOL GetBool
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		UBOOL&			Value,
		const TCHAR*	Filename
	)
	{
		guard(FConfigCacheIni::GetBool);
		TCHAR Text[80]; 
		if( GetString( Section, Key, Text, ARRAY_COUNT(Text), Filename ) )
		{
			if( appStricmp(Text,TEXT("True"))==0 )
			{
				Value = 1;
			}
			else
			{
				Value = appAtoi(Text)==1;
			}
			return 1;
		}
		return 0;
		unguard;
	}
	void SetInt
	(
		const TCHAR* Section,
		const TCHAR* Key,
		INT			 Value,
		const TCHAR* Filename
	)
	{
		guard(FConfigCacheIni::SetInt);
		TCHAR Text[30];
		appSprintf( Text, TEXT("%i"), Value );
		SetString( Section, Key, Text, Filename );
		unguard;
	}
	void SetFloat
	(
		const TCHAR*	Section,
		const TCHAR*	Key,
		FLOAT			Value,
		const TCHAR*	Filename
	)
	{
		guard(FConfigCacheIni::SetFloat);
		TCHAR Text[30];
		appSprintf( Text, TEXT("%f"), Value );
		SetString( Section, Key, Text, Filename );
		unguard;
	}
	void SetBool
	(
		const TCHAR* Section,
		const TCHAR* Key,
		UBOOL		 Value,
		const TCHAR* Filename
	)
	{
		guard(FConfigCacheIni::SetBool);
		SetString( Section, Key, Value ? TEXT("True") : TEXT("False"), Filename );
		unguard;
	}

	// Static allocator.
	static FConfigCache* Factory()
	{
		return new FConfigCacheIni();
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

