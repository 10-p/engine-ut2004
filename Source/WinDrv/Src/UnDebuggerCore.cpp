/*=============================================================================
	UnDebuggerCore.cpp: Debugger Core Logic
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Lucas Alonso, Demiurge Studios
	* Revised by Ron Prestenback
=============================================================================*/

#include "WinDrv.h"
#include "UnDebuggerCore.h"
#include "UnDebuggerInterface.h"
#include "UnDelphiInterface.h"
#include "OpCode.h"

#if ENGINE_VERSION < 2226
static UBOOL IsNumeric( FString& Test )
{
	guardSlow(IsNumeric);

	if ( Test.Len() == 0 )
		return 0;

	TArray<TCHAR> Arr = Test.GetCharArray();
	for ( INT i = 0; i < Arr.Num(); i++ )
	{
		if ( !appIsDigit(Arr(i)) )
			return 0;
	}

	return 1;

	unguardSlow;
}
#endif

#if 0
	#define PROFILEDEBUGGER
#endif

/*-----------------------------------------------------------------------------
	UDebuggerCore.
-----------------------------------------------------------------------------*/

UDebuggerCore::UDebuggerCore(WLog* InLogWindow)
: IsDebugging(0), IsClosing(0), CurrentState(NULL), PendingState(NULL), BreakpointManager(NULL),
CallStack(NULL), Interface(NULL), AccessedNone(0), BreakOnNone(0), BreakASAP(0), ArrayMemberIndex(INDEX_NONE)
{
	guard(UDebuggerCore::UDebuggerCore);

	if ( !GConfig->GetString(TEXT("DEBUGGER.DEBUGGER"), TEXT("InterfaceFilename"), InterfaceDllName, TEXT("UDebugger.ini")) )
		InterfaceDllName = TEXT("dinterface.dll");

	if ( InterfaceDllName.Right(4) != TEXT(".dll") )
		InterfaceDllName += TEXT(".dll");

	Interface = new DelphiInterface(*InterfaceDllName);
	if ( !Interface->Initialize( this ))
		appErrorf( TEXT("Could not initialize the debugger interface!") );

	CallStack = new FCallStack( this );
	ChangeState(new DSIdleState(this));
	BreakpointManager = new FBreakpointManager( this );
	
	GLogHook = new FDebuggerLogHook(InLogWindow);


	debugf( NAME_Init, TEXT("UnrealScript Debugger Core Initialized.") );


	int i = 0;
	guard(UDebuggerCore::LoadingBreakpoints);

	// Load saved breakpoints from UDebugger.ini
	int breakpointLine;

	FString breakpointName(TEXT("Breakpoint"));
	breakpointName += appItoa(i);

	// while the ini file has more lines that read BreakpointXline=some_line_number
	while(GConfig->GetInt(TEXT("DEBUGGER.BREAKPOINTS"), *(breakpointName + TEXT("line")), breakpointLine, TEXT("UDebugger.ini")))
	{
		// grab the class
		FString breakpointClass;

		// fails if there is a breakpointXline=1234 and no breakpointXclass=somepackage.someclass
		if ( !GConfig->GetString(TEXT("DEBUGGER.BREAKPOINTS"), *(breakpointName + TEXT("class")), breakpointClass, TEXT("UDebugger.ini")) )
			appErrorf(NAME_FriendlyError, TEXT("UDebugger.ini contains inconsistencies!  Found %sline but no %sclass"), *breakpointName, *breakpointName);

		// set via the interface...ugly. 
		Interface->SetBreakpoint(*breakpointClass, breakpointLine);

		i++;
		breakpointName = TEXT("Breakpoint");
		breakpointName += appItoa(i);
	}

	unguard;

	guard(UDebuggerCore::LoadingWatches);

	// Load saved watches
	i = 0;
	FString watchINIVar(TEXT("watch"));
	watchINIVar += appItoa(i);

	FString watchName;

	// while the ini file has more lines that read BreakpointXline=some_line_number
	while(GConfig->GetString(TEXT("DEBUGGER.WATCHES"), *watchINIVar, watchName, TEXT("UDebugger.ini")))
	{
		AddWatch(*watchName);

		i++;
		watchINIVar = TEXT("watch");
		watchINIVar += appItoa(i);
	}

	unguard;
	// Init recursion limits
	FString Value;
	if ( GConfig->GetString(TEXT("DEBUGGER.RECURSION"), TEXT("OBJECTMAX"), Value, TEXT("UDebugger.ini")) )
		MaxObjectRecursion = appAtoi(*Value);
	else MaxObjectRecursion = 1;

	if ( GConfig->GetString(TEXT("DEBUGGER.RECURSION"),TEXT("STRUCTMAX"),Value,TEXT("UDebugger.ini")) )
		MaxStructRecursion = appAtoi(*Value);
	else MaxStructRecursion = INDEX_NONE;

	if ( GConfig->GetString(TEXT("DEBUGGER.RECURSION"),TEXT("CLASSMAX"),Value,TEXT("UDebugger.ini")) )
		MaxClassRecursion = appAtoi(*Value);
	else MaxClassRecursion = 1;

	if ( GConfig->GetString(TEXT("DEBUGGER.RECURSION"),TEXT("STATICARRAYMAX"),Value,TEXT("UDebugger.ini")) )
		MaxStaticArrayRecursion = appAtoi(*Value);
	else MaxStaticArrayRecursion = 2;

	if ( GConfig->GetString(TEXT("DEBUGGER.RECURSION"),TEXT("DYNAMICARRAYMAX"),Value,TEXT("UDebugger.ini")) )
		MaxDynamicArrayRecursion = appAtoi(*Value);
	else MaxDynamicArrayRecursion = 1;

	CurrentObjectRecursion = CurrentStructRecursion = CurrentClassRecursion = CurrentStaticArrayRecursion = CurrentDynamicArrayRecursion = 0;

	ResultBuffer = new BYTE[10000];
	appMemzero( ResultBuffer, 10000 );

	unguard;
}

UDebuggerCore::~UDebuggerCore()
{
	guard(UDebuggerCore::~UDebuggerCore);
	debugf( NAME_Init, TEXT("UnrealScript Debugger Core Exit.") );
	
	for(int i=0; i<Watches.Num(); i++)
	{
		FString watchVarName(TEXT("watch"));
		watchVarName += appItoa(i);
		GConfig->SetString(TEXT("DEBUGGER.WATCHES"), *watchVarName, *Watches(i).WatchText, TEXT("UDebugger.ini"));
	}

	GConfig->SetString(TEXT("DEBUGGER.RECURSION"),TEXT("OBJECTMAX"),appItoa(MaxObjectRecursion),TEXT("UDebugger.ini"));
	GConfig->SetString(TEXT("DEBUGGER.RECURSION"),TEXT("STRUCTMAX"),appItoa(MaxStructRecursion),TEXT("UDebugger.ini"));
	GConfig->SetString(TEXT("DEBUGGER.RECURSION"),TEXT("CLASSMAX"),appItoa(MaxClassRecursion),TEXT("UDebugger.ini"));
	GConfig->SetString(TEXT("DEBUGGER.RECURSION"),TEXT("STATICARRAYMAX"),appItoa(MaxStaticArrayRecursion),TEXT("UDebugger.ini"));
	GConfig->SetString(TEXT("DEBUGGER.RECURSION"),TEXT("DYNAMICARRAYMAX"),appItoa(MaxDynamicArrayRecursion),TEXT("UDebugger.ini"));

	if(CurrentState)
		delete CurrentState;
	CurrentState = NULL;

	if(PendingState)
		delete PendingState;
	PendingState = NULL;

	if(BreakpointManager)
		delete BreakpointManager;
	BreakpointManager = NULL;
	
	if ( CallStack )
		delete CallStack;
	CallStack = NULL;
	
	if ( Interface )
	{
		Interface->Close();
		delete Interface;
	}
	Interface = NULL;

	if ( GLogHook )
		delete GLogHook;
	GLogHook = NULL;

	// Clean up our buffer
	delete [] ResultBuffer;
	ResultBuffer = NULL;

	unguard;
}

void UDebuggerCore::Initialize()
{
	guard(UDebuggerCore::Initialize);

	if ( IsInitialized() )
		return;

	IsClosing = 0;
	AccessedNone = 0;
	BreakASAP = 0;
	IsDebugging = 0;

	if ( !Interface->Initialize( this ) )
		appErrorf( TEXT("Could not initialize the debugger interface!") );

	unguard;
}

UBOOL UDebuggerCore::IsInitialized() const
{
	return !IsClosing;
}

void UDebuggerCore::Close()
{
	guard(UDebuggerCore::Close);

	if ( IsClosing )
		return;

	IsClosing = 1;
	if ( CallStack )
		CallStack->Empty();

	StackChanged(NULL);

	if ( CurrentState )
		CurrentState->SetCurrentNode(NULL);

	ChangeState(new DSIdleState(this));

	unguard;
}

void UDebuggerCore::ProcessInput( enum EUserAction UserAction )
{
	guard(UDebuggerCore::ProcessInput);

	CurrentState->HandleInput(UserAction);

	unguard;
}

const FStackNode* UDebuggerCore::GetCurrentNode() const
{
	guard(UDebugger::GetCurrentNode);

	const FStackNode* Node = CurrentState ? CurrentState->GetCurrentNode() : NULL;
	if ( !Node )
		Node = CallStack->GetTopNode();

	return Node;

	unguard;
}

void UDebuggerCore::AddWatch(const TCHAR* watchName)
{
	guard(UDebuggerCore::AddWatch);

	FDebuggerWatch* NewWatch = new(Watches) FDebuggerWatch(ErrorDevice, watchName);

	FDebuggerState* State = GetCurrentState();

	if ( IsDebugging && State && NewWatch )
	{
		const FStackNode* Node = State->GetCurrentNode();
		if ( Node )
		{
			NewWatch->Refresh( Node->Object, Node->StackNode );

			Interface->LockWatch(Interface->WATCH_WATCH);
			Interface->ClearAWatch(Interface->WATCH_WATCH);
			RefreshUserWatches();
			Interface->UnlockWatch(Interface->WATCH_WATCH);
		}
	}

	unguard;
}


void UDebuggerCore::RemoveWatch(const TCHAR* watchName)
{
	guard(UDebuggerCore::RemoveWatch);

	for ( INT i = 0; i < Watches.Num(); i++ )
	{
		if ( Watches(i).WatchText == watchName )
		{
			Watches.Remove(i);
			return;
		}
	}

	unguard;
}


