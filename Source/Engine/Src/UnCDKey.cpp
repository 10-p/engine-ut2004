/*=============================================================================
	UnCDKey.cpp: CD Key validation

	The Win32 branches key on `_WIN32` rather than `_MSC_VER`: the CD key lives in the Windows registry
	whichever compiler built the binary (TCHAR_CALL_OS and appAlloca are defined for GCC in UnGnuG.h).
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Jack Porter
=============================================================================*/

/*-----------------------------------------------------------------------------
	Includes
-----------------------------------------------------------------------------*/

// This sucks, but I really don't want the code to read the CD key to be
// in a DLL export.  So we have to declare the registry stuff we need here.

#if defined(_WIN32) && !_XBOX
#pragma pack(push, 8)
#include <windows.h>
#pragma pack(pop)
#endif
#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	Win32 Registry access
-----------------------------------------------------------------------------*/

#if defined(_WIN32) && !_XBOX
#define RegOpenKeyX(a,b,c)				TCHAR_CALL_OS(RegOpenKeyW(a,b,c),RegOpenKeyA(a,TCHAR_TO_ANSI(b),c))
static inline LONG RegQueryValueExX( HKEY hKey, LPCTSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData )
{
#if UNICODE
	if( !GUnicodeOS )
	{
		ANSICHAR* ACh = (ANSICHAR*)appAlloca(*lpcbData);
		LONG Result = RegQueryValueExA( hKey, TCHAR_TO_ANSI(lpValueName), lpReserved, lpType, (BYTE*)ACh, lpcbData );
		if( Result==ERROR_SUCCESS )
			MultiByteToWideChar( CP_ACP, 0, ACh, -1, (TCHAR*)lpData, *lpcbData );
		return Result;
	}
	else
#endif
	{
		return RegQueryValueEx( hKey, lpValueName, lpReserved, lpType, lpData, lpcbData );
	}
}
#endif

/*-----------------------------------------------------------------------------
	A key for an install that has none.

	A server identifies a player by a hash of the CD key: it is the identity a ban lands on. An
	install with no `cdkey` file would hash the same empty placeholder as every other such install
	(one identity for all of them, and every web install starts without a key file).

	The key is 14 random symbols plus the 6-symbol check the validator in UnCDKey.h computes, with
	the validator's own code (CDKeyCheckFor), written to `cdkey` so the install keeps its identity.
	A server only ever sees a hash, so nothing about how the key was made is observable on the wire,
	and a ban can only spill onto an install whose key is equal: 63 random bits.

	Engine-side on purpose: the alphabet and the append string are proprietary. They stay in this
	private repo and must never be reimplemented in the public host page.
-----------------------------------------------------------------------------*/

#if __UNIX__
#ifdef __EMSCRIPTEN__
#include <unistd.h>		// getentropy(): crypto.getRandomValues in a browser
#endif

static UBOOL CDKeyEntropy( QWORD& Out )
{
#ifdef __EMSCRIPTEN__
	return getentropy( &Out, sizeof(Out) )==0;
#else
	FILE* Urandom = fopen( "/dev/urandom", "rb" );
	if( !Urandom )
		return 0;
	UBOOL Ok = fread( &Out, sizeof(Out), 1, Urandom )==1;
	fclose( Urandom );
	return Ok;
#endif
}

