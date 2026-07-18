#include "UTVPackageMap.h"
#include "UnLinker.h"
#include "BunchDelayer.h"

void UUTVPackageMap::ShowStatus ()
{
	debugf (TEXT ("We have %i names and %i objects loaded"), MaxNameIndex, MaxObjectIndex);
/*	for(int a=0;a<100;++a){
		printf("%i ",NameIndices(a));
		IndexToObjectName(NameIndices(a));
	}*/
}

void UUTVPackageMap::Compute ()
{
	UPackageMap::Compute ();

	guard (UUTVPackageMap::Compute);

	WasComputeError = false;

	//check so we are not outdated
	for( INT j=0; j<List.Num(); j++ ) {
		if( List(j).LocalGeneration<List(j).RemoteGeneration ){
			debugf(TEXT("Error: Both server and primary has newer version of package than us %i %i %s"),List(j).RemoteGeneration,List(j).LocalGeneration,*List(j).URL);
			WasComputeError = true;
			return;
		}

		//debugf (TEXT ("Package %i (%s) - local %i - remote %i"), j, *List(j).URL, List(j).LocalGeneration, List(j).RemoteGeneration);
	}

	//Find the object id of our new replication object
	UTVReplication = 0;

	INT i = List.Num () - 1;
	FPackageInfo& Info = List (i);
	
	ExtraObjects = Info.ObjectCount;
	ExtraNames = Info.NameCount;

	debugf(TEXT("Package ") UTV2004C TEXT(" has %d names and %d objects"), ExtraNames, ExtraObjects);

	for (INT j = 0; j < Info.ObjectCount; ++j) {
		if (Info.Linker->ExportMap(j).ObjectName == FName (TEXT ("utvReplication"))) {
			UTVReplication = Info.ObjectBase + j;
			debugf (TEXT ("Found utvReplication with object id %i"), UTVReplication);
		}

		//debugf (TEXT ("Loaded %i: %s"), Info.ObjectBase + j, *(Info.Linker->ExportMap(j).ObjectName));
	}

	if (UTVReplication == 0) {
		debugf (TEXT ("Could not find the utvReplication!"));
	}

	//Create the needed things for reading out the utvReplication stuff
	RSend = 0;
	RGet = 0;
	RSendT = 0;
	RGetT = 0;

	UClass* tmp = StaticLoadClass( AActor::StaticClass(), NULL, UTV2004C TEXT(".utvReplication"), NULL, LOAD_NoFail, NULL );
	FClassNetCache* cc = GetClassNetCache(tmp);
	if (!cc) {
		debugf (TEXT ("Could not create the ClassNetCache for utvReplication!"));
	}

	printf("-");
	RMax = cc->GetMaxIndex ();
	for (int i=0; i < cc->GetMaxIndex(); ++i) {
		printf("\010-");
		FFieldNetCache* FieldCache = cc->GetFromIndex( i );
		printf("\010/");
		FName f = FieldCache->Field->GetFName();		
		printf("\010|");

		if (f == FName (TEXT ("SendToServer"))) {
			RSend = i;
			printf("\010");
			debugf (TEXT ("\010Found utvReplication.SendToServer with id %i"), RSend);
			printf("-");	
		}
		printf("\010\\");
		if (f == FName (TEXT ("GetFromServer"))) {
			RGet = i;
			printf("\010");
			debugf (TEXT ("Found utvReplication.GetFromServer with id %i"), RGet);
			printf("-");
		}
		if (f == FName (TEXT ("SendTarget"))) {
			RSendT = i;
			printf("\010");
			debugf (TEXT ("\010Found utvReplication.SendTarget with id %i"), RSendT);
			printf("-");	
		}
		printf("\010\\");
		if (f == FName (TEXT ("GetTarget"))) {
			RGetT = i;
			printf("\010");
			debugf (TEXT ("Found utvReplication.GetTarget with id %i"), RGetT);
			printf("-");
		}
	}
	printf("\010");
	if (RSend == 0) {
		debugf (TEXT ("Could not find utvReplication.SendToServer!"));
	}
	if (RGet == 0) {
		debugf (TEXT ("Could not find utvReplication.GetFromServer!"));
	}

	//precache
	//This can crash, and is probably not beneficial anyway
	/*debugf (TEXT ("Simple precache start.."));
	UObject* Object;
	Object = IndexToObject( UTVReplication - 1000, 1 );
	debugf (TEXT ("Simple precache end.."));*/

	unguard;
}

