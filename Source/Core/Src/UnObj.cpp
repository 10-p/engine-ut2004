/*=============================================================================
	UnObj.cpp: Unreal object manager.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
		* SaveConfig, LoadConfig, ResetConfig modified / ClearConfig added by Ron Prestenback
=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

// Object manager internal variables.
UBOOL						UObject::GObjInitialized		= 0;
UBOOL						UObject::GObjNoRegister			= 0;
INT							UObject::GObjBeginLoadCount		= 0;
UBOOL						UObject::GObjPreloading			= 0;
INT							UObject::GObjRegisterCount		= 0;
INT							UObject::GImportCount			= 0;
UObject*					UObject::GAutoRegister			= NULL;
UPackage*					UObject::GObjTransientPkg		= NULL;
TCHAR						UObject::GObjCachedLanguage[32] = TEXT("");
TCHAR						UObject::GLanguage[64]          = TEXT("int");
UObject*					UObject::GObjHash[4096];
TArray<UObject*>			UObject::GObjLoaded;
TArray<UObject*>			UObject::GObjObjects;
TArray<INT>					UObject::GObjAvailable;
TArray<UObject*>			UObject::GObjLoaders;
TArray<UObject*>			UObject::GObjRoot;
TArray<UObject*>			UObject::GObjRegistrants;
TArray<FPreferencesInfo>	UObject::GObjPreferences;
TArray<FRegistryObjectInfo> UObject::GObjDrivers;
TMultiMap<FName,FName>*		UObject::GObjPackageRemap;
static INT GGarbageRefCount=0;

/*-----------------------------------------------------------------------------
	UObject constructors.
-----------------------------------------------------------------------------*/

UObject::UObject()
{}
UObject::UObject( const UObject& Src )
{
	guard(UObject::UObject);
	check(&Src);
	if( Src.GetClass()!=GetClass() )
		appErrorf( TEXT("Attempt to copy-construct %s from %s"), GetFullName(), Src.GetFullName() );
	unguard;
}
UObject::UObject( EInPlaceConstructor, UClass* InClass, UObject* InOuter, FName InName, DWORD InFlags )
{
	guard(UObject::UObject);
	StaticAllocateObject( InClass, InOuter, InName, InFlags, NULL, GError, this );
	unguard;
}
UObject::UObject( ENativeConstructor, UClass* InClass, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags )
:	Index		( INDEX_NONE				)
,	HashNext	( NULL						)
,	StateFrame	( NULL						)
,	_Linker		( NULL						)
,	ObjectFlags	( InFlags | RF_Native	    )
,	Class		( InClass					)
,	LastIsA		( NULL						)
,	_LinkerIndex( INDEX_NONE				)
,	Outer       ( NULL						)
,	Name        ( NAME_None					)
{
	guard(UObject::UObject);

	// Make sure registration is allowed now.
	check(!GObjNoRegister);

	// Setup registration info, for processing now (if inited) or later (if starting up).
	check(sizeof(Outer       )>=sizeof(InPackageName));

	check(sizeof(_LinkerIndex)>=sizeof(GAutoRegister));
	*(const TCHAR  **)&Outer        = InPackageName;

#if SERIAL_POINTER_INDEX
	// FName is 4 bytes and the stashed name a pointer: it waits in StateFrame (pointer-sized, NULL until
	// Register(), which clears it again).
	*(const TCHAR  **)&StateFrame   = InName;
#else
	check(sizeof(Name        )>=sizeof(InName       ));
	*(const TCHAR  **)&Name         = InName;
#endif

	*(UObject      **)&_LinkerIndex = GAutoRegister;
	GAutoRegister                   = this;

	// Call native registration from terminal constructor.
	if( GetInitialized() && GetClass()==StaticClass() )
		Register();

	unguard;
}
UObject::UObject( EStaticConstructor, const TCHAR* InName, const TCHAR* InPackageName, DWORD InFlags )
:	Index		( INDEX_NONE				)
,	HashNext	( NULL						)
,	StateFrame	( NULL						)
,	_Linker		( NULL						)
,	ObjectFlags	( InFlags | RF_Native	    )
,	_LinkerIndex( INDEX_NONE				)
,	Outer       ( NULL						)
,	Name        ( NAME_None					)
{
	guard(UObject::UObject);

	// Setup registration info, for processing now (if inited) or later (if starting up).
	check(sizeof(Outer       )>=sizeof(InPackageName));
	check(sizeof(_LinkerIndex)>=sizeof(GAutoRegister));
	*(const TCHAR  **)&Outer        = InPackageName;

#if SERIAL_POINTER_INDEX
	// See the native constructor above.
	*(const TCHAR  **)&StateFrame   = InName;
#else
	check(sizeof(Name        )>=sizeof(InName       ));
	*(const TCHAR  **)&Name         = InName;
#endif
	
	// If we are not initialized yet, auto register.
	if (!GObjInitialized)
	{
		*(UObject      **)&_LinkerIndex = GAutoRegister;
		GAutoRegister                   = this;
	}
	unguard;
}
UObject& UObject::operator=( const UObject& Src )
{
	guard(UObject::operator=);
	check(&Src);
	if( Src.GetClass()!=GetClass() )
		appErrorf( TEXT("Attempt to assign %s from %s"), GetFullName(), Src.GetFullName() );
	return *this;
	unguardobj;
}

/*-----------------------------------------------------------------------------
	UObject class initializer.
-----------------------------------------------------------------------------*/

void UObject::StaticConstructor()
{
	guard(UObject::StaticConstructor);
	unguard;
}

/*-----------------------------------------------------------------------------
	UObject implementation.
-----------------------------------------------------------------------------*/

//
// Rename this object to a unique name.
//
void UObject::Rename( const TCHAR* InName, UObject* NewOuter )
{
	guard(UObject::Rename);

	FName NewName = InName ? FName(InName) : MakeUniqueObjectName( NewOuter ? NewOuter : GetOuter(), GetClass() );
	UnhashObject( Outer ? Outer->GetIndex() : 0 );
	debugfSlow( TEXT("Renaming %s to %s"), *Name, *NewName );
	Name = NewName;
	if( NewOuter )
		Outer = NewOuter;
	HashObject();

	unguardobj;
}

//
// Shutdown after a critical error.
//
void UObject::ShutdownAfterError()
{
	guard(UObject::ShutdownAfterError);
	unguard;
}

//
// Make sure the object is valid.
//
UBOOL UObject::IsValid()
{
	guard(UObject::IsValid);
	if( !this )
	{
		debugf( NAME_Warning, TEXT("NULL object") );
		return 0;
	}
	else if( !GObjObjects.IsValidIndex(GetIndex()) )
	{
		debugf( NAME_Warning, TEXT("Invalid object index %i"), GetIndex() );
		debugf( NAME_Warning, TEXT("This is: %s"), GetFullName() );
		return 0;
	}
	else if( GObjObjects(GetIndex())==NULL )
	{
		debugf( NAME_Warning, TEXT("Empty slot") );
		debugf( NAME_Warning, TEXT("This is: %s"), GetFullName() );
		return 0;
	}
	else if( GObjObjects(GetIndex())!=this )
	{
		debugf( NAME_Warning, TEXT("Other object in slot") );
		debugf( NAME_Warning, TEXT("Other is: %s"), GObjObjects(GetIndex())->GetFullName() );
		debugf( NAME_Warning, TEXT("This is: %s"), GetFullName() );
//		debugf( NAME_Warning, TEXT("Other is: %s"), GObjObjects(GetIndex())->GetFullName() );
		return 0;
	}
	else return 1;
	unguardobj;
}

//
// Do any object-specific cleanup required
// immediately after loading an object, and immediately
// after any undo/redo.
//
void UObject::PostLoad()
{
	guard(UObject::PostLoad);

	// Note that it has propagated.
	SetFlags( RF_DebugPostLoad );

	// Load per-object localization
	if( ObjectFlags&RF_PerObjectLocalized && !(GUglyHackFlags&64) )
		LoadLocalized();

	// HACK for bad subobjects in the editor.
	if( !GIsEditor && !Outer->IsA(UClass::StaticClass()) )
	{
		BYTE* Defaults			= &Class->Defaults(0);
		INT DefaultsCount		=  Class->Defaults.Num();
		for( UProperty* P=Class->ConstructorLink; P; P=P->ConstructorLinkNext )
		{
			if( P->Offset < DefaultsCount )
			{
				if( P->GetID()==NAME_ObjectProperty )
				{
					if( !appMemcmp((BYTE*)this + P->Offset, Defaults + P->Offset, P->GetSize()) )
					{
						// Check if any of the array items are non-NULL
						INT i;
						for( i=0;i<P->ArrayDim;i++ )
							if( ((UObject**)((BYTE*)this+P->Offset))[i] )
								break;

						if( i<P->ArrayDim )
						{
							// Instance.
							P->CopyCompleteValue( (BYTE*)this + P->Offset, Defaults + P->Offset, this );
						}
					}
				}
				else
				if( P->GetID()==NAME_ArrayProperty )
				{
					UArrayProperty* ArrProp = (UArrayProperty*)P;
					if( ArrProp->Inner->GetID()==NAME_ObjectProperty && (ArrProp->Inner->PropertyFlags&CPF_NeedCtorLink) )
					{
						FArray* Arr = (FArray*)((BYTE*)this+P->Offset);
						FArray* DefArr = (FArray*)(Defaults+P->Offset);
						for( INT i=0;i<Arr->Num()&&i<DefArr->Num();i++ )
						{
							UObject** Obj = &((UObject**)Arr->GetData())[i];
							UObject** DefObj = &((UObject**)DefArr->GetData())[i];

							if( *Obj && *Obj==*DefObj )
							{
								// Instance.
								ArrProp->Inner->CopyCompleteValue( Obj, DefObj, this );
							}						
						}
					}
				}
			}
		}
	}

	unguardobj;
}

//
// Edit change notification.
//
void UObject::PostEditChange()
{
	guard(UObject::PostEditChange);
	Modify();
	unguardobj;
}

//
// Do any object-specific cleanup required immediately
// before an object is killed.  Child classes may override
// this if they have to do anything here.
//
void UObject::Destroy()
{
	guard(UObject::Destroy);
	SetFlags( RF_DebugDestroy );

	// Destroy properties.
	ExitProperties( (BYTE*)this, GetClass() );

	// Log message.
	if( GObjInitialized && !GIsCriticalError )
		debugfSlow( NAME_DevKill, TEXT("Destroying %s"), GetFullName() );

	// Unhook from linker.
	SetLinker( NULL, INDEX_NONE );

	// Remember outer name index.
	_LinkerIndex = Outer ? Outer->GetIndex() : 0;

	unguardobj;
}

//
// Set the object's linker.
//
void UObject::SetLinker( ULinkerLoad* InLinker, INT InLinkerIndex )
{
	guard(UObject::SetLinker);

	// Detach from existing linker.
	if( _Linker )
	{
		check(_Linker->ExportMap(_LinkerIndex)._Object!=NULL);
		check(_Linker->ExportMap(_LinkerIndex)._Object==this);
		_Linker->ExportMap(_LinkerIndex)._Object = NULL;
	}

	// Set new linker.
	_Linker      = InLinker;
	_LinkerIndex = InLinkerIndex;

	unguardobj;
}

//
// Return the object's path name.
//warning: Must be safe for NULL objects.
//warning: Must be safe for class-default meta-objects.
//
const TCHAR* UObject::GetPathName( UObject* StopOuter, TCHAR* Str ) const
{
	guard(UObject::GetPathName);

	// Return one of 8 circular results.
	TCHAR* Result = Str ? Str : appStaticString1024();
	if( this!=StopOuter )
	{
		*Result = 0;
		if( Outer!=StopOuter )
		{
			Outer->GetPathName( StopOuter, Result );
			appStrcat( Result, TEXT(".") );
		}
		appStrcat( Result, GetName() );
	}
	else appSprintf( Result, TEXT("None") );

	return Result;
	unguard; // Not unguardobj, because it causes crash recusion.
}

//
// Return the object's full name.
//warning: Must be safe for NULL objects.
//
const TCHAR* UObject::GetFullName( TCHAR* Str ) const
{
	guard(UObject::GetFullName);

	// Return one of 8 circular results.
	TCHAR* Result = Str ? Str : appStaticString1024();
	if( this )
	{
		appSprintf( Result, TEXT("%s "), GetClass()->GetName() );
		GetPathName( NULL, Result + appStrlen( Result ) );
	}
	else
	{
		appStrcpy( Result, TEXT("None") );
	}

	return Result;
	unguard; // Not unguardobj, because it causes crash recusion.
}

//
// Destroy the object if necessary.
//
UBOOL UObject::ConditionalDestroy()
{
	guard(UObject::ConditionalDestroy);
	if( Index!=INDEX_NONE && !(GetFlags() & RF_Destroyed) )
	{
		SetFlags( RF_Destroyed );
		ClearFlags( RF_DebugDestroy );
		Destroy();
		if( !(GetFlags()&RF_DebugDestroy) )
			appErrorf( TEXT("%s failed to route Destroy"), GetFullName() );
		return 1;
	}
	else return 0;
	unguardobj;
}

//
// Register if needed.
//
void UObject::ConditionalRegister()
{
	guard(UObject::ConditionalRegister);
	if( GetIndex()==INDEX_NONE )
	{
		// Verify this object is on the list to register.
		INT i;
		for( i=0; i<GObjRegistrants.Num(); i++ )
			if( GObjRegistrants(i)==this )
				break;
		check(i!=GObjRegistrants.Num());

		// Register it.
		Register();
	}
	unguardobj;
}

//
// PostLoad if needed.
//
void UObject::ConditionalPostLoad()
{
	guard(UObject::ConditionalPostLoad);
	if( GetFlags() & RF_NeedPostLoad )
	{
		ClearFlags( RF_NeedPostLoad | RF_DebugPostLoad );
		PostLoad();
		if( !(GetFlags() & RF_DebugPostLoad) )
 			appErrorf( TEXT("%s failed to route PostLoad"), GetFullName() );
	}
	unguardobj;
}

//
// UObject destructor.
//warning: Called at shutdown.
//
UObject::~UObject()
{
	guard(UObject::~UObject);

	// If not initialized, skip out.
	if( Index!=INDEX_NONE && GObjInitialized && !GIsCriticalError )
	{
		// Validate it.
		check(IsValid());

		// Destroy the object if necessary.
		ConditionalDestroy();

		// Remove object from table.
		UnhashObject( _LinkerIndex );
		GObjObjects(Index) = NULL;
		GObjAvailable.AddItem( Index );
	}

	// Free execution stack.
	if( StateFrame )
		delete StateFrame;

	unguard;
}

//
// Archive for persistent CRC's.
//
class FArchiveCRC : public FArchive
{
public:
	FArchiveCRC( UObject* Src, UObject* InBase )
	: CRC( 0 )
	, Base( InBase )
	{
		ArIsSaving = ArIsPersistent = 1;
		Src->Serialize( *this );
	}
	SIZE_T GetCRC()
	{
		return CRC;
	}
	void Serialize( void* Data, INT Num )
	{
		CRC = appMemCrc( Data, Num, CRC );
	}
	void SerializeCapsString( const TCHAR* Str )
	{
		TCHAR TCh;
		while( *Str )
		{
			TCh = appToUpper(*Str++);
			ByteOrderSerialize( &TCh, sizeof(TCh) );
		}
		TCh = 0;
		ByteOrderSerialize( &TCh, sizeof(TCh) );
	}
	virtual FArchive& operator<<( class FName& N )
	{
		SerializeCapsString( *N );
		return *this;
	}
	virtual FArchive& operator<<( class UObject*& Res )
	{
		if( Res )
		{
			SerializeCapsString( Res->GetClass()->GetName() );
			SerializeCapsString( Res->GetPathName(Res->IsIn(Base) ? Base : NULL) );
		}
		else SerializeCapsString( TEXT("None") );
		return *this;
	}
protected:
	DWORD CRC;
	UObject* Base;
};

//
// Note that the object has been modified.
//
void UObject::Modify()
{
	guard(UObject::Modify);

	// Perform transaction tracking.
	if( GUndo && (GetFlags() & RF_Transactional) )
		GUndo->SaveObject( this );

	unguardobj;
}

//
// UObject serializer.
//
void UObject::Serialize( FArchive& Ar )
{
	guard(UObject::Serialize);
	SetFlags( RF_DebugSerialize );

	// Make sure this object's class's data is loaded.
	if( Class != UClass::StaticClass() )
		Ar.Preload( Class );

	// Special info.
	if( (!Ar.IsLoading() && !Ar.IsSaving()) )
		Ar << Name << Outer << Class;
	if( !Ar.IsLoading() && !Ar.IsSaving() )
		Ar << _Linker;

	// Execution stack.
	//!!how does the stack work in conjunction with transaction tracking?
	guard(SerializeStack);
	if( !Ar.IsTrans() )
	{
		if( GetFlags() & RF_HasStack )
		{
			if( !StateFrame )
				StateFrame = new(TEXT("ObjectStateFrame")) FStateFrame( this );
			Ar << StateFrame->Node << StateFrame->StateNode;
			Ar << StateFrame->ProbeMask;
			Ar << StateFrame->LatentAction;
			if( StateFrame->Node )
			{
				Ar.Preload( StateFrame->Node );
				if( Ar.IsSaving() && StateFrame->Code )
					check(StateFrame->Code>=&StateFrame->Node->Script(0) && StateFrame->Code<&StateFrame->Node->Script(StateFrame->Node->Script.Num()));
				INT Offset = StateFrame->Code ? StateFrame->Code - &StateFrame->Node->Script(0) : INDEX_NONE;
				Ar << AR_INDEX(Offset);
				if( Offset!=INDEX_NONE )
					if( Offset<0 || Offset>=StateFrame->Node->Script.Num() )
						appErrorf( TEXT("%s: Offset mismatch: %i %i"), GetFullName(), Offset, StateFrame->Node->Script.Num() );
				StateFrame->Code = Offset!=INDEX_NONE ? &StateFrame->Node->Script(Offset) : NULL;
			}
			else StateFrame->Code = NULL;
		}
		else if( StateFrame )
		{
			delete StateFrame;
			StateFrame = NULL;
		}
	}
	unguard;

	// Serialize object properties which are defined in the class.
	if( Class != UClass::StaticClass() )
	{
		if( (Ar.IsLoading() || Ar.IsSaving()) && !Ar.IsTrans() )
			GetClass()->SerializeTaggedProperties( Ar, (BYTE*)this, Class );
		else
			GetClass()->SerializeBin( Ar, (BYTE*)this, 0 );
	}

	// Memory counting.
	SIZE_T Size = GetClass()->GetPropertiesSize();
	Ar.CountBytes( Size, Size );
	unguardobj;
}

//
// Export text object properties.
//
void UObject::ExportProperties
(
	FOutputDevice&	Out,
	UClass*			ObjectClass,
	BYTE*			Object,
	INT				Indent,
	UClass*			DiffClass,
	BYTE*			Diff
)
{
	const TCHAR* ThisName = TEXT("(none)");
	TCHAR DelegateName[NAME_SIZE];

	guard(UObject::ExportProperties);
	check(ObjectClass!=NULL);
	UBOOL ExportObject = 0;
	UClass *ArrPropClass = UArrayProperty::StaticClass(), 
		   *ObjPropClass = UObjectProperty::StaticClass(), 
		   *DelPropClass = UDelegateProperty::StaticClass();

	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(ObjectClass); It; ++It )
	{
		if( It->Port() )
		{
			ThisName = It->GetName();			
			for( INT j=0; j<It->ArrayDim; j++ )
			{
				TCHAR Value[4096];
				if( It->IsA(ArrPropClass) ) 
				{
					// Export dynamic array.
					UProperty* InnerProp = Cast<UArrayProperty>(*It)->Inner;
					ExportObject = (It->PropertyFlags & CPF_ExportObject && InnerProp->IsA(ObjPropClass));
											
					FArray* Arr = (FArray*)((BYTE*)Object + It->Offset + j*It->ElementSize);
					FArray* DiffArr = NULL;

					if (DiffClass && Diff && DiffClass->IsChildOf(It.GetStruct()))
						DiffArr = (FArray*)((BYTE*)Diff + It->Offset + j * It->ElementSize);

					// TODO What if DiffArr->Num() >= Arr->Num()?  Won't catch the rest of the elements, in that case
					for( INT k=0;k<Arr->Num();k++ )
					{
						if( InnerProp->ExportText( k, Value, (BYTE*)Arr->GetData(), DiffArr && k < DiffArr->Num() ? (BYTE*)DiffArr->GetData() : NULL, PPF_Delimited ) )
						{					
							if( ExportObject )
							{
								UObject* Obj = ((UObject**)Arr->GetData())[k];
								if( Obj && !(Obj->GetFlags() & RF_TagImp) )
								{
									// Don't export more than once.
									Obj->SetFlags( RF_TagImp );
									UExporter::ExportToOutputDevice( Obj, NULL, Out, TEXT("T3D"), Indent+1 );
								}
							}
		
							Out.Logf( TEXT("%s%s(%i)=%s\r\n%s"), Indent >= 0 ? appSpc(Indent + 1) : TEXT(""), ThisName, k, Value, ExportObject ? TEXT("\r\n") : TEXT("") );
						}
					}
				}
				else
				{
					// Export single element.
					if( It->ExportText( j, Value, Object, (DiffClass && DiffClass->IsChildOf(It.GetStruct())) ? Diff : NULL, PPF_Delimited|PPF_Localized ) )
					{
						ExportObject = (It->PropertyFlags & CPF_ExportObject && It->IsA(ObjPropClass));
						if ( ExportObject )
						{
							UObject* Obj = *(UObject **)((BYTE*)Object + It->Offset + j*It->ElementSize);					
							if( Obj && !(Obj->GetFlags() & RF_TagImp) )
							{
								// Don't export more than once.
								Obj->SetFlags( RF_TagImp );
								UExporter::ExportToOutputDevice( Obj, NULL, Out, TEXT("T3D"), Indent+1 );
							}
						}					

				// GetName() is something like __DelegateName__Delegate
	 					if ( It->IsA(DelPropClass) )
						{
							appStrncpy(DelegateName,ThisName,NAME_SIZE);
							ThisName = DelegateName;
							while ( *ThisName && *ThisName == '_' )
								ThisName++;
							*appStrchr(ThisName,'_')=0;
						}
						if( It->ArrayDim == 1 )
							Out.Logf( TEXT("%s%s=%s\r\n%s"), Indent >= 0 ? appSpc(Indent + 1) : TEXT(""), ThisName, Value, ExportObject ? TEXT("\r\n") : TEXT("") );
						else
							Out.Logf( TEXT("%s%s(%i)=%s\r\n%s"), Indent >= 0 ? appSpc(Indent + 1) : TEXT(""), ThisName, j, Value, ExportObject ? TEXT("\r\n") : TEXT("") );
					}
				}
			}
		}
	}
	unguardf(( TEXT("(%s)"), ThisName ));
}

//
// Initialize script execution.
//
void UObject::InitExecution()
{
	guard(UObject::InitExecution);
	check(GetClass()!=NULL);

	if( StateFrame )
		delete StateFrame;
	StateFrame = new(TEXT("ObjectStateFrame"))FStateFrame( this );
	SetFlags( RF_HasStack );

	unguardobj;
}

//
// Command line.
//
UBOOL UObject::ScriptConsoleExec( const TCHAR* Str, FOutputDevice& Ar, UObject* Executor )
{
	guard(UObject::ScriptConsoleExec);

	// No script execution?
	if( !GIsScriptable )
		return 0;

	// Find UnrealScript exec function.
	TCHAR MsgStr[NAME_SIZE];
	FName Message;
	UFunction* Function;
	if
	(	!ParseToken(Str,MsgStr,ARRAY_COUNT(MsgStr),1)
	||	(Message=FName(MsgStr,FNAME_Find))==NAME_None 
	||	(Function=FindFunction(Message))==NULL 
	||	!(Function->FunctionFlags & FUNC_Exec) )
		return 0;

	// Parse all function parameters.
	BYTE* Parms = (BYTE*)appAlloca(Function->ParmsSize);
	appMemzero( Parms, Function->ParmsSize );
	UBOOL Failed = 0;
	// MC: Pre Count the Properties (Have something faster or cached ?)
	INT FldCount = -1;
	for( TFieldIterator<UProperty> ItCnt(Function); ItCnt && (ItCnt->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++ItCnt )
		FldCount++;

	INT Count = 0;
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It,Count++ )
	{
		if( Count==0 && Executor )
		{
			UObjectProperty* Op = Cast<UObjectProperty>(*It);
			if( Op && Executor->IsA(Op->PropertyClass) )
			{
				// First parameter is implicit reference to object executing the command.
				*(UObject**)(Parms + It->Offset) = Executor;
				continue;
			}
		}
		ParseNext( &Str );
		// MC: Prevent parsing full string if not last parameter
		Str = It->ImportText( Str, Parms+It->Offset, PPF_Localized | (Count==FldCount?0:PPF_Delimited) );
		if( !Str )
		{
			if( !(It->PropertyFlags & CPF_OptionalParm) )
			{
				Ar.Logf( LocalizeError(TEXT("BadProperty"),TEXT("Core")), *Message, It->GetName() );
				Failed = 1;
			}
			break;
		}
	}
	if( !Failed )
		ProcessEvent( Function, Parms );
	//!!destructframe see also UObject::ProcessEvent
	{for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
		It->DestroyValue( Parms + It->Offset );}

	// Success.
	return 1;
	unguardobj;
}

