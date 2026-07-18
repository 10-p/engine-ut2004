/*=============================================================================
	UnInteraction.cpp: See .UC for for info
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Joe Wilcox
=============================================================================*/

#include "EnginePrivate.h"
#include "UnLinker.h"

IMPLEMENT_CLASS(ASecurity);

//
// Perform various aspects of Cheat Protection
//
void ASecurity::execNativePerform( FFrame& Stack, RESULT_DECL )
{
	guard(ASecurity::execNativePerform::_Init);

	P_GET_INT(SecType)		// Type of security to perform
	P_GET_STR(Param1);	// Parameter #1
	P_GET_STR(Param2);	// Parameter #2

	P_FINISH;

	static INT CurrentLinker;

	TArray<UObject*>	ObjLoaders = UObject::GetLoaderList(); 

	switch (SecType)
	{
		case 0:	//  Return a QuickMD5 for a selected package from disk
			{

				for( INT i=0; i<ObjLoaders.Num(); i++ )
				{
	
					ULinker * Linker = CastChecked<ULinker>( ObjLoaders(i) );
					if (!appStricmp (Linker->LinkerRoot->GetName(), *Param1) )
					{

						FString MD5;
						FString GUID;

						UMasterMD5Commandlet* MasterMD5 = new UMasterMD5Commandlet;
						if (MasterMD5)
						{
							if (MasterMD5->DoQuickMD5(*Linker->Filename, MD5, GUID) )
							{
								eventServerCallback( SecType, MD5);
								return;
							}
						}
						else
							debugf(TEXT("Error creating a MasterMD5"));
					}
				}

				eventServerCallback(255,TEXT("Package Not Loaded"));
				return;
				break;
			}

		case 1: // Return a Code MD5 on a function
			{
			
				FString Test = FString::Printf(TEXT("function %s"), *Param1);

				for( TObjectIterator<UStruct> It ; It ; ++It )
				{

					if (!appStricmp(It->GetFullName(),*Test) )
					{
						eventServerCallback(SecType, It->FunctionMD5());
						return;
									
					}
				}

				eventServerCallback(255,TEXT("Function Not Found"));
				return;
				break;
			}

		case 2:	// Return a full MD5 on a file
			{
				FArchive* MD5Ar = GFileManager->CreateFileReader( *Param1 );
				int BytesToRead;
				if( !MD5Ar )
				{
						eventServerCallback(255,TEXT("File was not found"));
						return;
				}

				BYTE* MD5Buffer = (BYTE*)appMalloc(32767, TEXT(""));
	
				FMD5Context PMD5Context;
				appMD5Init( &PMD5Context );
			
				while ( MD5Ar->Tell() < MD5Ar->TotalSize() )
				{
					BytesToRead = MD5Ar->TotalSize() - MD5Ar->Tell();
					if (BytesToRead>32767)
						BytesToRead=32767;

					MD5Ar->Serialize(MD5Buffer, BytesToRead);
					appMD5Update( &PMD5Context, MD5Buffer, BytesToRead);
				}
				BYTE Digest[16];
				appMD5Final( Digest, &PMD5Context );


				// Convert to a string

				FString FullMD5;
				for (int i=0; i<16; i++)
					FullMD5 += FString::Printf(TEXT("%02x"), Digest[i]);	

				eventServerCallback(SecType, FullMD5);

				// Free the buffer
	
				appFree(MD5Buffer);

				delete MD5Ar;
				break;
			}

		case 3:	// Return First Package
			{
				CurrentLinker = 0;
				FString Packages;
				if (ObjLoaders.Num()>0 )
				{
					ULinker * Linker = CastChecked<ULinker>( ObjLoaders(0) );
					Packages += Linker->LinkerRoot->GetName();
				}
				eventServerCallback(SecType, Packages);
				break;
			}

		case 4:	// Returns next Package
			{
				FString Packages;
				for (INT i=0;i<ObjLoaders.Num();i++ )
				{
					if (i>CurrentLinker)
					{
						CurrentLinker=i;
						ULinker * Linker = CastChecked<ULinker>( ObjLoaders(i) );
						Packages += Linker->LinkerRoot->GetName();
					}
				}
				eventServerCallback(SecType, Packages);
				break;
			}

		case 5:	//  Return a QuickMD5 for a selected package from memory
			{
				for( INT i=0; i<ObjLoaders.Num(); i++ )
				{
	
					ULinker * Linker = CastChecked<ULinker>( ObjLoaders(i) );
					if (!appStricmp (Linker->LinkerRoot->GetName(), *Param1) )
					{
						eventServerCallback(255,Linker->QuickMD5() );
						return;
					}
				}

				eventServerCallback(255,TEXT("Package Not Loaded"));
				break;
			}

		case 6:	//  Return a GUID
			{
				for( INT i=0; i<ObjLoaders.Num(); i++ )
				{
	
					ULinker * Linker = CastChecked<ULinker>( ObjLoaders(i) );
					if (!appStricmp (Linker->LinkerRoot->GetName(), *Param1) )
					{
						FString GUID = Linker->Summary.Guid.String();
						eventServerCallback(255,GUID );
						return;
					}
				}

				eventServerCallback(255,TEXT("Package Not Loaded"));
				return;
				break;
			}
	}

	unguardexec;
}