void UUTVPackageMap::FixLinkers (UPackageMap* Other) 
{
	guard (UUTVPackageMap::FixLinkers);

	//Start with clearing the other's list (should be empty)
	while (Other->List.Num () > 0) {
		Other->List.Remove (0);
	}

	//now copy our list there
	for (INT i = 0; i < List.Num(); ++i) {
		FPackageInfo& Info = *new(Other->List)FPackageInfo(NULL);
		Info.Guid = List(i).Guid;
		Info.RemoteGeneration = List(i).RemoteGeneration;
		Info.LocalGeneration = List(i).RemoteGeneration;
		Info.FileSize = List(i).FileSize;
		Info.PackageFlags = List(i).PackageFlags;
		Info.URL = List(i).URL;
		Info.Parent = CreatePackage (NULL, List(i).Parent->GetName ());
		Info.Linker = List(i).Linker;
	}

	unguard;
}

//This function fixes all url entries that reside in the cache folder
void UUTVPackageMap::PostFixLinkers (UPackageMap* Other) 
{
	guard (UUTVPackageMap::PostFixLinkers);

	for (INT i = 0; i < Other->List.Num(); ++i) {
		if (RealNames(i).Len () > 0) {
			//debugf (TEXT ("Changing %s to %s"), *Other->List(i).URL, *RealNames (i));
			Other->List(i).URL = RealNames (i);
		}
	}

	unguard;
}

void UUTVPackageMap::ClearDepend ()
{
	LastDepend = 0;
}

INT UUTVPackageMap::GetDepend ()
{
	return LastDepend;
}

INT UUTVPackageMap::ObjectToIndex(UObject *Object)
{
	//debugf(TEXT("Converting %s to index"), Object->GetFullName());
	INT i = UPackageMap::ObjectToIndex(Object);
	//debugf(TEXT("Result: %d"), i);
	return i;
}

//When this is true, SerializeObject will use the "read" space (which means it will treat
//ints as coming from someone who doesn't know about utv2004c)
void UUTVPackageMap::SetSerializeRead(bool isRead)
{
	IsRead = isRead;
}

UBOOL UUTVPackageMap::SerializeObject( FArchive& Ar, UClass* Class, UObject*& Object )
{
	guard(UUTVPackageMap::SerializeObject);

#if PLATFORM_64BITS
    check(0);  // !!! FIXME
    return 0;
#else

	DWORD Index=0;
	if( Ar.IsLoading() )
	{
		Object = NULL;
		BYTE B=0; Ar.SerializeBits( &B, 1 );
		if (B)
		{
			// Dynamic actor or None.
			Ar.SerializeInt( Index, UNetConnection::MAX_CHANNELS );
			//debugf(TEXT("Receiving index: %i"), Index);
			Object = (UObject*)Index;
			//Object = Delayer->actors[Index].Actor;

			//debugf(TEXT("Got dynamic object %s"), Object->GetFullName());

			if (Class != NULL) {
				if (Class->IsChildOf (AActor::StaticClass ()))
					LastDepend = Index;
			}
//			LastDepend = Index;
		}
		else
		{
			// Static object.
			if (IsRead)
				Ar.SerializeInt( Index, MaxObjectIndex - ExtraObjects);
			else  
				Ar.SerializeInt(Index, MaxObjectIndex); 

			//Check forged
			UObject *tmp = IndexToObject( Index, 1 );

			if (Index > MaxObjectIndex - ExtraObjects)
				debugf(TEXT("Moo %d"), IsRead);

			if( tmp && !tmp->IsA(Class) )
				debugf(TEXT("Forged object: got %s, expecting %s (index %d of %d)"),tmp->GetFullName(),Class->GetFullName(), Index, MaxObjectIndex - ExtraObjects);
/*			else
				debugf(TEXT("Got ok object %s"), tmp->GetFullName()); */
			Object = tmp;

			//debugf (TEXT ("Static object with index %i %d"), Index, IsRead);
			Index = Index + 2000;
			Object = (UObject*)Index;
		}

		return 1;
	}
	else
	{
		Index = (DWORD)Object;
		if (Index < 2000) {
			BYTE B=1;
			Ar.SerializeBits (&B, 1);
			Ar.SerializeInt (Index, UNetConnection::MAX_CHANNELS);
		}
		else {
			Index = Index - 2000;
			BYTE B=0;
			Ar.SerializeBits (&B, 1);
			Ar.SerializeInt (Index, MaxObjectIndex);
		}
		return 1; 
	}
#endif
	unguard;
}

