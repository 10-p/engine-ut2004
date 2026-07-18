#include "BunchDelayer.h"
#include "ReplicatorEngine.h"
#include "UTVPackageMap.h"
#include "UtvVoiceChannel.h"
#include "DemoPassthrough.h"

/////////////////////////////////

MoveDelayer mDelayer;

MoveDelayer::MoveDelayer(void)
{
}

void MoveDelayer::Tick(float DeltaSeconds)
{
	while(!WaitingMoves.Empty() && WaitingMoves.Front().OutTime< Delayer->CurrentTime){
		WaitingMove wb=WaitingMoves.Front();
		WaitingMoves.PopFront();

//		debugf (TEXT ("Sending out delayed move: %s"), wb.Move);

		for( INT i=0; i<UtvEngine->ListenDriver->ClientConnections.Num(); i++ ){
			UNetConnection* Connection=UtvEngine->ListenDriver->ClientConnections(i);
			if(UtvEngine->OpenConnections.Find(Connection)->isReady && Connection!=UtvEngine->PrimaryConnection){
				UtvEngine->SendMessageToClient (Connection, wb.Move);
			}
		}
	}

	while(!WaitingActors.Empty() && WaitingActors.Front().OutTime < Delayer->CurrentTime){
		WaitingActor wb=WaitingActors.Front();
		WaitingActors.PopFront();

		for( INT i=0; i<UtvEngine->ListenDriver->ClientConnections.Num(); i++ ){
			UNetConnection* Connection=UtvEngine->ListenDriver->ClientConnections(i);
			if(UtvEngine->OpenConnections.Find(Connection)->isReady && Connection!=UtvEngine->PrimaryConnection){
				UtvEngine->SendActorToClient (Connection, wb.Actor);
			}
		}
	}
}

void MoveDelayer::AddMove(FString Move)
{
	WaitingMove wb;
	wb.Move = Move;
	wb.OutTime=Delayer->CurrentTime+Delayer->DelayTime-5;
	WaitingMoves.PushBack(wb);
}

void MoveDelayer::AddActor (INT Actor)
{
	WaitingActor wb;
	wb.Actor = Actor;
	wb.OutTime=Delayer->CurrentTime+Delayer->DelayTime;
	WaitingActors.PushBack(wb);
}

void MoveDelayer::Restart()
{
	while(!WaitingMoves.Empty()){
		WaitingMoves.PopFront();
	}
	while (!WaitingActors.Empty ()) {
		WaitingActors.PopFront ();
	}
}

/////////////////////////////////

VoiceDelayer vDelayer;

VoiceDelayer::VoiceDelayer(void)
{
}

void VoiceDelayer::Tick(float DeltaSeconds)
{
	while(!WaitingVoices.Empty() && WaitingVoices.Front().OutTime < Delayer->CurrentTime){
		WaitingVoice wv=WaitingVoices.Front();
		WaitingVoices.PopFront();

		if(UtvEngine->PrimaryConnection && UtvEngine->PrimaryConnection->VoiceChannel)
			((UtvVoiceChannel*)UtvEngine->PrimaryConnection->VoiceChannel)->DistributeVoicePacket(&wv.VoiceInfo);

		delete[] wv.VoiceInfo.PacketData;
	}
}

void VoiceDelayer::AddVoice(FVoiceInfo* VoiceInfo)
{
	WaitingVoice wv;
	wv.VoiceInfo = *VoiceInfo;
	wv.VoiceInfo.PacketData=new BYTE[wv.VoiceInfo.PacketSize];
	memcpy(wv.VoiceInfo.PacketData,VoiceInfo->PacketData,wv.VoiceInfo.PacketSize);
	wv.OutTime=Delayer->CurrentTime+Delayer->DelayTime-0.5;
	WaitingVoices.PushBack(wv);
}

void VoiceDelayer::Restart()
{
	while(!WaitingVoices.Empty()){
		delete[] WaitingVoices.Front().VoiceInfo.PacketData;
		WaitingVoices.PopFront();
	}
}

/////////////////////////////////

static inline void SerializeCompVector( FArchive& Ar, FVector& V )
{
	INT X(appRound(V.X)), Y(appRound(V.Y)), Z(appRound(V.Z));
	DWORD Bits = Clamp<DWORD>( appCeilLogTwo(1+Max(Max(Abs(X),Abs(Y)),Abs(Z))), 1, 20 )-1;
	Ar.SerializeInt( Bits, 20 );
	INT   Bias = 1<<(Bits+1);
	DWORD Max  = 1<<(Bits+2);
	DWORD DX(X+Bias), DY(Y+Bias), DZ(Z+Bias);
	Ar.SerializeInt( DX, Max );
	Ar.SerializeInt( DY, Max );
	Ar.SerializeInt( DZ, Max );
	if( Ar.IsLoading() )
		V = FVector((INT)DX-Bias,(INT)DY-Bias,(INT)DZ-Bias);
}