//
// Find an UnrealScript field.
//warning: Must be safe with class default metaobjects.
//
UField* UObject::FindObjectField( FName InName, UBOOL Global )
{
	guardSlow(UObject::StaticFindField);
#if 1 /* Fast, VF-hashed version */
	INT iHash = InName.GetIndex() & (UField::HASH_COUNT-1);

	// Search current state scope.
	if( StateFrame && StateFrame->StateNode && !Global )
		for( UField* Node=StateFrame->StateNode->VfHash[iHash]; Node; Node=Node->HashNext )
			if( Node->GetFName()==InName )
				return Node;

	// Search the global scope.
	for( UField* Node=GetClass()->VfHash[iHash]; Node; Node=Node->HashNext )
		if( Node->GetFName()==InName )
			return Node;

	// Not found.
	return NULL;
#else /* Slow, unhashed version */
	// Search current state scope.
	if( StateFrame && StateFrame->StateNode && !Global )
		for( TFieldIterator<UField> It(StateFrame->StateNode); It; ++It )
			if( It->GetFName()==InName )
				return *It;

	// Search the global scope.
	for( TFieldIterator<UField> It(GetClass()); It; ++It )
		if( It->GetFName()==InName )
			return *It;

	// Not found.
	return NULL;
#endif
	unguardfSlow(( TEXT("%s (node %s)"), GetFullName(), *InName ));
}
UFunction* UObject::FindFunction( FName InName, UBOOL Global )
{
	guardSlow(UObject::FindFunction);
    return FlagCast<UFunction,CLASS_IsAUFunction>( FindObjectField( InName, Global ) );
	unguardfSlow(( TEXT("%s (function %s)"), GetFullName(), *InName ));
}
UFunction* UObject::FindFunctionChecked( FName InName, UBOOL Global )
{
	guardSlow(UObject::FindFunctionChecked);
	if( !GIsScriptable )
		return NULL;
    UFunction* Result = FlagCast<UFunction,CLASS_IsAUFunction>( FindObjectField( InName, Global ) );
	if( !Result )
		appErrorf( TEXT("Failed to find function %s in %s"), *InName, GetFullName() );
	return Result;
	unguardfSlow(( TEXT("%s (function %s)"), GetFullName(), *InName ));
}
UState* UObject::FindState( FName InName )
{
	guardSlow(UObject::FindState);
    return FlagCast<UState,CLASS_IsAUState>( FindObjectField( InName, 1 ) );
	unguardfSlow(( TEXT("%s (state %s)"), GetFullName(), *InName ));
}
IMPLEMENT_CLASS(UObject);

/*-----------------------------------------------------------------------------
	UObject configuration.
-----------------------------------------------------------------------------*/

//
// Load configuration.
//warning: Must be safe on class-default metaobjects.
//
// Propagate - call LoadConfig() on my parent class (normally when this class contains globalconfig variables) 
// ForceScriptDefaults - we should reload script defaults prior to reading values from the ini 
// Iterate - call LoadConfig() on all child classes 
// Notify - call LoadConfig() on all object instances for this class-default metaobject 
void UObject::LoadConfig( UBOOL Propagate, UClass* Class, const TCHAR* InFilename, UBOOL ForceScriptDefaults, UBOOL Iterate, UBOOL Notify, UProperty* Prop )
{
	guard(UObject::LoadConfig);

	// rjp --
	// OrigClass is the actual class that LoadConfig() was called on 
	static UClass* OrigClass;
	if( !Class )
	{
	// If no class was specified in the call, this is the original class 
		Class = GetClass();
		OrigClass = Class;
	}
	if( !(Class->ClassFlags & CLASS_Config) )
	{
		if ( GetFlags() & RF_Marked )
            ClearFlags( RF_InSingularFunc );

		return;
	}

	UClass* ParentClass = Class->GetSuperClass();
	if (ParentClass)
	{
	// Proceed as always
		if (ParentClass->ClassFlags & CLASS_Config)
		{
			if (Propagate)
			{
				// call LoadConfig() on my parent class 
				if ( GetFlags() & RF_Marked )
				{
					debugf(NAME_DevSave, TEXT("(%s) %s propagating call to LoadConfig() updwards to %s  Iterate:%i Notify:%i"), Class->GetName(), GetFullName(), ParentClass->GetFullName(), Iterate, Notify);
					ParentClass->SetFlags(RF_Marked|RF_InSingularFunc);
				}

				LoadConfig( 1, ParentClass, NULL, ForceScriptDefaults, Iterate, Notify, Prop );

				if ( GetFlags() & RF_Marked )
				{
					if ( ParentClass->GetFlags() & RF_InSingularFunc )
						debugf(NAME_DevSave, TEXT("(%s) %s PARENT CLASS DIDN'T LOAD CONFIGURATION %s"), Class->GetName(), GetFullName(), ParentClass->GetFullName());
					ParentClass->ClearFlags(RF_Marked|RF_InSingularFunc);
				}
				
				// If we should propagate & iterate, wait until we've found the base class 
				if (Iterate || Notify)
					return;
			}
			
			else if (Iterate)
			{
				if ( GetFlags() & RF_Marked )
					debugf(NAME_DevSave, TEXT("(%s) %s Iterating LoadConfig() to all child classes"), Class->GetName(), GetFullName());

				// Not propagating call upwards 
				for (TObjectIterator<UClass> It; It; ++It)
					if (It->IsChildOf(Class))
					{
						UObject* Obj = It->GetDefaultObject();
						if ( GetFlags() & RF_Marked )
						{
							debugf(NAME_DevSave, TEXT("(%s) %s calling LoadConfig() on %s as part of iteration Notify:%i"), Class->GetName(), GetFullName(), It->GetFullName(), Notify);
							Obj->SetFlags(RF_InSingularFunc|RF_Marked);
						}

						It->GetDefaultObject()->LoadConfig(0, *It, NULL, ForceScriptDefaults,0,Notify,Prop);
						if ( GetFlags() & RF_Marked )
						{
							if ( Obj->GetFlags() & RF_InSingularFunc )
								debugf(NAME_DevSave, TEXT("(%s) %s HAD PARENTCLASS ... CLASS DIDN'T LOAD CONFIGURATION %s"), Class->GetName(), GetFullName(), It->GetFullName());

							Obj->ClearFlags(RF_Marked|RF_InSingularFunc);
						}
					}


				// LoadConfig() was called on this object during iteration, so stop here 
				return;
			}

			else if (Notify)
			{
				// call LoadConfig() on all objects of this class 
				// Do not propagate this call upwards, do not iterate or notify this call (would be redundant) 
				for (TObjectIterator<UObject> It; It; ++It)
				{
					if (It->IsA(Class))
					{
						if ( GetFlags() & RF_Marked )
						{
							debugf(NAME_DevSave, TEXT("(%s) %s calling LoadConfig() on %s as part of notification"), Class->GetName(), GetFullName(), It->GetFullName());
							It->SetFlags(RF_InSingularFunc|RF_Marked);
						}

						It->LoadConfig(0,It->GetClass(),NULL,ForceScriptDefaults,0,0,Prop);
						if ( GetFlags() & RF_Marked )
						{
							if ( It->GetFlags() & RF_InSingularFunc )
								debugf(NAME_DevSave, TEXT("(%s) %s HAD PARENT....OBJECT DIDN'T LOAD CONFIGURATION %s"), Class->GetName(), GetFullName(), It->GetFullName());

							It->ClearFlags(RF_InSingularFunc|RF_Marked);
						}

						It->PostEditChange();
					}
				}
			}
		}

		// We're at the base class 
		else if (Iterate)
		{
			for ( TObjectIterator<UClass> It; It; ++It )
				if (It->IsChildOf(Class))
				{
						UObject* Obj = It->GetDefaultObject();
						if ( GetFlags() & RF_Marked )
						{
							debugf(NAME_DevSave, TEXT("(%s) %s calling LoadConfig() on %s as part of iteration Notify:%i"), Class->GetName(), GetFullName(), It->GetFullName(), Notify);
							Obj->SetFlags(RF_InSingularFunc|RF_Marked);
						}
	
					It->GetDefaultObject()->LoadConfig( 0, *It, NULL, ForceScriptDefaults, 0, Notify, Prop );
						if ( GetFlags() & RF_Marked )
						{
							if ( Obj->GetFlags() & RF_InSingularFunc )
								debugf(NAME_DevSave, TEXT("(%s) %s CLASS DIDN'T LOAD CONFIGURATION %s"), Class->GetName(), GetFullName(), It->GetFullName());

							Obj->ClearFlags(RF_InSingularFunc|RF_Marked);
						}
				}

			return;
		}

		else if (Notify)
		{
			for (TObjectIterator<UObject> It; It; ++It)
				if (It->GetClass() == Class )
				{
						if ( GetFlags() & RF_Marked )
						{
							debugf(NAME_DevSave, TEXT("(%s) %s calling LoadConfig() on %s as part of notification (no parent)"), Class->GetName(), GetFullName(), It->GetFullName());
							It->SetFlags(RF_InSingularFunc|RF_Marked);
						}

					It->LoadConfig(0,It->GetClass(),NULL,ForceScriptDefaults,0,0,Prop);
						if ( GetFlags() & RF_Marked )
						{
							if ( It->GetFlags() & RF_InSingularFunc )
								debugf(NAME_DevSave, TEXT("(%s) %s OBJECT DIDN'T LOAD CONFIGURATION %s"), Class->GetName(), GetFullName(), It->GetFullName());

							It->ClearFlags(RF_InSingularFunc|RF_Marked);
						}
					It->PostEditChange();
				}
		}
	}
	// -- rjp

	if ( Prop && !Class->IsChildOf(Prop->GetOwnerClass()) )
	{
		if ( GetFlags() & RF_Marked )
            ClearFlags( RF_InSingularFunc );
		return;
	}

	if ( GetFlags() & RF_Marked )
	{
        if ( !(GetFlags() & RF_InSingularFunc) )
			debugf(NAME_DevSave, TEXT("(%s) I CALLED LOADCONFIG TWICE %s"), Class->GetName(), GetFullName());

        ClearFlags( RF_InSingularFunc );
	}

	UBOOL PerObject = ((GetClass()->ClassFlags & CLASS_PerObjectConfig) && GetIndex()!=INDEX_NONE);
	const TCHAR* Filename
	=	InFilename
	?	InFilename
	:	(PerObject && Outer!=GObjTransientPkg)
	?	(Class->ClassWithin==UObject::StaticClass() && GetOuter()->GetOuter()) ? GetOuter()->GetOuter()->GetName() : GetOuter()->GetName()
	:	*GetClass()->ClassConfigName;

	// If any of my properties are class variables, then LoadConfig() would also be called for each one of those classes.
	// Since OrigClass is a static variable, if the value of a class variable is a class different from the current class, 
	// we'll lose our nice reference to the original class - and cause any variables which were declared after this class variable to fail 
	// the 'if (OrigClass != Class)' check....better store it in a temporary place while we do the actual loading of our properties 
	UClass* MyOrigClass = OrigClass;

	const INT MaxNameSize = NAME_SIZE * 2;
	TCHAR Section[MaxNameSize];
	if ( PerObject )
	{
		appStrncpy(Section, GetName(), MaxNameSize);

		// If native class, stop here (compatibility with installer)
		if ( !(GetClass()->GetFlags() & RF_Native) )
		{
		// Append class name to this section's name, so that we can track the class this PerObjectConfig name belongs to 
		// This allows easy retrieval of section names for PerObjectConfig objects by class type 
			appStrncat(Section, TEXT(" "), MaxNameSize);
			appStrncat(Section, GetClass()->GetName(), MaxNameSize);
		}
	}

	if ( Prop )
		debugf(NAME_DevSave, TEXT("(%s) '%s' loading configuration for property '%s' from %s"), Class->GetName(), GetName(), Prop->GetName(), Filename);
	else debugf(NAME_DevSave, TEXT("(%s) '%s' loading configuration from %s"), Class->GetName(), GetName(), Filename);

	// Do the actual work here 
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Class); It; ++It )
	{
		if( It->PropertyFlags & CPF_Config )
		{
			if ( Prop && Prop != *It )
				continue;

			UBOOL globalConfig = It->PropertyFlags & CPF_GlobalConfig;
			UClass* OwnerClass = It->GetOwnerClass();

			if (MyOrigClass != Class &&	// If this class isn't the original class
				(!globalConfig ||		// and this property isn't global config

				// Or, if this property IS globalconfig, and the original class isn't a child of this property's owner class
				(globalConfig && !MyOrigClass->IsChildOf(OwnerClass)))
				)
				continue;				// do not process

			// In other words, process this value if
			// 1. This class is the original class calling LoadConfig, or
			// 2. This property is a globalconfig property, and this property's owner class is a parent of the original class

			FString Key, Value;
			UClass*         BaseClass = globalConfig ? OwnerClass : Class;
			if ( !PerObject )
				appStrncpy( Section, BaseClass->GetPathName(), MaxNameSize );
//			const TCHAR*    Section   = PerObject ? GetName() : BaseClass->GetPathName();
			UArrayProperty* ArrayProp = Cast<UArrayProperty>( *It );
			UMapProperty*   Map       = Cast<UMapProperty>( *It );

			Key = It->GetName();

			debugf(NAME_DevSave, TEXT("   Loading value for %s from [%s]"), *Key, Section);
			Value = TEXT("");

			if( ArrayProp && It->ArrayDim == 1 )	// TODO MultiDim array support
			{
				FArray* Array = (FArray*)((BYTE*)this + It->Offset);
				check(Array);

				INT Size = ArrayProp->Inner->ElementSize;
				TMap<INT,FString> ScriptDefaultText;

				// Here's where we grab the value of this property from the stored script defaults if ForceScriptDefaults is true 
				if ( ForceScriptDefaults )
				{
					FString DefaultValues;
					INT Count = 0;
					if ( globalConfig )
					{
						if ( ArrayProp->GetOwnerClass()->DefaultPropText.Len() > 0 )
						{
							TCHAR* DefaultPropText = new TCHAR [ArrayProp->GetOwnerClass()->DefaultPropText.Len() + 1];
							TCHAR* CurrentLine = DefaultPropText;
							appStrcpy( CurrentLine, *ArrayProp->GetOwnerClass()->DefaultPropText );

							while ( ParseLine((const TCHAR**)&CurrentLine, DefaultValues, 1) )
							{
								if ( Parse(*DefaultValues, *(FString::Printf(TEXT("%s("), It->GetName())), Value, TEXT(",() ")) )
								{
									Count = appAtoi(*Value.Left( Value.InStr(TEXT(")")) ));
									Value = Value.Mid(Value.InStr(TEXT("="))+1);
									if ( ScriptDefaultText.Find(Count) == NULL )
                                        ScriptDefaultText.Set(Count, *Value);
								}
							}

							delete [] DefaultPropText;
						}
					}
					else
					{
						for ( UClass* Cls = Class; Cls && Cls->IsChildOf(ArrayProp->GetOwnerClass()); Cls = Cls->GetSuperClass() )
						{
							if ( Cls->DefaultPropText.Len() > 0 )
							{
								TCHAR* DefaultPropText = new TCHAR [Cls->DefaultPropText.Len() + 1];
								TCHAR* CurrentLine = DefaultPropText;
								appStrcpy( CurrentLine, *Cls->DefaultPropText );

								Count=0;
								while ( ParseLine((const TCHAR**)&CurrentLine, DefaultValues, 1) )
								{
									if ( Parse(*DefaultValues, *(FString::Printf(TEXT("%s("), It->GetName())), Value, TEXT(",() ")) )
									{
										Count = appAtoi(*Value.Left( Value.InStr(TEXT(")")) ));
										Value = Value.Mid(Value.InStr(TEXT("="))+1);
										if ( ScriptDefaultText.Find(Count) == NULL )
											ScriptDefaultText.Set(Count, *Value);
									}
								}

								delete [] DefaultPropText;
							}
						}
					}
				}

				TMultiMap<FString,FString>* Sec = GConfig->GetSectionPrivate( Section, 0, 1, Filename );
				if( Sec && !(ForceScriptDefaults && Class != MyOrigClass) )
				{
					TArray<FString> List;
					Sec->MultiFind( Key, List );
					if ( List.Num() )
					{
						ArrayProp->DestroyValue(Array);
						Array->AddZeroed( Size, List.Num() );
					}

					else if ( ForceScriptDefaults )
					{
						// Clear current values
						ArrayProp->DestroyValue(Array);
						if ( ScriptDefaultText.Num() )
						{
							// Reverse sort the map so that we can access the highest index
							ScriptDefaultText.SortByKey(1);

							TMultiMap<INT,FString>::TIterator It(ScriptDefaultText);
							Array->AddZeroed( Size, It.Key() + 1 );

							// Add each value by index, skipping missing indexes (so that this behaves exactly like it would when the class is first initialized)
							for ( ; It; ++It )
								ArrayProp->Inner->ImportText( *It.Value(), (BYTE*)Array->GetData() + It.Key() * Size, 0 );

						}
						return;								
					}

					for( INT i=List.Num()-1,c=0; i>=0; i--,c++ )
						ArrayProp->Inner->ImportText( *List(i), (BYTE*)Array->GetData() + c*Size, 0 );
				}

				else if ( ForceScriptDefaults )
				{
					ArrayProp->ClearValue((BYTE*)Array);
					if (ScriptDefaultText.Num())
					{
						// Reverse sort the map so that we can access the highest index
						ScriptDefaultText.SortByKey(1);

						TMultiMap<INT,FString>::TIterator It(ScriptDefaultText);
						Array->AddZeroed( Size, It.Key() + 1 );

						// Add each value by index, skipping missing indexes (so that this behaves exactly like it would when the class is first initialized)
						for ( ; It; ++It )
							ArrayProp->Inner->ImportText( *It.Value(), (BYTE*)Array->GetData() + It.Key() * Size, 0 );
					}
				}
			}
			else if( Map )
			{
				TMultiMap<FString,FString>* Sec = GConfig->GetSectionPrivate( Section, 0, 1, Filename );
				if( Sec )
				{
					TArray<FString> List;
					Sec->MultiFind( Key, List );
					//FArray* Ptr  = (FArray*)((BYTE*)this + It->Offset);
					//INT     Size = Array->Inner->ElementSize;
					//Map->DestroyValue( Ptr );//!!won't do dction
					//Ptr->AddZeroed( Size, List.Num() );
					//for( INT i=List.Num()-1,c=0; i>=0; i--,c++ )
					//	Array->Inner->ImportText( *List(i), (BYTE*)Ptr->GetData() + c*Size, 0 );
				}
			}
			else for( INT i=0; i<It->ArrayDim; i++ )
			{
				Key = It->GetName();
				BYTE* Data = (BYTE*)this + It->Offset + i * It->ElementSize;
				if (It->ArrayDim != 1)
					Key += FString::Printf(TEXT("[%i]"), i);

				if( !(ForceScriptDefaults && Class != MyOrigClass) && GConfig->GetString( Section, *Key, Value, Filename ) )
					It->ImportText( *Value, Data, 0 );

				else if (ForceScriptDefaults)
				{
					Key = It->GetName();
					if ( It->ArrayDim == 1 )
						Key += TEXT("=");
					else
						Key += FString::Printf(TEXT("(%i)="), i);

					// Reset the value for this property to its instrinsic NULL value
					It->ClearValue( Data );

					// Now import the value from the class's defaultproperties block
					if ( globalConfig )
					{
						// If globalconfig, only check the property's owner class for default value
						if ( Parse(*It->GetOwnerClass()->DefaultPropText, *Key, Value, TEXT(",() ")) )
							It->ImportText( *Value, Data, 0 );
					}
					else
					{

						TArray<UClass*> Classes;

						// Otherwise, check each parent class until we find a default property for this value
						for ( UClass* Cls = Class; Cls && Cls->IsChildOf(It->GetOwnerClass()); Cls = Cls->GetSuperClass() )
							Classes.AddItem(Cls);

						for ( INT i=Classes.Num();i>0; i-- )
						{
							UClass* C = Classes(i-1);
							if ( Parse( *C->DefaultPropText, *Key, Value, TEXT(",() ")) )
							{
								It->ImportText(*Value, Data, 0);
							}
						}
/*
						// Otherwise, check each parent class until we find a default property for this value
						for ( UClass* Cls = Class; Cls && Cls->IsChildOf(It->GetOwnerClass()); Cls = Cls->GetSuperClass() )
						{
							if ( Parse(*Cls->DefaultPropText, *Key, Value, TEXT(",() ")) )
							{
								It->ImportText(*Value, Data, 0);
								break;
							}
						}

*/
					}
				}
			}

			if ( Prop )
				break;
		}
	}
	unguardobj;
}

// gam ---

static void LoadLocalizedProp( UProperty* Prop, const TCHAR *IntName, const TCHAR *SectionName, const TCHAR *KeyPrefix, BYTE* Data, UBOOL bQuiet = 0 );
static void LoadLocalizedStruct( UStruct* Struct, const TCHAR *IntName, const TCHAR *SectionName, const TCHAR *KeyPrefix, BYTE* Data, UBOOL bQuiet = 0 );

static void LoadLocalizedProp( UProperty* Prop, const TCHAR *IntName, const TCHAR *SectionName, const TCHAR *KeyPrefix, BYTE* Data, UBOOL bQuiet )
{
	guard(LoadLocalizedProp);

	UStructProperty* StructProperty = Cast<UStructProperty>( Prop );	
	if( StructProperty )
	{
        LoadLocalizedStruct(StructProperty->Struct, IntName, SectionName, KeyPrefix, Data, bQuiet );
        return;
    }

//	const TCHAR* LocalizedText = Localize( SectionName, KeyPrefix, IntName, NULL, 1 );
	static TCHAR BigString [8092];
	BigString[0] = 0;

	LocalizeBig( SectionName, KeyPrefix, IntName, NULL, bQuiet, BigString, ARRAY_COUNT(BigString) );

	if( *BigString )
		Prop->ImportText( BigString, Data, PPF_LocalizedOnly );
	unguard;
}

static void LoadLocalizedStruct( UStruct* Struct, const TCHAR *IntName, const TCHAR *SectionName, const TCHAR *KeyPrefix, BYTE* Data, UBOOL bQuiet )
{
	guard(LoadLocalizedStruct);

	UClass* Cls = Cast<UClass>(Struct);
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It( Struct ); It; ++It )
	{
	    UProperty* Prop = *It;
		if( !Prop->IsLocalized() ) 
			continue;

	    for( INT i = 0; i < Prop->ArrayDim; i++ )
	    {
	  	    FString NewPrefix;
            if( KeyPrefix )
                NewPrefix = FString::Printf( TEXT("%s."), KeyPrefix );

	        if( Prop->ArrayDim > 1 )
                NewPrefix += FString::Printf( TEXT("%s[%d]"), Prop->GetName(), i );
	        else
                NewPrefix += Prop->GetName();

            BYTE* NewData = Data + (Prop->Offset) + (i * Prop->ElementSize );

			// rjp -- 
			// TODO - fully implement support for writing arrays out to localization files properly
/*			UArrayProperty* ArrayProp = Cast<UArrayProperty>(Prop);
			if (ArrayProp)
			{
				INT InnerSize = ArrayProp->Inner->ElementSize, Count = 0;
				FArray* Array = (FArray*)NewData;
				if (Array)
					Count = Array->Num();

				for (INT j = 0; j < Count; j++)
					LoadLocalizedProp( ArrayProp->Inner, IntName, SectionName, *NewPrefix, (BYTE*)Array->GetData() + j * InnerSize);
			}
			else
*/
			UBOOL bOverride(bQuiet);

			// Might want to override quiet to avoid spamming
			if ( !bOverride )
			{
				// Don't spam messages about missing localization for properties that weren't given a value in script
				if ( !Prop->HasValue(NewData) )
					bOverride = 1;

				else if ( Cls )
				{
					BYTE* DefaultData = &Cls->Defaults(0);
					BYTE* SuperData = Prop->Offset < Cls->GetSuperClass()->Defaults.Num() ? &Cls->GetSuperClass()->Defaults(0) : NULL;

				// Don't spam messages about missing localization for instanced objects, when the object's value matches the class value
					bOverride = Prop->Matches(Data, DefaultData, i) &&
						( (((UObject*)Data)->GetOuter() != UObject::GetTransientPackage()) ||
                          (((UObject*)Data)->GetFlags() & RF_PerObjectLocalized) );

				// Don't spam messages about missing localization from child classes, since it will be inherited
					if ( !bOverride && SuperData )
                        bOverride = Prop->Matches(DefaultData, SuperData, i);
				}
			}
			// -- rjp
            LoadLocalizedProp( Prop, IntName, SectionName, *NewPrefix, NewData, bOverride );
	    }
	}
	unguard;
}

//
// Load localized text.
// Warning: Must be safe on class-default metaobjects.
//

void UObject::LoadLocalized()
{
	guard(UObject::LoadLocalized);

	UClass* Class = GetClass();
	checkSlow( Class );
	
	if( !(Class->ClassFlags & CLASS_Localized) )
		return;

	if( (GIsEditor && !GIsUCC) || (GUglyHackFlags & 64) )
		return;

	if( GetIndex()!=INDEX_NONE && ObjectFlags&RF_PerObjectLocalized && GetOuter()->GetOuter() )
	{
		// Localize as an instanced subobject
	    LoadLocalizedStruct( Class, GetOuter()->GetOuter()->GetName(), GetOuter()->GetName(), GetName(), (BYTE*)this, 1 );
	}
	else
	{
		const TCHAR* IntName     = GetIndex()==INDEX_NONE ? Class->GetOuter()->GetName() : GetOuter()->GetName();
		const TCHAR* SectionName = GetIndex()==INDEX_NONE ? Class->GetName()             : GetName();

	    LoadLocalizedStruct( Class, IntName, SectionName, NULL, (BYTE*)this );
	}

	unguardobj;
}

// --- gam