static FString GenerateCDKey()
{
	guard(GenerateCDKey);
	QWORD Value = 0;
	if( !CDKeyEntropy( Value ) )
		return TEXT("");

	// 63 bits: the validator reads the 14 symbols back as one 64-bit number and formats it signed.
	// 14 symbols of this alphabet can hold more than 64 bits, so a free draw could overflow that
	// parse, and how an overflow is handled is a property of the C library, not of the key.
	Value &= (QWORD)0x7FFFFFFFFFFFFFFFLL;

	const INT Base = CDKEYBASE;
	TCHAR Random[15];
	for( INT i=13; i>=0; i-- )
	{
		Random[i] = CDKEYBASEMAP[ (INT)(Value % Base) ];
		Value /= Base;
	}
	Random[14] = 0;

	// The validator reads the four groups as  [2][1][0][3]  =  random(14) + check(6).
	FString Full = FString(Random) + CDKeyCheckFor( FString(Random) );
	FString Key  = Full.Mid(10,5) + TEXT("-") + Full.Mid(5,5) + TEXT("-") + Full.Left(5) + TEXT("-") + Full.Mid(15,5);

	// Never write a key the validator itself would turn away.
	if( Key.Len()!=23 || !ValidateCDKey( *Key ) )
		return TEXT("");
	return Key;
	unguard;
}
#endif

/*-----------------------------------------------------------------------------
	GetRealCDKey
-----------------------------------------------------------------------------*/

static FString GetRealCDKey()
{
	static FString cdkey;

	if (GCDKS>0)
	{
		if (GCDKS==1)
			cdkey = TEXT("DMCTES-DMCTES-DMCTES-DMCTES");
		else 
			cdkey = TEXT("STAEHC-STAEHC-STAEHC-STAEHC");
	}

	if( cdkey != TEXT("") )
		return cdkey;

#if DEMOVERSION
	//cdkey = TEXT("UT3DEM-UT3DEM-UT3DEM-UT3DEM");
	cdkey = TEXT("UT2004-UTDEMO-UTDEMO-UT2004");
#else

#if defined(_WIN32) && !_XBOX
	// Read the cdkey from the registry.
	TCHAR Temp[256];
	FString Product;
	if( GConfig->GetString( TEXT("Engine.Engine"), TEXT("Product"), Temp, ARRAY_COUNT(Temp) ) )
		Product = Temp;
	HKEY hKey = NULL;
	bool RegOpened = ( RegOpenKeyX( HKEY_LOCAL_MACHINE, *FString::Printf(TEXT("Software\\Unreal Technology\\Installed Apps\\%s"),*Product), &hKey )==ERROR_SUCCESS );

// 32-bit installer on the retail disc doesn't have access to root of registry... --ryan.
#ifdef _WIN64
	if (!RegOpened)
		RegOpened = ( RegOpenKeyX( HKEY_LOCAL_MACHINE, *FString::Printf(TEXT("Software\\Wow6432Node\\Unreal Technology\\Installed Apps\\%s"),*Product), &hKey )==ERROR_SUCCESS );
#endif

	if (RegOpened)
	{
		TCHAR Buffer[4096]=TEXT("");
		DWORD Type=REG_SZ, BufferSize=sizeof(Buffer);
		if( RegQueryValueExX( hKey, TEXT("CDKey"), 0, &Type, (BYTE*)Buffer, &BufferSize )==ERROR_SUCCESS && Type==REG_SZ )
		{
			// force the intermediate characters in the cdkey to be -'s.
			if( appStrlen(Buffer) >= 23 )
			{
				Buffer[5] = '-';
				Buffer[11] = '-';
				Buffer[17] = '-';
			}
			cdkey = Buffer;
			cdkey = cdkey.Left(23).Caps();
		}
	}
#elif __UNIX__
    FArchive *in = GFileManager->CreateFileReader(TEXT("cdkey"));
    if (in == NULL)
    {
        // No key file: give this install an identity of its own and keep it (GenerateCDKey, above).
        // The key itself is never logged.
        cdkey = GenerateCDKey();
        FArchive *out = cdkey.Len() ? GFileManager->CreateFileWriter(TEXT("cdkey")) : NULL;
        if (out != NULL)
        {
            out->Serialize((void *) appToAnsi(*cdkey), cdkey.Len());
            delete out;
            debugf(TEXT("No cdkey file: generated a key for this install and saved it."));
        }
        else if (cdkey.Len())
        {
            debugf(TEXT("No cdkey file: generated a key for this install, but could not save it."));
        }
        else
        {
            debugf(TEXT("Couldn't open cdkey file."));
        }
    }
    else
    {
        char buf[128];
        int max = in->TotalSize();
        if (max >= sizeof (buf))
            max = sizeof (buf) - 1;
        in->Serialize(buf, max);
        delete in;

        for (int i = 0; i < max; i++)
        {
            char ch = buf[i];
            if ( ! ( ((ch >= '0') && (ch <= '9')) ||
                     ((ch >= 'a') && (ch <= 'z')) ||
                     ((ch >= 'A') && (ch <= 'Z')) ||
                     ((ch == '-')) ) )
            {
                buf[i] = '\0';
                break;
            }
        }

        buf[max] = '\0';  // just in case.
        cdkey = buf;

        //debugf("cdkey is [%s].", *cdkey);
    }
#else
#   error Please fill in CD Key stuff for your platform!
#endif

	// uppercase CD key
	cdkey = cdkey.Caps();

	// Dumbass user used spaces ' ' instead of dashes '-'

	while ( cdkey.InStr(TEXT(" "))>=0 )
		cdkey.Replace(TEXT(" "),TEXT("-"));

	// Dumass user used underscore '_' instead of dashes '-'

	while ( cdkey.InStr(TEXT("_"))>=0 )
		cdkey.Replace(TEXT("_"),TEXT("-"));

	// dumbass user just typed the text

	if (cdkey.InStr(TEXT("-")) == -1)  
    {
        FString newkey(cdkey.Left(5));
        newkey += TEXT("-");
        newkey += cdkey.Mid(5, 5);
        newkey += TEXT("-");
        newkey += cdkey.Mid(10, 5);
        newkey += TEXT("-");
        newkey += cdkey.Mid(15, 5);
        cdkey = newkey;
    }

#endif	// DEMOVERSION

	return cdkey;
}