static inline void SerializeCompRotator( FArchive& Ar, FRotator& R )
{
	BYTE Pitch(R.Pitch>>8), Yaw(R.Yaw>>8), Roll(R.Roll>>8), B;
	B = (Pitch!=0);
	Ar.SerializeBits( &B, 1 );
	if( B )
		Ar << Pitch;
	else
		Pitch = 0;
	B = (Yaw!=0);
	Ar.SerializeBits( &B, 1 );
	if( B )
		Ar << Yaw;
	else
		Yaw = 0;
	B = (Roll!=0);
	Ar.SerializeBits( &B, 1 );
	if( B )
		Ar << Roll;
	else
		Roll = 0;
	if( Ar.IsLoading() )
		R = FRotator(Pitch<<8,Yaw<<8,Roll<<8);
}

// passing NULL for UActorChannel will skip recent property update
static inline void SerializeCompressedInitial( FArchive& Bunch, FVector& Location, FRotator& Rotation, UBOOL bSerializeRotation, UActorChannel* Ch )
{
	guardSlow(SerializeCompressedInitial);

    // read/write compressed location
    SerializeCompVector( Bunch, Location );
    if( Ch && Ch->Recent.Num() )
        ((AActor*)&Ch->Recent(0))->Location = Location;

    // optionally read/write compressed rotation
    if( bSerializeRotation )
    {
        SerializeCompRotator( Bunch, Rotation );
	    if( Ch && Ch->Recent.Num() )
            ((AActor*)&Ch->Recent(0))->Rotation = Rotation;
    }
	unguardSlow;
}

/////////////////////////////////

BunchDelayer* Delayer;

BunchDelayer::BunchDelayer(void)
{
	CurrentTime=0;
	DelayTime=0;
	MaxChannelQueSize=10; //was 5
//	actors.SetSize (1024);
	ActorProps ap;
	ap.ActorClass=0;
	ap.Actor=0;
	for (INT i = 0; i <= UNetConnection::MAX_CHANNELS; ++i) {
		actors[i]=ap;
	}

	//Initialize the list of tracked actors intended for watchers (secondary clients)
	WatcherActor wa;
	wa.ActorClass = NULL;
	for (INT i = 0; i <= UNetConnection::MAX_CHANNELS; ++i) {
//		INT a=WActors.AddZeroed();
		WActors[i]=wa;
		OpenChannels[i]=0;
		DelayedOpenChannels[i]=0;
	}
	debugf (TEXT ("Initializing actor property list"));
}

void BunchDelayer::Destroy(void)
{
	guard(BunchDelayer::Destroy);
	Super::Destroy();
	while(!WaitingBunches.Empty()){
		delete WaitingBunches.Front().Bunch;
		WaitingBunches.PopFront();
	}
	for (INT i = 0; i <= UNetConnection::MAX_CHANNELS; ++i) {
		ClearActor (i);
		WActors[i].ActorClass = NULL;
		if (actors[i].Actor)
			delete actors[i].Actor;
	}
	unguard;
	
	for (INT i = 0; i <= UNetConnection::MAX_CHANNELS; ++i) {
		if(OpenChannels[i]!=0){
			delete OpenChannels[i];
			OpenChannels[i]=0;
		}
		if(DelayedOpenChannels[i]!=0){
			delete DelayedOpenChannels[i];
			DelayedOpenChannels[i]=0;
		}
	}
}