//
// Save configuration.
//warning: Must be safe on class-default metaobjects.
//!!may benefit from hierarchical propagation, deleting keys that match superclass...not sure what's best yet.
//
void UObject::SaveConfig( DWORD Flags, const TCHAR* InFilename )
{
	guard(UObject::SaveConfig);
	UBOOL PerObject = ((GetClass()->ClassFlags & CLASS_PerObjectConfig) && GetIndex()!=INDEX_NONE);
	UBOOL Iterate = 0;		// Iterate only if we had any globalconfig properties -- rjp
	UBOOL Propagate = 0;	// Propagate only if iterating, and at least one property wasn't from this class -- rjp
	const TCHAR* Filename
	=	InFilename
	?	InFilename
	:	(PerObject && Outer!=GObjTransientPkg)
	?	(Class->ClassWithin==UObject::StaticClass() && GetOuter()->GetOuter()) ? GetOuter()->GetOuter()->GetName() : GetOuter()->GetName()
	:	*GetClass()->ClassConfigName;

	if ( GDemoPlayback )	// Don't allow save configs when a demo is being replayed
		return;

	debugf(NAME_DevSave, TEXT("Saving %s configuration to %s"), PerObject ? GetName() : GetClass()->GetPathName(), Filename);

	const INT MaxNameSize = NAME_SIZE * 2;
	TCHAR Section[MaxNameSize];
	if ( PerObject )
	{
		appStrncpy(Section, GetName(), MaxNameSize);

		// If native class, stop here (compatibility with installer) -- rjp
		if ( !(GetClass()->GetFlags() & RF_Native) )
		{
			appStrncat(Section, TEXT(" "), MaxNameSize);
			appStrncat(Section, GetClass()->GetName(), MaxNameSize);
		}
	}
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(GetClass()); It; ++It )
	{
		if( (It->PropertyFlags & Flags)==Flags )
		{
			UClass* BaseClass = GetClass();

			// Only iterate if we have any globalconfig properties -- rjp
			if (It->PropertyFlags & CPF_GlobalConfig)
			{
				BaseClass = It->GetOwnerClass();
				Iterate = 1;
				if (BaseClass != GetClass())
					Propagate = 1;
			}

			TCHAR TempKey[256], Value[STATICSTRINGLENGTH]=TEXT("");
			const TCHAR*    Key     = It->GetName();
			if ( !PerObject )
				appStrncpy(Section, BaseClass->GetPathName(), MaxNameSize );
//			const TCHAR*    Section = PerObject ? GetName() : (It->PropertyFlags & CPF_GlobalConfig) ? It->GetOwnerClass()->GetPathName() : GetClass()->GetPathName();
			UArrayProperty* Array   = Cast<UArrayProperty>( *It );
			UMapProperty*   Map     = Cast<UMapProperty>( *It );
			if( Array )
			{
				TMultiMap<FString,FString>* Sec = GConfig->GetSectionPrivate( Section, 1, 0, Filename );
				check(Sec);
				Sec->Remove( Key );
				FArray* Ptr  = (FArray*)((BYTE*)this + It->Offset);
				INT     Size = Array->Inner->ElementSize;
				if (Array->Inner->IsA(UStructProperty::StaticClass()))
				{
					// UStructProperty::ExportText() has been adjusted to allow a much larger buffer...
					// so match it here to make sure we play nicely -- rjp
					TCHAR* Buffer = new TCHAR[256 * STATICSTRINGLENGTH];
					if (Buffer)
					{
						for (INT i = 0; i < Ptr->Num(); i++)
						{
							BYTE* Dest = (BYTE*)Ptr->GetData() + i*Size;
							Array->Inner->ExportTextItem( Buffer, Dest, Dest, 0 );
							Sec->Add( Key, Buffer );
						}

						delete [] Buffer;
					}

					else debugf(NAME_Warning, TEXT("Error allocating space for property '%s' while saving to ini!!"), Array->GetName());
				}
				else
				{
					for( INT i=0; i<Ptr->Num(); i++ )
					{
						TCHAR Buffer[STATICSTRINGLENGTH]=TEXT("");
						BYTE* Dest = (BYTE*)Ptr->GetData() + i*Size;
						Array->Inner->ExportTextItem( Buffer, Dest, Dest, 0 );
						Sec->Add( Key, Buffer );
					}
				}
			}
			else if( Map )
			{
				TMultiMap<FString,FString>* Sec = GConfig->GetSectionPrivate( Section, 1, 0, Filename );
				check(Sec);
				Sec->Remove( Key );
				//FArray* Ptr  = (FArray*)((BYTE*)this + It->Offset);
				//INT     Size = Array->Inner->ElementSize;
				//for( INT i=0; i<Ptr->Num(); i++ )
				//{
				//	TCHAR Buffer[1024]="";
				//	BYTE* Dest = (BYTE*)Ptr->GetData() + i*Size;
				//	Array->Inner->ExportTextItem( Buffer, Dest, Dest, 0 );
				//	Sec->Add( Key, Buffer );
				//}
			}
			else for( INT Index=0; Index<It->ArrayDim; Index++ )
			{
				if( It->ArrayDim!=1 )
				{
					appSprintf( TempKey, TEXT("%s[%i]"), It->GetName(), Index );
					Key = TempKey;
				}
				It->ExportText( Index, Value, (BYTE*)this, (BYTE*)this, 0 );
				GConfig->SetString( Section, Key, Value, Filename );
			}
		}
	}
	GConfig->Flush( 0 );

/*	
	** Old version ** 
	for( BaseClass=GetClass(); BaseClass->GetSuperClass(); BaseClass=BaseClass->GetSuperClass() )
		if( !(BaseClass->GetSuperClass()->ClassFlags & CLASS_Config) )
			break;
	if( BaseClass )
		for( TObjectIterator<UClass> It; It; ++It )
			if( It->IsChildOf(BaseClass) )
				It->GetDefaultObject()->LoadConfig( 0 );
*/

	// Only call LoadConfig() on the current class-default metaobject 
	// Let LoadConfig() sort out what to do -- rjp
	GetClass()->GetDefaultObject()->LoadConfig( Propagate, NULL, Filename, 0, Iterate );

	unguardobj;
}

//
// Reset configuration.
//
void UObject::ResetConfig( UClass* Class, const TCHAR* PropName )
{
	guard(UObject::ResetConfig);
	FString DefaultFilename, SrcFilename;

	check(Class!=NULL);

	// rjp --  Watch out for PerObjectConfig
	if ((Class->ClassFlags & CLASS_PerObjectConfig) && Class->GetIndex()!=INDEX_NONE)
	{
		Class->GetDefaultObject()->ClearConfig( Class, PropName );
		return;
	}
	// -- rjp

	if( Class->ClassConfigName==NAME_System )
	{
		DefaultFilename = TEXT("Default.ini");
		SrcFilename = GIni;
	}
	else if( Class->ClassConfigName==NAME_User )
	{
		DefaultFilename = TEXT("DefUser.ini");
		SrcFilename = GUserIni;
	}
	else
	{
		Class->GetDefaultObject()->ClearConfig( Class, PropName );	// rjp -- call ClearConfig() for objects not in default.ini or defuser.ini
		return;
	}

	UProperty* Prop=NULL;
	const TCHAR* PathName=Class->GetPathName();
	if ( PropName && *PropName )
	{
		Prop = FindField<UProperty>(Class, PropName);
		if ( !Prop )
			debugf(NAME_DevSave,TEXT("No property named '%s' found in %s while attempting to reset configuration (native only?)"), PropName, Class->GetName());

		if (Prop && (Prop->PropertyFlags&CPF_GlobalConfig))
			PathName = Prop->GetOwnerClass()->GetPathName();
		debugf(NAME_DevSave, TEXT("Resetting configuration for property '%s' in %s from %s"), PropName, PathName, *DefaultFilename);
	}
	else debugf(NAME_DevSave, TEXT("Resetting %s configuration from %s"), PathName, *DefaultFilename);

	TMultiMap<FString,FString>* DefaultSec(NULL), *Sec(NULL);
	DefaultSec = GConfig->GetSectionPrivate( PathName, 0, 1, *DefaultFilename );
	
	if (!DefaultSec)
	{
		// The default ini doesn't contain this section
		Class->GetDefaultObject()->ClearConfig( Class, PropName );
		return;
	}

	Sec = GConfig->GetSectionPrivate( PathName, DefaultSec->Num(), 0, *SrcFilename );

	TArray<FString> Keys;
	INT Count = DefaultSec->Num(Keys);

	UBOOL bFound = (PropName == NULL || *PropName == NULL);

	/* rjp --
	The following code is necessary for matching static array names when calling resetconfig/clearconfig on a static array
	e.g. you have a static int array var int IntArray[3];
	These will be stored in the ini as
	IntArray[0]=x
	IntArray[1]=x
	IntArray[2]=x

	You don't want to require calling ResetConfig/ClearConfig for each element of a static array, so we need to do this manually
	-- rjp */
	for (INT i = 0; i < Count; i++)
	{
		FString& Key = Keys(i);
		if (PropName && *PropName)
		{
			INT len = appStrlen(PropName);
			if (Key.Left(len) != PropName)
				continue;

			// Make sure this isn't a different variable with a matching name (e.g. looking for MyVar, and this var is MyVarAndSomeMore)
			if ( len < Key.Len() )
			{
				TCHAR next = Key[len];
				if ( next != '(' && next != '[' && next != '=' )
					continue;
			}

			bFound = 1;
		}
		TArray<FString> Values;
		Sec->MultiRemove(Key);
        DefaultSec->MultiFind( Key, Values );

		// Must add manually, because ConfigSections are upside down -- rjp
		for (INT j = Values.Num() - 1; j >= 0; j--)
			Sec->Add( *Key, *Values(j) );
	}

	if ( !bFound )
	{
		Class->GetDefaultObject()->ClearConfig(Class,PropName);
		return;
	}

	GConfig->Flush( 0, *SrcFilename );

	UObject* Obj = Class->GetDefaultObject();

	Obj->SetFlags(RF_InSingularFunc|RF_Marked);
	Obj->LoadConfig( 0, NULL, *SrcFilename, 0, 1, 1, Prop );

	if ( Obj->GetFlags() & RF_InSingularFunc )
		debugf(NAME_DevSave, TEXT("RESET:: CLASS DIDN'T LOAD CONFIGURATION %s"), Obj->GetFullName());

	Obj->ClearFlags(RF_Marked|RF_InSingularFunc);

	unguard;
}

// rjp --
void UObject::ClearConfig( UClass* Class, const TCHAR* PropName )
{
	guard(UObject::ClearConfig);
	if ( Class == NULL )
		Class = GetClass();

	// Class->GetIndex() == INDEX_NONE when classes are being registered
	UBOOL PerObject = ((Class->ClassFlags & CLASS_PerObjectConfig) && Class->GetIndex()!=INDEX_NONE);

	const INT MaxNameSize = NAME_SIZE * 2;
	TCHAR Section[MaxNameSize];
	const TCHAR* InFilename = (PerObject && Outer!=GObjTransientPkg) ? (Class->ClassWithin==UObject::StaticClass() && GetOuter()->GetOuter()) ? GetOuter()->GetOuter()->GetName() : GetOuter()->GetName() : *Class->ClassConfigName;
	// If PerObject, section headers for native classes will look like:
	//  [ObjectName] ; old way - compatibility with installer
	// Otherwise, section headers will look like: [ObjectName ClassName]
	UProperty* Prop = NULL;
	if ( PropName && *PropName )
	{
		Prop = FindField<UProperty>(Class, PropName);
		if ( !Prop )
			debugf(NAME_DevSave,TEXT("No property named '%s' found in %s while attempting to clear configuration (native only?)"), PropName, Class->GetName());
	}

	if ( PerObject )
	{
		appStrncpy( Section, GetName(), MaxNameSize );
		
		// If native class, stop here (compatibility with installer)
		if ( !(GetClass()->GetFlags() & RF_Native) )
		{
			appStrncat( Section, TEXT(" "), MaxNameSize );
			appStrncat( Section, GetClass()->GetName(), MaxNameSize );
		}
	}
	else appStrncpy( Section, (Prop&&(Prop->PropertyFlags&CPF_GlobalConfig)) ? Prop->GetOwnerClass()->GetPathName():Class->GetPathName(), MaxNameSize );

	// Add .ini extension.
	TCHAR Filename [256];
	appStrncpy(Filename, InFilename, ARRAY_COUNT(Filename));

	INT Len = appStrlen(Filename);
	if( Len<5 || (Filename[Len-4]!='.' && Filename[Len-5]!='.') )
		appStrcat( Filename, TEXT(".ini") );

	if ( PropName && *PropName )
	{
		debugf(NAME_DevSave, TEXT("Clearing configuration for property '%s' in %s from %s"), PropName, Section, Filename);

		/* rjp --
		The following code is necessary for matching static array names when calling resetconfig/clearconfig on a static array
		e.g. you have a static int array var int IntArray[3];
		These will be stored in the ini as
		IntArray[0]=x
		IntArray[1]=x
		IntArray[2]=x

		You don't want to require calling ResetConfig/ClearConfig for each element of a static array, so we need to do this manually
		-- rjp */
		// Only want to remove a single line from the .ini file, so remove it from the FConfigFile object, then write to disk
		TMultiMap<FString,FString>* Sec = GConfig->GetSectionPrivate( Section, 0, 0, Filename );
		if ( Sec )
		{
			TArray<FString> Keys;
			INT Count = Sec->Num(Keys);
			INT len = appStrlen(PropName);
			UBOOL bFound = 0;

			for ( INT i = 0; i < Count; i++ )
			{
				FString& Key = Keys(i);
				if (Key.Left(len) != PropName)
					continue;

			// Make sure this isn't a different variable with a matching name (e.g. looking for MyVar, and this var is MyVarAndSomeMore)
			if ( len < Key.Len() )
				{
					TCHAR next = Key[len];
					if ( next != '(' && next != '[' && next != '=' )
						continue;
				}

				if ( Sec->MultiRemove(Key) > 0 )
					bFound = 1;
			}

			if ( bFound )
				GConfig->Flush(0, Filename);
		}
	}
	else
	{
		debugf(NAME_DevSave, TEXT("Clearing configuration for %s from %s"), Section, Filename);
		GConfig->EmptySection( Section, Filename);
	}

	SetFlags(RF_Marked|RF_InSingularFunc);

	// Only notify if not perobjectconfig
	LoadConfig( 0, NULL, Filename, 1, 1, !PerObject, Prop );

	if ( GetFlags() & RF_InSingularFunc )
		debugf(NAME_DevSave, TEXT("CLEAR:: I DIDN'T LOAD CONFIGURATION %s"), GetFullName());

	ClearFlags(RF_Marked|RF_InSingularFunc);

	unguard;
}
// -- rjp

/*-----------------------------------------------------------------------------
	UObject IUnknown implementation.
-----------------------------------------------------------------------------*/

// Note: This are included as part of UObject in case we ever want
// to make Unreal objects into Component Object Model objects.
// The Unreal framework is set up so that it would be easy to componentize
// everything, but there is not yet any benefit to doing so, thus these
// functions aren't implemented.

//
// Query the object on behalf of an Ole client.
//
DWORD STDCALL UObject::QueryInterface( const FGuid &RefIID, void **InterfacePtr )
{
	guard(UObject::QueryInterface);
	// This is not implemented and might not ever be.
	*InterfacePtr = NULL;
	return 0;
	unguardobj;
}

//
// Add a reference to the object on behalf of an Ole client.
//
DWORD STDCALL UObject::AddRef()
{
	guard(UObject::AddRef);
	// This is not implemented and might not ever be.
	return 0;
	unguardobj;
}

//
// Release the object on behalf of an Ole client.
//
DWORD STDCALL UObject::Release()
{
	guard(UObject::Release);
	// This is not implemented and might not ever be.
	return 0;
	unguardobj;
}

/*-----------------------------------------------------------------------------
	Mo Functions.
-----------------------------------------------------------------------------*/

//
// Object accessor.
//
UObject* UObject::GetIndexedObject( INT Index )
{
	guardSlow(UObject::GetIndexedObject);
	if( Index>=0 && Index<GObjObjects.Num() )
		return GObjObjects(Index);
	else
		return NULL;
	unguardSlow;
}

//
// Find an optional object.
//
UObject* UObject::StaticFindObject( UClass* ObjectClass, UObject* InObjectPackage, const TCHAR* InName, UBOOL ExactClass )
{
	guard(UObject::StaticFindObject);

	// Resolve the object and package name.
	UObject* ObjectPackage = InObjectPackage!=ANY_PACKAGE ? InObjectPackage : NULL;
	if( !ResolveName( ObjectPackage, InName, 0, 0 ) )
		return NULL;

	// Make sure it's an existing name.
	FName ObjectName(InName,FNAME_Find);
	if( ObjectName==NAME_None )
		return NULL;

	// Find in the specified package.
	INT iHash = GetObjectHash( ObjectName, ObjectPackage ? ObjectPackage->GetIndex() : 0 );
	for( UObject* Hash=GObjHash[iHash]; Hash!=NULL; Hash=Hash->HashNext )
	{
		/*
		InName: the object name to search for. Two possibilities.
			A = No dots. ie: 'S_Actor', a texture in Engine
			B = Dots. ie: 'Package.Name' or 'Package.Group.Name', or an even longer chain of outers. The first one needs to be relative to InObjectPackage.
			I'll define InName's package to be everything before the last period, or "" otherwise.
		InObjectPackage: the package or Outer to look for the object in. Can be ANY_PACKAGE, or NULL. Three possibilities:
			A = Non-null. Search for the object relative to this package.
			B = Null. We're looking for the object, as specified exactly in InName.
			C = ANY_PACKAGE. Search anywhere for the package (resrictions on functionality, see below)
		ObjectPackage: The package we need to be searching in. NULL means we don't care what package to search in
			InName.A &&  InObjectPackage.C ==> ObjectPackage = NULL
			InName.A && !InObjectPackage.C ==> ObjectPackage = InObjectPackage
			InName.B &&  InObjectPackage.C ==> ObjectPackage = InName's package
			InName.B && !InObjectPackage.C ==> ObjectPackage = InName's package, but as a subpackage of InObjectPackage
		*/
		if
		(	(Hash->GetFName()==ObjectName)
			/*If there is no package (no InObjectPackage specified, and InName's package is "") 
			and the caller specified any_package, then accept it, regardless of its package.*/ 
		&&	(	(!ObjectPackage && InObjectPackage==ANY_PACKAGE)
			/*Or, if the object is directly within this package, (whether that package is non-NULL or not), 
			then accept it immediately.*/ 
			||	(Hash->Outer == ObjectPackage)
			/*Or finally, if the caller specified any_package, but they specified InName's package, 
			then check if the object IsIn (is recursively within) InName's package. 
			We don't check if IsIn if ObjectPackage is NULL, since that would be true for every object. 
			This check also allows you to specify Package.Name to access an object in Package.Group.Name.*/ 
			||	(InObjectPackage == ANY_PACKAGE && ObjectPackage && Hash->IsIn(ObjectPackage)) )
		&&	(ObjectClass==NULL || (ExactClass ? Hash->GetClass()==ObjectClass : Hash->IsA(ObjectClass))) )
			return Hash;
	}

	// Not found.
	return NULL;
	unguard;
}

//
// Find an object; can't fail.
//
UObject* UObject::StaticFindObjectChecked( UClass* ObjectClass, UObject* ObjectParent, const TCHAR* InName, UBOOL ExactClass )
{
	guard(UObject::StaticFindObjectChecked);
	UObject* Result = StaticFindObject( ObjectClass, ObjectParent, InName, ExactClass );
	if( !Result )
		appErrorf( LocalizeError(TEXT("ObjectNotFound"),TEXT("Core")), ObjectClass->GetName(), ObjectParent==ANY_PACKAGE ? TEXT("Any") : ObjectParent ? ObjectParent->GetName() : TEXT("None"), InName );
	return Result;
	unguard;
}

//
// Binary initialize object properties to zero or defaults.
//
void UObject::InitProperties( BYTE* Data, INT DataCount, UClass* DefaultsClass, BYTE* Defaults, INT DefaultsCount, UObject* DestObject, UObject* SuperObject, UBOOL InstanceSubobjects )
{
	guard(UObject::InitProperties);
	check(DataCount>=sizeof(UObject));
	INT Inited = sizeof(UObject);

	// Find class defaults if no template was specified.
	//warning: At startup, DefaultsClass->Defaults.Num() will be zero for some native classes.
	guardSlow(FindDefaults);
	if( !Defaults && DefaultsClass && DefaultsClass->Defaults.Num() )
	{
		Defaults      = &DefaultsClass->Defaults(0);
		DefaultsCount =  DefaultsClass->Defaults.Num();
	}
	unguardSlow;

	// Copy defaults.
	guardSlow(DefaultsFill);
	if( Defaults )
	{
		checkSlow(DefaultsCount>=Inited);
		checkSlow(DefaultsCount<=DataCount);
		appMemcpy( Data+Inited, Defaults+Inited, DefaultsCount-Inited );
		Inited = DefaultsCount;
	}
	unguardSlow;

	// Zero-fill any remaining portion.
	guardSlow(ZeroFill);
	if( Inited < DataCount )
		appMemzero( Data+Inited, DataCount-Inited );
	unguardSlow;

	// This is a duplicate. Set all transients to their default value.
	if( SuperObject )
	{
		checkSlow(DestObject);
		BYTE* ClassDefaults = &DefaultsClass->Defaults(0);
		for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(DestObject->GetClass()); It; ++It )
			if( It->PropertyFlags & CPF_Transient )
				appMemcpy( Data + It->Offset, ClassDefaults + It->Offset, It->ArrayDim*It->ElementSize );
	}

	// Construct anything required.
	guardSlow(ConstructProperties);

//	if ( DefaultsClass && DefaultsClass->GetOuter() && !appStricmp(DefaultsClass->GetName(), TEXT("UT2K4PerformWarn")) )
//		debugf(TEXT(""));

	if( DefaultsClass )
	{
		for( UProperty* P=DefaultsClass->ConstructorLink; P; P=P->ConstructorLinkNext )
		{
			if( P->Offset < DefaultsCount )
			{
				appMemzero( Data + P->Offset, P->GetSize() );//bad for bools, but not a real problem because they aren't constructed!!
				/* rjp -- fix for nesting inline objects 
				          ObjectA contains instanced ObjectB, which contains instanced ObjectC -
						  pass ObjectB in as ObjectC's SuperObject, rather than ObjectA
				P->CopyCompleteValue( Data + P->Offset, Defaults + P->Offset, SuperObject ? SuperObject : DestObject, InstanceSubobjects ); -- rjp */
				P->CopyCompleteValue( Data + P->Offset, Defaults + P->Offset, SuperObject ? ( P->IsA(UObjectProperty::StaticClass()) && DestObject ? DestObject : SuperObject ) : DestObject, InstanceSubobjects );
			}
		}
	}
	unguardSlow;

	unguard;
}

//
// Destroy properties.
//
void UObject::ExitProperties( BYTE* Data, UClass* Class )
{
	UProperty* P=NULL;
	guard(UObject::ExitProperties);
	for( P=Class->ConstructorLink; P; P=P->ConstructorLinkNext )
		P->DestroyValue( Data + P->Offset );
	unguardf(( TEXT("(%s)"), P->GetFullName() ));
}

//
// Init class default object.
//
void UObject::InitClassDefaultObject( UClass* InClass, UBOOL SetOuter )
{
	guard(UObject::InitClassDefaultObject);

	// Init UObject portion.
	appMemset( this, 0, sizeof(UObject) );
	*(void**)this = *(void**)InClass;
	Class         = InClass;
	Index         = INDEX_NONE;

	// Init post-UObject portion.
	if( SetOuter )
		Outer = InClass->GetOuter();
	InitProperties( (BYTE*)this, InClass->GetPropertiesSize(), InClass->GetSuperClass(), NULL, 0, SetOuter ? this : NULL );

	unguard;
}

//
// Global property setting.
//
void UObject::GlobalSetProperty( const TCHAR* Value, UClass* Class, UProperty* Property, INT Offset, UBOOL Immediate )
{
	guard(UObject::GlobalSetProperty);

	// Apply to existing objects of the class, with notification.
	if( Immediate )
	{
		for( FObjectIterator It; It; ++It )
		{
			if( It->IsA(Class) )
			{
				Property->ImportText( Value, (BYTE*)*It + Offset, PPF_Localized );
				It->PostEditChange();
			}
		}
	}

	// Apply to defaults.
	Property->ImportText( Value, &Class->Defaults(Offset), PPF_Localized );
	Class->GetDefaultObject()->SaveConfig();

	unguard;
}

/*-----------------------------------------------------------------------------
	Object registration.
-----------------------------------------------------------------------------*/

//
// Preregister an object.
//warning: Sometimes called at startup time.
//
void UObject::Register()
{
	guard(UObject::Register);
	check(GObjInitialized);

	// Get stashed registration info.
	const TCHAR* InOuter = *(const TCHAR**)&Outer;

#if SERIAL_POINTER_INDEX
	const TCHAR* InName  = *(const TCHAR**)&StateFrame;
	StateFrame   = NULL;
#else
	const TCHAR* InName  = *(const TCHAR**)&Name;
#endif

	// Set object properties.
	Outer        = CreatePackage(NULL,InOuter);
	Name         = InName;
	_LinkerIndex = INDEX_NONE;

	// Validate the object.
	if( Outer==NULL )
		appErrorf( TEXT("Autoregistered object %s is unpackaged"), GetFullName() );
	if( GetFName()==NAME_None )
		appErrorf( TEXT("Autoregistered object %s has invalid name"), GetFullName() );
	if( StaticFindObject( NULL, GetOuter(), GetName() ) )
		appErrorf( TEXT("Autoregistered object %s already exists"), GetFullName() );

	// Add to the global object table.
	AddObject( INDEX_NONE );

	unguard;
}

//
// Handle language change.
//
void UObject::LanguageChange()
{
	guard(UObject::LanguageChange);
	LoadLocalized();
	unguard;
}

