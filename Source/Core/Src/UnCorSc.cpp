/*=============================================================================
	UnCorSc.cpp: UnrealScript execution and support code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Description:
	UnrealScript execution and support code.

Revision history:
	* Created by Tim Sweeney 

=============================================================================*/

#include "CorePrivate.h"
#include "OpCode.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

CORE_API void (UObject::*GNatives[EX_Max])( FFrame &Stack, RESULT_DECL );
CORE_API INT GNativeDuplicate=0;

CORE_API void (UObject::*GCasts[CST_Max])( FFrame &Stack, RESULT_DECL );
CORE_API INT GCastDuplicate=0;

#define RUNAWAY_LIMIT 10000000
#define RECURSE_LIMIT 250

#if DO_GUARD
	static INT Runaway=0;
	static INT Recurse=0;
	#define CHECK_RUNAWAY {if( ++Runaway > RUNAWAY_LIMIT ) {if(!ParseParam(appCmdLine(),TEXT("norunaway"))&&(!GDebugger||GDebugger->NotifyInfiniteLoop())) Stack.Logf( NAME_Critical, TEXT("Runaway loop detected (over %i iterations)"), RUNAWAY_LIMIT ); Runaway=0;}}
	CORE_API void GInitRunaway() {Recurse=Runaway=0;}
#else
	#define CHECK_RUNAWAY
	CORE_API void GInitRunaway() {}
#endif

#define IMPLEMENT_CAST_FUNCTION(cls,num,func) \
		IMPLEMENT_FUNCTION(cls,-1,func); \
		static BYTE cls##func##CastTemp = GRegisterCast( num, int##cls##func );
 
static double StopWatchTimer;

/*-----------------------------------------------------------------------------
	Call graph profiling.
-----------------------------------------------------------------------------*/

/**
 * Constructor
 *
 * @param InSoftMemoryLimit		Max number of bytes used by call graph data (soft limit).
 */
FScriptCallGraph::FScriptCallGraph( DWORD InSoftMemoryLimit )
{
	check( InSoftMemoryLimit );
	Reset( InSoftMemoryLimit );
}

/**
 * Resets data collection and memory use.
 *
 * @param InSoftMemoryLimit		Max number of bytes used by call graph data (soft limit).
 */
void FScriptCallGraph::Reset( DWORD InSoftMemoryLimit )
{
	if( InSoftMemoryLimit )
	{
		SoftMemoryLimit = InSoftMemoryLimit;
	}

	Data.Empty( SoftMemoryLimit * 11 / 10 / sizeof(PTRINT) );
}

/**
 * Serializes the profiling data to the passed in archive.	
 *
 * @param Ar	Archive to serialize data to.
 */
void FScriptCallGraph::Serialize( FArchive& Ar )
{
	if( Data.Num() )
	{
		INT StartIndex	= 0;
		INT EndIndex	= 0;

		// Skip the first frame as we most likely are in the middle of the call stack.
		while( StartIndex<Data.Num() && !IsFrameEndMarker(Data(StartIndex)) )
		{
			StartIndex++;
		}
		check(StartIndex<Data.Num());

		// Find the end of the call graph data.
		EndIndex = StartIndex;
		while( EndIndex<Data.Num() && !IsEndMarker(Data(EndIndex)) )
		{
			EndIndex++;
		}

		// Create a list of all functions currently loaded.
		TArray<UFunction*> Functions;
		for( TObjectIterator<UFunction> It; It; ++It )
		{
			Functions.AddItem( *It );
		}

		// First value to serialize is the number of usecs per cycle. (DOUBLE)
		DOUBLE usecsPerCycle = GSecondsPerCycle * 1000.0 * 1000.0;
		Ar << usecsPerCycle;

		// Second value is the number of functions that are being serialized (DWORD).
		DWORD FunctionCount = Functions.Num();
		Ar << FunctionCount;

		for( INT FunctionIndex=0; FunctionIndex<Functions.Num(); FunctionIndex++ )
		{
			TCHAR*			Name		= (TCHAR*) Functions(FunctionIndex)->GetPathName();
			PTRINT			Length		= appStrlen(Name);
			PTRINT			Pointer		= (PTRINT) Functions(FunctionIndex);
			const ANSICHAR*	AnsiName	= appToAnsi( Name, NULL, 1024 );

			// Serialize function pointer, length of name in characters and then serialize name without the
			// trailing '\0' as a chain of ANSI characters.
			Ar << Pointer;	// (PTRINT)
			Ar << Length;	// (PTRINT)
			for( DWORD i=0; i<Length; i++ )
			{
				ANSICHAR ACh = AnsiName[i];
				Ar << ACh;	// (ANSICHAR)
			}
		}

		// Serialize raw collected data.
		Ar.Serialize( &Data(StartIndex), (EndIndex - StartIndex) * sizeof(PTRINT) );
	}
}

/**
 * Emits the end of frame marker. Needs to be called when there are no script functions
 * on the stack. Also responsible for reseting call graph data if memory limit is reached.
 */
void FScriptCallGraph::Tick()
{
	EmitFrameEnd();
	if( (Data.Num()*sizeof(DWORD)) > SoftMemoryLimit )
	{
		Reset( SoftMemoryLimit );
	}
}

/** Global script call graph profiler */
FScriptCallGraph* GScriptCallGraph = NULL;

/*-----------------------------------------------------------------------------
	FFrame implementation.
-----------------------------------------------------------------------------*/

//
// Error or warning handler.
//
void FFrame::Serialize( const TCHAR* V, EName Event )
{
	guard(FFrame::Serialize);
	if( Event==NAME_Critical || GIsStrict ) appErrorf
	(
		TEXT("%s (%s:%04X) %s"),
		Object->GetFullName(),
		Node->GetFullName(),
		Code - &Node->Script(0),
		V
	);
	else debugf
	(
        Event, // gam
		TEXT("%s (%s:%04X) %s"),
		Object->GetFullName(),
		Node->GetFullName(),
		Code - &Node->Script(0),
		V
	);
	unguard;
}

/*-----------------------------------------------------------------------------
	Global script execution functions.
-----------------------------------------------------------------------------*/

//
// Have an object go to a named state, and idle at no label.
// If state is NAME_None or was not found, goes to no state.
// Returns 1 if we went to a state, 0 if went to no state.
//
EGotoState UObject::GotoState( FName NewState )
{
	guard(UObject::GotoState);
	if( !StateFrame )
		return GOTOSTATE_NotFound;

	StateFrame->LatentAction = 0;
	UState* StateNode = NULL;
	FName OldStateName = StateFrame->StateNode!=Class ? StateFrame->StateNode->GetFName() : FName(NAME_None);
	if( NewState != NAME_Auto )
	{
		// Find regular state.
		StateNode = FindState( NewState );
	}
	else
	{
		// Find auto state.
        for( TFieldFlagIterator<UState,CLASS_IsAUState> It(GetClass()); It && !StateNode; ++It )
			if( It->StateFlags & STATE_Auto )
				StateNode = *It;
	}

	if( !StateNode )
	{
		// Going nowhere.
		NewState  = NAME_None;
		StateNode = GetClass();
	}
	else if( NewState == NAME_Auto )
	{
		// Going to auto state.
		NewState = StateNode->GetFName();
	}

	// Send EndState notification.
	if
	(	OldStateName!=NAME_None
	&&	NewState!=OldStateName
	&&	!(GetFlags() & RF_InEndState) )
	{
		if ( IsProbing(NAME_EndState) )
		{
			ClearFlags( RF_StateChanged );
			SetFlags( RF_InEndState );
			eventEndState();
			ClearFlags( RF_InEndState );
			if( GetFlags() & RF_StateChanged )
				return GOTOSTATE_Preempted;
		}

		// UDebugger - rjp
		// When changing states from within state code, before the StateFrame's code pointer is changed,
		// the matching PREVSTACK for NEWSTACK that entered this state must be processed by the UDebugger
		// in order to clear this state's stacknode from the UDebugger's callstack
		if ( StateFrame->Node == StateFrame->StateNode &&
			 StateFrame->Code && *StateFrame->Code == EX_DebugInfo )
			StateFrame->Step(this,NULL);
	}

	// Go there.
	StateFrame->Node	   = StateNode;
	StateFrame->StateNode  = StateNode;
	StateFrame->Code	   = NULL;
	StateFrame->ProbeMask  = (StateNode->ProbeMask | GetClass()->ProbeMask) & StateNode->IgnoreMask;

	/* SCRIPTTIME
	unclock(GScriptCycles);
	debugf(TEXT("%s goto state %s at %f"),GetName(),*NewState, GScriptCycles * GSecondsPerCycle * 1000.f);
	clock(GScriptCycles);
	*/

	// Send BeginState notification.
	if( NewState!=NAME_None && NewState!=OldStateName && IsProbing(NAME_BeginState) )
	{
		ClearFlags( RF_StateChanged );
		eventBeginState();
		if( GetFlags() & RF_StateChanged )
			return GOTOSTATE_Preempted;
	}

	// Return result.
	if( NewState != NAME_None )
	{
		SetFlags( RF_StateChanged );
		return GOTOSTATE_Success;
	}
	else return GOTOSTATE_NotFound;

	unguard;
}

//
// Goto a label in the current state.
// Returns 1 if went, 0 if not found.
//
UBOOL UObject::GotoLabel( FName FindLabel )
{
	guard(UObject::GotoLabel);
	if( StateFrame )
	{
		StateFrame->LatentAction = 0;
		if( FindLabel != NAME_None )
		{
			for( UState* SourceState=StateFrame->StateNode; SourceState; SourceState=SourceState->GetSuperState() )
			{
				if( SourceState->LabelTableOffset != MAXWORD )
				{
					for( FLabelEntry* Label = (FLabelEntry *)&SourceState->Script(SourceState->LabelTableOffset); Label->Name!=NAME_None; Label++ )
					{
						if( Label->Name==FindLabel )
						{
							StateFrame->Node = SourceState;
							StateFrame->Code = &SourceState->Script(Label->iCode);
							return 1;
						}
					}
				}
			}
		}
		StateFrame->Code = NULL;
	}
	return 0;
	unguard;
}


/*-----------------------------------------------------------------------------
	Natives.
-----------------------------------------------------------------------------*/

//////////////////////////////
// Undefined native handler //
//////////////////////////////

void UObject::ExecutingBadStateCode(FFrame& Stack) 
{
	guard(UObject::ExecutingBadStateCode);

	Stack.Logf( NAME_Critical, TEXT("Unknown code token %02X"), Stack.Code[-1] );
	unguard;
}

void UObject::execUndefined( FFrame& Stack, RESULT_DECL  )
{
	guardSlow(UObject::execUndefined);

	ExecutingBadStateCode(Stack);
	unguardexecSlow;
}

///////////////
// Variables //
///////////////

void UObject::execLocalVariable( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLocalVariable);

	checkSlow(Stack.Object==this);
	checkSlow(Stack.Locals!=NULL);
	GProperty = (UProperty*)Stack.ReadObject();
	GPropAddr = Stack.Locals + GProperty->Offset;
	GPropObject = NULL;

    if( Result )
        GProperty->CopyCompleteValue( Result, GPropAddr );

    unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_LocalVariable, execLocalVariable );

void UObject::execInstanceVariable( FFrame& Stack, RESULT_DECL)
{
	guardSlow(UObject::execInstanceVariable);

	GProperty = (UProperty*)Stack.ReadObject();
	GPropAddr = (BYTE*)this + GProperty->Offset;
	GPropObject = this;

    if( Result )
		GProperty->CopyCompleteValue( Result, GPropAddr );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_InstanceVariable, execInstanceVariable );

void UObject::execDefaultVariable( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDefaultVariable);

	GProperty = (UProperty*)Stack.ReadObject();
	GPropAddr = &GetClass()->Defaults(GProperty->Offset);
	GPropObject = NULL;
	if( Result )
		GProperty->CopyCompleteValue( Result, GPropAddr );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_DefaultVariable, execDefaultVariable );

void UObject::execClassContext( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execClassContext);

	// Get class expression.
	UClass* ClassContext=NULL;
	Stack.Step( Stack.Object, &ClassContext );

	// Execute expression in class context.
	if( ClassContext )
	{
		Stack.Code += 3;
		Stack.Step( ClassContext->GetDefaultObject(), Result );
	}
	else
	{
		if ( GProperty )
			Stack.Logf( NAME_Error, TEXT("Accessed null class context '%s'"), GProperty->GetName() );	// rjp
		else Stack.Logf( NAME_Error, TEXT("Accessed null class context") ); // gam

		if ( GDebugger )
			GDebugger->NotifyAccessedNone();

		INT wSkip = Stack.ReadWord();
		BYTE bSize = *Stack.Code++;
		Stack.Code += wSkip;
		GPropAddr = NULL;
		GPropObject = NULL;
		GProperty = NULL;
		if( Result )
			appMemzero( Result, bSize );
	}
	unguardexec;
}
IMPLEMENT_FUNCTION( UObject, EX_ClassContext, execClassContext );

void UObject::execArrayElement( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execArrayElement);

	// Get array index expression.
	INT Index=0;
	Stack.Step( Stack.Object, &Index );

	// Get base element (must be a variable!!).
	GProperty = NULL;
	Stack.Step( this, NULL );

	// Add scaled offset to base pointer.
	if( GProperty && GPropAddr )
	{
		// Bounds check.
		if( Index>=GProperty->ArrayDim || Index<0 )
		{
			// Display out-of-bounds warning and continue on with index clamped to valid range.
			Stack.Logf( NAME_Error, TEXT("Accessed array '%s' out of bounds (%i/%i)"), GProperty->GetName(), Index, GProperty->ArrayDim ); // gam, rjp
			Index = Clamp( Index, 0, GProperty->ArrayDim - 1 );
		}

		// Update address.
		GPropAddr += Index * GProperty->ElementSize;
		guardSlow(ArrayElementCopySingleValue);
		if( Result )//!!
			GProperty->CopySingleValue( Result, GPropAddr );
		unguardSlow;
	}

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_ArrayElement, execArrayElement );