void UDebuggerCore::ClearWatches()
{
	guard(UDebuggerCore::ClearWatches);

	Watches.Empty();

	unguard;
}

#define SETPARENT(m,c,i) m.Set(c,i + 1)
#define GETPARENT(m,c)   m.FindRef(c) - 1

void UDebuggerCore::BuildParentChain( INT WatchType, TMap<UClass*,INT>& ParentChain, UClass* BaseClass, INT ParentIndex )
{
	guard(UDebuggerCore::BuildParentChain);

	if ( !BaseClass )
		return;

	ParentChain.Empty();
	SETPARENT(ParentChain,BaseClass,ParentIndex);

	for ( UClass* Parent = BaseClass->GetSuperClass(); Parent; Parent = Parent->GetSuperClass() )
	{
		if ( ParentChain.Find(Parent) == NULL )
		{
			ParentIndex = Interface->AddAWatch( WatchType, ParentIndex, *FString::Printf(TEXT("[[ %s ]]"), Parent->GetName()), TEXT("[[ Base Class ]]") );
			SETPARENT(ParentChain,Parent,ParentIndex);
		}
	}

	unguard;
}
	
// Insert a given property into the watch window. 
void UDebuggerCore::PropertyToWatch( UProperty* Prop, BYTE* PropAddr, UBOOL bResetIndex, INT watch, const TCHAR* PropName )
{
	guard(UDebuggerCore::PropertyToWatch::Frame);

	INT ParentIndex = INDEX_NONE;
	static TMap<UClass*,INT> InheritanceChain;

	if ( bResetIndex )
	{
		if ( watch == Interface->GLOBAL_WATCH )
			BuildParentChain(watch, InheritanceChain, ((UObject*)(PropAddr - Prop->Offset))->GetClass());
		else
			InheritanceChain.Empty();
	}

	ArrayMemberIndex = INDEX_NONE;
	ParentIndex = GETPARENT(InheritanceChain,Prop->GetOwnerClass());
	PropertyToWatch(Prop, PropAddr, 0, watch, ParentIndex, PropName);

	unguard;
}

// Extract the value of a given property at a given address
void UDebuggerCore::PropertyToWatch(UProperty* Prop, BYTE* PropAddr, INT CurrentDepth, INT watch, INT watchParent, const TCHAR* PropName )
{
	guard(UDebuggerCore::PropertyToWatch::Property);

	// This SHOULD be sufficient.
	FString VarName, VarValue;
	TCHAR* Buffer = (TCHAR*)ResultBuffer;
	*Buffer = 0;
	if ( ArrayMemberIndex < INDEX_NONE )
		ArrayMemberIndex = INDEX_NONE;

	if ( Prop->ArrayDim > 1 && ArrayMemberIndex < 0 )
	{
		if ( CurrentStaticArrayRecursion < MaxStaticArrayRecursion || MaxStaticArrayRecursion == INDEX_NONE )
		{
			VarName = PropName ? PropName : FString::Printf( TEXT("%s ( Static %s Array )"), Prop->GetName(), GetShortName(Prop) );
			VarValue = FString::Printf(TEXT("%i Elements"), Prop->ArrayDim);

			INT WatchID = Interface->AddAWatch(watch, watchParent, *VarName, *VarValue);

			CurrentStaticArrayRecursion++;
			for ( INT i = 0; i < Prop->ArrayDim; i++ )
			{
				ArrayMemberIndex++;
				PropertyToWatch(Prop, PropAddr + Prop->ElementSize * i, CurrentDepth + 1, watch, WatchID);
			}
			CurrentStaticArrayRecursion--;

			ArrayMemberIndex = INDEX_NONE;
		}
		return;
	}

	VarName = PropName 
		? PropName : (ArrayMemberIndex >= 0
		? FString::Printf(TEXT("%s[%i]"), (Prop->IsA(UDelegateProperty::StaticClass()) ? Cast<UDelegateProperty>(Prop)->Function->GetName() : Prop->GetName()), ArrayMemberIndex)
		: (FString::Printf(TEXT("%s ( %s )"), (Prop->IsA(UDelegateProperty::StaticClass()) ? Cast<UDelegateProperty>(Prop)->Function->GetName() : Prop->GetName()), GetShortName(Prop))));


	if ( Prop->IsA(UStructProperty::StaticClass()) )
		VarValue = GetShortName(Prop);

	else if ( Prop->IsA(UArrayProperty::StaticClass()) )
		VarValue = FString::Printf(TEXT("%i %s %s"), ((FArray*)PropAddr)->Num(), GetShortName(Cast<UArrayProperty>(Prop)->Inner), ((FArray*)PropAddr)->Num() != 1 ? TEXT("Elements") : TEXT("Element"));

	else if ( Prop->IsA(UObjectProperty::StaticClass()) )
	{
		if ( *(UObject**)PropAddr )
			VarValue = (*(UObject**)PropAddr)->GetName();
		else VarValue = TEXT("None");
	}
	else
	{
		Prop->ExportTextItem( Buffer, PropAddr, PropAddr, PPF_Delimited );
		VarValue = Buffer;
	}

	int ID = Interface->AddAWatch(watch, watchParent, *VarName, *VarValue);
	if ( Prop->IsA(UStructProperty::StaticClass()) && (CurrentStructRecursion < MaxStructRecursion || MaxStructRecursion == INDEX_NONE) )
	{
		INT CurrentIndex = ArrayMemberIndex;
		ArrayMemberIndex = INDEX_NONE;

		CurrentStructRecursion++;
		// Recurse every property in this struct, and copy it's value into Result;
		for( TFieldIterator<UProperty> It(Cast<UStructProperty>(Prop)->Struct); It; ++It )
		{
			if (Prop == *It) continue;

			// Special case for nested stuff, don't leave it up to VarName/VarValue since we need to recurse
			PropertyToWatch(*It, PropAddr + It->Offset, CurrentDepth + 1, watch, ID);
		}
		ArrayMemberIndex = CurrentIndex;
		CurrentStructRecursion--;
	}

	else if ( Prop->IsA(UClassProperty::StaticClass()) && (CurrentClassRecursion < MaxClassRecursion || MaxClassRecursion == INDEX_NONE) )
	{
		UClass* ClassResult = *(UClass**)PropAddr;
		if ( !ClassResult )
			return;

		INT CurrentIndex = ArrayMemberIndex, CurrentID;
		ArrayMemberIndex = INDEX_NONE;

		TMap<UClass*,INT> ParentChain;
		UClass* PropOwner = NULL;

		BuildParentChain(watch, ParentChain, ClassResult, ID);
		CurrentClassRecursion++;
		for ( TFieldIterator<UProperty> It(ClassResult); It; ++It )
		{
			if ( Prop == *It ) continue;

			PropOwner = It->GetOwnerClass();
			if ( PropOwner == UObject::StaticClass() ) continue;

			CurrentID = GETPARENT(ParentChain,PropOwner);
			PropertyToWatch(*It, (BYTE*) &ClassResult->Defaults(It->Offset), CurrentDepth + 1, watch, CurrentID);
		}
		CurrentClassRecursion--;

		ArrayMemberIndex = CurrentIndex;
	}

	else if(Prop->IsA(UObjectProperty::StaticClass()) && (CurrentObjectRecursion < MaxObjectRecursion || MaxObjectRecursion == INDEX_NONE) )
	{
		UObject* ObjResult = *(UObject**)PropAddr;
		if ( !ObjResult )
			return;

		INT CurrentIndex = ArrayMemberIndex, CurrentID;
		ArrayMemberIndex = INDEX_NONE;

		TMap<UClass*,INT> ParentChain;

		BuildParentChain(watch, ParentChain, ObjResult->GetClass(), ID);
		CurrentObjectRecursion++;

		UClass* PropOwner = NULL;
		for( TFieldIterator<UProperty> It( ObjResult->GetClass() ); It; ++It )
		{
			if (Prop == *It) continue;

			PropOwner = It->GetOwnerClass();
			if ( PropOwner == UObject::StaticClass() ) continue;

			CurrentID = GETPARENT(ParentChain,PropOwner);
			PropertyToWatch( *It, (BYTE*)ObjResult + It->Offset, CurrentDepth + 1, watch, CurrentID );
		}
		ArrayMemberIndex = CurrentIndex;
		CurrentObjectRecursion--;
	}

	else if (Prop->IsA( UArrayProperty::StaticClass() ) && (CurrentDynamicArrayRecursion < MaxDynamicArrayRecursion || MaxDynamicArrayRecursion == INDEX_NONE) )
	{
		const INT Size		= Cast<UArrayProperty>(Prop)->Inner->ElementSize;
		FArray* Array		= ((FArray*)PropAddr);

		INT CurrentIndex = ArrayMemberIndex;
		ArrayMemberIndex = INDEX_NONE;

		CurrentDynamicArrayRecursion++;
		for ( INT i = 0; i < Array->Num(); i++ )
		{
			ArrayMemberIndex++;
			PropertyToWatch(Cast<UArrayProperty>(Prop)->Inner, (BYTE*)Array->GetData() + i * Size, CurrentDepth + 1, watch, ID );
		}

		ArrayMemberIndex = CurrentIndex;
		CurrentDynamicArrayRecursion--;
	}

	unguardf(( TEXT("Property '%s'"), Prop->GetName()));
}

void UDebuggerCore::NotifyAccessedNone()
{
	AccessedNone=1;
}

void UDebuggerCore::SetBreakOnNone(UBOOL inBreakOnNone)
{
	BreakOnNone = inBreakOnNone;
	AccessedNone = 0;
}

void UDebuggerCore::SetCondition( const TCHAR* ConditionName, const TCHAR* ConditionValue )
{
	guard(UDebuggerCore::SetCondition);

	if ( GIsRequestingExit || IsClosing )
		return;

//	ChangeState( new DSCondition(this,ConditionName,ConditionValue,CurrentState) );
	unguard;
}

void UDebuggerCore::SetDataBreakpoint( const TCHAR* BreakpointName )
{
	guard(UDebuggerCore::SetDataBreakpoint);

	if ( GIsRequestingExit || IsClosing )
		return;

	ChangeState( new DSBreakOnChange(this,BreakpointName,CurrentState) );
	unguard;
}

#if ENGINE_VERSION > 2225
void UDebuggerCore::NotifyGC()
{
	guard(UDebuggerCore::NotifyGC);

	unguard;
}

