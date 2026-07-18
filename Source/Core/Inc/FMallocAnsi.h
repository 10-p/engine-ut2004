/*=============================================================================
	FMallocAnsi.h: ANSI memory allocator.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

// !!! FIXME: Some uscript classes have four bytes extra at the end of them
// !!! FIXME:  on the Hammer, so we need to pad out the memory allocations
// !!! FIXME:  (32 bytes to be safe)...fixing UnProp.cpp, UnClass.cpp or GCC
// !!! FIXME:  itself would be smarter, though.  --ryan.
#include "Core.h"
#if PLATFORM_64BITS
#define MALLOC_PADDING 32
#else
#define MALLOC_PADDING 0
#endif

//
// ANSI C memory allocator.
//
class FMallocAnsi : public FMalloc
{
public:
	// FMalloc interface.
	void* Malloc( size_t Size, const TCHAR* Tag )
	{
		guard(FMallocAnsi::Malloc);
		check(Size>=0);
		void* Ptr = malloc( Size + MALLOC_PADDING );
		check(Ptr);
		return Ptr;
		unguard;
	}
	void* Realloc( void* Ptr, size_t NewSize, const TCHAR* Tag )
	{
		guard(FMallocAnsi::Realloc);
		check(NewSize>=0);
		void* Result;
if ((PTRINT) Ptr < 0x1000) Ptr = NULL;
		if( Ptr && NewSize )
		{
			Result = realloc( Ptr, NewSize + MALLOC_PADDING );
			if(!Result) { fprintf(stderr, "REALLOC FAILED: Ptr=%p NewSize=%u\n", Ptr, (unsigned)NewSize); }
		}
		else if( NewSize )
		{
			Result = malloc( NewSize + MALLOC_PADDING );
		}
		else
		{
			if( Ptr )
				free( Ptr );
			Result = NULL;
		}
		return Result;
		unguardf(( TEXT("%08X %i %s"), (INT) ((PTRINT)Ptr), NewSize, Tag ));
	}
	void Free( void* Ptr )
	{
		guard(FMallocAnsi::Free);
		free( Ptr );
		unguard;
	}
	void DumpAllocs()
	{
		guard(FMallocAnsi::DumpAllocs);
		debugf( NAME_Exit, TEXT("Allocation checking disabled") );
		unguard;
	}
	void HeapCheck()
	{
		guard(FMallocAnsi::HeapCheck);
#if (defined _MSC_VER) && (!defined _XBOX || defined _DEBUG) && (!defined __GCN__) && DO_CHECK
		INT Result = _heapchk();
		check(Result!=_HEAPBADBEGIN);
		check(Result!=_HEAPBADNODE);
		check(Result!=_HEAPBADPTR);
		check(Result!=_HEAPEMPTY);
		check(Result==_HEAPOK);
#endif
		unguard;
	}
	void Init()
	{
		guard(FMallocAnsi::Init);
		unguard;
	}
	void Exit()
	{
		guard(FMallocAnsi::Exit);
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