void UObject::execDebugInfo( FFrame& Stack, RESULT_DECL ) //DEBUGGER
{
	guardSlow(UObject::execDebugInfo);
	INT GVER		= Stack.ReadInt();
	
	// Correct version?
	if ( GVER != 100 )
	{
		Stack.Code -= sizeof(INT);
		Stack.Code--;
		return;
	}
	
	INT LineNumber	= Stack.ReadInt();
	INT InputPos	= Stack.ReadInt();
	BYTE OpCode		= *Stack.Code++;

/*
	FString InfoType = appFromAnsi((ANSICHAR*)Stack.Code);

	while( *Stack.Code )
		Stack.Code++;
	Stack.Code++;
*/
	// Only valid when the debugger is running.
	if ( GDebugger != NULL )
		GDebugger->DebugInfo( this, &Stack, OpCode, LineNumber, InputPos );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_DebugInfo, execDebugInfo );

void UObject::execDynArrayElement( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDynArrayElement);

	// Get array index expression.
	INT Index=0;
	Stack.Step( Stack.Object, &Index );

	GProperty = NULL;
	Stack.Step( this, NULL );
	GPropObject = this;

#if DO_CHECK_SLOW
	if( !GProperty )
		Stack.Logf( NAME_Error, TEXT("Accessed dynamic array through None") );
	else
	if( !GProperty->IsA(UArrayProperty::StaticClass()))
		Stack.Logf( NAME_Error, TEXT("Accessed dynamic array through non-ArrayProperty") );
#endif

	// Add scaled offset to base pointer.
	if( GProperty && GPropAddr )
	{
		FArray* Array=(FArray*)GPropAddr;
		UArrayProperty* ArrayProp = (UArrayProperty*)GProperty;
		if( Index>=Array->Num() || Index<0 )
		{
			//if we are returning a value, check for out-of-bounds
			if ( Result || Index<0 )
			{
				Stack.Logf( NAME_Error, TEXT("Accessed array '%s' out of bounds (%i/%i)"), ArrayProp->GetName(), Index, Array->Num() );
				GPropAddr = 0;
				GPropObject = NULL;
				if (Result) appMemzero( Result, ArrayProp->Inner->ElementSize );
				return;
			}
			//if we are setting a value, allow the array to be resized
			else
			{
				Array->AddZeroed(ArrayProp->Inner->ElementSize,Index-Array->Num()+1);
			}
		}

		GPropAddr = (BYTE*)Array->GetData() + Index * ArrayProp->Inner->ElementSize;

		// Add scaled offset to base pointer.
		if( Result )
		{
			ArrayProp->Inner->CopySingleValue( Result, GPropAddr );
		}
	}

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_DynArrayElement, execDynArrayElement );

void UObject::execDynArrayLength( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDynArrayLength);

	GProperty = NULL;
	Stack.Step( this, NULL );
	GPropObject = this;

	if (GPropAddr)
	{
		FArray* Array=(FArray*)GPropAddr;
		if ( !Result )
			GRuntimeUCFlags |= RUC_ArrayLengthSet; //so that EX_Let knows that this is a length 'set'-ting
		else
			*(INT*)Result = Array->Num();
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_DynArrayLength, execDynArrayLength );

void UObject::execDynArrayInsert( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDynArrayInsert);

	GPropObject = this;
	GProperty = NULL;
	Stack.Step( this, NULL );
	UArrayProperty* ArrayProperty = Cast<UArrayProperty>(GProperty);
	FArray* Array=(FArray*)GPropAddr;

	P_GET_INT(Index);
	P_GET_INT(Count);
	if (Array && Count)
	{
		if ( Count < 0 )
		{
			Stack.Logf( TEXT("Attempt to insert a negative number of elements '%s'"), ArrayProperty->GetName() );
			return;
		}
		if ( Index < 0 || Index > Array->Num() )
		{
			Stack.Logf( TEXT("Attempt to insert %i elements at %i an %i-element array '%s'"), Count, Index, Array->Num(), ArrayProperty->GetName() );
			Index = Clamp(Index, 0,Array->Num());
		}
		Array->InsertZeroed( Index, Count, ArrayProperty->Inner->ElementSize);
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_DynArrayInsert, execDynArrayInsert );

void UObject::execDynArrayRemove( FFrame& Stack, RESULT_DECL )
{	
	guardSlow(UObject::execDynArrayRemove);

	GProperty = NULL;
	GPropObject = this;
	Stack.Step( this, NULL );
	UArrayProperty* ArrayProperty = Cast<UArrayProperty>(GProperty);
	FArray* Array=(FArray*)GPropAddr;

	P_GET_INT(Index);
	P_GET_INT(Count);
	if (Array && Count)
	{
		if ( Count < 0 )
		{
			Stack.Logf( TEXT("Attempt to remove a negative number of elements '%s'"), ArrayProperty->GetName() );
			return;
		}
		if ( Index < 0 || Index >= Array->Num() || Index + Count > Array->Num() )
		{
			if (Count == 1)
				Stack.Logf( TEXT("Attempt to remove element %i in an %i-element array '%s'"), Index, Array->Num(), ArrayProperty->GetName() );
			else
				Stack.Logf( TEXT("Attempt to remove elements %i through %i in an %i-element array '%s'"), Index, Index+Count-1, Array->Num(), ArrayProperty->GetName() );
			Index = Clamp(Index, 0,Array->Num());
			if ( Index + Count > Array->Num() )
				Count = Array->Num() - Index;
		}

		for (INT i=Index+Count-1; i>=Index; i--)
			ArrayProperty->Inner->DestroyValue((BYTE*)Array->GetData() + ArrayProperty->Inner->ElementSize*i);
		Array->Remove( Index, Count, ArrayProperty->Inner->ElementSize);
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_DynArrayRemove, execDynArrayRemove );

void UObject::execBoolVariable( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execBoolVariable);

	// Get bool variable.
	BYTE B = *Stack.Code++;

	UBoolProperty* Property;
#if SERIAL_POINTER_INDEX
	Property = (UBoolProperty*) UObject::SerialIndexPointer( *(INT*)Stack.Code );
#else
  #if __PSX2_EE__ || __GCN__
	appMemcpy(&Property, Stack.Code, sizeof(INT));
  #else
	Property = *(UBoolProperty**)Stack.Code;
  #endif
#endif

	(this->*GNatives[B])( Stack, NULL );
	GProperty = Property;
	GPropObject = this;

	// Note that we're not returning an in-place pointer to to the bool, so EX_Let 
	// must take special precautions with bools.
	if( Result )
		*(BITFIELD*)Result = (GPropAddr && (*(BITFIELD*)GPropAddr & ((UBoolProperty*)GProperty)->BitMask)) ? 1 : 0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_BoolVariable, execBoolVariable );

#define BREAK_MENUS 1// sjs
#if BREAK_MENUS
void UObject::execStructMember( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStructMember);

	// Get structure element.
	UProperty* Property = (UProperty*)Stack.ReadObject();

	// Get struct expression.
	UStruct* Struct = CastChecked<UStruct>(Property->GetOuter());
	BYTE* Buffer = (BYTE*)appAlloca(Struct->PropertiesSize);
	appMemzero( Buffer, Struct->PropertiesSize );
	GPropAddr = NULL;
	Stack.Step( this, Buffer );

	// Set result.
	GProperty = Property;
	GPropObject = this;
	if( GPropAddr )
		GPropAddr += Property->Offset;
	guardSlow(StructMemberCopyComplete);
	if( Result )
		Property->CopyCompleteValue( Result, Buffer+Property->Offset );
	unguardSlow;
	guardSlow(StructMemberDestroyValue);
	for( UProperty* P=Struct->ConstructorLink; P; P=P->ConstructorLinkNext )
		P->DestroyValue( Buffer + P->Offset );
	unguardSlow;

	unguardexecSlow;
}
#else
// gam ---
void UObject::execStructMember( FFrame& Stack, RESULT_DECL )
{
    guardSlow(UObject::execStructMember);

    // Get structure element
	UProperty* Property = (UProperty*)Stack.ReadObject();

    // Set GProperty & GPropAddr to address containing struct
	GPropAddr = NULL;
    Stack.Step( this, NULL );

    GProperty = Property;
    GPropObject = this;
    // Set result if required
    if( Result && GPropAddr )
    {
        checkSlow( GProperty );
        checkSlow( GPropAddr );
        Property->CopyCompleteValue( Result, GPropAddr + Property->Offset );
    }

    // advance property pointer
	if( GPropAddr )
		GPropAddr += Property->Offset;

    unguardexecSlow;
}
// --- gam
#endif
IMPLEMENT_FUNCTION( UObject, EX_StructMember, execStructMember );

/////////////
// Nothing //
/////////////

void UObject::execNothing( FFrame& Stack, RESULT_DECL )
{
	// Do nothing.
}
IMPLEMENT_FUNCTION( UObject, EX_Nothing, execNothing );

void UObject::execEndOfScript( FFrame& Stack, RESULT_DECL )
{
	appErrorf(TEXT("Execution beyond end of script!"));
}
IMPLEMENT_FUNCTION( UObject, EX_EndOfScript, execEndOfScript );

void UObject::execNativeParm( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNativeParm);

	UProperty* Property = (UProperty*)Stack.ReadObject();
	if( Result )
	{
		GPropAddr = Stack.Locals + Property->Offset;
		GPropObject = NULL;  
		Property->CopyCompleteValue( Result, Stack.Locals + Property->Offset );
	}

	unguardSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_NativeParm, execNativeParm );

void UObject::execEndFunctionParms( FFrame& Stack, RESULT_DECL )
{
	// For skipping over optional function parms without values specified.
	GPropObject = NULL;  
	Stack.Code--;
}
IMPLEMENT_FUNCTION( UObject, EX_EndFunctionParms, execEndFunctionParms );

//////////////
// Commands //
//////////////

void UObject::execStop( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStop);
	Stack.Code = NULL;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_Stop, execStop );

//!!warning: Does not support UProperty's fully, will break
// when TArray's are supported in UnrealScript!
void UObject::execSwitch( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSwitch);

	// Get switch size.
	BYTE bSize = *Stack.Code++;

	// Get switch expression.
	BYTE SwitchBuffer[1024], Buffer[1024];
	appMemzero( Buffer,       sizeof(FString) );
	appMemzero( SwitchBuffer, sizeof(FString) );
	Stack.Step( Stack.Object, SwitchBuffer );

	// Check each case clause till we find a match.
	for( ; ; )
	{
		// Skip over case token.
		checkSlow(*Stack.Code==EX_Case);
		Stack.Code++;

		// Get address of next handler.
		INT wNext = Stack.ReadWord();
		if( wNext == MAXWORD ) // Default case or end of cases.
			break;

		// Get case expression.
		Stack.Step( Stack.Object, Buffer );

		// Compare.
		if( bSize ? (appMemcmp(SwitchBuffer,Buffer,bSize)==0) : (*(FString*)SwitchBuffer==*(FString*)Buffer) )
			break;

		// Jump to next handler.
		Stack.Code = &Stack.Node->Script(wNext);
	}
	if( !bSize )
	{
		(*(FString*)SwitchBuffer).~FString();
		(*(FString*)Buffer      ).~FString();
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_Switch, execSwitch );

void UObject::execCase( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execCase);
	INT wNext = Stack.ReadWord();
	if( wNext != MAXWORD )
	{
		// Skip expression.
		BYTE Buffer[1024];
		appMemzero( Buffer, sizeof(FString) );
		Stack.Step( Stack.Object, Buffer );
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_Case, execCase );

void UObject::execJump( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execJump);
	CHECK_RUNAWAY;

	// Jump immediate.
	INT Offset = Stack.ReadWord();
	Stack.Code = &Stack.Node->Script(Offset);
//	Stack.Code = &Stack.Node->Script(Stack.ReadWord() );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_Jump, execJump );

void UObject::execJumpIfNot( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execJumpIfNot);
	CHECK_RUNAWAY;

	// Get code offset.
	INT wOffset = Stack.ReadWord();

	// Get boolean test value.
	UBOOL Value=0;
	Stack.Step( Stack.Object, &Value );

	// Jump if false.
	if( !Value )
		Stack.Code = &Stack.Node->Script( wOffset );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_JumpIfNot, execJumpIfNot );

void UObject::execAssert( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAssert);

	// Get line number.
	INT wLine = Stack.ReadWord();

	// Get boolean assert value.
	DWORD Value=0;
	Stack.Step( Stack.Object, &Value );

	// Check it.
	if( !Value )
	{
		if ( !GDebugger || !GDebugger->NotifyAssertionFailed( wLine ) )
			Stack.Logf( NAME_Critical, TEXT("Assertion failed, line %i"), wLine );
	}

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_Assert, execAssert );

void UObject::execGotoLabel( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGotoLabel);

	P_GET_NAME(N);
	if( !GotoLabel( N ) )
        Stack.Logf( NAME_Error, TEXT("GotoLabel (%s): Label not found"), *N ); // gam

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_GotoLabel, execGotoLabel );

////////////////
// Assignment //
////////////////

