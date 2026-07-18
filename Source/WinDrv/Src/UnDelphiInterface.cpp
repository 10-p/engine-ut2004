/*=============================================================================
	UnDelphiInterface.cpp: Debugger Interface Interface
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Lucas Alonso, Demiurge Studios
=============================================================================*/

#include "WinDrv.h"
#include "UnDebuggerCore.h"
#include "UnDelphiInterface.h"

void DelphiCallback( const char* C )
{
	((DelphiInterface*)((UDebuggerCore*)GDebugger)->GetInterface())->Callback( C );
}



DelphiInterface::DelphiInterface( const TCHAR* InDLLName ) : 
hInterface(NULL),
ShowDllForm(NULL),
AddClassToHierarchy(NULL),
BuildHierarchy(NULL),
ClearHierarchy(NULL),
SetCallback(NULL),
EditorCommand(NULL),
EditorLoadTextBuffer(NULL),
ClearWatch(NULL),
AddWatch(NULL),
AddBreakpoint(NULL),
DRemoveBreakpoint(NULL),
EditorGotoLine(NULL),
AddLineToLog(NULL),
EditorLoadClass(NULL),
CallStackClear(NULL),
CallStackAdd(NULL),
DebugWindowState(NULL),
DClearAWatch(NULL),
DAddAWatch(NULL),
DLockList(NULL),
DUnlockList(NULL),
SetCurrentObjectName(NULL)
{
	guard(DelphiInterface::DelphiInterface);
	DllName = InDLLName;
	Debugger = NULL;
	LoadCount = 0;
	unguard;
}

DelphiInterface::~DelphiInterface()
{
	Debugger = 0;
}

int DelphiInterface::AddAWatch(int watch, int ParentIndex, const TCHAR* ObjectName, const TCHAR* Data)
{
	return DAddAWatch(watch, ParentIndex, TCHAR_TO_ANSI(ObjectName), TCHAR_TO_ANSI(Data));
}


void DelphiInterface::ClearAWatch(int watch)
{
	DClearAWatch(watch);
}


UBOOL DelphiInterface::Initialize( UDebuggerCore* DebuggerOwner )
{
	guard(UDelphiInterface::Initialize);

	Debugger = DebuggerOwner;
	if ( LoadCount == 0 )
	{
		BindToDll();
		SetCallback( &DelphiCallback );
		ClearWatch( LOCAL_WATCH );
		ClearWatch( GLOBAL_WATCH );
		ClearWatch( WATCH_WATCH );
	}
	Show();
	return TRUE;

	unguard;
}


void DelphiInterface::Callback( const char* C )
{
	guard(DelphiInterface::Callback);

	// uncomment to log all callback mesages from the UI
	const TCHAR* command = ANSI_TO_TCHAR(C);
//	debugf(TEXT("Callback: %s"), command);

	if(ParseCommand(&command, TEXT("addbreakpoint")))
	{		
		TCHAR className[256];
		ParseToken(command, className, 256, 0);
		TCHAR lineNumString[10];
		ParseToken(command, lineNumString, 10, 0);
		SetBreakpoint(className, appAtoi(lineNumString));

	}
	else if(ParseCommand(&command, TEXT("removebreakpoint")))
	{
		TCHAR className[256];
		ParseToken(command, className, 256, 0);
		TCHAR lineNumString[10];
		ParseToken(command, lineNumString, 10, 0);
		RemoveBreakpoint(className, appAtoi(lineNumString));
	}
	else if(ParseCommand(&command, TEXT("addwatch")))
	{
		TCHAR watchName[256];
		ParseToken(command, watchName, 256, 0);
		Debugger->AddWatch(watchName);
	}
	else if(ParseCommand(&command, TEXT("removewatch")))
	{
		TCHAR watchName[256];
		ParseToken(command, watchName, 256, 0);
		Debugger->RemoveWatch(watchName);
	}
	else if(ParseCommand(&command, TEXT("clearwatch")))
	{
		Debugger->ClearWatches();
	}
	else if ( ParseCommand(&command,TEXT("setcondition")) )
	{
		FString ConditionName, Value;
		if ( !ParseToken(command,ConditionName,1) )
		{
			debugf(TEXT("Callback error (setcondition): Couldn't parse condition name"));
			return;
		}
		if ( !ParseToken(command,Value,1) )
		{
			debugf(TEXT("Callback error (setcondition): Failed parsing condition value"));
			return;
		}

		Debugger->SetCondition(*ConditionName,*Value);
		return;
	}
	else if (ParseCommand(&command,TEXT("setdatawatch")))
	{
		FString WatchText;
		if ( !ParseToken(command,WatchText,1) )
		{
			debugf(TEXT("Callback error (setdatawatch): Failed parsing watch text"));
			return;
		}

		Debugger->SetDataBreakpoint(*WatchText);
		return;
	}
	else if(ParseCommand(&command, TEXT("breakonnone")))
	{
		TCHAR breakValueString[5];
		ParseToken(command, breakValueString, 5, 0);
		Debugger->SetBreakOnNone(appAtoi(breakValueString));
	}
	else if(ParseCommand(&command, TEXT("break")))
		Debugger->BreakASAP = 1;

	else if(ParseCommand(&command, TEXT("stopdebugging")))
	{
		Debugger->Close();
		return;
	}
	else if (Debugger->IsDebugging)
	{
		EUserAction Action = UA_None;
		if(ParseCommand(&command, TEXT("go")))
			Action = UA_Go;
		else if ( ParseCommand(&command,TEXT("stepinto")) )
			Action = UA_StepInto;
		else if ( ParseCommand(&command,TEXT("stepover")) )
			Action = UA_StepOverStack;
		else if(ParseCommand(&command, TEXT("stepoutof")))
			Action = UA_StepOut;
		Debugger->ProcessInput(Action);
	}

	unguard;
}



