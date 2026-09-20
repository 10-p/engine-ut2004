/*=============================================================================
	UnCDKey.h: CD Key validation
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Jack Porter
=============================================================================*/

/*-----------------------------------------------------------------------------
	Defines
-----------------------------------------------------------------------------*/

#define CDKEYBASEMAP (TEXT("ABCDEFGHJLKMNPQRTUVWXYZ2346789"))
#define CDKEYBASE (appStrlen(CDKEYBASEMAP))
#define CDKEYBASEMAPINDEX(ch) (INT)((appStrchr(CDKEYBASEMAP,(ch))-CDKEYBASEMAP))

#define CDKEYAPPENDSTRING_UT2003 TEXT("You need to change this")
#define CDKEYAPPENDSTRING_UT2004 TEXT("You need to change this")

/*-----------------------------------------------------------------------------
	Global CD Key functions
-----------------------------------------------------------------------------*/

UBOOL ENGINE_API ValidateCDKey();
FString ENGINE_API GetCDKeyHash();
FString ENGINE_API MangleKey(FString KeyHash, FString Key);
FString ENGINE_API UnmangleKey(FString KeyHash, FString Key);
FString ENGINE_API GetCDKeyResponse( const TCHAR* Challenge );
FString ENGINE_API EncryptWithCDKeyHash( const TCHAR* String, const TCHAR* HashAppend );
FString ENGINE_API DecryptWithCDKeyHash( const TCHAR* String, const TCHAR* HashAppend, const TCHAR* InCDKey );

/*-----------------------------------------------------------------------------
	Inlines (for security)
-----------------------------------------------------------------------------*/

union FCdKeyMD5Qword
{
	BYTE Digest[16];
	QWORD Q;
};

inline TCHAR HexToCDKeyMap( TCHAR ch )
{
	if( ch >= '0' && ch <='9' )
		return CDKEYBASEMAP[ch-'0'];
	if( ch >= 'A' && ch <='Z' )
		return CDKEYBASEMAP[ch-'A'+10];
	if( ch >= 'a' && ch <='z' )
		return CDKEYBASEMAP[ch-'a'+10];
	return 0;
}

inline FString HexToCDKeyMap( const TCHAR *hex )
{
	FString Map;
	for( INT i=0;hex[i];i++ )
		Map = Map + FString::Printf(TEXT("%c"), HexToCDKeyMap(hex[i]) );
	return Map;
}

inline TCHAR CDKeyMapToHex( TCHAR ch )
{
	INT Index = CDKEYBASEMAPINDEX(ch);
	if( Index < 10 )
		return Index + '0';
	else
		return Index - 10 + 'a';
}

inline FString CDKeyMapToHex( const TCHAR* ch )
{
	FString Map;
	for( INT i=0;ch[i];i++ )
		Map = Map + FString::Printf(TEXT("%c"), CDKeyMapToHex(ch[i]) );
	return Map;
}

//
// The 6-symbol check that belongs to a key's 14 random symbols. This IS the validator's maths, lifted
// out unchanged so the per-install key generator (UnCDKey.cpp, ufront 2.79h) computes a check with the
// same code that will later verify it, instead of a second copy that could drift.
//
inline FString CDKeyCheckFor( const FString& Random, const TCHAR* AppendString=CDKEYAPPENDSTRING_UT2004 )
{
	QWORD Seed = appStrtoq( *CDKeyMapToHex(*Random), NULL, CDKEYBASE );
	
	TCHAR buf[128];
#if _MSC_VER
	FString Check = _i64tow(*((SQWORD *) &Seed), buf, 10);
#else
	{
		SQWORD val = *((SQWORD *) &Seed);
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "%lld", (long long)val);
		TCHAR* p = buf;
		for(const char* s = tmp; *s; s++) *p++ = (TCHAR)*s;
		*p = 0;
	}
	FString Check = buf;
#endif
	Check += AppendString;

	const ANSICHAR* AnsiCheck = appToAnsi(*Check);
	FCdKeyMD5Qword md5;

	FMD5Context Context;
	appMD5Init( &Context );
	appMD5Update( &Context, (unsigned char*)AnsiCheck, Check.Len() );
	appMD5Final( md5.Digest, &Context );

	FString CheckOutput = HexToCDKeyMap(appQtoa(INTEL_ORDER64(md5.Q), CDKEYBASE));
	CheckOutput = CheckOutput.Left(6);
	while( CheckOutput.Len() < 6 )
		CheckOutput = FString::Printf( TEXT("%c%s"), CDKEYBASEMAP[0], *CheckOutput );

	return CheckOutput;
}

inline UBOOL ValidateCDKey( const TCHAR* CDKey, const TCHAR* AppendString=CDKEYAPPENDSTRING_UT2004 )
{
	FString CDKeyStr = CDKey;
	FString FullKey = CDKeyStr.Mid(12,5)+CDKeyStr.Mid(6,5)+CDKeyStr.Mid(0,5)+CDKeyStr.Mid(18,5);
	return CDKeyCheckFor( FullKey.Left(14), AppendString ) == FullKey.Mid(14);
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