void BunchDelayer::Tick(float DeltaSeconds)
{
	guard(BunchDelayer::Tick);

	CurrentTime=appSeconds();
	int sentdata=0;

	while(!WaitingBunches.Empty() && WaitingBunches.Front().OutTime<CurrentTime){
		WaitingBunch wb=WaitingBunches.Front();
		WaitingBunches.PopFront();
		FInBunch* Bunch=wb.Bunch->bunch;

		if(Bunch->bOpen){
			check(DelayedOpenChannels[Bunch->ChIndex]==0);
			DelayedOpenChannels[Bunch->ChIndex]=new OpenPackets;
//			INT a=DelayedOpenChannels[Bunch->ChIndex]->AddZeroed();
			new (*DelayedOpenChannels[Bunch->ChIndex]) InternalBunch(*wb.Bunch);
			OpenChannelOnClients(Bunch->ChIndex);
		} else {
			check(DelayedOpenChannels[Bunch->ChIndex]!=0);
			if(DelayedOpenChannels[Bunch->ChIndex]->Num()<(INT)(MaxChannelQueSize* (wb.Important?8:1))){
//				INT a=DelayedOpenChannels[Bunch->ChIndex]->AddZeroed();
				new (*DelayedOpenChannels[Bunch->ChIndex]) InternalBunch(*wb.Bunch);
			}
		}
		//Parse some extra things
		ParseWatcherProperties (wb.Bunch);

		for( INT i=0; i<UtvEngine->ListenDriver->ClientConnections.Num(); i++ ){
			UNetConnection* Connection=UtvEngine->ListenDriver->ClientConnections(i);
			UReplicatorEngine::OpenConnection* oc = UtvEngine->OpenConnections.Find(Connection);

			//Drop packets for clients not following the primary client
			if ((!Bunch->bClose) && (!Bunch->bOpen) && (wb.Bunch->drop) && (!oc->followPrimary))
				continue;

			if(oc->isReady && Connection!=UtvEngine->PrimaryConnection){
				if(Connection->Channels[Bunch->ChIndex] && Connection->Channels[Bunch->ChIndex]->OpenTemporary && !Connection->Channels[Bunch->ChIndex]->Closing){
					if((*DelayedOpenChannels[Bunch->ChIndex])(0).bunch->bReliable){
						debugf(TEXT("Pre send: OpenTemporary flagged channel with reliable open packet detected %i"),Bunch->ChIndex);
					}
				}
				UtvEngine->SendBunch(Connection,wb.Bunch);
				if(Connection->Channels[Bunch->ChIndex] && Connection->Channels[Bunch->ChIndex]->OpenTemporary && !Connection->Channels[Bunch->ChIndex]->Closing){
					if((*DelayedOpenChannels[Bunch->ChIndex])(0).bunch->bReliable){
						debugf(TEXT("Post send: OpenTemporary flagged channel with reliable open packet detected %i"),Bunch->ChIndex);
						for(TArray<InternalBunch>::TIterator opi(*DelayedOpenChannels[Bunch->ChIndex]);opi;++opi)
							debugf (TEXT ("Data %i %i %i"),opi->bunch->bOpen,opi->bunch->bClose,opi->bunch->bReliable);
					}
				}
				sentdata += wb.Bunch->bunch->GetNumBytes();
			}

			//It the threshold is exceeded, tick the listendriver again. This might prevent some kind of buffer overflow
			if (sentdata > UtvEngine->FlushThreshold) {
				UtvEngine->ListenDriver->TickDispatch (DeltaSeconds);
				UtvEngine->ListenDriver->TickFlush();
				//debugf(TEXT("Running double tick"));
				sentdata = 0;
			}
		}

		if(Bunch->bClose){
			check(DelayedOpenChannels[Bunch->ChIndex]!=0);
			delete DelayedOpenChannels[Bunch->ChIndex];
			DelayedOpenChannels[Bunch->ChIndex]=0;
		}

		delete wb.Bunch;
		UtvEngine->SentClientData=true;
	}
	unguard;
}

void BunchDelayer::ClearActor (DWORD index)
{
	guard (BunchDelayer::ClearActor);

	//Clear any old data since this one is new and fine
	for (INT i = 0; i < WActors[index].Data.Num(); ++i) {
		BYTE* d = WActors[index].Data(i);
		if (d && (d != (BYTE *)1))
			delete[] d;
	}
	WActors[index].Data.Empty ();

	unguard;
}

//If out is null, it will use packagemap which include utv2004c, otherwise not
//Make sure to call SetSerializeRead correctly before calling
//In can also be null, if only writing is wanted. Setserializedread doesn't matter then of course
void BunchDelayer::ParseProperty(UProperty &Prop, BYTE *Data, int Element, FArchive *in, FArchive *out, bool useOffset)
{
	guard(BunchDelayer::ParseProperty);
	int Offset = useOffset ? Prop.Offset : 0;
	BYTE *Dest = Data + Offset + Element * Prop.ElementSize;

	//debugf(TEXT("Parsing property %s"), Prop.GetFullName());

	//If we are reading, clear out first
	if (in)
		appMemzero(Dest, Prop.ElementSize);

	//Need to handle certain properties specially since they (names) dont use the provided serverpackagemap :(
	if (Prop.IsA(UNameProperty::StaticClass())) {
		if (in)
			UtvEngine->ServerPackageMap->SerializeName(*in, *(DWORD *)Dest);
		if (out)
			UtvEngine->ServerPackageMap->SerializeName(*out, *(DWORD *)Dest);
	}

	//Structs can contain names, so need special handling here as well. Will break if the epic special cases change
	else if (Prop.IsA(UStructProperty::StaticClass())) {
		UStruct *Struct = ((UStructProperty *)(&Prop))->Struct;

		//These are special case serialized structs, and dont contain names. So can use default handling
		if ((Struct->GetFName() == NAME_Vector) ||
			(Struct->GetFName() == NAME_Rotator) ||
			(Struct->GetFName() == NAME_CompressedPosition) ||
			(Struct->GetFName() == NAME_Quat) ||
			(Struct->GetFName() == NAME_Plane)) 
		{
			if (in)
				Prop.NetSerializeItem(*in, UtvEngine->ServerPackageMap, Dest);
			if (out)
				Prop.NetSerializeItem(*out, UtvEngine->ServerPackageMap, Dest);
		}
		else {
			//Call ParseProperty on these recursively, since they in turn could be structs etc.
			for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Struct); It; ++It ) {
				if( UtvEngine->ServerPackageMap->ObjectToIndex(*It)!=INDEX_NONE ) {
					for( INT i=0; i<It->ArrayDim; i++ ) {
						ParseProperty(**It, Dest, 0, in, out);
					}
				}
			}
		}
	}
	//Other properties can use the default way
	else {
		if (in)
			Prop.NetSerializeItem(*in, UtvEngine->ServerPackageMap, Dest);
		if (out)
			Prop.NetSerializeItem(*out, UtvEngine->ServerPackageMap, Dest);
	}
	unguard;
}

