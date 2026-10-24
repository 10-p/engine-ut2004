/*=============================================================================
	UnIX: Unix port of UnVcWin32.cpp.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Cloned by Mike Danylchuk
		* Severely amputated and mutilated by Brandon Reinhart
		* Surgically altered by Jack Porter
		* Mangled and rehabilitated by Brandon Reinhart
		* Obfuscated by Daniel Vogel
		* Peed on by Ryan C. Gordon
=============================================================================*/
#if __GNUC__ || (__INTEL_COMPILER && linux)

// Standard includes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <time.h>
#include <errno.h>

// System includes.
// This file is the platform layer for every non-MSVC target, Windows (MinGW) included. MinGW has
// unistd/dirent/utime/sys-time/sys-stat, but not the process, resource-limit or name-resolution headers;
// Windows answers those through Win32 and WinSock.
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <exception>
#include <signal.h>
#include <process.h>	// _spawnv
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <pthread.h>	// winpthreads (the toolchain's `posix` thread model)
#include <sys/types.h>
#else
#include <sys/param.h>
#include <unistd.h>
#include <dirent.h>
#include <utime.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/resource.h>

#if MACOSX
#  include <mach-o/dyld.h>
#  include "dlfcn_macosx.h"
#else
#  include <dlfcn.h>
#endif

#include <netdb.h>
//#include <fpu_control.h>

#include <pthread.h>
#include <sys/types.h>
#include <sys/wait.h>
#if defined(__ANDROID__)
#include <sys/system_properties.h>   // __system_property_get for debug.ut2004.nofault
#endif
#endif

#if __FILE_NOCASE
#include <dirent.h>
#endif

// Stack traces via C runtime...
// MinGW has no <execinfo.h> either: backtrace()/backtrace_symbols() are a glibc extension.
#if ((MACOSX) || (__FreeBSD__) || defined(__EMSCRIPTEN__) || defined(_WIN32))
#  define HAVE_EXECINFO_H 0
#else
#  define HAVE_EXECINFO_H 1
#  include <execinfo.h>
#endif

#ifdef __EMSCRIPTEN__
#  include <emscripten.h>
#endif

#ifdef _WIN32
// MAXPATHLEN is a BSD/glibc spelling; the Win32 equivalent is MAX_PATH.
#  ifndef MAXPATHLEN
#    define MAXPATHLEN MAX_PATH
#  endif
#endif

// Core includes.
#include "CorePrivate.h"

// rcg04232002 Just to make sure this is sane...
#if defined(NO_UNICODE_OS_SUPPORT)
#  if defined(UNICODE)
#    undef UNICODE
#    define UNICODE 0
#  endif
#endif

#if UNICODE
#  include <locale.h>
#  if !MACOSX
#    include <langinfo.h>
#  endif
#endif

#if MACOSX  // Carbon includes.  --ryan.
   // Carbon's headers define "check" and "verify", which we do, too,
   //  so just predeclare functions and types we need here.
extern "C"
{
    #define noErr 0
    typedef unsigned char Str255[256];
    typedef unsigned char Str63[64];
    typedef unsigned char Str32[33];
    typedef unsigned char Str31[32];
    typedef unsigned char Str27[28];
    typedef unsigned char Str15[16];
    typedef Str63 StrFileName;
    typedef const unsigned char *ConstStr255Param;
    typedef unsigned long OSType;
    typedef signed int OSStatus;
    typedef signed short OSErr;
    signed int GetCPUSpeed(void);

    typedef signed int CFIndex;
    typedef unsigned int CFStringEncoding;
                
    // (This isn't accurate, but will do for our purposes...)
    typedef void *CFURLRef;
    typedef void *CFStringRef;
    typedef void *CFAllocatorRef;

    typedef enum {
        kCFStringEncodingMacRoman = 0,
        kCFStringEncodingWindowsLatin1 = 0x0500, /* ANSI codepage 1252 */
        kCFStringEncodingISOLatin1 = 0x0201, /* ISO 8859-1 */
        kCFStringEncodingNextStepLatin = 0x0B01, /* NextStep encoding*/
        kCFStringEncodingASCII = 0x0600, /* 0..127 (in creating CFString, values greater than 0x7F are treated as corresponding Unicode value) */
        kCFStringEncodingUnicode = 0x0100, /* kTextEncodingUnicodeDefault  + kTextEncodingDefaultFormat (aka kUnicode16BitFormat) */
        kCFStringEncodingUTF8 = 0x08000100, /* kTextEncodingUnicodeDefault + kUnicodeUTF8Format */
        kCFStringEncodingNonLossyASCII = 0x0BFF /* 7bit Unicode variants used by Cocoa & Java */
    } CFStringBuiltInEncodings;

    CFURLRef CFURLCreateWithBytes(CFAllocatorRef allocator,
                                  const unsigned char *URLBytes,
                                  CFIndex length,
                                  CFStringEncoding encoding,
                                  CFURLRef baseURL);
    void LSOpenCFURLRef(CFURLRef urlref, void *something);

    CFStringRef CFStringCreateWithBytes (
        CFAllocatorRef alloc,
        const BYTE /*UInt8*/ *bytes,
        CFIndex numBytes,
        CFStringEncoding encoding,
        unsigned char /*Boolean*/ isExternalRepresentation
    );

    void CFRelease(void *ref);

    OSErr Gestalt(OSType selector, long *response);

    enum
    {
        gestaltCPU601                 = 0x0101, /* IBM 601 */
        gestaltCPU603                 = 0x0103,
        gestaltCPU604                 = 0x0104,
        gestaltCPU603e                = 0x0106,
        gestaltCPU603ev               = 0x0107,
        gestaltCPU750                 = 0x0108, /* Also 740 - "G3" */
        gestaltCPU604e                = 0x0109,
        gestaltCPU604ev               = 0x010A, /* Mach 5, 250Mhz and up */
        gestaltCPUG4                  = 0x010C, /* Max */
        gestaltCPUG47450              = 0x0110 /* Vger , Altivec */
    };

    enum
    {
        gestaltNativeCPUType          = 'cput', /* processor type */
        gestalt68000                  = 1,
        gestalt68010                  = 2,
        gestalt68020                  = 3,
        gestalt68030                  = 4,
        gestalt68040                  = 5
    };

    enum
    {
        gestaltSystemVersion = 'sysv'
    };

    enum
    {
        gestaltPowerPCProcessorFeatures = 'ppcf',
        gestaltPowerPCHasGraphicsInstructions = 0,
        gestaltPowerPCHasSTFIWXInstruction = 1,
        gestaltPowerPCHasSquareRootInstructions = 2,
        gestaltPowerPCHasDCBAInstruction = 3,
        gestaltPowerPCHasVectorInstructions = 4,
        gestaltPowerPCHasDataStreams = 5
    };

    enum
    {
        gestaltPhysicalRAMSize = 'ram '
    };

    enum
    {
        kAlertStopAlert = 0,
        kAlertNoteAlert = 1,
        kAlertCautionAlert = 2,
        kAlertPlainAlert = 3
    };

    OSErr StandardAlert (
       signed short inAlertType,
       ConstStr255Param inError,
       ConstStr255Param inExplanation,
       void *inAlertParam,
       signed short *outItemHit
    );

    OSStatus CreateStandardAlert (
        signed short /*AlertType*/ alertType,
        CFStringRef error,
        CFStringRef explanation,
        void /*const AlertStdCFStringAlertParamRec*/ * param,
        void * /*DialogRef*/ * outAlert
    );

    OSStatus RunStandardAlert (
        void * /*DialogRef*/ inAlert,
        void * /*ModalFilterUPP*/ filterProc,
        SWORD /*DialogItemIndex*/ * outItemHit
    );
}

#if UNICODE
    #define TCHAR_MAC_ENCODING kCFStringEncodingUnicode
#else
    #define TCHAR_MAC_ENCODING kCFStringEncodingISOLatin1
#endif

#endif

#include "vorbis/codec.h"
#include "vorbis/vorbisfile.h"

/*-----------------------------------------------------------------------------
	Globals
-----------------------------------------------------------------------------*/

// Module
ANSICHAR GModule[32];

// Environment
extern char **environ;

TCHAR *GUnixSpawnOnExit = NULL;

#if MACOSX
TCHAR *GMacOSAlertBoxOnExit = NULL;
TCHAR *GMacOSLaunchURLOnExit = NULL;
#endif

// filler.  --ryan.
HINSTANCE hInstance = 0;


#if UNICODE
CORE_API void unicode_str_to_stdout(const UNICHAR *str)
{
    if (str == NULL)
        str = TEXT("(null)");

    printf("UNICODE str: ");
    for (int i = 0; str[i]; i++)
        printf("%c", (ANSICHAR) str[i]);
    printf("\n");
}


CORE_API ANSICHAR* unixToANSI( ANSICHAR* ACh, const UNICHAR* InUCh )
{
	guardSlow(unixToANSI);
	if(InUCh)
	{
        INT Index;
		for(Index = 0;InUCh[Index] != 0;Index++)
		{
        	#if 0
				if (InUCh[Index] > 255)
					debugf("unixToANSI: can't convert character %d!", (int) InUCh[Index]);
            #endif

			ACh[Index] = (ANSICHAR) InUCh[Index];
		}
        ACh[Index] = 0;
		return ACh;
	}
	else
	{
		ACh[0] = 0;
		return ACh;
	}
	unguardSlow;
}


CORE_API INT unixGetSizeANSI( const UNICHAR* InUCh )
{
	guardSlow(unixGetSizeANSI);
	if(InUCh)
		return (wcslen(InUCh) + 1) * sizeof(ANSICHAR);
	else
		return sizeof(ANSICHAR);
	unguardSlow;
}


CORE_API UNICHAR* unixToUNICODE( UNICHAR* UCh, const ANSICHAR* InACh )
{
	guardSlow(unixToUNICODE);
	if(InACh)
	{
        INT Index;
		for(Index = 0;InACh[Index] != 0;Index++)
        {
			UCh[Index] = (UNICHAR) InACh[Index];
        }
        UCh[Index] = 0;
		return UCh;
	}
	else
	{
		UCh[0] = 0;
		return UCh;
	}
    unguardSlow;
}


CORE_API INT unixGetSizeUNICODE( const ANSICHAR* InACh )
{
	guardSlow(unixGetSizeUNICODE);
	if(InACh)
		return (strlen(InACh) + 1) * sizeof(UNICHAR);
	else
		return sizeof(UNICHAR);
    unguardSlow;
}


CORE_API UNICHAR* unixANSIToUNICODE(char* str)
{
	guardSlow(unixANSIToUnicode);
	INT iLength = unixGetSizeUNICODE(str);
	UNICHAR* pBuffer = new UNICHAR[iLength];
	appStrcpy(pBuffer,TEXT(""));  // !!! FIXME: Is this really necessary?
	return unixToUNICODE(pBuffer,str);
    unguardSlow;
}


CORE_API void unixDetectUNICODE( void )
{
    guard(unixDetectUNICODE);

#if 0
    errno = 0;
    if (!setlocale(LC_CTYPE, ""))
    {
        debugf(TEXT("setlocale() failed: %s"), strerror(errno));
        debugf(TEXT("Must assume ANSI (8-bit) character encoding."));
    }
    else
    {
        char *encoding = "Unknown";
        if (getenv("UNREAL_UNICODE") != NULL)
            GUnicode = GUnicodeOS = 1;
        else
        {
            #if MACOSX
                debugf(TEXT("No locale detection on OS X at this moment."));
            #else
                encoding = nl_langinfo(_NL_CTYPE_CODESET_NAME);
                char *unicode_encodings[] = { "UTF-8", "UTF8", NULL };
                for (INT i = 0; unicode_encodings[i]; i++)
                {
                    if (strcmp(encoding, unicode_encodings[i]) == 0)
                    {
                        GUnicode = GUnicodeOS = 1;
                    }
                }
            #endif
        }

        debugf(TEXT("Your locale: [%s].\n"), ANSI_TO_TCHAR(encoding));
        if (GUnicodeOS)
            debugf(TEXT("Enabled UNICODE support."));
    }
#endif

    unguard;
}
#endif


#if UNICODE_BY_HAND
#ifndef __EMSCRIPTEN__
// On Emscripten, system <wchar.h> provides standard wcs* functions.
// Only compile the engine's custom implementations on native builds
// where -fshort-wchar makes wchar_t 16-bit (incompatible with glibc).
CORE_API UNICHAR* wcscpy( UNICHAR* _Dest, const UNICHAR* Src)
{
    guardSlow(wcscpy)

    UNICHAR *Dest = _Dest;

    while (*Src)
    {
        *Dest = *Src;
        Dest++;
        Src++;
    }

    *Dest = 0;

    return(_Dest);

    unguardSlow;
}


CORE_API UNICHAR* wcsncpy( UNICHAR* _Dest, const UNICHAR* Src, INT MaxLen )
{
    guardSlow(wcsncpy)

    UNICHAR *Dest = _Dest;

    MaxLen--;  // always leave room for null char.
    for (INT i = 0; ((i < MaxLen) && (*Src)); i++)
    {
        *Dest = *Src;
        Dest++;
        Src++;
    }

    *Dest = 0;

    return(Dest);

    unguardSlow;
}


CORE_API INT wcslen( const UNICHAR* String )
{
    guardSlow(wcslen);

    INT retval = 0;
    for (retval = 0; *String; String++)
        retval++;

    return(retval);

    unguardSlow;
}


CORE_API INT wcscmp( const UNICHAR* Str1, const UNICHAR *Str2 )
{
    guardSlow(wcscmp);

    while (true)
    {
        UNICHAR ch1 = *Str1;
        UNICHAR ch2 = *Str2;

        if ((ch1 == 0) && (ch1 == ch2))
            return(0);

        if (ch1 < ch2)
            return(-1);
        else if (ch1 > ch2)
            return(1);

        Str1++;
        Str2++;
    }

    return(1);  // shouldn't ever hit this.

    unguardSlow;
}


CORE_API INT wcsncmp( const UNICHAR* Str1, const UNICHAR *Str2, INT max )
{
    guardSlow(wcscmp);

    INT i = 0;
    while (true)
    {
        UNICHAR ch1 = *Str1;
        UNICHAR ch2 = *Str2;

        if (i == max)
            return(0);

        if ((ch1 == 0) && (ch1 == ch2))
            return(0);

        if (ch1 < ch2)
            return(-1);
        else if (ch1 > ch2)
            return(1);

        i++;
        Str1++;
        Str2++;
    }

    return(1);  // shouldn't ever hit this.

    unguardSlow;
}

// Engine-specific functions that don't conflict with system headers
// (compiled on all platforms including Emscripten)
#endif // !__EMSCRIPTEN__ (standard wcs* functions -- reopened below)

