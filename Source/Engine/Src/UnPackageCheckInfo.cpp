/*=============================================================================
	UnPackageCheckInfo : Stores allowable MD5s for each package
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Joe Wilcox
	
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UPackageCheckInfo);

UBOOL UPackageCheckInfo::VerifyID(FString CurrentId)
{
	// Look for this id in the chain

	for ( int i=0; i<AllowedIDs.Num(); i++ )
	{
		if ( CurrentId == AllowedIDs(i) )
			return true;
	}

	return false;
}

void UPackageCheckInfo::Serialize( FArchive& Ar )
{

	guard(UPackageCheckInfo::Serialize);

	Super::Serialize( Ar );

	if( Ar.IsLoading() )		// Load in the block
	{
		FString TmpStr;
		Ar << PackageID;		// Get the Package's GUID
		Ar << AllowedIDs;		// Save the array
		if( Ar.Ver() > 120 )
		{
			Ar << Native;
			Ar << RevisionLevel;
		}
		else
		{
			Native=false;
			RevisionLevel=0;
		}

	}
	else if ( Ar.IsSaving() )// && Native)						// Save out the block
	{
		Ar << PackageID;
		Ar << AllowedIDs;
		Ar << Native;
		Ar << RevisionLevel;
	}

	unguard;

}

/* ====================================================================================== */

#define BF_F(x)	( ( (SB[0][x.byte.zero] + SB[1][x.byte.one]) ^ SB[2][x.byte.two]) + SB[3][x.byte.three])

void FBlowFish::BF_En(BF_Word *x1,BF_Word *x2)
{
  BF_Word w1=*x1,w2=*x2;

  w1.word ^= PA[0];
  w2.word ^= BF_F(w1)^PA[1];       w1.word ^= BF_F(w2)^PA[2];
  w2.word ^= BF_F(w1)^PA[3];       w1.word ^= BF_F(w2)^PA[4];
  w2.word ^= BF_F(w1)^PA[5];       w1.word ^= BF_F(w2)^PA[6];
  w2.word ^= BF_F(w1)^PA[7];       w1.word ^= BF_F(w2)^PA[8];
  w2.word ^= BF_F(w1)^PA[9];       w1.word ^= BF_F(w2)^PA[10];
  w2.word ^= BF_F(w1)^PA[11];      w1.word ^= BF_F(w2)^PA[12];
  w2.word ^= BF_F(w1)^PA[13];      w1.word ^= BF_F(w2)^PA[14];
  w2.word ^= BF_F(w1)^PA[15];      w1.word ^= BF_F(w2)^PA[16];
  w2.word ^= PA[17];

  *x1 = w2;
  *x2 = w1;
}

void FBlowFish::BF_De(BF_Word *x1,BF_Word *x2)
{
  BF_Word w1=*x1,w2=*x2;

  w1.word ^= PA[17];
  w2.word ^= BF_F(w1)^PA[16];      w1.word ^= BF_F(w2)^PA[15];
  w2.word ^= BF_F(w1)^PA[14];      w1.word ^= BF_F(w2)^PA[13];
  w2.word ^= BF_F(w1)^PA[12];      w1.word ^= BF_F(w2)^PA[11];
  w2.word ^= BF_F(w1)^PA[10];      w1.word ^= BF_F(w2)^PA[9];
  w2.word ^= BF_F(w1)^PA[8];       w1.word ^= BF_F(w2)^PA[7];
  w2.word ^= BF_F(w1)^PA[6];       w1.word ^= BF_F(w2)^PA[5];
  w2.word ^= BF_F(w1)^PA[4];       w1.word ^= BF_F(w2)^PA[3];
  w2.word ^= BF_F(w1)^PA[2];       w1.word ^= BF_F(w2)^PA[1];
  w2.word ^= PA[0];

  *x1 = w2;
  *x2 = w1;
}

void FBlowFish::Reset(const TCHAR *Key)
{
	INT i,j;

	for (i=0;i<BF_NUM_SUBKEYS;i++)
	{
		PA[i] = PA_Init[i];
	}

	for (j=0;j<BF_NUM_S_BOXES;j++)
	{
		for (i=0;i<BF_NUM_ENTRIES;i++)
		{
			SB[j][i] = SB_Init[j][i];
		}
	}

	INT Length = appStrlen(Key);
	BF_Word Work;
	BF_Word TempA;
	BF_Word TempB;

	if (Length > 0)
	{
		j = 0;
	    for (i=0;i<BF_NUM_SUBKEYS;i++)
	    {
			Work.byte.zero =  Key[ (j++) % Length];
			Work.byte.one =   Key[ (j++) % Length];
			Work.byte.two =   Key[ (j++) % Length];
			Work.byte.three = Key[ (j++) % Length];
			PA[i] ^= Work.word;
		}

		TempA.word = TempB.word = 0;
		
		for (i=0;i<BF_NUM_SUBKEYS;i+=2)
		{
			BF_En(&TempA,&TempB);
			PA[i] = TempA.word;
			PA[i+1] = TempB.word;
		}

		for (j=0;j<BF_NUM_S_BOXES;j++)
		{
			for (i=0;i<BF_NUM_ENTRIES;i+=2)
			{
				BF_En(&TempA,&TempB);
				SB[j][i] = TempA.word;
				SB[j][i+1] = TempB.word;
			}
		}
	}
}

void FBlowFish::Encrypt(void *Buffer, INT Length, FString Key)
{
	BF_DWord *Work;

	Reset(*Key);

	if ( Length % 8 )
	{
		GWarn->Logf(TEXT("Blowfish requires the input to be a multiple of 8 bytes (64 bits) to work."));
		return;
	}

	Length /= 8;
	Work = (BF_DWord *)Buffer;

	for (INT i=0; i<Length; i++)
	{
		BF_En(&Work->Word0,&Work->Word1);
		Work++;
	}
}

void FBlowFish::Decrypt(void *Buffer,INT Length, FString Key)
{
	BF_DWord *Work;

	Reset(*Key);

	if ( Length % 8 )
	{
		GWarn->Logf(TEXT("Blowfish requires the input to be a multiple of 8 bytes (64 bits) to work."));
		return;
	}

	Length /= 8;
	Work = (BF_DWord *)Buffer;
	for (INT i=0; i<Length; i++)
	{
		BF_De(&Work->Word0,&Work->Word1);
		Work++;
	}
}


