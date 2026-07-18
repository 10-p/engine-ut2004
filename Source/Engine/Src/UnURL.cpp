/*=============================================================================
	UnURL.cpp: Various file-management functions.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	FURL Statics.
-----------------------------------------------------------------------------*/

// Variables.
FString FURL::DefaultProtocol;
FString FURL::DefaultProtocolDescription;
FString FURL::DefaultName;
FString FURL::DefaultMap;
FString FURL::DefaultLocalMap;
FString FURL::DefaultNetBrowseMap;
FString FURL::DefaultHost;
FString FURL::DefaultPortal;
FString FURL::DefaultMapExt;
FString FURL::DefaultSaveExt;
INT		FURL::DefaultPort=0;

// Static init.
void FURL::StaticInit()
{
	guard(FURL::StaticInit);

	DefaultProtocol				= GConfig->GetStr( TEXT("URL"), TEXT("Protocol") );
	DefaultProtocolDescription	= GConfig->GetStr( TEXT("URL"), TEXT("ProtocolDescription") );
	DefaultName					= GConfig->GetStr( TEXT("URL"), TEXT("Name") );
	DefaultMap					= GConfig->GetStr( TEXT("URL"), TEXT("Map") );
	DefaultLocalMap				= GConfig->GetStr( TEXT("URL"), TEXT("LocalMap") );
	DefaultNetBrowseMap			= GConfig->GetStr( TEXT("URL"), TEXT("NetBrowseMap") );	
	DefaultHost					= GConfig->GetStr( TEXT("URL"), TEXT("Host") );
	DefaultPortal				= GConfig->GetStr( TEXT("URL"), TEXT("Portal") );
	DefaultMapExt				= GConfig->GetStr( TEXT("URL"), TEXT("MapExt") );
	DefaultSaveExt				= GConfig->GetStr( TEXT("URL"), TEXT("SaveExt") );
	DefaultPort					= appAtoi( GConfig->GetStr( TEXT("URL"), TEXT("Port") ) );

	unguard;
}
void FURL::StaticExit()
{
	guard(FURL::StaticExit);

	DefaultProtocol				= TEXT("");
	DefaultProtocolDescription	= TEXT("");
	DefaultName					= TEXT("");
	DefaultMap					= TEXT("");
	DefaultLocalMap				= TEXT("");
	DefaultHost					= TEXT("");
	DefaultPortal				= TEXT("");
	DefaultMapExt				= TEXT("");
	DefaultSaveExt				= TEXT("");

	unguard;
}

ENGINE_API FArchive& operator<<( FArchive& Ar, FURL& U )
{
	guard(FURL<<);
	Ar << U.Protocol << U.Host << U.Map << U.Portal << U.Op << U.Port << U.Valid;
	return Ar;
	unguard;
}

/*-----------------------------------------------------------------------------
	Internal.
-----------------------------------------------------------------------------*/

static UBOOL ValidNetChar( const TCHAR* c )
{
	if( appStrchr(c,' ') )
		return 0;
	else
		return 1;
}

/*-----------------------------------------------------------------------------
	Constructors.
-----------------------------------------------------------------------------*/

//
// Constuct a purely default, local URL from an optional filename.
//
FURL::FURL( const TCHAR* LocalFilename )
:	Protocol	( DefaultProtocol )
,	Host		( DefaultHost )
,	Map			( LocalFilename ? FString(LocalFilename) : DefaultMap )
,	Portal		( DefaultPortal )
,	Port		( DefaultPort )
,	Op			()
,	Valid		( 1 )
{}

//
// Helper function.
//
TCHAR* appStrchr( TCHAR* Src, TCHAR A, TCHAR B )
{
	TCHAR* AA = appStrchr( Src, A );
	TCHAR* BB = appStrchr( Src, B );
	return (AA && (!BB || AA<BB)) ? AA : BB;
}

