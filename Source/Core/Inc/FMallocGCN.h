/*=============================================================================
	FMallocGCN.h: GCN memory allocator.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

//
// GCN C memory allocator.
//
#include <dolphin/os.h>

class FMallocGCN : public FMalloc
{
public:
	// FMalloc interface.
	void* Malloc( DWORD Size, const TCHAR* Tag )
	{
		guard(FMallocGCN::Malloc);
		check(Size>0);
		void* Ptr = OSAlloc( OSRoundUp32B(Size) );
		check(Ptr);
		return Ptr;
		unguard;
	}
	void* Realloc( void* Ptr, DWORD NewSize, const TCHAR* Tag )
	{
		guard(FMallocGCN::Realloc);
		check(NewSize>=0);
		void* Result;
		if( Ptr && NewSize )
		{
#ifdef EMU
			Result = realloc(Ptr, NewSize);
#else
			Result = this->Malloc( NewSize, Tag );
			appMemcpy(Result, Ptr, NewSize);
			this->Free( Ptr );
#endif
		}
		else if( NewSize )
		{
			Result = this->Malloc( NewSize, Tag );
		}
		else
		{
			if( Ptr )
				this->Free( Ptr );
			Result = NULL;
		}
		return Result;
		unguardf(( TEXT("%08X %i %s"), (INT)Ptr, NewSize, Tag ));
	}
	void Free( void* Ptr )
	{
		guard(FMallocGCN::Free);
		if (Ptr)
			OSFree( Ptr );
		unguard;
	}
	void DumpAllocs()
	{
		guard(FMallocGCN::DumpAllocs);
		debugf( NAME_Exit, TEXT("Allocation checking disabled") );
		unguard;
	}
	void HeapCheck()
	{
		guard(FMallocGCN::HeapCheck);
#if (defined _MSC_VER) && (!defined _XBOX || defined _DEBUG) && (!defined __GCN__)
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
		guard(FMallocGCN::Init);
/*
		// SL TODO: Reinitialize properly!
		void* arenaLo = OSGetArenaLo();
		void* arenaHi = OSGetArenaHi();

		// OSInitAlloc should only ever be invoked once.
		arenaLo = OSInitAlloc(arenaLo, arenaHi, 1); // 1 heap
		OSSetArenaLo(arenaLo);

		// The boundaries given to OSCreateHeap should be 32B aligned
		TheHeap = OSCreateHeap((void*)OSRoundUp32B(arenaLo),
							   (void*)OSRoundDown32B(arenaHi));
		OSSetCurrentHeap(TheHeap);
		// From here on out, OSAlloc and OSFree behave like malloc and free
		// respectively

		OSSetArenaLo(arenaLo = arenaHi);
*/
		unguard;
	}
	void Exit()
	{
		guard(FMallocGCN::Exit);
#ifndef EMU
		OSDestroyHeap(TheHeap);
#endif
		unguard;
	}
protected:
	OSHeapHandle TheHeap;
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