void UObject::execLet( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLet);
	checkSlow(!IsA(UBoolProperty::StaticClass()));

	// Get variable address.
	GPropAddr = NULL;
	Stack.Step( Stack.Object, NULL ); // Evaluate variable.
	if( !GPropAddr )
	{
		if (GProperty)
			Stack.Logf( NAME_Warning, TEXT("Attempt to assign variable through None '%s'"), GProperty->GetName() ); // rjp
		Stack.Logf( NAME_Warning, TEXT("Attempt to assign variable through None")); // gam
		static BYTE Crud[1024];//!!temp
		GPropAddr = Crud;
		appMemzero( GPropAddr, sizeof(FString) );
	}
	else if ( GPropObject && GProperty && (GProperty->PropertyFlags & CPF_Net) )
		GPropObject->NetDirty(GProperty); //FIXME - use object property instead for performance

	if (GRuntimeUCFlags & RUC_ArrayLengthSet)
	{
		GRuntimeUCFlags &= ~RUC_ArrayLengthSet;
		FArray* Array=(FArray*)GPropAddr;
		UArrayProperty* ArrayProp = (UArrayProperty*)GProperty;
		INT NewSize = 0;
		Stack.Step( Stack.Object, &NewSize); // Evaluate expression into variable.
		if (NewSize > Array->Num())
		{
			Array->AddZeroed(ArrayProp->Inner->ElementSize, NewSize-Array->Num());
		}
		else if (NewSize < Array->Num())
		{
			for (INT i=Array->Num()-1; i>=NewSize; i--)
				ArrayProp->Inner->DestroyValue((BYTE*)Array->GetData() + ArrayProp->Inner->ElementSize*i);
			Array->Remove(NewSize, Array->Num()-NewSize, ArrayProp->Inner->ElementSize );
		}
	} else
		Stack.Step( Stack.Object, GPropAddr ); // Evaluate expression into variable.
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_Let, execLet );

void UObject::execLetBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLetBool);

	// Get variable address.
	GPropAddr = NULL;
	GProperty = NULL;
	GPropObject = NULL;
	Stack.Step( Stack.Object, NULL ); // Variable.
	BITFIELD*      BoolAddr     = (BITFIELD*)GPropAddr;
	UBoolProperty* BoolProperty = (UBoolProperty*)GProperty;
	INT Value=0;  // must be INT...execTrue/execFalse return 32 bits. --ryan.
	if ( GPropObject && GProperty && (GProperty->PropertyFlags & CPF_Net) )
		GPropObject->NetDirty(GProperty);
	Stack.Step( Stack.Object, &Value );
	if( BoolAddr )
	{
		checkSlow(BoolProperty->IsA(UBoolProperty::StaticClass()));
		if( Value ) *BoolAddr |=  BoolProperty->BitMask;
		else        *BoolAddr &= ~BoolProperty->BitMask;
	}

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_LetBool, execLetBool );

/////////////////////////
// Context expressions //
/////////////////////////

void UObject::execSelf( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSelf);

	// Get Self actor for this context.
	*(UObject**)Result = this;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_Self, execSelf );

void UObject::execContext( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execContext);

	// Get actor variable.
	UObject* NewContext=NULL;
	Stack.Step( this, &NewContext );

	// Execute or skip the following expression in the actor's context.
	if( NewContext != NULL && !NewContext->IsPendingKill()) // amb: return None for actors that are about to be deleted
	{
		Stack.Code += 3;
		Stack.Step( NewContext, Result );
	}
	else
	{
		if (GProperty)
			Stack.Logf( NAME_Warning, TEXT("Accessed None '%s'"), GProperty->GetName() ); // rjp
		else
			Stack.Logf( NAME_Warning, TEXT("Accessed None")); // gam

		// DEBUGGER

		if (GDebugger)
			GDebugger->NotifyAccessedNone();	// jmw

		INT wSkip = Stack.ReadWord();
		BYTE bSize = *Stack.Code++;
		Stack.Code += wSkip;
		GPropAddr = NULL;
		GProperty = NULL;
		GPropObject = NULL;
		if( Result )
			appMemzero( Result, bSize );
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_Context, execContext );

////////////////////
// Function calls //
////////////////////

void UObject::execVirtualFunction( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execVirtualFunction);

	// Call the virtual function.
	CallFunction( Stack, Result, FindFunctionChecked(Stack.ReadName()) );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_VirtualFunction, execVirtualFunction );

void UObject::execFinalFunction( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFinalFunction);

	// Call the final function.
	CallFunction( Stack, Result, (UFunction*)Stack.ReadObject() );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_FinalFunction, execFinalFunction );

void UObject::execGlobalFunction( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGlobalFunction);

	// Call global version of virtual function.
	CallFunction( Stack, Result, FindFunctionChecked(Stack.ReadName(),1) );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_GlobalFunction, execGlobalFunction );

///////////////////////
// Delegates         //
///////////////////////

void UObject::execDelegateFunction( FFrame& Stack, RESULT_DECL )
{
	//guardSlow(UObject::execDelegateFunction);
	guard(UObject::execDelegateFunction);

	// Look up delegate property
	UProperty* DelegateProperty = (UProperty*)Stack.ReadObject();
	FScriptDelegate* Delegate = (FScriptDelegate*)((BYTE*)this + DelegateProperty->Offset);
	FName DelegateName = Stack.ReadName();
	if( Delegate->Object && Delegate->Object->IsPendingKill() )
	{
		Delegate->Object = NULL;
		Delegate->FunctionName = NAME_None;
	}
	if( Delegate->Object )
		Delegate->Object->CallFunction( Stack, Result, Delegate->Object->FindFunctionChecked(Delegate->FunctionName) );	
	else
		CallFunction( Stack, Result, FindFunctionChecked(DelegateName) );
	unguardexec;
	//unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_DelegateFunction, execDelegateFunction );

void UObject::execDelegateProperty( FFrame& Stack, RESULT_DECL )
{
//	guardSlow(UObject::execDelegateProperty);
	guard(UObject::execDelegateProperty);
	FName FunctionName = Stack.ReadName();
	((FScriptDelegate*)Result)->FunctionName = FunctionName;
	if( FunctionName == NAME_None )
		((FScriptDelegate*)Result)->Object = NULL;
	else
		((FScriptDelegate*)Result)->Object = this;
	unguardexec;
//	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_DelegateProperty, execDelegateProperty );

void UObject::execLetDelegate( FFrame& Stack, RESULT_DECL )
{
//	guardSlow(UObject::execLetDelegate);
	guard(UObject::execLetDelegate);

	// Get variable address.
	GPropAddr = NULL;
	GProperty = NULL;
	GPropObject = NULL;
	Stack.Step( Stack.Object, NULL ); // Variable.
	FScriptDelegate* DelegateAddr = (FScriptDelegate*)GPropAddr;
	FScriptDelegate Delegate;
	Stack.Step( Stack.Object, &Delegate );
	if( DelegateAddr )
	{
		DelegateAddr->FunctionName = Delegate.FunctionName;
		DelegateAddr->Object	   = Delegate.Object;
	}
	unguardexec;
	//unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_LetDelegate, execLetDelegate );


///////////////////////
// Struct comparison //
///////////////////////

void UObject::execStructCmpEq( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStructCmpEq);

	UStruct* Struct  = (UStruct*)Stack.ReadObject();
	BYTE*    Buffer1 = (BYTE*)appAlloca(Struct->PropertiesSize);
	BYTE*    Buffer2 = (BYTE*)appAlloca(Struct->PropertiesSize);
	appMemzero( Buffer1, Struct->PropertiesSize );
	appMemzero( Buffer2, Struct->PropertiesSize );
	Stack.Step( this, Buffer1 );
	Stack.Step( this, Buffer2 );
	*(DWORD*)Result  = Struct->StructCompare( Buffer1, Buffer2 );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_StructCmpEq, execStructCmpEq );

void UObject::execStructCmpNe( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStructCmpNe);

	UStruct* Struct = (UStruct*)Stack.ReadObject();
	BYTE*    Buffer1 = (BYTE*)appAlloca(Struct->PropertiesSize);
	BYTE*    Buffer2 = (BYTE*)appAlloca(Struct->PropertiesSize);
	appMemzero( Buffer1, Struct->PropertiesSize );
	appMemzero( Buffer2, Struct->PropertiesSize );
	Stack.Step( this, Buffer1 );
	Stack.Step( this, Buffer2 );
	*(DWORD*)Result = !Struct->StructCompare(Buffer1,Buffer2);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_StructCmpNe, execStructCmpNe );

///////////////
// Constants //
///////////////

void UObject::execIntConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIntConst);
	*(INT*)Result = Stack.ReadInt();
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_IntConst, execIntConst );

void UObject::execFloatConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFloatConst);
	*(FLOAT*)Result = Stack.ReadFloat();
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_FloatConst, execFloatConst );

void UObject::execStringConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStringConst);
	*(FString*)Result = appFromAnsi((ANSICHAR*)Stack.Code);
	while( *Stack.Code )
		Stack.Code++;
	Stack.Code++;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_StringConst, execStringConst );

void UObject::execUnicodeStringConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStringConst);
	*(FString*)Result = appFromUnicode((UNICHAR*)Stack.Code);
	while( *(_WORD*)Stack.Code )
		Stack.Code+=sizeof(_WORD);
	Stack.Code+=sizeof(_WORD);
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_UnicodeStringConst, execUnicodeStringConst );

void UObject::execObjectConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execObjectConst);
	*(UObject**)Result = (UObject*)Stack.ReadObject();
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_ObjectConst, execObjectConst );

void UObject::execNameConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNameConst);
	*(FName*)Result = Stack.ReadName();
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_NameConst, execNameConst );

void UObject::execByteConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execByteConst);
	*(BYTE*)Result = *Stack.Code++;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_ByteConst, execByteConst );

void UObject::execIntZero( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIntZero);
	*(INT*)Result = 0;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_IntZero, execIntZero );

void UObject::execIntOne( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIntOne);
	*(INT*)Result = 1;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_IntOne, execIntOne );

void UObject::execTrue( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execTrue);
	*(INT*)Result = 1;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_True, execTrue );

void UObject::execFalse( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFalse);
	*(DWORD*)Result = 0;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_False, execFalse );

void UObject::execNoObject( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNoObject);
	*(UObject**)Result = NULL;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_NoObject, execNoObject );

void UObject::execIntConstByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIntConstByte);
	*(INT*)Result = *Stack.Code++;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_IntConstByte, execIntConstByte );

/////////////////
// Conversions //
/////////////////

void UObject::execDynamicCast( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDynamicCast);

	// Get destination class of dynamic actor class.
	UClass* Class = (UClass *)Stack.ReadObject();

	// Compile object expression.
	UObject* Castee = NULL;
	Stack.Step( Stack.Object, &Castee );
	*(UObject**)Result = (Castee && Castee->IsA(Class)) ? Castee : NULL;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_DynamicCast, execDynamicCast );

void UObject::execMetaCast( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMetaCast);

	// Get destination class of dynamic actor class.
	UClass* MetaClass = (UClass*)Stack.ReadObject();

	// Compile actor expression.
	UObject* Castee=NULL;
	Stack.Step( Stack.Object, &Castee );
	*(UObject**)Result = (Castee && Castee->IsA(UClass::StaticClass()) && ((UClass*)Castee)->IsChildOf(MetaClass)) ? Castee : NULL;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_MetaCast, execMetaCast );

void UObject::execPrimitiveCast( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execPrimitiveCast);

	INT B = *(Stack.Code)++;
	(Stack.Object->*GCasts[B])( Stack, Result );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_PrimitiveCast, execPrimitiveCast );

void UObject::execByteToInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execByteToInt);
	BYTE B=0;
	Stack.Step( Stack.Object, &B );
	*(INT*)Result = B;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_ByteToInt, execByteToInt );

void UObject::execByteToBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execByteToBool);
	BYTE B=0;
	Stack.Step( Stack.Object, &B );
	*(DWORD*)Result = B ? 1 : 0;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_ByteToBool, execByteToBool );

void UObject::execByteToFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execByteToFloat);
	BYTE B=0;
	Stack.Step( Stack.Object, &B );
	*(FLOAT*)Result = B;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_ByteToFloat, execByteToFloat );

void UObject::execByteToString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execByteToString);
	P_GET_BYTE(B);
	*(FString*)Result = FString::Printf(TEXT("%i"),B);
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_ByteToString, execByteToString );

void UObject::execIntToByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIntToByte);
	INT I=0;
	Stack.Step( Stack.Object, &I );
	*(BYTE*)Result = I;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_IntToByte, execIntToByte );

void UObject::execIntToBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIntToBool);
	INT I=0;
	Stack.Step( Stack.Object, &I );
	*(INT*)Result = I ? 1 : 0;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_IntToBool, execIntToBool );

void UObject::execIntToFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIntToFloat);
	INT I=0;
	Stack.Step( Stack.Object, &I );
	*(FLOAT*)Result = I;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_IntToFloat, execIntToFloat );

void UObject::execIntToString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIntToString);
	P_GET_INT(I);
	*(FString*)Result = FString::Printf(TEXT("%i"),I);
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_IntToString, execIntToString );

void UObject::execBoolToByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execBoolToByte);
	UBOOL B=0;
	Stack.Step( Stack.Object, &B );
	*(BYTE*)Result = B & 1;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_BoolToByte, execBoolToByte );

void UObject::execBoolToInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execBoolToInt);
	UBOOL B=0;
	Stack.Step( Stack.Object, &B );
	*(INT*)Result = B & 1;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_BoolToInt, execBoolToInt );

void UObject::execBoolToFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execBoolToFloat);
	UBOOL B=0;
	Stack.Step( Stack.Object, &B );
	*(FLOAT*)Result = B & 1;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_BoolToFloat, execBoolToFloat );

void UObject::execBoolToString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execBoolToString);
	P_GET_UBOOL(B);
	*(FString*)Result = B ? GTrue : GFalse;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_BoolToString, execBoolToString );

void UObject::execFloatToByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFloatToByte);
	FLOAT F=0.f;
	Stack.Step( Stack.Object, &F );
	*(BYTE*)Result = (BYTE)F;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_FloatToByte, execFloatToByte );

void UObject::execFloatToInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFloatToInt);
	FLOAT F=0.f;
	Stack.Step( Stack.Object, &F );
	*(INT*)Result = (INT)F;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_FloatToInt, execFloatToInt );