//
// Construct a URL from text and an optional relative base.
//
FURL::FURL( FURL* Base, const TCHAR* TextURL, ETravelType Type )
:	Protocol	( DefaultProtocol )
,	Host		( DefaultHost )
,	Map			( DefaultMap )
,	Portal		( DefaultPortal )
,	Port		( DefaultPort )
,	Op			()
,	Valid		( 1 )
{
	guard(FURL::FURL);
	check(TextURL);

	// Make a copy.
	TCHAR Temp[1024], *URL=Temp;
	appStrncpy( Temp, TextURL, ARRAY_COUNT(Temp) );

	// Copy Base.
	if( Type==TRAVEL_Relative )
	{
		check(Base);
		Protocol = Base->Protocol;
		Host     = Base->Host;
		Map      = Base->Map;
		Portal   = Base->Portal;
		Port     = Base->Port;
	}
	if( Type==TRAVEL_Relative || Type==TRAVEL_Partial )
	{
		check(Base);
		for( INT i=0; i<Base->Op.Num(); i++ )
		{
			if
			(	appStricmp(*Base->Op(i),TEXT("PUSH"))!=0
			&&	appStricmp(*Base->Op(i),TEXT("POP" ))!=0
			&&	appStricmp(*Base->Op(i),TEXT("PEER"))!=0
			&&	appStricmp(*Base->Op(i),TEXT("LOAD"))!=0
			&&	appStricmp(*Base->Op(i),TEXT("QUIET"))!=0 )
				new(Op)FString(Base->Op(i));
		}
	}

	// Skip leading blanks.
	while( *URL == ' ' )
		URL++;

	// Options.
	TCHAR* s = appStrchr(URL,'?','#');
	if( s )
	{
		TCHAR OptionChar=*s, NextOptionChar=0;
		*s++ = 0;
		do
		{
			TCHAR* t = appStrchr(s,'?','#');
			if( t )
			{
				NextOptionChar = *t;
				*t++ = 0;
			}
			if( !ValidNetChar( s ) )
			{
				*this = FURL();
				Valid = 0;
				return;
			}
			if( OptionChar=='?' )
				AddOption( s );
			else
				Portal = s;
			s = t;
			OptionChar = NextOptionChar;
		} while( s );
	}

	// Handle pure filenames.
	UBOOL FarHost=0;
	UBOOL FarMap=0;
	if( appStrlen(URL)>2 && URL[1]==':' )
	{
		// Pure filename.
		Protocol = DefaultProtocol;
		Host = DefaultHost;
		Map = URL;
		Portal = DefaultPortal;
		URL = NULL;
		FarHost = 1;
		FarMap = 1;
		Host = TEXT("");
	}
	else
	{
		// Parse protocol.
		if
		(	(appStrchr(URL,':')!=NULL)
		&&	(appStrchr(URL,':')>URL+1)
		&&	(appStrchr(URL,'.')==NULL || appStrchr(URL,':')<appStrchr(URL,'.')) )
		{
			TCHAR* s = URL;
			URL      = appStrchr(URL,':');
			*URL++   = 0;
			Protocol = s;

			// This prevents applications from being launched. -pv
			if (appStricmp(s, TEXT("file")) == 0)
			{
				*this = FURL();
				Valid = 0;
				return;
			}
		}

		// Parse optional leading slashes.
		if( *URL=='/' )
		{
			URL++;
			if( *URL++ != '/' )
			{
				*this = FURL();
				Valid = 0;
				return;
			}
			FarHost = 1;
			Host = TEXT("");
		}

		// Parse optional host name and port.
		const TCHAR* Dot = appStrchr(URL,'.');
		if
		(	(Dot)
		&&	(Dot-URL>0)
		&&	(appStrnicmp( Dot+1,*DefaultMapExt,  DefaultMapExt .Len() )!=0 || appIsAlnum(Dot[DefaultMapExt .Len()+1]) )
		&&	(appStrnicmp( Dot+1,*DefaultSaveExt, DefaultSaveExt.Len() )!=0 || appIsAlnum(Dot[DefaultSaveExt.Len()+1]) ) )
		{
			TCHAR* s = URL;
			URL     = appStrchr(URL,'/');
			if( URL )
				*URL++ = 0;
			TCHAR* t = appStrchr(s,':');
			if( t )
			{
				// Port.
				*t++ = 0;
				Port = appAtoi( t );
			}
			Host = s;
			if( appStricmp(*Protocol,*DefaultProtocol)==0 )
				Map = DefaultMap;
			else
				Map = TEXT("");
			FarHost = 1;
		}
	}

	// Copy existing options which aren't in current URL

#ifdef _XBOX // the best e3 hack evar!
    if( Type==TRAVEL_Absolute && Base && IsInternal() )
    {
        debugf(TEXT("Trouncing e3 related URL options!"));
        const TCHAR* PlayerName = Base->GetOption( TEXT("NAME="), NULL );
        if( PlayerName )
            AddOption( *FString::Printf(TEXT("NAME=%s"), PlayerName) );
        const TCHAR* Team = Base->GetOption( TEXT("TEAM="), NULL );
        if( Team )
            AddOption( *FString::Printf(TEXT("Team=%s"), Team) );
        const TCHAR* Voice = Base->GetOption( TEXT("VoiceMask="), NULL );
        if( Voice )
            AddOption( *FString::Printf(TEXT("VoiceMask=%s"), Voice) );
    }
#endif

	if( Type==TRAVEL_Absolute && Base && IsInternal())
	{
		for( INT i=0; i<Base->Op.Num(); i++ )
		{
			if
			(	appStrnicmp(*Base->Op(i),TEXT("Name="),5)==0
			||	appStrnicmp(*Base->Op(i),TEXT("Team=" ),5)==0
            ||	appStrnicmp(*Base->Op(i),TEXT("Character="),10)==0 // sjs
#ifdef _XBOX
            ||	appStrnicmp(*Base->Op(i),TEXT("VoiceMask="),10)==0 // sjs
#endif
			||	appStrnicmp(*Base->Op(i),TEXT("Class="),6)==0
			||	appStrnicmp(*Base->Op(i),TEXT("Skin="),5)==0 
			||	appStrnicmp(*Base->Op(i),TEXT("Face="),5)==0 
			||	appStrnicmp(*Base->Op(i),TEXT("Voice="),6 )==0 
			||	appStrnicmp(*Base->Op(i),TEXT("OverrideClass="),14)==0 )
			{
				TCHAR OptName[100];
				TCHAR *Pos;

				Pos = appStrchr( *Base->Op(i), '=');
				if(Pos)
					appStrncpy(	OptName, *Base->Op(i), Pos - *Base->Op(i) + 1);
				else
					appStrcpy( OptName, *Base->Op(i) );
							
				if( !appStrcmp( GetOption( OptName, TEXT("")), TEXT("") ) )
				{
					debugf( TEXT("URL: Adding default option %s"), *Base->Op(i) );
					new(Op)FString( Base->Op(i) );
				}
			}
		}
	}

	// Parse optional map and teleporter.
	if( URL && *URL )
	{
		if(IsInternal())
		{
			// Portal.
			FarMap = 1;
			TCHAR* t = appStrchr(URL,'/');
			if( t )
			{
				// Trailing slash.
				*t++ = 0;
				TCHAR* u = appStrchr(t,'/');
				if( u )
				{
					*u++ = 0;
					if( *u != 0 )
					{
						*this = FURL();
						Valid = 0;
						return;
					}
				}

				// Portal name.
				Portal = t;
			}
		}

		// Map.
		Map = URL;
	}
	
	// Validate everything.
	if
	(	!ValidNetChar(*Protocol  )
	||	!ValidNetChar(*Host      )
	//||	!ValidNetChar(*Map       )
	||	!ValidNetChar(*Portal    )
	||	(!FarHost && !FarMap && !Op.Num()) )
	{
		*this = FURL();
		Valid = 0;
		return;
	}

	// Success.
	unguard;
}