UBOOL UDebuggerCore::NotifyAssertionFailed( const INT LineNumber )
{
	guard(UDebuggerCore::NotifyAssertionFailed);

	if ( GIsRequestingExit || IsClosing )
		return 0;

	debugf(TEXT("Assertion failed, line %i"), LineNumber);

	ChangeState( new DSWaitForInput(this), 1 );
	return !(GIsRequestingExit || IsClosing);
	unguard;
}

UBOOL UDebuggerCore::NotifyInfiniteLoop()
{
	guard(UDebuggerCore::NotifyInfiniteLoop);

	if ( GIsRequestingExit || IsClosing )
		return 0;

	debugf(TEXT("Recursion limit reached...breaking UDebugger"));

	ChangeState( new DSWaitForInput(this), 1 );
	return !(GIsRequestingExit || IsClosing);
	unguard;
}

#endif

void UDebuggerCore::StackChanged( const FStackNode* CurrentNode )
{
	guard(UDebuggerCore::StackChanged);

	// For now, simply refresh user watches
	// later, we can modify this to work for all watches, allowing the ability to view values from anywhere on the callstack

	const UObject* Obj = CurrentNode ? CurrentNode->Object : NULL;
	const FFrame* Node = CurrentNode ? CurrentNode->StackNode : NULL;

	for ( INT i = 0; i < Watches.Num(); i++ )
		Watches(i).Refresh(Obj, Node);

	unguard;
}

// Update the interface
void UDebuggerCore::UpdateInterface()
{
	guard(UDebuggerCore::UpdateInterface);

	if ( IsDebugging && CallStack)
	{
#ifdef PROFILEDEBUGGER
		DOUBLE StartTime = appSeconds();
#endif
		const FStackNode* TopNode = CallStack->GetTopNode();
		if ( !TopNode )
			return;
		
		// Get package name
		const TCHAR* cName = TopNode->GetClass()->GetName(),
			*pName = TopNode->GetClass()->GetOuter()->GetName();

		Interface->Update(	cName, 
							pName,
							TopNode->GetLine(),
							TopNode->GetInfo(),
							TopNode->Object->GetName());
		RefreshWatch( TopNode );

		TArray<FString> StackNames;
		for(int i=0;i < CallStack->StackDepth;i++) 
		{
			const FStackNode* TestNode = CallStack->GetNode(i);
			if (TestNode && TestNode->StackNode && TestNode->StackNode->Node)
				new(StackNames) FString( TestNode->StackNode->Node->GetFullName() );
		}
		Interface->UpdateCallStack( StackNames );

#ifdef PROFILEDEBUGGER
		DOUBLE Time = (appSeconds() - StartTime) * 1000;
		debugf(TEXT("**+++  UpdateInterface took %i ms  +++**"), (INT)Time);
#endif
	}
	unguard;
}

// Update the Watch ListView with all the current variables the Stack/Object contain.
void UDebuggerCore::RefreshWatch( const FStackNode* CNode )
{
	guard(UDebuggerCore::RefreshWatch);

	TArray<INT> foundWatchNamesIndicies;

	if ( CNode == NULL )
		return;

#ifdef PROFILEDEBUGGER
	DOUBLE StartTime = appSeconds();
#endif

	Interface->LockWatch(Interface->GLOBAL_WATCH);
	Interface->LockWatch(Interface->LOCAL_WATCH);
	Interface->LockWatch(Interface->WATCH_WATCH);
	Interface->ClearAWatch(Interface->GLOBAL_WATCH);
	Interface->ClearAWatch(Interface->LOCAL_WATCH);
	Interface->ClearAWatch(Interface->WATCH_WATCH);

#ifdef PROFILEDEBUGGER
	DOUBLE Time = (appSeconds() - StartTime) * 1000;
	debugf(TEXT("** Locking and clearing watches took %i ms **"), (INT)Time);
#endif

	UFunction* Function = Cast<UFunction>(CNode->GetFrame()->Node);
	const UObject* ContextObject = CNode->GetObject();
	UProperty* Parm;

#ifdef PROFILEDEBUGGER
	StartTime = appSeconds();
#endif

	// Setup the local variable watch
	guard(UDebuggerCore::RefreshLocalWatch);
	if ( Function )
	{
		for ( Parm = Function->PropertyLink; Parm; Parm = Parm->PropertyLinkNext )
			PropertyToWatch( Parm, CNode->GetFrame()->Locals + Parm->Offset, Parm == Function->PropertyLink, Interface->LOCAL_WATCH );
	}
	unguard;

#ifdef PROFILEDEBUGGER
	Time = (appSeconds() - StartTime) * 1000;
	debugf(TEXT("++ Filling local watches took %i ms ++"), (INT)Time);

	StartTime = appSeconds();
#endif

	// Setup the global vars watch
	guard(UDebuggerCore::RefreshGlobalWatch);
	TFieldFlagIterator<UProperty,CLASS_IsAUProperty> PropertyIt(ContextObject->GetClass());
	for( Parm = *PropertyIt; PropertyIt; ++PropertyIt )
		PropertyToWatch( *PropertyIt, (BYTE*)ContextObject + PropertyIt->Offset, *PropertyIt == Parm, Interface->GLOBAL_WATCH );
	unguard;

#ifdef PROFILEDEBUGGER
	Time = (appSeconds() - StartTime) * 1000;
	debugf(TEXT("++ Filling global watches took %i ms ++"), (INT)Time);


	StartTime = appSeconds();	
#endif

	RefreshUserWatches();
#ifdef PROFILEDEBUGGER
	Time = (appSeconds() - StartTime) * 1000;
	debugf(TEXT("++ Filling user watches took %i ms ++"), (INT)Time);


	StartTime = appSeconds();	
#endif

	Interface->UnlockWatch(Interface->GLOBAL_WATCH);
	Interface->UnlockWatch(Interface->LOCAL_WATCH);
	Interface->UnlockWatch(Interface->WATCH_WATCH);

#ifdef PROFILEDEBUGGER
	Time = (appSeconds() - StartTime) * 1000;
	debugf(TEXT("** Unlocking watches took %i ms **"), (INT)Time);
#endif

	unguard;
}

void UDebuggerCore::RefreshUserWatches()
{
	guard(UDebuggerCore::RefreshUserWatches);

	// Fill the custom watch values from the context of the current node
	for ( INT i = 0; i < Watches.Num(); i++ )
	{
		UProperty* Prop = NULL;
		BYTE* PropAddr = NULL;
		ErrorDevice.Empty();

		FDebuggerWatch& Watch = Watches(i);

#ifdef PROFILEDEBUGGER		
		DOUBLE WatchStartTime = appSeconds();
#endif
		if ( Watch.GetWatchValue((const UProperty *&) Prop, (const BYTE *&) PropAddr, ArrayMemberIndex) )
			PropertyToWatch(Prop, PropAddr, 0, Interface->WATCH_WATCH, INDEX_NONE, *Watch.WatchText);
		else Interface->AddAWatch( Interface->WATCH_WATCH, -1, *Watch.WatchText, *ErrorDevice );
#ifdef PROFILEDEBUGGER
		debugf(TEXT("  |  Watch %i (%s) took %i ms  |"), i, *Watch.WatchText, (INT)((appSeconds() - WatchStartTime) * 1000));
#endif
	}

	unguard;
}

void UDebuggerCore::LoadEditPackages()
{
	guard(UDebuggerCore::LoadEditPackages);

	TArray<FString> EditPackages;
	TMultiMap<FString,FString>* Sec = GConfig->GetSectionPrivate( TEXT("Editor.EditorEngine"), 0, 1, GIni );
	Sec->MultiFind( FString(TEXT("EditPackages")), EditPackages );
	TObjectIterator<UEngine> EngineIt;
	if ( EngineIt )
		for( INT i=0; i<EditPackages.Num(); i++ )
		{
			if(appStrcmp(*EditPackages(i), TEXT("UnrealEd"))) // don't load the UnrealEd 
			{
				if( !EngineIt->LoadPackage( NULL, *EditPackages(i), LOAD_NoWarn ) )
					appErrorf( TEXT("Can't find edit package '%s'"), *EditPackages(i) );
			}
		}
	Interface->UpdateClassTree();
	unguard;
}


UClass* UDebuggerCore::GetStackOwnerClass( const FFrame* Stack ) const
{
	guard(UDebuggerCore::GetStackOwnerClass);

	UClass* RClass;
	
	// Function?
	RClass = Cast<UClass>( Stack->Node->GetOuter() ); 
	
	// Nope, a state, we need to go one level higher to get the class
	if ( RClass == NULL )
		RClass = Cast<UClass>( Stack->Node->GetOuter()->GetOuter() );

	if ( RClass == NULL )
		RClass = Cast<UClass>( Stack->Node );
	
	// Make sure it's a real class
	check(RClass!=NULL);

	return RClass;

	unguard;
}


FDebuggerLogHook::FDebuggerLogHook( WLog* InLogWindow )
: LogWindow(InLogWindow)
{ }

void FDebuggerLogHook::Serialize( const TCHAR* Msg, EName Event )
{
	guardSlow(FDebuggerLogHook::Serialize);

	if ( Event != NAME_Title )
	{
		UDebuggerCore* Debugger = (UDebuggerCore*)GDebugger;
		if ( LogWindow )
			LogWindow->Serialize( Msg, Event );

		if ( Debugger && Debugger->Interface && Debugger->Interface->IsLoaded() )
			Debugger->Interface->AddToLog( *FString::Printf(TEXT("%s: %s"), FName::SafeString(Event), Msg) );
	}

	unguardSlow;
}
/*-----------------------------------------------------------------------------
	FCallStack.
-----------------------------------------------------------------------------*/

FCallStack::FCallStack( UDebuggerCore* InParent )
: Parent(InParent), StackDepth(0)
{
}

FCallStack::~FCallStack()
{
	Empty();
	Parent = NULL;
}

void FCallStack::Empty()
{
	QueuedCommands.Empty();
	Stack.Empty();
	StackDepth = 0;
}

/*-----------------------------------------------------------------------------
	FStackNode Implementation
-----------------------------------------------------------------------------*/

FStackNode::FStackNode( const UObject* Debugee, const FFrame* Stack, UClass* InClass, INT CurrentDepth, INT InLineNumber, INT InPos, BYTE InCode )
: Object(Debugee), StackNode(Stack), Class(InClass)
{
	Lines.AddItem(InLineNumber);
	Positions.AddItem(InPos);
	Depths.AddItem(CurrentDepth);
	OpCodes.AddItem(InCode);
}