void UObject::execFloatToBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFloatToBool);
	FLOAT F=0.f;
	Stack.Step( Stack.Object, &F );
	*(DWORD*)Result = F!=0.f ? 1 : 0;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_FloatToBool, execFloatToBool );

void UObject::execFloatToString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFloatToString);
	P_GET_FLOAT(F);
	*(FString*)Result = FString::Printf(TEXT("%.2f"),F);
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_FloatToString, execFloatToString );

void UObject::execObjectToBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execObjectToBool);
	UObject* Obj=NULL;
	Stack.Step( Stack.Object, &Obj );
	*(DWORD*)Result = Obj!=NULL;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_ObjectToBool, execObjectToBool );

void UObject::execObjectToString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execObjectToString);
	P_GET_OBJECT(UObject,Obj);
	*(FString*)Result = Obj ? Obj->GetPathName() : TEXT("None");
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_ObjectToString, execObjectToString );

void UObject::execNameToBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNameToBool);
	FName N=NAME_None;
	Stack.Step( Stack.Object, &N );
	*(DWORD*)Result = N!=NAME_None ? 1 : 0;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_NameToBool, execNameToBool );

void UObject::execNameToString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNameToString);
	P_GET_NAME(N);
	*(FString*)Result = *N;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_NameToString, execNameToString );

void UObject::execStringToByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStringToByte);
	P_GET_STR(Str);
	*(BYTE*)Result = appAtoi( *Str );
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_StringToByte, execStringToByte );

void UObject::execStringToInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStringToInt);
	P_GET_STR(Str);
	*(INT*)Result = appAtoi( *Str );
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_StringToInt, execStringToInt );

void UObject::execStringToBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStringToBool);
	P_GET_STR(Str);
	if( appStricmp(*Str,TEXT("True") )==0 || appStricmp(*Str,GTrue)==0 )
	{
		*(INT*)Result = 1;
	}
	else if( appStricmp(*Str,TEXT("False"))==0 || appStricmp(*Str,GFalse)==0 )
	{
		*(INT*)Result = 0;
	}
	else
	{
		*(INT*)Result = appAtoi(*Str) ? 1 : 0;
	}
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_StringToBool, execStringToBool );

void UObject::execStringToFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStringToFloat);
	P_GET_STR(Str);
	*(FLOAT*)Result = appAtof( *Str );
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_StringToFloat, execStringToFloat );

/////////////////////////////////////////
// Native bool operators and functions //
/////////////////////////////////////////

void UObject::execNot_PreBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNot_PreBool);

	P_GET_UBOOL(A);
	P_FINISH;

	*(DWORD*)Result = !A;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 129, execNot_PreBool );

void UObject::execEqualEqual_BoolBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEqualEqual_BoolBool);

	P_GET_UBOOL(A);
	P_GET_UBOOL(B);
	P_FINISH;

	*(DWORD*)Result = ((!A) == (!B));
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 242, execEqualEqual_BoolBool );

void UObject::execNotEqual_BoolBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNotEqual_BoolBool);

	P_GET_UBOOL(A);
	P_GET_UBOOL(B);
	P_FINISH;

	*(DWORD*)Result = ((!A) != (!B));

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 243, execNotEqual_BoolBool );

void UObject::execAndAnd_BoolBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAndAnd_BoolBool);

	P_GET_UBOOL(A);
	P_GET_SKIP_OFFSET(W);

	if( A )
	{
		P_GET_UBOOL(B);
		*(DWORD*)Result = A && B;
		Stack.Code++; //DEBUGGER
	}
	else
	{
		*(DWORD*)Result = 0;
		Stack.Code += W;
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 130, execAndAnd_BoolBool );

void UObject::execXorXor_BoolBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execXorXor_BoolBool);

	P_GET_UBOOL(A);
	P_GET_UBOOL(B);
	P_FINISH;

	*(DWORD*)Result = !A ^ !B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 131, execXorXor_BoolBool );

void UObject::execOrOr_BoolBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execOrOr_BoolBool);
	P_GET_UBOOL(A);
	P_GET_SKIP_OFFSET(W);
	if( !A )
	{
		P_GET_UBOOL(B);
		*(DWORD*)Result = A || B;
		Stack.Code++; //DEBUGGER
	}
	else
	{
		*(DWORD*)Result = 1;
		Stack.Code += W;
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 132, execOrOr_BoolBool );

/////////////////////////////////////////
// Native byte operators and functions //
/////////////////////////////////////////

void UObject::execMultiplyEqual_ByteByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiplyEqual_ByteByte);

	P_GET_BYTE_REF(A);
	P_GET_BYTE(B);
	P_FINISH;

	*(BYTE*)Result = (*A *= B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 133, execMultiplyEqual_ByteByte );

void UObject::execDivideEqual_ByteByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivideEqual_ByteByte);

	P_GET_BYTE_REF(A);
	P_GET_BYTE(B);
	P_FINISH;

	*(BYTE*)Result = B ? (*A /= B) : 0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 134, execDivideEqual_ByteByte );

void UObject::execAddEqual_ByteByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddEqual_ByteByte);

	P_GET_BYTE_REF(A);
	P_GET_BYTE(B);
	P_FINISH;

	*(BYTE*)Result = (*A += B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 135, execAddEqual_ByteByte );

void UObject::execSubtractEqual_ByteByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractEqual_ByteByte);

	P_GET_BYTE_REF(A);
	P_GET_BYTE(B);
	P_FINISH;

	*(BYTE*)Result = (*A -= B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 136, execSubtractEqual_ByteByte );

void UObject::execAddAdd_PreByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddAdd_PreByte);

	P_GET_BYTE_REF(A);
	P_FINISH;

	*(BYTE*)Result = ++(*A);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 137, execAddAdd_PreByte );

void UObject::execSubtractSubtract_PreByte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractSubtract_PreByte);

	P_GET_BYTE_REF(A);
	P_FINISH;

	*(BYTE*)Result = --(*A);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 138, execSubtractSubtract_PreByte );

void UObject::execAddAdd_Byte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddAdd_Byte);

	P_GET_BYTE_REF(A);
	P_FINISH;

	*(BYTE*)Result = (*A)++;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 139, execAddAdd_Byte );

void UObject::execSubtractSubtract_Byte( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractSubtract_Byte);

	P_GET_BYTE_REF(A);
	P_FINISH;

	*(BYTE*)Result = (*A)--;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 140, execSubtractSubtract_Byte );

/////////////////////////////////
// Int operators and functions //
/////////////////////////////////

void UObject::execComplement_PreInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execComplement_PreInt);

	P_GET_INT(A);
	P_FINISH;

	*(INT*)Result = ~A;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 141, execComplement_PreInt );

void UObject::execGreaterGreaterGreater_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreaterGreaterGreater_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = ((DWORD)A) >> B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 196, execGreaterGreaterGreater_IntInt );

void UObject::execSubtract_PreInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtract_PreInt);

	P_GET_INT(A);
	P_FINISH;

	*(INT*)Result = -A;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 143, execSubtract_PreInt );

void UObject::execMultiply_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiply_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A * B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 144, execMultiply_IntInt );

void UObject::execDivide_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivide_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = B ? A / B : 0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 145, execDivide_IntInt );

void UObject::execAdd_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAdd_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A + B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 146, execAdd_IntInt );

void UObject::execSubtract_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtract_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A - B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 147, execSubtract_IntInt );

void UObject::execLessLess_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLessLess_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A << B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 148, execLessLess_IntInt );

void UObject::execGreaterGreater_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreaterGreater_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A >> B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 149, execGreaterGreater_IntInt );

void UObject::execLess_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLess_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A < B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 150, execLess_IntInt );

void UObject::execGreater_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreater_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A > B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 151, execGreater_IntInt );

void UObject::execLessEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLessEqual_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A <= B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 152, execLessEqual_IntInt );

void UObject::execGreaterEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreaterEqual_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A >= B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 153, execGreaterEqual_IntInt );

void UObject::execEqualEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEqualEqual_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A == B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 154, execEqualEqual_IntInt );

void UObject::execNotEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNotEqual_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(DWORD*)Result = A != B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 155, execNotEqual_IntInt );

void UObject::execAnd_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAnd_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A & B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 156, execAnd_IntInt );

void UObject::execXor_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execXor_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A ^ B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 157, execXor_IntInt );

void UObject::execOr_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execOr_IntInt);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = A | B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 158, execOr_IntInt );

void UObject::execMultiplyEqual_IntFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiplyEqual_IntInt);

	P_GET_INT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(INT*)Result = *A = (INT)(*A * B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 159, execMultiplyEqual_IntFloat );

void UObject::execDivideEqual_IntFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivideEqual_IntInt);

	P_GET_INT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(INT*)Result = *A = (INT)(B ? *A/B : 0.f);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 160, execDivideEqual_IntFloat );

void UObject::execAddEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddEqual_IntInt);

	P_GET_INT_REF(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = (*A += B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 161, execAddEqual_IntInt );

void UObject::execSubtractEqual_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractEqual_IntInt);

	P_GET_INT_REF(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = (*A -= B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 162, execSubtractEqual_IntInt );

void UObject::execAddAdd_PreInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddAdd_PreInt);

	P_GET_INT_REF(A);
	P_FINISH;

	*(INT*)Result = ++(*A);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 163, execAddAdd_PreInt );

void UObject::execSubtractSubtract_PreInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractSubtract_PreInt);

	P_GET_INT_REF(A);
	P_FINISH;

	*(INT*)Result = --(*A);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 164, execSubtractSubtract_PreInt );

void UObject::execAddAdd_Int( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddAdd_Int);

	P_GET_INT_REF(A);
	P_FINISH;

	*(INT*)Result = (*A)++;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 165, execAddAdd_Int );

void UObject::execSubtractSubtract_Int( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractSubtract_Int);

	P_GET_INT_REF(A);
	P_FINISH;

	*(INT*)Result = (*A)--;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 166, execSubtractSubtract_Int );

void UObject::execRand( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execRand);

	P_GET_INT(A);
	P_FINISH;

	*(INT*)Result = A>0 ? (appRand() % A) : 0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 167, execRand );

void UObject::execMin( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMin);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = Min(A,B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 249, execMin );

void UObject::execMax( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMax);

	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = Max(A,B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 250, execMax );

void UObject::execClamp( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execClamp);

	P_GET_INT(V);
	P_GET_INT(A);
	P_GET_INT(B);
	P_FINISH;

	*(INT*)Result = Clamp(V,A,B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 251, execClamp );

///////////////////////////////////
// Float operators and functions //
///////////////////////////////////

void UObject::execSubtract_PreFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtract_PreFloat);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = -A;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 169, execSubtract_PreFloat );

void UObject::execMultiplyMultiply_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiplyMultiply_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = appPow(A,B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 170, execMultiplyMultiply_FloatFloat );

void UObject::execMultiply_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiply_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A * B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 171, execMultiply_FloatFloat );

void UObject::execDivide_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivide_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A / B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 172, execDivide_FloatFloat );

void UObject::execPercent_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execPercent_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = appFmod(A,B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 173, execPercent_FloatFloat );

void UObject::execAdd_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAdd_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A + B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 174, execAdd_FloatFloat );

void UObject::execSubtract_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtract_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A - B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 175, execSubtract_FloatFloat );

void UObject::execLess_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLess_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A < B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 176, execLess_FloatFloat );

void UObject::execGreater_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreater_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A > B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 177, execGreater_FloatFloat );

void UObject::execLessEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLessEqual_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A <= B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 178, execLessEqual_FloatFloat );

void UObject::execGreaterEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreaterEqual_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A >= B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 179, execGreaterEqual_FloatFloat );

void UObject::execEqualEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEqualEqual_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A == B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 180, execEqualEqual_FloatFloat );

void UObject::execNotEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNotEqual_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = A != B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 181, execNotEqual_FloatFloat );

void UObject::execComplementEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execComplementEqual_FloatFloat);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(DWORD*)Result = Abs(A - B) < (1.e-4);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 210, execComplementEqual_FloatFloat );

void UObject::execMultiplyEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiplyEqual_FloatFloat);

	P_GET_FLOAT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = (*A *= B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 182, execMultiplyEqual_FloatFloat );

void UObject::execDivideEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivideEqual_FloatFloat);

	P_GET_FLOAT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = (*A /= B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 183, execDivideEqual_FloatFloat );

void UObject::execAddEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddEqual_FloatFloat);

	P_GET_FLOAT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = (*A += B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 184, execAddEqual_FloatFloat );

void UObject::execSubtractEqual_FloatFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractEqual_FloatFloat);

	P_GET_FLOAT_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = (*A -= B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 185, execSubtractEqual_FloatFloat );

void UObject::execAbs( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAbs);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = Abs(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 186, execAbs );

void UObject::execSin( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSin);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appSin(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 187, execSin );

void UObject::execAsin( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAsin);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appAsin(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, -1, execAsin );

void UObject::execCos( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execCos);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appCos(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 188, execCos );

void UObject::execAcos( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAcos);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appAcos(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, -1, execAcos );

void UObject::execTan( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execTan);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appTan(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 189, execTan );

void UObject::execAtan( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAtan);

	P_GET_FLOAT(A);
    P_GET_FLOAT(B); //amb
	P_FINISH;

    *(FLOAT*)Result = appAtan2(A,B); //amb: changed to atan2

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 190, execAtan );

void UObject::execExp( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execExp);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appExp(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 191, execExp );

void UObject::execLoge( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLoge);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appLoge(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 192, execLoge );

void UObject::execSqrt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSqrt);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = appSqrt(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 193, execSqrt );

void UObject::execSquare( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSquare);

	P_GET_FLOAT(A);
	P_FINISH;

	*(FLOAT*)Result = Square(A);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 194, execSquare );