CORE_API INT _wcsicmp( const UNICHAR* String1, const UNICHAR* String2 )
{
    guardSlow(_wcsicmp);

    /*
    printf("_wcsicmp:\n");
    printf("  "); unicode_str_to_stdout(String1);
    printf("  "); unicode_str_to_stdout(String2);
    */

    while (true)
    {
        int ch1 = toupper((int) *String1);
        int ch2 = toupper((int) *String2);

        if ((ch1 == 0) && (ch1 == ch2))
            return(0);

        if (ch1 < ch2)
            return(-1);
        else if (ch1 > ch2)
            return(1);

        String1++;
        String2++;
    }

    return(1);  // shouldn't ever hit this.

    unguardSlow;
}



CORE_API INT _wcsnicmp( const UNICHAR* Str1, const UNICHAR *Str2, INT max )
{
    guardSlow(_wcsnicmp);

    /*
    printf("_wcsnicmp:\n");
    printf("  "); unicode_str_to_stdout(Str1);
    printf("  "); unicode_str_to_stdout(Str2);
    */

    INT i = 0;
    while (true)
    {
        UNICHAR ch1 = toupper((int) *Str1);
        UNICHAR ch2 = toupper((int) *Str2);

        if (i == max)
            return(0);

        if ((ch1 == 0) && (ch1 == ch2))
            return(0);

        if (ch1 < ch2)
            return(-1);
        else if (ch1 > ch2)
            return(1);

        i++;
        Str1++;
        Str2++;
    }

    return(1);  // shouldn't ever hit this.

    unguardSlow;
}


#ifndef __EMSCRIPTEN__
// Standard wcs* functions (continued) -- provided by system <wchar.h> on Emscripten
CORE_API UNICHAR* wcschr( const UNICHAR* String, const UNICHAR Find )
{
    guardSlow(wcschr);

    while (*String)
    {
        if (*String == Find)
            return((UNICHAR *) String);

        String++;
    }

    return(NULL);

    unguardSlow;
}


CORE_API UNICHAR* wcsstr( const UNICHAR* String, const UNICHAR* Find )
{
    guardSlow(wcsstr);
    INT findlen = wcslen(Find);

    while (true)
    {
        String = wcschr(String, *Find);
        if ( String == NULL)
            return(NULL);

        if (wcsncmp(String, Find, findlen) == 0)
            return((UNICHAR *) String);

        String++;
    }

    return(NULL);  // shouldn't ever hit this point.
    unguardSlow;
}


CORE_API UNICHAR* wcscat( UNICHAR* String, const UNICHAR *Add )
{
    guardSlow(wcscat);
    wcscpy(String + wcslen(String), Add);
    return(String);
    unguardSlow;
}

#endif // !__EMSCRIPTEN__ (wcschr, wcsstr, wcscat)

CORE_API UNICHAR* _wcsupr( UNICHAR* String )
{
    guardSlow(_wcsupr);
    for (UNICHAR *ptr = String; *ptr; ptr++)
        *ptr = (UNICHAR) toupper((int) *ptr);

    return(String);
    unguardSlow;
}


CORE_API INT _wtoi( const UNICHAR* Str )
{
    // !!! FIXME: This might be completely broken.
    char *ansistr = TCHAR_TO_ANSI(Str);
    return(atoi(ansistr));
}


#ifndef __EMSCRIPTEN__
// wcstoul and swscanf conflict with system <wchar.h> on Emscripten
CORE_API INT wcstoul( const TCHAR* Start, TCHAR** End, INT Base )
{
    // !!! FIXME: This might be completely broken.
    return(strtoul(TCHAR_TO_ANSI(Start), (char **) End, Base));
}

CORE_API INT swscanf( const UNICHAR* fmt, ... )
{
    debugf(TEXT("This is currently unimplemented on Unix."));
    appDebugBreak();
}
#endif // !__EMSCRIPTEN__

// !!! FIXME: This can always be more robust.
#ifndef __EMSCRIPTEN__
CORE_API INT wvsnprintf( TCHAR *buf, INT max, const TCHAR *fmt, va_list args )
{
    if (fmt == NULL)
    {
        if ((max > 0) && (buf != NULL))
            *buf = 0;
        return(0);
    }

    TCHAR *src = (TCHAR *) appAlloca((wcslen(fmt) + 1) * sizeof (TCHAR));
    wcscpy(src, fmt);

    TCHAR *dst = buf;
    TCHAR *enddst = dst + (max - 1);

    //printf("printf by-hand formatting "); unicode_str_to_stdout(src);

    while ((*src) && (dst < enddst))
    {
        if (*src != '%')
        {
            *dst = *src;
            dst++;
            src++;
            continue;
        }

        TCHAR *percent_ptr = src;
        INT fieldlen = -1;
        INT precisionlen = -1;

        src++; // skip the '%' char...

        while (*src == ' ')
        {
            *dst = ' ';
            dst++;
            src++;
        }

            // check for field width requests...
        if ((*src == '-') || ((*src >= '0') && (*src <= '9')))
        {
            TCHAR *ptr = src + 1;
            while ((*ptr >= '0') && (*ptr <= '9'))
                ptr++;

            TCHAR ch = *ptr;
            *ptr = '\0';
            fieldlen = atoi(TCHAR_TO_ANSI(src));
            *ptr = ch;

            src = ptr;
        }

        if (*src == '.')
        {
            TCHAR *ptr = src + 1;
            while ((*ptr >= '0') && (*ptr <= '9'))
                ptr++;

            TCHAR ch = *ptr;
            *ptr = '\0';
            precisionlen = atoi(TCHAR_TO_ANSI(src + 1));
            *ptr = ch;
            src = ptr;
        }

        if (*src == 'l')  /* might be meaningless; let it slide. */
            src++;

        switch (*src)
        {
            case '%':
            {
                src++;
                *dst = '%';
                dst++;
            }
            break;

            case 'c':
            {
                TCHAR val = (TCHAR) va_arg(args, int);
                src++;
                *dst = val;
                dst++;
            }
            break;

            case 'd':
            case 'i':
            case 'X':
            case 'x':
            case 'u':
            {
                src++;
                int val = va_arg(args, int);
                ANSICHAR ansinum[30];
                ANSICHAR fmtbuf[30];

                // Yes, this is lame.
                INT cpyidx = 0;
                while (percent_ptr < src)
                {
                    fmtbuf[cpyidx] = (ANSICHAR) *percent_ptr;
                    percent_ptr++;
                    cpyidx++;
                }
                fmtbuf[cpyidx] = 0;

                int rc = snprintf(ansinum, sizeof (ansinum), fmtbuf, val);
                if (rc > (int) sizeof (ansinum) - 1)
                    rc = sizeof (ansinum) - 1;
                if ((dst + rc) > enddst)
                    rc = enddst - dst;
                for (int i = 0; i < rc; i++)
                {
                    *dst = (TCHAR) ansinum[i];
                    dst++;
                }
            }
            break;

            case 'I':
            {
                if ((src[1] != '6') || (src[2] != '4'))
                {
                    debugf(TEXT("Unknown percent in UnUnix.cpp::wvsnprintf()."));
                    unicode_str_to_stdout(fmt);
                    appDebugBreak();
                    src++;  // skip it, I guess.
                    break;
                }

                src += 4;
                QWORD val = va_arg(args, QWORD);
                ANSICHAR ansinum[60];
                ANSICHAR fmtbuf[30];
                strcpy(fmtbuf, "%L");
                percent_ptr += 4;

                // Yes, this is lame.
                INT cpyidx = 2;
                while (percent_ptr < src)
                {
                    fmtbuf[cpyidx] = (ANSICHAR) *percent_ptr;
                    percent_ptr++;
                    cpyidx++;
                }
                fmtbuf[cpyidx] = 0;

                int rc = snprintf(ansinum, sizeof (ansinum), fmtbuf, val);
                if (rc > (int) sizeof (ansinum) - 1)
                    rc = sizeof (ansinum) - 1;
                if ((dst + rc) > enddst)
                    rc = enddst - dst;
                for (int i = 0; i < rc; i++)
                {
                    *dst = (TCHAR) ansinum[i];
                    dst++;
                }
            }
            break;

            case 'f':
            case 'e':
            case 'E':
            case 'g':
            case 'G':
            {
                src++;
                double val = va_arg(args, double);
                ANSICHAR ansinum[64];
                ANSICHAR fmtbuf[30];

                // Yes, this is lame.
                INT cpyidx = 0;
                while (percent_ptr < src)
                {
                    fmtbuf[cpyidx] = (ANSICHAR) *percent_ptr;
                    percent_ptr++;
                    cpyidx++;
                }
                fmtbuf[cpyidx] = 0;

                int rc = snprintf(ansinum, sizeof (ansinum), fmtbuf, val);
                // snprintf returns the length it wanted: copy only what the buffer holds.
                if (rc > (int) sizeof (ansinum) - 1)
                    rc = sizeof (ansinum) - 1;
                if ((dst + rc) > enddst)
                    rc = enddst - dst;
                for (int i = 0; i < rc; i++)
                {
                    *dst = (TCHAR) ansinum[i];
                    dst++;
                }
            }
            break;

            case 's':
            {
                src++;
                TCHAR *val = va_arg(args, TCHAR *);
                if (val == NULL)
                    val = TEXT("(null)");

                int rc = wcslen(val);
                if ((dst + rc) > enddst)
                    rc = enddst - dst;
                for (int i = 0; i < rc; i++)
                {
                    *dst = *val;
                    dst++;
                    val++;
                }
            }
            break;

            default:
                src++;  // skip char, I guess.
                debugf(TEXT("Unknown percent in UnUnix.cpp::wvsnprintf()."));
                unicode_str_to_stdout(fmt);
                appDebugBreak();
                break;
        }
    }

    *dst = 0;  // null terminate the new string.
    return(dst - buf);
}
#endif // !__EMSCRIPTEN__ (wvsnprintf)


#ifndef __EMSCRIPTEN__
// wprintf conflicts with system <wchar.h> on Emscripten
CORE_API INT wprintf( const TCHAR* fmt, ... )
{
    va_list args;
    TCHAR buf[1024];  // (*shrug*)

    va_start(args, fmt);
    INT rc = wvsnprintf(buf, sizeof (buf) / sizeof (TCHAR), fmt, args);
    va_end(args);

    if (GUnicodeOS)
        fwrite(buf, sizeof (TCHAR), rc, stdout);
    else
        fwrite(appToAnsi(buf), sizeof (ANSICHAR), rc, stdout);

    return(rc);
}
#endif // !__EMSCRIPTEN__ (wprintf)

#ifndef __EMSCRIPTEN__
// wvsnprintf and iswspace are provided by system headers on Emscripten.
CORE_API INT iswspace( UNICHAR ch  )
{
    return((INT) isspace((int) ch));
}
#endif // !__EMSCRIPTEN__

CORE_API TCHAR *_itow( const INT Num, TCHAR *Buffer, const INT Radix )
{
    return(_i64tow((SQWORD) Num, Buffer, Radix));
}

CORE_API QWORD _wcstoui64( const TCHAR* Start, TCHAR** End, INT Base )
{
    check((Base >= 2) && (Base <= 36));

    while (*Start == ' ')
        Start++;

    const TCHAR *ptr = Start;
    while (1)
    {
        char ch = toupper((char) *ptr);

        if ((ch >= 'A') && (ch <= 'Z'))
        {
            if ((Base <= 10) || (ch >= ('A' + Base)))
                break;
        }

        else if ((ch >= '0') && (ch <= '9'))
        {
            if (ch >= ('0' + Base))
                break;
        }

        else
            break;

        ptr++;
    }

    if (End != NULL)
        *End = const_cast<TCHAR *>(ptr);

    QWORD retval = 0;
    QWORD accumulator = 1;
    while (--ptr >= Start)
    {
        char ch = toupper((char) *ptr);
        QWORD val = (((ch >= 'A') && (ch <= 'Z')) ? (ch - 'A') + 10 : ch - '0');
        retval += val * accumulator;
        accumulator *= (QWORD) Base;
    }

    return(retval);
}


CORE_API TCHAR* _ui64tow( QWORD Num, TCHAR *Buffer, INT Base )
{
    check((Base >= 2) && (Base <= 36));

    int bufidx = 1;
    for (QWORD x = Num; x >= Base; x /= Base)
        bufidx++;

    Buffer[bufidx] = 0;  /* null terminate where the end will be. */

    while (--bufidx >= 0)
    {
        int val = (int) (Num % Base);
        Buffer[bufidx] = ((val > 9) ? ('a' + (val - 10)) : ('0' + val));
        Num /= Base;
    }

    return(Buffer);
}

CORE_API TCHAR* _i64tow( SQWORD Num, TCHAR *Buffer, INT Base )
{
    check((Base >= 2) && (Base <= 36));

    int bufidx = 1;
    int negative = 0;
    if (Num < 0)
    {
        negative = 1;
        Num = -Num;
        bufidx++;
        Buffer[0] = '-';
    }

    for (QWORD x = Num; x >= Base; x /= Base)
        bufidx++;

    Buffer[bufidx] = 0;  /* null terminate where the end will be. */

    while ((--bufidx - negative) >= 0)
    {
        int val = (int) (Num % Base);
        Buffer[bufidx] = ((val > 9) ? ('a' + (val - 10)) : ('0' + val));
        Num /= Base;
    }

    return(Buffer);
}


CORE_API FLOAT _wtof( const TCHAR* Str )
{
    return((FLOAT) atof(appToAnsi(Str)));
}

#endif


/* visual C++ compatibility. --ryan. */
char *strlwr(char *str)
{
    for (char *p = str; *p; p++)
        *p = (char) tolower((int) *p);

    return(str);
}


CORE_API const UBOOL appMsgf( INT Type, const TCHAR* Fmt, ... )
{
	TCHAR TempStr[4096]=TEXT("");

	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt, Fmt );

#if MACOSX
    guard(MacOSX_appMsgf);
    debugf( TEXT("appMsgf: %s"), TempStr );
    INT len = appStrlen(TempStr);
    GMacOSAlertBoxOnExit = new TCHAR[len + 1];
    appStrcpy(GMacOSAlertBoxOnExit, TempStr);
    appRequestExit(0);
    unguard;
#else
    printf("appMsgf(): %s\n", TCHAR_TO_ANSI(TempStr));
    appDebugBreak();
#endif

    return 1;
}



// Interface for recording loading errors in the editor
CORE_API const void EdClearLoadErrors()
{
	GEdLoadErrors.Empty();
}


/*-----------------------------------------------------------------------------
	USystem.
-----------------------------------------------------------------------------*/