const TCHAR* FStackNode::GetInfo() const
{
	guard(FStackNode::GetInfo);
	return GetOpCodeName(OpCodes.Last());
	unguard;
}

void FStackNode::Show() const
{
	guard(FStackNode::Show);

	debugf(TEXT("Object:%s  Class:%s  Line:%i  Code:%s"),
		Object ? Object->GetName() : TEXT("NULL"),
		Class  ? Class->GetName()  : TEXT("NULL"),
		GetLine(), GetInfo());

	unguard;
}

/*-----------------------------------------------------------------------------
	FDebuggerWatch
-----------------------------------------------------------------------------*/

static TCHAR* ParseNextName( TCHAR*& o )
{
	guard(ParseNextName);

	INT count(0);

	bool literal=false; // literal object name

	TCHAR* c = o;
	while ( c && *c )
	{
		if ( *c == '[' )
			count++;

		else if ( *c == ']')
			count--;

		else if ( count == 0 )
		{
			if ( *c == '\'' )
				literal = !literal;
			else if ( !literal )
			{
				if ( *c == '(' )
				{
					o = c;
					*o++ = 0;
				}
				else if ( *c == ')' )
					*c = 0;

				else if ( *c == '.' )
				{
					*c++ = 0;
					return c;
				}
			}
		}

		c++;
	}

	return NULL;

	unguard;
}

FDebuggerWatch::FDebuggerWatch(FStringOutputDevice& ErrorHandler, const TCHAR* WatchString )
: WatchText(WatchString)
{
	guard(FDebuggerWatch::FDebuggerWatch);
	WatchNode = new FDebuggerWatchNode(ErrorHandler, WatchString);
	unguard;
}

void FDebuggerWatch::Refresh( const UObject* CurrentObject, const FFrame* CurrentFrame )
{
	guard(FDebuggerWatch::Refresh);

	if ( !CurrentObject || !CurrentFrame )
		return;

	Object = CurrentObject;
	Class = CurrentObject->GetClass();
	Function = Cast<UFunction>(CurrentFrame->Node);

	if ( WatchNode )
		WatchNode->ResetBase(Class, Object, Function, (BYTE*)Object, CurrentFrame->Locals);

	unguard;
}

UBOOL FDebuggerWatch::GetWatchValue( const UProperty*& OutProp, const BYTE*& OutPropAddr, INT& ArrayIndexOverride )
{
	guard(FDebuggerWatch::GetWatchValue);

	if ( WatchNode && WatchNode->Refresh(Class, Object, (BYTE*)Object ) )
		return WatchNode->GetWatchValue(OutProp, OutPropAddr, ArrayIndexOverride);

	return 0;

	unguard;
}

FDebuggerWatch::~FDebuggerWatch()
{
	guard(FDebuggerWatch::~FDebuggerWatch);

	if ( WatchNode )
		delete WatchNode;

	WatchNode = NULL;

	unguard;
}

/*-----------------------------------------------------------------------------
	FDebuggerDataWatch
-----------------------------------------------------------------------------*/
FDebuggerDataWatch::FDebuggerDataWatch( FStringOutputDevice& ErrorHandler, const TCHAR* WatchString )
: FDebuggerWatch(ErrorHandler, WatchString)
{ }

void FDebuggerDataWatch::Refresh( const UObject* CurrentObject, const FFrame* CurrentFrame )
{
	guard(FDebuggerDataWatch::Refresh);

	// reset the current value of the watch


	unguard;
}

UBOOL FDebuggerDataWatch::GetWatchValue( const UProperty*& OutProp, const BYTE*& OutPropAddr, INT& ArrayIndexOverride )
{
	guard(FDebuggerDataWatch::GetWatchValue);

	return 0;

	unguard;
}

UBOOL FDebuggerDataWatch::Modified() const
{
	guard(FDebuggerDataWatch::Modified);

	check(Property);

	// TODO for arrays that have been reduced in size, this will crash

	return Property->Identical( OriginalValue, DataAddress );
	unguard;
}

/*-----------------------------------------------------------------------------
	FDebuggerWatchNode
-----------------------------------------------------------------------------*/
FDebuggerWatchNode::FDebuggerWatchNode( FStringOutputDevice& ErrorHandler, const TCHAR* NodeText )
: NextNode(NULL), ArrayNode(NULL), PropAddr(NULL), Property(NULL), GlobalData(NULL), Base(NULL), LocalData(NULL),
  Function(NULL), TopObject(NULL), ContextObject(NULL), TopClass(NULL), ContextClass(NULL), Error(ErrorHandler)
{
	guard(FDebuggerWatchNode::FDebuggerWatchNode);

	TCHAR* Buffer = new TCHAR [ appStrlen(NodeText) + 1 ];
	appStrncpy(Buffer, NodeText, appStrlen(NodeText) + 1);

	TCHAR* NodeName = Buffer;
	TCHAR* Next = ParseNextName(NodeName);
	if ( Next )
		NextNode = new FDebuggerWatchNode(Error, Next);

	PropertyName = NodeName;

	FString ArrayDelim;
	if ( GetArrayDelimiter(PropertyName, ArrayDelim) )
		AddArrayNode(*ArrayDelim);

	delete[] Buffer;

	unguard;
}

FDebuggerWatchNode::~FDebuggerWatchNode()
{
	guard(FDebuggerWatchNode::~FDebuggerWatchNode);

	if ( NextNode )
		delete NextNode;

	if ( ArrayNode )
		delete ArrayNode;

	NextNode = NULL;
	ArrayNode = NULL;

	unguard;
}

UBOOL FDebuggerWatchNode::GetArrayDelimiter( FString& Test, FString& Result ) const
{
	guard(FDebuggerWatchNode::GetArrayDelimiter);

	Result = TEXT("");
	INT pos = Test.InStr(TEXT("["));
	if ( pos != INDEX_NONE )
	{
		Result = Test.Mid(pos+1);
		Test = Test.Left(pos);

		pos = Result.InStr(TEXT("]"),1);
		if ( pos != INDEX_NONE )
			Result = Result.Left(pos);
	}

	return Result.Len();

	unguard;
}

void FDebuggerWatchNode::AddArrayNode( const TCHAR* ArrayText )
{
	guard(FDebuggerWatchNode::AddArrayNode);

	if ( !ArrayText || !(*ArrayText) )
		return;

	ArrayNode = new FDebuggerArrayNode(Error, ArrayText);

	unguard;
}

void FDebuggerWatchNode::ResetBase( const UClass* CurrentClass, const UObject* CurrentObject, const UFunction* CurrentFunction, const BYTE* CurrentBase, const BYTE* CurrentLocals )
{
	guard(FDebuggerWatchNode::ResetBase);

	TopClass   = CurrentClass;
	TopObject  = CurrentObject;
	Function   = CurrentFunction;
	GlobalData = CurrentBase;
	LocalData  = CurrentLocals;

	if ( NextNode )
		NextNode->ResetBase(CurrentClass, CurrentObject, CurrentFunction, CurrentBase, CurrentLocals);

	if ( ArrayNode )
		ArrayNode->ResetBase(CurrentClass, CurrentObject, CurrentFunction, CurrentBase, CurrentLocals);

	unguard;
}

UBOOL FDebuggerWatchNode::Refresh( const UStruct* RelativeClass, const UObject* RelativeObject, const BYTE* Data )
{
	guard(FDebuggerWatchNode::Refresh);

	ContextObject = RelativeObject;
	ContextClass = RelativeClass;

	check(ContextClass);

	if ( !Data )
	{
		if ( appIsDebuggerPresent() )
			appDebugBreak();
		else
			appErrorf(NAME_FriendlyError, TEXT("Corrupted data found in user watch %s (class:%s function:%s)"), *PropertyName, ContextClass->GetName(), Function->GetName());

		return 0;
	}

	Property = NULL;
	PropAddr = NULL;
	Base = Data;

	if ( Data == GlobalData )
	{
		// Current context is the current function - allow searching the local parameters for properties
		Property = FindField<UProperty>( const_cast<UFunction*>(Function), *PropertyName);
		if ( Property )
			PropAddr = LocalData + Property->Offset;
	}

	if ( !Property )
	{
		Property = FindField<UProperty>( const_cast<UStruct*>(ContextClass), *PropertyName);
		if ( Property )
			PropAddr = Base + Property->Offset;
	}

/*	if ( !Property )
	{
		UObject* Obj = FindObject<UObject>(ANY_PACKAGE,*PropertyName);
		if ( Obj )
		{
			ContextObject = Obj;
			ContextClass  = Obj->GetClass();
		}
	}
*/
	ArrayIndex = GetArrayIndex();

	if ( !Property )
	{
		Error.Logf(TEXT("Member '%s' couldn't be found in local or global scope '%s'"), *PropertyName, ContextClass->GetName());
		return 0;
	}

	if ( ArrayIndex < INDEX_NONE )
		return 0;

	return 1;

	unguard;
}

INT FDebuggerWatchNode::GetArrayIndex() const
{
	guard(FDebuggerWatchNode::GetArrayIndex);

	if ( ArrayNode )
		return ArrayNode->GetArrayIndex();

	return INDEX_NONE;

	unguard;
}


// ArrayIndexOverride is to prevent PropertyToWatch from incorrectly interpreting individual elements of static arrays as the entire array