void ASecurity::execLocalPerform( FFrame& Stack, RESULT_DECL )
{
	guard(ASecurity::execLocalPerform::_Init);

	P_GET_INT(SecType)		// Type of security to perform
	P_GET_STR(Param1);	// Parameter #1
	P_GET_STR(Param2);	// Parameter #2

	P_GET_TARRAY_REF(Results, FString);
	P_FINISH

	Results->Empty();

	TArray<UObject*>	ObjLoaders = UObject::GetLoaderList(); 
	
	switch (SecType)
	{
		case 0:	//  Return a QuickMD5 for a selected package from disk
			{

				for( INT i=0; i<ObjLoaders.Num(); i++ )
				{
					ULinker * Linker = CastChecked<ULinker>( ObjLoaders(i) );
					if (!appStricmp (Linker->LinkerRoot->GetName(), *Param1) )
					{
						FString MD5;
						FString GUID;

						UMasterMD5Commandlet* MasterMD5 = new UMasterMD5Commandlet;
						if (MasterMD5)
						{
							if (MasterMD5->DoQuickMD5(*Linker->Filename, MD5, GUID) )
							{
								int k = Results->AddZeroed(); (*Results)(k)= FString( *FString::Printf( TEXT("%s%s"),*GUID,*MD5) );
								return;
							}
						}
						else
							debugf(TEXT("Error creating a MasterMD5"));
					}
				}
				return;
				break;
			}

		case 1: // Return a Code MD5 on a function
			{
			
				FString Test = FString::Printf(TEXT("function %s"), *Param1);

				for( TObjectIterator<UStruct> It ; It ; ++It )
				{

					if (!appStricmp(It->GetFullName(),*Test) )
					{
						int k = Results->AddZeroed(); (*Results)(k)= FString::Printf( TEXT("%s"),*It->FunctionMD5() );
						return;
									
					}
				}

				return;
				break;
			}

		case 2:	// Return a full MD5 on a file
			{
				FArchive* MD5Ar = GFileManager->CreateFileReader( *Param1 );
				int BytesToRead;
				if( !MD5Ar )
				{
						return;
				}

				BYTE* MD5Buffer = (BYTE*)appMalloc(32767, TEXT(""));
	
				FMD5Context PMD5Context;
				appMD5Init( &PMD5Context );
			
				while ( MD5Ar->Tell() < MD5Ar->TotalSize() )
				{
					BytesToRead = MD5Ar->TotalSize() - MD5Ar->Tell();
					if (BytesToRead>32767)
						BytesToRead=32767;

					MD5Ar->Serialize(MD5Buffer, BytesToRead);
					appMD5Update( &PMD5Context, MD5Buffer, BytesToRead);
				}
				BYTE Digest[16];
				appMD5Final( Digest, &PMD5Context );


				// Convert to a string

				FString FullMD5;
				for (int i=0; i<16; i++)
					FullMD5 += FString::Printf(TEXT("%02x"), Digest[i]);	

				int k = Results->AddZeroed(); (*Results)(k)= FString::Printf( TEXT("%s"),*FullMD5 );

				// Free the buffer
	
				appFree(MD5Buffer);

				delete MD5Ar;
				break;
			}

		case 3:	// Returns all packages
			{
				FString Packages;
				for (INT i=0;i<ObjLoaders.Num();i++ )
				{
					ULinker * Linker = CastChecked<ULinker>( ObjLoaders(i) );
					int k = Results->AddZeroed(); 
					(*Results)(k) = FString::Printf( TEXT("%s:%s:%s:%i"),Linker->LinkerRoot->GetName(),Linker->Summary.Guid.String(), *Linker->QuickMD5(), Linker->LinksToCode() );
				}
				break;
			}

		case 4:	// Returns the Security Info
			{
				UGameEngine* GE = Cast<UGameEngine>(XLevel->Engine);
				for (INT i=0;i<GE->PackageValidation.Num();i++)
				{
					for (INT j=0;j<GE->PackageValidation(i)->AllowedIDs.Num();j++)
					{
						int k = Results->AddZeroed(); 
						(*Results)(k)= FString::Printf( TEXT("%s:%s"),*GE->PackageValidation(i)->PackageID, *GE->PackageValidation(i)->AllowedIDs(j)  );
					}
				}
				break;
			}

		case 5:	//  Return a QuickMD5 for a selected package from memory
			{

				for( INT i=0; i<ObjLoaders.Num(); i++ )
				{
	
					ULinker * Linker = CastChecked<ULinker>( ObjLoaders(i) );
					if (!appStricmp (Linker->LinkerRoot->GetName(), *Param1) )
					{
						int k = Results->AddZeroed(); (*Results)(k)= FString::Printf( TEXT("%s"),*Linker->QuickMD5() );
						return;
					}
				}

				break;
			}

		case 6:	//  Return a GUID
			{
				for( INT i=0; i<ObjLoaders.Num(); i++ )
				{
					ULinker * Linker = CastChecked<ULinker>( ObjLoaders(i) );
					if (!appStricmp (Linker->LinkerRoot->GetName(), *Param1) )
					{
						int k = Results->AddZeroed(); (*Results)(k)= FString::Printf( TEXT("%s"),Linker->Summary.Guid.String() );
						return;
					}
				}

				break;
			}
		case 7: // Get MD5 Database info on a file
			{
				FString Guid;
				
				for( INT i=0; i<ObjLoaders.Num(); i++ )
				{
					ULinker * Linker = CastChecked<ULinker>( ObjLoaders(i) );
					if (!appStricmp (Linker->LinkerRoot->GetName(), *Param1) )
					{
						Guid = Linker->Summary.Guid.String();
						break;
					}
				}

				UGameEngine* GE = Cast<UGameEngine>(XLevel->Engine);
				for (INT j=0;j<GE->PackageValidation.Num();j++)
				{
					if (GE->PackageValidation(j)->PackageID = Guid)
					{
						for (INT k=0;k<GE->PackageValidation(j)->AllowedIDs.Num(); k++)
						{
							int l = Results->AddZeroed(); 
							(*Results)(l)= FString::Printf( TEXT("%s"),*GE->PackageValidation(j)->AllowedIDs(k) );
						}

						return;
					}
				}
				break;
			}

		case 100: // Get Aliases
			{
				UViewport* VP = XLevel->Engine->Client->Viewports(0);
				if ( VP )
				{
					for (INT i=0; i<ARRAY_COUNT(VP->Input->Aliases); i++ )
					{
						int l = Results->AddZeroed(); 
						(*Results)(l)= FString::Printf( TEXT("%s = [%s]"),*VP->Input->Aliases[i].Alias,*VP->Input->Aliases[i].Command );
					}
				}
				break;
			}
					
		case 101: // Get Binds
			{
				UViewport* VP = XLevel->Engine->Client->Viewports(0);
				if ( VP )
				{
					for (INT i=0; i<ARRAY_COUNT(VP->Input->Bindings); i++ )
					{
						int l = Results->AddZeroed(); 
						(*Results)(l)= FString::Printf( TEXT("%s"),*VP->Input->Bindings[i]);
					}
				}
				break;
			}
	}

	unguardexec;
}