void UObject::execFRand( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFRand);

	P_FINISH;

	*(FLOAT*)Result = appFrand();

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 195, execFRand );

void UObject::execFMin( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFMin);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = Min(A,B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 244, execFMin );

void UObject::execFMax( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFMax);

	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = Max(A,B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 245, execFMax );

void UObject::execFClamp( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFClamp);

	P_GET_FLOAT(V);
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = Clamp(V,A,B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 246, execFClamp );

void UObject::execLerp( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLerp);

	P_GET_FLOAT(V);
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
    P_GET_UBOOL_OPTX(bClamp,0); //amb
	P_FINISH;

    FLOAT f = A + V*(B-A);

    // amb --- 
    if (bClamp)
        f = Clamp(f,A,B);
    // --- amb

    *(FLOAT*)Result = f;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 247, execLerp );

void UObject::execSmerp( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSmerp);

	P_GET_FLOAT(V);
	P_GET_FLOAT(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FLOAT*)Result = A + (3.f*V*V - 2.f*V*V*V)*(B-A);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 248, execSmerp );

// gam ---
void UObject::execCeil( FFrame& Stack, RESULT_DECL )
{
    guardSlow(UObject::execCeil);

    P_GET_FLOAT(A);
    P_FINISH;

    *(FLOAT*)Result = appCeil (A);

    unguardexecSlow;
}   
IMPLEMENT_FUNCTION( UObject, 253, execCeil );
void UObject::execRound( FFrame& Stack, RESULT_DECL )
{
    guardSlow(UObject::execRound);

    P_GET_FLOAT(A);
    P_FINISH;

    *(FLOAT*)Result = appRound (A);

    unguardexecSlow;
}   
IMPLEMENT_FUNCTION( UObject, 257, execRound );
// --- gam

void UObject::execRotationConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execRotationConst);
	((FRotator*)Result)->Pitch = Stack.ReadInt();
	((FRotator*)Result)->Yaw   = Stack.ReadInt();
	((FRotator*)Result)->Roll  = Stack.ReadInt();
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_RotationConst, execRotationConst );

void UObject::execVectorConst( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execVectorConst);
#if __PSX2_EE__ || __GCN__ || (defined(__ANDROID__) && defined(__arm__))
	appMemcpy(Result, Stack.Code, sizeof(FVector));
	#else
	*(FVector*)Result = *(FVector*)Stack.Code;
	#endif
	Stack.Code += sizeof(FVector);
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_VectorConst, execVectorConst );

/////////////////
// Conversions //
/////////////////

void UObject::execStringToVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStringToVector);
	P_GET_STR(Str);

	const TCHAR* Stream = *Str;
	FVector Value(0,0,0);
	Value.X = appAtof(Stream);
	Stream = appStrstr(Stream,TEXT(","));
	if( Stream )
	{
		Value.Y = appAtof(++Stream);
		Stream = appStrstr(Stream,TEXT(","));
		if( Stream )
			Value.Z = appAtof(++Stream);
	}
	*(FVector*)Result = Value;

	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_StringToVector, execStringToVector );

void UObject::execStringToRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStringToRotator);
	P_GET_STR(Str);

	const TCHAR* Stream = *Str;
	FRotator Rotation(0,0,0);
	Rotation.Pitch = appAtoi(Stream);
	Stream = appStrstr(Stream,TEXT(","));
	if( Stream )
	{
		Rotation.Yaw = appAtoi(++Stream);
		Stream = appStrstr(Stream,TEXT(","));
		if( Stream )
			Rotation.Roll = appAtoi(++Stream);
	}
	*(FRotator*)Result = Rotation;

	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_StringToRotator, execStringToRotator );

void UObject::execVectorToBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execVectorToBool);
	FVector V(0,0,0);
	Stack.Step( Stack.Object, &V );
	*(DWORD*)Result = V.IsZero() ? 0 : 1;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_VectorToBool, execVectorToBool );

void UObject::execVectorToString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execVectorToString);
	P_GET_VECTOR(V);
	*(FString*)Result = FString::Printf( TEXT("%.2f,%.2f,%.2f"), V.X, V.Y, V.Z );
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_VectorToString, execVectorToString );

void UObject::execVectorToRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execVectorToRotator);
	FVector V(0,0,0);
	Stack.Step( Stack.Object, &V );
	*(FRotator*)Result = V.Rotation();
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_VectorToRotator, execVectorToRotator );

void UObject::execRotatorToBool( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execRotatorToBool);
	FRotator R(0,0,0);
	Stack.Step( Stack.Object, &R );
	*(DWORD*)Result = R.IsZero() ? 0 : 1;
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_RotatorToBool, execRotatorToBool );

void UObject::execRotatorToVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execRotatorToVector);
	FRotator R(0,0,0);
	Stack.Step( Stack.Object, &R );
	*(FVector*)Result = R.Vector();
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_RotatorToVector, execRotatorToVector );

void UObject::execRotatorToString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execRotatorToString);
	P_GET_ROTATOR(R);
	*(FString*)Result = FString::Printf( TEXT("%i,%i,%i"), R.Pitch&65535, R.Yaw&65535, R.Roll&65535 );
	unguardexecSlow;
}
IMPLEMENT_CAST_FUNCTION( UObject, CST_RotatorToString, execRotatorToString );

////////////////////////////////////
// Vector operators and functions //
////////////////////////////////////

void UObject::execSubtract_PreVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtract_PreVector);

	P_GET_VECTOR(A);
	P_FINISH;

	*(FVector*)Result = -A;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 83, execSubtract_PreVector );

void UObject::execMultiply_VectorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiply_VectorFloat);

	P_GET_VECTOR(A);
	P_GET_FLOAT (B);
	P_FINISH;

	*(FVector*)Result = A*B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 84, execMultiply_VectorFloat );

void UObject::execMultiply_FloatVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiply_FloatVector);

	P_GET_FLOAT (A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A*B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 85, execMultiply_FloatVector );

void UObject::execMultiply_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiply_VectorVector);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A*B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 296, execMultiply_VectorVector );

void UObject::execDivide_VectorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivide_VectorFloat);

	P_GET_VECTOR(A);
	P_GET_FLOAT (B);
	P_FINISH;

	*(FVector*)Result = A/B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 86, execDivide_VectorFloat );

void UObject::execAdd_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAdd_VectorVector);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A+B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 87, execAdd_VectorVector );

void UObject::execSubtract_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtract_VectorVector);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A-B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 88, execSubtract_VectorVector );

void UObject::execLessLess_VectorRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLessLess_VectorRotator);

	P_GET_VECTOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FVector*)Result = A.TransformVectorBy(GMath.UnitCoords / B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 275, execLessLess_VectorRotator );

void UObject::execGreaterGreater_VectorRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreaterGreater_VectorRotator);

	P_GET_VECTOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FVector*)Result = A.TransformVectorBy(GMath.UnitCoords * B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 276, execGreaterGreater_VectorRotator );

void UObject::execEqualEqual_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEqualEqual_VectorVector);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(DWORD*)Result = A.X==B.X && A.Y==B.Y && A.Z==B.Z;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 89, execEqualEqual_VectorVector );

void UObject::execNotEqual_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNotEqual_VectorVector);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(DWORD*)Result = A.X!=B.X || A.Y!=B.Y || A.Z!=B.Z;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 90, execNotEqual_VectorVector );

void UObject::execDot_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDot_VectorVector);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FLOAT*)Result = A|B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 91, execDot_VectorVector );

void UObject::execCross_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execCross_VectorVector);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A^B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 92, execCross_VectorVector );

void UObject::execMultiplyEqual_VectorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiplyEqual_VectorFloat);

	P_GET_VECTOR_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FVector*)Result = (*A *= B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 93, execMultiplyEqual_VectorFloat );

void UObject::execMultiplyEqual_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiplyEqual_VectorVector);

	P_GET_VECTOR_REF(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = (*A *= B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 297, execMultiplyEqual_VectorVector );

void UObject::execDivideEqual_VectorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivideEqual_VectorFloat);

	P_GET_VECTOR_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FVector*)Result = (*A /= B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 94, execDivideEqual_VectorFloat );

void UObject::execAddEqual_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddEqual_VectorVector);

	P_GET_VECTOR_REF(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = (*A += B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 95, execAddEqual_VectorVector );

void UObject::execSubtractEqual_VectorVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractEqual_VectorVector);

	P_GET_VECTOR_REF(A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = (*A -= B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 96, execSubtractEqual_VectorVector );

void UObject::execVSize( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execVSize);

	P_GET_VECTOR(A);
	P_FINISH;

	*(FLOAT*)Result = A.Size();

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 97, execVSize );

void UObject::execNormal( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNormal);

	P_GET_VECTOR(A);
	P_FINISH;

	*(FVector*)Result = A.SafeNormal();

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 0x80 + 98, execNormal );

void UObject::execInvert( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execInvert);

	P_GET_VECTOR_REF(X);
	P_GET_VECTOR_REF(Y);
	P_GET_VECTOR_REF(Z);
	P_FINISH;

	FCoords Temp = FCoords( FVector(0,0,0), *X, *Y, *Z ).Inverse();
	*X           = Temp.XAxis;
	*Y           = Temp.YAxis;
	*Z           = Temp.ZAxis;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 0x80 + 99, execInvert );

void UObject::execVRand( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execVRand);
	P_FINISH;
	*((FVector*)Result) = VRand();
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 0x80 + 124, execVRand );

void UObject::execRotRand( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execRotRand);
	P_GET_UBOOL_OPTX(bRoll, 0);
	P_FINISH;

	FRotator RRot;
	RRot.Yaw = ((2 * appRand()) % 65535);
	RRot.Pitch = ((2 * appRand()) % 65535);
	if ( bRoll )
		RRot.Roll = ((2 * appRand()) % 65535);
	else
		RRot.Roll = 0;
	*((FRotator*)Result) = RRot;
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 320, execRotRand );

void UObject::execMirrorVectorByNormal( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMirrorVectorByNormal);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	B = B.SafeNormal();
	*(FVector*)Result = A - 2.f * B * (B | A);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 300, execMirrorVectorByNormal );

//////////////////////////////////////
// Rotation operators and functions //
//////////////////////////////////////

void UObject::execEqualEqual_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEqualEqual_RotatorRotator);

	P_GET_ROTATOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(DWORD*)Result = A.Pitch==B.Pitch && A.Yaw==B.Yaw && A.Roll==B.Roll;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 14, execEqualEqual_RotatorRotator );

void UObject::execNotEqual_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNotEqual_RotatorRotator);

	P_GET_ROTATOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(DWORD*)Result = A.Pitch!=B.Pitch || A.Yaw!=B.Yaw || A.Roll!=B.Roll;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 0x80 + 75, execNotEqual_RotatorRotator );

void UObject::execMultiply_RotatorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiply_RotatorFloat);

	P_GET_ROTATOR(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FRotator*)Result = A * B;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 287, execMultiply_RotatorFloat );

void UObject::execMultiply_FloatRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiply_FloatRotator);

	P_GET_FLOAT(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = B * A;

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 288, execMultiply_FloatRotator );

void UObject::execDivide_RotatorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivide_RotatorFloat);

	P_GET_ROTATOR(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FRotator*)Result = A * (1.f/B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 289, execDivide_RotatorFloat );

void UObject::execMultiplyEqual_RotatorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMultiplyEqual_RotatorFloat);

	P_GET_ROTATOR_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FRotator*)Result = (*A *= B);

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 290, execMultiplyEqual_RotatorFloat );

void UObject::execDivideEqual_RotatorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivideEqual_RotatorFloat);

	P_GET_ROTATOR_REF(A);
	P_GET_FLOAT(B);
	P_FINISH;

	*(FRotator*)Result = (*A *= (1.f/B));

	unguardexecSlow;
}	
IMPLEMENT_FUNCTION( UObject, 291, execDivideEqual_RotatorFloat );

void UObject::execAdd_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAdd_RotatorRotator);

	P_GET_ROTATOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = A + B;

	unguardSlow;
}
IMPLEMENT_FUNCTION( UObject, 316, execAdd_RotatorRotator );

void UObject::execSubtract_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtract_RotatorRotator);

	P_GET_ROTATOR(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = A - B;

	unguardSlow;
}
IMPLEMENT_FUNCTION( UObject, 317, execSubtract_RotatorRotator );

void UObject::execAddEqual_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAddEqual_RotatorRotator);

	P_GET_ROTATOR_REF(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = (*A += B);

	unguardSlow;
}
IMPLEMENT_FUNCTION( UObject, 318, execAddEqual_RotatorRotator );

void UObject::execSubtractEqual_RotatorRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractEqual_RotatorRotator);

	P_GET_ROTATOR_REF(A);
	P_GET_ROTATOR(B);
	P_FINISH;

	*(FRotator*)Result = (*A -= B);

	unguardSlow;
}
IMPLEMENT_FUNCTION( UObject, 319, execSubtractEqual_RotatorRotator );

void UObject::execGetAxes( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGetAxes);

	P_GET_ROTATOR(A);
	P_GET_VECTOR_REF(X);
	P_GET_VECTOR_REF(Y);
	P_GET_VECTOR_REF(Z);
	P_FINISH;

	FCoords Coords = GMath.UnitCoords / A;
	*X = Coords.XAxis;
	*Y = Coords.YAxis;
	*Z = Coords.ZAxis;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 0x80 + 101, execGetAxes );

void UObject::execGetUnAxes( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGetUnAxes);

	P_GET_ROTATOR(A);
	P_GET_VECTOR_REF(X);
	P_GET_VECTOR_REF(Y);
	P_GET_VECTOR_REF(Z);
	P_FINISH;

	FCoords Coords = GMath.UnitCoords * A;
	*X = Coords.XAxis;
	*Y = Coords.YAxis;
	*Z = Coords.ZAxis;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 0x80 + 102, execGetUnAxes );