UBOOL FDebuggerWatchNode::GetWatchValue( const UProperty*& OutProp, const BYTE*& OutPropAddr, INT& ArrayIndexOverride )
{
	guard(FDebuggerWatchNode::GetWatchValue);

	if ( Property == NULL )
	{
//		if ( PropAddr == NULL )
//		{
			Error.Logf(TEXT("Member '%s' couldn't be found in local or global scope '%s'"), *PropertyName, ContextClass->GetName());
			return 0;
//		}
	}
	
	else if ( PropAddr == NULL )
	{
		Error.Logf(TEXT("Member '%s' couldn't be found in local or global scope '%s'"), *PropertyName, ContextClass->GetName());
		return 0;
	}

	const UStructProperty* StructProperty = NULL;
	const UArrayProperty* ArrayProperty = NULL;
	const UObjectProperty* ObjProperty = NULL;
	const UClassProperty* ClassProperty = ConstCast<UClassProperty>(Property);
	if ( !ClassProperty )
	{
		ObjProperty = ConstCast<UObjectProperty>(Property);
		if ( !ObjProperty )
		{
			ArrayProperty = ConstCast<UArrayProperty>(Property);
			if ( !ArrayProperty )
				StructProperty = ConstCast<UStructProperty>(Property);
		}
	}

	if ( ObjProperty )
	{
		const BYTE* Data = PropAddr + Max(ArrayIndex,0) * Property->ElementSize;
		const UObject* Obj = *(UObject**)Data;

		if ( NextNode )
		{
			if ( !Obj )
			{
				Error.Logf(TEXT("Expression could not be evaluated: Value of '%s' is None"), *PropertyName);
				return 0;
			}

			if ( !NextNode->Refresh( Obj ? Obj->GetClass() : ObjProperty->PropertyClass, Obj, (BYTE*)Obj ) )
				return 0;

			return NextNode->GetWatchValue( OutProp, OutPropAddr, ArrayIndexOverride );
		}

		OutProp = Property;
		OutPropAddr = Data;
		ArrayIndexOverride = ArrayIndex;
		return 1;
	}

	else if ( ClassProperty )
	{
		const BYTE* Data = PropAddr + Max(ArrayIndex,0) * Property->ElementSize;
		const UClass* Cls = *(UClass**)Data;
		if ( NextNode )
		{
			if ( !Cls )
			{
				Error.Logf(TEXT("Expression couldn't be evaluated: Value of '%s' is None"), *PropertyName);
				return 0;
			}

#if ENGINE_VERSION > 2225
			if ( !NextNode->Refresh( Cls ? Cls : ClassProperty->MetaClass, Cls ? Cls->GetDefaultObject() : NULL, (BYTE*)&Cls->Defaults(0)) )
#else
			if ( !NextNode->Refresh( Cls ? Cls : ClassProperty->MetaClass, Cls ? (UObject*)&Cls->Defaults(0) : NULL, (BYTE*)&Cls->Defaults(0)) )
#endif
				return 0;

			return NextNode->GetWatchValue( OutProp, OutPropAddr, ArrayIndexOverride );
		}

		OutProp = Property;
		OutPropAddr = Data;
		ArrayIndexOverride = ArrayIndex;
		return 1;
	}

	else if ( StructProperty )
	{
		const BYTE* Data = PropAddr + Max(ArrayIndex,0) * Property->ElementSize;
		UStruct* Struct = StructProperty->Struct;
		if ( Struct )
		{
			if ( NextNode )
			{
				if ( !NextNode->Refresh( Struct, ContextObject, Data ) )
					return 0;

                return NextNode->GetWatchValue(OutProp, OutPropAddr, ArrayIndexOverride);
			}

			OutProp = StructProperty;
			OutPropAddr = Data;
			ArrayIndexOverride = ArrayIndex;
			return 1;
		}

		Error.Logf(TEXT("No data could be found for struct '%s'"), Property->GetName());
		return 0;
	}

	else if ( ArrayProperty )
	{
		const FArray* Array = (FArray*)PropAddr;
		if ( Array )
		{
			// If the array index is -1, then we want the entire array, not just a single element
			if ( ArrayIndex != INDEX_NONE )
			{
				if ( ArrayIndex < 0 || ArrayIndex >= Array->Num() )
				{
					Error.Logf(TEXT("Index (%i) out of bounds: %s array only has %i element%s"), ArrayIndex, Property->GetName(), Array->Num(), Array->Num() == 1 ? TEXT("") : TEXT("s"));
					return 0;
				}

				ObjProperty = NULL;
				StructProperty = NULL;
				ClassProperty = ConstCast<UClassProperty>(ArrayProperty->Inner);
				if ( !ClassProperty )
				{
					ObjProperty = Cast<UObjectProperty>(ArrayProperty->Inner);
					if ( !ObjProperty )
                        StructProperty = ConstCast<UStructProperty>(ArrayProperty->Inner);
				}

				if ( ObjProperty )
				{
					const BYTE* Data = ((BYTE*)Array->GetData() + ArrayIndex * ObjProperty->ElementSize);
					const UObject* Obj = *(UObject**) Data;

					// object is none
					if ( NextNode )
					{
						if ( !NextNode->Refresh( Obj ? Obj->GetClass() : ObjProperty->PropertyClass, Obj, (BYTE*)Obj ) )
							return 0;

						return NextNode->GetWatchValue( OutProp, OutPropAddr, ArrayIndexOverride );
					}

					OutProp = ObjProperty;
					OutPropAddr = Data;
					ArrayIndexOverride = ArrayIndex;
					return 1;
				}

				else if ( ClassProperty )
				{
					const BYTE* Data = ((BYTE*)Array->GetData() + ClassProperty->ElementSize * ArrayIndex);
					const UClass* Cls = *(UClass**) Data;

					if ( NextNode )
					{
#if ENGINE_VERSION > 2225
						if ( !NextNode->Refresh( Cls ? Cls : ClassProperty->MetaClass, Cls ? Cls->GetDefaultObject() : NULL, Cls ? (BYTE*)&Cls->Defaults(0) : NULL ) )
#else
						if ( !NextNode->Refresh( Cls ? Cls : ClassProperty->MetaClass, Cls ? (UObject*)&Cls->Defaults(0) : NULL, Cls ? (BYTE*)&Cls->Defaults(0) : NULL ) )
#endif
							return 0;

						return NextNode->GetWatchValue(OutProp, OutPropAddr, ArrayIndexOverride);
					}

					OutProp = ClassProperty;
					OutPropAddr = Data;
					ArrayIndexOverride = ArrayIndex;
					return 1;
				}

				else if ( StructProperty )
				{
					const BYTE* Data = (BYTE*)Array->GetData() + StructProperty->ElementSize * ArrayIndex;
					UStruct* Struct = StructProperty->Struct;
					if ( Struct )
					{
						if ( NextNode )
						{
							if ( !NextNode->Refresh( Struct, NULL, Data ) )
								return 0;

							return NextNode->GetWatchValue(OutProp, OutPropAddr, ArrayIndexOverride);
						}

						OutProp = StructProperty;
						OutPropAddr = Data;
						ArrayIndexOverride = ArrayIndex;
						return 1;
					}

					Error.Logf(TEXT("No data could be found for struct '%s'"), StructProperty->GetName());
					return 0;
				}

				else 
				{
					OutProp = ArrayProperty->Inner;
					OutPropAddr = (BYTE*)Array->GetData() + OutProp->ElementSize * ArrayIndex;
					ArrayIndexOverride = ArrayIndex;
					return 1;
				}
			}
		}

		else
		{
			Error.Logf(TEXT("No data could be found for array '%s'"), Property->GetName());
			return 0;
		}
	}

	OutProp = Property;
	OutPropAddr = PropAddr + Max(ArrayIndex,0) * Property->ElementSize;
	ArrayIndexOverride = ArrayIndex;

	return 1;

	unguard;
}

/*-----------------------------------------------------------------------------
	FDebuggerArrayNode
-----------------------------------------------------------------------------*/

FDebuggerArrayNode::FDebuggerArrayNode(FStringOutputDevice& ErrorHandler, const TCHAR* ArrayText )
: FDebuggerWatchNode(ErrorHandler, ArrayText)
{
	guard(FDebuggerArrayNode::FDebuggerArrayNode);
	unguard;
}

FDebuggerArrayNode::~FDebuggerArrayNode()
{
	guard(FDebuggerArrayNode::~FDebuggerArrayNode);
	unguard;
}

void FDebuggerArrayNode::ResetBase( const UClass* CurrentClass, const UObject* CurrentObject, const UFunction* CurrentFunction, const BYTE* CurrentBase, const BYTE* CurrentLocals )
{
	guard(FDebuggerArrayNode::ResetBase);

	Value = INDEX_NONE;
#if ENGINE_VERSION > 2225
	if ( PropertyName.IsNumeric() )
#else
	if ( IsNumeric(PropertyName) )
#endif
	{
		Value = appAtoi(*PropertyName);
		return;
	}

	FDebuggerWatchNode::ResetBase(CurrentClass, CurrentObject, CurrentFunction, CurrentBase, CurrentLocals);
	Refresh(CurrentClass, CurrentObject, CurrentBase);

	unguard;
}

INT FDebuggerArrayNode::GetArrayIndex()
{
	guard(FDebuggerArrayNode::GetArrayIndex);

	// if the property is simply a number, just return that
#if ENGINE_VERSION > 2225
	if ( Value != INDEX_NONE && PropertyName.IsNumeric() )
#else
	if ( Value != INDEX_NONE && IsNumeric(PropertyName) )
#endif
		return Value;

	const UProperty* Prop = NULL;
	const BYTE* Data = NULL;

	INT dummy(0);
	if ( GetWatchValue(Prop, Data, dummy) )
	{
		TCHAR Buffer [32] = TEXT("");

		// Must const_cast here because ExportTextItem isn't made const even though it doesn't modify the property value.
		Prop->ExportTextItem(Buffer, const_cast<BYTE*>(Data), NULL, NULL);
		Value = appAtoi(Buffer);
	}
	else return INDEX_NONE - 1;

	return Value;

	unguard;
}

/*-----------------------------------------------------------------------------
	Breakpoints
-----------------------------------------------------------------------------*/

INT FBreakpoint::breakpointIndex;

FBreakpoint::FBreakpoint( const TCHAR* InClassName, INT InLine )
{
	guard(FBreakpoint::FBreakpoint);

	if (breakpointIndex < 0)
		breakpointIndex = 0;
	FString bpName = FString::Printf(TEXT("Breakpoint%iline"), breakpointIndex), bpClass = FString::Printf(TEXT("Breakpoint%iclass"), breakpointIndex);

	ClassName = InClassName;
	Line = InLine;
	IsEnabled = true;

	// Make sure breakpoints are saved
	GConfig->SetInt(TEXT("DEBUGGER.BREAKPOINTS"), *bpName, Line, TEXT("UDebugger.ini"));
	GConfig->SetString(TEXT("DEBUGGER.BREAKPOINTS"), *bpClass, *ClassName, TEXT("UDebugger.ini"));
	GConfig->Flush(false, TEXT("UDebugger.ini"));

	this->breakpointIndex++;
	unguard;
}

FBreakpoint::~FBreakpoint()
{
	guard(FBreakpoint::~FBreakpoint);

	this->breakpointIndex--;
	if (breakpointIndex < 0)
		breakpointIndex = 0;

	unguard;
}

FBreakpointManager::FBreakpointManager( UDebuggerCore* InParent )
{
}