//Returns an object from the stream. If out != null, uses read-map like ParseProperty
UObject *BunchDelayer::ParseObject(FArchive *in, FArchive *out)
{
	guard(BunchDelayer::ParseObject);

	DWORD MaxIndexRead = out ? UtvEngine->ServerPackageMap->GetMaxObjectIndexRead() : UtvEngine->ServerPackageMap->GetMaxObjectIndexWrite();
	UObject *Object = NULL;

	BYTE B=0; 
	in->SerializeBits(&B, 1);
	if (out)
		out->SerializeBits(&B, 1);

	//Does this object refer to a static or dynamic class? (would not make sense if it was dynamic here though)
	if (!B) {
		DWORD Index = 0;
		in->SerializeInt(Index, MaxIndexRead);
		//debugf(TEXT("Got index %d %d %d %p"), Index, MaxIndexRead, UtvEngine->ServerPackageMap->GetMaxObjectIndexRead(), out);
		if (out)
			out->SerializeInt(Index, UtvEngine->ServerPackageMap->GetMaxObjectIndexWrite());
		Object = UtvEngine->ServerPackageMap->IndexToObject(Index, 1);
		return Object;
	}
	else {
		debugf(TEXT("Creating object from dynamic channel, very strange"));
		return NULL;
	}

	unguard;
}

//If spawn is false, it will only get the bits of the bunch
AActor *BunchDelayer::ParseActor(UClass *ActorClass, bool spawn, FArchive *in, FArchive *out)
{
	guard(BunchDelayer::ParseActor);
	AActor *Actor;
	FVector Location;
	FRotator Rotation(0, 0, 0);

	SerializeCompressedInitial(*in, Location, Rotation, ActorClass->GetDefaultActor()->bNetInitialRotation, NULL);
	if (out)
		SerializeCompressedInitial(*out, Location, Rotation, ActorClass->GetDefaultActor()->bNetInitialRotation, NULL);
	
	//Spawn this actor
	if (spawn) {
		Actor = (AActor *)StaticConstructObject(ActorClass, GetOuter());
		if (!Actor)
			debugf(TEXT("Unable to spawn actor?"));

		return Actor;
	}
	else
		return NULL;

	unguard;
}