void UObject::execOrthoRotation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execOrthoRotation);

	P_GET_VECTOR(X);
	P_GET_VECTOR(Y);
	P_GET_VECTOR(Z);
	P_FINISH;

	FCoords Coords( FVector(0,0,0), X, Y, Z );
	*(FRotator*)Result = Coords.OrthoRotation();

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execOrthoRotation );

void UObject::execNormalize( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNormalize);

	P_GET_ROTATOR(Rot);
	P_FINISH;

	Rot.Pitch = Rot.Pitch & 0xFFFF; if( Rot.Pitch > 32767 ) Rot.Pitch -= 0x10000;
	Rot.Roll  = Rot.Roll  & 0xFFFF; if( Rot.Roll  > 32767 )	Rot.Roll  -= 0x10000;
	Rot.Yaw   = Rot.Yaw   & 0xFFFF; if( Rot.Yaw   > 32767 )	Rot.Yaw   -= 0x10000;
	*(FRotator*)Result = Rot;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execNormalize );

void UObject::execClockwiseFrom_IntInt( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execClockwiseFrom_IntInt);

	P_GET_INT(IntA);
	P_GET_INT(IntB);
	P_FINISH;

	IntA = IntA & 0xFFFF;
	IntB = IntB & 0xFFFF;

	*(DWORD*)Result = ( Abs(IntA - IntB) > 32768 ) ? ( IntA < IntB ) : ( IntA > IntB );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execClockwiseFrom_IntInt );

////////////////////////////////////
// InterpCurve functions          //
////////////////////////////////////

void UObject::execInterpCurveEval( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execInterpCurveEval);

	P_GET_STRUCT(FInterpCurveInit, curve); // We use a FInterpCurveInit, because we want to initialise it before copying into it.
	P_GET_FLOAT(input);
	P_FINISH;

	*(FLOAT*)Result = curve.Eval(input);

	unguard;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execInterpCurveEval);

void UObject::execInterpCurveGetInputDomain( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execInterpCurveGetInputDomain);

	P_GET_STRUCT(FInterpCurveInit, curve);
	P_GET_FLOAT_REF(min);
	P_GET_FLOAT_REF(max);
	P_FINISH;

	if(curve.Points.Num() < 2)
	{
		*min = 0.f;
		*max = 0.f;
		return;
	}

	*min = curve.Points(0).InVal;
	*max = curve.Points(0).InVal;

	for(INT i=1; i<curve.Points.Num(); i++)
	{
		*min = Min(*min, curve.Points(i).InVal);
		*max = Max(*max, curve.Points(i).InVal);
	}

	unguard;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execInterpCurveGetInputDomain);

void UObject::execInterpCurveGetOutputRange( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execInterpCurveGetOutputRange);

	P_GET_STRUCT(FInterpCurveInit, curve);
	P_GET_FLOAT_REF(min);
	P_GET_FLOAT_REF(max);
	P_FINISH;

	if(curve.Points.Num() < 2)
	{
		*min = 0.f;
		*max = 0.f;
		return;
	}

	*min = curve.Points(0).OutVal;
	*max = curve.Points(0).OutVal;

	for(INT i=1; i<curve.Points.Num(); i++)
	{
		*min = Min(*min, curve.Points(i).OutVal);
		*max = Max(*max, curve.Points(i).OutVal);
	}

	unguard;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execInterpCurveGetOutputRange);

////////////////////////////////////
// Quaternion functions           //
////////////////////////////////////

void UObject::execQuatProduct( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execQuatProduct);

	P_GET_STRUCT(FQuat, A);
	P_GET_STRUCT(FQuat, B);
	P_FINISH;

	*(FQuat*)Result = A * B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execQuatProduct);

void UObject::execQuatInvert( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execQuatInvert);

	P_GET_STRUCT(FQuat, A);
	P_FINISH;

	FQuat invA(-A.X, -A.Y, -A.Z, A.W);
	*(FQuat*)Result = invA;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execQuatInvert);

void UObject::execQuatRotateVector( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execQuatRotateVector);

	P_GET_STRUCT(FQuat, A);
	P_GET_VECTOR(B);
	P_FINISH;

	*(FVector*)Result = A.RotateVector(B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execQuatRotateVector);

// Generate the 'smallest' (geodesic) rotation between these two vectors.
void UObject::execQuatFindBetween( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execQuatFindBetween);

	P_GET_VECTOR(A);
	P_GET_VECTOR(B);
	P_FINISH;

	FVector cross = A ^ B;
	FLOAT crossMag = cross.Size();

	// If these vectors are basically parallel - just return identity quaternion (ie no rotation).
	if(crossMag < KINDA_SMALL_NUMBER)
	{
		*(FQuat*)Result = FQuat(0, 0, 0, 1);
		return;
	}

	FLOAT angle = appAsin(crossMag);

	FLOAT dot = A | B;
	if(dot < 0.f)
		angle = PI - angle;

	FLOAT sinHalfAng = appSin(0.5f * angle);
	FLOAT cosHalfAng = appCos(0.5f * angle);
	FVector axis = cross / crossMag;

	*(FQuat*)Result = FQuat(
		sinHalfAng * axis.X,
		sinHalfAng * axis.Y,
		sinHalfAng * axis.Z,
		cosHalfAng );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execQuatFindBetween);

void UObject::execQuatFromAxisAndAngle( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execQuatFromAxisAndAngle);

	P_GET_VECTOR(Axis);
	P_GET_FLOAT(Angle);
	P_FINISH;

	FLOAT sinHalfAng = appSin(0.5f * Angle);
	FLOAT cosHalfAng = appCos(0.5f * Angle);
	FVector normAxis = Axis.SafeNormal();

	*(FQuat*)Result = FQuat(
		sinHalfAng * normAxis.X,
		sinHalfAng * normAxis.Y,
		sinHalfAng * normAxis.Z,
		cosHalfAng );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execQuatFromAxisAndAngle);

void UObject::execQuatFromRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execQuatFromRotator);

	P_GET_ROTATOR(A);
	P_FINISH;

	FCoords C = GMath.UnitCoords / A;

	*(FQuat*)Result = FCoordsQuaternion(C);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execQuatFromRotator);

void UObject::execQuatToRotator( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execQuatToRotator);

	P_GET_STRUCT(FQuat, A);
	P_FINISH;

	FCoords C = FQuaternionCoords(A);

	*(FRotator*)Result = C.OrthoRotation();

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execQuatToRotator);

void UObject::execQuatSlerp( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execQuatSlerp);

	P_GET_STRUCT(FQuat, A);
	P_GET_STRUCT(FQuat, B);
	P_GET_FLOAT(Slerp);
	P_FINISH;

	*(FQuat*)Result = SlerpQuat(A, B, Slerp);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execQuatSlerp);

////////////////////////////////////
// Str operators and functions //
////////////////////////////////////

void UObject::execEatString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEatString);

	// Call function returning a string, then discard the result.
	FString String;
	Stack.Step( this, &String );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_EatString, execEatString );

void UObject::execConcat_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execConcat_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(FString*)Result = (A+B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 112, execConcat_StringString );

void UObject::execConcatEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execConcatEqual_StringString);

	P_GET_STR_REF(A);
	P_GET_STR(B);
	P_FINISH;

	*(FString*)Result = ( *(A) += B );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 322, execConcatEqual_StringString );

void UObject::execAt_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAt_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(FString*)Result = (A+TEXT(" ")+B);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 168, execAt_StringString );

void UObject::execAtEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAtEqual_StringString);

	P_GET_STR_REF(A);
	P_GET_STR(B);
	P_FINISH;

	*(FString*)Result = (*(A) += FString::Printf(TEXT(" %s"), *B));

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 323, execAtEqual_StringString );

void UObject::execLess_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLess_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)<0;;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 115, execLess_StringString );

void UObject::execGreater_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreater_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)>0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 116, execGreater_StringString );

void UObject::execLessEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLessEqual_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)<=0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 120, execLessEqual_StringString );

void UObject::execGreaterEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGreaterEqual_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)>=0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 121, execGreaterEqual_StringString );

void UObject::execEqualEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEqualEqual_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)==0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 122, execEqualEqual_StringString );

void UObject::execNotEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNotEqual_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStrcmp(*A,*B)!=0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 123, execNotEqual_StringString );

void UObject::execComplementEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execComplementEqual_StringString);

	P_GET_STR(A);
	P_GET_STR(B);
	P_FINISH;

	*(DWORD*)Result = appStricmp(*A,*B)==0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 124, execComplementEqual_StringString );

void UObject::execSubtractEqual_StringString( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSubtractEqual_StringString);

	P_GET_STR_REF(A);
	P_GET_STR(B);
	P_FINISH;

	AT.Empty();
	INT pos = A->InStr(B);
	while (pos != -1)
	{
		AT += A->Left(pos);
		*A = A->Mid(pos + B.Len());
		pos = A->InStr(B);
	}

	if (AT != TEXT("")) *A = (AT + *A);
	*(FString*)Result = *A;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 324, execSubtractEqual_StringString );

void UObject::execLen( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLen);

	P_GET_STR(S);
	P_FINISH;

	*(INT*)Result = S.Len();

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 125, execLen );

void UObject::execInStr( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execInStr);

	P_GET_STR(S);
	P_GET_STR(A);
	P_FINISH;
	*(INT*)Result = S.InStr(A);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 126, execInStr );

void UObject::execMid( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execMid);

	P_GET_STR(A);
	P_GET_INT(I);
	P_GET_INT_OPTX(C,65535);
	P_FINISH;

	*(FString*)Result = A.Mid(I,C);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 127, execMid );

void UObject::execLeft( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLeft);

	P_GET_STR(A);
	P_GET_INT(N);
	P_FINISH;

	*(FString*)Result = A.Left(N);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 128, execLeft );

void UObject::execRight( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execRight);

	P_GET_STR(A);
	P_GET_INT(N);
	P_FINISH;

	*(FString*)Result = A.Right(N);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 234, execRight );

void UObject::execCaps( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execCaps);

	P_GET_STR(A);
	P_FINISH;

	*(FString*)Result = A.Caps();

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 235, execCaps );

// rjp --
void UObject::execLocs( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLocs);

	P_GET_STR(Str);
	P_FINISH;

	*(FString*)Result = Str.Locs();

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 238, execLocs );

void UObject::execStrCmp( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execStrCmp);

	P_GET_STR(S);
	P_GET_STR(T);
	P_GET_INT_OPTX(Count,INDEX_NONE);
	P_GET_UBOOL_OPTX(bCaseSensitive,0);
	P_FINISH;

	if ( bCaseSensitive )
		*(INT*)Result = Count == INDEX_NONE ? appStrcmp(*S,*T) : appStrncmp( *S, *T, Count);
	else 
		*(INT*)Result = Count == INDEX_NONE ? appStricmp(*S,*T) : appStrnicmp( *S, *T, Count );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 200, execStrCmp );

void UObject::execRepl( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execRepl);

	P_GET_STR(Src);
	P_GET_STR(Match);
	P_GET_STR(With);
	P_GET_UBOOL_OPTX( bCaseSensitive, 0 );
	P_FINISH;

	INT i = bCaseSensitive ? Src.InStr( Match ) : Src.Caps().InStr( Match.Caps() );

	FString NewSrc = TEXT("");
	while (i != -1)
	{
		NewSrc += (Src.Left(i) + With);
		Src = Src.Mid( i + Match.Len() );
		i = bCaseSensitive ? Src.InStr(Match) : Src.Caps().InStr( Match.Caps() );
	}

	*(FString*)Result = (NewSrc + Src);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 201, execRepl );

void UObject::execEval( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEval);

	P_GET_UBOOL(Condition);
	P_GET_STR(ResultIfTrue);
	P_GET_STR(ResultIfFalse);
	P_FINISH;

	if ( Condition )
		*(FString*)Result = ResultIfTrue;
	else *(FString*)Result = ResultIfFalse;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 202, execEval );
// -- rjp

void UObject::execChr( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execChr);

	P_GET_INT(i);
	P_FINISH;

	TCHAR Temp[2];
	Temp[0] = i;
	Temp[1] = 0;
	*(FString*)Result = Temp;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 236, execChr );

void UObject::execAsc( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAsc);

	P_GET_STR(S);
	P_FINISH;

	*(INT*)Result = **S;	

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 237, execAsc );

void UObject::execDivide( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDivideString);

	P_GET_STR(Src);
	P_GET_STR(Divider);
	P_GET_STR_REF(LeftPart);
	P_GET_STR_REF(RightPart);
	P_FINISH;

	*(DWORD*) Result = Src.Split(Divider, LeftPart, RightPart);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 239, execDivide);

void UObject::execSplit( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSplit);
	P_GET_STR(Src);
	P_GET_STR(Divider);
	P_GET_TARRAY_REF(Parts, FString);
	P_FINISH;

	Parts->Empty();
	*(INT*) Result = Src.ParseIntoArray(*Divider, Parts);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 240, execSplit);


/////////////////////////////////////////
// Native name operators and functions //
/////////////////////////////////////////

void UObject::execEqualEqual_NameName( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEqualEqual_NameName);

	P_GET_NAME(A);
	P_GET_NAME(B);
	P_FINISH;

	*(DWORD*)Result = A == B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 254, execEqualEqual_NameName );

void UObject::execNotEqual_NameName( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNotEqual_NameName);

	P_GET_NAME(A);
	P_GET_NAME(B);
	P_FINISH;

	*(DWORD*)Result = A != B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 255, execNotEqual_NameName );

////////////////////////////////////
// Object operators and functions //
////////////////////////////////////