/*-----------------------------------------------------------------------------
	StaticInit & StaticExit.
-----------------------------------------------------------------------------*/

void SerTest( FArchive& Ar, DWORD& Value, DWORD Max )
{
	DWORD NewValue=0;
	for( DWORD Mask=1; NewValue+Mask<Max && Mask; Mask*=2 )
	{
		BYTE B = ((Value&Mask)!=0);
		Ar.SerializeBits( &B, 1 );
		if( B )
			NewValue |= Mask;
	}
	Value = NewValue;
}

//
// Init the object manager and allocate tables.
//
void UObject::StaticInit()
{
	guard(UObject::StaticInit);
	GObjNoRegister = 1;

	// Checks.
	check(sizeof(BYTE)==1);
	check(sizeof(SBYTE)==1);
	check(sizeof(_WORD)==2);
	check(sizeof(DWORD)==4);
	check(sizeof(QWORD)==8);
	check(sizeof(ANSICHAR)==1);
	check(sizeof(UNICHAR)==2);
	check(sizeof(SWORD)==2);
	check(sizeof(INT)==4);
	check(sizeof(SQWORD)==8);
	check(sizeof(UBOOL)==4);
	check(sizeof(FLOAT)==4);
	check(sizeof(DOUBLE)==8);
	check(ENGINE_MIN_NET_VERSION<=ENGINE_VERSION);

	// Development.
	GCheckConflicts = ParseParam(appCmdLine(),TEXT("CONFLICTS"));
	GNoGC           = ParseParam(appCmdLine(),TEXT("NOGC"));

	// Init hash.
	for( INT i=0; i<ARRAY_COUNT(GObjHash); i++ )
		GObjHash[i] = NULL;

	// If statically linked, initialize registrants.
	#if __STATIC_LINK
		AUTO_INITIALIZE_REGISTRANTS;
	#endif
	
	// Note initialized.
	GObjInitialized = 1;

	// Add all autoregistered classes.
	ProcessRegistrants();

	// Allocate special packages.
	GObjTransientPkg = new( NULL, TEXT("Transient") )UPackage;
	GObjTransientPkg->AddToRoot();

	// Load package remap.
	GObjPackageRemap = new TMultiMap<FName,FName>;
	GObjPackageRemap->Add( TEXT("UnrealI"), TEXT("UnrealShare") );

	debugf( NAME_Init, TEXT("Object subsystem initialized") );
	unguard;
}

//
// Process all objects that have registered.
//
void UObject::ProcessRegistrants()
{
	guard(UObject::ProcessRegistrants);
	if( ++GObjRegisterCount==1 )
	{
		// Make list of all objects to be registered.
		for( ; GAutoRegister; GAutoRegister=*(UObject **)&GAutoRegister->_LinkerIndex )
			GObjRegistrants.AddItem( GAutoRegister );
		for( INT i=0; i<GObjRegistrants.Num(); i++ )
			GObjRegistrants(i)->ConditionalRegister();
		GObjRegistrants.Empty();
		check(!GAutoRegister);
	}
	GObjRegisterCount--;
	unguard;
}

//
// Shut down the object manager.
//
void UObject::StaticExit()
{
	guard(UObject::StaticExit);
	check(GObjLoaded.Num()==0);
	check(GObjRegistrants.Num()==0);
	check(!GAutoRegister);

	// Cleanup root.
	GObjTransientPkg->RemoveFromRoot();

	// Tag all objects as unreachable.
	for( FObjectIterator It; It; ++It )
		It->SetFlags( RF_Unreachable | RF_TagGarbage );

	// Tag all names as unreachable.
	for( INT i=0; i<FName::GetMaxNames(); i++ )
		if( FName::GetEntry(i) )
			FName::GetEntry(i)->Flags |= RF_Unreachable;

	// Purge all objects.
	GExitPurge = 1;
	PurgeGarbage();
	GObjObjects.Empty();

	// Empty arrays to prevent falsely-reported memory leaks.
	GObjLoaded			.Empty();
	GObjObjects			.Empty();
	GObjAvailable		.Empty();
	GObjLoaders			.Empty();
	GObjRoot			.Empty();
	GObjRegistrants		.Empty();
	GObjPreferences		.Empty();
	GObjDrivers			.Empty();
	delete GObjPackageRemap;

	GObjInitialized = 0;
	debugf( NAME_Exit, TEXT("Object subsystem successfully closed.") );
	unguard;
}

/*-----------------------------------------------------------------------------
	UObject Tick.
-----------------------------------------------------------------------------*/

//
// Mark one unit of passing time. This is used to update the object
// caching status. This must be called when the object manager is
// in a clean state (outside of all code which retains pointers to
// object data that was gotten).
//
void UObject::StaticTick()
{
	guard(UObject::StaticTick);
	check(GObjBeginLoadCount==0);

	// Check natives.
	CORE_API extern int GNativeDuplicate;
	if( GNativeDuplicate )
		appErrorf( TEXT("Duplicate native registered: %i"), GNativeDuplicate );

	CORE_API extern int GCastDuplicate;
	if( GCastDuplicate )
		appErrorf( TEXT("Duplicate cast registered: %i"), GCastDuplicate );

	unguard;
}

/*-----------------------------------------------------------------------------
   Shutdown.
-----------------------------------------------------------------------------*/

//
// Make sure this object has been shut down.
//
void UObject::ConditionalShutdownAfterError()
{
	if( !(GetFlags() & RF_ErrorShutdown) )
	{
		SetFlags( RF_ErrorShutdown );
		try
		{
			ShutdownAfterError();
		}
		catch( ... )
		{
			debugf( NAME_Exit, TEXT("Double fault in object ShutdownAfterError") );
		}
	}
}

//
// After a critical error, shutdown all objects which require
// mission-critical cleanup, such as restoring the video mode,
// releasing hardware resources.
//
void UObject::StaticShutdownAfterError()
{
	guard(UObject::StaticShutdownAfterError);
	if( GObjInitialized )
	{
		static UBOOL Shutdown=0;
		if( Shutdown )
			return;
		Shutdown = 1;
		debugf( NAME_Exit, TEXT("Executing UObject::StaticShutdownAfterError") );
		try
		{
			for( INT i=0; i<GObjObjects.Num(); i++ )
				if( GObjObjects(i) )
					GObjObjects(i)->ConditionalShutdownAfterError();
		}
		catch( ... )
		{
			debugf( NAME_Exit, TEXT("Double fault in object manager ShutdownAfterError") );
		}
	}
	unguard;
}

//
// Bind package to DLL.
//warning: Must only find packages in the \Unreal\System directory!
//
void UObject::BindPackage( UPackage* Pkg )
{
	guard(UPackage::BindPackage);
	if( !Pkg->DllHandle && !Pkg->GetOuter() && !Pkg->AttemptedBind )
	{
		TCHAR PathName[1024];
		appSprintf( PathName, TEXT("%s%s"), appBaseDir(), Pkg->GetName() );
		Pkg->AttemptedBind  = 1;
		GObjNoRegister      = 0;
		Pkg->DllHandle      = appGetDllHandle( PathName );
		GObjNoRegister      = 1;
		if( Pkg->DllHandle )
		{
			#if !__STATIC_LINK
			debugf( NAME_DevLoad, TEXT("Bound to %s%s"), Pkg->GetName(), DLLEXT );
			#endif
			ProcessRegistrants();
		}
	}
	unguard;
}



/*---------------------------------------------------------------------------------
   By-name member access functionality.    Useful for non-native object dissection.
-----------------------------------------------------------------------------------*/

UBOOL UObject::FindArrayProperty( FString Name, FArray** Array, INT* ElementSize )
{
	BYTE* Base = (BYTE*) this;
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(GetClass()); It; ++It )
	{				
		UArrayProperty* ArrayProperty;
		if( (ArrayProperty=Cast<UArrayProperty>(*It)) != NULL )
		{			
			FString ArrayName = It->GetName();					
			if( ArrayName == Name)
			{						
				*Array = (FArray*)(Base+It->Offset);
				*ElementSize = ArrayProperty->Inner->ElementSize;
				return true;
			}
		}
	}
	return false;
}

UBOOL UObject::FindObjectProperty( FString Name, UObject** Object )
{
	BYTE* Base = (BYTE*) this;
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(GetClass()); It; ++It )
	{				
		UObjectProperty* ObjectProperty;
		if( (ObjectProperty=Cast<UObjectProperty>(*It)) != NULL )
		{			
			FString ObjectName = It->GetName();					
			if( ObjectName == Name)
			{						
				*Object = (UObject*)(Base+It->Offset);
				return true;
			}
		}
	}
	return false;
}

UBOOL UObject::FindStructProperty( FString Name, UStruct** Struct )
{
	BYTE* Base = (BYTE*) this;
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(GetClass()); It; ++It )
	{				
		UStructProperty* StructProperty;
		if( (StructProperty=Cast<UStructProperty>(*It)) != NULL )
		{			
			FString StructName = It->GetName();					
			if( StructName == Name)
			{						
				*Struct = (UStruct*)(Base+It->Offset);
				return true;
			}
		}
	}
	return false;
}

UBOOL UObject::FindFloatProperty( FString Name, FLOAT* FloatVar )
{
	BYTE* Base = (BYTE*) this;
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(GetClass()); It; ++It )
	{				
		UFloatProperty* FloatProperty;
		if( (FloatProperty=Cast<UFloatProperty>(*It)) != NULL )
		{			
			FString FloatName = It->GetName();					
			if( FloatName == Name)
			{						
				*FloatVar = *(FLOAT*)(Base+It->Offset);
				return true;
			}
		}
	}
	return false;
}

UBOOL UObject::FindIntProperty( FString Name, INT* IntVar )
{
	BYTE* Base = (BYTE*) this;
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(GetClass()); It; ++It )
	{				
		UIntProperty* IntProperty;
		if( (IntProperty=Cast<UIntProperty>(*It)) != NULL )
		{			
			FString IntName = It->GetName();					
			if( IntName == Name)
			{						
				*IntVar = *(INT*)(Base+It->Offset);
				return true;
			}
		}
	}
	return false;
}

UBOOL UObject::FindPointerProperty( FString Name, PTRINT* PointerVar )
{
	BYTE* Base = (BYTE*) this;
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(GetClass()); It; ++It )
	{
		UPointerProperty* PointerProperty;
		if( (PointerProperty=Cast<UPointerProperty>(*It)) != NULL )
		{
			FString PointerName = It->GetName();
			if( PointerName == Name)
			{
				*PointerVar = *(PTRINT*)(Base+It->Offset);
				return true;
			}
		}
	}
	return false;
}

UBOOL UObject::FindBoolProperty( FString Name, UBOOL* BoolVar )
{
	BYTE* Base = (BYTE*) this;
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(GetClass()); It; ++It )
	{				
		UBoolProperty* BoolProperty;
		if( (BoolProperty=Cast<UBoolProperty>(*It)) != NULL )
		{			
			FString BoolName = It->GetName();					
			if( BoolName == Name)
			{						
				*BoolVar = *(UBOOL*)(Base+It->Offset);
				return true;
			}
		}
	}
	return false;
}

UBOOL UObject::FindFNameProperty( FString Name, FName* FNameVar )
{	
	BYTE* Base = (BYTE*) this; 
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(GetClass()); It; ++It )
	{				
		UNameProperty* FNameProperty;
		if( (FNameProperty=Cast<UNameProperty>(*It)) != NULL )
		{			
			FString FNameName = It->GetName();					
			if( FNameName == Name)
			{						
				*FNameVar = *(FName*)(Base+It->Offset);
				return true;
			}
		}
	}
	return false;		
}



/*-----------------------------------------------------------------------------
   Command line.
-----------------------------------------------------------------------------*/

//
// Archive for enumerating other objects referencing a given object.
//
class FArchiveShowReferences : public FArchive
{
public:
	FArchiveShowReferences( FOutputDevice& InAr, UObject* InOuter, UObject* InObj, TArray<UObject*>& InExclude )
	: DidRef( 0 ), Ar( InAr ), Parent( InOuter ), Obj( InObj ), Exclude( InExclude )
	{
		Obj->Serialize( *this );
	}
	UBOOL DidRef;
private:
	FArchive& operator<<( UObject*& Obj )
	{
		guard(FArchiveShowReferences<<Obj);
		if( Obj && Obj->GetOuter()!=Parent )
		{
			INT i;
			for( i=0; i<Exclude.Num(); i++ )
				if( Exclude(i) == Obj->GetOuter() )
					break;
			if( i==Exclude.Num() )
			{
				if( !DidRef )
					Ar.Logf( TEXT("   %s references:"), Obj->GetFullName() );
				Ar.Logf( TEXT("      %s"), Obj->GetFullName() );
				DidRef=1;
			}
		}
		return *this;
		unguard;
	}
	FOutputDevice& Ar;
	UObject* Parent;
	UObject* Obj;
	TArray<UObject*>& Exclude;
};

// Archive for enumerating the objects referenced by an object
class FArchiveListRefs : public FArchive
{
public:
	FArchiveListRefs( UObject* Src )
		: Base(Src)
	{
		Base->Serialize(*this);
	}
	INT GetCount() { return Refs.Num(); }
	const TArray<UObject*>& GetObjects() const
	{
		return Refs;
	}
	FArchive& operator<<( class UObject*& Obj )
	{
		if ( Obj && Obj != UClass::StaticClass() )
			Refs.AddUniqueItem(Obj);
		return *this;
	}

protected:
	UObject* Base;
	TArray<UObject*> Refs;
};

//
// Archive for finding who references an object.
//
class FArchiveFindCulprit : public FArchive
{
public:
	FArchiveFindCulprit( UObject* InFind, UObject* Src )
	: Find(InFind), Count(0)
	{
		Src->Serialize( *this );
	}
	INT GetCount()
	{
		return Count;
	}
	FArchive& operator<<( class UObject*& Obj )
	{
		if( Obj==Find )
			Count++;
		return *this;
	}
protected:
	UObject* Find;
	INT Count;
};

//
// Archive for finding shortest path from root to a particular object.
// Depth-first search.
//
struct FTraceRouteRecord
{
	INT Depth;
	UObject* Referencer;
	FTraceRouteRecord( INT InDepth, UObject* InReferencer )
	: Depth(InDepth), Referencer(InReferencer)
	{}
};
class FArchiveTraceRoute : public FArchive
{
public:
	static TArray<UObject*> FindShortestRootPath( UObject* Obj )
	{
		guard(FArchiveTraceRoute::FindShortestRootPath);
		TMap<UObject*,FTraceRouteRecord> Routes;
		FArchiveTraceRoute Rt( Routes );
		TArray<UObject*> Result;
		if( Routes.Find(Obj) )
		{
			Result.AddItem( Obj );
			for( ; ; )
			{
				FTraceRouteRecord* Rec = Routes.Find(Obj);
				if( Rec->Depth==0 )
					break;
				Obj = Rec->Referencer;
				Result.Insert(0);
				Result(0) = Obj;
			}
		}
		return Result;
		unguard;
	}
private:
	FArchiveTraceRoute( TMap<UObject*,FTraceRouteRecord>& InRoutes )
	: Routes(InRoutes), Depth(0), Prev(NULL)
	{
		{for( FObjectIterator It; It; ++It )
			It->SetFlags( RF_TagExp );}
		UObject::SerializeRootSet( *this, RF_Native, 0 );
		{for( FObjectIterator It; It; ++It )
			It->ClearFlags( RF_TagExp );}
	}
	FArchive& operator<<( class UObject*& Obj )
	{
		if( Obj )
		{
			FTraceRouteRecord* Rec = Routes.Find(Obj);
			if( !Rec || Depth<Rec->Depth )
				Routes.Set( Obj, FTraceRouteRecord(Depth,Prev) );
		}
		if( Obj && (Obj->GetFlags() & RF_TagExp) )
		{
			Obj->ClearFlags( RF_TagExp );
			UObject* SavedPrev = Prev;
			Prev = Obj;
			Depth++;
			Obj->Serialize( *this );
			Depth--;
			Prev = SavedPrev;
		}
		return *this;
	}
	TMap<UObject*,FTraceRouteRecord>& Routes;
	INT Depth;
	UObject* Prev;
};

// rjp --
void UObject::execGetReferencers( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execGetReferencers);

	P_GET_OBJECT(UObject, Object);
	P_GET_TARRAY_REF(Refs, UObject*);
	P_FINISH;

	GetReferencers(Object, *Refs);
	unguardexec;
}
IMPLEMENT_FUNCTION( UObject, INDEX_NONE, execGetReferencers );

void UObject::GetShortestRoute( UObject* Obj, TArray<UObject*>& RouteObjects )
{
	guard(UObject::GetShortestRoute);

	if ( !Obj )
		return;

	RouteObjects = FArchiveTraceRoute::FindShortestRootPath(Obj);

	unguard;
}

void UObject::GetReferencers( UObject* Obj, TArray<UObject*>& Refs )
{
	guard(UObject::GetReferencers);

	Refs.Empty();
	if ( !Obj )
		return;

	for( FObjectIterator It; It; ++It )
	{
		FArchiveFindCulprit ArFind(Obj,*It);
		if( ArFind.GetCount() )
			Refs.AddItem(*It);
	}

	unguard;
}

// -- rjp



static void ListReferences( FOutputDevice& Ar, UObject* Obj, TArray<UObject*>& MasterList, FString& IndentText, INT MaxDepth = INDEX_NONE )
{
	if ( !Obj )
		return;

	static INT Depth = 0;

	Depth++;

	FArchiveListRefs ArList(Obj);
	if ( ArList.GetCount() )
	{
		MasterList.AddUniqueItem(Obj);
		Ar.Logf(TEXT("%s%s references:"), *IndentText, Obj->GetFullName());
		IndentText += TEXT("  ");

		const TArray<UObject*>& Objs = ArList.GetObjects();
		UObject* TempObj = NULL;
		for ( INT i = 0; i < Objs.Num(); i++ )
		{
			if ( Objs(i) && Objs(i) != Obj &&
				(Objs(i)->IsA(UClass::StaticClass()) || Objs(i)->GetClass()->IsChildOf(UClass::StaticClass())) 
				&& Objs(i) != Obj->GetClass() )
				Ar.Logf(TEXT("%s%s"), *IndentText, Objs(i)->GetFullName());
		}

		if ( MaxDepth == INDEX_NONE || Depth < MaxDepth )
		{
			for ( INT i = 0; i < Objs.Num(); i++ )
			{
				TempObj = Objs(i);
				if ( TempObj && TempObj != Obj && !FlagCast<UProperty,CLASS_IsAUProperty>(TempObj) &&
					TempObj != Obj->GetClass() && MasterList.FindItemIndex(TempObj) == INDEX_NONE )
					ListReferences(Ar,TempObj,MasterList,IndentText,MaxDepth);
			}
		}
	
		IndentText = IndentText.LeftChop(2);
	}

	Depth--;
}

//
// Show the inheretance graph of all loaded classes.
//
static void ShowClasses( UClass* Class, FOutputDevice& Ar, int Indent )
{
	Ar.Logf( TEXT("%s%s"), appSpc(Indent), Class->GetName() );
	for( TObjectIterator<UClass> It; It; ++It )
		if( It->GetSuperClass() == Class )
			ShowClasses( *It, Ar, Indent+2 );
}

struct FItem
{
	UClass*	Class;
	INT		Count;
	SIZE_T	Num, Max;
	FItem( UClass* InClass=NULL )
	: Class(InClass), Count(0), Num(0), Max(0)
	{}
	void Add( FArchiveCountMem& Ar )
	{Count++; Num+=Ar.GetNum(); Max+=Ar.GetMax();}
};
struct FSubItem
{
	UObject* Object;
	SIZE_T Num, Max;
	FSubItem( UObject* InObject, SIZE_T InNum, SIZE_T InMax )
	: Object( InObject ), Num( InNum ), Max( InMax )
	{}
};
static QSORT_RETURN CDECL CompareSubItems( const FSubItem* A, const FSubItem* B )
{
	return B->Max - A->Max;
}
static QSORT_RETURN CDECL CompareItems( const FItem* A, const FItem* B )
{
	return B->Max - A->Max;
}

