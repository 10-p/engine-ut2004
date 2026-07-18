/*=============================================================================
	FMallocPSX2.h: PlayStation 2 memory allocator.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

enum EMemoryPad
{
	PAD_Head=0x3377BBFF,
	PAD_Tail=0x2266AAEE,
	PAD_Free=0x115599DD,
	PAD_Used=0x004488CC,
};
#define IFPAD(x)
INT GMallocDebug;
INT MallocInitted = 0;
struct FMallocPSX2 : public FMalloc
{
	struct FAllocHeader
	{

		FAllocHeader *Prev, *Next, *FreeNext, **FreePrevLink;
		IFPAD(INT Pad; INT XXX; INT YYY; INT ZZZ;)
		INT GetSize() {return ((BYTE*)Next-(BYTE*)this-sizeof(FAllocHeader));}
	} *Head,*Tail,*FreeList[32];
	INT AllSize;
	void TryToConsumeNext( FAllocHeader* H )
	{
		if( H->FreePrevLink && H->Next->FreePrevLink )
		{
			RemoveFromFreeList(H->Next);
			RemoveFromFreeList(H);
			H->Next->Next->Prev  = H;
			H->Next              = H->Next->Next;
			AddToFreeList(H);
		}
	}
	INT SizeToFreeBin( INT Size )
	{
		return appCeilLogTwo(Size);
	}
	void AddToFreeList( FAllocHeader* H )
	{
		IFPAD(check(H->Pad==PAD_Used);)
		INT Bin                         = SizeToFreeBin(H->GetSize());
		H->FreeNext                     = FreeList[Bin];
		H->FreePrevLink                 = &FreeList[Bin];
		IFPAD(H->Pad                    = PAD_Free;)
		if( FreeList[Bin] )
			FreeList[Bin]->FreePrevLink = &H->FreeNext;
		FreeList[Bin]                   = H;
	}
	void RemoveFromFreeList( FAllocHeader* H )
	{
		IFPAD(check(H->Pad==PAD_Free);)
		*H->FreePrevLink                = H->FreeNext;
		if( H->FreeNext )
			H->FreeNext->FreePrevLink   = H->FreePrevLink;
		H->FreeNext				        = NULL;
		H->FreePrevLink                 = NULL;
		IFPAD(H->Pad                    = PAD_Used;)
	}
	void* Malloc( DWORD Size, const TCHAR* Tag )
	{
		return this->Realloc( NULL, Size, Tag);
	}
	void Free( void* Ptr )
	{
		this->Realloc( Ptr, 0, NULL );
	}
	void* Realloc( void* Ptr, DWORD Size, const TCHAR* Tag )
	{
if (GMallocDebug)
debugf("Reallocing %s to %d", Tag, Size);
		guard(FMallocPSX2::Realloc);
		check(Size>=0);
		Size=Align(Size,16);
		FAllocHeader* Original = Ptr ? (FAllocHeader*)Ptr-1 : NULL;
		FAllocHeader* Result   = NULL;
		if( Size && Original && Original->GetSize()==Size )
			return Original+1;
		if( Size )
		{
			INT Count=0;
			for( INT Bin=SizeToFreeBin(Size); Bin<ARRAY_COUNT(FreeList) && !Result; Bin++ )
				for( FAllocHeader* H=FreeList[Bin]; H; H=H->FreeNext )
					if( H->GetSize()>=Size )
					{
						if( !Result || (Result->GetSize()<Result->GetSize()) )
							Result=H;
						if( Count++ > 16 )
							break;
					}
			if( !Result )
				appErrorf(TEXT("Realloc failed: %i %s"),Size,Tag);
			if( Result->GetSize() > Size+sizeof(FAllocHeader) )
			{
				FAllocHeader* Free = (FAllocHeader*)((BYTE*)Result+sizeof(FAllocHeader)+Size);
				Free->Prev         = Result;
				Free->Next         = Result->Next;
				IFPAD(Free->Pad    = PAD_Used);
				AddToFreeList(Free);
				Result->Next->Prev = Free;
				Result->Next       = Free;
			}
			RemoveFromFreeList(Result);
		}
		if( Size && Original )
		{
			appMemcpy(Result+1,Original+1,Min(Result->GetSize(),Original->GetSize()));
		}
		if( Original )
		{
			check(!Original->FreePrevLink);
			IFPAD(check(Original->Pad==PAD_Used);)
			AddToFreeList(Original);
			TryToConsumeNext(Original);
			TryToConsumeNext(Original->Prev);
		}
		return Result ? Result+1 : NULL;
		unguardf(( TEXT("%08X %i %s"), (INT)Ptr, Size, Tag ));
	}
	void DumpAllocs( void )
	{
		INT Mem[2]={0,0}, Count[2]={0,0}, Frag=0;
		for( FAllocHeader* H=Head->Next; H!=Tail; H=H->Next )
			Mem[H->FreePrevLink==NULL]+=H->GetSize(), Count[H->FreePrevLink==NULL]++;
//		debugf(TEXT("All memory view:"));
//		for( H=Head->Next; H!=Tail; H=H->Next )
//			debugf( TEXT("   %i: %i"), H->FreePrevLink==NULL, H->GetSize());
//		debugf(TEXT("Fragmentation view:"));
//		for( H=Head->Next; H!=Tail; H=H->Next )
//			if( H->FreePrevLink )
//				debugf( TEXT("   %i %i: %i"), H->Prev->FreePrevLink==NULL, H->Next->FreePrevLink==NULL, H->GetSize());
		debugf( TEXT("%i Free Allocations, %i Used Allocations, %i bytes lost"), Count[0], Count[1], AllSize-2*sizeof(FAllocHeader)-(Count[0]+Count[1])*sizeof(FAllocHeader)-Mem[0]-Mem[1] );
		debugf( TEXT("%.2fM Free Tail + %.2fM Fragmented + %.2fM Used = %.2fM"), (Mem[0]-Frag)/1024.f/1024.f, Frag/1024.f/1024.f, Mem[1]/1024.f/1024.f, (Mem[0]+Mem[1])/1024.f/1024.f );
	}
	void Init()
	{
		if (MallocInitted)
			return;
		MallocInitted = 1;
		check((sizeof(FAllocHeader)%16)==0);
		for(INT i=0; i<ARRAY_COUNT(FreeList); i++) FreeList[i]=NULL;
		//SL TODO: This will be around 128 Megs on TOOL, 32 on PS2, set in PSX2Launch/Src/app.cmd!!!!!!!!!!!
		for(AllSize=128*1024*1024; (Head=(FAllocHeader*)malloc(AllSize))==NULL; AllSize-=1024);
		
		free(Head);
		//SL TODO: Take out thos next line???
//		AllSize             -= 1*1024*1024;//work around for library's broken print-f which leaks memory!!
		Head                 = (FAllocHeader*)malloc(AllSize);
		check(Head);
		//while(malloc(1));

		FAllocHeader* AllMem = Head+1;
		Tail                 = (FAllocHeader*)((BYTE*)Head+AllSize-sizeof(FAllocHeader));

		Head->Next           = AllMem;
		Head->Prev           = NULL;
		Head->FreePrevLink   = NULL;
		IFPAD(Head->Pad      = PAD_Head;)

		AllMem->Next         = Tail;
		AllMem->Prev         = Head;
		IFPAD(AllMem->Pad    = PAD_Used;)

		Tail->Next           = NULL;
		Tail->Prev           = AllMem;
		Tail->FreePrevLink   = NULL;
		IFPAD(Tail->Pad      = PAD_Tail;)

		AddToFreeList(AllMem);
printf("Initted mem");
		debugf(TEXT("Memory allocator initialized: %i @ %08X"),AllSize,(INT)AllMem);
	}
	void Exit() {}
	void HeapCheck() {}
	INT MemSize( void* Ptr )
	{
		return Ptr ? ((FAllocHeader*)Ptr-1)->GetSize() : 0;
	}
	void Clean()
	{
		for( FAllocHeader* H=Head->Next; H!=Tail; H=H->Next )
			if( H->FreePrevLink )
				appMemzero( H+1, H->GetSize() );
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