//
// System manager.
//
static void Recurse()
{
	guard(Recurse);
	Recurse();
	unguard;
}
USystem::USystem()
:	SavePath	( E_NoInit )
,	CachePath	( E_NoInit )
,	CacheExt	( E_NoInit )
,	Paths		( E_NoInit )
,	Suppress	( E_NoInit )
,   CacheRecordPath( E_NoInit )
,	MusicPath	( E_NoInit )
,	SpeechPath	( E_NoInit )
// amb ---
,   LicenseeMode (1) 
// --- amb
{}
void USystem::StaticConstructor()
{
	guard(USystem::StaticConstructor);

	new(GetClass(),TEXT("PurgeCacheDays"),      RF_Public)UIntProperty   (CPP_PROPERTY(PurgeCacheDays     ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("SavePath"),            RF_Public)UStrProperty   (CPP_PROPERTY(SavePath           ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("CachePath"),           RF_Public)UStrProperty   (CPP_PROPERTY(CachePath          ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("CacheExt"),            RF_Public)UStrProperty   (CPP_PROPERTY(CacheExt           ), TEXT("Options"), CPF_Config );
//	new(GetClass(),TEXT("CacheRecordPath"),     RF_Public)UStrProperty   (CPP_PROPERTY(CacheRecordPath    ), TEXT("Options"), CPF_Config );
//	new(GetClass(),TEXT("MusicPath"),           RF_Public)UStrProperty   (CPP_PROPERTY(MusicPath          ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("SpeechPath"),          RF_Public)UStrProperty   (CPP_PROPERTY(SpeechPath         ), TEXT("Options"), CPF_Config );

	UArrayProperty* A = new(GetClass(),TEXT("Paths"),RF_Public)UArrayProperty( CPP_PROPERTY(Paths), TEXT("Options"), CPF_Config );
	A->Inner = new(A,TEXT("StrProperty0"),RF_Public)UStrProperty;

	UArrayProperty* B = new(GetClass(),TEXT("Suppress"),RF_Public)UArrayProperty( CPP_PROPERTY(Suppress), TEXT("Options"), CPF_Config );
	B->Inner = new(B,TEXT("NameProperty0"),RF_Public)UNameProperty;

	UArrayProperty* C = new(GetClass(),TEXT("CacheRecordPath"),RF_Public)UArrayProperty( CPP_PROPERTY(CacheRecordPath), TEXT("Options"), CPF_Config );
	C->Inner = new(C,TEXT("StrProperty1"),RF_Public)UStrProperty;

	UArrayProperty* D = new(GetClass(),TEXT("MusicPath"),RF_Public)UArrayProperty( CPP_PROPERTY(MusicPath), TEXT("Options"), CPF_Config );
	D->Inner = new(D,TEXT("StrProperty2"),RF_Public)UStrProperty;

	unguard;
}

static bool FindBinaryInPath(FString &cmd);

UBOOL USystem::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( ParseCommand(&Cmd,TEXT("MEMSTAT")) )
	{
		//!UNIX No MEMSTAT command.
		Ar.Logf( TEXT("MEMSTAT command not available.") );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("EXIT")) )
	{
		Ar.Log( TEXT("Closing by request") );
		appRequestExit( 0 );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("APP")) )
	{
		//!UNIX No APP command.
		Ar.Logf( TEXT("APP command not available.") );
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("RELAUNCH") ) )
	{
		debugf( TEXT("Relaunch: %s"), Cmd );
		GConfig->Flush( 0 );

        if ((GIsClient) && (appStrlen(Cmd) == 0))  // just restarting client?
        {
            GUnixSpawnOnExit = new TCHAR[128];
            appStrcpy(GUnixSpawnOnExit, TEXT("./ut2004-bin"));  // !!! FIXME
            appRequestExit( 0 );
            return 1;
        }

		// Fork out a new process using Cmd as arguments.
		TCHAR* EndArg0 = appStrstr( Cmd, TEXT(" ") );
		INT CmdLen = appStrlen( Cmd );
		INT RestLen = ((EndArg0 != NULL) ? appStrlen( EndArg0 ) : 0);
        #define RELAUNCH_ARRAY 1024
		TCHAR Arg0[RELAUNCH_ARRAY];
        INT max = (CmdLen - RestLen) + 1;
        if (max > RELAUNCH_ARRAY)
            max = RELAUNCH_ARRAY;
		appStrncpy( Arg0, Cmd, max );
        Arg0[RELAUNCH_ARRAY - 1] = 0;  // just in case.

#if 1
        INT arraylen = RELAUNCH_ARRAY + 128;
        FString xterm(TEXT(""));

        #if MACOSX
            char cwdbuf[MAXPATHLEN];
            getcwd(cwdbuf, sizeof (cwdbuf));
            cwdbuf[sizeof (cwdbuf) - 1] = '\0';

            xterm = TEXT("./osxterminal.sh --title '");
            xterm += LocalizeGeneral("Product", appPackage());
            xterm += TEXT(" Dedicated Server'");  // !!! FIXME: localize!
            xterm += TEXT(" --showshell false --showsize false");
            xterm += TEXT(" --showfile false --showdevice false");
            xterm += TEXT(" --activate -e 'cd \"");
            xterm += appFromAnsi(cwdbuf);
            xterm += TEXT("\" ; ./ucc-bin server \"");
            xterm += Arg0;
            xterm += TEXT("\" ; exit'");

            GUnixSpawnOnExit = new TCHAR[xterm.Len() + 128];
            appStrcpy(GUnixSpawnOnExit, *xterm);
        #elif defined(_WIN32)
            // The console UCC beside the game; Windows gives it a console window of its own.
            GUnixSpawnOnExit = new TCHAR[arraylen];
            appStrcpy(GUnixSpawnOnExit, TEXT("ucc.exe server "));
            appStrcat(GUnixSpawnOnExit, Arg0);
        #else
            xterm = TEXT("xterm");
            if (FindBinaryInPath(xterm))
            {
                arraylen += xterm.Len();
                xterm += TEXT(" -e ");
            }
            else
            {
                xterm = TEXT("");
            }

            GUnixSpawnOnExit = new TCHAR[arraylen];
            appStrcpy(GUnixSpawnOnExit, *xterm);
            appStrcat(GUnixSpawnOnExit, TEXT("./ucc-bin server "));
            appStrcat(GUnixSpawnOnExit, Arg0);
        #endif

        appRequestExit( 0 );
#else

        char ansistr[RELAUNCH_ARRAY];
        appToAnsi(Arg0, ansistr);

		if (fork() == 0)
		{
			appSleep(3);
			INT error = execl("./ucc-bin", "./ucc-bin", "server", ansistr, NULL );
			if (error == -1)
				appErrorf( TEXT("Failed to launch process.") );
		} else {
			appRequestExit( 0 );
		}
#endif
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("DEBUG") ) )
	{
		if( ParseCommand(&Cmd,TEXT("CRASH")) )
		{
			appErrorf( TEXT("%s"), TEXT("Unreal crashed at your request") );
			return 1;
		}
		else if( ParseCommand( &Cmd, TEXT("GPF") ) )
		{
			Ar.Log( TEXT("Unreal crashing with voluntary GPF") );
			*(int *)NULL = 123;
			return 1;
		}
		else if( ParseCommand( &Cmd, TEXT("RECURSE") ) )
		{
			Ar.Logf( TEXT("Recursing") );
			Recurse();
			return 1;
		}
		else if( ParseCommand( &Cmd, TEXT("EATMEM") ) )
		{
			Ar.Log( TEXT("Eating up all available memory") );
			while( 1 )
			{
				void* Eat = appMalloc(65536,TEXT("EatMem"));
				memset( Eat, 0, 65536 );
			}
			return 1;
		}
		else return 0;
	}
	else return 0;
}
IMPLEMENT_CLASS(USystem);

/*-----------------------------------------------------------------------------
	Exit.
-----------------------------------------------------------------------------*/