void UObject::execEqualEqual_ObjectObject( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEqualEqual_ObjectObject);

	P_GET_OBJECT(UObject,A);
	P_GET_OBJECT(UObject,B);
	P_FINISH;

    // amb --- Object is invalid if bdeleteme is set
    if (A && A->IsPendingKill())
        A = NULL;
    
    if (B && B->IsPendingKill())
        B = NULL;
    // --- amb
	*(DWORD*)Result = A == B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 114, execEqualEqual_ObjectObject );

void UObject::execNotEqual_ObjectObject( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNotEqual_ObjectObject);

	P_GET_OBJECT(UObject,A);
	P_GET_OBJECT(UObject,B);
	P_FINISH;

    // amb --- Object is invalid if bdeleteme is set
    if (A && A->IsPendingKill())
        A = NULL;
    
    if (B && B->IsPendingKill())
        B = NULL;
    // --- amb

	*(DWORD*)Result = A != B;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 119, execNotEqual_ObjectObject );

/////////////////////////////
// Log and error functions //
/////////////////////////////

void UObject::execLog( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLog);

	P_GET_STR(S);
	P_GET_NAME_OPTX(N,NAME_ScriptLog);
	P_FINISH;

	INT len = 960 - appStrlen(*N);  // Account for colon, space, and \n
	for (; S.Len() >= len; S = S.Mid(len) )
		debugf( (EName)N.GetIndex(), TEXT("%s"), *S.Left(len) );
	debugf( (EName)N.GetIndex(), TEXT("%s%s"), StopWatchTimer != 0.f ? *FString::Printf(TEXT("%lf: "), (DOUBLE)(StopWatchTimer+(appCycles()-12)) * GSecondsPerCycle * 1000.f) : TEXT(""), *S );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 231, execLog );

void UObject::execWarn( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execWarn);

	P_GET_STR(S);
	P_FINISH;

    Stack.Logf( NAME_Warning, TEXT("%s"), *S ); // gam

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 232, execWarn );

void UObject::execLocalize( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execLocalize);

	P_GET_STR(SectionName);
	P_GET_STR(KeyName);
	P_GET_STR(PackageName);
	P_FINISH;

	*(FString*)Result = Localize( *SectionName, *KeyName, *PackageName );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execLocalize );

//////////////////
// High natives //
//////////////////

#define HIGH_NATIVE(n) \
void UObject::execHighNative##n( FFrame& Stack, RESULT_DECL ) \
{ \
	guardSlow(UObject::execHighNative##n); \
	BYTE B = *Stack.Code++; \
	(this->*GNatives[ n*0x100 + B ])( Stack, Result ); \
	unguardexecSlow; \
} \
IMPLEMENT_FUNCTION( UObject, 0x60 + n, execHighNative##n );

HIGH_NATIVE(0);
HIGH_NATIVE(1);
HIGH_NATIVE(2);
HIGH_NATIVE(3);
HIGH_NATIVE(4);
HIGH_NATIVE(5);
HIGH_NATIVE(6);
HIGH_NATIVE(7);
HIGH_NATIVE(8);
HIGH_NATIVE(9);
HIGH_NATIVE(10);
HIGH_NATIVE(11);
HIGH_NATIVE(12);
HIGH_NATIVE(13);
HIGH_NATIVE(14);
HIGH_NATIVE(15);
#undef HIGH_NATIVE

/////////////////////////
// Object construction //
/////////////////////////

void UObject::execNew( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execNew);

	// Get parameters.
	P_GET_OBJECT_OPTX(UObject,Outer,NULL);
	P_GET_STR_OPTX(Name,TEXT(""));
	P_GET_INT_OPTX(Flags,0);
	P_GET_OBJECT_OPTX(UClass,Cls,NULL);

	// Validate parameters.
	if( Flags & ~RF_ScriptMask )
        Stack.Logf( NAME_Error, TEXT("new: Flags %08X not allowed"), Flags & ~RF_ScriptMask ); // gam

	// Construct new object.
	if( !Outer )
		Outer = GetTransientPackage();
	UObject* obj = StaticConstructObject( Cls, Outer, Name.Len()?FName(*Name):NAME_None, Flags&RF_ScriptMask, NULL, &Stack );
    *(UObject**)Result = obj;

    // amb ---
    if (obj)
        obj->eventCreated(); 
    // --- amb

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, EX_New, execNew );

/////////////////////////////
// Class related functions //
/////////////////////////////

void UObject::execClassIsChildOf( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execClassIsChildOf);

	P_GET_OBJECT(UClass,K);
	P_GET_OBJECT(UClass,C);
	P_FINISH;

	*(DWORD*)Result = (C && K) ? K->IsChildOf(C) : 0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 258, execClassIsChildOf );

///////////////////////////////
// State and label functions //
///////////////////////////////

void UObject::execGotoState( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGotoState);

	// Get parameters.
	FName CurrentStateName = (StateFrame && StateFrame->StateNode!=Class) ? StateFrame->StateNode->GetFName() : FName(NAME_None);
	P_GET_NAME_OPTX( S, CurrentStateName );
	P_GET_NAME_OPTX( L, NAME_None );
	P_FINISH;

    // gam ---
    if( S == GetClass()->GetFName() )
        S = NAME_None;
    // --- gam

	// Go to the state.
	EGotoState Result = GOTOSTATE_Success;
	if( S!=CurrentStateName )
		Result = GotoState( S );

	// Handle success.
	if( Result==GOTOSTATE_Success )
	{
		// Now go to the label.
		if( !GotoLabel( L==NAME_None ? FName(NAME_Begin) : L ) && L!=NAME_None )
			Stack.Logf( NAME_Error, TEXT("GotoState (%s %s): Label not found"), *S, *L );
	}
	else if( Result==GOTOSTATE_NotFound )
	{
		// Warning.
		if( S!=NAME_None && S!=NAME_Auto )
            Stack.Logf( NAME_Error, TEXT("GotoState (%s %s): State not found"), *S, *L ); // gam
		else if ( S==NAME_None && GDebugger )
			GDebugger->DebugInfo(this, &Stack,DI_PrevStackState,0,0);
	}
	else
	{
		// Safely preempted by another GotoState.
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 113, execGotoState );

void UObject::execEnable( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execEnable);

	P_GET_NAME(N);
	if( N.GetIndex()>=NAME_PROBEMIN && N.GetIndex()<NAME_PROBEMAX && StateFrame )
	{
		QWORD BaseProbeMask = (GetStateFrame()->StateNode->ProbeMask | GetClass()->ProbeMask) & GetStateFrame()->StateNode->IgnoreMask;
		GetStateFrame()->ProbeMask |= (BaseProbeMask & ((QWORD)1<<(N.GetIndex()-NAME_PROBEMIN)));
	}
    else Stack.Logf( NAME_Error, TEXT("Enable: '%s' is not a probe function"), *N ); // gam
	P_FINISH;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 117, execEnable );

void UObject::execDisable( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDiable);

	P_GET_NAME(N);
	P_FINISH;

	if( N.GetIndex()>=NAME_PROBEMIN && N.GetIndex()<NAME_PROBEMAX && StateFrame )
		GetStateFrame()->ProbeMask &= ~((QWORD)1<<(N.GetIndex()-NAME_PROBEMIN));
	else
        Stack.Logf( NAME_Error, TEXT("Enable: '%s' is not a probe function"), *N ); // gam

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 118, execDisable );

///////////////////
// Property text //
///////////////////

void UObject::execGetPropertyText( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGetPropertyText);

	P_GET_STR(PropName);
	P_FINISH;

	UProperty* Property=FindField<UProperty>( Class, *PropName );
	if( Property && (Property->GetFlags() & RF_Public) )
	{
		TCHAR Temp[1024]=TEXT("");//!!
        for( INT i=0; i<Property->ArrayDim; i++ )
        {
		    Property->ExportText( i, Temp, (BYTE*)this, (BYTE*)this, PPF_Localized );
            if( i > 0 )
                *(FString*)Result += TEXT(",");
            *(FString*)Result += Temp;
        }
		//*(FString*)Result = Temp;
	}
	else *(FString*)Result = TEXT("");

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execGetPropertyText );

void UObject::execSetPropertyText( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execSetPropertyText);

	P_GET_STR(PropName);
	P_GET_STR(PropValue);
	P_FINISH;

	UProperty* Property=FindField<UProperty>( Class, *PropName );
	if
	(	(Property)
	&&	(Property->GetFlags() & RF_Public)
	&&	!(Property->PropertyFlags & CPF_Const) )
    {
		Property->ImportText( *PropValue, (BYTE*)this + Property->Offset, PPF_Localized );
        *(DWORD*)Result = 1; //amb: return success
    }
    else
        *(DWORD*)Result = 0; //amb: return failure

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execSetPropertyText );

void UObject::execSaveConfig( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execSaveConfig);
	P_FINISH;
	SaveConfig();
	unguard;
}
IMPLEMENT_FUNCTION( UObject, 536, execSaveConfig);

void UObject::execStaticSaveConfig( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execStaticSaveConfig);
	P_FINISH;
	Class->GetDefaultObject()->SaveConfig();
	unguard;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execStaticSaveConfig);

void UObject::execResetConfig( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execResetConfig);
	P_GET_STR_OPTX( PropName, TEXT("") );	// rjp
	P_FINISH;
	ResetConfig(GetClass(), *PropName);
	unguard;
}
IMPLEMENT_FUNCTION( UObject, 543, execResetConfig);

// rjp --
void UObject::execClearConfig( FFrame& Stack, RESULT_DECL)
{
	guard(UObject::execClearConfig);
	P_GET_STR_OPTX(PropName, TEXT(""));
	P_FINISH;
	ClearConfig(GetClass(), *PropName);
	unguard;
}
IMPLEMENT_FUNCTION( UObject, 537, execClearConfig);

void UObject::execStaticClearConfig( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execClearConfig);
	P_GET_STR_OPTX(PropName, TEXT(""));
	P_FINISH;

	Class->GetDefaultObject()->ClearConfig( Class, *PropName );
	unguard;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execStaticClearConfig );
// -- rjp

void UObject::execGetPerObjectNames( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execGetPerObjectNames)

	P_GET_STR(Filename);
	P_GET_STR_OPTX(BaseClass,Class->GetName());
	P_GET_INT_OPTX(MaxResults,1024);
	P_FINISH;

	TArray<FString>* Sections = (TArray<FString>*)Result;
	GConfig->GetSectionNames(*Sections, Filename != TEXT("") ? *Filename : NULL);

	//find and remove any sections that reference a package (and thus aren't data for a perobjectconfig object)
	INT pos = INDEX_NONE;
	FString Section, ObjClass;

	for ( INT i = Sections->Num() - 1; i >= 0; i-- )
	{
		Section = (*Sections)(i);
		pos = Section.InStr(TEXT("."));
		if ( pos != INDEX_NONE && GFileManager->FileSize( *(Section.Left(pos) + TEXT(".u")) ) > 0 )
		{
			Sections->Remove(i);
			continue;
		}

		if ( BaseClass != TEXT("") )
		{
			if ( !Section.Split(TEXT(" "), &(*Sections)(i), &ObjClass) )
			{
				Sections->Remove(i);
				continue;
			}

			if ( ObjClass != BaseClass )
				Sections->Remove(i);
		}
	}

	if ( Sections->Num() > MaxResults )
		Sections->Remove(MaxResults, Sections->Num() - MaxResults);

	unguard;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execGetPerObjectNames );

void UObject::execGetEnum( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execGetEnum);

	P_GET_OBJECT(UObject,E);
	P_GET_INT(i);
	P_FINISH;

	*(FName*)Result = NAME_None;
	if( Cast<UEnum>(E) && i>=0 && i<Cast<UEnum>(E)->Names.Num() )
		*(FName*)Result = Cast<UEnum>(E)->Names(i);

	unguard;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execGetEnum);

void UObject::execDynamicLoadObject( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execDynamicLoadObject);

	P_GET_STR(Name);
	P_GET_OBJECT(UClass,Class);
	P_GET_UBOOL_OPTX(bMayFail,0);
	P_FINISH;
	if ( !Class )
	{
		debugf(TEXT("WARNING - DynamicLoadObject() called with no object class!"));
		*(UObject**)Result = NULL;
	}
	else
		*(UObject**)Result = StaticLoadObject( Class, NULL, *Name, NULL, LOAD_NoWarn | (bMayFail?LOAD_Quiet:0), NULL );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execDynamicLoadObject );

void UObject::execFindObject( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execFindObject);

	P_GET_STR(Name);
	P_GET_OBJECT(UClass,Class);
	P_FINISH;

	*(UObject**)Result = StaticFindObject( Class, NULL, *Name );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execFindObject );

void UObject::execIsInState( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIsInState);

	P_GET_NAME(StateName);
	P_FINISH;

	if( StateFrame )
		for( UState* Test=StateFrame->StateNode; Test; Test=Test->GetSuperState() )
			if( Test->GetFName()==StateName )
				{*(DWORD*)Result=1; return;}
	*(DWORD*)Result = 0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 281, execIsInState );

void UObject::execGetStateName( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execGetStateName);
	P_FINISH;
	*(FName*)Result = (StateFrame && StateFrame->StateNode) ? StateFrame->StateNode->GetFName() : FName(NAME_None);
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 284, execGetStateName );

void UObject::execIsA( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execIsA);

	P_GET_NAME(ClassName);
	P_FINISH;

	UClass* TempClass;
	for( TempClass=GetClass(); TempClass; TempClass=TempClass->GetSuperClass() )
		if( TempClass->GetFName() == ClassName )
			break;
	*(DWORD*)Result = (TempClass!=NULL);

	unguardexecSlow;
}
IMPLEMENT_FUNCTION(UObject,303,execIsA);

/*-----------------------------------------------------------------------------
	Native iterator functions.
-----------------------------------------------------------------------------*/

void UObject::execIterator( FFrame& Stack, RESULT_DECL )
{}
IMPLEMENT_FUNCTION( UObject, EX_Iterator, execIterator );