UBOOL UObject::StaticExec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UObject::StaticExec);
	const TCHAR *Str = Cmd;
	if( ParseCommand(&Str,TEXT("MEM")) )
	{
		GMalloc->DumpAllocs();
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("DUMPNATIVES")) )
	{
		// Linux: Defined out because of error: "taking
		// the address of a bound member function to form
		// a pointer to member function, say '&Object::execUndefined'
		#if _MSC_VER
		for( INT i=0; i<EX_Max; i++ )
			if( GNatives[i] == &execUndefined )
				debugf( TEXT("Native index %i is available"), i );
		#endif
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("GET")) )
	{
		// Get a class default variable.
		TCHAR ClassName[256], PropertyName[256];
		UClass* Class;
		UProperty* Property;
		if
		(	ParseToken( Str, ClassName, ARRAY_COUNT(ClassName), 1 )
		&&	(Class=FindObject<UClass>( ANY_PACKAGE, ClassName))!=NULL )
		{
			if
			(	ParseToken( Str, PropertyName, ARRAY_COUNT(PropertyName), 1 )
			&&	(Property=FindField<UProperty>( Class, PropertyName))!=NULL )
			{
				TCHAR Temp[256]=TEXT("");
				if( Class->Defaults.Num() )
					Property->ExportText( 0, Temp, &Class->Defaults(0), &Class->Defaults(0), PPF_Localized );
				Ar.Log( Temp );
			}
			else Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized property %s"), PropertyName );
		}
		else Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized class %s"), ClassName );
		return 1;
	}
	else if ( ParseCommand(&Str, TEXT("GETALL")) ) // -- rjp
	{
		// Iterate through all objects of the specified type and return the value for each object
		TCHAR ClassName[256], PropertyName[256];
		UClass* Class;
		UProperty* Property;

		if ( ParseToken(Str,ClassName,ARRAY_COUNT(ClassName), 1) &&
			(Class=FindObject<UClass>( ANY_PACKAGE, ClassName)) != NULL )
		{
			if ( ParseToken(Str,PropertyName,ARRAY_COUNT(PropertyName),1) &&
				(Property=FindField<UProperty>(Class,PropertyName)) != NULL )
			{
				INT cnt = 0;
				UBOOL bNoClasses = ParseCommand(&Str,TEXT("HIDECLASSES"));
				for ( TObjectIterator<UObject> It; It; ++It )
				{
					static TCHAR Temp [256*256] = TEXT("");
					Temp[0] = 0;

					UClass* ClsIt = Cast<UClass>(*It);
					if ( !bNoClasses && ClsIt && (ClsIt == Class || ClsIt->IsChildOf(Class)) )
					{
						if ( ClsIt->Defaults.Num() )
						{
							TCHAR* c = Temp;

							if ( Property->ArrayDim > 1 )
								*c++ = '(';

							for ( INT i = 0; i < Property->ArrayDim; i++ )
							{
								if ( i > 0 )
								{
									c += appStrlen(c);
									appStrcat(c++,TEXT(","));
								}

								Property->ExportText( i, c, &ClsIt->Defaults(0), &ClsIt->Defaults(0), PPF_Localized );
							}

							if ( Property->ArrayDim > 0 && c > Temp )
							{
								c += appStrlen(c);
								*c++ = ')';
								*c++ = 0;
							}

							Ar.Logf(TEXT("%i) %s.%s = %s"), cnt++, ClsIt->GetFullName(), Property->GetName(), Temp);
						}
					}
					else if ( It->IsA(Class) )
					{
						TCHAR* c = Temp;
						if ( Property->ArrayDim > 1 )
							*c++ = '(';

						for ( INT i = 0; i < Property->ArrayDim; i++ )
						{
							if ( i > 0 )
							{
								c += appStrlen(c);
								appStrcat(c++,TEXT(","));
							}

							Property->ExportText( i, c, (BYTE*)*It, (BYTE*)*It, PPF_Localized );
						}

						if ( Property->ArrayDim > 1 && c > Temp )
						{
							c += appStrlen(c);
							*c++ = ')';
							*c++ = 0;
						}

						Ar.Logf(TEXT("%i) %s.%s = %s"), cnt++, It->GetFullName(), Property->GetName(), Temp);
					}
				}
			}
			else Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized property %s"), PropertyName );
		}
		else Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized class %s"), ClassName );
		return 1;
	}

	else if( ParseCommand(&Str,TEXT("SET")) )
	{
		// Set a class default variable.
		TCHAR ClassName[256], PropertyName[256];
		UClass* Class;
		UProperty* Property;
		if
		(	ParseToken( Str, ClassName, ARRAY_COUNT(ClassName), 1 )
		&&	(Class=FindObject<UClass>( ANY_PACKAGE, ClassName))!=NULL )
		{
			if
			(	ParseToken( Str, PropertyName, ARRAY_COUNT(PropertyName), 1 )
			&&	(Property=FindField<UProperty>( Class, PropertyName))!=NULL )
			{
				while( *Str==' ' )
					Str++;
				GlobalSetProperty( Str, Class, Property, Property->Offset, 1 );
			}
			else Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized property %s"), PropertyName );
		}
		else Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized class %s"), ClassName );
		return 1;
	}
	// gam ---
	else if( ParseCommand(&Str,TEXT("POKE")) )
	{
    	// eg: POKE Material AbaddonArchitecture.Base.wal52go SurfaceType 3

		TCHAR ClassName[256] = TEXT("");
		TCHAR ObjectName[256] = TEXT("");
		TCHAR PropertyName[256] = TEXT("");
		
		UClass* Class;
		UObject* Object;
		UProperty* Property;

		if(	!ParseToken( Str, ClassName, ARRAY_COUNT(ClassName), 1 ) )
		{
			Ar.Logf( NAME_ExecWarning, TEXT("No class specified!") );
			return 1;
	    }

		Class = FindObject<UClass>( ANY_PACKAGE, ClassName );
		
        if( !Class )		
		{
			Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized class %s"), ClassName );
			return 1;
	    }
	    
		if(	!ParseToken( Str, ObjectName, ARRAY_COUNT(ObjectName), 1 ) )
		{
			Ar.Logf( NAME_ExecWarning, TEXT("No object specified!") );
			return 1;
	    }

		Object = StaticFindObject( Class, NULL, ObjectName );
		
        if( !Object )		
		{
			Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized object %s"), ObjectName );
			return 1;
	    }

		if(	!ParseToken( Str, PropertyName, ARRAY_COUNT(PropertyName), 1 ) )
		{
			Ar.Logf( NAME_ExecWarning, TEXT("No property specified!") );
			return 1;
	    }

		Property = FindField<UProperty>( Class, PropertyName );
		
        if( !Property )		
		{
			Ar.Logf( NAME_ExecWarning, TEXT("Unrecognized property %s"), PropertyName );
			return 1;
	    }

		while( *Str==' ' )
			Str++;

		Property->ImportText( Str, (BYTE*)(Object) + Property->Offset, PPF_Localized );
		Object->PostEditChange();

        Ar.Logf( TEXT("POKED %s.%s"), ObjectName, PropertyName );

		return 1;
	}
	// --- gam
	else if( ParseCommand(&Str,TEXT("OBJ")) )
	{
		if( ParseCommand(&Str,TEXT("GARBAGE")) )
		{
			// Purge unclaimed objects.
			UBOOL GSavedNoGC=GNoGC;
			GNoGC = 0;
			CollectGarbage( RF_Native | (GIsEditor ? RF_Standalone : 0) );
			GNoGC = GSavedNoGC;
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("MARK")) )
		{
			debugf( TEXT("Marking objects") );
			for( FObjectIterator It; It; ++It )
				It->SetFlags( RF_Marked );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("MARKCHECK")) )
		{
			debugf( TEXT("Unmarked objects:") );
			for( FObjectIterator It; It; ++It )
				if( !(It->GetFlags() & RF_Marked) )
					debugf( TEXT("%s"), It->GetFullName() );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("REFS")) )
		{
			UClass* Class;
			UObject* Object;
			if
			(	ParseObject<UClass>( Str, TEXT("CLASS="), Class, ANY_PACKAGE )
			&&	ParseObject(Str,TEXT("NAME="), Class, Object, ANY_PACKAGE ) )
			{
				Ar.Logf( TEXT("") );
				Ar.Logf( TEXT("Referencers of %s:"), Object->GetFullName() );
				for( FObjectIterator It; It; ++It )
				{
					FArchiveFindCulprit ArFind(Object,*It);
					if( ArFind.GetCount() )
						Ar.Logf( TEXT("   %s (%i)"), It->GetFullName(), ArFind.GetCount() );
				}
				Ar.Logf(TEXT("") );
				Ar.Logf(TEXT("Shortest reachability from root to %s:"), Object->GetFullName() );
				TArray<UObject*> Rt = FArchiveTraceRoute::FindShortestRootPath(Object);
				for( INT i=0; i<Rt.Num(); i++ )
					Ar.Logf(TEXT("   %s%s"), Rt(i)->GetFullName(), i!=0 ? TEXT("") : (Rt(i)->GetFlags()&RF_Native)?TEXT(" (native)"):TEXT(" (root)") );
				if( !Rt.Num() )
					Ar.Logf(TEXT("   (Object is not currently rooted)"));
				Ar.Logf(TEXT("") );
			}
			return 1;
		}
		else if ( ParseCommand(&Str, TEXT("REFMAP")) )
		{
			TCHAR ObjectName[256];
			UClass* Class;
			UObject* Obj=NULL;
			if ( !ParseObject<UClass>( Str, TEXT("CLASS="), Class, ANY_PACKAGE ) || !ParseObject(Str,TEXT("NAME="), Class, Obj, ANY_PACKAGE) )
			{
				if ( !ParseToken(Str,ObjectName,ARRAY_COUNT(ObjectName), 1) )
					debugf(TEXT("Invalid parameters for REFMAP!"));

                Obj = FindObject<UObject>(ANY_PACKAGE,ObjectName);
			}

			FString IndentText = TEXT("  ");
			if ( Obj )
			{
				INT MaxDepth = INDEX_NONE;
				if ( !Parse(Str,TEXT("DEPTH="), MaxDepth) )
					MaxDepth = INDEX_NONE;

				TArray<UObject*> MasterList;
				ListReferences(Ar, Obj, MasterList, IndentText, MaxDepth);
				Ar.Logf(TEXT(" Total number of objects referenced directly or indirectly by %s: %i"), Obj->GetFullName(), MasterList.Num());
			}
			else Ar.Logf(TEXT(" No %s objects found!"), ObjectName);

			return 1;
		}
		else if( ParseCommand(&Str,TEXT("HASH")) )
		{
			// Hash info.
			FName::DisplayHash( Ar );
			INT ObjCount=0, HashCount=0;
			for( FObjectIterator It; It; ++It )
				ObjCount++;
			for( INT i=0; i<ARRAY_COUNT(GObjHash); i++ )
			{
				INT c=0;
				for( UObject* Hash=GObjHash[i]; Hash; Hash=Hash->HashNext )
					c++;
				if( c )
					HashCount++;
				//debugf( "%i: %i", i, c );
			}
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("CLASSES")) )
		{
			ShowClasses( StaticClass(), Ar, 0 );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("DEPENDENCIES")) )
		{
			UPackage* Pkg;
			if( ParseObject<UPackage>(Str,TEXT("PACKAGE="),Pkg,NULL) )
			{
				TArray<UObject*> Exclude;
				for( int i=0; i<16; i++ )
				{
					TCHAR Temp[32];
					appSprintf( Temp, TEXT("EXCLUDE%i="), i );
					FName F;
					if( Parse(Str,Temp,F) )
						Exclude.AddItem( CreatePackage(NULL,*F) );
				}
				Ar.Logf( TEXT("Dependencies of %s:"), Pkg->GetPathName() );
				for( FObjectIterator It; It; ++It )
					if( It->GetOuter()==Pkg )
						FArchiveShowReferences ArShowReferences( Ar, Pkg, *It, Exclude );
			}
			return 1;
		}
        else if( ParseCommand(&Str,TEXT("ISACOUNT")) ) // sjs temp
        {
            #if TRACK_ISA
            for( TObjectIterator<UClass> It; It; ++It )
            {
                if( It->IsACount > 0 )
                    debugf(TEXT("%s IsA queried: %d"), It->GetName(), It->IsACount );
            }
            #else
            debugf(TEXT("TRACK_ISA not define for this build.") );
            #endif
            return 1;
        }
        else if( ParseCommand(&Str,TEXT("ISARESET")) ) // sjs temp
        {
            #if TRACK_ISA
            for( TObjectIterator<UClass> It; It; ++It )
            {
                It->IsACount = 0;
            }
            #else
            debugf(TEXT("TRACK_ISA not define for this build.") );
            #endif
            return 1;
        }
		else if( ParseCommand(&Str,TEXT("LIST")) )
		{
			Ar.Log( TEXT("Objects:") );
			Ar.Log( TEXT("") );
			UClass*   CheckType     = NULL;
			UPackage* CheckPackage  = NULL;
			UPackage* InsidePackage = NULL;
			ParseObject<UClass>  ( Str, TEXT("CLASS="  ), CheckType,     ANY_PACKAGE );
			ParseObject<UPackage>( Str, TEXT("PACKAGE="), CheckPackage,  NULL );
			ParseObject<UPackage>( Str, TEXT("INSIDE=" ), InsidePackage, NULL );
			TArray<FItem> List;
			TArray<FSubItem> Objects;
			FItem Total;

			UBOOL ClassSpecified = appStrfind(Str,TEXT("CLASS=")) != NULL;
			for( FObjectIterator It; It; ++It )
			{
			    // gam ---
			    if( CheckType && !It->IsA(CheckType) )
			        continue;

			    if( CheckPackage && (It->GetOuter()!=CheckPackage) )
			        continue;

			    if( InsidePackage && !It->IsIn(InsidePackage) )
			        continue;
			    // --- gam

				if ( ClassSpecified && !CheckType )
					continue;

					FArchiveCountMem Count( *It );
					INT i;
					for( i=0; i<List.Num(); i++ )
						if( List(i).Class == It->GetClass() )
							break;
					if( i==List.Num() )
						i = List.AddItem(FItem( It->GetClass() ));
					if( CheckType || CheckPackage || InsidePackage )
						new(Objects)FSubItem( *It, Count.GetNum(), Count.GetMax() );
					List(i).Add( Count );
					Total.Add( Count );
			}
			if( Objects.Num() )
			{
				appQsort( &Objects(0), Objects.Num(), sizeof(Objects(0)), (QSORT_COMPARE)CompareSubItems );
				Ar.Logf( TEXT("%60s % 10s % 10s"), TEXT("Object"), TEXT("NumBytes"), TEXT("MaxBytes") );
				for( INT i=0; i<Objects.Num(); i++ )
					Ar.Logf( TEXT("%60s % 10i % 10i"), Objects(i).Object->GetFullName(), Objects(i).Num, Objects(i).Max );
				Ar.Log( TEXT("") );
			}
			if( List.Num() )
			{
				appQsort( &List(0), List.Num(), sizeof(List(0)), (QSORT_COMPARE)CompareItems );
				Ar.Logf(TEXT(" %30s % 6s % 10s  % 10s "), TEXT("Class"), TEXT("Count"), TEXT("NumBytes"), TEXT("MaxBytes") );
				for( INT i=0; i<List.Num(); i++ )
					Ar.Logf(TEXT(" %30s % 6i % 10iK % 10iK"), List(i).Class->GetName(), List(i).Count, List(i).Num/1024, List(i).Max/1024 );
				Ar.Log( TEXT("") );
			}
			Ar.Logf( TEXT("%i Objects (%.3fM / %.3fM)"), Total.Count, (FLOAT)Total.Num/1024.0/1024.0, (FLOAT)Total.Max/1024.0/1024.0 );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("VFHASH")) )
		{
			Ar.Logf( TEXT("Class VfHashes:") );
			for( TObjectIterator<UState> It; It; ++It )
			{
				Ar.Logf( TEXT("%s:"), It->GetName() );
				for( INT i=0; i<UField::HASH_COUNT; i++ )
				{
					INT c=0;
					for( UField* F=It->VfHash[i]; F; F=F->HashNext )
						c++;
					Ar.Logf( TEXT("   %i: %i"), i, c );
				}
			}
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("LINKERS")) )
		{
			Ar.Logf( TEXT("Linkers:") );
			for( INT i=0; i<GObjLoaders.Num(); i++ )
			{
				ULinkerLoad* Linker = CastChecked<ULinkerLoad>( GObjLoaders(i) );
				INT NameSize = 0;
				for( INT i=0; i<Linker->NameMap.Num(); i++ )
					if( Linker->NameMap(i) != NAME_None )
						NameSize += (INT) (sizeof(FNameEntry) - (NAME_SIZE - appStrlen(*Linker->NameMap(i)) - 1) * sizeof(TCHAR));
				Ar.Logf
				(
					TEXT("%s (%s): Names=%i (%iK/%iK) Imports=%i (%iK) Exports=%i (%iK) Gen=%i Lazy=%i"),
					*Linker->Filename,
					Linker->LinkerRoot->GetFullName(),
					Linker->NameMap.Num(),
					Linker->NameMap.Num() * sizeof(FName) / 1024,
					NameSize / 1024,
					Linker->ImportMap.Num(),
					Linker->ImportMap.Num() * sizeof(FObjectImport) / 1024,
					Linker->ExportMap.Num(),
					Linker->ExportMap.Num() * sizeof(FObjectExport) / 1024,
					Linker->Summary.Generations.Num(),
					Linker->LazyLoaders.Num()
				);
			}
			return 1;
		}

		// rjp --
		else if ( ParseCommand(&Str,TEXT("DUMP")) )
		{
			// Dump all variable values for the specified object
			// supports specifying categories to hide or show
			// OBJ DUMP playercontroller0 hide=actor,object,lighting,movement     OR
			// OBJ DUMP playercontroller0 show=playercontroller,controller        OR
			// OBJ DUMP class=playercontroller name=playercontroller0 show=object OR
			// OBJ DUMP playercontroller0 recurse=true
			TCHAR ObjectName[256];
			UObject* Obj = NULL;
			UClass* Cls = NULL;

			TArray<FString> HiddenCategories, ShowingCategories;

			if ( !ParseObject<UClass>( Str, TEXT("CLASS="), Cls, ANY_PACKAGE ) || !ParseObject(Str,TEXT("NAME="), Cls, Obj, ANY_PACKAGE) )
			{
				if ( ParseToken(Str,ObjectName,ARRAY_COUNT(ObjectName), 1) )
					Obj = FindObject<UObject>(ANY_PACKAGE,ObjectName);
			}


			if ( Obj )
			{
				TCHAR* Value = new TCHAR [256 * 1024];

				Ar.Logf(TEXT(""));

				UBOOL Recurse = Parse(Str, TEXT("RECURSE=TRUE"), Value, 15);
				Ar.Logf(TEXT("*** Property dump for object %s'%s' ***"), Recurse ? TEXT("(Recursive) ") : TEXT(""), Obj->GetFullName() );

				if ( Recurse )
					ExportProperties(Ar,Obj->GetClass(), (BYTE*)Obj, 0, NULL, NULL);
				else
				{
					if ( Parse(Str, TEXT("HIDE="), Value, 1024, TEXT(",")) )
						FString(Value).ParseIntoArray(TEXT(","), &HiddenCategories);

					else if ( Parse(Str, TEXT("SHOW="), Value, 1024, TEXT(",")) )
						FString(Value).ParseIntoArray(TEXT(","), &ShowingCategories);

					for ( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Obj->GetClass()); It; ++It )
					{
						if ( It->Port() )
						{
							if ( HiddenCategories.Num() )
							{
								INT i;
								for ( i = 0; i < HiddenCategories.Num(); i++ )
								{
									if ( It->Category != NAME_None && HiddenCategories(i) == *It->Category )
										break;

									if ( HiddenCategories(i) == It->GetOwnerClass()->GetName() )
										break;
								}

								if ( i < HiddenCategories.Num() )
									continue;
							}

							else if ( ShowingCategories.Num() )
							{
								INT i;
								for ( i = 0; i < ShowingCategories.Num(); i++ )
								{
									if ( It->Category != NAME_None && ShowingCategories(i) == *It->Category )
										break;
									
									if ( ShowingCategories(i) == It->GetOwnerClass()->GetName() )
										break;
								}

								if ( i == ShowingCategories.Num() )
									continue;
							}

							if ( It->ArrayDim > 1 )
							{
								for ( INT i = 0; i < It->ArrayDim; i++ )
								{
									It->ExportText(i, Value, (BYTE*)Obj, (BYTE*)Obj, PPF_Localized);
									Value[960] = 0;
									Ar.Logf(TEXT("  %s[%i]=%s"), It->GetName(), i, Value);
								}
							}
							else
							{
								if ( It->IsA(UArrayProperty::StaticClass() ) )
								{
									UArrayProperty* ArrayProp = Cast<UArrayProperty>(*It);
									FArray* Array       = (FArray*)((BYTE*)Obj + ArrayProp->Offset);
									INT     ElementSize = ArrayProp->Inner->ElementSize;
									for( INT i=0; i<Array->Num(); i++ )
									{
										ArrayProp->Inner->ExportTextItem( Value, (BYTE*)Array->GetData() + i * ElementSize, NULL, PPF_Localized );
										Value[960] = 0;
										Ar.Logf(TEXT("  %s(%i)=%s"), ArrayProp->GetName(), i, Value);
									}
								}

								else
								{
									It->ExportText(0, Value, (BYTE*)Obj, (BYTE*)Obj, PPF_Localized);
									Value[960] = 0;
									Ar.Logf(TEXT("  %s=%s"), It->GetName(), Value);
								}
							}
						}
					}
				}
				delete [] Value;

			}
			else Ar.Logf(NAME_ExecWarning, TEXT("No objects found using command '%s'"), *Str);

			return 1;
		}
		// -- rjp

		else return 0;
	}
	else if( ParseCommand(&Str,TEXT("GTIME")) )
	{
		debugf( TEXT("GTime = %f"), GTempDouble );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("PROFILESCRIPT")) )
	{
		if( ParseCommand(&Str,TEXT("START")) )
		{
			delete GScriptCallGraph;
			GScriptCallGraph = new FScriptCallGraph( 32 * 1024 * 1024 );
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("STOP")) )
		{
			if( GScriptCallGraph )
			{
				GScriptCallGraph->EmitEnd();

				INT Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec;
				appSystemTime( Year, Month, DayOfWeek, Day, Hour, Min, Sec, MSec );
				FString	Filename = FString::Printf(TEXT("ut2004-%i-%i-%i_%i-%i-%i.uprof"), Year, Month, Day, Hour, Min, Sec );

				FArchive* Archive = GFileManager->CreateFileWriter( *Filename );
				if( Archive )
				{
					GScriptCallGraph->Serialize( *Archive );
					delete Archive;
				}

				delete GScriptCallGraph;
				GScriptCallGraph = NULL;
			}
			return 1;
		}
		else if( ParseCommand(&Str,TEXT("RESET"))  )
		{
			if( GScriptCallGraph )
				GScriptCallGraph->Reset();
			return 1;
		}
		return 0;
	}
	else return 0; // Not executed

	unguard;
}

/*-----------------------------------------------------------------------------
   File loading.
-----------------------------------------------------------------------------*/

//
// Safe load error-handling.
//
void UObject::SafeLoadError( DWORD LoadFlags, const TCHAR* Error, const TCHAR* Fmt, ... )
{
	// Variable arguments setup.
	TCHAR TempStr[4096];
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt, Fmt );

	guard(UObject::SafeLoadError);
	if( !(LoadFlags & LOAD_Quiet)  ) debugf( NAME_Warning, TEXT("%s"), TempStr );
#ifndef __EMSCRIPTEN__
	if(   LoadFlags & LOAD_Throw   ) appThrowf( TEXT("%s"), Error   );
#else
	// Emscripten: do NOT re-throw on LOAD_Throw. C++ exceptions cannot unwind
	// across Asyncify-transformed frames, so a re-thrown load error escapes every
	// caller's catch and aborts the WASM module. Each loader (GetPackageLinker,
	// StaticLoadObject) already caught the error in its own LOCAL catch before
	// calling us; not re-throwing lets the load resolve to NULL, which callers
	// (e.g. UGameEngine::LoadMap) turn into a graceful "could not load map".
	(void)Error;
#endif
	if(   LoadFlags & LOAD_NoFail  ) appErrorf( TEXT("%s"), TempStr );
	if( !(LoadFlags & LOAD_NoWarn) ) GWarn->Logf( TEXT("%s"), TempStr );
	unguard;
}

//
// Find or create the linker for a package.
//
ULinkerLoad* UObject::GetPackageLinker
(
	UObject*		InOuter,
	const TCHAR*	InFilename,
	DWORD			LoadFlags,
	UPackageMap*	Sandbox,
	FGuid*			CompatibleGuid,
	INT				GenerationLevel
)
{
	guard(UObject::GetPackageLinker);
	check(GObjBeginLoadCount);

	// See if there is already a linker for this package.
	ULinkerLoad* Result = NULL;
	if( InOuter )
		for( INT i=0; i<GObjLoaders.Num() && !Result; i++ )
			if( GetLoader(i)->LinkerRoot == InOuter )
				Result = GetLoader( i );

	// Try to load the linker.
	try
	{
		// See if the linker is already loaded.
		TCHAR NewFilename[256]=TEXT("");
		if( Result )
		{
			// Linker already found.
			appStrcpy( NewFilename, TEXT("") );
		}
		else if( !InFilename )
		{
			// Resolve filename from package name.
			if( !InOuter )
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
				return NULL;
#else
				appThrowf( LocalizeError(TEXT("PackageResolveFailed"),TEXT("Core")) );
#endif
			if( !appFindPackageFile( InOuter->GetName(), CompatibleGuid, NewFilename, GenerationLevel ) )
			{
				// See about looking in the dll.
				if( (LoadFlags & LOAD_AllowDll) && InOuter->IsA(UPackage::StaticClass()) && ((UPackage*)InOuter)->DllHandle )
					return NULL;
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
				debugf( NAME_Warning, TEXT("Package not found: %s (returning NULL, not throwing)"), InOuter->GetName() );
				return NULL;
#else
				appThrowf( LocalizeError(TEXT("PackageNotFound"),TEXT("Core")), InOuter->GetName() );
#endif
			}
		}
		else
		{
			// Verify that the file exists.
			if( !appFindPackageFile( InFilename, CompatibleGuid, NewFilename, GenerationLevel ) )
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
			{
				// Return NULL instead of throwing. C++ exceptions cannot unwind across
				// this Asyncify-instrumented frame (it does async file I/O via
				// appFindPackageFile), so even our own catch below won't fire — the
				// throw would escape uncaught and abort the module. Callers handle a
				// NULL linker (→ NULL object → graceful "could not load map").
				debugf( NAME_Warning, TEXT("Package file not found: %s (returning NULL, not throwing)"), InFilename );
				return NULL;
			}
#else
				appThrowf( LocalizeError(TEXT("FileNotFound"),TEXT("Core")), InFilename );
#endif

			// Resolve package name from filename.
			TCHAR Tmp[256], *T=Tmp;
			appStrncpy( Tmp, InFilename, ARRAY_COUNT(Tmp) );
			while( 1 )
			{
				if( appStrstr(T,PATH_SEPARATOR) )
					T = appStrstr(T,PATH_SEPARATOR)+appStrlen(PATH_SEPARATOR);
				else if( appStrstr(T,TEXT("/")) )
					T = appStrstr(T,TEXT("/"))+1;
				else if( appStrstr(T,TEXT(":")) )
					T = appStrstr(T,TEXT(":"))+1;
				else
					break;
			}
			if( appStrstr(T,TEXT(".")) )
				*appStrstr(T,TEXT(".")) = 0;
			UPackage* FilenamePkg = CreatePackage( NULL, T );

			// If no package specified, use package from file.
			if( InOuter==NULL )
			{
				if( !FilenamePkg )
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
				{
					debugf( NAME_Warning, TEXT("[PACKAGE-LOAD] Could not derive a package name from file '%s'"), InFilename );
					return NULL;
				}
#else
					appThrowf( LocalizeError(TEXT("FilenameToPackage"),TEXT("Core")), InFilename );
#endif
				InOuter = FilenamePkg;
				for( INT i=0; i<GObjLoaders.Num() && !Result; i++ )
					if( GetLoader(i)->LinkerRoot == InOuter )
						Result = GetLoader(i);
			}
			else if( InOuter != FilenamePkg )//!!should be tested and validated in new UnrealEd
			{
				// Loading a new file into an existing package, so reset the loader.
				debugf( TEXT("New File, Existing Package (%s, %s)"), InOuter->GetFullName(), FilenamePkg->GetFullName() );
				ResetLoaders( InOuter, 0, 1 );
			}
		}

		// Make sure the package is accessible in the sandbox.
		if( Sandbox && !Sandbox->SupportsPackage(InOuter) )
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
		{
			debugf( NAME_Warning, TEXT("[PACKAGE-LOAD] Package '%s' is not allowed in this context (sandbox)"), InOuter->GetName() );
			return NULL;
		}
#else
			appThrowf( LocalizeError(TEXT("Sandbox"),TEXT("Core")), InOuter->GetName() );
#endif

		// Create new linker.
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
		const UBOOL FreshLinker = !Result;	// only a linker made here may be dropped below
#endif
		if( !Result )
		{
			Result = new ULinkerLoad( InOuter, NewFilename, LoadFlags );
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
			// The ULinkerLoad constructor fails soft here: a corrupt / incompatible / unopenable
			// package leaves Success != 1 and logs a message at the failure point. Drop the
			// half-built linker and return NULL (a failed load, not an abort). Only a freshly
			// created linker: a found-existing Result may legitimately be mid-construction
			// (Success == sentinel) during nested or circular imports.
			if( Result && Result->Success != 1 )
			{
				GObjLoaders.RemoveItem( Result );
				return NULL;
			}
#endif
		}

		// Verify compatibility.
		if( CompatibleGuid && Result->Summary.Guid!=*CompatibleGuid )
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
		{
			debugf( NAME_Warning, TEXT("[PACKAGE-LOAD] '%s' is an incompatible version of package '%s' (GUID mismatch)"), NewFilename, InOuter->GetName() );
			// Drop the linker only if it was made here: one that was already there belongs to a package in
			// use (a server naming a mod the menus have loaded in another version), and taking it out of
			// GObjLoaders would orphan every object loaded through it. Natively the throw leaves it alone too.
			if( FreshLinker )
				GObjLoaders.RemoveItem( Result );
			return NULL;
		}
#else
			appThrowf( LocalizeError(TEXT("PackageVersion"),TEXT("Core")), InOuter->GetName() );
#endif
	}
	catch( const TCHAR* Error )
	{
		// If we're in the editor (and not running from UCC) we don't want this to be a fatal error.
		if( GIsEditor && !GIsUCC )
		{
			EdLoadErrorf( FEdLoadError::TYPE_FILE, InFilename ? InFilename : InOuter ? InOuter->GetPathName() : TEXT("NULL") );
			debugf( LocalizeError(TEXT("FailedLoad"),TEXT("Core")), InFilename ? InFilename : InOuter ? InOuter->GetName() : TEXT("NULL"), Error);
		}
		else
			SafeLoadError( LoadFlags, Error, LocalizeError(TEXT("FailedLoad"),TEXT("Core")), InFilename ? InFilename : InOuter ? InOuter->GetName() : TEXT("NULL"), Error );
	}

	// Success.
	return Result;
	unguard;
}