UBOOL UUTVPackageMap::SerializeName(FArchive &Ar, DWORD &Index)
{
	guard("UTVPackageMap::SerializeName");
	//debugf(TEXT("Using serializename"));
	if (Ar.IsLoading()) {
		
		if (IsRead)
			Ar.SerializeInt( Index, MaxNameIndex+1 - ExtraNames);
		else  
			Ar.SerializeInt( Index, MaxNameIndex+1 );

/*		FName Name = NAME_None;
		if( Index<MaxNameIndex && !Ar.IsError() )
		{
			for( INT i=0; i<List.Num(); i++ )
			{
				FPackageInfo& Info = List(i);
				if( Index < (DWORD)Info.NameCount )
				{
					Name = Info.Linker->NameMap(Index);
					debugf(TEXT("Got name: %s"), *Name);
					break;
				}
				Index -= Info.NameCount;
			}
		}*/

		return 1;
	}
	else {
		Ar.SerializeInt( Index, MaxNameIndex+1 );
		return 1;
	}
	unguard;
}

/*UBOOL UUTVPackageMap::SerializeObject( FArchive& Ar, UClass* Class, UObject*& Object )
{
	guard(UPackageMapLevel::SerializeObject);
	DWORD Index=0;
	if( Ar.IsLoading() )
	{
		Object = NULL;
		BYTE B=0; Ar.SerializeBits( &B, 1 );
		if( B )
		{
			// Dynamic actor or None.
			Ar.SerializeInt( Index, UNetConnection::MAX_CHANNELS );
			if( Index==0 )
			{
				Object = NULL;
			}
			else if
			(	!Ar.IsError()
			&&	Index>=0
			&&	Index<UNetConnection::MAX_CHANNELS
			&&	Connection->Channels[Index]
			&&	Connection->Channels[Index]->ChType==CHTYPE_Actor 
			&&	!Connection->Channels[Index]->Closing )
				Object = ((UActorChannel*)Connection->Channels[Index])->GetActor();
		}
		else
		{
			// Static object.
			Ar.SerializeInt( Index, MaxObjectIndex );
			if( !Ar.IsError() )
				Object = IndexToObject( Index, 1 );
		}
		if( Object && !Object->IsA(Class) )
		{
			debugf(TEXT("Forged object: got %s, expecting %s"),Object->GetFullName(),Class->GetFullName());
			Object = NULL;
		}
		return 1;
	}
	else
	{
		AActor* Actor = Cast<AActor>(Object);
		if( Actor && !Actor->bStatic && !Actor->bNoDelete )
		{
			// Map dynamic actor through channel index.
			BYTE B=1; Ar.SerializeBits( &B, 1 );
			UActorChannel* Ch = Connection->ActorChannels.FindRef(Actor);
			UBOOL Mapped = 0;
			if( Ch )
			{
				Index  = Ch->ChIndex;
				Mapped = Ch->OpenAcked;
			}
			Ar.SerializeInt( Index, UNetConnection::MAX_CHANNELS );
			return Mapped;
		}
		else if( !Object || (Index=ObjectToIndex(Object))==INDEX_NONE )
		{
			BYTE B=1; Ar.SerializeBits( &B, 1 );
			Ar.SerializeInt( Index, UNetConnection::MAX_CHANNELS );
			return 1;
		}
		else
		{
			// Map regular object.
			// Since mappability doesn't change dynamically, there is no advantage to setting Result!=0.
			BYTE B=0; Ar.SerializeBits( &B, 1 );
			Ar.SerializeInt( Index, MaxObjectIndex );
			return 1;
		}
	}
	unguardf(( TEXT("(%s %s)"), Class->GetFullName(), Object->GetFullName() ));
}*/