void DelphiInterface::AddToLog( const TCHAR* Line )
{
	guard(DephiInterface::AddToLog);
	AddLineToLog(TCHAR_TO_ANSI(Line));
	unguard;
}

void DelphiInterface::Show()
{
	ShowDllForm();
}

void DelphiInterface::Close()
{
	guard(UDelphiInterface::Close);

	if ( hInterface )
	{
		if ( !FreeLibrary(hInterface) )
		{
			debugf(NAME_Warning, TEXT("Unknown error attempting to unload interface dll '%s'"), *DllName);
			return;
		}
        LoadCount--;

		if ( LoadCount == 0 )
			UnbindDll();
	}

	hInterface = NULL;

	unguard;
}

void DelphiInterface::Hide()
{

}

void DelphiInterface::Update( const TCHAR* ClassName, const TCHAR* PackageName, INT LineNumber, const TCHAR* OpcodeName, const TCHAR* objectName )
{
	guard(DephiInterface::Update);

	FString openName(PackageName);
	openName += TEXT(".");
	openName += ClassName;

	EditorLoadClass(TCHAR_TO_ANSI(*openName));
	EditorGotoLine(LineNumber, 1);

	SetCurrentObjectName(TCHAR_TO_ANSI(objectName));

	unguard;
}


void DelphiInterface::UpdateCallStack( TArray<FString>& StackNames )
{
	guard(DephiInterface::UpdateCallStack);

	CallStackClear();
	for(INT i = 0; i<StackNames.Num(); i++)
		CallStackAdd(TCHAR_TO_ANSI(*StackNames(i)));

	unguard;
}



void DelphiInterface::SetBreakpoint( const TCHAR* ClassName, INT Line )
{
	FString upper(ClassName);
	upper = upper.Caps();
	Debugger->GetBreakpointManager()->SetBreakpoint( ClassName, Line );
	AddBreakpoint(TCHAR_TO_ANSI(*upper), Line);
}



void DelphiInterface::RemoveBreakpoint( const TCHAR* ClassName, INT Line )
{
	FString upper(ClassName);
	upper = upper.Caps();
	Debugger->GetBreakpointManager()->RemoveBreakpoint( ClassName, Line );
	DRemoveBreakpoint(TCHAR_TO_ANSI(*upper), Line);
}	



void DelphiInterface::UpdateClassTree()
{
	ClassTree.Empty();
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* ParentClass = Cast<UClass>(It->SuperField);
		if (ParentClass)
			ClassTree.Add( ParentClass, *It );
	}

	ClearHierarchy();
	AddClassToHierarchy( "Core..Object" );
	RecurseClassTree( UObject::StaticClass() );
	BuildHierarchy();
}



void DelphiInterface::RecurseClassTree( UClass* ParentClass )
{
	TArray<UClass*> ChildClasses;
	ClassTree.MultiFind( ParentClass, ChildClasses );

	for (INT i = 0; i < ChildClasses.Num(); i++)
	{
		// Get package name
		FString FullName = ChildClasses(i)->GetFullName();
		int CutPos = FullName.InStr( TEXT(".") );
		
		// Extract the package name and chop off the 'Class' thing.
		FString PackageName = FullName.Left( CutPos );
		PackageName = PackageName.Right( PackageName.Len() - 6 );
		AddClassToHierarchy( TCHAR_TO_ANSI(*FString::Printf( TEXT("%s.%s.%s"), *PackageName, ParentClass->GetName(), ChildClasses(i)->GetName() )) );

		RecurseClassTree( ChildClasses(i) );
	}

#if ENGINE_VERSION > 2225
	ClassTree.MultiRemove( ParentClass );
#else
	for ( INT i = ChildClasses.Num() - 1; i >= 0; i-- )
		ClassTree.RemovePair( ParentClass, ChildClasses(i) );
#endif

}