/*-----------------------------------------------------------------------------
	Internal MD5 processing
-----------------------------------------------------------------------------*/

static FString GetDigestString( BYTE* Digest )
{
	FString MD5;
	for( INT i=0; i<16; i++ )
		MD5 += FString::Printf(TEXT("%02x"), Digest[i]);	
	return MD5;
}

static FString MD5HashAnsiString( const TCHAR* String )
{
	const ANSICHAR* AnsiChallenge = appToAnsi( String );
	BYTE Digest[16];
	FMD5Context Context;
	appMD5Init( &Context );
	appMD5Update( &Context, (unsigned char*)AnsiChallenge, appStrlen( String ) );
	appMD5Final( Digest, &Context );
	return GetDigestString( Digest );
}

static INT HexDigit( TCHAR c )
{
	if( c>='0' && c<='9' )
		return c - '0';
	else if( c>='a' && c<='f' )
		return c + 10 - 'a';
	else if( c>='A' && c<='F' )
		return c + 10 - 'A';
	else
		return 0;
}

/*-----------------------------------------------------------------------------
	Global CD Key functions
-----------------------------------------------------------------------------*/
#define TEST_CDKEY_VALIDATION_AND_TERMINATE 0

UBOOL ENGINE_API ValidateCDKey()
{
	return 1;
#if DEMOVERSION
	return 1;
#else

	#if TEST_CDKEY_VALIDATION_AND_TERMINATE
        // This is a file with valid cdkeys in ASCII with NO whitespace
        //  or newlines between them (the '-' chars should be included in,
        //  the file, though.
        // This lets me test for portability issues: byte ordering,
        //  64-bitness, compiler bugs, etc.  --ryan.
		FILE *fp = fopen("testcdkeysdump.txt", "rb");
		int keycount = 0;
		if (fp != NULL)
		{
			do
			{
				TCHAR tbuf[24];
				char buf[24];
				if (fread(buf, 23, 1, fp) == 1)
				{
					keycount++;
					buf[23] = '\0';
					appFromAnsi(buf, tbuf);
					if (!ValidateCDKey(tbuf))
						printf("cdkey: [%s] didn't validate!\n", buf);
					else
					{
						tbuf[3] = '1';  // not that this proves a lot...
						if (ValidateCDKey(tbuf))
							printf("cdkey: [%s] validated incorrectly!\n", buf);
					}
				}
			} while (!feof(fp));
		}

		printf("\n\n\ndone checking %d keys\n", keycount);

		fclose(fp);
		appRequestExit(0);
	#endif

	return ValidateCDKey(*GetRealCDKey());
#endif
}