FBreakpointManager::~FBreakpointManager()
{
	Debugger = 0;
}


UBOOL FBreakpointManager::QueryBreakpoint( const TCHAR* sClassName, INT sLine )
{
	guard(FBreakpointManager::QueryBreakpoint);

	for(int i=0;i<Breakpoints.Num();i++)
	{
		if ( Breakpoints(i).IsEnabled && Breakpoints(i).ClassName == sClassName && Breakpoints(i).Line == sLine )
			return 1;
	}
	
	return 0;
	unguard;
}


void FBreakpointManager::SetBreakpoint( const TCHAR* sClassName, INT sLine )
{
	guard(FBreakpointManager::SetBreakpoint);

	for(int i=0;i<Breakpoints.Num();i++)
	{
		if ( Breakpoints(i).ClassName == sClassName && Breakpoints(i).Line == sLine )
			return;
	}

	new(Breakpoints) FBreakpoint( sClassName, sLine );
	unguard;
}


void FBreakpointManager::RemoveBreakpoint( const TCHAR* sClassName, INT sLine )
{
	guard(FBreakpointManager::RemoveBreakpoint);
	INT i = 0;
	for( i = 0; i < Breakpoints.Num(); i++)
		if ( Breakpoints(i).ClassName == sClassName && Breakpoints(i).Line == sLine )
			break;

	if (i < Breakpoints.Num())
	{
		TMultiMap<FString,FString>* Sec = GConfig->GetSectionPrivate( TEXT("DEBUGGER.BREAKPOINTS"), 1, 0, TEXT("UDebugger.ini") );
		check(Sec);
		
		Sec->Empty();
		Breakpoints.Remove(i);

		FString bpLine, bpClass;
		for (i = 0; i < Breakpoints.Num(); i++)
		{
			bpLine = FString::Printf(TEXT("Breakpoint%iline"),i); bpClass = FString::Printf(TEXT("Breakpoint%iclass"), i);
			GConfig->SetInt(TEXT("DEBUGGER.BREAKPOINTS"), *bpLine, Breakpoints(i).Line, TEXT("UDebugger.ini"));
			GConfig->SetString(TEXT("DEBUGGER.BREAKPOINTS"), *bpClass, *Breakpoints(i).ClassName, TEXT("UDebugger.ini"));
		}

		GConfig->Flush(0, TEXT("UDebugger.ini"));
	}
	unguard;
}



void FBreakpointManager::Serialize( FArchive& Ar )
{
	guard(FBreakpointManager::Serialize);

	// Make sure we're loading the right type of file
	FString Ident(TEXT("UDEBUGV1"));
	Ar << Ident;

	if ( Ident != TEXT("UDEBUGV1") )
	{
		GWarn->Logf(TEXT("Incorrect breakpoint file format!"));
		return;
	}

	if ( Ar.IsLoading() )
	{
		INT Num = 0;
		Ar << Num;
		Breakpoints.Empty();
		GLog->Logf(TEXT("Loading %i breakpoints."), Num );
		Breakpoints.Add( Num );
		for(int i=0;i<Num;i++)
		{
			INT VerifyNum = 0;
			Ar << VerifyNum;

			// File is bad... reset the breakpoints and return
			if ( i != VerifyNum )
			{
				GWarn->Logf(TEXT("Error, Expected %i, got %i."), i, VerifyNum );
				Breakpoints.Empty();
				return;
			}
			Ar << Breakpoints(i).Line;
			Ar << Breakpoints(i).ClassName;
			Ar << Breakpoints(i).IsEnabled;
		}
	}
	else
	{
		INT Num = Breakpoints.Num();
		Ar << Num;
		GLog->Logf(TEXT("Saving %i breakpoints."), Num );
		for(int i=0;i<Num;i++)
		{
			Ar << i;
			Ar << Breakpoints(i).Line;
			Ar << Breakpoints(i).ClassName;
			Ar << Breakpoints(i).IsEnabled;
		}
	}

	FString Term(TEXT("ENDV1"));
	Ar << Term;

	if ( Term != TEXT("ENDV1") )
	{
		GWarn->Logf(TEXT("Unexptected terminator."));
		Breakpoints.Empty();
		return;
	}
	unguard;
}
/*-----------------------------------------------------------------------------
	Debugger states
-----------------------------------------------------------------------------*/
FDebuggerState::FDebuggerState(UDebuggerCore* const inDebugger)
: CurrentNode(NULL), Debugger(inDebugger), LineNumber(INDEX_NONE), EvalDepth(Debugger->CallStack->StackDepth)
{
	guard(FDebuggerState::FDebuggerState);

	const FStackNode* Node = Debugger->GetCurrentNode();
	if ( Node )	
		LineNumber = Node->GetLine();

	unguard;
}

FDebuggerState::~FDebuggerState()
{
}

void FDebuggerState::UpdateStackInfo( const FStackNode* CNode )
{
	guard(FDebuggerState::UpdateStackInfo);

	if ( Debugger != NULL && ((Debugger->IsDebugging && CNode != GetCurrentNode()) || (CNode == NULL)) )
	{
		Debugger->StackChanged(CNode);
		if ( CNode == NULL )
			Debugger->IsDebugging = 0;
	}

	SetCurrentNode(CNode);
	unguard;
}

/*-----------------------------------------------------------------------------
	Constructors.
-----------------------------------------------------------------------------*/

DSIdleState::DSIdleState(UDebuggerCore* const inDebugger)
: FDebuggerState(inDebugger)
{
	guard(DSIdleState::DSIdleState);
	Debugger->IsDebugging = 0;
	unguard;
}

DSWaitForInput::DSWaitForInput(UDebuggerCore* const inDebugger)
: FDebuggerState(inDebugger)
{
	guard(DSWaitForInput::DSWaitForInput);
	Debugger->IsDebugging = 1;
	unguard;
}

DSWaitForCondition::DSWaitForCondition(UDebuggerCore* const inDebugger)
: FDebuggerState(inDebugger)
{
	guard(DSWaitForCondition::DSWaitForCondition);

	Debugger->IsDebugging = 0;

	unguard;
}

DSBreakOnChange::DSBreakOnChange( UDebuggerCore* const inDebugger, const TCHAR* WatchText, FDebuggerState* NewState )
: DSWaitForCondition(inDebugger), SubState(NewState), Watch(NULL), bDataBreak(0)
{
	guard(DSBreakOnChange::DSBreakOnChange);

	Watch = new FDebuggerDataWatch(Debugger->ErrorDevice, WatchText);
	const FStackNode* StackNode = SubState ? SubState->GetCurrentNode() : NULL;
	const UObject* Obj = StackNode ? StackNode->Object : NULL;
	const FFrame* Node = StackNode ? StackNode->StackNode : NULL;

	Watch->Refresh( Obj, Node );

	unguard;
}

DSBreakOnChange::~DSBreakOnChange()
{
	guard(DSBreakOnChange::~DSBreakOnChange);

	if ( SubState )
		delete SubState;

	SubState = NULL;

	unguard;
}

DSRunToCursor::DSRunToCursor( UDebuggerCore* const inDebugger )
: DSWaitForCondition(inDebugger) 
{ }


DSStepOut::DSStepOut( UDebuggerCore* const inDebugger ) 
: DSWaitForCondition(inDebugger)
{ }

DSStepInto::DSStepInto( UDebuggerCore* const inDebugger )
: DSWaitForCondition(inDebugger)
{ }

DSStepOverStack::DSStepOverStack( const UObject* inObject, UDebuggerCore* const inDebugger ) 
: DSWaitForCondition(inDebugger), EvalObject(inObject)
{
	guard(DSStepOverStack::DSStepOverStack);
	unguard;
}

/*-----------------------------------------------------------------------------
	DSBreakOnChange specifics.
-----------------------------------------------------------------------------*/
void DSBreakOnChange::SetCurrentNode( const FStackNode* Node )
{
	guard(DSBreakOnChange::SetCurrentNode);

	if ( SubState )
		SubState->SetCurrentNode(Node);

	else DSWaitForCondition::SetCurrentNode(Node);
	unguard;
}

FDebuggerState* DSBreakOnChange::GetCurrent()
{
	guard(DSBreakOnChange::GetCurrent);

	if ( SubState )
		return SubState->GetCurrent();

	return FDebuggerState::GetCurrent();

	unguard;
}

const FStackNode* DSBreakOnChange::GetCurrentNode() const
{
	guard(DSBreakOnChange::GetCurrentNode);

	if ( SubState )
		return SubState->GetCurrentNode();

	return FDebuggerState::GetCurrentNode();
	unguard;
}

UBOOL DSBreakOnChange::InterceptNewState( FDebuggerState* NewState )
{
	guard(DSBreakOnChange::InterceptNewState);

	if ( !NewState )
		return 0;

	if ( SubState )
	{
		if ( SubState->InterceptNewState(NewState) )
			return 1;

		delete SubState;
	}

	SubState = NewState;
	return 1;

	unguard;
}

UBOOL DSBreakOnChange::InterceptOldState( FDebuggerState* OldState )
{
	guard(DSBreakOnChange::InterceptOldState);

	if ( !OldState || !SubState || OldState == this )
		return 0;

	if ( SubState && SubState->InterceptOldState(OldState) )
		return 1;

	return OldState == SubState;
	unguard;
}

/*-----------------------------------------------------------------------------
	HandleInput.
-----------------------------------------------------------------------------*/
void DSBreakOnChange::HandleInput( EUserAction Action )
{
	guard(DSBreakOnChange::HandleInput);

	if ( Action >= UA_MAX )
		appErrorf(NAME_FriendlyError, TEXT("Invalid UserAction received by HandleInput()!"));

	if ( Action != UA_Exit && Action != UA_None && bDataBreak )
	{
		// refresh the watch's value with the current value
	}

	bDataBreak = 0;
	if ( SubState )
		SubState->HandleInput(Action);

	unguard;
}