//
// Find or optionally create a package.
//
UPackage* UObject::CreatePackage( UObject* InOuter, const TCHAR* InName )
{
	guard(UObject::CreatePackage);

	ResolveName( InOuter, InName, 1, 0 );
	UPackage* Result = FindObject<UPackage>( InOuter, InName );
	if( !Result )
		Result = new( InOuter, InName, RF_Public )UPackage;
	return Result;

	unguard;
}

//
// Resolve a package and name.
//
UBOOL UObject::ResolveName( UObject*& InPackage, const TCHAR*& InName, UBOOL Create, UBOOL Throw )
{
	guard(UObject::ResolveName);
	check(InName);

	// See if the name is specified in the .ini file.
	UBOOL SystemIni = (appStrnicmp( InName, TEXT("ini:"), 4 )==0);
	UBOOL UserIni   = (appStrnicmp( InName, TEXT("usr:"), 4 )==0);

	UBOOL bAddToNameTable = false; // OP ADDITION [20-08-2003]

	if( (SystemIni||UserIni) && appStrlen(InName)<1024 && appStrstr(InName,TEXT(".")) )
	{
		// Get .ini key and section.
		TCHAR Section[256];
		appStrncpy( Section, InName+4, ARRAY_COUNT(Section)-1 );
		TCHAR* Key = Section;
		while( appStrstr(Key,TEXT(".")) )
			Key = appStrstr(Key,TEXT("."))+1;
		check(Key!=Section);
		Key[-1] = 0;

		// Look up name.
		TCHAR* Temp = appStaticString1024();
		if( !GConfig->GetString( Section, Key, Temp, 1024, SystemIni?TEXT("System"):TEXT("User") ) )
		{
			if( Throw )
				appThrowf( LocalizeError(TEXT("ConfigNotFound"),TEXT("Core")), InName );
			return 0;
		}
		InName = Temp;

		bAddToNameTable = true; // OP ADDITION [20-08-2003]

	}

	// Don't allow names with a % in them

	if ( appStrstr(InName,TEXT("%")) )
		return 0;

	// Handle specified packages.
	while( appStrstr(InName,TEXT(".")) )
	{
		if ( InName[0] == '.' || InName[appStrlen(InName)-1] == '.' )
			return 0;

		TCHAR PartialName[256];
		appStrncpy( PartialName, InName, ARRAY_COUNT(PartialName)-1 );

		TCHAR* Work = appStrstr( PartialName, TEXT(".") );
		if (  Work == NULL )
		{
			return 0;
		}
		else
		{
			*Work = 0;
		}

		if( Create )
		{
			InPackage = CreatePackage( InPackage, PartialName );
		}
		else
		{
			UObject* NewPackage = FindObject<UPackage>( InPackage, PartialName );
			if( !NewPackage )
			{
				NewPackage = FindObject<UObject>( InPackage, PartialName );
//				if ( !NewPackage )
//					return 0;
				if( !NewPackage )
				{
					// If InPackage was specified, and object wasn't found, return a failure
					if ( InPackage )
						return 0;
					
					// If no InPackage was specified, attempt to find the object in any package
					// This should only occur when searching for subobjects using ClassName.SubObjectName,
					// instead of PackageName.ClassName.SubObjectName
					NewPackage = FindObject<UObject>( ANY_PACKAGE, PartialName );
					if ( !NewPackage )
						return 0;

					// For tracking what this change affects
					debugfSlow(TEXT("Change to ResolveName() made Object reference search for %s succeed!"), InName);
				}
			}
			InPackage = NewPackage;
		}
		InName = appStrstr(InName,TEXT("."))+1;
	}
	// OP ADDITION START [20-08-2003]
	if (bAddToNameTable)
		InName = *FName(InName);
	// OP ADDITION END [20-08-2003]

	return 1;
	unguard;
}

//
// Load an object.
//
UObject* UObject::StaticLoadObject( UClass* ObjectClass, UObject* InOuter, const TCHAR* InName, const TCHAR* Filename, DWORD LoadFlags, UPackageMap* Sandbox )
{
	guard(UObject::StaticLoadObject);
	check(ObjectClass);
	check(InName);

	debugf(NAME_DevLoad, TEXT("Loading object '%s'  class '%s'"), InName, ObjectClass->GetName());

	// Try to load.
	UObject* Result=NULL;
	BeginLoad();
	try
	{
		// Create a new linker object which goes off and tries load the file.
		ULinkerLoad* Linker = NULL;
		ResolveName( InOuter, InName, 1, 1 );
		UObject*	TopOuter = InOuter;
		while( TopOuter && TopOuter->GetOuter() )//!!can only load top-level packages from files
			TopOuter = TopOuter->GetOuter();
		if( !(LoadFlags & LOAD_DisallowFiles) )
			Linker = GetPackageLinker( TopOuter, Filename, LoadFlags | LOAD_Throw | LOAD_AllowDll, Sandbox, NULL );
		//!!this sucks because it supports wildcard sub-package matching of InName, which requires a long search.
		//!!also because linker classes require exact match
		if( Linker )
			Result = Linker->Create( ObjectClass, InName, LoadFlags, 0 );
		if( !Result )
			Result = StaticFindObject( ObjectClass, InOuter, InName );
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
		if( !Result )
		{
			// Don't throw — the catch below can't fire across Asyncify frames, so the
			// throw would escape uncaught and abort the module. Return NULL; callers
			// (DynamicLoadObject → None, LoadMap → "could not load map") handle it.
			EndLoad();
			return NULL;
		}
#else
		if( !Result )
			appThrowf( LocalizeError(TEXT("ObjectNotFound"),TEXT("Core")), ObjectClass->GetName(), InOuter ? InOuter->GetPathName() : TEXT("None"), InName );
#endif
		EndLoad();
	}
	catch( const TCHAR* Error )
	{
		EndLoad();
/*
		if( InOuter && !(LoadFlags & LOAD_NoRemap) && !Filename )
		{
			// Try loading a remapped package.
			TArray<FName> Remaps;
			GObjPackageRemap->MultiFind( InOuter->GetFName(), Remaps );
			for( INT i=0; i<Remaps.Num(); i++ )
			{
				InOuter = CreatePackage( NULL, *Remaps(i) );
				Result  = StaticLoadObject( ObjectClass, InOuter, InName, Filename, LoadFlags|LOAD_NoRemap, Sandbox );
				if( Result )
					return Result;
			}
		}
*/
			SafeLoadError( LoadFlags, Error, LocalizeError(TEXT("FailedLoadObject"),TEXT("Core")), ObjectClass->GetName(), InOuter ? InOuter->GetPathName() : TEXT("None"), InName, Error );
	}
	catch( ... )
	{
		// Emscripten's JS-based exception handling may not match const TCHAR* catches.
		// Catch-all as fallback to prevent unhandled exception crashes.
		EndLoad();
		SafeLoadError( LoadFlags, TEXT("Unknown error"), LocalizeError(TEXT("FailedLoadObject"),TEXT("Core")), ObjectClass->GetName(), InOuter ? InOuter->GetPathName() : TEXT("None"), InName, TEXT("Unknown error") );
	}
	return Result;
	unguardf(( TEXT("(%s %s.%s %s)"), ObjectClass->GetPathName(), InOuter ? InOuter->GetPathName() : TEXT("None"), InName, Filename ? Filename : TEXT("NULL") ));
}

//
// Load a class.
//
UClass* UObject::StaticLoadClass( UClass* BaseClass, UObject* InOuter, const TCHAR* InName, const TCHAR* Filename, DWORD LoadFlags, UPackageMap* Sandbox )
{
	guard(UObject::StaticLoadClass);
	check(BaseClass);
	try
	{
		UClass* Class = LoadObject<UClass>( InOuter, InName, Filename, LoadFlags | LOAD_Throw, Sandbox );
		if( Class && !Class->IsChildOf(BaseClass) )
			appThrowf( LocalizeError(TEXT("LoadClassMismatch"),TEXT("Core")), Class->GetFullName(), BaseClass->GetFullName() );
		return Class;
	}
	catch( const TCHAR* Error )
	{
		// Failed.
		SafeLoadError(LoadFlags, Error, TEXT("%s"), Error);
		return NULL;
	}
	unguard;
}

//
// Load all objects in a package.
//
UObject* UObject::LoadPackage( UObject* InOuter, const TCHAR* Filename, DWORD LoadFlags )
{
	guard(UObject::LoadPackage);
	UObject* Result;

    // gam ---
    if( appStrlen( Filename ) == 0 )
        return( NULL );
    // --- gam

	// Try to load.
	BeginLoad();
	try
	{
		// Create a new linker object which goes off and tries load the file.
		ULinkerLoad* Linker = GetPackageLinker( InOuter, Filename ? Filename : InOuter->GetName(), LoadFlags | LOAD_Throw, NULL, NULL );

        // gam ---
        if( !Linker )
        {
            // Balance the BeginLoad() above: GetPackageLinker returns NULL instead of throwing on the
            // fail-soft builds, and without EndLoad() GObjBeginLoadCount would stay at 1 and trip
            // check(GObjBeginLoadCount==0) in StaticTick() on the next frame.
            EndLoad();
            return( NULL );
        }
        // --- gam

		if( !(LoadFlags & LOAD_Verify) )
			Linker->LoadAllObjects();
		Result = Linker->LinkerRoot;
		EndLoad();
	}
	catch( const TCHAR* Error )
	{
		EndLoad();
		SafeLoadError( LoadFlags, Error, LocalizeError(TEXT("FailedLoadPackage"),TEXT("Core")), Error );
		Result = NULL;
	}
	return Result;
	unguard;
}

//
// Verify a linker.
//
void UObject::VerifyLinker( ULinkerLoad* Linker )
{
	guard(UObject::VerifyLinker);
	Linker->Verify();
	unguard;
}

//
// Begin loading packages.
//warning: Objects may not be destroyed between BeginLoad/EndLoad calls.
//
void UObject::BeginLoad()
{
	guard(UObject::BeginLoad);
	if( ++GObjBeginLoadCount == 1 )
	{
		// Validate clean load state.
		//!!needed? check(GObjLoaded.Num()==0);
		check(!GAutoRegister);
		for( INT i=0; i<GObjLoaders.Num(); i++ )
			check(GetLoader(i)->Success);
	}
	unguard;
}

static inline int Compare( const UObject* T1, const UObject* T2 )
{
	ULinker* L1 = T1->GetLinker();
	ULinker* L2 = T2->GetLinker();

	if( !L1 || !L2 || L1!=L2 )
		return L1 - L2;
    
	FObjectExport& E1 = L1->ExportMap( T1->GetLinkerIndex() );
	FObjectExport& E2 = L2->ExportMap( T2->GetLinkerIndex() );
	return E1.SerialOffset - E2.SerialOffset;	
}

//
// End loading packages.
//
void UObject::EndLoad()
{
	guard(UObject::EndLoad);
	check(GObjBeginLoadCount>0);
	if( --GObjBeginLoadCount == 0 )
	{
		try
		{
			TArray<UObject*>	ObjLoaded;

			while(GObjLoaded.Num())
			{
				appMemcpy(&ObjLoaded(ObjLoaded.Add(GObjLoaded.Num())),&GObjLoaded(0),GObjLoaded.Num() * sizeof(UObject*));
				GObjLoaded.Empty();

				// Sort by file and seek
				Sort( &ObjLoaded(0), ObjLoaded.Num() );

				// Finish loading everything.
				//warning: Array may expand during iteration.
				guard(PreLoadObjects);
				debugfSlow( NAME_DevLoad, TEXT("Loading objects...") );
				// From here until the loop ends, ObjLoaded holds objects that exist in the object table and
				// have not been serialized (GObjBeginLoadCount is already 0, so it cannot be the signal).
				// Saved and restored rather than assigned: EndLoad loops (`if(GObjLoaded.Num()) continue;`)
				// and can re-enter.
				const UBOOL WasPreloading = GObjPreloading;
				GObjPreloading = 1;
#if __PSX2_EE__ // add the cool endLoad printout
			for( INT i=0,n=0; i<ObjLoaded.Num(); i++ )
			{
				if( i==n )
				{
					n=ObjLoaded.Num();
					debugf(TEXT("endLoad: %i"),n-i);
				}
				// Preload.
				UObject* Obj = ObjLoaded(i);
				{
					if( Obj->GetFlags() & RF_NeedLoad )
					{
						check(Obj->GetLinker());
						Obj->GetLinker()->Preload( Obj );
					}
				}
			}
#else
			for( INT i=0; i<ObjLoaded.Num(); i++ )
			{
				// Preload.
				UObject* Obj = ObjLoaded(i);
				{
					if( Obj->GetFlags() & RF_NeedLoad )
					{
						check(Obj->GetLinker());
						Obj->GetLinker()->Preload( Obj );
				}
			}
			}
#endif
			GObjPreloading = WasPreloading;
			unguard;

			if(GObjLoaded.Num())
				continue;

			// Postload objects.
			guard(PostLoadObjects);
			for( INT i=0; i<ObjLoaded.Num(); i++ )
				ObjLoaded(i)->ConditionalPostLoad();
			unguard;
			}

			// Dissociate all linker import object references, since they 
			// may be destroyed, causing their pointers to become invalid.
			guard(DissociateImports);
			if( GImportCount )
			{
				for( INT i=0; i<GObjLoaders.Num(); i++ )
				{
					for( INT j=0; j<GetLoader(i)->ImportMap.Num(); j++ )
					{
						FObjectImport& Import = GetLoader(i)->ImportMap(j);
						if( Import.XObject && !(Import.XObject->GetFlags() & RF_Native) )
							Import.XObject = NULL;
					}
				}
			}
			GImportCount=0;
			unguard;
		}
		catch( const TCHAR* Error )
		{
			// Any errors here are fatal.
			appErrorf( Error );
		}
	}
	unguard;
}