bool BunchDelayer::ParseProperties (InternalBunch* IntBunch)
{
	bool interesting = false;
	BYTE Data[32768];

	//return false;

	guard(BunchDelayer::ParseProperties);

	FInBunch* Bunch=IntBunch->bunch;
	FInBunch tmp(*Bunch);
	//FOutBunch out(UtvEngine->ConnectDriver->ServerConnection->Channels[0], false);

	if (UtvEngine->DemoDriver)
		UtvEngine->DemoDriver->SetCloak(true);

	//If we are connected to a proxy, the incoming packets will already be utv2004c-corrected
	FOutBunch *out = NULL;
	if (!UtvEngine->ConnectedToUtvProxy) {
		out = new FOutBunch(Bunch->Connection->Channels[Bunch->ChIndex], false);
		UtvEngine->ServerPackageMap->SetSerializeRead(true);
	}
	else {
		UtvEngine->ServerPackageMap->SetSerializeRead(false);
	}

	if (UtvEngine->DemoDriver)
		UtvEngine->DemoDriver->SetCloak(false);

	guard(HandleOpen);
	//New actor channel?
	if (tmp.bOpen) {
		//Mark it as broken/unused first in case we fail to interpret it
		actors[tmp.ChIndex].ActorClass = NULL;
		actors[tmp.ChIndex].Actor = NULL;

		UObject *Object = ParseObject(&tmp, out);
		AActor *InActor = Cast<AActor>(Object);

		//Is this a transient actor? If so, create it
		if (!InActor) {
			UClass *ActorClass = Cast<UClass>(Object);

			if (ActorClass) {
				actors[tmp.ChIndex].Actor = ParseActor(ActorClass, true, &tmp, out);
				actors[tmp.ChIndex].ActorClass = ActorClass;

				//Make sure SeeAll is enabled on the server if the utv server thinks it is
				if ((tmp.ChIndex == 1) && (UtvEngine->SeeAll) && (!UtvEngine->DemoDriver)) {
					if (!appStrfind(actors[tmp.ChIndex].Actor->GetName(), TEXT("UTV"))) {
						debugf(TEXT("------------------------------------"));
						debugf(TEXT("Warning: SeeAll is enabled, but the game server is not running UTV2004S!"));
						debugf(TEXT("SeeAll will most likely not work as intended. Consider restarting without"));
						debugf(TEXT("SeeAll enabled or installing UTV2004S on the game server."));
						debugf(TEXT("Controller is %d %s"), tmp.ChIndex, actors[tmp.ChIndex].Actor->GetName());
						debugf(TEXT("------------------------------------"));
					}
				}
			}
			else {
				debugf(TEXT("Null actor in parseproperties! %s"), Object->GetFullName());
				return false;
			}
		}
		else {
			actors[tmp.ChIndex].Actor = InActor;
			actors[tmp.ChIndex].ActorClass = InActor->GetClass();
		}
	}
	unguard;

	//Now process properties if we have a valid actor
	guard(ProcessProperties);
	if (actors[tmp.ChIndex].Actor) {
		FClassNetCache* ClassCache = UtvEngine->ServerPackageMap->GetClassNetCache(actors[tmp.ChIndex].ActorClass);
		INT             RepIndex   = tmp.ReadInt( ClassCache->GetMaxIndex() );
		FFieldNetCache* FieldCache = tmp.IsError() ? NULL : ClassCache->GetFromIndex( RepIndex );
		
		//if (tmp.ChIndex == 1)
			//debugf (TEXT ("Now parsing actor %s"), (actors[tmp.ChIndex].Actor)->GetName ());

		while( FieldCache ) {

			//Only write the repindex if there was an actual field
			if (out)
				out->WriteInt(RepIndex, ClassCache->GetMaxIndex());

			if(actors[tmp.ChIndex].PropertiesUsed.Find(RepIndex)==0){
				actors[tmp.ChIndex].PropertiesUsed.Set(RepIndex,1);	//the 1 doesnt matter
				//debugf(TEXT("New property %i on channel %i"),RepIndex,tmp.ChIndex);
				interesting = true;
			}
			
			//Parse the data so we can check for new properties
			UProperty* It;			

			if( FieldCache && (It=FlagCast<UProperty,CLASS_IsAUProperty>(FieldCache->Field))!=NULL )
			{		
				//if (tmp.ChIndex == 1)
				//	debugf (TEXT ("PP: Now checking field %s"), It->GetFullName());

				//When doing seeall with a primary client, we need to drop certain bunches
				if ((UtvEngine->SeeAll) && (!UtvEngine->NoPrimary) && (tmp.ChIndex == 1)) {
					if (appStrfind(It->GetName(), TEXT("TargetViewRotation"))) {
						IntBunch->drop = true;
					}
				}

				//Is it an array property? get index if so
				BYTE Element=0;
				if( It->ArrayDim != 1 ){
					tmp << Element;
					if (out)
						*out << Element;
				}

				// Receive property. Just throw away the value.. we only want the bits to get off the bunch
				guard(ReceiveProperty);

				UtvEngine->ServerPackageMap->ClearDepend ();

				ParseProperty(*It, Data, Element, &tmp, out, false);

				//check for dependency
				/*if (UtvEngine->ServerPackageMap->GetDepend () > 0) {
//					INT a=IntBunch->dependsOn.AddZeroed();
					
					Removed for now
					//new(IntBunch->dependsOn) INT(UtvEngine->ServerPackageMap->GetDepend ());

					//debugf (TEXT ("Now checking field %s"), It->GetFullName());
					//debugf (TEXT ("Depending on %i"), UtvEngine->ServerPackageMap->GetDepend ());
				}*/

				unguard;
			} 
			else {
				//This bunch contains RPC, flag this
				//debugf(TEXT("RPC Comparison: %d %d"), tmp.GetNumBits(), out.GetNumBits());
				IntBunch->hasRPC = true;
	
				guard(ParseFunction);
				//Now we actually parse this!
				FName Message = FieldCache->Field->GetFName();
				UFunction* Function = actors[tmp.ChIndex].Actor->FindFunction( Message );
				// Get the parameters.
//				FMemMark Mark(GMem);

				if (Function) {

					//if (tmp.ChIndex == 1)
					//	debugf(TEXT("Got rpc: %s"), Function->GetFullName());

					//When doing seeall with a primary client, we need to drop certain bunches
					if ((UtvEngine->SeeAll) && (!UtvEngine->NoPrimary) && (tmp.ChIndex == 1)) {
						if (appStrfind(Function->GetName(), TEXT("ShortClientAdjustPosition"))) {
							IntBunch->drop = true;
						}
					}

					//appMemzero (Data, Function->ParmsSize);

					guard(GetParams);
					//BYTE* Parms = new(GMem,MEM_Zeroed,Function->ParmsSize)BYTE;
					for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
						if( UtvEngine->ServerPackageMap->ObjectToIndex(*It)!=INDEX_NONE )
							if( FlagCast<UBoolProperty,CLASS_IsAUBoolProperty>(*It) || tmp.ReadBit() ) {
								//It->NetSerializeItem(tmp,UtvEngine->ServerPackageMap,Data+It->Offset);

								if (!(FlagCast<UBoolProperty,CLASS_IsAUBoolProperty>(*It)))
									if (out)
										out->WriteBit(1);
								//It->NetSerializeItem(out,UtvEngine->ServerPackageMap,Data+It->Offset);

								ParseProperty(**It, Data, 0, &tmp, out);
							} 
							else {
								if (out)
									out->WriteBit(0);
							}
					unguard;
	
					//Mark.Pop();

				}
				else {
					debugf(TEXT("Could not find function %s"), *Message);
				}
				unguard;
				//return interesting;				
			}

			// Get next
			RepIndex   = tmp.ReadInt( ClassCache->GetMaxIndex() );
			FieldCache = tmp.IsError() ? NULL : ClassCache->GetFromIndex( RepIndex );
		}
	}
	unguard;

	//Replace the inbunch with our new fresh one if we have created one
	if (out) {
		if (out->IsError())
			debugf(TEXT("New bunch iserror"));

		FBitReader br(out->GetData(), out->GetNumBits());
		IntBunch->bunch->SetData(br, br.GetNumBits());

		delete out;
	}

	//Closing packet?
	guard(HandleClose);
	if (tmp.bClose) {
		//memory will be cleared when reused.. should perhaps move here.
		actors[tmp.ChIndex].ActorClass = NULL;
		if (actors[tmp.ChIndex].Actor)
			delete actors[tmp.ChIndex].Actor;
		actors[tmp.ChIndex].Actor = NULL;
		actors[tmp.ChIndex].PropertiesUsed.Empty();
	
		//Closing means it is not really interesting since the channel dies now anyway
		return false;
	}
	unguard;

	return interesting;
	
	unguard;
}