/*-----------------------------------------------------------------------------
	Conversion to text.
-----------------------------------------------------------------------------*/

//
// Convert this URL to text.
//
FString FURL::String( UBOOL FullyQualified, UBOOL bHidePasswords ) const
{
	guard(FURL::String);
	FString Result;

	// Emit protocol.
	if( Protocol!=DefaultProtocol || FullyQualified )
	{
		Result += Protocol;
		Result += TEXT(":");
		if( Host!=DefaultHost )
			Result += TEXT("//");
	}

	// Emit host.
	if( Host!=DefaultHost || Port!=DefaultPort )
	{
		Result += Host;
		if( Port!=DefaultPort )
		{
			Result += TEXT(":");
			Result += FString::Printf( TEXT("%i"), Port );
		}
		Result += TEXT("/");
	}

	// Emit map.
	if( Map )
		Result += Map;

	// Emit options.
	for( INT i=0; i<Op.Num(); i++ )
	{
		if ( bHidePasswords && !appStrPrefix( *(Op(i).Caps()), TEXT("ADMINPASSWORD=")) )
			Result += TEXT("?ADMINPASSWORD=XXXXX");
		else if ( bHidePasswords && !appStrPrefix( *(Op(i).Caps()), TEXT("PASSWORD=")) )
			Result += TEXT("?PASSWORD=XXXXX");
		else
		{
			Result += TEXT("?");
			Result += Op(i);
		}
	}

	// Emit portal.
	if( Portal )
	{
		Result += TEXT("#");
		Result += Portal;
	}

	return Result;
	unguard;
}