void DelphiInterface::LockWatch(int watch)
{
	DLockList(watch);
}



void DelphiInterface::UnlockWatch(int watch)
{
	DUnlockList(watch);
}


void DelphiInterface::BindToDll()
{
	guard(DelphiInterface::BindToDll);

	if ( LoadCount > 0 )
		return;

	hInterface = LoadLibrary( *DllName );
	if ( hInterface == NULL )
	{
		debugf( NAME_Warning, TEXT("Couldn't load interface dll '%s'"), *DllName );
		hInterface = LoadLibrary(TEXT("dinterface.dll"));
	}

	if ( hInterface == NULL )
	{
		appThrowf(TEXT("No suitable interface dll found!"));
		return;
	}

	LoadCount++;

	// Get pointers to the delphi functions
	ShowDllForm = (DelphiVoidVoid)GetProcAddress( hInterface, "ShowDllForm" );
	EditorCommand = (DelphiVoidChar)GetProcAddress( hInterface, "EditorCommand" );
	EditorLoadTextBuffer = (DelphiVoidCharChar)GetProcAddress( hInterface, "EditorLoadTextBuffer" );
	AddClassToHierarchy = (DelphiVoidChar)GetProcAddress( hInterface, "AddClassToHierarchy" );
	BuildHierarchy = (DelphiVoidVoid)GetProcAddress( hInterface, "BuildHierarchy" );
	ClearHierarchy = (DelphiVoidVoid)GetProcAddress( hInterface, "ClearHierarchy" );
	BuildHierarchy = (DelphiVoidVoid)GetProcAddress( hInterface, "BuildHierarchy" );
	ClearWatch = (DelphiVoidInt)GetProcAddress( hInterface, "ClearWatch" );
	AddWatch = (DelphiVoidIntChar)GetProcAddress( hInterface, "AddWatch" );
	SetCallback = (DelphiVoidVoidPtr)GetProcAddress( hInterface, "SetCallback" );
	AddBreakpoint = (DelphiVoidCharInt)GetProcAddress( hInterface, "AddBreakpoint" );
	DRemoveBreakpoint = (DelphiVoidCharInt)GetProcAddress( hInterface, "RemoveBreakpoint" );
	EditorGotoLine = (DelphiVoidIntInt)GetProcAddress( hInterface, "EditorGotoLine" );
	AddLineToLog = (DelphiVoidChar)GetProcAddress( hInterface, "AddLineToLog" );
	EditorLoadClass = (DelphiVoidChar)GetProcAddress( hInterface, "EditorLoadClass" );
	CallStackClear = (DelphiVoidVoid)GetProcAddress( hInterface, "CallStackClear" );
	CallStackAdd = (DelphiVoidChar)GetProcAddress( hInterface, "CallStackAdd" );
	DebugWindowState = (DelphiVoidInt)GetProcAddress( hInterface, "DebugWindowState" );
	DClearAWatch = (DelphiVoidInt)GetProcAddress( hInterface, "ClearAWatch" );
	DAddAWatch = (DelphiIntIntIntCharChar)GetProcAddress( hInterface, "AddAWatch" );
	DLockList = (DelphiVoidInt)GetProcAddress( hInterface, "LockList" );
	DUnlockList = (DelphiVoidInt)GetProcAddress( hInterface, "UnlockList" );
	SetCurrentObjectName = (DelphiVoidChar)GetProcAddress( hInterface, "SetCurrentObjectName" );

	unguard;
}

void DelphiInterface::UnbindDll()
{
	guard(DelphiInterface::UnbindDll);

	ShowDllForm = NULL;
	EditorCommand = NULL;
	EditorLoadTextBuffer = NULL;
	AddClassToHierarchy = NULL;
	BuildHierarchy = NULL;
	ClearHierarchy = NULL;
	BuildHierarchy = NULL;
	ClearWatch = NULL;
	AddWatch = NULL;
	SetCallback = NULL;
	AddBreakpoint = NULL;
	DRemoveBreakpoint = NULL;
	EditorGotoLine = NULL;
	AddLineToLog = NULL;
	EditorLoadClass = NULL;
	CallStackClear = NULL;
	CallStackAdd = NULL;
	DebugWindowState = NULL;
	DClearAWatch = NULL;
	DAddAWatch = NULL;
	DLockList = NULL;
	DUnlockList = NULL;
	SetCurrentObjectName = NULL;

	unguard;
}

