#ifndef REPLICATIONENGINE
#define REPLICATIONENGINE

#include "UtvChannel.h"
#include "UTVUplink.h"

#include "UnForcePacking_begin.h"

class UUTVPackageMap;
class UtvChannel;
class UNetConnection;
class FInBunch;
class UTVRemoteControll;

//Our own simple stats tracker
class UUTVStats
{
public:
	void Clear ();
	void Add (INT Index, INT Time);
	void Show ();
protected:
	INT Times[20];
};

extern UUTVStats* UTVStats;

template< class T > class UTVList	//uses limited std::deque syntax+unreal style iterator
{
	class UTVListNode
	{
	public:
		UTVListNode* Next;
		UTVListNode* Prev;
		T* Payload;
		UTVListNode() :	Next(this),Prev(this),Payload(0) {}
		~UTVListNode() {Unlink();delete Payload;}

		void Unlink()
		{
			Next->Prev = Prev;
			Prev->Next = Next;
			Prev = this;
			Next = this;
		}
		void Link( UTVListNode* NextCell )
		{
			Next->Prev = NextCell->Prev;
			NextCell->Prev->Next = Next;
			NextCell->Prev = this;
			Next = NextCell;
		}
	};

public:
	UTVListNode* Head;
	INT num;

	UTVList()
	{
		Head=new UTVListNode();
		num=0;
	}
	~UTVList()
	{
		while(!Empty())
			PopFront();
		delete Head;
	}

	T& Front(){ return *(Head->Next->Payload);}
	void PopFront(){ num--;delete Head->Next;}
	bool Empty(){return Head->Next==Head;}
	void PushBack(const T& elem)
	{
		UTVListNode* node=new UTVListNode;
		node->Payload=new T(elem);
		node->Link(Head);
		num++;
	}
	INT Size(){return num;};

	// Iterator.
	class TIterator
	{
	public:
		TIterator( UTVList<T>& InList ) : List(InList), Node(InList.Head) { ++*this;															}
		void operator++()      { Node=Node->Next;																																	}
		void operator--()      { Node=Node->Prev;																																	}
		void RemoveCurrent()   { List.num--;Node=Node->Prev;delete Node->Next;																		}
		operator UBOOL() const { return Node != List.Head;																												}
		T& operator*()   const { return *Node->Payload;																									          }
		T* operator->()  const { return Node->Payload;																														}
		T& GetCurrent()  const { return *Node->Payload;																														}
		T& GetPrev()     const { return Node->Prev==List.Head ?  *Node->Prev->Prev->Payload: *Node->Prev->Payload;}
		T& GetNext()     const { return Node->Next==List.Head ?  *Node->Next->Next->Payload: *Node->Next->Payload;}
	private:
		UTVList<T>& List;
		UTVListNode* Node;
	};

};

struct InternalBunch{
	FInBunch* bunch;
	bool hasRPC;
	bool drop;					//If true, this bunch will not be sent to clients (used by seeall)
	TArray<int> dependsOn;
	InternalBunch(){
		bunch=0;
		hasRPC=false;
		drop=false;
	}
	InternalBunch(const InternalBunch& ib){
		guard(InternalBunch::CopyConstructor);
		bunch=new FInBunch(*(ib.bunch));
		dependsOn=ib.dependsOn;
		hasRPC=ib.hasRPC;
		drop=ib.drop;
		unguard;
	};
	InternalBunch& operator=(const InternalBunch &ib){
		guard(InternalBunch::Assignment);
		delete bunch;
		bunch=new FInBunch(*(ib.bunch));
		dependsOn=ib.dependsOn;
		hasRPC=ib.hasRPC;
		drop=ib.drop;
		return *this;
		unguard;
	};
	~InternalBunch(){
		delete bunch;
	}
};

class UReplicatorEngine : public UEngine, public FNetworkNotify
{
public:
	DECLARE_CLASS(UReplicatorEngine,UEngine,CLASS_Transient,utv2004)
	struct OpenConnection{
		bool primaryConnection;
		bool vipConnection;
		bool utvProxy;
		bool isReady;
		bool doRemove;
		bool loggedIn;
		bool followPrimary;
		FString username;
		int numErrs;
		int	TotalSubClients;
		TMap<INT,INT> waitingChannels;		//used as a set, value is not used
		struct StalledChannel{
			UTVList<InternalBunch> bunches;
		};
		TMap<int,StalledChannel> StalledChannels;
		TArray<FString> ChannelHistory[UNetConnection::MAX_CHANNELS+1];
	};
	TMap<UNetConnection*,OpenConnection> OpenConnections;


