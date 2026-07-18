/*=============================================================================
	OpCode.h: UDebugger bytecode debug information.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Ron Prestenback
=============================================================================*/

/*-----------------------------------------------------------------------------
	UDebugger Opcodes.
-----------------------------------------------------------------------------*/

enum EDebugInfo
{
	DI_Let					= 0x00,
	DI_SimpleIf				= 0x01,
	DI_Switch				= 0x02,
	DI_While				= 0x03,
	DI_Assert				= 0x04,
	DI_Return				= 0x10,
	DI_ReturnNothing		= 0x11,
	DI_NewStack				= 0x20,
	DI_NewStackLatent		= 0x21,
	DI_NewStackLabel		= 0x22,
	DI_PrevStack			= 0x30,
	DI_PrevStackLatent		= 0x31,
	DI_PrevStackLabel		= 0x32,
	DI_PrevStackState		= 0x33,
	DI_EFP					= 0x40,
	DI_EFPOper				= 0x41,
	DI_EFPIter				= 0x42,
	DI_ForInit				= 0x50,
	DI_ForEval				= 0x51,
	DI_ForInc				= 0x52,
	DI_BreakLoop			= 0x60,
	DI_BreakFor				= 0x61,
	DI_BreakForEach			= 0x62,
	DI_BreakSwitch			= 0x63,
	DI_ContinueLoop			= 0x70,
	DI_ContinueForeach		= 0x71,
	DI_ContinueFor			= 0x72,
	DI_MAX					= 0xFF,
};


static inline TCHAR* GetOpCodeName( BYTE OpCode )
{
	switch ( OpCode )
	{
	case DI_Let:				return TEXT("Let");
	case DI_SimpleIf:			return TEXT("SimpleIf");
	case DI_Switch:				return TEXT("Switch");
	case DI_While:				return TEXT("While");
	case DI_Assert:				return TEXT("Assert");
	case DI_Return:				return TEXT("Return");
	case DI_ReturnNothing:		return TEXT("ReturnNothing");
	case DI_NewStack:			return TEXT("NewStack");
	case DI_NewStackLatent:		return TEXT("NewStackLatent");
	case DI_NewStackLabel:		return TEXT("NewStackLabel");
	case DI_PrevStack:			return TEXT("PrevStack");
	case DI_PrevStackLatent:	return TEXT("PrevStackLatent");
	case DI_PrevStackLabel:		return TEXT("PrevStackLabel");
	case DI_PrevStackState:		return TEXT("PrevStackState");
	case DI_EFP:				return TEXT("EFP");
	case DI_EFPOper:			return TEXT("EFPOper");
	case DI_EFPIter:			return TEXT("EFPIter");
	case DI_ForInit:			return TEXT("ForInit");
	case DI_ForEval:			return TEXT("ForEval");
	case DI_ForInc:				return TEXT("ForInc");
	case DI_BreakLoop:			return TEXT("BreakLoop");
	case DI_BreakFor:			return TEXT("BreakFor");
	case DI_BreakForEach:		return TEXT("BreakForEach");
	case DI_BreakSwitch:		return TEXT("BreakSwitch");
	case DI_ContinueLoop:		return TEXT("ContinueLoop");
	case DI_ContinueForeach:	return TEXT("ContinueForeach");	
	case DI_ContinueFor:		return TEXT("ContinueFor");
	}

	return NULL;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