void BunchDelayer::ParseWatcherProperties (InternalBunch* IntBunch)
{
	guard(BunchDelayer::ParseWatcherProperties);

	FInBunch* Bunch=IntBunch->bunch;
	FInBunch tmp(*Bunch);

	//If watching a demo, tmp will incorrectly be flagged as having unpacked data
	if (UtvEngine->DemoDriver) {
		UtvEngine->DemoDriver->SetCloak(true);
		FInBunch newtmp(Bunch->Connection);
		newtmp.ChIndex = tmp.ChIndex;
		newtmp.bOpen = tmp.bOpen;
		newtmp.bClose = tmp.bClose;
		newtmp.bReliable = tmp.bReliable;
		newtmp.SetData(tmp, tmp.GetNumBits());
		UtvEngine->DemoDriver->SetCloak(false);
		tmp = newtmp;
	}

	UtvEngine->ServerPackageMap->SetSerializeRead(false);	

	//New actor channel?
	if (tmp.bOpen) {
		//Mark it as broken/unused first in case we fail to interpret it
		WActors[tmp.ChIndex].ActorClass = NULL;

		UObject *Object = ParseObject(&tmp, NULL);
		AActor *InActor = Cast<AActor>(Object);

		//Is this a transient actor? If so, create it
		if (!InActor) {
			UClass *ActorClass = Cast<UClass>(Object);

			if (ActorClass) {
				ParseActor(ActorClass, false, &tmp, NULL);
				WActors[tmp.ChIndex].ActorClass = ActorClass;
			}
			else {
				debugf(TEXT("Null actor in parsewatcherproperties!"));
				return;
			}
		}
		else {
			//We only care about transients actors
			return;
		}

		//Check if this is a class we want to do things with
		UClass* actor=WActors[tmp.ChIndex].ActorClass;
		if (!actor)
			return;
		if ((actor->IsChildOf(AReplicationInfo::StaticClass())) ||
			(actor->IsChildOf(APawn::StaticClass()))) {
			//more readable if-statement
		}
		else {
			WActors[tmp.ChIndex].ActorClass = NULL;
			return;
		}

		//Initialize the properties array
		if (WActors[tmp.ChIndex].ActorClass != NULL) {
			
			guard (FullTrackInit);
			ClearActor (tmp.ChIndex);

			//And initalize a new list to null
			FClassNetCache* ClassCache = UtvEngine->ServerPackageMap->GetClassNetCache(WActors[tmp.ChIndex].ActorClass);
			for (int i = 0; i <= ClassCache->GetMaxIndex (); ++i) {
				WActors[tmp.ChIndex].Data.AddItem (NULL);
			}

			//debugf (TEXT ("Parse: started channel %i: %s"), tmp.ChIndex, WActors[tmp.ChIndex].ActorClass->GetName ());

			unguard;
		}
	}

	//Closing packet?
	if (tmp.bClose) {
		guard (CloseChannel);
		WActors[tmp.ChIndex].ActorClass = NULL;
		ClearActor (tmp.ChIndex);		

		//No need do do anything more with this bunch
		return;

		unguard;
	}

	//Now process properties if we have a valid actor
	if (WActors[tmp.ChIndex].ActorClass) {
		guard (BigParse);
		FClassNetCache* ClassCache = UtvEngine->ServerPackageMap->GetClassNetCache(WActors[tmp.ChIndex].ActorClass);
		INT             RepIndex   = tmp.ReadInt( ClassCache->GetMaxIndex() );
		FFieldNetCache* FieldCache = tmp.IsError() ? NULL : ClassCache->GetFromIndex( RepIndex );
		
		while( FieldCache ) {

			guard (FoundReplicationIndex);
			
			//Parse the data so we can check for new properties
			UProperty* It;			

			if( FieldCache && (It=FlagCast<UProperty,CLASS_IsAUProperty>(FieldCache->Field))!=NULL )
			{		
				guard (FoundProperty);
				//Is it an array property? get index if so
				BYTE Element=0;
				if( It->ArrayDim != 1 ){
					tmp << Element;
				}
				//Find offset and an area to put it
				INT Offset = Element * It->ElementSize;				
				BYTE *Data;
				
				Data = WActors[tmp.ChIndex].Data(RepIndex);
				if (!Data) {
					Data = new BYTE[It->ElementSize * It->ArrayDim];
					appMemzero( Data, It->ElementSize * It->ArrayDim);		//suck, kraschar utan denna :)
					WActors[tmp.ChIndex].Data(RepIndex) = Data;
				}

				ParseProperty(*It, Data, Element, &tmp, NULL, false);
				
				//Now save this value to our allocated place

				//appMemzero( Data, It->ElementSize * It->ArrayDim);		//suck, kraschar utan denna :)

				//debugf (TEXT ("PW: Now checking field %s"), It->GetFullName ());
				//It->NetSerializeItem (tmp, UtvEngine->ServerPackageMap, Data + Offset);

				/*
				if (It->IsA(UBoolProperty::StaticClass())) {
					INT b = *(char *)(Data + Offset);
					UBoolProperty *bp = (UBoolProperty *)It;
					debugf(TEXT("Value: %i (%i)"), b, bp->BitMask);
				}
				if (It->IsA(UStrProperty::StaticClass())) {
					debugf(TEXT("Value: %s"), **(FString *)(Data + Offset));
				}
				if (It->IsA(UFloatProperty::StaticClass())) {
					debugf(TEXT("Value: %f"), *(float *)(Data + Offset));
				}
				if (It->IsA(UIntProperty::StaticClass())) {
					debugf(TEXT("Value: %i"), *(int *)(Data + Offset));
				}
				*/

				//This is used by the master server component
				guard(ExtractName);
				if (appStrfind(It->GetName(), TEXT("PlayerName"))) {
					FString *Buffer = (FString *)(Data + Offset);
					PlayerInfo p;
					p.Name = *Buffer;
					players.Set(tmp.ChIndex, p);
				}
				unguard;
				unguard;
			} 
			else {
				//We only care about properties
				return;				
			}

			// Get next
			RepIndex   = tmp.ReadInt( ClassCache->GetMaxIndex() );
			FieldCache = tmp.IsError() ? NULL : ClassCache->GetFromIndex( RepIndex );

			unguard;
		}
		unguard;
	}
	
	unguard;
}

