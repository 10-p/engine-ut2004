/*=============================================================================
	UnIpDrvNative.cpp: Native function lookup table for static libraries.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Brandon Reinhart
=============================================================================*/

#include "UnIpDrv.h"
#include "UnIpDrvCommandlets.h"
#include "UnIpDrvNative.h"

#if __STATIC_LINK
/*
AInternetLinkNativeInfo GIpDrvAInternetLinkNatives[] =
{
	MAP_NATIVE(AInternetLink, execGetLocalIP)
	MAP_NATIVE(AInternetLink, execStringToIpAddr)
	MAP_NATIVE(AInternetLink, execIpAddrToString)
	MAP_NATIVE(AInternetLink, execGetLastError)
	MAP_NATIVE(AInternetLink, execResolve)
	MAP_NATIVE(AInternetLink, execParseURL)
	MAP_NATIVE(AInternetLink, execIsDataPending)
	MAP_NATIVE(AInternetLink, execGetLocalIP)
	{NULL, NULL}
};
IMPLEMENT_NATIVE_HANDLER(IpDrv,AInternetLink);

AUdpLinkNativeInfo GIpDrvAUdpLinkNatives[] =
{
	MAP_NATIVE(AUdpLink, execReadBinary)
	MAP_NATIVE(AUdpLink, execReadText)
	MAP_NATIVE(AUdpLink, execSendBinary)
	MAP_NATIVE(AUdpLink, execSendText)
	MAP_NATIVE(AUdpLink, execBindPort)
	{NULL, NULL}
};
IMPLEMENT_NATIVE_HANDLER(IpDrv,AUdpLink);

ATcpLinkNativeInfo GIpDrvATcpLinkNatives[] =
{
	MAP_NATIVE(ATcpLink, execReadBinary)
	MAP_NATIVE(ATcpLink, execReadText)
	MAP_NATIVE(ATcpLink, execSendBinary)
	MAP_NATIVE(ATcpLink, execSendText)
	MAP_NATIVE(ATcpLink, execBindPort)	
	MAP_NATIVE(ATcpLink, execIsConnected)
	MAP_NATIVE(ATcpLink, execClose)
	MAP_NATIVE(ATcpLink, execOpen)
	MAP_NATIVE(ATcpLink, execListen)
	{NULL, NULL}
};
IMPLEMENT_NATIVE_HANDLER(IpDrv,ATcpLink);

AMasterServerLinkNativeInfo GIpDrvAMasterServerLinkNatives[] =
{
	MAP_NATIVE(AMasterServerLink, execPoll)
    {NULL, NULL}
};
IMPLEMENT_NATIVE_HANDLER(IpDrv,AMasterServerLink);

AMasterServerClientNativeInfo GIpDrvAMasterServerClientNatives[] =
{
	MAP_NATIVE(AMasterServerClient, execStartQuery)
	MAP_NATIVE(AMasterServerClient, execPingServer)
    {NULL, NULL}
};
IMPLEMENT_NATIVE_HANDLER(IpDrv,AMasterServerClient);

AMasterServerUplinkNativeInfo GIpDrvAMasterServerUplinkNatives[] =
{
	MAP_NATIVE(AMasterServerUplink, execReconnect)
	MAP_NATIVE(AMasterServerUplink, execLogStatLine)
    {NULL, NULL}
};
IMPLEMENT_NATIVE_HANDLER(IpDrv,AMasterServerUplink);
*/
#endif