void DSWaitForInput::HandleInput( EUserAction UserInput ) 
{
	guard(DSWaitForInput::HandleInput);

	ContinueExecution();
	switch ( UserInput )
	{
	case UA_RunToCursor:
		/*CHARRANGE sel;

		RichEdit_ExGetSel (Parent->Edit.hWnd, &sel);

		if ( sel.cpMax != sel.cpMin )
		{

		//appMsgf(0,TEXT("Invalid cursor position"));			
			return;
		}
		Parent->ChangeState( new DSRunToCursor( sel.cpMax, Parent->GetCallStack()->GetStackDepth() ) );*/
		Debugger->IsDebugging = 0;
		break;
	case UA_Exit:
		GIsRequestingExit = 1;
		Debugger->Close();
		break;
	case UA_StepInto:
		Debugger->ChangeState( new DSStepInto(Debugger) );
		break;

	case UA_StepOver:
	/*	if ( CurrentInfo != TEXT("RETURN") && CurrentInfo != TEXT("RETURNNOTHING") )
		{
			
			Debugger->ChangeState( new DSStepOver( CurrentObject,
												 CurrentClass,
												 CurrentStack, 
												 CurrentLine, 
												 CurrentPos, 
												 CurrentInfo,
												 Debugger->GetCallStack()->GetStackDepth(), Debugger ) );
			

		}*/
		debugf(TEXT("Warning: UA_StepOver currently unimplemented"));
//		Debugger->IsDebugging = 1;
		break;
	case UA_StepOverStack:
		{
			const FStackNode* Top = Debugger->CallStack->GetTopNode();
			check(Top);

			Debugger->ChangeState( new DSStepOverStack(Top->Object,Debugger) );
		}

		break;

	case UA_StepOut:
		Debugger->ChangeState( new DSStepOut(Debugger) );
		break;

	case UA_Go:
		Debugger->ChangeState( new DSIdleState(Debugger) );
		break;
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	Process.
-----------------------------------------------------------------------------*/
void FDebuggerState::Process( UBOOL bOptional )
{
	guard(FDebuggerState::Process);

	if ( !Debugger->IsClosing && Debugger->CallStack->StackDepth && EvaluateCondition(bOptional) )
		Debugger->Break();

	unguard;
}

void DSWaitForInput::Process(UBOOL bOptional) 
{
	guard(DSWaitForInput::Process);

	if ( !GIsScriptable || Debugger->IsClosing )
		return;

	Debugger->AccessedNone = 0;
	Debugger->BreakASAP = 0;

	Debugger->UpdateInterface();
	bContinue = 0;

	// Disable all script execution while we're waiting for input
	GIsScriptable = 0;

#ifdef PROFILEDEBUGGER
	DOUBLE Start = appSeconds();
#endif

	Debugger->GetInterface()->Show();

#ifdef PROFILEDEBUGGER
	DOUBLE Time = (appSeconds() - Start) * 1000;
	debugf(TEXT("ShowInterface() took %i seconds"), (INT)Time);
#endif

	PumpMessages();

	GIsScriptable = 1;

	unguard;
}

void DSWaitForCondition::Process(UBOOL bOptional) 
{
	guard(DSWaitForCondition::Process);

	check(Debugger);

	const FStackNode* Node = GetCurrentNode();
	check(Node);

	if ( !Debugger->IsClosing && Debugger->CallStack->StackDepth && EvaluateCondition(bOptional) )
	{
		// Condition was MET. We now delegate control to a
		// user-controlled state.

		if ( Node && Node->StackNode && Node->StackNode->Node &&
			 Node->StackNode->Node->IsA(UClass::StaticClass()) )
		{
			if ( appIsDebuggerPresent() )
				appDebugBreak();

			return;
		}

		Debugger->Break();
	}

	unguard;
}

void DSBreakOnChange::Process(UBOOL bOptional)
{
	guard(DSBreakOnChange::Process);

	check(Debugger);

	const FStackNode* Node = GetCurrentNode();
	check(Node);

	if ( !Debugger->IsClosing && Debugger->CallStack->StackDepth && EvaluateCondition(bOptional) )
	{
		if ( Node && Node->StackNode && Node->StackNode->Node &&
			 Node->StackNode->Node->IsA(UClass::StaticClass()) )
		{
			if ( appIsDebuggerPresent() )
				appDebugBreak();

			return;
		}

		// TODO : post message box with reason for breaking the udebugger
		Debugger->Break();
		return;
	}

	if ( SubState )
		SubState->Process(bOptional);

	unguard;
}

void DSWaitForInput::PumpMessages() 
{
	guard(DSWaitForInput::PumpMessages);

	GIsRunning = false;
	while( !bContinue && !Debugger->IsClosing && !GIsRequestingExit )
	{
		guard(MessagePump);
		MSG Msg;
		
		while( PeekMessageX(&Msg,NULL,0,0,PM_REMOVE) )
		{
			if( Msg.message == WM_QUIT )
			{
				GIsRequestingExit = 1;
				ContinueExecution();
			}

			guardSlow(TranslateMessage);
			TranslateMessage( &Msg );
			unguardfSlow(( TEXT("%08X %i"), (INT)Msg.hwnd, Msg.message ));

			guardSlow(DispatchMessage);
			DispatchMessageX( &Msg );
			unguardfSlow(( TEXT("%08X %i"), (INT)Msg.hwnd, Msg.message ));
		}
		unguard;
	}
	GIsRunning = true;
	unguard;
}

void DSWaitForInput::ContinueExecution() 
{
	bContinue = TRUE;
}

UBOOL FDebuggerState::EvaluateCondition( UBOOL bOptional )
{
	guard(FDebuggerState::EvaluateCondition);

	const FStackNode* Node = GetCurrentNode();
	check(Node);
	check(Debugger->CallStack->StackDepth);
	check(Debugger);
	check(Debugger->BreakpointManager);
	check(!Debugger->IsClosing);

	// Check if we've hit a breakpoint
	INT Line = Node->GetLine();
	if ( /*Line != LineNumber && */Debugger->BreakpointManager->QueryBreakpoint(Debugger->GetStackOwnerClass(Node->StackNode)->GetPathName(), Line) )
		return 1;

	return 0;
	unguard;
}

UBOOL DSWaitForCondition::EvaluateCondition(UBOOL bOptional)
{
	return FDebuggerState::EvaluateCondition(bOptional);
}

UBOOL DSRunToCursor::EvaluateCondition(UBOOL bOptional)
{
	return DSWaitForCondition::EvaluateCondition(bOptional);
}

UBOOL DSStepOut::EvaluateCondition(UBOOL bOptional)
{
	guard(DSStepOut::EvaluateCondition);

	check(Debugger->CallStack->StackDepth);
	check(!Debugger->IsClosing);

	const FStackNode* Node = GetCurrentNode();
	check(Node);

	if ( Debugger->CallStack->StackDepth >= EvalDepth )
		return FDebuggerState::EvaluateCondition(bOptional);

	// ?! Is this the desired result?
	// This seems like it could possibly result in the udebugger skipping a function while stepping out, if the
	// opcode was DI_PrevStack when 'stepout' was received
/*	if ( bOptional )
	{
		if ( !Debugger->CallStack->StackDepth < EvalDepth - 1 )
			return DSIdleState::EvaluateCondition(bOptional);
	}
	else*/ 
	if ( Debugger->CallStack->StackDepth < EvalDepth )
		return 1;

    return DSWaitForCondition::EvaluateCondition(bOptional);

	unguard;
}

UBOOL DSStepInto::EvaluateCondition(UBOOL bOptional)
{
	guard(DSStepInto::EvaluateCondition);

	check(Debugger->CallStack->StackDepth);
	check(!Debugger->IsClosing);
	const FStackNode* Node = GetCurrentNode();
	check(Node);

	return Debugger->CallStack->StackDepth != EvalDepth || Node->GetLine() != LineNumber;

	unguard;
}

UBOOL DSStepOverStack::EvaluateCondition(UBOOL bOptional)
{
	guard(DSStepOverStack::EvaluateCondition);

	check(Debugger->CallStack->StackDepth);
	check(!Debugger->IsClosing);
	const FStackNode* Node = GetCurrentNode();
	check(Node);

	if ( Debugger->CallStack->StackDepth != EvalDepth || Node->GetLine() != LineNumber )
	{
		if ( Debugger->CallStack->StackDepth < EvalDepth )
			return 1;
		if ( Debugger->CallStack->StackDepth == EvalDepth )
			return !bOptional;
		return FDebuggerState::EvaluateCondition(bOptional);
	}
	return 0;

	unguard;
}

UBOOL DSBreakOnChange::EvaluateCondition( UBOOL bOptional )
{
	guard(DSBreakOnChange::EvaluateCondition);
	
	// TODO
	//first, evaluate whether our data watch has changed...if so, set a flag to indicate that we've requested the udbegger to break
	// (will be checked in HandleInput), and break
	// otherwise, just execute normal behavior
	check(Watch);
	if ( Watch->Modified() )
	{
		bDataBreak = 1;
		return 1;
	}

	if ( SubState )
		return SubState->EvaluateCondition(bOptional);

	return DSWaitForCondition::EvaluateCondition(bOptional);
	unguard;
}

/*-----------------------------------------------------------------------------
	Primary UDebugger methods.
-----------------------------------------------------------------------------*/
void UDebuggerCore::ChangeState( FDebuggerState* NewState, UBOOL bImmediately )
{
	guard(UDebuggerCore::ChangeState);

	if( PendingState )
		delete PendingState;

	PendingState = NewState ? NewState : NULL;
	if ( bImmediately && PendingState )
	{
		AccessedNone = 0;
		BreakASAP = 0;
		PendingState->UpdateStackInfo(CurrentState ? CurrentState->GetCurrentNode() : NULL);
		ProcessPendingState();
		CurrentState->Process();
	}

	unguard;
}

void UDebuggerCore::ProcessPendingState()
{
	guard(UDebuggerCore::ProcessPendingState);

	if ( PendingState )
	{
		if ( CurrentState )
		{
			if ( CurrentState->InterceptNewState(PendingState) )
			{
				PendingState = NULL;
				return;
			}

			if ( !PendingState->InterceptOldState(CurrentState) )
				delete CurrentState;
		}

		CurrentState = PendingState;
		PendingState = NULL;
	}

	unguard;
}

// Main debugger entry point
#if ENGINE_VERSION > 2225
void UDebuggerCore::DebugInfo( const UObject* Debugee, const FFrame* Stack, BYTE OpCode, INT LineNumber, INT InputPos )
#else
void UDebuggerCore::DebugInfo( UObject* Debugee, FFrame* Stack, FString InfoType, int LineNumber, int InputPos )
#endif
{
	guard(UDebuggerCore::DebugInfo);

	// Wierd Devastation fix
	if ( Stack->Node->IsA( UClass::StaticClass() ) )
	{
		if ( appIsDebuggerPresent() )
			appDebugBreak();

		return;
	}

	// Process any waiting states
	ProcessPendingState();
	check(CurrentState);

	if ( CallStack && BreakpointManager && CurrentState )
	{
		if ( IsClosing )
		{
			if ( Interface->IsLoaded() )
				Interface->Close();
		}
		else if ( !GIsRequestingExit )
		{
			// Returns true if it handled updating the stack
			if ( CallStack->UpdateStack(Debugee, Stack, LineNumber, InputPos, OpCode) )
				return;

			if ( CallStack->StackDepth > 0 )
			{
				CurrentState->UpdateStackInfo( CallStack->GetTopNode() );

				// Halt execution, and wait for user input if we have a breakpoint for this line
//				if ( BreakpointManager->QueryBreakpoint(pName,LineNumber) || (AccessedNone && BreakOnNone) || BreakASAP )
				if ( (AccessedNone && BreakOnNone) || BreakASAP )
					Break();
				else
				{
					// Otherwise, update the debugger's state with the currently executing stacknode, then
					// pass control to debugger state for further processing (i.e. if stepping over, are we ready to break again)
					CurrentState->Process();
				}
			}
		}
	}

	unguard;
}

// Update the call stack, adding new FStackNodes if necessary
// Take into account latent state stack anomalies...
UBOOL FCallStack::UpdateStack( const UObject* Debugee, const FFrame* FStack, int LineNumber, int InputPos, BYTE OpCode )
{
	guard(UDebuggerCore::FCallStack::UpdateStack);

	check(StackDepth == Stack.Num());

	if ( StackDepth == 0 )
		QueuedCommands.Empty();

	FDebuggerState* CurrentState = Parent->GetCurrentState();

	switch ( OpCode )
	{
	// Check if stack change is due to a latent function in a state (meaning thread of execution
	case DI_PrevStackLatent:
		{
			if ( StackDepth != 1 )
				Parent->DumpStack();

		#if ENGINE_VERSION > 2225
			if ( StackDepth != 1 )
				appErrorf(NAME_FriendlyError, TEXT("PrevStackLatent received with stack depth != 1.  Verify that all packages have been compiled in debug mode."));

			Stack.Pop();
			StackDepth--;
		#else
			Stack.Empty();
			StackDepth = 0;
		#endif

			CurrentState->UpdateStackInfo(NULL);
			return 1;
		}

	// Normal change... pop the top stack off the call stack
	case DI_PrevStack:
		{
			if ( StackDepth <= 0 )
				Parent->DumpStack();

			if ( StackDepth <= 0 )
				appErrorf(NAME_FriendlyError, TEXT("PrevStack received with StackDepth <= 0.  Verify that all packages have been compiled in debug mode."));

			FStackNode* Last = &Stack.Last();
			if ( Last->StackNode != FStack )
			{
				if ( !Last->StackNode->Node->IsA(UState::StaticClass()) && FStack->Node->IsA(UState::StaticClass()) )
				{
					// We've received a call to execDebugInfo() from UObject::GotoState() as a result of the state change,
					// but we were executing a function, not state code.
					// Queue this prevstack until we're back in state code
					new(QueuedCommands) StackCommand( FStack, OpCode, LineNumber );
					return 1;
				}
			}

			if ( Last->StackNode != FStack )
				appErrorf(NAME_FriendlyError, TEXT("UDebugger CallStack inconsistency detected.  Verify that all packages have been compiled in debug mode."));

			Stack.Pop();
			StackDepth--;

			// If we're returning to state code (StackDepth == 1 && stack node is an FStateFrame), and the current object has been marked
			// to be deleted, we'll never receive the PREVSTACK (since state code isn't executed for actors marked bDeleteMe)
			// Remove this stacknode now, but don't change the current state of the debugger (in case we were stepping into, or out of)
			if ( StackDepth == 1 )
			{
				const FFrame* Node = Stack(0).StackNode;
				if ( Node && Node->Node && Node->Node->IsA(UState::StaticClass()) && Node->Object->IsPendingKill() )
				{
					Stack.Pop();
					StackDepth--;

					CurrentState->UpdateStackInfo(NULL);
					return 1;
				}
			}

			if ( StackDepth == 0 )
				CurrentState->UpdateStackInfo(NULL);

			else
			{
				CurrentState->UpdateStackInfo( &Stack.Last() );
				CurrentState->Process(1);
			}

			// If we're returning to state code and we have a queued command for this state, execute it now
			if ( StackDepth == 1 && QueuedCommands.Num() )
			{
				StackCommand Command = QueuedCommands(0);
				if ( Command.Frame == Stack(0).StackNode )
				{
					QueuedCommands.Remove(0);
					UpdateStack( Debugee, Command.Frame, Command.LineNumber, InputPos, Command.OpCode );
				}
			}

			return 1;
		}
		
	case DI_PrevStackState:
		{
			if ( StackDepth == 1 && FStack->Node->IsA(UState::StaticClass()) )
			{
				FStackNode& Node = Stack(0);
				UpdateStack( Debugee, Node.StackNode, Node.Lines.Last() + 1, 0, DI_PrevStack );
				return 1;
			}

			goto Default;
		}


	case DI_NewStack:
		{
			FStackNode* CurrentTop = NULL;
			if (StackDepth)
				CurrentTop = &Stack.Last();

			if ( CurrentTop && CurrentTop->StackNode == FStack )
			{
		#if ENGINE_VERSION > 2225
				Parent->DumpStack();
				if ( appIsDebuggerPresent() )
					appDebugBreak();
				else
					appErrorf(NAME_FriendlyError, TEXT("Received call for new stack with identical stack node!  Verify that all packages have been compiled in debug mode."));
		#else
				return 1;
		#endif
			}

			CurrentTop = new(Stack) 
			FStackNode( Debugee, FStack, Parent->GetStackOwnerClass(FStack),
						StackDepth, LineNumber, InputPos, OpCode );
			CurrentState->UpdateStackInfo( CurrentTop );
			StackDepth++;

			CurrentState->Process();

			return 1;
		}

	case DI_NewStackLatent:
		{
		#if ENGINE_VERSION > 2225
			if ( StackDepth )
			{
				Parent->DumpStack();
				appErrorf(NAME_FriendlyError,TEXT("Received LATENTNEWSTACK with stack depth  Object:%s Class:%s Line:%i OpCode:%s"), Parent->GetStackOwnerClass(FStack)->GetName(), Debugee->GetName(), LineNumber, OpCode);
			}

		#else
			if ( StackDepth )
			{
				Stack.Empty();
				StackDepth = 0;
			}
		#endif
			CurrentState->UpdateStackInfo(new(Stack) FStackNode(Debugee, FStack, Parent->GetStackOwnerClass(FStack), StackDepth, LineNumber,InputPos,OpCode));
			StackDepth++;

			CurrentState->Process();

			return 1;
		}

	case DI_NewStackLabel:
		{
			if ( StackDepth == 0 )
			{
				// was result of a native gotostate
				CurrentState->UpdateStackInfo(new(Stack) FStackNode(Debugee, FStack, Parent->GetStackOwnerClass(FStack), StackDepth, LineNumber,InputPos,OpCode));
				StackDepth++;

				CurrentState->Process();
				return 1;
			}

			else
			{
				Stack.Last().Update( LineNumber, InputPos, OpCode, StackDepth );
				return 0;
			}
		}
	}

	// Stack has not changed. Update the current node with line number and current opcode type
Default:
#if ENGINE_VERSION > 2225
	if ( StackDepth <= 0 )
	{
		Parent->DumpStack();
		appErrorf(NAME_FriendlyError,TEXT("Received call to UpdateStack with CallStack depth of 0.  Verify that all packages have been compiled in debug mode."));
	}

#else
	if ( StackDepth == 0 )
		return 1;
#endif
	FStackNode* Last = &Stack.Last();
	if ( Last->StackNode != FStack )
	{
		if ( !Last->StackNode->Node->IsA(UState::StaticClass()) && FStack->Node->IsA(UState::StaticClass()) )
		{
			// We've received a call to execDebugInfo() from UObject::GotoState() as a result of the state change,
			// but we were executing a function, not state code.
			// Back up the state's pointer to the EX_DebugInfo, and ignore this update.
			FFrame* HijackStack = const_cast<FFrame*>(FStack);
			while ( --HijackStack->Code && *HijackStack->Code != EX_DebugInfo );
			return 1;
		}

		Parent->DumpStack();
		if ( appIsDebuggerPresent() )
			appDebugBreak();
		else
			appErrorf(NAME_FriendlyError,TEXT("Received call to UpdateStack with stack out of sync  Object:%s Class:%s Line:%i OpCode:%s"), Parent->GetStackOwnerClass(FStack)->GetName(), Debugee->GetName(), LineNumber, OpCode);
	}

	Last->Update( LineNumber, InputPos, OpCode, StackDepth );

	// Skip over OPEREFP & FORINIT opcodes to simplify stepping into/over
	return OpCode == DI_EFPOper || OpCode == DI_ForInit;

	unguard;
}

void UDebuggerCore::Break()
{
	guard(UDebuggerCore::Break);

#ifdef _DEBUG
	if ( GetCurrentNode() )
        GetCurrentNode()->Show();
#endif

	ChangeState( new DSWaitForInput(this), 1 );
	unguard;
}

void UDebuggerCore::DumpStack()
{
	guard(UDebuggerCore::DumpStack);

	check(CallStack);
	debugf(TEXT("CALLSTACK DUMP - SOMETHING BAD HAPPENED  STACKDEPTH: %i !"), CallStack->StackDepth);

	for ( INT i = 0; i < CallStack->Stack.Num(); i++ )
	{
		FStackNode* Node = &CallStack->Stack(i);
		if ( !Node )
			debugf(TEXT("%i)  INVALID NODE"), i);
		else
		{
			debugf(TEXT("%i) Class '%s'  Object '%s'  Node  '%s'"),
				i, 
				Node->Class ? Node->Class->GetName() : TEXT("NONE"),
				Node->Object ? Node->Object->GetFullName() : TEXT("NONE"),
				Node->StackNode && Node->StackNode->Node
				? Node->StackNode->Node->GetFullName() : TEXT("NONE") );

			for ( INT j = 0; j < Node->Lines.Num() && j < Node->OpCodes.Num(); j++ )
				debugf(TEXT("   %i): Line '%i'  OpCode '%s'  Depth  '%i'"), j, Node->Lines(j), GetOpCodeName(Node->OpCodes(j)), Node->Depths(j));
		}
	}


	unguard;
}