INT UUTVPackageMap::GetReplicationId ()
{
	return UTVReplication;
}

INT UUTVPackageMap::GetReplicationGet ()
{
	return RGet;
}

INT UUTVPackageMap::GetReplicationSend ()
{
	return RSend;
}

INT UUTVPackageMap::GetReplicationSendTarget ()
{
	return RSendT;
}

INT UUTVPackageMap::GetReplicationGetTarget ()
{
	return RGetT;
}

INT UUTVPackageMap::GetReplicationMax ()
{
	return RMax;
}

INT UUTVPackageMap::NeededPackage ()
{
	return MissingPackage;
}

UBOOL UUTVPackageMap::ComputeError ()
{
	return WasComputeError;
}

UBOOL UUTVPackageMap::CreateLinkers ()
{
	TCHAR t[256] = TEXT("");

	guard (UTVPackageMap::CreateLinkers);
	
	//Indicate nothing missing yet
	MissingPackage = -1;
	RealNames.Empty ();
	
	BeginLoad ();

	for( INT i=0; i<List.Num(); i++ ) {

		//Start with checking if the load will fail
		if (!appFindPackageFile( List(i).Parent->GetName(), &List(i).Guid, t, List(i).RemoteGeneration )) {
			debugf (TEXT ("Packagemap: Could not find the package: %s"), List(i).Parent->GetName ());
			MissingPackage = i;

			EndLoad ();
			return false;
		}
		else {
			//Update the list with actual path+name of this file
			INT j = RealNames.AddZeroed ();	//must return == i :)
			check (i == j);
			RealNames (i) = FString::Printf (TEXT ("%s"), t);
		}

		//Now get a linker
		List(i).Linker = GetPackageLinker
		(
			List(i).Parent,
			NULL,
			//LOAD_Verify | LOAD_Throw | LOAD_NoWarn | LOAD_NoVerify,
			LOAD_NoWarn,
			NULL,
			&List(i).Guid,
			List(i).RemoteGeneration
		);

		List(i).LocalGeneration  = List(i).Linker->Summary.Generations.Num();
		//debugf (TEXT ("Linker %i (%s): %s"), i, *List(i).URL, *List(i).Linker->GetFName ());
	}

	EndLoad ();

	return true;

	unguard;
}

INT UUTVPackageMap::GetMaxObjectIndex ()
{
	debugf(TEXT("Calling GetMaxObjectIndex"));
	return MaxObjectIndex;
}

INT UUTVPackageMap::GetMaxObjectIndexWrite ()
{
	return MaxObjectIndex;
}

INT UUTVPackageMap::GetMaxObjectIndexRead ()
{
	return MaxObjectIndex - ExtraObjects;
}

void UUTVPackageMap::IndexToObjectName( INT Index )
{
	guard(UUTVPackageMap::IndexToObjectName);
	if( Index>=0 )
	{
		for( INT i=0; i<List.Num(); i++ )
		{
			FPackageInfo& Info = List(i);
			if( Index < Info.ObjectCount )
			{
				const TCHAR *t = *(Info.Linker->ExportMap(Index).ObjectName);
				debugf (TEXT("Actorname: %s"), t);
				//Info.Linker->ExportMap(Index).
				return;
				
				//debugf(TEXT("jao %s"),Info.Linker->ExportMap(Index).ObjectName);
				//return Info.Linker->ExportMap(Index).ObjectName;
			}
			Index -= Info.ObjectCount;
		}
	}
	//return NULL;
	unguard;
}

IMPLEMENT_CLASS(UUTVPackageMap);