void BunchDelayer::AddBunch(FInBunch* Bunch)
{
	guard(BunchDelayer::AddBunch);

	WaitingBunch wb;
	InternalBunch* ib=new InternalBunch;
	ib->bunch=new FInBunch(*Bunch);

	//debugf(TEXT("--start--"));

	//This call is important, it creates the outbunch that is actually sent. So it can not be skipped!
	bool important=ParseProperties(ib);

	guard(Start);
	wb.Bunch=ib;
	wb.OutTime=CurrentTime+DelayTime;
	wb.Important = important;
	
	//ParseWatcherProperties(wb.Bunch); //test

	unguard;
	

	WaitingBunches.PushBack(wb);

	check(Bunch);

	guard(Primary);
	if(UtvEngine->PrimaryConnection){
		if(Bunch->bOpen){
			check(OpenChannels[Bunch->ChIndex]==0);
			OpenChannels[Bunch->ChIndex]=new OpenPackets;
//			INT a=OpenChannels[Bunch->ChIndex]->AddZeroed();
			new(*OpenChannels[Bunch->ChIndex]) InternalBunch(*ib);
			OpenChannel(UtvEngine->PrimaryConnection,Bunch->ChIndex);
		} else {
			check(OpenChannels[Bunch->ChIndex]!=0);
			if(OpenChannels[Bunch->ChIndex]->Num()<(INT)MaxChannelQueSize*(wb.Important?8:1) && Bunch->bReliable){
//				INT a=OpenChannels[Bunch->ChIndex]->AddZeroed();
				new(*OpenChannels[Bunch->ChIndex]) InternalBunch(*ib);
			}
		}
		if(Bunch->bClose){
			delete OpenChannels[Bunch->ChIndex];
			OpenChannels[Bunch->ChIndex]=0;
		}
		UtvEngine->SendBunch(UtvEngine->PrimaryConnection,wb.Bunch);
	}
	unguard;

	//mu
	//ParseProperties (wb.Bunch, false);
	unguard;
}