FString FURL::OptionString( UBOOL bStripCommon ) const
{
	guard(FURL::OptionString);
	FString Result;

	for( INT i=0; i<Op.Num(); i++ )
	{
		if ( bStripCommon )
		{
			INT Pos = Op(i).InStr(TEXT("="));
			if ( Pos != INDEX_NONE )
			{
				FString OpName = Op(i).Left(Pos);
				if (OpName == TEXT("Name") ||
					OpName == TEXT("Class") ||
					OpName == TEXT("Character") ||
					OpName == TEXT("Team"))
					continue;
			}
		}
		Result += TEXT("?");
		Result += Op(i);

	}
	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	Informational.
-----------------------------------------------------------------------------*/

//
// Return whether this URL corrsponds to an internal object, i.e. an Unreal
// level which this app can try to connect to locally or on the net. If this
// is fals, the URL refers to an object that a remote application like Internet
// Explorer can execute.
//
UBOOL FURL::IsInternal() const
{
	guard(FURL::IsInternal);
	return Protocol==DefaultProtocol;
	unguard;
}

//
// Return whether this URL corresponds to an internal object on this local 
// process. In this case, no Internet use is necessary.
//
UBOOL FURL::IsLocalInternal() const
{
	guard(FURL::IsLocalInternal);
	return IsInternal() && Host.Len()==0;
	unguard;
}

//
// Add a unique option to the URL, replacing any existing one.
//
void FURL::AddOption( const TCHAR* KeyValuePair )
{
	guard(FURL::AddOption);
	INT Match = appStrchr(KeyValuePair,'=') ? appStrchr(KeyValuePair,'=')+1-KeyValuePair : appStrlen(KeyValuePair)+1;
	INT i;
	for( i=0; i<Op.Num(); i++ )
		if( appStrnicmp( *Op(i), KeyValuePair, Match )==0 )
			break;
	if( i==Op.Num() )	new( Op )FString( KeyValuePair );
	else				Op( i ) = KeyValuePair;
	unguard;
}

//
// Remove an option from the URL
//
void FURL::RemoveOption( const TCHAR* Key, const TCHAR* Section, const TCHAR* Filename )
{
	guard(FURL::RemoveOption);
	if ( !Key )
		return;

	if ( !Filename )
		Filename = GUserIni;

	for ( INT i = Op.Num() - 1; i >= 0; i-- )
	{
		if ( Op(i).Left(appStrlen(Key)) == Key )
		{
			TMultiMap<FString,FString>* Sec = GConfig->GetSectionPrivate( Section ? Section : TEXT("DefaultPlayer"), 0, 0, Filename );
			if ( Sec )
			{
				FString DefaultFilename = FName(Filename) == NAME_System ? TEXT("Default.ini") : TEXT("DefUser.ini");
				const TCHAR* DefaultValue = GConfig->GetStr(Section ? Section : TEXT("DefaultPlayer"), Key, *DefaultFilename);
				if ( *DefaultValue )
				{
					GConfig->SetString(Section ? Section : TEXT("DefaultPlayer"), Key, DefaultValue, Filename);
					GConfig->Flush( 0, Filename );
				}
				else if (Sec->Remove( Key ) > 0)
					GConfig->Flush( 0, Filename );
			}

			Op.Remove(i);
		}
	}

	unguard;
}

//
// Load URL from config.
//
void FURL::LoadURLConfig( const TCHAR* Section, const TCHAR* Filename )
{
	guard(FURL::LoadURLConfig);
	TCHAR Text[32000], *Ptr=Text;
	GConfig->GetSection( Section, Text, ARRAY_COUNT(Text), Filename ? Filename : GUserIni );
	while( *Ptr )
	{
		AddOption( Ptr );
		Ptr += appStrlen(Ptr)+1;
	}
	unguard;
}

//
// Save URL to config.
//
void FURL::SaveURLConfig( const TCHAR* Section, const TCHAR* Item, const TCHAR* Filename ) const
{
	guard(FURL::SaveURLConfig);
	for( INT i=0; i<Op.Num(); i++ )
	{
		TCHAR Temp[1024];
		appStrcpy( Temp, *Op(i) );
		TCHAR* Value = appStrchr(Temp,'=');
		if( Value )
		{
			*Value++ = 0;
			if( !Item || appStricmp(Temp,Item)==0 )
				GConfig->SetString( Section, Temp, Value, Filename ? Filename : GUserIni );
		}
	}
	unguard;
}

//
// See if the URL contains an option string.
//
UBOOL FURL::HasOption( const TCHAR* Test ) const
{
	guard(FURL::HasOption);
	for( INT i=0; i<Op.Num(); i++ )
		if( appStricmp( *Op(i), Test )==0 )
			return 1;
	return 0;
	unguard;
}

//
// Return an option if it exists.
//
const TCHAR* FURL::GetOption( const TCHAR* Match, const TCHAR* Default ) const
{
	guard(FURL::GetOption);
	for( INT i=0; i<Op.Num(); i++ )
		if( appStrnicmp( *Op(i), Match, appStrlen(Match) )==0 )
			return *Op(i) + appStrlen(Match);
	return Default;
	unguard;
}

/*-----------------------------------------------------------------------------
	Comparing.
-----------------------------------------------------------------------------*/

//
// Compare two URL's to see if they refer to the same exact thing.
//
UBOOL FURL::operator==( const FURL& Other ) const
{
	guard(FURL::operator==);
	if
	(	Protocol	!= Other.Protocol
	||	Host		!= Other.Host
	||	Map			!= Other.Map
	||	Port		!= Other.Port
	||  Op.Num()    != Other.Op.Num() )
		return 0;

	for( int i=0; i<Op.Num(); i++ )
		if( Op(i) != Other.Op(i) )
			return 0;

	return 1;
	unguard;
}


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