	UNetConnection* PrimaryConnection;
	bool ServerConnReady;
	bool DoRestart;
	float DoRestartIn;
	float MsgTimeOut;
	bool GameEnded;
	FString ServerAdress;
	INT ServerPort;
	INT ListenPort;
	FString PrimaryPassword;
	FString NormalPassword;
	FString VipPassword;
	FString JoinPassword;
	FString iniFile;
	int MaxClients;
	INT TotalClients;
	INT TotalClientsLocal;
	float TotalDelay;
	float LastTotalClientsUpdate;
	bool SentClientData;
	int IgnoreChainedChat;
	TArray<FString> DownloadManagers;
	TArray<FString> ServerDLManagers;
	TArray<FDownloadInfo> RetryManagers;
	int needStats;
	int clockInterval;
	int tickRate;
	int FlushThreshold;
	int noPrimaryNetspeed;
	FString CDKey;
	UTVUplink* uplink;
	FServerResponseLine ServerState;
	UBOOL useMaster;
	UBOOL replyQueries;

	//0=wait for primary client
	//1=primary client sent login
	//2=we have sent hello to server
	//3=server sent welcome+uses
	//4=we sent login+uses to primary
	//5=primary sent join
	//6=we sent join to server
	int ConnectStatus;

	typedef UTVList<UNetConnection*> LoginWaitQue;
	LoginWaitQue WaitingAfterLogin;

	INT SeeAll;
	INT NoPrimary;
	INT DelayPrimaryVoice;
	bool ConnectedToUtvProxy;
	bool TestedForUtvProxy;
	int LoggedInPlayers;	//players that has come the login stage
	
	FURL			LastURL;
	class ULinkerLoad* ClientPackage;
	class UUTVPackageMap*	ServerPackageMap;
	//class UDemoRecDriver* DemoDriver;

	class UNetDriver*	ListenDriver;
	class UNetDriver*	ConnectDriver;
	class UDemoPassthroughDriver* DemoDriver;
	UTVRemoteControll* RemoteController;

	TCHAR ServerMap[NAME_SIZE];
	TCHAR GameType[NAME_SIZE];

	UReplicatorEngine ();
	void Destroy();

	void Init(FString IniFileName);
//	void Exit();
	void Tick( FLOAT DeltaSeconds );	
//	void StaticConstructor();

	// UEngine interface.
	void Draw( UViewport* Viewport, UBOOL Blit=1, BYTE* HitData=NULL, INT* HitSize=NULL );
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	void MouseDelta( UViewport*, DWORD, FLOAT, FLOAT );
	void MousePosition( class UViewport*, DWORD, FLOAT, FLOAT );
	void MouseWheel( UViewport* Viewport, DWORD Buttons, INT Delta );
	void Click( UViewport*, DWORD, FLOAT, FLOAT );
	void UnClick( UViewport*, DWORD, INT, INT );
	void SetClientTravel( UPlayer* Viewport, const TCHAR* NextURL, UBOOL bItems, ETravelType TravelType );
	FLOAT GetMaxTickRate();
	
// FNetworkNotify interface.

	INT ChallengeResponse( INT Challenge );
	EAcceptConnection NotifyAcceptingConnection();
	void NotifyAcceptedConnection( class UNetConnection* Connection );
	UBOOL NotifyAcceptingChannel( class UChannel* Channel );
	ULevel* NotifyGetLevel();
	void NotifyReceivedText( UNetConnection* Connection, const TCHAR* Text );
	void NotifyReceivedFile( UNetConnection* Connection, INT PackageIndex, const TCHAR* Error, UBOOL Skipped, INT attempt );
	UBOOL NotifySendingFile( UNetConnection* Connection, FGuid GUID );

	void NotifyProgress(  const TCHAR* CmdStr, const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds ); 
public:
	void OpenChannel(UNetConnection* Connection,FInBunch* Bunch);
	bool SendBunch(UNetConnection* Connection,InternalBunch* IntBunch,bool fromQue=false);
	void SendBunch(UNetConnection* Connection,FInBunch* InBunch);
	void SendBunch(UNetConnection* Connection,FOutBunch* OutBunch);
	void Restart(void);

	void CreateFakeActor (UNetConnection* Connection);
	void ParseReplicationBunch (UNetConnection* Connection, FInBunch* tempBunch);
	void SendMessageToClient (UNetConnection* Connection, const FString& msg);
	void SendActorToClient (UNetConnection* Connection, INT Actor);
	void SendStatusToClient (UNetConnection* Connection);
	void ParseCmdLine(const TCHAR* Parms);
	void SendDownloadManagers (UNetConnection* Connection);
	void InitServerState (const FString iniName);
	void ResetServerState();
	void UpdateServerState();
	void InitListenDriver();
};
extern UReplicatorEngine* UtvEngine;

#include "UnForcePacking_end.h"

#endif