void BunchDelayer::NewConnection(UNetConnection* Connection)
{
	guard(BunchDelayer::NewConnection);

	//Create channels and send initialization packets
	for(int a=0;a<=UNetConnection::MAX_CHANNELS;++a){
		if(DelayedOpenChannels[a])
			OpenChannel(Connection,a);
	}

	for(int a=0;a<=UNetConnection::MAX_CHANNELS;++a){
		if(DelayedOpenChannels[a])
			UtvEngine->SendBunch(Connection,&((*DelayedOpenChannels[a])(0)));
	}

	for(int a=0;a<=UNetConnection::MAX_CHANNELS;++a){
		if(DelayedOpenChannels[a]){
			for(INT i=1;i<DelayedOpenChannels[a]->Num();++i) {
				InternalBunch* ib = &(*DelayedOpenChannels[a])(i);
				if (!ib->hasRPC)
					UtvEngine->SendBunch(Connection,&(*DelayedOpenChannels[a])(i));
			}
		}
	} 

	//Create the fake actor
	UtvEngine->CreateFakeActor (Connection);

	//Now we are ready to send bunches containing RPC as well
	for(int a=0;a<=UNetConnection::MAX_CHANNELS;++a){
		if(DelayedOpenChannels[a]){
			for(INT i=1;i<DelayedOpenChannels[a]->Num();++i) {
				InternalBunch* ib = &(*DelayedOpenChannels[a])(i);
				if (ib->hasRPC)
					UtvEngine->SendBunch(Connection,&(*DelayedOpenChannels[a])(i));
			}
		}
	} 

	//Now send out all recorded properties that we are sitting on
	guard (SendRecorded);
	for (INT i = 0; i <= UNetConnection::MAX_CHANNELS; ++i) {
		if (WActors[i].ActorClass) {
			FClassNetCache* ClassCache = UtvEngine->ServerPackageMap->GetClassNetCache(WActors [i].ActorClass);

			//Create ourselves a bunch
			FOutBunch* out = new FOutBunch (Connection->Channels[i], false);
			out->bOpen = false;
			out->bClose = false;
			out->bReliable = true;
			UBOOL written = 0;

			//debugf (TEXT ("Sending replication for %i: %s"), i, WActors[i].ActorClass->GetName ());

			//Now go through all properties to see which have data to send
			for (INT j = 0; j < WActors[i].Data.Num (); ++j) {
			
				//Only send properties that have information
				BYTE* Data = WActors[i].Data(j);
				if (Data) {
					//Obtain the field cache
					FFieldNetCache* FieldCache = ClassCache->GetFromIndex (j);
					UProperty* It;

					//A regular property?
					if ((It=FlagCast<UProperty,CLASS_IsAUProperty>(FieldCache->Field))!=NULL) {
						//debugf (TEXT ("Property %i: %s"), j, It->GetName ());

						if (It->ArrayDim != 1) {
							DWORD Index = j;
							for (INT k = 0; k < It->ArrayDim; ++k) {
	
								//Only send array elements that are set to something
								if (!appMemIsZero (Data + (k * It->ElementSize), It->ElementSize)) {
									BYTE el = k;
									out->SerializeInt (Index, ClassCache->GetMaxIndex ());
									*out << el;
									//It->NetSerializeItem (*out, UtvEngine->ServerPackageMap, Data);								
									ParseProperty(*It, Data, el, NULL, out, false);
									written = true;
								}
	
								//Data += It->ElementSize; 
							} 
						}
						else {
							DWORD Index = j;
							out->SerializeInt (Index, ClassCache->GetMaxIndex ());
							//It->NetSerializeItem (*out, UtvEngine->ServerPackageMap, Data);
							ParseProperty(*It, Data, 0, NULL, out, false);
							written = true;
						}

					}
				}			

				if (out->GetNumBits() > 200) {
					UtvEngine->SendBunch (Connection, out);
					written = false;
					delete out;
					out = new FOutBunch (Connection->Channels[i], false);
					out->bOpen = false;
					out->bClose = false;
					out->bReliable = true;
				}
			}

			if (written) {
				UtvEngine->SendBunch (Connection, out);
			}
			delete out;
		}
	}
	unguard;

	unguard;	
}

//Requires that the channel has been added to the correct openchannels map before this is called
void BunchDelayer::OpenChannel(UNetConnection* Connection,INT ChIndex,bool SendQuedBunches)
{
	OpenPackets* op;
	if(Connection==UtvEngine->PrimaryConnection)
		op=OpenChannels[ChIndex];
	else
		op=DelayedOpenChannels[ChIndex];

	if(!op){
//		if(Connection==UtvEngine->PrimaryConnection){
//			printf("Attempt to open channel without start packet %i\n",ChIndex);
//		}
//		new(UtvEngine->OpenConnections.Find(Connection)->ChannelHistory[ChIndex]) FString("Opening channel without start packet");
		return;
	}

	if(op){
		UtvEngine->OpenChannel(Connection,(*op)(0).bunch);
		if(SendQuedBunches){
			for(INT i=0;i<op->Num();++i)
				UtvEngine->SendBunch(Connection,&(*op)(i));
		}
	}
}

void BunchDelayer::OpenChannelOnClients(int ChIndex)
{
	for( INT i=0; i<UtvEngine->ListenDriver->ClientConnections.Num(); i++ ){
		UNetConnection* Connection=UtvEngine->ListenDriver->ClientConnections(i);
		if(UtvEngine->OpenConnections.Find(Connection)->isReady && Connection!=UtvEngine->PrimaryConnection){
			OpenChannel(Connection,ChIndex);
		}
	}
}

IMPLEMENT_CLASS(BunchDelayer);