//
// Empty the loaders.
//
void UObject::ResetLoaders( UObject* Pkg, UBOOL DynamicOnly, UBOOL ForceLazyLoad )
{
	guard(UObject::ResetLoaders);
	for( INT i=GObjLoaders.Num()-1; i>=0; i-- )
	{
		ULinkerLoad* Linker = CastChecked<ULinkerLoad>( GetLoader(i) );
		if( Pkg==NULL || Linker->LinkerRoot==Pkg )
		{
			if( DynamicOnly )
			{
				// Reset runtime-dynamic objects.
				for( INT i=0; i<Linker->ExportMap.Num(); i++ )
				{
					UObject*& Object = Linker->ExportMap(i)._Object;
					if( Object && !(Object->GetClass()->ClassFlags & CLASS_RuntimeStatic))
						Linker->DetachExport( i );
				}
			}
			else
			{
				// Fully reset the loader.
				if( ForceLazyLoad )
					Linker->DetachAllLazyLoaders( 1 );
			}
		}
		else
		{
			for(INT i = 0;i < Linker->ImportMap.Num();i++)
			{
				if(Linker->ImportMap(i).SourceLinker && Linker->ImportMap(i).SourceLinker->LinkerRoot == Pkg)
				{
					Linker->ImportMap(i).SourceLinker = NULL;
					Linker->ImportMap(i).SourceIndex = INDEX_NONE;
				}
			}
		}
	}
	for( INT i=GObjLoaders.Num()-1; i>=0; i-- )
	{
		ULinkerLoad* Linker = CastChecked<ULinkerLoad>( GetLoader(i) );
		if( Pkg==NULL || Linker->LinkerRoot==Pkg )
		{
			if( !DynamicOnly )
			{
				delete Linker;
			}
		}
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	File saving.
-----------------------------------------------------------------------------*/

//
// Archive for tagging objects and names that must be exported
// to the file.  It tags the objects passed to it, and recursively
// tags all of the objects this object references.
//
class FArchiveSaveTagExports : public FArchive
{
public:
	FArchiveSaveTagExports( UObject* InOuter )
	: Parent(InOuter)
	{
		ArIsSaving = ArIsPersistent = 1;
	}
	FArchive& operator<<( UObject*& Obj )
	{
		guard(FArchiveSaveTagExports<<Obj);
		if( Obj && Obj->IsIn(Parent) && !(Obj->GetFlags() & (RF_Transient|RF_TagExp)) )//&& !Obj->IsPendingKill() )
		{
			// Set flags.
			Obj->SetFlags(RF_TagExp);
			if( !(Obj->GetFlags() & RF_NotForEdit  ) ) Obj->SetFlags(RF_LoadForEdit);
			if( !(Obj->GetFlags() & RF_NotForClient) ) Obj->SetFlags(RF_LoadForClient);
			if( !(Obj->GetFlags() & RF_NotForServer) ) Obj->SetFlags(RF_LoadForServer);

			// Recurse with this object's class and package.
			UObject* Class  = Obj->GetClass();
			UObject* Parent = Obj->GetOuter();
			*this << Class << Parent;

			// Recurse with this object's children.
			Obj->Serialize( *this );
		}
		return *this;
		unguard;
	}
	UObject* Parent;
};

//
// QSort comparators.
//
static ULinkerSave* GTempSave;
INT CDECL LinkerNameSort( const void* A, const void* B )
{
	return GTempSave->MapName((FName*)B) - GTempSave->MapName((FName*)A);
}
INT CDECL LinkerImportSort( const void* A, const void* B )
{
	return GTempSave->MapObject(((FObjectImport*)B)->XObject) - GTempSave->MapObject(((FObjectImport*)A)->XObject);
}
INT CDECL LinkerExportSort( const void* A, const void* B )
{
	return GTempSave->MapObject(((FObjectExport*)B)->_Object) - GTempSave->MapObject(((FObjectExport*)A)->_Object);
}

//
// Archive for tagging objects and names that must be listed in the
// file's imports table.
//
class FArchiveSaveTagImports : public FArchive
{
public:
	DWORD ContextFlags;
	FArchiveSaveTagImports( ULinkerSave* InLinker, DWORD InContextFlags )
	: ContextFlags( InContextFlags ), Linker( InLinker )
	{
		ArIsSaving = ArIsPersistent = 1;
	}
	FArchive& operator<<( UObject*& Obj )
	{
		guard(FArchiveSaveTagImports<<Obj);
		if( Obj && !Obj->IsPendingKill() )
		{
			if( !(Obj->GetFlags() & RF_Transient) || (Obj->GetFlags() & RF_Public) )
			{
				Linker->ObjectIndices(Obj->GetIndex())++;
				if( !(Obj->GetFlags() & RF_TagExp ) )
				{
					Obj->SetFlags( RF_TagImp );
					if( !(Obj->GetFlags() & RF_NotForEdit  ) ) Obj->SetFlags(RF_LoadForEdit);
					if( !(Obj->GetFlags() & RF_NotForClient) ) Obj->SetFlags(RF_LoadForClient);
					if( !(Obj->GetFlags() & RF_NotForServer) ) Obj->SetFlags(RF_LoadForServer);
					UObject* Parent = Obj->GetOuter();
					if( Parent )
						*this << Parent;
				}
			}
		}
		return *this;
		unguard;
	}
	FArchive& operator<<( FName& Name )
	{
		guard(FArchiveSaveTagImports<<Name);
		Name.SetFlags( RF_TagExp | ContextFlags );
		Linker->NameIndices(Name.GetIndex())++;
		return *this;
		unguard;
	}
	ULinkerSave* Linker;
};

//
// Save one specific object into an Unrealfile.
//
UBOOL UObject::SavePackage( UObject* InOuter, UObject* Base, DWORD TopLevelFlags, const TCHAR* Filename, FOutputDevice* Error, ULinkerLoad* Conform )
{
	guard(UObject::SavePackage);
	check(InOuter);
	check(Filename);
	DWORD Time=0; clock(Time);

	// Tag parent flags.
	//!!should be user-controllable in UnrealEd.
	UPackage* P = Cast<UPackage>( InOuter );
	if( P!=NULL )
	{
		P->PackageFlags |= PKG_AllowDownload;
		UBOOL Official;
        if( GConfig->GetBool( TEXT("Packages"), TEXT("SavePackagesAsOfficial"), Official, TEXT("Official.ini") ) )
			if( Official )
				P->PackageFlags |= PKG_Official;
	}

	// Make temp file.
	TCHAR TempFilename[256];
	appStrncpy( TempFilename, Filename, ARRAY_COUNT(TempFilename) );
	INT c = appStrlen(TempFilename);
	while( c>0 && TempFilename[c-1]!=PATH_SEPARATOR[0] && TempFilename[c-1]!='/' && TempFilename[c-1]!=':' )
		c--;
	TempFilename[c]=0;
	appStrcat( TempFilename, TEXT("Save.tmp") );

	// Init.
	GWarn->StatusUpdatef( 0, 0, LocalizeProgress(TEXT("Saving"),TEXT("Core")), Filename );
	UBOOL Success = 0;

	// If we have a loader for the package, unload it to prevent conflicts.
	ResetLoaders( InOuter, 0, 1 );

	// Untag all objects and names.
	guard(Untag);
	for( FObjectIterator It; It; ++It )
		It->ClearFlags( RF_TagImp | RF_TagExp | RF_LoadForEdit | RF_LoadForClient | RF_LoadForServer );
	for( INT i=0; i<FName::GetMaxNames(); i++ )
		if( FName::GetEntry(i) )
			FName::GetEntry(i)->Flags &= ~(RF_TagImp | RF_TagExp | RF_LoadForEdit | RF_LoadForClient | RF_LoadForServer);
	unguard;

	// Export objects.
	guard(TagExports);
	FArchiveSaveTagExports Ar( InOuter );
	if( Base )
		Ar << Base;
	for( FObjectIterator It; It; ++It )
	{
		if( (It->GetFlags() & TopLevelFlags) && It->IsIn(InOuter) )
		{
			UObject* Obj = *It;
			Ar << Obj;
		}
	}
	unguard;

	ULinkerSave* Linker = NULL;
	try
	{
		// Allocate the linker.
		Linker = new ULinkerSave( InOuter, TempFilename );

		// Import objects and names.
		guard(TagImports);
		for( FObjectIterator It; It; ++It )
		{
			if( It->GetFlags() & RF_TagExp )
			{
				// Build list.
				FArchiveSaveTagImports Ar( Linker, It->GetFlags() & RF_LoadContextFlags );
				It->Serialize( Ar );
				UClass* Class = It->GetClass();
				Ar << Class;
				if( It->IsIn(GetTransientPackage()) )
					appErrorf( LocalizeError(TEXT("TransientImport"),TEXT("Core")), It->GetFullName() );
			}
		}
		unguard;

		// Export all relevant object, class, and package names.
		guard(ExportNames);
		for( FObjectIterator It; It; ++It )
		{
			if( It->GetFlags() & (RF_TagExp|RF_TagImp) )
			{
				It->GetFName().SetFlags( RF_TagExp | RF_LoadForEdit | RF_LoadForClient | RF_LoadForServer );
				if( It->GetOuter() )
					It->GetOuter()->GetFName().SetFlags( RF_TagExp | RF_LoadForEdit | RF_LoadForClient | RF_LoadForServer );
				if( It->GetFlags() & RF_TagImp )
				{
					It->Class->GetFName().SetFlags( RF_TagExp | RF_LoadForEdit | RF_LoadForClient | RF_LoadForServer );
					check(It->Class->GetOuter());
					It->Class->GetOuter()->GetFName().SetFlags( RF_TagExp | RF_LoadForEdit | RF_LoadForClient | RF_LoadForServer );
					if( !(It->GetFlags() & RF_Public) )
					{
						GWarn->Logf( TEXT("Referencers of %s:"), It->GetFullName() );
						for( FObjectIterator RefIt; RefIt; ++RefIt )
						{
							FArchiveFindCulprit ArFind(*It,*RefIt);
							if( ArFind.GetCount() )
								GWarn->Logf( TEXT("   %s"), RefIt->GetFullName() );
						}
						appThrowf( LocalizeError(TEXT("FailedSavePrivate"),TEXT("Core")), Filename, It->GetFullName() );
					}
				}
				else debugfSlow( NAME_DevSave, TEXT("Saving %s"), It->GetFullName() );
			}
		}
		unguard;

		// Write fixed-length file summary to overwrite later.
		guard(SaveSummary);
		if( Conform )
		{
			// Conform to previous generation of file.
			debugf( TEXT("Conformal save, relative to: %s, Generation %i"), *Conform->Filename, Conform->Summary.Generations.Num()+1 );
			Linker->Summary.Guid        = Conform->Summary.Guid;
			Linker->Summary.Generations = Conform->Summary.Generations;
		}
		else
		{
			// First generation file.
			Linker->Summary.Guid        = appCreateGuid();
			Linker->Summary.Generations = TArray<FGenerationInfo>();
		}
		new(Linker->Summary.Generations)FGenerationInfo( 0, 0 );
		*Linker << Linker->Summary;
		unguard;

		// Build NameMap.
		guard(BuildNameMap);
		Linker->Summary.NameOffset = Linker->Tell();
		for( INT i=0; i<FName::GetMaxNames(); i++ )
		{
			if( FName::GetEntry(i) )
			{
				FName Name( (EName)i );
				if( Name.GetFlags() & RF_TagExp )
					Linker->NameMap.AddItem( Name );
			}
		}
		unguard;

		// Sort names by usage count in order to maximize compression.
		guard(SortNames);
		GTempSave = Linker;
		INT FirstSort = 0;
		if( Conform )
		{
#if DO_GUARD_SLOW
			TArray<FName> Orig = Linker->NameMap;
#endif
			for( INT i=0; i<Conform->NameMap.Num(); i++ )
			{
				INT Index = Linker->NameMap.FindItemIndex(Conform->NameMap(i));
				if( Conform->NameMap(i)==NAME_None || Index==INDEX_NONE )
				{
					Linker->NameMap.Add();
					Linker->NameMap.Last() = Linker->NameMap(i);
				}
				else Exchange( Linker->NameMap(i), Linker->NameMap(Index) );
				Linker->NameMap(i) = Conform->NameMap(i);
			}
#if DO_GUARD_SLOW
			for( INT i=0; i<Conform->NameMap.Num(); i++ )
				check(Linker->NameMap(i)==Conform->NameMap(i));
			for( INT i=0; i<Orig.Num(); i++ )
				check(Linker->NameMap.FindItemIndex(Orig(i))!=INDEX_NONE);
#endif
			FirstSort = Conform->NameMap.Num();
		}
		appQsort( &Linker->NameMap(FirstSort), Linker->NameMap.Num()-FirstSort, sizeof(Linker->NameMap(0)), LinkerNameSort );
		unguard;

		// Save names.
		guard(SaveNames);
		Linker->Summary.NameCount = Linker->NameMap.Num();
		for( INT i=0; i<Linker->NameMap.Num(); i++ )
		{
			*Linker << *FName::GetEntry( Linker->NameMap(i).GetIndex() );
			Linker->NameIndices(Linker->NameMap(i).GetIndex()) = i;
		}
		unguard;

		// Build ImportMap.
		guard(BuildImportMap);
		for( FObjectIterator It; It; ++It )
			if( It->GetFlags() & RF_TagImp )
				new( Linker->ImportMap )FObjectImport( *It );
		Linker->Summary.ImportCount = Linker->ImportMap.Num();
		unguard;

		// Sort imports by usage count.
		guard(SortImports);
		GTempSave = Linker;
		appQsort( &Linker->ImportMap(0), Linker->ImportMap.Num(), sizeof(Linker->ImportMap(0)), LinkerImportSort );
		unguard;

		// Build ExportMap.
		guard(BuildExports);
		for( FObjectIterator It; It; ++It )
			if( It->GetFlags() & RF_TagExp )
				new( Linker->ExportMap )FObjectExport( *It );
		unguard;

		// Sort exports by usage count.
		guard(SortExports);
		INT FirstSort = 0;
		if( Conform )
		{
			TArray<FObjectExport> Orig = Linker->ExportMap;
			Linker->ExportMap.Empty( Linker->ExportMap.Num() );
			TArray<BYTE> Used; Used.AddZeroed( Orig.Num() );
			TMap<FString,INT> Map;
			{for( INT i=0; i<Orig.Num(); i++ )
				Map.Set( Orig(i)._Object->GetFullName(), i );}
			{for( INT i=0; i<Conform->ExportMap.Num(); i++ )
			{
				INT* Find = Map.Find( Conform->GetExportFullName(i,Linker->LinkerRoot->GetPathName()) );
				if( Find )
				{
					new(Linker->ExportMap)FObjectExport( Orig(*Find) );
					check(Linker->ExportMap.Last()._Object == Orig(*Find)._Object);
					Used( *Find ) = 1;
				}
				else new(Linker->ExportMap)FObjectExport( NULL );
			}}
			FirstSort = Conform->ExportMap.Num();
			{for( INT i=0; i<Used.Num(); i++ )
				if( !Used(i) )
					new(Linker->ExportMap)FObjectExport( Orig(i) );}
#if DO_GUARD_SLOW
			{for( INT i=0; i<Orig.Num(); i++ )
			{
				INT j = 0;
				for( j=0; j<Linker->ExportMap.Num(); j++ )
					if( Linker->ExportMap(j)._Object == Orig(i)._Object )
						break;
				check(j<Linker->ExportMap.Num());
			}}
#endif
		}
		appQsort( &Linker->ExportMap(FirstSort), Linker->ExportMap.Num()-FirstSort, sizeof(Linker->ExportMap(0)), LinkerExportSort );
		Linker->Summary.ExportCount = Linker->ExportMap.Num();
		unguard;

		// Set linker reverse mappings.
		guard(SetLinkerMappings);
		{for( INT i=0; i<Linker->ExportMap.Num(); i++ )
			if( Linker->ExportMap(i)._Object )
				Linker->ObjectIndices(Linker->ExportMap(i)._Object->GetIndex()) = i+1;}
		{for( INT i=0; i<Linker->ImportMap.Num(); i++ )
			Linker->ObjectIndices(Linker->ImportMap(i).XObject->GetIndex()) = -i-1;}
		unguard;

		// Save exports.
		guard(SaveExports);
		for( INT i=0; i<Linker->ExportMap.Num(); i++ )
		{
			FObjectExport& Export = Linker->ExportMap(i);
			if( Export._Object )
			{
				// Set class index.
				if( !Export._Object->IsA(UClass::StaticClass()) )
				{
					Export.ClassIndex = Linker->ObjectIndices(Export._Object->GetClass()->GetIndex());
					check(Export.ClassIndex!=0);
				}
				if( Export._Object->IsA(UStruct::StaticClass()) )
				{
					UStruct* Struct = (UStruct*)Export._Object;
					if( Struct->SuperField )
					{
						Export.SuperIndex = Linker->ObjectIndices(Struct->SuperField->GetIndex());
						check(Export.SuperIndex!=0);
					}
				}

				// Set package index.
				if( Export._Object->GetOuter() != InOuter )
				{
					check(Export._Object->GetOuter()->IsIn(InOuter));
					Export.PackageIndex = Linker->ObjectIndices(Export._Object->GetOuter()->GetIndex());
					check(Export.PackageIndex>0);
				}

				// Save it.
				Export.SerialOffset = Linker->Tell();
				Export._Object->Serialize( *Linker );
				Export.SerialSize = Linker->Tell() - Linker->ExportMap(i).SerialOffset;
			}
		}
		unguard;

		// Save the import map.
		guard(SaveImportMap);
		Linker->Summary.ImportOffset = Linker->Tell();
		for( INT i=0; i<Linker->ImportMap.Num(); i++ )
		{
			FObjectImport& Import = Linker->ImportMap( i );

			// Set the package index.
			if( Import.XObject->GetOuter() )
			{
				check(!Import.XObject->GetOuter()->IsIn(InOuter));
				Import.PackageIndex = Linker->ObjectIndices(Import.XObject->GetOuter()->GetIndex());
				check(Import.PackageIndex<0);
			}

			// Save it.
			*Linker << Import;
		}
		unguard;

		// Save the export map.
		guard(SaveExportMap);
		Linker->Summary.ExportOffset = Linker->Tell();
		for( int i=0; i<Linker->ExportMap.Num(); i++ )
			*Linker << Linker->ExportMap( i );
		unguard;

		// Rewrite updated file summary.
		guard(RewriteSummary);
		GWarn->StatusUpdatef( 0, 0, LocalizeProgress(TEXT("Closing"),TEXT("Core")) );
		Linker->Summary.Generations.Last().ExportCount = Linker->Summary.ExportCount;
		Linker->Summary.Generations.Last().NameCount   = Linker->Summary.NameCount;
		Linker->Seek(0);
		*Linker << Linker->Summary;
		unguard;

		Success = 1;
	}
	catch( const TCHAR* Msg )
	{
		// Delete the temporary file.
		GFileManager->Delete( TempFilename );
		Error->Logf( NAME_Warning, TEXT("%s"), Msg );
	}
	if( Linker )
	{
		delete Linker;
	}
	unclock(Time);
	debugf( NAME_Log, TEXT("Save=%f"), GSecondsPerCycle*1000*Time );
	if( Success )
	{
		// Move the temporary file.
		debugf( NAME_Log, TEXT("Moving '%s' to '%s'"), TempFilename, Filename );
		if( !GFileManager->Move( Filename, TempFilename ) )
		{
			GFileManager->Delete( TempFilename );
			GWarn->Logf( LocalizeError(TEXT("SaveWarning"),TEXT("Core")), Filename );
			Success = 0;
		}
	}
	return Success;
	unguard;
}

// amb ---
// many changes by Capps and Andrew to avoid crash when deleting
// last gameprofile during gameplay
UBOOL UObject::DeletePackage(const TCHAR* Filename)
{
    guard(UObject::DeletePackage);

    check(Filename);

    // remove the file extension
    TCHAR Temp[256], *End;
    appStrncpy(Temp, Filename, ARRAY_COUNT(Temp));
    End = appStrchr(Temp,'.');
    if (End)
        *End++ = 0;

    UPackage* pkg = (UPackage*)LoadPackage(NULL, Filename, 0);

    UBOOL Success = 0;
   
    if (pkg)
    {
		/*
        // delete all objects in the package
        for (TObjectIterator<UObject> It; It; ++It)
        {
            if (It->IsIn(pkg))
			{
                delete *It;
            }
        }
		*/
        UObject::ResetLoaders(pkg, 0, 1); // prevents sharing violation
		//delete pkg;
    }

    Success = GFileManager->Delete(Filename);

    return Success;
    unguard;
}
// -- amb

/*-----------------------------------------------------------------------------
	Misc.
-----------------------------------------------------------------------------*/

//
// Return whether statics are initialized.
//
UBOOL UObject::GetInitialized()
{
	return UObject::GObjInitialized;
}

//
// Return the static transient package.
//
UPackage* UObject::GetTransientPackage()
{
	return UObject::GObjTransientPkg;
}

//
// Return the ith loader.
//
ULinkerLoad* UObject::GetLoader( INT i )
{
	return (ULinkerLoad*)GObjLoaders(i);
}

//
// Add an object to the root array. This prevents the object and all
// its descendents from being deleted during garbage collection.
//
void UObject::AddToRoot()
{
	guard(UObject::AddToRoot);
	GObjRoot.AddItem( this );
	unguard;
}

//
// Remove an object from the root array.
//
void UObject::RemoveFromRoot()
{
	guard(UObject::RemoveFromRoot);
	GObjRoot.RemoveItem( this );
	unguard;
}

/*-----------------------------------------------------------------------------
	Object name functions.
-----------------------------------------------------------------------------*/

//
// Create a unique name by combining a base name and an arbitrary number string.  
// The object name returned is guaranteed not to exist.
//
FName UObject::MakeUniqueObjectName( UObject* Parent, UClass* Class )
{
	guard(UObject::MakeUniqueObjectName);
	check(Class);
	
	TCHAR NewBase[NAME_SIZE], Result[NAME_SIZE];
	TCHAR TempIntStr[NAME_SIZE];

	// Make base name sans appended numbers.
	appStrcpy( NewBase, Class->GetName() );
	TCHAR* End = NewBase + appStrlen(NewBase);
	while( End>NewBase && appIsDigit(End[-1]) )
		End--;
	*End = 0;

	// Append numbers to base name.
	do
	{
		appSprintf( TempIntStr, TEXT("%i"), Class->ClassUnique++ );
		appStrncpy( Result, NewBase, NAME_SIZE-appStrlen(TempIntStr)-1 );
		appStrcat( Result, TempIntStr );
	} while( StaticFindObject( NULL, Parent, Result ) );

	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	Object hashing.
-----------------------------------------------------------------------------*/

//
// Add an object to the hash table.
//
void UObject::HashObject()
{
	guard(UObject::HashObject);

	INT iHash       = GetObjectHash( Name, Outer ? Outer->GetIndex() : 0 );
	HashNext        = GObjHash[iHash];
	GObjHash[iHash] = this;

	unguard;
}

//
// Remove an object from the hash table.
//
void UObject::UnhashObject( INT OuterIndex )
{
	guard(UObject::UnhashObject);
	INT       iHash   = GetObjectHash( Name, OuterIndex );
	UObject** Hash    = &GObjHash[iHash];
	INT       Removed = 0;
	while( *Hash != NULL )
	{
		if( *Hash != this )
		{
			Hash = &(*Hash)->HashNext;
 		}
		else
		{
			*Hash = (*Hash)->HashNext;
			Removed++;
		}
	}
	check(Removed!=0);
	check(Removed==1);
	unguard;
}

/*-----------------------------------------------------------------------------
	Creating and allocating data for new objects.
-----------------------------------------------------------------------------*/

//
// Add an object to the table.
//
void UObject::AddObject( INT InIndex )
{
	guard(UObject::AddObject);

	// Find an available index.
	if( InIndex==INDEX_NONE )
	{
		if( GObjAvailable.Num() )
		{
			InIndex = GObjAvailable.Pop();
			check(GObjObjects(InIndex)==NULL);
		}
		else InIndex = GObjObjects.Add();
	}

	// Add to global table.
	GObjObjects(InIndex) = this;
	Index = InIndex;
	HashObject();

	unguard;
}

//
// Create a new object or replace an existing one.
// If Name is NAME_None, tries to create an object with an arbitrary unique name.
// No failure conditions.
//
UObject* UObject::StaticAllocateObject
(
	UClass*			InClass,
	UObject*		InOuter,
	FName			InName,
	DWORD			InFlags,
	UObject*		InTemplate,
	FOutputDevice*	Error,
	UObject*		Ptr,
	UObject*		SuperObject
)
{
	guard(UObject::StaticAllocateObject);
	check(Error);
	check(!InClass || InClass->ClassWithin);
	check(!InClass || InClass->ClassConstructor);

	// Validation checks.
	if( !InClass )
	{
		Error->Logf( TEXT("Empty class for object %s"), *InName );
		return NULL;
	}
	if( InClass->GetIndex()==INDEX_NONE && GObjRegisterCount==0 )
	{
		Error->Logf( TEXT("Unregistered class for %s"), *InName );
		return NULL;
	}
	if( InClass->ClassFlags & CLASS_Abstract )
	{
		Error->Logf( LocalizeError(TEXT("Abstract"),TEXT("Core")), *InName, InClass->GetName() );
		return NULL;
	}
	if( !InOuter && InClass!=UPackage::StaticClass() )
	{
		Error->Logf( LocalizeError(TEXT("NotPackaged"),TEXT("Core")), InClass->GetName(), *InName );
		return NULL;
	}
	if( InOuter && !InOuter->IsA(InClass->ClassWithin) )
	{
		Error->Logf( LocalizeError(TEXT("NotWithin"),TEXT("Core")), InClass->GetName(), *InName, InOuter->GetClass()->GetName(), InClass->ClassWithin->GetName() );
		return NULL;
	}

#if 1
    // sjs - if transient naming is off, call everything 'NAME_Transient' where appropriate
    if( InName==NAME_None && !GTransientNaming && (InOuter==GetTransientPackage() || ( InOuter==NULL || InOuter->IsIn(GetTransientPackage()) ) || InFlags&RF_Transient) ) // sjs will it work?
        InName = NAME_Transient;
#endif

	// Compose name, if public and unnamed.
	//if( InName==NAME_None && (InFlags&RF_Public) )
	if( InName==NAME_None )//oldver?? what about compatibility matching of unnamed objects?
		InName = MakeUniqueObjectName( InOuter, InClass );

	// Check for name conflicts.
	if( GCheckConflicts && InName!=NAME_None )
		for( UObject* Hash=GObjHash[GetObjectHash(InName,InOuter?InOuter->GetIndex():0)]; Hash!=NULL; Hash=Hash->HashNext )
			if
			(	Hash->GetFName()==InName
			&&	Hash->Outer==InOuter
			&&	Hash->GetClass()!=InClass )
				debugf( NAME_Log, TEXT("CONFLICT: %s - %s"), Hash->GetFullName(), InClass->GetName() );

	// See if object already exists.
    UObject* Obj = NULL;
    if( InName!=NAME_Transient ) // sjs
	    Obj                             = StaticFindObject( InClass, InOuter, *InName );//oldver: Should use NULL instead of InClass to prevent conflicts by name rather than name-class.
	UClass*  Cls                        = Cast<UClass>( Obj );
	INT      Index                      = INDEX_NONE;
	UClass*  ClassWithin				= NULL;
	DWORD    ClassFlags                 = 0;
	void     (*ClassConstructor)(void*) = NULL;
	if( !Obj )
	{
		// Create a new object.
		Obj = Ptr ? Ptr : (UObject*)appMalloc( InClass->GetPropertiesSize(), *InName );
	}
	else
	{
		// Replace an existing object without affecting the original's address or index.
		check(!Ptr || Ptr==Obj);
		debugfSlow( NAME_DevReplace, TEXT("Replacing %s"), Obj->GetName() );

		// Can only replace if class is identical.
		if( Obj->GetClass() != InClass )
			appErrorf( LocalizeError(TEXT("NoReplace"),TEXT("Engine")), Obj->GetFullName(), InClass->GetName() );

		// Remember flags, index, and native class info.
		InFlags |= (Obj->GetFlags() & RF_Keep);
		Index    = Obj->Index;
		if( Cls )
		{
			ClassWithin		 = Cls->ClassWithin;
			ClassFlags       = Cls->ClassFlags & CLASS_Abstract;
			ClassConstructor = Cls->ClassConstructor;
		}

		// Destroy the object.
		Obj->~UObject();
		check(GObjAvailable.Num() && GObjAvailable.Last()==Index);
		GObjAvailable.Pop();
	}

	// If class is transient, objects must be transient.
	if( InClass->ClassFlags & CLASS_Transient )
		InFlags |= RF_Transient;

	// Set the base properties.
	Obj->Index			 = INDEX_NONE;
	Obj->HashNext		 = NULL;
	Obj->StateFrame      = NULL;
	Obj->_Linker		 = NULL;
	Obj->_LinkerIndex	 = INDEX_NONE;
	Obj->Outer			 = InOuter;
	Obj->ObjectFlags	 = InFlags;
    Obj->Name			 = InName==NAME_Transient ? InClass->GetFName() : InName; // sjs
	Obj->Class			 = InClass;

	// Init the properties.
	InitProperties( (BYTE*)Obj, InClass->GetPropertiesSize(), InClass, (BYTE*)InTemplate, InClass->GetPropertiesSize(), (InFlags&RF_NeedLoad) ? NULL : Obj, SuperObject);

	// Add to global table.
	Obj->AddObject( Index );
	check(Obj->IsValid());

	// If config is per-object, load it.
	if( InClass->ClassFlags & CLASS_PerObjectConfig )
	{
		Obj->LoadConfig();
		Obj->LoadLocalized();
	}

	// Restore class information if replacing native class.
	if( Cls )
	{
		Cls->ClassWithin	   = ClassWithin;
		Cls->ClassFlags       |= ClassFlags;
		Cls->ClassConstructor  = ClassConstructor;
	}

	// Success.
	return Obj;
	unguardf(( TEXT("(%s %s)"), InClass ? InClass->GetName() : TEXT("NULL"), *InName ));
}

//
// Construct an object.
//
UObject* UObject::StaticConstructObject
(
	UClass*			InClass,
	UObject*		InOuter,
	FName			InName,
	DWORD			InFlags,
	UObject*		InTemplate,
	FOutputDevice*	Error,
	UObject*		SuperObject
)
{
	guard(UObject::StaticConstructObject);
	check(Error);

	// Allocate the object.
	UObject* Result = StaticAllocateObject( InClass, InOuter, InName, InFlags, InTemplate, Error, NULL, SuperObject );
	if( Result )
		(*InClass->ClassConstructor)( Result );
	return Result;

	unguard;
}

/*-----------------------------------------------------------------------------
   Garbage collection.
-----------------------------------------------------------------------------*/

//
// Serialize the global root set to an archive.
//
void UObject::SerializeRootSet( FArchive& Ar, DWORD KeepFlags, DWORD RequiredFlags )
{
	guard(UObject::SerializeRootSet);
	Ar << GObjRoot;
	for( FObjectIterator It; It; ++It )
	{
		if
		(	(It->GetFlags() & KeepFlags)
		&&	(It->GetFlags()&RequiredFlags)==RequiredFlags )
		{
			UObject* Obj = *It;
			Ar << Obj;
		}
	}
	unguard;
}

//
// Archive for finding unused objects.
//
class FArchiveTagUsed : public FArchive
{
	FName LevelName, UdpGamespyQueryName;
public:
	// Worklist for iterative traversal (avoids deep recursion that
	// overflows the call stack on WASM/Emscripten).
	TArray<UObject*> Worklist;

	FArchiveTagUsed()
	: Context( NULL )
	, LevelName(TEXT("Level"))
	, UdpGamespyQueryName(TEXT("UdpGamespyQuery"))
	{
		guard(FArchiveTagUsed::FArchiveTagUsed);
		GGarbageRefCount=0;

		// Tag all objects as unreachable.
		for( FObjectIterator It; It; ++It )
			It->SetFlags( RF_Unreachable | RF_TagGarbage );

		// Tag all names as unreachable.
		for( INT i=0; i<FName::GetMaxNames(); i++ )
			if( FName::GetEntry(i) )
				FName::GetEntry(i)->Flags |= RF_Unreachable;

		unguard;
	}

	// Process all deferred objects iteratively instead of recursively.
	void DrainWorklist()
	{
		guard(FArchiveTagUsed::DrainWorklist);
		while( Worklist.Num() )
		{
			UObject* Obj = Worklist( Worklist.Num()-1 );
			Worklist.Remove( Worklist.Num()-1 );

			if( Obj->GetFlags() & RF_TagGarbage )
			{
				UObject* OriginalContext = Context;
				Context = Obj;
				Obj->Serialize( *this );
				if( !(Obj->GetFlags() & RF_DebugSerialize) )
					appErrorf( TEXT("%s failed to route Serialize"), Obj->GetFullName() );
				Context = OriginalContext;
			}
			else
			{
				debugfSlow( NAME_Log, TEXT("%s is referenced by %s"), Obj->GetFullName(), Context ? Context->GetFullName() : NULL );
			}
		}
		unguard;
	}
private:
	FArchive& operator<<( UObject*& Object )
	{
		guardSlow(FArchiveTagUsed<<Obj);
		GGarbageRefCount++;

		// Object could be a misaligned pointer.
		// Copy the contents of the pointer into a temporary and work on that.
		UObject* Obj;
		appMemcpy(&Obj, &Object, sizeof(UObject*));

#if DO_GUARD_SLOW
		guard(CheckValid);
		if( Obj )
			check(Obj->IsValid());
		unguard;
#endif
		if( Obj && (Obj->GetFlags() & RF_EliminateObject) )
		{
			// Dereference it.
			Obj = NULL;
		}
		else if( Obj && (Obj->GetFlags() & RF_Unreachable) )
		{
			// Only process the first time object is claimed.
			guard(TestReach);
			Obj->ClearFlags( RF_Unreachable | RF_DebugSerialize );

			// Defer to worklist instead of recursing (prevents stack overflow on WASM).
			Worklist.AddItem( Obj );

			unguardf(( TEXT("(%s)"), Obj->GetFullName() ));
		}

		// Contents of pointer might have been modified.
		// Copy the results back into the misaligned pointer.
		appMemcpy(&Object, &Obj, sizeof(UObject*));
		return *this;
		unguardSlow;
	}
	FArchive& operator<<( FName& Name )
	{
		guardSlow(FArchiveTagUsed::Name);

#if __PSX2_EE__ || __GCN__
		// Name is probably not aligned correctly.
		// Memcpy workaround copies into aligned temporary then back.
		FName LocalName;
		appMemcpy(&LocalName, &Name, sizeof(FName));
		LocalName.ClearFlags( RF_Unreachable );
		appMemcpy(&Name, &LocalName, sizeof(FName));
#else
		Name.ClearFlags( RF_Unreachable );
#endif

		return *this;
		unguardSlow;
	}
	UObject* Context;
};

// gam ---
// #define SHOW_GARBAGE_STATS
// --- gam

//
// Purge garbage.
//
void UObject::PurgeGarbage()
{
	guard(UObject::PurgeGarbage);
	INT CountBefore=0, CountPurged=0;
	if( GNoGC )
	{
		debugf( NAME_Log, TEXT("Not purging garbage") );
		return;
	}
	debugf( NAME_Log, TEXT("Purging garbage") );

    // gam ---
    #ifdef SHOW_GARBAGE_STATS
    {
        TMap<FName,INT> GarbageMap;

        INT Count;

        GarbageMap.Empty();

	    for( INT i=0; i<GObjObjects.Num(); i++ )
	    {
            UObject* Object = GObjObjects(i);

		    if( !Object )
                continue;

		    if( Object->GetFlags() & RF_Native )
                continue;

            if( Object->IsPendingKill() )
                continue;

        	INT* Count = GarbageMap.Find( Object->GetClass()->GetFName() );

            if( Count )
                (*Count)++;
            else
            	GarbageMap.Set( Object->GetClass()->GetFName(), 1 );
	    }

        if( GarbageMap.Num() )
        {
            GarbageMap.SortByValue( true );

            Count = Min( 50, GarbageMap.Num() );

            debugf( TEXT("Top %d types of object in memory:"), Count );

    	    for( TMap<FName,INT>::TIterator It(GarbageMap); It; ++It )
            {
                debugf( TEXT("%-4d %s"), It.Value(), *(It.Key()) );

                if( --Count == 0 )
                    break;
            }
        }

        GarbageMap.Empty();

    	UClass* ActorClass = Cast<UClass>( StaticFindObject( UClass::StaticClass(), ANY_PACKAGE, TEXT("Actor"), 1 ) );

        if( ActorClass )
        {
	        for( INT i=0; i<GObjObjects.Num(); i++ )
	        {
                UObject* Object = GObjObjects(i);

		        if( !Object )
                    continue;

		        if( Object->GetFlags() & RF_Native )
                    continue;

		        if( !Object->IsA( ActorClass ) )
                    continue;

                if( Object->IsPendingKill() )
                    continue;

        	    INT* Count = GarbageMap.Find( Object->GetClass()->GetFName() );

                if( Count )
                    (*Count)++;
                else
            	    GarbageMap.Set( Object->GetClass()->GetFName(), 1 );
	        }

            if( GarbageMap.Num() )
            {
                GarbageMap.SortByValue( true );

                Count = Min( 50, GarbageMap.Num() );

                debugf( TEXT("Top %d types of actors in memory:"), Count );

    	        for( TMap<FName,INT>::TIterator It(GarbageMap); It; ++It )
                {
                    debugf( TEXT("%-4d %s"), It.Value(), *(It.Key()) );

                    if( --Count == 0 )
                        break;
                }
            }

            GarbageMap.Empty();

	        for( INT i=0; i<GObjObjects.Num(); i++ )
	        {
                UObject* Object = GObjObjects(i);

		        if( !Object )
                    continue;

		        if( !(Object->GetFlags() & RF_Unreachable) ) 
                    continue;

		        if( Object->GetFlags() & RF_Native )
                    continue;

		        if( !Object->IsA( ActorClass ) )
                    continue;

                if( Object->IsPendingKill() )
                    continue;

        	    INT* Count = GarbageMap.Find( Object->GetClass()->GetFName() );

                if( Count )
                    (*Count)++;
                else
            	    GarbageMap.Set( Object->GetClass()->GetFName(), 1 );
	        }

            if( GarbageMap.Num() )
            {
                GarbageMap.SortByValue( true );

                Count = Min( 50, GarbageMap.Num() );

                debugf( TEXT("Top %d types of unreachable actors in memory:"), Count );

    	        for( TMap<FName,INT>::TIterator It(GarbageMap); It; ++It )
                {
                    debugf( TEXT("%-4d %s"), It.Value(), *(It.Key()) );

                    if( --Count == 0 )
                        break;
                }
            }
        }
    }
    #endif
    // --- gam

	GIsGarbageCollecting = 1; // Set 'I'm garbage collecting' flag - might be checked inside UObject::Destropy etc.

	// Notify UDebugger to clear its stack, since all FFrame's will be destroyed -- rjp
	if ( GDebugger )
		GDebugger->NotifyGC();

	// Dispatch all Destroy messages.
	guard(DispatchDestroys);
	for( INT i=0; i<GObjObjects.Num(); i++ )
	{
		guard(DispatchDestroy);
		CountBefore += (GObjObjects(i)!=NULL);
		if
		(	GObjObjects(i)
		&&	(GObjObjects(i)->GetFlags() & RF_Unreachable)
		&&  (!(GObjObjects(i)->GetFlags() & RF_Native) || GExitPurge) )
		{
			debugfSlow( NAME_DevGarbage, TEXT("Garbage collected object %i: %s"), i, GObjObjects(i)->GetFullName() );
			// gam ---
			#ifdef _DEBUG
			    FName LastObjectName = GObjObjects(i)->GetFName();
			    checkSlow( LastObjectName.IsValid() );
			    FName LastObjectClassName = GObjObjects(i)->GetClass()->GetFName();
			    checkSlow( LastObjectClassName.IsValid() );
			#endif
			// --- gam
			GObjObjects(i)->ConditionalDestroy();
			CountPurged++;
		}
		unguardf(( TEXT("(%i: %s %s)"), i, GObjObjects(i)->GetFullName(), GObjObjects(i)->GetClass() && GObjObjects(i)->GetClass()->IsValid() ? GObjObjects(i)->GetClass()->GetFullName() : TEXT("Invalid Class")));
	}

	unguard;

	// Purge all unreachable objects.
	//warning: Can't use FObjectIterator here because classes may be destroyed before objects.
	FName DeleteName=NAME_None;
	guard(DeleteGarbage);
	for( INT i=0; i<GObjObjects.Num(); i++ )
	{
		guard(DeleteObject);
		if
		(	GObjObjects(i)
		&&	(GObjObjects(i)->GetFlags() & RF_Unreachable) 
		&& !(GObjObjects(i)->GetFlags() & RF_Native) )
		{
			DeleteName = GObjObjects(i)->GetFName();
			delete GObjObjects(i);
		}
		unguardf(( TEXT("(%i)"), i ));
	}
	unguardf(( TEXT("(%s)"), *DeleteName ));

	GIsGarbageCollecting = 0; // Unset flag

	// Purge all unreachable names.
	guard(Names);
	for( INT i=0; i<FName::GetMaxNames(); i++ )
	{
		FNameEntry* Name = FName::GetEntry(i);
		if
		(	(Name)
		&&	(Name->Flags & RF_Unreachable)
		&& !(Name->Flags & RF_Native     ) )
		{
			debugfSlow( NAME_DevGarbage, TEXT("Garbage collected name %i: %s"), i, Name->Name );
			FName::DeleteEntry(i);
		}
	}
	unguard;

	debugf(TEXT("Garbage: objects: %i->%i; refs: %i"),CountBefore,CountBefore-CountPurged,GGarbageRefCount);
	unguard;
}

//
// Delete all unreferenced objects.
//
void UObject::CollectGarbage( DWORD KeepFlags )
{
	guard(UObject::CollectGarbage);
	debugf( NAME_Log, TEXT("Collecting garbage") );

	// Tag and purge garbage.
	FArchiveTagUsed TagUsedAr;
	SerializeRootSet( TagUsedAr, KeepFlags, RF_TagGarbage );
	TagUsedAr.DrainWorklist();

	// Purge it.
	PurgeGarbage();

	unguard;
}

//
// Returns whether an object is referenced, not counting the
// one reference at Obj. No side effects.
//
UBOOL UObject::IsReferenced( UObject*& Obj, DWORD KeepFlags, UBOOL IgnoreReference )
{
	guard(UObject::RefCount);

	// Remember it.
	UObject* OriginalObj = Obj;
	if( IgnoreReference )
		Obj = NULL;

	// Tag all garbage.
	FArchiveTagUsed TagUsedAr;
	OriginalObj->ClearFlags( RF_TagGarbage );
	SerializeRootSet( TagUsedAr, KeepFlags, RF_TagGarbage );
	TagUsedAr.DrainWorklist();

	// Stick the reference back.
	Obj = OriginalObj;

	// Return whether this is tagged.
	return (Obj->GetFlags() & RF_Unreachable)==0;
	unguard;
}

//
// Attempt to delete an object. Only succeeds if unreferenced.
//
UBOOL UObject::AttemptDelete( UObject*& Obj, DWORD KeepFlags, UBOOL IgnoreReference )
{
	guard(UObject::AttemptDelete);
	if( !(Obj->GetFlags() & RF_Native) && !IsReferenced( Obj, KeepFlags, IgnoreReference ) )
	{
		PurgeGarbage();
		return 1;
	}
	else return 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	Importing and exporting.
-----------------------------------------------------------------------------*/

//
// Import an object from a file.
//
void UObject::ParseParms( const TCHAR* Parms )
{
	guard(ParseObjectParms);
	if( !Parms )
		return;
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(GetClass()); It; ++It )
	{
		if( It->GetOuter()!=UObject::StaticClass() )
		{
			FString Value;
			if( Parse(Parms,*(FString(It->GetName())+TEXT("=")),Value) )
				It->ImportText( *Value, (BYTE*)this + It->Offset, PPF_Localized );
		}
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Driver support.
-----------------------------------------------------------------------------*/

//
// Cache a list of globally registered objects into memory.
//
void UObject::CacheDrivers( UBOOL ForceRefresh )
{
	guard(UObject::CacheDrivers);
    TCHAR Buffer[31500];
	if( ForceRefresh || appStricmp(GObjCachedLanguage,UObject::GetLanguage())!=0 )
	{
		appStrcpy( GObjCachedLanguage, UObject::GetLanguage() );
		GObjPreferences.Empty();
		GObjDrivers.Empty();
		for( INT i=0; i<GSys->Paths.Num(); i++ )
		{
			TCHAR Filename[256];
			appSprintf( Filename, TEXT("%s%s"), appBaseDir(), *GSys->Paths(i) );
			TCHAR* Tmp = appStrstr( Filename, TEXT("*.") );
			if( Tmp )
			{
				appSprintf( Tmp, TEXT("*.int") );
				TArray<FString> Files = GFileManager->FindFiles( Filename, 1, 0 );
				for( INT j=0; j<Files.Num(); j++ )
				{
//					appSprintf( Tmp, TEXT("%s%s"), appBaseDir(), *Files(j) );
					appSprintf( Tmp, TEXT("%s"), *Files(j) );
					TCHAR* End = Tmp + appStrlen( Tmp ) - 4;
					appSprintf( End, TEXT(".%s"), UObject::GetLanguage() );
					UBOOL Success = GConfig->GetSection( TEXT("Public"), Buffer, ARRAY_COUNT(Buffer), Filename );
					if( !Success )
					{
						appSprintf( End, TEXT(".int") );
						Success = GConfig->GetSection( TEXT("Public"), Buffer, ARRAY_COUNT(Buffer), Filename );
					}
					if( Success )
					{
						TCHAR* Next;
						for( TCHAR* Key=Buffer; *Key; Key=Next )
						{
							Next = Key + appStrlen(Key) + 1;
							if ( *Key==';' || *Key=='!' )
								continue;

							TCHAR* Value = appStrstr(Key,TEXT("="));
							if( Value )
							{
								*Value++ = 0;
								if( *Value=='(' )
									*Value++=0;
								if( *Value && Value[appStrlen(Value)-1]==')' )
									Value[appStrlen(Value)-1]=0;
								if( appStricmp(Key,TEXT("Object"))==0 )
								{
									FRegistryObjectInfo& Info = *new(GObjDrivers)FRegistryObjectInfo;
									Parse( Value, TEXT("Name="), Info.Object );
									Parse( Value, TEXT("Class="), Info.Class );
									Parse( Value, TEXT("MetaClass="), Info.MetaClass );
									Parse( Value, TEXT("Description="), Info.Description );
									Parse( Value, TEXT("Autodetect="), Info.Autodetect );
								}
								else if( appStricmp(Key,TEXT("Preferences"))==0 )
								{
									FPreferencesInfo& Info = *new(GObjPreferences)FPreferencesInfo;
									Parse( Value, TEXT("Caption="), Info.Caption );
									Parse( Value, TEXT("Parent="), Info.ParentCaption );
									Parse( Value, TEXT("Class="), Info.Class );
									Parse( Value, TEXT("Category="), Info.Category );
									ParseUBOOL( Value, TEXT("Immediate="), Info.Immediate );
									Info.Category.SetFlags( RF_Native );
								}
							}
						}
					}
				}
			}
		}

		TArray<FString> CoreLangs = GFileManager->FindFiles( TEXT("Core.*"), 1, 0 );

		for( INT i=0; i<CoreLangs.Num(); i++ )
		{
			FString Ext;
			if( CoreLangs(i).Split(FString(TEXT(".")),NULL,&Ext,1) )
			{
				Ext = FString(TEXT(".")) + Ext;
				if( Ext!=DLLEXT && Ext!=TEXT(".u") && Ext!=TEXT(".ilk") )
				{
					FRegistryObjectInfo& Info = *new(GObjDrivers)FRegistryObjectInfo;
					Info.Object		= appStrstr(*CoreLangs(i),TEXT("."))+1;
					Info.Class		= TEXT("Class");
					Info.MetaClass	= ULanguage::StaticClass()->GetPathName();
				}
			}
		}
	}
	unguard;
}

//
// Get a list of globally registered objects complying to 
// the specified class and optional metaclass (base class of UClasses).
//
void UObject::GetRegistryObjects
(
	TArray<FRegistryObjectInfo>&	Results,
	UClass*							Class,
	UClass*							MetaClass,
	UBOOL							ForceRefresh
)
{
	guard(UObject::GetDriverClasses);
	check(Class);
	check(Class!=UClass::StaticClass() || MetaClass);
	CacheDrivers( ForceRefresh );
	const TCHAR* ClassName = Class->GetName();
	const TCHAR* MetaClassName = MetaClass ? MetaClass->GetPathName() : TEXT("");
	for( INT i=0; i<GObjDrivers.Num(); i++ )
	{
		if
		(	appStricmp(*GObjDrivers(i).Class, ClassName)==0
		&&	appStricmp(*GObjDrivers(i).MetaClass, MetaClassName)==0 )
			new(Results)FRegistryObjectInfo(GObjDrivers(i));
	}
	unguard;
}

//
// Get a list of everything that should show up in the hierarchical
// preferences window.
//
void UObject::GetPreferences( TArray<FPreferencesInfo>& Results, const TCHAR* ParentCaption, UBOOL ForceRefresh )
{
	guard(UObject::GetPreferences);
	CacheDrivers( ForceRefresh );
	Results.Empty();
	for( INT i=0; i<GObjPreferences.Num(); i++ )
		if( appStricmp(*GObjPreferences(i).ParentCaption,ParentCaption)==0 )
			new(Results)FPreferencesInfo(GObjPreferences(i));
	unguard;
}

/*-----------------------------------------------------------------------------
	UTextBuffer implementation.
-----------------------------------------------------------------------------*/

UTextBuffer::UTextBuffer( const TCHAR* InText )
: Text( InText )
{}
void UTextBuffer::Serialize( const TCHAR* Data, EName Event )
{
	guard(UTextBuffer::Serialize);
	Text += (TCHAR*)Data;
	unguardobj;
}
void UTextBuffer::Serialize( FArchive& Ar )
{
	guard(UTextBuffer::Serialize);
	Super::Serialize(Ar);
	Ar << Pos << Top << Text;
	unguardobj;
}
IMPLEMENT_CLASS(UTextBuffer);

/*-----------------------------------------------------------------------------
	UEnum implementation.
-----------------------------------------------------------------------------*/

UEnum::UEnum( UEnum* InSuperEnum )
: UField( InSuperEnum )
{}
void UEnum::Serialize( FArchive& Ar )
{
	guard(UEnum::Serialize);
	Super::Serialize(Ar);
	Ar << Names;
	unguardobj;
}
IMPLEMENT_CLASS(UEnum);

/*-----------------------------------------------------------------------------
	FCompactIndex implementation.
-----------------------------------------------------------------------------*/

//
// FCompactIndex serializer.
//
FArchive& operator<<( FArchive& Ar, FCompactIndex& I )
{
	guard(FCompactIndex<<);
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << I.Value;
	}
	else
	{
		INT		Original = 0;
		DWORD	V        = 0;
		BYTE	B0       = 0;
		if( !Ar.IsLoading() )
		{
			Original = I.Value;
			V        = Abs(I.Value);
			B0       = ((I.Value>=0) ? 0 : 0x80) + ((V < 0x40) ? V : ((V & 0x3f)+0x40));
		}
		I.Value      = 0;
		Ar << B0;
		if( B0 & 0x40 )
		{
			V >>= 6;
			BYTE B1 = (V < 0x80) ? V : ((V & 0x7f)+0x80);
			Ar << B1;
			if( B1 & 0x80 )
			{
				V >>= 7;
				BYTE B2 = (V < 0x80) ? V : ((V & 0x7f)+0x80);
				Ar << B2;
				if( B2 & 0x80 )
				{
					V >>= 7;
					BYTE B3 = (V < 0x80) ? V : ((V & 0x7f)+0x80);
					Ar << B3;
					if( B3 & 0x80 )
					{
						V >>= 7;
						BYTE B4 = V;
						Ar << B4;
						I.Value = B4;
					}
					I.Value = (I.Value << 7) + (B3 & 0x7f);
				}
				I.Value = (I.Value << 7) + (B2 & 0x7f);
			}
			I.Value = (I.Value << 7) + (B1 & 0x7f);
		}
		I.Value = (I.Value << 6) + (B0 & 0x3f);
		if( B0 & 0x80 )
			I.Value = -I.Value;
		if( Ar.IsSaving() && I.Value!=Original )
			appErrorf( TEXT("Mismatch: %08X %08X"), I.Value, Original );
	}
	return Ar;
	unguard;
}

/*-----------------------------------------------------------------------------
	Implementations.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(ULinker);
IMPLEMENT_CLASS(ULinkerLoad);
IMPLEMENT_CLASS(ULinkerSave);
IMPLEMENT_CLASS(USubsystem);

/*-----------------------------------------------------------------------------
	UCommandlet.
-----------------------------------------------------------------------------*/

UCommandlet::UCommandlet()
: HelpCmd(E_NoInit), HelpOneLiner(E_NoInit), HelpUsage(E_NoInit), HelpWebLink(E_NoInit)
{}
INT UCommandlet::Main( const TCHAR* Parms )
{
	guard(UCommandlet::Main);
	return eventMain( Parms );
	unguard;
}
void UCommandlet::execMain( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UCommandlet::execMain);

	P_GET_STR(Parms);
	P_FINISH;

	*(INT*)Result = Main( *Parms );

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( UCommandlet, INDEX_NONE, execMain );
IMPLEMENT_CLASS(UCommandlet);

/*-----------------------------------------------------------------------------
	UPackage.
-----------------------------------------------------------------------------*/

UPackage::UPackage()
{
	guard(UPackage::UPackage);

	BindPackage( this );
	bDirty = 0;

	unguard;
}
void UPackage::Serialize( FArchive& Ar )
{
	guard(UPackage::Serialize);
	Super::Serialize( Ar );
	unguard;
}
void UPackage::Destroy()
{
	guard(UPackage::Destroy);
	if( DllHandle )
	{
		#if !__STATIC_LINK
		debugf( NAME_Log, TEXT("Unbound to %s%s"), GetName(), DLLEXT );
		#endif
		//Danger: If package is garbage collected before its autoregistered classes,
		//the autoregistered classes will be unmapped from memory.
		//appFreeDllHandle( DllHandle );
	}
	Super::Destroy();
	unguard;
}
void* UPackage::GetDllExport( const TCHAR* ExportName, UBOOL Checked )
{
	guard(UPackage::GetDllExport);
	void* Result;
	#if !__STATIC_LINK
	if( !DllHandle )
	{
		if( Checked && !ParseParam(appCmdLine(),TEXT("nobind")) )
			appErrorf( LocalizeError(TEXT("NotDll"),TEXT("Core")), GetName(), ExportName );
		Result = NULL;
	}
	else
	#endif
	{
		#if __STATIC_LINK
		Result = FindNative( (TCHAR*) ExportName );
		#else
		Result = appGetDllExport( DllHandle, ExportName );
		#endif
		if( !Result && Checked && !ParseParam(appCmdLine(),TEXT("nobind")) )
			appErrorf( LocalizeError(TEXT("NotInDll"),TEXT("Core")), ExportName, GetName() );
		if( Result )
			debugfSlow( NAME_DevBind, TEXT("Found %s in %s%s"), ExportName, GetName(), DLLEXT );
		
	}
	return Result;
	unguard;
}
IMPLEMENT_CLASS(UPackage);

/*-----------------------------------------------------------------------------
	UTextBufferFactory.
-----------------------------------------------------------------------------*/

void UTextBufferFactory::StaticConstructor()
{
	guard(UTextBufferFactory::StaticConstructor);

	SupportedClass = UTextBuffer::StaticClass();
	bCreateNew     = 0;
	bText          = 1;
	new(Formats)FString( TEXT("txt;Text files") );

	unguard;
}
UTextBufferFactory::UTextBufferFactory()
{}
UObject* UTextBufferFactory::FactoryCreateText
(
	ULevel*				InLevel,
	UClass*				Class,
	UObject*			InOuter,
	FName				InName,
	DWORD				InFlags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(UTextBufferFactory::FactoryCreateText);

	// Import.
	UTextBuffer* Result = new(InOuter,InName,InFlags)UTextBuffer;
	Result->Text = Buffer;
	return Result;

	unguard;
}
IMPLEMENT_CLASS(UTextBufferFactory);

/*----------------------------------------------------------------------------
	ULanguage.
----------------------------------------------------------------------------*/

const TCHAR* UObject::GetLanguage()
{
	guard(UObject::GetLanguage);
	return GLanguage;
	unguard;
}
void UObject::SetLanguage( const TCHAR* LangExt )
{
	guard(UObject::SetLanguage);
	if( appStricmp(LangExt,GLanguage)!=0 )
	{
		appStrcpy( GLanguage, LangExt );
		appStrcpy( GNone,  LocalizeGeneral( TEXT("None"),  TEXT("Core")) );
		appStrcpy( GTrue,  LocalizeGeneral( TEXT("True"),  TEXT("Core")) );
		appStrcpy( GFalse, LocalizeGeneral( TEXT("False"), TEXT("Core")) );
		appStrcpy( GYes,   LocalizeGeneral( TEXT("Yes"),   TEXT("Core")) );
		appStrcpy( GNo,    LocalizeGeneral( TEXT("No"),    TEXT("Core")) );
		for( FObjectIterator It; It; ++It )
			It->LanguageChange();
	}
	unguard;
}
IMPLEMENT_CLASS(ULanguage);


static void FindObjRefs(UObject *checkObj)
{
	//debugf(TEXT("####### searching for ref to (%s)"),checkObj->GetFullName());
	for (FObjectIterator It; It; ++It)
	{
		// Iterate it's object properties and figure out if there is a
		// reference to this destroyed object in an object's properties
		for (TFieldIterator<UProperty>
			FieldIt(It->GetClass()); FieldIt; ++FieldIt)
		{
			// If this is an object property
			if (Cast<UObjectProperty>(*FieldIt))
			{
				// Loop through static arrays of objects
				for (INT Index = 0; Index < FieldIt->ArrayDim; Index++)
				{
					// Get the object pointer
					UObject* Obj = *(UObject**)(((BYTE*)*It) +
						FieldIt->Offset + (Index * FieldIt->ElementSize));
					// If they match, bad things will happen once the
					// actor in question is deleted
					if (Obj == checkObj)
					{
						debugf(TEXT("######## (%s) ref'd by (%s) [property: %s]"),
							checkObj->GetFullName(),It->GetFullName(),FieldIt->GetFullName());
					}
				}
			}
			else
			{
				// Check for a dyn array of objects
				UArrayProperty* ArrayProp = Cast<UArrayProperty>(*FieldIt);
				if (ArrayProp && Cast<UObjectProperty>(ArrayProp->Inner))
				{
					// Get the dynamic array pointer
					FArray* Array = (FArray*)(((BYTE*)*It) + FieldIt->Offset);
					// Now iterate through each element and check
					for (INT Index = 0; Index < Array->Num(); Index++)
					{
						// Get the object at the specified index
						UObject* Obj = *(UObject**)((BYTE*)Array->GetData() +
							Index * ArrayProp->Inner->ElementSize);
						// If they match, bad things will happen once the
						// actor in question is deleted
						if (Obj == checkObj)
						{
							debugf(TEXT("######## (%s) ref'd by (%s) [property: %s]"),
								checkObj->GetFullName(),It->GetFullName(),FieldIt->GetFullName());
						}
					}
				}
			}
		}
	}
}

/* scion ======================================================================
 * UObject::CheckDanglingRefs
 * Author: jg
 *
 * Searches through all objects checking for any object that still maintains a
 * reference to the passed in object. Slow as hell, but faster than chasing
 * crashes down.
 *
 * input:	pDeadObject -- The object that is deemed dead and not usable
 *
 * ============================================================================
 */
void UObject::CheckDanglingRefs(UObject* pDeadObject)
{
	guard(UObject::CheckDanglingRefs);
	check(pDeadObject);
	// Iterate all objects and check for object still holding a ref to this
	// object
	for (FObjectIterator It; It; ++It)
	{
		if ( It->IsPendingKill() )
			continue;

		// Iterate it's object properties and figure out if there is a
		// reference to this destroyed object in an object's properties
		for (TFieldIterator<UProperty>
			FieldIt(It->GetClass()); FieldIt; ++FieldIt)
		{
			// If this is an object property
			if (Cast<UObjectProperty>(*FieldIt))
			{
				// Loop through static arrays of objects
				for (INT Index = 0; Index < FieldIt->ArrayDim; Index++)
				{
					// Get the object pointer
					UObject* Obj = *(UObject**)(((BYTE*)*It) +
						FieldIt->Offset + (Index * FieldIt->ElementSize));
					// If they match, bad things will happen once the
					// actor in question is deleted
					if (Obj == pDeadObject)
					{
						debugf(TEXT("########Reference to destroyed object (%s) [property: %s] [obj: %s]"),
							pDeadObject->GetFullName(),FieldIt->GetFullName(),It->GetFullName());
						FindObjRefs(*It);
					}
				}
			}
			else
			{
				// Check for a dyn array of objects
				UArrayProperty* ArrayProp = Cast<UArrayProperty>(*FieldIt);
				if (ArrayProp && Cast<UObjectProperty>(ArrayProp->Inner))
				{
					// Get the dynamic array pointer
					FArray* Array = (FArray*)(((BYTE*)*It) + FieldIt->Offset);
					// Now iterate through each element and check
					for (INT Index = 0; Index < Array->Num(); Index++)
					{
						// Get the object at the specified index
						UObject* Obj = *(UObject**)((BYTE*)Array->GetData() +
							Index * ArrayProp->Inner->ElementSize);
						// If they match, bad things will happen once the
						// actor in question is deleted
						if (Obj == pDeadObject)
						{
							debugf(TEXT("########Reference to destroyed object (%s) [property: %s]"),
								pDeadObject->GetFullName(),FieldIt->GetFullName());
							FindObjRefs(*It);
						}
					}
				}
			}
		}
	}
	unguard;
}

/* scion ======================================================================
 * UObject::CheckDanglingOuter
 * Author: jg
 *
 * Verify that no objects have this object as an outer. This is a very slow
 * safety net that should only be used outside of production
 *
 * input:	pDeadObject -- The object that is deemed dead and not usable
 *
 * ============================================================================
 */
void UObject::CheckDanglingOuter(UObject* pDeadObject)
{
	guard(UObject::CheckDanglingOuter);
	check(pDeadObject);
	// Iterate all objects and check for an outer of this actor
	for (FObjectIterator It; It; ++It)
	{
		// This is mega bad (GC crash forthcoming) so log it
		if (pDeadObject == It->GetOuter())
		{
			debugf(TEXT("******Object (%s) has a destroyed outer of (%s)"),
				It->GetFullName(),pDeadObject->GetFullName());
		}
	}
	unguard;
}
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