//
// Immediate exit.
//
CORE_API void appRequestExit( UBOOL Force )
{
	guard(appForceExit);
	debugf( TEXT("appRequestExit(%i)"), Force );
	if( Force )
	{
		// Force immediate exit. Dangerous because config code isn't flushed, etc.
		exit( 1 );
	}
	else
	{
		// Tell the platform specific code we want to exit cleanly from the main loop.
		GIsRequestingExit = 1;
#ifdef __EMSCRIPTEN__
		// Ask the page to return to the launcher after cleanup. MAIN_THREAD_EM_ASM on the threaded build:
		// this runs on the worker, where `window` does not exist, and only the browser main thread can
		// navigate. A plain EM_ASM on a non-threaded build.
#ifdef __EMSCRIPTEN_PTHREADS__
		MAIN_THREAD_EM_ASM({
#else
		EM_ASM({
#endif
			if (typeof window !== 'undefined') {
				console.log('[SHELL] Engine exit requested, returning to menu');
				setTimeout(function() {
					window.location.href = window.location.pathname;
				}, 0);
			}
		});
#endif
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Clipboard.
-----------------------------------------------------------------------------*/

CORE_API void ClipboardCopy( const TCHAR* Str )
{
	guard(ClipboardCopy);
	//!UNIX Not supported in UNIX.
	unguard;
}

CORE_API void ClipboardPaste( FString& Result )
{
	guard(ClipboardPasteString);
	//!UNIX Not supported in UNIX.
	unguard;
}

/*-----------------------------------------------------------------------------
	Shared libraries.
-----------------------------------------------------------------------------*/

//
// Load a library.
//
void* appGetDllHandle( const TCHAR* Filename )
{
	guard(appGetDllHandle);

#ifdef __EMSCRIPTEN__
	// All modules are statically linked on Emscripten -- no dlopen.
	// Return sentinel meaning "symbol is in the main binary."
	return (void*)-1;
#endif

#ifdef _WIN32
	// The package path (UObject::BindPackage asking "is this native package available?"). BUILD_STATIC is
	// on, so every package that exists is the main binary, and the sentinel says so. Answering NULL for
	// packages this build lacks (as the POSIX path below does) crashes at startup, and a LoadLibrary
	// fallback would load one of the retail 2004 DLLs an asset tree ships. A genuine external library has
	// its own entry point: appGetSystemDllHandle.
	return (void*)-1;
#endif

	void *Result = NULL;

	#if 0 //__STATIC_LINK
		return((void*) -1);
	#endif

#ifndef _WIN32	// unreachable on Windows (sentinel above), and dlfcn does not exist there.
	check(Filename);
	const TCHAR* PackageName = Filename;
	TCHAR* Cur;
	TCHAR* Test = appStaticString1024();

#if MACOSX
    #define SYMPREPEND TEXT("_")
#else
    #define SYMPREPEND TEXT("")
#endif

	// Get GLoadedPackage symbol name from full path.
 	while( (Cur = appStrchr( PackageName, '/' )) != NULL )
		PackageName = Cur + 1;
 	while( (Cur = appStrchr( PackageName, '\\' )) != NULL )
		PackageName = Cur + 1;
	appSprintf( Test, TEXT("%sGLoaded%s"), SYMPREPEND, PackageName );
	if( (Cur = appStrchr( Test, '.' )) != NULL )
		*Cur = '\0';

#if MACOSX
    // written with guidance from OpenDarwin.org's "dlcompat".
    //   http://opendarwin.org/cgi-bin/cvsweb.cgi/proj/dlcompat/

    check(appStrlen(Filename) < 1000);

    if (NSIsSymbolNameDefined(appToAnsi(Test)))
        Result = (void *) -1;  // it's in the main binary.
    else
    {
        char *path = appAnsiStaticString1024();
        appToAnsi(Filename, path);

        if (strstr(path, ".dylib") == NULL)
            strcat(path, ".dylib");

	Result = dlopen(path, RTLD_GLOBAL | RTLD_NOW);
   }

#else

	const char* Error;
	dlerror();	// Clear any error condition.

	// Check if the library was linked to the executable.
	Result = (void*)dlopen( NULL, RTLD_NOW | RTLD_GLOBAL );
	Error = dlerror();
	if( Error != NULL )
		debugf( TEXT("dlerror(): %s"), Error );
	else
	{
		(void*)dlsym( Result, appToAnsi(Test) );
		Error = dlerror();
		if( Error == NULL )
			return Result;
	}

#if defined(__ANDROID__)
	// The engine is the app's shared library, not the executable, so the lookup above misses the packages linked
	// into it; bionic's RTLD_DEFAULT also searches the calling library.
	if( dlsym( RTLD_DEFAULT, appToAnsi(Test) ) )
		return (void*)-1;
#endif

	// Load the new library.
	Result = (void*)dlopen( appToAnsi(Filename), RTLD_NOW | RTLD_GLOBAL );
	if( Result == NULL )
	{
		FString Temp = FString(Filename) + DLLEXT;	// a path of any length
		Result = (void*)dlopen( appToAnsi(*Temp), RTLD_NOW | RTLD_GLOBAL );
        #ifdef LOOKING_FOR_UNDEFINED_SYMBOLS
    		Error = dlerror();
    		if( Error != NULL )
    		debugf( "dlerror(): %s", Error );
        #endif
	}

#endif // MACOSX vs. Linux

	if (Result != NULL)
	{
		const char *envr = getenv("UNREAL_PAUSE_ON_DLOPEN");
		if (envr)
		{
			const TCHAR *libname = ANSI_TO_TCHAR(envr);
			if (appStrfind(Filename, libname))
			{
				printf("%s loaded. Sleeping ten seconds...\n", envr);
				sleep(10);
			}
		}
	}

#if 0
	if (Result == NULL)
		debugf(TEXT("Failed to dynamically load [%s]."), Filename);
#endif

	return Result;
#endif // !_WIN32
	unguard;
}

//
// Load a genuine external library, not an engine package (UnFile.h: on a static Windows build the two
// cannot share an entry point).
//
void* appGetSystemDllHandle( const TCHAR* Filename )
{
	guard(appGetSystemDllHandle);
	check(Filename);

#ifdef __EMSCRIPTEN__
	// Statically linked, and FindProcs binds straight to the in-binary symbols; the sentinel just has to
	// be non-NULL so ALAudio's Init() proceeds.
	return (void*)-1;
#elif defined(_WIN32)
	// ALAudio is DYNAMIC_BIND: it asks for "OpenAL32.dll" by name and resolves every AL entry point from
	// the handle, so this is a real LoadLibrary (appGetDllHandle answers with the in-exe sentinel).
	{
		HMODULE Module = LoadLibraryA( appToAnsi(Filename) );
		if( !Module )
		{
			TCHAR Temp[256];
			appStrcpy( Temp, Filename );
			appStrcat( Temp, DLLEXT );
			Module = LoadLibraryA( appToAnsi(Temp) );
		}
		return (void*)Module;
	}
#else
	void* Result = (void*)dlopen( appToAnsi(Filename), RTLD_NOW | RTLD_GLOBAL );
	if( !Result )
	{
		TCHAR Temp[256];
		appStrcpy( Temp, Filename );
		appStrcat( Temp, DLLEXT );
		Result = (void*)dlopen( appToAnsi(Temp), RTLD_NOW | RTLD_GLOBAL );
	}
	return Result;
#endif

	unguard;
}

//
// Free a library.
//
void appFreeDllHandle( void* DllHandle )
{
	guard(appFreeDllHandle);
	check(DllHandle);

	if (DllHandle == (void *) -1)
		return;

#ifdef _WIN32
	// appGetDllHandle can hand back a real module (the sentinel is filtered out above): release it.
	FreeLibrary( (HMODULE)DllHandle );
	return;
#else
	// remember that you can unload bundles,
	//  but not .dylib files on Darwin! --ryan.
	dlclose( DllHandle );
#endif

	unguard;
}

//
// Lookup the address of a shared library function.
//
void* appGetDllExport( void* DllHandle, const TCHAR* ProcName )
{
	guard(appGetDllExport);
	check(DllHandle);
	check(ProcName);

#ifdef __EMSCRIPTEN__
	// On Emscripten, all modules are statically linked. The sentinel (void*)-1
	// from appGetDllHandle means "in main binary." Look up symbols in the
	// global scope using RTLD_DEFAULT (works for -lopenal linked functions).
	if( DllHandle == (void*)-1 )
	{
		void* Result = dlsym( RTLD_DEFAULT, appToAnsi(ProcName) );
		return Result;
	}
#endif

#ifdef _WIN32
	// The sentinel means "it is in the exe". GetProcAddress on the module handle is the Win32 spelling of
	// dlsym(RTLD_DEFAULT) and resolves only exported names, so the launcher links with
	// --export-all-symbols, which makes the package registrants (GLoaded<Package>) visible here. A real
	// module (OpenAL32.dll) is asked first; a library that does not export the name falls back to the exe.
	{
		HMODULE Module = (DllHandle == (void*)-1) ? GetModuleHandleA( NULL ) : (HMODULE)DllHandle;
		void* Result = (void*)GetProcAddress( Module, appToAnsi(ProcName) );
		if( !Result && Module != GetModuleHandleA( NULL ) )
			Result = (void*)GetProcAddress( GetModuleHandleA( NULL ), appToAnsi(ProcName) );
		return Result;
	}
#endif

#ifndef _WIN32	// unreachable on Windows (GetProcAddress above).
	void* Result = NULL;

	const char *Error;

	dlerror();	// Clear any error condition.

	Result = (void*)dlsym( DllHandle, appToAnsi(ProcName) );
	Error = dlerror();
	if( Error != NULL )
		debugf( TEXT("dlerror: %s"), ANSI_TO_TCHAR(Error) );

	return Result;
#endif // !_WIN32

	unguard;
}

//
// Break the debugger.
//
void appDebugBreak()
{
	guard(appDebugBreak);
#if defined(ASMLINUX) && !defined(UE_NO_X86_INTRIN)
	asm("int $03");
#else
	raise(SIGTRAP);
#endif
	unguard;
}

//
// IsDebuggerPresent()
//
UBOOL appIsDebuggerPresent()
{
	// Does not need to be implemented.
	return 0;
}

/*-----------------------------------------------------------------------------
	External processes.
-----------------------------------------------------------------------------*/

static TMap<pid_t,int>* ExitCodeMap = NULL;

void* appCreateProc( const TCHAR* URL, const TCHAR* Parms )
{
	guard(appCreateProc);

	debugf( TEXT("Create Proc: %s %s"), URL, Parms );

	TCHAR LocalParms[PATH_MAX] = TEXT("");
	appStrcpy(LocalParms, URL);
	appStrcat(LocalParms, TEXT(" "));
	appStrcat(LocalParms, Parms);

#ifdef _WIN32
	// _spawnlp via the shell is the Windows equivalent of fork-then-system. The returned handle is a process
	// handle rather than a pid; appGetProcReturnCode's map is keyed on it either way.
	intptr_t pid = _spawnlp( _P_NOWAIT, "cmd.exe", "cmd.exe", "/c", appToAnsi(LocalParms), (char*)NULL );
	if( pid == -1 )
		debugf( TEXT("appCreateProc: failed to launch %s"), LocalParms );
#else
	pid_t pid = fork();
	if (pid == 0)
		_exit( system( appToAnsi(LocalParms) ) );
#endif

	return (void*) ((PTRINT) pid);
	unguard;
}

UBOOL appGetProcReturnCode( void* ProcHandle, INT* ReturnCode )
{
	guard(appGetProcReturnCode);
	int* p = ExitCodeMap->Find( (pid_t) ((PTRINT) ProcHandle) );
	if(p)
	{
		*ReturnCode = *p;
		ExitCodeMap->Remove( (pid_t) ((PTRINT) ProcHandle) );
		return 1;
	}
	return 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	Signal Handling.
-----------------------------------------------------------------------------*/

static void HandleChild(int Signal)
{
#ifndef _WIN32
	// SIGCHLD reaping is POSIX-only. Windows has no zombie processes to reap (a spawned child's exit code is
	// read from its handle), and this is only installed by the POSIX-only sigaction block in appPlatformInit.
	int Status;
	pid_t pid;
	while( (pid=waitpid(-1, &Status, WNOHANG)) > 0 )
		ExitCodeMap->Set( pid, WEXITSTATUS(Status) );
#endif
}

static pid_t mainPID;
static INT AlreadyAborting	= 0;
static INT SignalExit		= 0;
static INT SignalCritical	= 0;

static void HandleSignal( int Signal )
{
	char* sig_text;
	switch (Signal)
	{
#ifndef _WIN32	// not defined by MinGW's <signal.h>: no such signal on Windows.
		case SIGHUP:
			sig_text = "Signal: SIGHUP [hangup]";
			SignalExit++;
			break;
#endif
#ifndef _WIN32	// not defined by MinGW's <signal.h>: no such signal on Windows.
		case SIGQUIT:
			sig_text = "Signal: SIGQUIT [quit]";
			SignalExit++;
			break;
#endif
		case SIGILL:
			sig_text = "Signal: SIGILL [illegal instruction]";
			SignalCritical++;
			break;
#ifndef _WIN32	// not defined by MinGW's <signal.h>: no such signal on Windows.
		case SIGTRAP:
			sig_text = "Signal: SIGTRAP [trap]";
			SignalCritical++;
			break;
#endif
#ifndef _WIN32	// not defined by MinGW's <signal.h>: no such signal on Windows.
		case SIGIOT:
			sig_text = "Signal: SIGIOT [iot trap]";
			SignalCritical++;
			break;
#endif
#ifndef _WIN32	// not defined by MinGW's <signal.h>: no such signal on Windows.
		case SIGBUS:
			sig_text = "Signal: SIGBUS [bus error]";
			SignalCritical++;
			break;
#endif
		case SIGFPE:
			sig_text = "Signal: SIGFPE [floating point exception]";
			SignalCritical++;
			break;
		case SIGSEGV:
			sig_text = "Signal: SIGSEGV [segmentation fault]";
			SignalCritical++;
			break;
		case SIGTERM:
			sig_text = "Signal: SIGTERM [terminate]";
			SignalExit++;
			break;
		case SIGINT:  // must have gone through HandleInterrupt previously.
			sig_text = "Signal: SIGINT [interrupt]";
			SignalCritical++;
			break;
		default:
			sig_text = "Signal: UNKNOWN SIGNAL [%i]";
			SignalCritical++;
			break;
	}

	// A second exit signal is reported and ignored: the engine is already leaving its main loop for the first,
	// and the abort below would run appExit() inside this handler on top of the interrupted thread's own exit
	// (the same objects twice, or a lock the interrupted code holds). `timeout` signals the process and then its
	// process group. SIGKILL still ends the process at once; Ctrl-C twice still aborts (HandleInterrupt).
	if ( SignalCritical == 0 && SignalExit > 1 )
	{
		static const char Ignored[] = "Signal: exit already requested; this one is ignored (SIGKILL ends the process now).\n";
		write( 1, sig_text, strlen(sig_text) );
		write( 1, "\n", 1 );
		write( 1, Ignored, sizeof(Ignored)-1 );
		return;
	}

	if ( (SignalCritical > 0) || (SignalExit > 1) )
	{
		if (!AlreadyAborting)
		{
			// Avoid calling appExit again.
			AlreadyAborting = true;

#if !defined(_WIN32) && !defined(__EMSCRIPTEN__)
			// The report and appExit() below are not async-signal-safe (malloc, the log, the inis) and can
			// block for ever on a lock the crashed thread held; SIGALRM's default action ends the process if
			// they have not finished in 10 seconds.
			alarm( 10 );
#endif

			printf( "%s\n", sig_text );
			printf( "Aborting.\n\n" );
			printf( "\nCrash information will be saved to your logfile.\n" );

			#if HAVE_EXECINFO_H && !defined(__ANDROID__)
				void *aTrace[64];
				char **tString;
				int size = backtrace(aTrace, ARRAY_COUNT(aTrace));
				tString = backtrace_symbols(aTrace, size);
				debugf(TEXT("\nDeveloper Backtrace:"));
				for (int i = 0; i < size; i++)
					debugf(TEXT("[%2.i]  %s"), i+1, appFromAnsi(tString[i]));
  			#endif

			#if DO_PORTABLE_GUARD_BLOCKS
				FString GuardBackTrace;
				UnGuardBlock::GetBackTrace(GuardBackTrace);
				debugf(TEXT("Unreal Call Stack: %s"), *GuardBackTrace);
			#endif

			appExit();
		}
		exit(1);
	}
	if ( SignalExit == 1 )
	{
		// Only async-signal-safe calls here (write, and the flag the main loop polls): printf and
		// appRequestExit could deadlock or corrupt the log when the signal interrupted a thread inside malloc
		// or the log, and the signal may land on any thread.
		static const char Requesting[] = "Requesting Exit.\n";
		write( 1, sig_text, strlen(sig_text) );
		write( 1, "\n", 1 );
		write( 1, Requesting, sizeof(Requesting)-1 );
		GIsRequestingExit = 1;
	}
}


// special case for CTRL-C.  --ryan.
static void HandleInterrupt( int Signal )
{
    check(Signal==SIGINT);

    if (getpid() == mainPID)
    {
        static bool alreadyTriggered = false;

        if (alreadyTriggered)
        {
            // User is mashing CTRL-C, or appRequestExit() work is deadlocked.
            // Pass SIGINT to our standard interrupt handler for panic crash.
            HandleSignal(Signal);
        }

        alreadyTriggered = true;

        if (GIsRunning)   // already in main loop.
        {
            // let the engine try to shut down cleanly.
            // Async-signal-safe, as in HandleSignal.
            static const char Pressed[] = "CTRL-C has been pressed.\n";
            write( 1, Pressed, sizeof(Pressed)-1 );
            GIsRequestingExit = 1;
        }

        else
        {
            // Need to force an immediate termination with _exit(),
            //  since things with static destructors may not be initialized
            //  yet! Otherwise, this could cause a confusing segfault in
            //  the normal exit() processing. Please note that calling
            //  appRequestExit(1) will result in a normal exit() call.
            printf("CTRL-C before main loop ... forcing exit.\n");
            _exit(42);  // all threads should die.
        }
    }
}


/*-----------------------------------------------------------------------------
	Timing.
-----------------------------------------------------------------------------*/

//
// String timestamp.
//
CORE_API const TCHAR* appTimestamp()
{
	guard(appTimestamp);

	TCHAR* 		Result = appStaticString1024();
	time_t		CurTime;
	struct tm*	SysTime;

	CurTime = time( NULL );
	SysTime = localtime( &CurTime );
	appSprintf( Result, ANSI_TO_TCHAR(asctime( SysTime )) );
	// Strip newline.
	Result[appStrlen( Result )-1] = '\0';
	return Result;

	unguard;
}

//
// Get file time.
//
CORE_API DWORD appGetTime( const TCHAR* Filename )
{
	guard(appGetTime);

	struct utimbuf FileTime;
	if( utime(TCHAR_TO_ANSI(Filename),&FileTime)!=0 )
		return 0;
	//FIXME Is this the right "time" to return?
	return (DWORD)FileTime.modtime;

	unguard;
}

//
// Get time in seconds.
//
CORE_API DOUBLE appSecondsSlow()
{
	static DOUBLE InitialTime = 0.0;
	static DOUBLE TimeCounter = 0.0;
	DOUBLE NewTime;
	struct timeval TimeOfDay;

	gettimeofday( &TimeOfDay, NULL );

	// Initialize.
	if( InitialTime == 0.0 )
		 InitialTime = TimeOfDay.tv_sec + TimeOfDay.tv_usec / 1000000.0;

	// Accumulate difference to prevent wraparound.
	NewTime = TimeOfDay.tv_sec + TimeOfDay.tv_usec / 1000000.0;
	TimeCounter += NewTime - InitialTime;
	InitialTime = NewTime;

	return TimeCounter;
}


#if !DEFINED_appCycles
CORE_API DWORD appCycles()
{
	return (DWORD)appSeconds();
}
#endif


//
// Get time in seconds. Origin is arbitrary.
//
#if !DEFINED_appSeconds
CORE_API DOUBLE appSeconds()
{
	return appSecondsSlow();
}
#endif


CORE_API void appArgv0(const char *argv0)
{
#if MACOSX
    // MacOS X has this concept of "Application Bundles" which makes the whole
    //  install tree look like a single icon in the Finder, which, when
    //  launched, loads the game binary. They, however, totally fuck up your
    //  game's filesystem...best thing to do is see if the binary is running
    //  in a dir that appears to be embedded in an Application Bundle and
    //  chdir to the game's System dir...  --ryan.
    //
    //  (FIXME: There _are_ Carbon APIs to determine this via process info...)

    char buf[MAXPATHLEN];
    if ((argv0 != NULL) && (strchr(argv0, '/') != NULL)) // no path specifed?
        strncpy(buf, argv0, sizeof (buf));
    else
    {
        // From the Finder, current working directory is "/", which sucks.
        if ((getcwd(buf, sizeof (buf)) == NULL) || (strcmp(buf, "/") == 0))
            return;  // hail mary...probably fail elsewhere.
    }

    buf[sizeof (buf) - 1] = '\0';  // null terminate, just in case.

    char realbuf[MAXPATHLEN];
#ifdef _WIN32	// _fullpath is the Win32 realpath (note the reversed argument order).
    if (_fullpath(realbuf, buf, sizeof(realbuf)) == NULL)
#else
    if (realpath(buf, realbuf) == NULL)
#endif
        return;

    char *ptr = strstr(realbuf, "/Contents/MacOS/");
    if (ptr != NULL)
    {
        *ptr = '\0';  // chop off bundle bin dirs...
        strcat(realbuf, "/System");   // add system dir to path...
    }
    else
    {
        // not an application bundle, but might be a symlink to elsewhere,
        //  so chdir to there, just in case.
        ptr = strrchr(realbuf, '/');  // chop off binary name.
        if (ptr != NULL)
            *ptr = '\0';
    }

    if (chdir(realbuf) == -1)  // go there.
        return;
#endif
}


//
// Return the system time.
//
CORE_API void appSystemTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	guard(appSystemTime);

	time_t			CurTime;
	struct tm		*St;		// System time.
	struct timeval	Tv;			// Use timeval to get milliseconds.

	gettimeofday( &Tv, NULL );
	CurTime = time( NULL );
	St = localtime( &CurTime );

	Year		= St->tm_year + 1900;
	Month		= St->tm_mon + 1;
	DayOfWeek	= St->tm_wday;
	Day			= St->tm_mday;
	Hour		= St->tm_hour;
	Min			= St->tm_min;
	Sec			= St->tm_sec;
	MSec		= (INT) (Tv.tv_usec / 1000);

	unguard;
}

CORE_API void appSleep( FLOAT Seconds )
{
	guard(appSleep);

#ifdef __EMSCRIPTEN__
	// Yield to the worker's event loop (Asyncify) instead of busy-waiting. EnforceTickRate calls this as the
	// frame-rate cap, so it is also the per-frame yield a networked client needs: WebRTC data-channel
	// messages and signaling run between engine frames.
	emscripten_sleep( (unsigned int)(Seconds * 1000.0f) );
#else
	INT SleepTime = (INT) (Seconds * 1000000);
	usleep( SleepTime );
#endif

	unguard;
}

/*-----------------------------------------------------------------------------
	Link functions.
-----------------------------------------------------------------------------*/

static bool FindBinaryInPath(FString &cmd)
{
    INT binpos = cmd.InStr(TEXT(" "));
    FString bin((binpos == -1) ? cmd : cmd.Left(binpos));

    if (bin.Len() == 0)
        return(false);

    if (bin.InStr(TEXT("/")) != -1)  // path specified; don't examine $PATH.
        return(access(appToAnsi(*bin), X_OK) != -1);

    const char *_envr = getenv("PATH");
    if (_envr == NULL)
    {
        debugf(TEXT("No $PATH environment var set"));
        return(false);
    }

    FString envr(_envr);

    while (true)
    {
        INT pos = envr.InStr(TEXT(":"));
        FString path((pos == -1) ? envr : envr.Left(pos));
        path += TEXT("/");

        FString binpath = path;
        binpath += bin;

        char realized[MAXPATHLEN];
#ifdef _WIN32	// _fullpath, as above.
        if (_fullpath(realized, appToAnsi(*binpath), sizeof(realized)) != NULL)
#else
        if (realpath(appToAnsi(*binpath), realized) != NULL)
#endif
        {
            struct stat statbuf;
            if (stat(realized, &statbuf) != -1)
            {
                /* dirs are usually "executable", but aren't programs. */
                if (!S_ISDIR(statbuf.st_mode))
                {
                    if (access(realized, X_OK) != -1)
                    {
                        cmd = FString(realized) + cmd.Mid(binpos);
                        return(true);
                    }
                }
            }
        }

        if (pos == -1)
            break;
        envr = envr.Mid(pos + 1);
    }

    return(false);
}


//
// Launch a uniform resource locator (i.e. http://www.epicgames.com/unreal).
// This is expected to return immediately as the URL is launched by another
// task.
//
void appLaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error )
{
	guard(appLaunchURL);

#if MACOSX

    // DO NOT MERGE INTO WARFARE    
    // change hardcoded URL from Infogrames to MacSoft for Mac version...
    if (appStrcmp(URL, TEXT("http://www.unrealtournament.com/")) == 0)
        URL = TEXT("http://store.yahoo.com/boldgames/");
    // DO NOT MERGE INTO WARFARE

    GMacOSLaunchURLOnExit = new TCHAR[appStrlen(URL) + 1];
    appStrcpy(GMacOSLaunchURLOnExit, URL);

#else

    const char *_envr = getenv("BROWSER");
    if (!_envr)
    {
        debugf(TEXT("Tried to view URL but BROWSER environment var isn't set!"));
        debugf(TEXT(" URL to view was [%s]."), URL);
        debugf(TEXT(" Trying some standard browsers..."), URL);
        //_envr = "opera:netscape -raise -remote \"openURL(%s,new-window)\":mozilla:konqueror:galeon:xterm -e links:xterm -e lynx:";
        _envr = "opera:mozilla:galeon:konqueror:netscape:xterm -e links:xterm -e lynx:";
    }

    FString envr(appFromAnsi(_envr));

    bool foundBrowser = false;

    while (true)
    {
        INT pos = envr.InStr(TEXT(":"));
        FString cmd((pos == -1) ? envr : envr.Left(pos));
        INT had_percent_s = false;
        INT percentpos = 0;
        while (percentpos < cmd.Len() - 1)
        {
            percentpos++;
            if (cmd[percentpos - 1] != '%')
                continue;

            // need to deal with a '%' sequence...

            if (cmd[percentpos] == '%')
            {
                cmd = cmd.Left(percentpos) + cmd.Mid(percentpos + 1);
            }
            else if (cmd[percentpos] == 's')
            {
                cmd = cmd.Left(percentpos - 1) + URL + cmd.Mid(percentpos + 1);
                percentpos += appStrlen(URL);
                had_percent_s = true;
            }
            else
            {
                debugf(TEXT("Unrecognized percent sequence (%%%c) in BROWSER string."), (TCHAR) cmd[percentpos]);
                percentpos++;
            }
        }

        if (!had_percent_s)
        {
            cmd += TEXT(" ");
            cmd += URL;
        }

        if (FindBinaryInPath(cmd))
        {
            foundBrowser = true;
            debugf(TEXT("Spawning browser: %s"), *cmd);
            GUnixSpawnOnExit = new TCHAR[cmd.Len() + 1];
            appStrcpy(GUnixSpawnOnExit, *cmd);
            break;
        }

        if (pos == -1)
            break;
        envr = envr.Mid(pos + 1);
    }

    if (!foundBrowser)
    {
        debugf(TEXT("Couldn't find any browser to launch!"));
        debugf(TEXT("Please set the environment variable BROWSER next time."));

        fprintf(stderr, "Couldn't find any browser to launch!\n");
        fprintf(stderr, "Please set the environment variable BROWSER next time.\n");
    }

#endif

	appRequestExit(0);

	unguard;
}

/*-----------------------------------------------------------------------------
	File finding.
-----------------------------------------------------------------------------*/

//
// Clean out the file cache.
//
static INT GetFileAgeDays( const TCHAR* Filename )
{
	guard(GetFileAgeDays);
	struct stat Buf;
	INT Result = 0;

	Result = stat(TCHAR_TO_ANSI(Filename),&Buf);
	if( Result==0 )
	{
		time_t CurrentTime, FileTime;
		FileTime = Buf.st_mtime;
		time( &CurrentTime );
		DOUBLE DiffSeconds = difftime( CurrentTime, FileTime );
		return (INT)(DiffSeconds / 60.0 / 60.0 / 24.0);
	}
	return 0;
	unguard;
}

CORE_API void appCleanFileCache()
{
	guard(appCleanFileCache);

	// Delete all temporary files.
	guard(DeleteTemps);
	FString Temp = FString::Printf( TEXT("%s%s*.tmp"), *GSys->CachePath, PATH_SEPARATOR );
	TArray<FString> Found = GFileManager->FindFiles( *Temp, 1, 0 );
	for( INT i=0; i<Found.Num(); i++ )
	{
		Temp = FString::Printf( TEXT("%s%s%s"), *GSys->CachePath, PATH_SEPARATOR, *Found(i) );
		debugf( TEXT("Deleting temporary file: %s"), *Temp );
		GFileManager->Delete( *Temp );
	}
	unguard;

	// Delete cache files that are no longer wanted.
	guard(DeleteExpired);
	TArray<FString> Found = GFileManager->FindFiles( *(GSys->CachePath * TEXT("*") + GSys->CacheExt), 1, 0 );
	if( GSys->PurgeCacheDays )
	{
		for( INT i=0; i<Found.Num(); i++ )
		{
			FString Temp = FString::Printf( TEXT("%s%s%s"), *GSys->CachePath, PATH_SEPARATOR, *Found(i) );
			INT DiffDays = GetFileAgeDays( *Temp );
			if( DiffDays > GSys->PurgeCacheDays )
			{
				debugf( TEXT("Purging outdated file from cache: %s (%i days old)"), *Temp, DiffDays );
				GFileManager->Delete( *Temp );
			}
		}
	}
	unguard;

	unguard;
}

/*-----------------------------------------------------------------------------
	Guids.
-----------------------------------------------------------------------------*/

//
// Create a new globally unique identifier.
//
CORE_API FGuid appCreateGuid()
{
	guard(appCreateGuid);

	FGuid Result;
	appGetGUID( (void*)&Result );
	return Result;

	unguard;
}

/*-----------------------------------------------------------------------------
	Clipboard
-----------------------------------------------------------------------------*/
static FString ClipboardText;
CORE_API void appClipboardCopy( const TCHAR* Str )
{
	guard(appClipboardCopy);
	ClipboardText = FString( Str );
	unguard;
}

CORE_API FString appClipboardPaste()
{
	guard(appClipboardPaste);
	return ClipboardText;
	unguard;
}
/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/

// Get startup directory.
CORE_API const TCHAR* appBaseDir()
{
	guard(appBaseDir);
	static TCHAR BaseDir[PATH_MAX]=TEXT("");
    ANSICHAR AnsiBaseDir[PATH_MAX];

	if( !BaseDir[0] )
	{
#if defined(__ANDROID__)
		// The app's JNI bridge sets UT2004_ANDROID_BASEDIR to the on-storage System dir (and chdir's
		// there) so the engine reads the game tree in place through its POSIX file layer. Re-chdir in
		// case the cwd moved during SDL/thread startup, then getcwd below seeds BaseDir. Unset: falls through.
		const ANSICHAR* AndroidBaseDir = getenv("UT2004_ANDROID_BASEDIR");
		if( AndroidBaseDir && AndroidBaseDir[0] )
			chdir( AndroidBaseDir );
#endif
		// If the executable isn't launched from its own directory, then the
		// dynamic linker won't be able to find the shared libraries.
		getcwd( AnsiBaseDir, sizeof(AnsiBaseDir) );
        appStrcpy( BaseDir, ANSI_TO_TCHAR(AnsiBaseDir));
		appStrcat( BaseDir, TEXT("/") );
	}
	return BaseDir;
	unguard;
}

// Get computer name.
CORE_API const TCHAR* appComputerName()
{
	guard(appComputerName);
	static TCHAR Result[256]=TEXT("");
	static ANSICHAR AnsiResult[256];
	if( !Result[0] )
	{
		gethostname( AnsiResult, sizeof(AnsiResult) );
		appStrcpy( Result, ANSI_TO_TCHAR(AnsiResult));
	}
	return Result;
	unguard;
}

// Get user name.
CORE_API const TCHAR* appUserName()
{
	guard(appUserName);
	static TCHAR Result[256]=TEXT("");
	if( !Result[0] )
	{
#ifdef _WIN32	// no getlogin() on Windows.
		ANSICHAR NameBuf[256] = "";
		DWORD NameLen = ARRAY_COUNT(NameBuf);
		if( !GetUserNameA( NameBuf, &NameLen ) )
			strcpy( NameBuf, "Player" );
		ANSICHAR *str = NameBuf;
#else
		ANSICHAR *str = getlogin();
#endif
		appStrncpy( Result, ANSI_TO_TCHAR(str), sizeof(Result) );
	}
	return Result;
	unguard;
}

// Get launch package base name.
CORE_API const TCHAR* appPackage()
{
	guard(appPackage);
	return(appStrcpy(appStaticString1024(), ANSI_TO_TCHAR(GModule)));
	unguard;
}



/*-----------------------------------------------------------------------------
	App init/exit.
-----------------------------------------------------------------------------*/

//
// Platform specific initialization.
//
void appPlatformPreInit()
{
#if UNICODE
    unixDetectUNICODE();
#endif

#if __LINUX_X86__
    #ifdef __x86_64__
        GRunningOS = OS_LINUX_X86_64;
        appSprintf(GMachineOS, TEXT("x86-64 Linux"));
    #else
        GRunningOS = OS_LINUX_X86;
        appSprintf(GMachineOS, TEXT("x86 Linux"));
    #endif

#elif (defined __FreeBSD__)
    #error determine if x86 or Alpha or whatnot...
    GRunningOS = OS_FREEBSD_X86;
    appSprintf(GMachineOS, TEXT("x86 FreeBSD"));

#elif (defined MACOSX)
    GRunningOS = OS_MAC_OSX_PPC;

    long osver = 0x0000;
	OSErr err = Gestalt(gestaltSystemVersion, &osver);
    TCHAR verstr[16];
    appSprintf(verstr, TEXT("%X"), osver);

    appSprintf(GMachineOS, TEXT("MacOS %c%c.%c.%c"),
                verstr[0], verstr[1], verstr[2], verstr[3]);

    GMacOSVer = (INT) osver;
    check((long) GMacOSVer == osver);

#elif defined(_WIN32)
    // The engine only wants a human-readable OS string here.
    {
        OSVERSIONINFOA osvi;
        appMemzero( &osvi, sizeof(osvi) );
        osvi.dwOSVersionInfoSize = sizeof(osvi);
        if( GetVersionExA( &osvi ) )
            appSprintf( GMachineOS, TEXT("Windows %u.%u (build %u)"),
                (unsigned) osvi.dwMajorVersion, (unsigned) osvi.dwMinorVersion, (unsigned) osvi.dwBuildNumber );
        else
            appStrcpy( GMachineOS, TEXT("Windows") );
    }
#else
    // (more to come. --ryan.)
    #error fill in your platform.
#endif

    mainPID = getpid();
}


// bleh, no recursive mutexes on MacOSX.  :(  --ryan.
typedef struct
{
    pthread_mutex_t mutex;
    pthread_t tid;
    INT refcount;
} pthread_mutex_block;

FCriticalSection::FCriticalSection()
{
	guard(FCriticalSection::FCriticalSection);
	pthread_mutex_block *block = new pthread_mutex_block;
	int rc = pthread_mutex_init(&block->mutex, NULL);
	check(rc == 0);
    block->refcount = 0;
    block->tid = 0;
    Handle = block;
	unguard;
}

FCriticalSection::~FCriticalSection()
{
	guard(FCriticalSection::~FCriticalSection);
	pthread_mutex_block *block = (pthread_mutex_block *) Handle;
	int rc = pthread_mutex_destroy(&block->mutex);
	check(rc == 0);
    delete block;
	unguard;
}

void FCriticalSection::Lock()
{
    //printf("MUTEX LOCK START %p\n", Handle);
	pthread_mutex_block *block = (pthread_mutex_block *) Handle;
    pthread_t self = pthread_self();
    if (self == block->tid)
    {
        //printf("MUTEX REQUIRED RECURSION! (%p:%d)\n",
        //        Handle, (int) block->refcount);
        block->refcount++;
    }
    else
    {
	    int rc = pthread_mutex_lock(&block->mutex);
	    check(rc == 0);
        check(block->refcount == 0);
        block->tid = self;
        block->refcount = 1;
    }
    //printf("MUTEX LOCK END   %p\n", Handle);
}

void FCriticalSection::Unlock()
{
    //printf("MUTEX UNLOCK START %p\n", Handle);
	pthread_mutex_block *block = (pthread_mutex_block *) Handle;
    pthread_t self = pthread_self();
    check(self == block->tid);
    block->refcount--;
    check(block->refcount >= 0);
    if (block->refcount == 0)
	{
        block->tid = 0;
        int rc = pthread_mutex_unlock(&block->mutex);
	    check(rc == 0);
    }
    //printf("MUTEX UNLOCK END   %p\n", Handle);
}

FScopeCriticalSection::FScopeCriticalSection( FCriticalSection* InCriticalSection )
{
	CriticalSection = InCriticalSection;
	CriticalSection->Lock();
}

FScopeCriticalSection::~FScopeCriticalSection()
{
	CriticalSection->Unlock();
}


static void DoCPUID( int i, DWORD *A, DWORD *B, DWORD *C, DWORD *D )
{
// !!! FIXME: Crashes on Hammer after ASM block!  --ryan.
#if ((__LINUX_X86__) && (!defined __x86_64__) && !defined(UE_NO_X86_INTRIN))
    DWORD buf[4];

 	__asm__ __volatile__ (
        // the very definition of gay: EBX is used by the caller,
        //  who won't save it, and won't acknoledge we're clobbering it.
        //  Do a pushall/popall, just in case.
        #ifdef __x86_64__
        "pushq %%rax           \n\t"
        "pushq %%rbx           \n\t"
        "pushq %%rcx           \n\t"
        "pushq %%rdx           \n\t"
        #else
        "pushl %%eax           \n\t"
        "pushl %%ebx           \n\t"
        "pushl %%ecx           \n\t"
        "pushl %%edx           \n\t"
        #endif

        "cpuid                      \n\t"
        "movl  %%eax,  0("UREG_EDI") \n\t"
        "movl  %%ebx,  4("UREG_EDI") \n\t"
        "movl  %%ecx,  8("UREG_EDI") \n\t"
        "movl  %%edx, 12("UREG_EDI") \n\t"

        #ifdef __x86_64__
        "popq  %%rdx           \n\t"
        "popq  %%rcx           \n\t"
        "popq  %%rbx           \n\t"
        "popq  %%rax           \n\t"
        #else
        "popl  %%edx           \n\t"
        "popl  %%ecx           \n\t"
        "popl  %%ebx           \n\t"
        "popl  %%eax           \n\t"
        #endif

            : /* output to memory directly. */
            : "a" (i), "D" (buf)
            : "memory"
    );

    *A = buf[0];
    *B = buf[1];
    *C = buf[2];
    *D = buf[3];
#else
	*A=*B=*C=*D=0;
#endif
}


#ifndef _WIN32
// <winnt.h> already defines LARGE_INTEGER (as a union, with the members the other way round). Nothing here
// uses it on Windows, so take the platform's rather than redefine it.
typedef struct
{
    DWORD HighPart;
    DWORD LowPart;
} LARGE_INTEGER;
#endif

#define DESIRED_FILE_HANDLES 1024

#ifdef _WIN32
//
// Last-resort crash reporter for the Windows build: the POSIX sigaction block below does not apply there,
// and the guard/unguard chain reports only thrown errors, so a hardware fault would end the process with
// no output.
//
// It does not symbolise: the exe carries DWARF, which dbghelp cannot read, and a DWARF reader is the least
// safe code to run in a crash path. It prints the image base and raw addresses, which
// `addr2line -e ut2004.exe` turns into file:line on the machine that built it (the image is ASLR'd, so
// the base is needed).
//
static LONG WINAPI appWinUnhandledException( EXCEPTION_POINTERS* Info )
{
	static UBOOL Reentered = 0;
	if( Reentered )
		return EXCEPTION_EXECUTE_HANDLER;	// a fault inside the reporter must not loop
	Reentered = 1;

	GIsCriticalError = 1;

	const DWORD Code = Info && Info->ExceptionRecord ? Info->ExceptionRecord->ExceptionCode : 0;
	const TCHAR* Name;
	switch( Code )
	{
		case EXCEPTION_ACCESS_VIOLATION:      Name = TEXT("EXCEPTION_ACCESS_VIOLATION [segmentation fault]"); break;
		case EXCEPTION_ILLEGAL_INSTRUCTION:   Name = TEXT("EXCEPTION_ILLEGAL_INSTRUCTION"); break;
		case EXCEPTION_INT_DIVIDE_BY_ZERO:    Name = TEXT("EXCEPTION_INT_DIVIDE_BY_ZERO"); break;
		case EXCEPTION_FLT_DIVIDE_BY_ZERO:    Name = TEXT("EXCEPTION_FLT_DIVIDE_BY_ZERO"); break;
		case EXCEPTION_STACK_OVERFLOW:        Name = TEXT("EXCEPTION_STACK_OVERFLOW"); break;
		case EXCEPTION_PRIV_INSTRUCTION:      Name = TEXT("EXCEPTION_PRIV_INSTRUCTION"); break;
		case EXCEPTION_IN_PAGE_ERROR:         Name = TEXT("EXCEPTION_IN_PAGE_ERROR"); break;
		default:                              Name = TEXT("EXCEPTION (see code)"); break;
	}

	const BYTE* Base = (const BYTE*)GetModuleHandleA( NULL );

	debugf( NAME_Critical, TEXT("=== CRASH (unhandled exception) ===") );
	debugf( NAME_Critical, TEXT("%s  code=0x%08x"), Name, (DWORD)Code );
	// %016llx: on Win64 the PE's preferred base is 0x140000000, which %08x would truncate.
	debugf( NAME_Critical, TEXT("Image base: 0x%016llx   (subtract this from the addresses below)"),
	        (QWORD)(size_t)Base );

	if( Info && Info->ExceptionRecord )
	{
		const BYTE* At = (const BYTE*)Info->ExceptionRecord->ExceptionAddress;
		debugf( NAME_Critical, TEXT("Faulting address: 0x%016llx   rva=0x%08x"),
		        (QWORD)(size_t)At, (DWORD)(At - Base) );
		// An access violation records what it was doing and to which address -- usually the whole story
		// (a read of 0x00000000 is a null deref, and the offset says which field).
		if( Code == EXCEPTION_ACCESS_VIOLATION && Info->ExceptionRecord->NumberParameters >= 2 )
		{
			const ULONG_PTR Op = Info->ExceptionRecord->ExceptionInformation[0];
			debugf( NAME_Critical, TEXT("Tried to %s address 0x%08x"),
				Op == 0 ? TEXT("READ") : Op == 1 ? TEXT("WRITE") : TEXT("EXECUTE"),
				(DWORD)Info->ExceptionRecord->ExceptionInformation[1] );
		}
	}

	// Flush the header before any call-path reconstruction: if the trace itself faults, the header has
	// still been written. The output is ordered by how reliably it can be produced.
	if( GLog )
		GLog->Flush();

	// A raw scan of the stack for values that land inside this image. With -O2 there are no frame
	// pointers on i686, so these ARE the return addresses, mixed with data -- noisy, but a noisy call
	// path beats no call path, and addr2line discards the ones that do not resolve to a function.
	// Cheapest and safest of the two techniques, so it goes first.
	if( Info && Info->ContextRecord )
	{
		#ifdef _WIN64
			const BYTE** Sp = (const BYTE**)Info->ContextRecord->Rsp;
		#else
			const BYTE** Sp = (const BYTE**)Info->ContextRecord->Esp;
		#endif
		// Image span straight out of the PE headers -- GetModuleInformation would drag in psapi for one
		// field, and the headers are already mapped at Base.
		SIZE_T Span = 0;
		{
			const IMAGE_DOS_HEADER* Dos = (const IMAGE_DOS_HEADER*)Base;
			if( Dos->e_magic == IMAGE_DOS_SIGNATURE )
			{
				const IMAGE_NT_HEADERS* Nt = (const IMAGE_NT_HEADERS*)(Base + Dos->e_lfanew);
				if( Nt->Signature == IMAGE_NT_SIGNATURE )
					Span = Nt->OptionalHeader.SizeOfImage;
			}
		}
		if( Span && Sp )
		{
			debugf( NAME_Critical, TEXT("Stack scan (candidate return addresses):") );
			INT Printed = 0;
			for( INT i = 0; i < 2048 && Printed < 48; i++ )
			{
				// IsBadReadPtr is deprecated but this is a crash path; a fault here would be worse.
				if( IsBadReadPtr( Sp + i, sizeof(void*) ) )
					break;
				const BYTE* V = Sp[i];
				if( V > Base && (SIZE_T)(V - Base) < Span )
				{
					debugf( NAME_Critical, TEXT("  0x%08x  rva=0x%08x"), (DWORD)(size_t)V, (DWORD)(V - Base) );
					Printed++;
				}
			}
		}
	}

	if( GLog )
		GLog->Flush();

	// The frame-pointer walk comes first and RtlCaptureStackBackTrace last: on Windows
	// RtlCaptureStackBackTrace unwinds from inside the handler and returns only the handler's own frames,
	// and under wine it faults, which costs everything printed after it. The walk needs no API and, with
	// -fno-omit-frame-pointer, is the accurate one.
	if( Info && Info->ContextRecord )
	{
		#ifndef _WIN64
		const DWORD** Fp = (const DWORD**)Info->ContextRecord->Ebp;
		for( INT i = 0; i < 48 && Fp && !IsBadReadPtr( Fp, 2*sizeof(void*) ); i++ )
		{
			const BYTE* Ret = (const BYTE*)Fp[1];
			if( !Ret )
				break;
			debugf( NAME_Critical, TEXT("  ebp[%02d] 0x%08x  rva=0x%08x"), i,
				(DWORD)(size_t)Ret, (DWORD)(Ret - Base) );
			const DWORD** Next = (const DWORD**)Fp[0];
			if( Next <= Fp )		// the chain must climb, or it is not a chain
				break;
			Fp = Next;
		}
		#endif
	}

	if( GLog )
		GLog->Flush();

	{
		void* Frames[62];
		USHORT Count = RtlCaptureStackBackTrace( 0, 62, Frames, NULL );
		for( USHORT i = 0; i < Count; i++ )
			debugf( NAME_Critical, TEXT("  frame[%02d] 0x%08x  rva=0x%08x"), (INT)i,
				(DWORD)(size_t)Frames[i], (DWORD)((const BYTE*)Frames[i] - Base) );
	}

	// appInit seeds GErrorHist with "General protection fault!" and the guard chain appends to it, so a
	// non-empty buffer does not mean an error just happened: it is whatever the last unwind left there.
	if( GErrorHist[0] )
		debugf( NAME_Critical, TEXT("Guard history (may be stale, not necessarily this crash): %s"), GErrorHist );

	// The image base, not a literal: 0x400000 is the 32-bit PE default and is wrong on x86-64.
	debugf( NAME_Critical,
	        TEXT("Symbolise with: addr2line -f -C -e ut2004.exe <rva + 0x%016llx>"),
	        (QWORD)(size_t)GetModuleHandleA( NULL ) );
	debugf( NAME_Critical, TEXT("=== END CRASH ===") );

	if( GLog )
		GLog->Flush();

	return EXCEPTION_EXECUTE_HANDLER;
}

// An uncaught C++ exception. The engine's own errors travel this way (appThrowf), so when one escapes
// every catch the log must say so rather than just stopping.
static void appWinTerminate()
{
	GIsCriticalError = 1;
	debugf( NAME_Critical, TEXT("=== CRASH (std::terminate -- an exception escaped every handler) ===") );
	if( GErrorHist[0] )
		debugf( NAME_Critical, TEXT("Last engine error: %s"), GErrorHist );
	if( GLog )
		GLog->Flush();
	abort();	// -> appWinAbort, which prints the call path
}

static void appWinAbort( int )
{
	static UBOOL Reentered = 0;
	if( !Reentered )
	{
		Reentered = 1;
		GIsCriticalError = 1;
		debugf( NAME_Critical, TEXT("=== CRASH (abort) ===") );
		debugf( NAME_Critical, TEXT("Image base: 0x%016llx"), (QWORD)(size_t)GetModuleHandleA( NULL ) );
		if( GErrorHist[0] )
			debugf( NAME_Critical, TEXT("Last engine error: %s"), GErrorHist );
		{
			const BYTE* Base = (const BYTE*)GetModuleHandleA( NULL );
			void* Frames[62];
			USHORT Count = RtlCaptureStackBackTrace( 0, 62, Frames, NULL );
			for( USHORT i = 0; i < Count; i++ )
				debugf( NAME_Critical, TEXT("  frame[%02d] 0x%08x  rva=0x%08x"), (INT)i,
					(DWORD)(size_t)Frames[i], (DWORD)((const BYTE*)Frames[i] - Base) );
		}
		if( GLog )
			GLog->Flush();
	}
	_exit( 3 );
}

// The console's Ctrl-C, Ctrl-Break and close request a clean exit, as SIGTERM does on the other targets. The
// handler runs on a thread of its own; a further event is reported and ignored.
static BOOL WINAPI appWinConsoleCtrl( DWORD CtrlType )
{
	static volatile LONG Events = 0;
	const char* Name;
	if( CtrlType == CTRL_C_EVENT )
		Name = "Signal: Ctrl-C";
	else if( CtrlType == CTRL_BREAK_EVENT )
		Name = "Signal: Ctrl-Break";
	else if( CtrlType == CTRL_CLOSE_EVENT )
		Name = "Signal: console closed";
	else
		return FALSE;
	if( InterlockedIncrement( &Events ) == 1 )
	{
		printf( "%s\nRequesting Exit.\n", Name );
		GIsRequestingExit = 1;
	}
	else printf( "%s\nSignal: exit already requested; this one is ignored.\n", Name );
	fflush( stdout );
	return TRUE;
}
#endif // _WIN32

void appPlatformInit()
{
	guard(appPlatformInit);

	FLOAT CpuSpeed = 0.0f;

	//printf("GRunningOS is (%d). ENGINE_VERSION is (%d).\n", (int) GRunningOS, (int) ENGINE_VERSION);

	// System initialization.
	GSys = new USystem;
	GSys->AddToRoot();
	for( INT i=0; i<GSys->Suppress.Num(); i++ )
		GSys->Suppress(i).SetFlags( RF_Suppress );

	// Randomize.
	srand( (unsigned)time( NULL ) );

	// Exit code handling
	ExitCodeMap = new TMap<pid_t,int>;

	debugf( NAME_Init, TEXT("OS Type=%s"), GMachineOS );

	guard(UnixDetectCPU);

#if ASMLINUX
	// Check processor version with CPUID.
	try
	{
		TCHAR Brand[13], *Model, FeatStr[256]=TEXT("");
		DWORD A=0, B=0, C=0, D=0;
		DoCPUID(0,&A,&B,&C,&D);
		Brand[ 0] = (ANSICHAR)(B);
		Brand[ 1] = (ANSICHAR)(B>>8);
		Brand[ 2] = (ANSICHAR)(B>>16);
		Brand[ 3] = (ANSICHAR)(B>>24);
		Brand[ 4] = (ANSICHAR)(D);
		Brand[ 5] = (ANSICHAR)(D>>8);
		Brand[ 6] = (ANSICHAR)(D>>16);
		Brand[ 7] = (ANSICHAR)(D>>24);
		Brand[ 8] = (ANSICHAR)(C);
		Brand[ 9] = (ANSICHAR)(C>>8);
		Brand[10] = (ANSICHAR)(C>>16);
		Brand[11] = (ANSICHAR)(C>>24);
		Brand[12] = (ANSICHAR)(0);
		DoCPUID( 1, &A, &B, &C, &D );
		switch( (A>>8) & 0x000f )
		{
			case 4:  Model=TEXT("486-class processor");        break;
			case 5:  Model=TEXT("Pentium-class processor");    break;
			case 6:  Model=TEXT("PentiumPro-class processor"); break;
			case 7:  Model=TEXT("P7-class processor");         break;
			default: Model=TEXT("Unknown processor");          break;
		}
		if( (D & 0x00008000) ) {appStrcat( FeatStr, TEXT(" CMov") );}
		if( (D & 0x00000001) ) {appStrcat( FeatStr, TEXT(" FPU") );}
		if( (D & 0x00000010) ) {appStrcat( FeatStr, TEXT(" RDTSC") );}
		if( (D & 0x00000040) ) {appStrcat( FeatStr, TEXT(" PAE") );}

        #ifdef __x86_64__  // Hammer always has these instruction sets.
		appStrcat( FeatStr, TEXT(" MMX") );
		appStrcat( FeatStr, TEXT(" SSE") );
        #else
		if( (D & 0x00800000) && !ParseParam(appCmdLine(),TEXT("NOMMX")) ) {appStrcat( FeatStr, TEXT(" MMX") ); GIsMMX=1;}
		if( (D & 0x02000000) && !ParseParam(appCmdLine(),TEXT("NOSSE")) ) {appStrcat( FeatStr, TEXT(" SSE") ); GIsSSE=1;}
        #endif

		// Print features.
		debugf( NAME_Init, TEXT("CPU Detected: %s (%s)"), Model, Brand );
		debugf( NAME_Init, TEXT("CPU Features:%s"), FeatStr );

		appSprintf( GMachineCPU, TEXT("%s %s"), Brand, Model ); // gam

		#ifdef __x86_64__
		GRunningCPU = CPU_AMD64;
		#else
		if( appStricmp( TEXT("GenuineIntel"), Brand ) == 0 )
			GRunningCPU = CPU_INTEL;
		else
		if( appStricmp( TEXT("AuthenticAMD"), Brand ) == 0 )
			GRunningCPU = CPU_AMD;
		#endif
	}
	catch( ... )
	{
		debugf( NAME_Init, TEXT("Couldn't detect CPU: Probably 486 or non-Intel processor") );
		appSprintf( GMachineCPU, TEXT("Unknown CPU") ); // gam
	}

#elif MACOSX
	long cputype = 0;
	const TCHAR *cpuname = TEXT("Unknown CPU");

	OSErr err = Gestalt(gestaltNativeCPUType, &cputype);
	if (err)
	{
		debugf(NAME_Init, TEXT("gestaltNativeCPUType failure: err=%d"), (int) err);
	}
	else
	{
		typedef struct
		{
			int gestaltID;
			const TCHAR *namestr;
		} PowerPC_CPU_Info;

		static PowerPC_CPU_Info ppcCPUs[] =
		{
			// shouldn't ever be a 680x0 chip, but it's here for completeness.
			{ gestalt68000,     TEXT("68000")                   },
			{ gestalt68010,     TEXT("68010")                   },
			{ gestalt68020,     TEXT("68020")                   },
			{ gestalt68030,     TEXT("68030")                   },
			{ gestalt68040,     TEXT("68040")                   },
			{ gestaltCPU601,    TEXT("PowerPC 601")             },
			{ gestaltCPU603,    TEXT("PowerPC 603")             },
			{ gestaltCPU604,    TEXT("PowerPC 604")             },
			{ gestaltCPU603e,   TEXT("PowerPC 603e")            },
			{ gestaltCPU603ev,  TEXT("PowerPC 603ev")           },
			{ gestaltCPU750,    TEXT("PowerPC 740/750/G3")      },
			{ gestaltCPU604e,   TEXT("PowerPC 604e")            },
			{ gestaltCPU604ev,  TEXT("PowerPC 604ev")           },
			{ gestaltCPUG4,     TEXT("PowerPC G4")              },
			{ gestaltCPUG47450, TEXT("PowerPC G4/Vger/Altivec") },
			{ 0, NULL }
		};

		for (int i = 0; ppcCPUs[i].namestr != NULL; i++)
		{
			if (ppcCPUs[i].gestaltID == cputype)
			{
				cpuname = ppcCPUs[i].namestr;
				break;
			}
		}
	}

	appStrcpy( GMachineCPU, cpuname );
	debugf( NAME_Init, TEXT("CPU Type=%s"), GMachineCPU );

	GIsAltivec = 0;

    long cpufeature = 0;
    err = Gestalt(gestaltPowerPCProcessorFeatures, &cpufeature);
    if (err != noErr)
    {
		debugf(NAME_Init, TEXT("gestaltPowerPCProcessorFeatures failure: err=%d"), (int) err);
    }
    else if ((1 << gestaltPowerPCHasVectorInstructions) & cpufeature)
    {
	    GIsAltivec = 1;
    }

    debugf( NAME_Init, TEXT("This CPU %s Altivec support."),
            GIsAltivec ? TEXT("has") : TEXT("does not have") );

    GPhysicalMemory = 0;

    unsigned long ramsize = 0;
    err = Gestalt(gestaltPhysicalRAMSize, (long *) &ramsize);
    if (err != noErr)
    {
		debugf(NAME_Init, TEXT("gestaltPhysicalRAMSize failure: err=%d"), (int) err);
    }
    else
    {
        GPhysicalMemory = (DWORD) ramsize;
		debugf( NAME_Init, TEXT("Physical RAM: %u bytes (%d megabytes)"),
		        (unsigned int) ramsize, (int) ((ramsize / 1024.0f) / 1024.0f) );
    }

	GRunningCPU = CPU_PPC;

#else
	#warning Need CPU detection for this platform.
	appStrcpy( GMachineCPU, TEXT("Unknown CPU (no detection code!)") );
	debugf( NAME_Init, TEXT("No CPU type detection on this platform.") );

#endif

	unguard;


	// CPU speed.
	guard(CheckCpuSpeed);

#if ((ASMLINUX) || (MACOSX)) && !defined(UE_NO_X86_INTRIN)
	try
	{
		GSecondsPerCycle = 1.f;

		for( INT i=0; i<3; i++ )
		{
			LARGE_INTEGER	StartCycles, EndCycles;
			volatile DOUBLE	DoubleDummy = 0.0;
			volatile INT	IntDummy	= 0;

			#if  MACOSX
			__asm__ __volatile__( "mftb %0" : "=r" (StartCycles.LowPart) );
			__asm__ __volatile__( "mftbu %0" : "=r" (StartCycles.HighPart) );
			#else
			__asm__ __volatile__
			(
				"rdtsc"
				: "=a" (StartCycles.LowPart), "=d" (StartCycles.HighPart)
			);
			#endif

			struct timeval	StartMsec;
			gettimeofday(&StartMsec, NULL);
			struct timeval EndMsec		= StartMsec;

			int diff = (((EndMsec.tv_sec - StartMsec.tv_sec) * 1000) +
			           ((EndMsec.tv_usec - StartMsec.tv_usec) / 1000));
			while( diff < 200 )
			{
				DoubleDummy += appSqrt(DoubleDummy) + 3.14;
				IntDummy	*= ((INT) (DoubleDummy) + 555) / 13;
				gettimeofday(&EndMsec, NULL);
				diff = (((EndMsec.tv_sec - StartMsec.tv_sec) * 1000) +
				       ((EndMsec.tv_usec - StartMsec.tv_usec) / 1000));
			}

			#if  MACOSX
			__asm__ __volatile__( "mftb %0" : "=r" (EndCycles.LowPart) );
			__asm__ __volatile__( "mftbu %0" : "=r" (EndCycles.HighPart) );
			#else
			__asm__ __volatile__
			(
				"rdtsc"
				: "=a" (EndCycles.LowPart), "=d" (EndCycles.HighPart)
			);
			#endif

			DOUBLE	C1	= (DOUBLE)(SQWORD)(((QWORD)StartCycles.LowPart) + ((QWORD)StartCycles.HighPart<<32));
			DOUBLE  C2	= (DOUBLE)(SQWORD)(((QWORD)EndCycles.LowPart) + ((QWORD)EndCycles.HighPart<<32));

			GSecondsPerCycle = Min( GSecondsPerCycle, 1.0 / (1000.f * ( C2 - C1 ) / (diff)) );
		}

		DOUBLE mhz = 0.0;
		#if MACOSX
			mhz = (DOUBLE) GetCPUSpeed();  // (Carbon function.)
		#else
			mhz = ((DOUBLE) 0.000001) / ((DOUBLE) GSecondsPerCycle);
		#endif

		debugf( NAME_Init, TEXT("CPU Speed=%f MHz"), (float) mhz );
		appStrcat( GMachineCPU, *FString::Printf(TEXT(" @ %d MHz"), appRound(mhz) ) ); // gam
	}
	catch( ... )
	{
		debugf( NAME_Init, TEXT("Timestamp not supported (Possibly 486 or Cyrix processor)") );
		appStrcat( GMachineCPU, TEXT(" Unknown clock") ); // gam
		GSecondsPerCycle = 1;
	}

#else
	if( !Parse(appCmdLine(),TEXT("CPUSPEED="),CpuSpeed) )
	{
		#warning need CPU speed detection code.
		appErrorf( TEXT("Please specify CPUSPEED=x on cmdline.") );
	}
#endif

	if( Parse(appCmdLine(),TEXT("CPUSPEED="),CpuSpeed) )
	{
		GSecondsPerCycle = 0.000001/CpuSpeed;
		debugf( NAME_Init, TEXT("CPU Speed Overridden=%f MHz"), 0.000001 / GSecondsPerCycle );
		appStrcat( GMachineCPU, *FString::Printf(TEXT(" @ %d MHz"), appRound(0.000001 / GSecondsPerCycle) ) ); // gam
	}

#ifdef UE_NO_X86_INTRIN
	// appCycles() counts microseconds on this path, so the conversion factor is fixed and the CPUSPEED=
	// override above must not scale it. CPUSPEED= is still needed: it keeps the `#else` branch above from
	// appErrorf'ing (there is no CPU-speed detection for wasm or arm) and it is what GMachineCPU reports.
	// Nothing times gameplay with GSecondsPerCycle: the game loop runs on appSecondsSlow() (the rdtsc
	// appSeconds() in UnGnuG.h is `#if 0`'d); its readers here are stats, debugf lines and the -benchmark
	// frame log.
	GSecondsPerCycle = 0.000001;
	debugf( NAME_Init, TEXT("appCycles() is a microsecond counter; GSecondsPerCycle pinned to %e"), GSecondsPerCycle );
#endif

	unguard;

#ifdef _WIN32
	// The POSIX process plumbing is skipped here:
	//   * setrlimit(RLIMIT_NOFILE): Windows has no per-process descriptor limit to raise; the CRT's
	//     _setmaxstdio ceiling applies only to stdio streams, which the engine does not use for packages.
	//   * sigaction() crash handlers: the Win32 equivalent is installed instead, so a hardware fault is
	//     reported (the guard/unguard chain reports only thrown errors).
	SetUnhandledExceptionFilter( appWinUnhandledException );
	// SetUnhandledExceptionFilter covers hardware faults only. An uncaught C++ throw (std::terminate) and
	// abort() bypass it, and the engine throws for a living (appThrowf is how a failed load unwinds).
	std::set_terminate( appWinTerminate );
	signal( SIGABRT, appWinAbort );
	SetConsoleCtrlHandler( appWinConsoleCtrl, TRUE );
#endif
#ifndef _WIN32
	struct sigaction SigAction;

	// UT_NO_FAULT_HANDLER leaves the hardware faults to the OS. On Android this handler catches SIGSEGV,
	// logs a line and exits, so `debuggerd` never runs and there is no tombstone and no backtrace. With the
	// variable set, the six hardware-fault signals keep their default disposition and the platform's crash
	// reporter produces a real backtrace (`logcat -s DEBUG`, then llvm-addr2line against the unstripped
	// .so). The job-control signals below are installed either way: they are how the engine shuts down
	// cleanly. A diagnostic, off by default. An Android app cannot be given an environment variable, so
	// there the system property does the same: `adb shell setprop debug.ut2004.nofault 1` (settable on a
	// stock user build without root).
#if defined(__ANDROID__)
	char NoFaultProp[128] = { 0 };   /* bionic's PROP_VALUE_MAX is 92 */
	__system_property_get( "debug.ut2004.nofault", NoFaultProp );
	const UBOOL bNoFaultHandler = (NoFaultProp[0] != 0 && NoFaultProp[0] != '0');
#else
	const UBOOL bNoFaultHandler = (getenv("UT_NO_FAULT_HANDLER") != NULL);
#endif
	if( bNoFaultHandler )
		debugf( NAME_Init, TEXT("UT_NO_FAULT_HANDLER set: leaving SIGSEGV/SIGILL/SIGTRAP/SIGIOT/SIGBUS/SIGFPE to the OS.") );

#if !defined(__EMSCRIPTEN__)
	// A signal taken on the main thread runs its handler on a stack of its own (SA_ONSTACK below): Pixomatic's
	// span code uses ESP as a general register, and on that ESP the kernel cannot build the signal frame.
	static BYTE SignalStack[1024 * 1024];
	stack_t SigStack;
	SigStack.ss_sp		= SignalStack;
	SigStack.ss_size	= sizeof(SignalStack);
	SigStack.ss_flags	= 0;
	if( sigaltstack( &SigStack, NULL ) != 0 )
		debugf( NAME_Init, TEXT("sigaltstack failed (errno %i): signal handlers run on the interrupted stack"), errno );
#endif

	// Only try once.
	INT DefaultFlag			= SA_RESETHAND | SA_ONSTACK;

	if( !bNoFaultHandler )
	{
	SigAction.sa_handler	= HandleSignal;
	SigAction.sa_flags		= DefaultFlag;
	sigemptyset( &SigAction.sa_mask );
	sigaction( SIGSEGV, &SigAction, 0 );

	SigAction.sa_handler	= HandleSignal;
	SigAction.sa_flags		= DefaultFlag;
	sigemptyset( &SigAction.sa_mask );
	sigaction( SIGILL, &SigAction, 0 );
	}

	// Try multiple times.
	DefaultFlag				= SA_ONSTACK;

	SigAction.sa_handler	= HandleChild;
	SigAction.sa_flags		= SA_ONSTACK;
	sigemptyset( &SigAction.sa_mask );
	sigaction(SIGCHLD, &SigAction, 0);

	SigAction.sa_handler	= HandleSignal;
	SigAction.sa_flags		= DefaultFlag;
	sigemptyset( &SigAction.sa_mask );
	sigaction( SIGHUP, &SigAction, 0 );

	SigAction.sa_handler	= HandleSignal;
	SigAction.sa_flags		= DefaultFlag;
	sigemptyset( &SigAction.sa_mask );
	sigaction( SIGQUIT, &SigAction, 0 );

	if( !bNoFaultHandler )
	{
	SigAction.sa_handler	= HandleSignal;
	SigAction.sa_flags		= DefaultFlag;
	sigemptyset( &SigAction.sa_mask );
	sigaction( SIGTRAP, &SigAction, 0 );
	}

	if( !bNoFaultHandler )
	{
	SigAction.sa_handler	= HandleSignal;
	SigAction.sa_flags		= DefaultFlag;
	sigemptyset( &SigAction.sa_mask );
	sigaction( SIGIOT, &SigAction, 0 );
	}

	if( !bNoFaultHandler )
	{
	SigAction.sa_handler	= HandleSignal;
	SigAction.sa_flags		= DefaultFlag;
	sigemptyset( &SigAction.sa_mask );
	sigaction( SIGBUS, &SigAction, 0 );
	}

	if( !bNoFaultHandler )
	{
	SigAction.sa_handler	= HandleSignal;
	SigAction.sa_flags		= DefaultFlag;
	sigemptyset( &SigAction.sa_mask );
	sigaction( SIGFPE, &SigAction, 0 );
	}

	SigAction.sa_handler	= HandleSignal;
	SigAction.sa_flags		= DefaultFlag;
	sigemptyset( &SigAction.sa_mask );
	sigaction( SIGTERM, &SigAction, 0 );

	SigAction.sa_handler	= HandleInterrupt;
	SigAction.sa_flags		= DefaultFlag;
	sigemptyset( &SigAction.sa_mask );
	sigaction( SIGINT, &SigAction, 0 );

	signal( SIGPIPE, SIG_IGN );

    // attempt to change max file handles for this process...
    struct rlimit rl;
    if (getrlimit(RLIMIT_NOFILE, &rl) != -1)
        debugf(NAME_Init, TEXT("File handle limit is soft=(%d), hard=(%d)."), (int) rl.rlim_cur, (int) rl.rlim_max);
    else
    {
        rl.rlim_cur = DESIRED_FILE_HANDLES; // wing it.
        rl.rlim_max = RLIM_INFINITY;
    }

    if ( ((rl.rlim_cur != RLIM_INFINITY) && (rl.rlim_cur < DESIRED_FILE_HANDLES)) ||
         ((rl.rlim_max != RLIM_INFINITY) && (rl.rlim_max < DESIRED_FILE_HANDLES)) )
    {
        bool changed = false;

        if ((rl.rlim_cur != RLIM_INFINITY) && (rl.rlim_cur < DESIRED_FILE_HANDLES))
            rl.rlim_cur = DESIRED_FILE_HANDLES;
        if ((rl.rlim_max != RLIM_INFINITY) && (rl.rlim_max < DESIRED_FILE_HANDLES))
            rl.rlim_max = RLIM_INFINITY;

        changed = (setrlimit(RLIMIT_NOFILE, &rl) != -1);
        if (!changed)
        {
            debugf(NAME_Init, TEXT("failed to set open file limit to %d, %d."), (int) rl.rlim_cur, (int) rl.rlim_max);
            rl.rlim_max = DESIRED_FILE_HANDLES;
            changed = (setrlimit(RLIMIT_NOFILE, &rl) != -1);
            if (!changed)
                debugf(NAME_Init, TEXT("failed to set open file limit to %d, %d."), (int) rl.rlim_cur, (int) rl.rlim_max);
        }

        if (changed)
        {
            getrlimit(RLIMIT_NOFILE, &rl);
            debugf(NAME_Init, TEXT("Changed file handle limit to soft=(%d), hard=(%d)."), (int) rl.rlim_cur, (int) rl.rlim_max);
        }
    }
#endif // !_WIN32

	unguard;
}

void appPlatformPreExit()
{
}


static void ShowAlertDialogOnExit(void)
{
    guard(ShowAlertDialogOnExit);

#if MACOSX
    if (GMacOSAlertBoxOnExit == NULL)
        return;

    bool failed = true;

    CFStringRef msg = CFStringCreateWithBytes(NULL,
                              (const BYTE *) GMacOSAlertBoxOnExit,
                              appStrlen(GMacOSAlertBoxOnExit) * sizeof (TCHAR),
                              TCHAR_MAC_ENCODING, 0);

    FString str = LocalizeGeneral("Product", appPackage());
    CFStringRef title = CFStringCreateWithBytes(NULL, (const BYTE *) *str,
                                                str.Len() * sizeof (TCHAR),
                                                TCHAR_MAC_ENCODING, 0);
    if ((msg != NULL) && (title != NULL))
    {
        SWORD val = 0;  // Should be "DialogItemIndex".
        void *dlg = NULL;  // Should be "DialogRef".

        if (CreateStandardAlert(kAlertStopAlert, title, msg, NULL, &dlg) == noErr)
        {
            if (RunStandardAlert(dlg, NULL, &val) == noErr)
                failed = false;
        }
    }

    if (failed)  // oh well.
        printf("\n\nMacOS Alert Box: %s\n\n\n", appToAnsi(GMacOSAlertBoxOnExit));

    if (msg != NULL)
        CFRelease(msg);

    if (title != NULL)
        CFRelease(title);

    delete[] GMacOSAlertBoxOnExit;
    GMacOSAlertBoxOnExit = NULL;
#endif

    unguard;
}


static void LaunchURLOnExit(void)
{
    guard(LaunchURLOnExit);

#if MACOSX
    if (GMacOSLaunchURLOnExit == NULL)
        return;

    CFURLRef cfurl = CFURLCreateWithBytes(NULL,
                            (const unsigned char *) GMacOSLaunchURLOnExit,
                            appStrlen(GMacOSLaunchURLOnExit) * sizeof (TCHAR),
                            TCHAR_MAC_ENCODING, NULL);

    LSOpenCFURLRef(cfurl, NULL);
    CFRelease(cfurl);
    delete[] GMacOSLaunchURLOnExit;
    GMacOSLaunchURLOnExit = NULL;
#endif

    unguard;
}


#define DEBUG_HACKY_CMDLINE_PARSER 0
void appPlatformExit()
{
    ShowAlertDialogOnExit();
    LaunchURLOnExit();

    if (GUnixSpawnOnExit != NULL)
    {
        FString fstrcmd(GUnixSpawnOnExit);
        TArray<FString> args;

        int startstr = 0;
        int max = fstrcmd.Len();

        #if DEBUG_HACKY_CMDLINE_PARSER
            fprintf(stderr, "\nOriginal cmdline:\n[%s].\n\n", appToAnsi(*fstrcmd));
        #endif

        for (int i = 0; i < max; i++)
        {
            char ch = (char) fstrcmd[i];
            if (ch == ' ')
            {
                if ( (i == 0) ||
                     (fstrcmd[i-1] == ' ') ||
                     (fstrcmd[i-1] == '\"') ||
                     (fstrcmd[i-1] == '\'') )
                {
                    startstr = i + 1;
                    continue;
                }

    		    new(args)FString( fstrcmd.Mid(startstr, i - startstr) );
                startstr = i + 1;
            }

            else if ((ch == '\"') || (ch == '\''))
            {
                if (startstr != i)
    		        new(args)FString( fstrcmd.Mid(startstr, i - startstr) );

                int j = i + 1;
                while ((j < max) && ((char) fstrcmd[j] != ch))
                    j++;

                startstr = j + 1;

                if (j == max)  // uh...oh well.
                {
                    if (j > i + 1)
			            new(args)FString( fstrcmd.Mid(i+1) );
                    break;
                }

                if (j > i + 1)
                {
			        new(args)FString( fstrcmd.Mid(i+1, (j - i) - 1) );
                    i = j;  // will increment past delimiter in next loop.
                }
            }
        }

        if (startstr < max)
    	    new(args)FString( fstrcmd.Mid(startstr) );


        // appToAnsi returns the converted string (its own text in the ANSI builds, which leave a buffer argument
        // untouched), so each is copied out.
        char *cmd = new char[args(0).Len() + 1];
        strcpy(cmd, appToAnsi(*(args(0))));
        char **ansiargs = new char *[args.Num() + 1];
        for (int i = 0; i < args.Num(); i++)
        {
            ansiargs[i] = new char[args(i).Len() + 1];
            strcpy(ansiargs[i], appToAnsi(*args(i)));
        }
        ansiargs[args.Num()] = NULL;

#ifdef _WIN32
        // The "respawn the game after exit" path (GUnixSpawnOnExit): fork-then-execv is one _spawnv, and the
        // parent then exits.
        _spawnv( _P_NOWAIT, cmd, ansiargs );
        _exit(0);
#else
        int rc = fork();
        if (rc == -1)
        {
            fprintf(stderr, "fork() failed!\n errno (%d) [%s].\n",
	            errno, strerror(errno));
            _exit(42);
        }
        else if (rc > 0)  // parent: die.
        {
            _exit(0);
        }
#endif

        #if DEBUG_HACKY_CMDLINE_PARSER
            for (int x = 0; ansiargs[x]; x++)
                fprintf(stderr, "[%s]\n", ansiargs[x]);
            fprintf(stderr, "\n\n");
            fflush(stderr);
        #endif

        appSleep(1);
        execv(cmd, ansiargs);  // hail mary.

        // if you hit this...there were problems.
        fprintf(stderr, "execv() failed.\nerrno (%d) [%s].\n Command line:\n  ",
                errno, strerror(errno));

        for (int x = 0; ansiargs[x]; x++)
            fprintf(stderr, "%s ", ansiargs[x]);
        fprintf(stderr, "\n\n");

        fflush(stderr);
        _exit(42);
    }
}

/*-----------------------------------------------------------------------------
	Pathnames.
-----------------------------------------------------------------------------*/

#if __FILE_NOCASE
// See if file specified by (Path) exists with a different case. This
//  assumes that the directories specified exist with the given case, and
//  only looks at the file part of the path. The first file found via
//  readdir() is the one picked, which may not be the first in ASCII order.
//  If a file is found, (Path) is overwritten to reflect the new case, and
//  (true) is returned. If no file is found, (Path) is untouched, and (false)
//  is returned.
static bool find_alternate_filecase(char *Path)
{
    guard(find_alternate_filecase);

    bool retval = false;
    char *ptr = strrchr(Path, '/');
    char *filename = (ptr != NULL) ? ptr + 1 : Path;
    char *basedir = (ptr != NULL) ? Path : ((char *) ".");

    if (ptr != NULL)
        *ptr = '\0';  // separate dir and filename.

    // fast rejection: only check this if there's no wildcard in filename.
    if (strchr(filename, '*') == NULL)
    {
        DIR *dir = opendir(basedir);
        if (dir != NULL)
        {
            struct dirent *ent;
            while (((ent = readdir(dir)) != NULL) && (!retval))
            {
                if (strcasecmp(ent->d_name, filename) == 0)  // a match?
                {
                    strcpy(filename, ent->d_name);  // overwrite with new case.
                    retval = true;
                }
            }
            closedir(dir);
        }
    }

    if (ptr != NULL)
        *ptr = '/';  // recombine dir and filename into one path.

    return(retval);

    unguard;
}
#endif


// Convert pathname to Unix format.
char* appUnixPath( const char* Path )
{
	guard(appUnixPath);
	static char UnixPath[1024];
	char* Cur = UnixPath;
	strncpy( UnixPath, Path, 1024 );
    UnixPath[1023] = '\0';

	while( Cur = strchr( Cur, '\\' ) )
		*Cur = '/';

#if __FILE_NOCASE
    // fast rejection: only check this if the current path doesn't exist...
    if (access(UnixPath, F_OK) != 0)
    {
        Cur = UnixPath;
        if (*Cur == '/')
            Cur++;

        bool keep_looking = true;
        while (keep_looking)
        {
            Cur = strchr( Cur, '/' );
            if (Cur != NULL)
                *Cur = '\0';  // null-terminate so we have this chunk of path.

            // do insensitive check only if the current path doesn't exist...
            if (access(UnixPath, F_OK) != 0)
                keep_looking = find_alternate_filecase(UnixPath);

            if (Cur == NULL)
                keep_looking = false;
            else
            {
                *Cur = '/';   // reset string for next check.
                Cur++;
            }
        }
    }
#endif

	return UnixPath;
	unguard;
}


/*-----------------------------------------------------------------------------
	Networking.
-----------------------------------------------------------------------------*/

DWORD appGetLocalIP( void )
{
	static bool lookedUp = false;
	static DWORD LocalIP = 0;

	if( !lookedUp )
	{
		lookedUp = true;
		char Hostname[256];
		Hostname[0] = '\0';
		if (gethostname( Hostname, sizeof(Hostname) ) == 0)
		{
			check(gethostbyname_mutex != NULL);
			gethostbyname_mutex->Lock();
			struct hostent *Hostinfo = gethostbyname( Hostname );
			if (Hostinfo != NULL)
				LocalIP = *(DWORD*)Hostinfo->h_addr_list[0];
			gethostbyname_mutex->Unlock();
		}
	}

	return LocalIP;
}


/*-----------------------------------------------------------------------------
	String functions.
-----------------------------------------------------------------------------*/

int stricmp( const char* s, const char* t )
{
	int	i;
	for( i = 0; tolower(s[i]) == tolower(t[i]); i++ )
		if( s[i] == '\0' )
			return 0;
	return s[i] - t[i];
}

int strnicmp( const char* s, const char* t, int n )
{
	int	i;
	if( n <= 0 )
		return 0;
	for( i = 0; tolower(s[i]) == tolower(t[i]); i++ )
		if( (s[i] == '\0') || (i == n - 1) )
			return 0;
	return s[i] - t[i];
}

char* strupr( char* s )
{
	int	i;
	for( i = 0; s[i] != '\0'; i++ )
		s[i] = toupper(s[i]);
	return s;
}


/*-----------------------------------------------------------------------------
	Stuff.
-----------------------------------------------------------------------------*/

CORE_API const void EdLoadErrorf( INT Type, const TCHAR* Fmt, ... )
{ }


#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