// amb ---
void UObject::execAllObjects( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::execAllObjects);

	// Get the parms.
	P_GET_OBJECT(UClass,objClass);
	P_GET_OBJECT_REF(UObject,obj);
	P_FINISH;

	objClass = objClass ? objClass : UObject::StaticClass();
    TObjectIterator<UObject> It;

	PRE_ITERATOR;
		// Fetch next object in the iteration.
		*obj = NULL;
        while (It && *obj==NULL)
        {
            if (It->IsA(objClass) && !It->IsPendingKill())
            {
                *obj = *It;
            }
            ++It;
        }
		if( *obj == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 197, execAllObjects );
// --- amb

/*-----------------------------------------------------------------------------
    Platform detection functions for script code.
-----------------------------------------------------------------------------*/

// amb --- hacky hack

void UObject::execStopWatch( FFrame& Stack, RESULT_DECL )
{
    guardSlow(UObject::execStopWatch);
    P_GET_UBOOL_OPTX(bStop,0);
    P_FINISH;

    if (bStop)
    {
		unclock(StopWatchTimer);
        debugf(TEXT("Time=%lf ms"), (DOUBLE)(StopWatchTimer * GSecondsPerCycle * 1000.f));
        StopWatchTimer = 0.0;
    }
    else clock(StopWatchTimer);

    unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, 535, execStopWatch );
// --- amb

// gam ---
void UObject::execIsOnConsole( FFrame& Stack, RESULT_DECL )
{
    guardSlow(UObject::execIsOnConsole);
    P_FINISH;
    
    *(UBOOL*)Result = appIsOnConsole();
        
    unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execIsOnConsole );
// --- gam

void UObject::execPlatformIsMacOS( FFrame& Stack, RESULT_DECL )
{
    guardSlow(UObject::execPlatformIsMacOS);
    P_FINISH;
    
    #if MACOSX
    *(UBOOL*)Result = 1;
    #else
    *(UBOOL*)Result = 0;
    #endif

    unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execPlatformIsMacOS );

void UObject::execPlatformIsUnix( FFrame& Stack, RESULT_DECL )
{
    guardSlow(UObject::execPlatformIsUnix);
    P_FINISH;
    
    #if ((__UNIX__) && (!MACOSX))
    *(UBOOL*)Result = 1;
    #else
    *(UBOOL*)Result = 0;
    #endif

    unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execPlatformIsUnix );

void UObject::execPlatformIsWindows( FFrame& Stack, RESULT_DECL )
{
    guardSlow(UObject::execPlatformIsWindows);
    P_FINISH;
    
    #if ((defined _WIN32) || (defined _WIN64))
    *(UBOOL*)Result = 1;
    #else
    *(UBOOL*)Result = 0;
    #endif

    unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execPlatformIsWindows );

void UObject::execPlatformIs64Bit( FFrame& Stack, RESULT_DECL )
{
    guardSlow(UObject::execPlatformIs64Bit);
    P_FINISH;
    
    #if PLATFORM_64BITS
    *(UBOOL*)Result = 1;
    #else
    *(UBOOL*)Result = 0;
    #endif

    unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execPlatformIs64Bit );


// gam ---
void UObject::execIsSoaking( FFrame& Stack, RESULT_DECL )
{
    guardSlow(UObject::execIsSoaking);
    P_FINISH;

	*(UBOOL*)Result = GIsSoaking;
        
    unguardexecSlow;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execIsSoaking );
// --- gam

/*-----------------------------------------------------------------------------
	Native registry.
-----------------------------------------------------------------------------*/

//
// Register a native function.
// Warning: Called at startup time, before engine initialization.
//
BYTE CORE_API GRegisterNative( INT iNative, const Native& Func )
{
	static int Initialized = 0;
	if( !Initialized )
	{
		Initialized = 1;
		for( int i=0; i<ARRAY_COUNT(GNatives); i++ )
			GNatives[i] = &UObject::execUndefined;
	}
	if( iNative != INDEX_NONE )
	{
		if( iNative<0 || iNative>ARRAY_COUNT(GNatives) || GNatives[iNative]!=&UObject::execUndefined) 
			GNativeDuplicate = iNative;
		GNatives[iNative] = Func;
	}
	return 0;
}

BYTE CORE_API GRegisterCast( INT CastCode, const Native& Func )
{
	static int Initialized = 0;
	if( !Initialized )
	{
		Initialized = 1;
		for( int i=0; i<ARRAY_COUNT(GCasts); i++ )
			GCasts[i] = &UObject::execUndefined;
	}
	if( CastCode != INDEX_NONE )
	{
		if( CastCode<0 || CastCode>ARRAY_COUNT(GCasts) || GCasts[CastCode]!=&UObject::execUndefined) 
			GCastDuplicate = CastCode;
		GCasts[CastCode] = Func;
	}
	return 0;
}

/*-----------------------------------------------------------------------------
	Script processing function.
-----------------------------------------------------------------------------*/

//
// Information remembered about an Out parameter.
//
struct FOutParmRec
{
	UProperty* Property;
	BYTE*      PropAddr;
};

/**
 * Helper class for call graph profiling. Scoped to correctly deal with multiple return
 * paths.
 */
struct FScopedScriptStats
{
	/** 
	 * Constructor, emiting function pointer to stream.
	 *
	 * @param InFunction	Function about to be called.
	 */
	FScopedScriptStats( UFunction* InFunction )
	{
		if( GScriptCallGraph )
		{
			StartCycles = appCycles();
			Function	= InFunction;
			GScriptCallGraph->EmitFunction( Function );
		}
	}

	/**
	 * Destructor, emiting cycles spent in function call.	
	 */
	~FScopedScriptStats()
	{
		if( GScriptCallGraph )
		{
			DWORD Cycles = appCycles() - StartCycles;
			GScriptCallGraph->EmitCycles( Cycles );
		}
	}

private:
	/** Function associated with callss */
	UFunction*	Function;
	/** Cycle count before function was being called. */
	DWORD		StartCycles;
};

//
// Call a function.
//
void UObject::CallFunction( FFrame& Stack, RESULT_DECL, UFunction* Function )
{
	guardSlow(UObject::CallFunction);
	FScopedScriptStats ScriptStats( Function );
	// Found it.
	UBOOL SkipIt = 0;
	if( Function->iNative )
	{
		// Call native final function.
		(this->*Function->Func)( Stack, Result );
	}
	else if( Function->FunctionFlags & FUNC_Native )
	{
		// Call native networkable function.
		BYTE Buffer[1024];
		if( !ProcessRemoteFunction( Function, Buffer, &Stack ) )
		{
			// Call regular native function.
			(this->*Function->Func)( Stack, Result );
		}
		else
		{
			// Eat up the remaining parameters in the stream.
			SkipIt = 1;
			goto Temporary;
		}
	}
	else
	{
		// Make new stack frame in the current context.
		Temporary:

        // gam ---
        if( Function->PropertiesSize == 0 ) // No parameters or locals 
        {
            FFrame NewStack( this, Function, 0, NULL );

            checkSlow (*Stack.Code==EX_EndFunctionParms);
            Stack.Code++;

			// DEBUGGER: Grab the optional EFP debug info
			if ( *Stack.Code == EX_DebugInfo )
				Stack.Step( Stack.Object, NULL );

		// Execute the code.
            if( !SkipIt )
                ProcessInternal( NewStack, Result );
        }
        else // Parameters and/or locals
        {
		BYTE* Frame = (BYTE*)appAlloca(Function->PropertiesSize);
		appMemzero( Frame, Function->PropertiesSize );
		FFrame NewStack( this, Function, 0, Frame );
		FOutParmRec Outs[MAX_FUNC_PARMS], *Out = Outs;
		for( UProperty* Property=(UProperty*)Function->Children; *Stack.Code!=EX_EndFunctionParms; Property=(UProperty*)Property->Next )
		{
			GPropAddr = NULL;
			GPropObject = NULL;

			BYTE* Param = NewStack.Locals + Property->Offset;
			Stack.Step( Stack.Object, Param );
#if !__INTEL_BYTE_ORDER__
			if( Property->GetID()==NAME_BoolProperty  )
 			{
				if( *(DWORD*)(Param) )
					*(DWORD*)(Param) = FIRST_BITFIELD;
				else
					*(DWORD*)(Param) = 0;
			}
#endif
			if( (Property->PropertyFlags & CPF_OutParm) && GPropAddr )
			{
				Out->PropAddr = GPropAddr;
				Out->Property = Property;
				Out++;
				if ( GPropObject && GProperty && (GProperty->PropertyFlags & CPF_Net) )
					GPropObject->NetDirty(GProperty);
			}
		}
		Stack.Code++;

		//DEBUGGER
		// This is necessary until I find a better solution, otherwise, 
		// when the DebugInfo gets read out of the bytecode, it'll be called 
		// AFTER the function returns, which is not useful. This is one of the
		// few places I check for debug information.
		if ( *Stack.Code == EX_DebugInfo )
			Stack.Step( Stack.Object, NULL );

		// Execute the code.
		if( !SkipIt )
			ProcessInternal( NewStack, Result );

		// Copy back outparms.
		while( --Out >= Outs )
			Out->Property->CopyCompleteValue( Out->PropAddr, NewStack.Locals + Out->Property->Offset );

		// Destruct properties on the stack.
		for( UProperty* Destruct=Function->ConstructorLink; Destruct; Destruct=Destruct->ConstructorLinkNext )
			Destruct->DestroyValue( NewStack.Locals + Destruct->Offset );
	}
        // --- gam
    }

	unguardSlow;
}

//
// Internal function call processing.
//!!might not write anything to Result if singular or proper type isn't returned.
//
void UObject::ProcessInternal( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UObject::ProcessInternal);

	DWORD SingularFlag = ((UFunction*)Stack.Node)->FunctionFlags & FUNC_Singular;
	if
	(	!ProcessRemoteFunction( (UFunction*)Stack.Node, Stack.Locals, NULL )
	&&	IsProbing( Stack.Node->GetFName() )
	&&	!(ObjectFlags & SingularFlag) )
	{
		ObjectFlags |= SingularFlag;
		BYTE Buffer[1024];//!!hardcoded size
		appMemzero( Buffer, sizeof(FString) );//!!
#if DO_GUARD
		if( ++Recurse > RECURSE_LIMIT )
		{
			if ( GDebugger && GDebugger->NotifyInfiniteLoop() )
				Recurse = 0;
			else
			Stack.Logf( NAME_Critical, TEXT("Infinite script recursion (%i calls) detected"), RECURSE_LIMIT );
		}
#endif
		while( *Stack.Code != EX_Return )
			Stack.Step( Stack.Object, Buffer );
		Stack.Code++;
		Stack.Step( Stack.Object, Result );

		//DEBUGGER: Necessary for the call stack. Grab an optional 'PREVSTACK' debug info.
		if ( *Stack.Code == EX_DebugInfo )
			Stack.Step( Stack.Object, Result );
		
		ObjectFlags &= ~SingularFlag;
#if DO_GUARD
		--Recurse;
#endif
	}
	unguardSlow;
}

//
// Script processing functions.
//
void UObject::ProcessEvent( UFunction* Function, void* Parms, void* UnusedResult )
{
	guard(UObject::ProcessEvent);

	// Reject.
	if
	(	!GIsScriptable
	||	!IsProbing( Function->GetFName() )
	||	IsPendingKill()
	||	Function->iNative
	||	((Function->FunctionFlags & FUNC_Native) && ProcessRemoteFunction( Function, Parms, NULL )) )
		return;
	checkSlow(Function->ParmsSize==0 || Parms!=NULL);

	FScopedScriptStats ScriptStats( Function );

	// Start timer.
	// SCRIPTTIME FLOAT StartTime = GScriptCycles;
	if( ++GScriptEntryTag == 1 )
		clock(GScriptCycles);

	// Create a new local execution stack.
	FFrame NewStack( this, Function, 0, appAlloca(Function->PropertiesSize) );
	appMemcpy( NewStack.Locals, Parms, Function->ParmsSize );
	appMemzero( NewStack.Locals+Function->ParmsSize, Function->PropertiesSize-Function->ParmsSize );

	// Call native function or UObject::ProcessInternal.
	(this->*Function->Func)( NewStack, NewStack.Locals+Function->ReturnValueOffset );

	// Copy everything back.
	appMemcpy( Parms, NewStack.Locals, Function->ParmsSize );

	// Destroy local variables except function parameters.!! see also UObject::ScriptConsoleExec
	for( UProperty* P=Function->ConstructorLink; P; P=P->ConstructorLinkNext )
		if( P->Offset >= Function->ParmsSize )
			P->DestroyValue( NewStack.Locals + P->Offset );

	// Stop timer.
	if( --GScriptEntryTag == 0 )
	{
		unclock(GScriptCycles);
	}
	unguardf(( TEXT("(%s, %s)"), GetFullName(), Function->GetFullName() ));
}

void UObject::ProcessDelegate( FName DelegateName, FScriptDelegate* Delegate, void* Parms, void* UnusedResult )
{
	guard(UObject::ProcessDelegate);
	if( Delegate->Object && Delegate->Object->IsPendingKill() )
	{
		Delegate->Object = NULL;
		Delegate->FunctionName = NAME_None;
	}
	if( Delegate->Object )
		Delegate->Object->ProcessEvent( Delegate->Object->FindFunctionChecked(Delegate->FunctionName), Parms, UnusedResult );
	else
		ProcessEvent( FindFunctionChecked(DelegateName), Parms, UnusedResult );
	unguard;
}

//
// Execute the state code of the object.
//
void UObject::ProcessState( FLOAT DeltaSeconds )
{}

//
// Process a remote function; returns 1 if remote, 0 if local.
//
UBOOL UObject::ProcessRemoteFunction( UFunction* Function, void* Parms, FFrame* Stack )
{
	return 0;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