FString ENGINE_API GetCDKeyHash()
{
	return MD5HashAnsiString(*GetRealCDKey());
}

FString ENGINE_API MangleKey(FString KeyHash, FString Key)
{
	// Compute MD5 Hash

	BYTE Buffer[16];
	for (INT i=0;i<16;i++)
	{
		Buffer[i] = KeyHash.Mid(2*i,2).HexValue();
	}
	FBlowFish BF;
	DWORD InitialCRC = appMemCrc(&Buffer, 16, 0);
	BF.Encrypt(&Buffer, 16, Key);

	BYTE Test[16];
	appMemcpy(Test, Buffer, 16);
	BF.Decrypt(&Test, 16, Key);

	FString EncHash;
	for( INT i=0; i<16; i++ )
	{
		EncHash += FString::Printf( TEXT("%02x"), Buffer[i]);	
	}

	EncHash+= FString::Printf(TEXT("%08x"),InitialCRC);
	return EncHash;
}

FString ENGINE_API UnmangleKey(FString KeyHash, FString Key)
{
	guard(Engine::UnmangleKey);

	if ( KeyHash.Len() != 40 )
	{
		return KeyHash;
	}
	else
	{
		// Convert the string back in to data		
		BYTE Buffer[16];
		for (INT i=0;i<16;i++)
		{
			Buffer[i] = KeyHash.Mid(2*i,2).HexValue();
		}
			
		INT Crc = KeyHash.Right(8).HexValue();
		FBlowFish BlowFish;

		BlowFish.Decrypt(&Buffer, 16, Key );								
		INT DecCrc = appMemCrc(&Buffer, 16, 0);

		if (DecCrc == Crc)		
		{

			debugf(TEXT("CRC=%04x vs %04x"),DecCrc,Crc);

			FString DecryptedHash;
			for( INT i=0; i<16; i++ )
			{
				DecryptedHash += FString::Printf(TEXT("%02x"), Buffer[i]);	
			}
			return DecryptedHash;
		}
		else
		{	
			debugf(TEXT("Dec Failed"));
		}
	}
	return KeyHash;

	unguard;
}


FString ENGINE_API GetCDKeyResponse( const TCHAR* Challenge )
{
	// Append challenge
	FString CDKeyChallenge = GetRealCDKey() + Challenge;

	// MD5
	return MD5HashAnsiString( *CDKeyChallenge );
}

FString ENGINE_API EncryptWithCDKeyHash( const TCHAR* String, const TCHAR* HashAppend )
{
	FString Result;
	FString Key = FString::Printf(TEXT("%s%s"), *GetRealCDKey(), HashAppend );
	FString Hash = MD5HashAnsiString( *Key );

    for( INT i=0;String[i];i++ )
		Result = Result + FString::Printf( TEXT("%02x"), (*Hash)[i%16]^String[i] );

	return Result;
}

FString ENGINE_API DecryptWithCDKeyHash( const TCHAR* String, const TCHAR* HashAppend, const TCHAR* InCDKey )
{
	FString Result, Hash;
	if( InCDKey )
	{
		FString Key = FString::Printf(TEXT("%s%s"), InCDKey, HashAppend );
		Hash = MD5HashAnsiString( *Key );
	}
	else
		Hash = HashAppend;

    for( INT i=0;String[i]&&String[i+1];i+=2 )
	{
		INT Ch = 16*HexDigit(String[i])+HexDigit(String[i+1]);
		Result = Result + FString::Printf( TEXT("%c"), (*Hash)[(i/2)%16]^Ch );
	}

	return Result;
}




/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

