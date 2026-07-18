#include "UTVRemoteControll.h"
#include "BunchDelayer.h"
#include "UTVPackageMap.h"

// Win64 compiler complains about implicit size_t to INT conversion in plain
//  strlen(), since it could in theory lose bits, but we probably won't have
//  a >4GB string in here.  :)  --ryan.
static inline INT istrlen(const char *str)
{
    return((INT) strlen(str));
}

UTVRemoteControll::UTVRemoteControll(INT port)
{
	BindPort(port);
}

UTVRemoteControll::~UTVRemoteControll(void)
{
}

void UTVRemoteControll::Tick(void)
{
	Poll();
	while(WaitingClients.Size()>10)
		WaitingClients.PopFront();
}

void UTVRemoteControll::OnReceivedData( FIpAddr SrcAddr, BYTE* Data, INT Count )
{
	if(Data[0]==255 && Data[1]==255 && Data[2]==255 && Data[3]==255){
		//debugf(TEXT("Got 0xff udp data from %s"), *SrcAddr.GetString(true));

		if(Count>=14 && memcmp(&Data[4],"challenge rcon\n",14)==0){
			DWORD Challenge=rand();		//fix?

			WaitingClient wc;
			wc.address=SrcAddr;
			wc.ChallengeGiven=Challenge;
			WaitingClients.PushBack(wc);

			char Response[50];
			sprintf(Response,"xxxxchallenge rcon %u\n",Challenge);
			*(DWORD*)Response=0xffffffff;
			SendTo(SrcAddr,(BYTE*)Response,istrlen(Response));
		} else if(Count>=9 && memcmp(&Data[4],"rcon ",5)==0){
			char* tempBuf=new char[Count];
			memcpy(tempBuf,&Data[9],Count-9);
			tempBuf[Count-9]=0;
			FString s(tempBuf);
			delete[] tempBuf;

			INT pos;
			FString Challenge;
			for(pos=0;pos<Count-9;++pos){
				if(s[pos]==TEXT(' '))
					break;
				Challenge+=s.Mid(pos,1);
			}
			DWORD remoteChallenge=appAtoi(*Challenge);

			bool challengeOk=false;
			for(UTVList<WaitingClient>::TIterator wci(WaitingClients);wci;++wci){
				if(wci->address==SrcAddr && wci->ChallengeGiven==remoteChallenge){
					challengeOk=true;
					break;
				}
			}
			if(!challengeOk) {
				debugf(TEXT("Rcon: Client %s failed challenge"), *SrcAddr.GetString(false));
				return;
			}
			FString password;
			if(s[++pos]!='\"')
				return;
			for(++pos;pos<Count-9;++pos){
				if(s[pos]==TEXT('\"'))
					break;
				password+=s.Mid(pos,1);
			}
			if(password!=UtvEngine->PrimaryPassword) {
				debugf(TEXT("Rcon: Client %s sent bad password (%s)"), *SrcAddr.GetString(false), *password);
				return;
			}

			FString command=s.Mid(pos + 2);
			debugf(TEXT("Rcon: Client %s sent command: %s"), *SrcAddr.GetString(false), *command);

			//Check if the command is a special remotecommand and not a regular update url
			//is fstring overflow safe? probably
			if ((command[0] == 'g') && (command[1] == 'e') && (command[2] == 't')) {
				//Need to convert the string values to non unicode
				char ServerAddr[100];
				char PrimaryPass[100];
				char NormalPass[100];
				char VipPass[100];
				char JoinPass[100];
				FToChar(UtvEngine->ServerAdress, ServerAddr, 100);
				FToChar(UtvEngine->PrimaryPassword, PrimaryPass, 100);
				FToChar(UtvEngine->NormalPassword, NormalPass, 100);
				FToChar(UtvEngine->VipPassword, VipPass, 100);
				FToChar(UtvEngine->JoinPassword, JoinPass, 100);

				//Construct the reply
				char Response[1000];
				sprintf(Response,"xxxxstatus serveraddress=%s?serverport=%i?listenport=%i?delay=%f?"
					"maxclients=%i?primarypassword=%s?normalpassword=%s?vippassword=%s?joinpassword=%s?"
					"seeall=%i?noprimary=%i?ignorechainedchat=%i?state=%i?count=%i",
					ServerAddr, UtvEngine->ServerPort, UtvEngine->ListenPort, Delayer->DelayTime,
					UtvEngine->MaxClients, PrimaryPass, NormalPass, VipPass, JoinPass,
					UtvEngine->SeeAll, UtvEngine->NoPrimary, UtvEngine->IgnoreChainedChat, 
					UtvEngine->ConnectStatus, UtvEngine->TotalClientsLocal);
				*(DWORD*)Response=0xffffffff;

				//pass it on
				SendTo(SrcAddr,(BYTE*)Response,istrlen(Response));
			}
			else if ((command[0] == 'v') && (command[1] == 'e') && (command[2] == 'r')) {
				FString v(UTVVERSION);
				char Ver[100];
				FToChar(v, Ver, 100);
				char Response[1000];
				//The "1" means remote protocol 1.. will probably never change
				sprintf(Response,"xxxxversion 1 %s", Ver);
				*(DWORD*)Response=0xffffffff;
				SendTo(SrcAddr,(BYTE*)Response,istrlen(Response));
			}
			else if ((command[0] == 'c') && (command[1] == 'u') && (command[2] == 'r')) {
				char Response[1000];
				sprintf(Response,"xxxxstate %i %i", UtvEngine->ConnectStatus, UtvEngine->TotalClientsLocal);
				*(DWORD*)Response=0xffffffff;
				SendTo(SrcAddr,(BYTE*)Response,istrlen(Response));
			}
			else {
				UtvEngine->ParseCmdLine(*command);			
				char Response[1000];
				sprintf(Response,"xxxxsetok");
				*(DWORD*)Response=0xffffffff;
				SendTo(SrcAddr,(BYTE*)Response,istrlen(Response));
			}
		}
	}
}

//De-unicodes an fstring..
void UTVRemoteControll::FToChar(const FString &f, char *outbuf, int max)
{
	int i;
	for (i = 0; (i < f.Len()) && (i < (max - 1)); ++i) {
		outbuf[i] = f[i];
	}
	outbuf[i] = 0;
}
