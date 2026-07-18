/*=============================================================================
	UnPlayInfo.cpp: PlayInfo native functions.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Michel Comeau
		* Struct, Array, and sorting support added by Ron Prestenback

	PlayInfo is a class that can handle modifying other classes properties
	defined as Config/GlobalConfig as a batch. It validates any new values
	sent its way with the property itself, except for strings. It can only
	handle simple types of values, like string, int, bool, byte, enum(?)
	and float. New Values are not modifying the handled classes defaults
	directly but in a batch when SaveSettings() is issued.

=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UPlayInfo);

#define PLAYINFOERROR(action,prop,msg) LastError = FString::Printf(TEXT("%s for %s: %s"), TEXT(#action), prop, msg)

// 0 = Grouping, 1 = Weight, 2 = RenderType, 3 = DisplayName, 4 = SettingName, 5 = SecLevel
static BYTE SortType;

// This comparison structure provides for optimal sorting by allowing the sorting to continue to fall through
// to another sorting method to create a more aesthetically pleasing layout for settings
static inline INT Compare(FPlayInfoData& SettingA, FPlayInfoData& SettingB)
{
	INT Result;
	switch (SortType)
	{
	case 0:
	case 6:
		Result = SortType == 0 ?
			appStrcmp( *SettingA.Grouping, *SettingB.Grouping ) : appStrcmp( *SettingB.Grouping, *SettingA.Grouping );
		if (Result != 0)
			return Result;

	case 1:
		Result = SettingA.Weight - SettingB.Weight;
		if (Result != 0)
			return Result;

	case 2:
		Result = SettingA.RenderType - SettingB.RenderType;
		if ( Result == 0 )
			Result = (SettingA.ArrayDim + SettingA.bStruct) - (SettingB.bStruct + SettingB.ArrayDim);

		if (Result != 0)
			return Result;

	case 3:
		Result = appStrcmp( *SettingA.DisplayName, *SettingB.DisplayName );
		if (Result != 0)
			return Result;

	case 4:
		Result = appStrcmp( *SettingA.SettingName, *SettingB.SettingName );
		if (Result != 0)
			return Result;

	case 5:
		Result = SettingA.SecLevel - SettingB.SecLevel;
		if (Result != 0)
			return Result;

		break;

	case 7:
		Result = SettingB.Weight - SettingA.Weight;
		if (Result != 0)
			return Result;

	case 8:
		Result = SettingB.RenderType - SettingA.RenderType;
		if ( Result == 0 )
			Result = (SettingB.ArrayDim + SettingB.bStruct) - (SettingA.bStruct + SettingB.ArrayDim);

		if (Result != 0)
			return Result;

	case 9:
		Result = appStrcmp( *SettingB.DisplayName, *SettingA.DisplayName );
		if (Result != 0)
			return Result;

	case 10:
		Result = appStrcmp( *SettingB.SettingName, *SettingA.SettingName );
		if (Result != 0)
			return Result;

	case 11:
		Result = SettingB.SecLevel - SettingA.SecLevel;
		if (Result != 0)
			return Result;

	}
	return 0;
}

// 0 = Grouping, 1 = Weight, 2 = RenderType, 3 = DisplayName, 4 = SettingName, 5 = SecLevel
// Equal items will be sorted by the next highest type
//native(709) final function Sort(byte SortingMethod);
void UPlayInfo::execSort( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UPlayInfo::execSort);

	P_GET_BYTE(SortingMethod);
	P_FINISH;

	if (SortingMethod >= 0 && SortingMethod < 11)
		SortType = SortingMethod;

	Sort( &Settings(0), Settings.Num() );

	SortType = 0;

	unguardexecSlow;
}

static void SplitIntoArray( FString Value, TArray<FString>& Parts )
{
	guardSlow(UnPlayInfo::SplitIntoArray);

	Parts.Empty();

	if ( Value == TEXT("") )
		return;

	TCHAR* Str = (TCHAR*)Value.GetCharArray().GetData();

	if ( *Str == '(' )
		Str++;

	TCHAR* end = &Str[appStrlen(Str) - 1];
	if ( *end == ')' )
		*end-- = 0;

	bool literal = false;
	INT cnt = 0;

	TCHAR* Cur = Str;
	while ( *Cur )
	{
		if ( cnt == 0 && *Cur == '"' )
		{
			Cur++;
			literal = !literal;
			continue;
		}

		if ( literal )
		{
			Cur++;
			continue;
		}

		if ( *Cur == '(' )
		{
			Cur++;
			cnt++;
		}
		else if ( *Cur == ')' )
		{
			Cur++;
			cnt--;
		}

		if ( cnt > 0 )
		{
			Cur++;
			continue;
		}

		if ( *Cur == ',' )
		{
			*Cur++ = 0;
			end = &Str[appStrlen(Str) - 1];
			if ( *Str == '"' && *end == '"' )
			{
				*Str++ = 0;
				*end-- = 0;
			}
			new(Parts) FString(Str);
			Str = Cur;
			continue;
		}

		Cur++;
	}

	if ( *Str == '"' && *end == '"' )
	{
		*Str++ = 0;
		*end-- = 0;
	}
	new(Parts) FString(Str);

	unguardSlow;
}

// native(700) final function bool Clear();
void UPlayInfo::execClear( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UPlayInfo::execClear);

	P_FINISH;

	Settings.Empty();
	Groups.Empty();
	ClassStack.Empty();
	InfoClasses.Empty();
	*(UBOOL*)Result = 0;

	unguardexecSlow;
}

// native(701) final function bool AddClass(class<Info> Class);
void UPlayInfo::execAddClass( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UPlayInfo::execAddClass);

	P_GET_OBJECT(UClass,Class);
	P_FINISH;

	if ( !Class )
	{
		UClass* badclass = NULL;
		INT last = ClassStack.Num() ? ClassStack.Last() : INDEX_NONE;
		if ( last != INDEX_NONE && last >= 0 && last < InfoClasses.Num() )
			badclass = InfoClasses(last);

		debugf(NAME_Warning, TEXT("%s called PlayInfo.AddClass() with AddingClass == None!"), badclass ? badclass->GetName() : TEXT("Someone"));
		*(UBOOL*)Result = 0;
		return;
	}

	for (INT i = 0; i < InfoClasses.Num(); i++)
		if (InfoClasses(i) == Class)
		{
			PLAYINFOERROR(AddClass,Class->GetFullName(),TEXT("Class already in PlayInfo!"));
			*(UBOOL*)Result = 0;
			return;
		}

	ClassStack.AddItem( InfoClasses.AddItem(Class) );
	*(UBOOL*)Result = 1;

	unguardexecSlow;
}


// native(702) final function bool RemoveClass(class<Info> Class);
void UPlayInfo::execRemoveClass( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UPlayInfo::execRemoveClassSettings);
	P_GET_OBJECT(UClass, Class);
	P_FINISH;

	UBOOL& bResult = *(UBOOL*)Result = 0;
	if ( !Class )
	{
		UClass* badclass = NULL;
		INT last = ClassStack.Num() ? ClassStack.Last() : INDEX_NONE;
		if ( last != INDEX_NONE && last >= 0 && last < InfoClasses.Num() )
			badclass = InfoClasses(last);
		debugf(TEXT("%s called PlayInfo.RemoveClass() with RemovingClass == None!"), badclass ? badclass->GetName() : TEXT("Someone"));
		return;
	}

	TArray<INT> TaggedGroups;
	INT InfoIdx = InfoClasses.FindItemIndex(Class);
	INT StackIdx = ClassStack.FindItemIndex(InfoIdx);

	if (StackIdx == INDEX_NONE)	// if this class is not in the class stack, check if we can remove it
		if (!Cast<AInfo>(Class->GetDefaultObject())->eventAllowClassRemoval())
			return;

	bResult = 1;
	for ( INT i = Settings.Num() - 1; i >= 0; i-- )
	{
		if (Settings(i).ClassFrom == Class)
		{
			INT GroupIdx = Groups.FindItemIndex(Settings(i).Grouping);
			TaggedGroups.AddUniqueItem(GroupIdx);
	
			Settings.Remove(i);
		}
	}

	if (InfoIdx != INDEX_NONE)
	{
		if (StackIdx != INDEX_NONE)
			ClassStack.Remove(StackIdx);

		// Correct any stack discrepancies
		for ( INT i = InfoIdx + 1; i < InfoClasses.Num(); i++ )
		{
			INT idx = ClassStack.FindItemIndex(i);
			if ( idx != INDEX_NONE )
				ClassStack(idx) = i - 1;
		}
		InfoClasses.Remove(InfoIdx);
	}

		INT j=0;
	// Remove group from Groups stack if all settings for this group were removed
	for ( INT i = TaggedGroups.Num() - 1; i >= 0; i-- )
	{
		for (j = 0; j < Settings.Num(); ++j)
		{
			if (Settings(j).Grouping == Groups(TaggedGroups(i)))
				break;
		}

		if (j < Settings.Num())
			continue;

		Groups.Remove(TaggedGroups(i));
	}
	
	unguardexecSlow;
}

// PopClass is used after a different class has been added and we need
// to come back to continue adding settings to the original class
// native(703) final function bool PopClass();
void UPlayInfo::execPopClass( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UPlayInfo::execPopClass);
	P_FINISH;

	if ( ClassStack.Num() )
	{
		*(UBOOL*)Result = 1;
        ClassStack.Pop();
		return;
	}

	PLAYINFOERROR(PopClass,TEXT(""),TEXT("Class stack is empty!"));
	*(UBOOL*)Result = 0;

	unguardexecSlow;
}


// native(704) final function bool AddSetting(string Group, string PropertyName, string Description, byte SecLevel, byte Weight, string RenderType, optional string Extras, optional string ExtraPrivs, optional bool bMultiPlayerOnly, optional bool bAdvanced);
void UPlayInfo::execAddSetting( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UPlayInfo::execAddSetting);
	P_GET_STR(Group);
	P_GET_STR(PropertyName);
	P_GET_STR(DisplayName);
	P_GET_BYTE(SecLevel);
	P_GET_BYTE(Weight);
	P_GET_STR(RenderType);
	P_GET_STR_OPTX(Extras, TEXT(""));
	P_GET_STR_OPTX(ExtraPrivs, TEXT(""));
	P_GET_UBOOL_OPTX(bMPOnly,0);
	P_GET_UBOOL_OPTX(bAdvanced,0);
	P_FINISH;

	UBOOL& bResult = *(UBOOL*)Result = 0;

	UProperty *Prop = NULL;
	INT i = 0;

//	debugf(TEXT("PlayInfo.AddSettings: PropName='%s', DispName='%s'"), (const TCHAR *)*PropertyName, (const TCHAR *)*DisplayName);
	// First, Context validity checks

	if (ClassStack.Num() < 1)
	{
		PLAYINFOERROR(AddSetting,*PropertyName,TEXT("No classes in the stack!"));
		debugf(NAME_Warning, TEXT("Invalid call to AddSetting() with no classes in the stack!"));
		return;
	}

	// 2nd, Set which class we will find the property in
	INT last = ClassStack.Last();

	class UClass* Class = NULL;
	if ( last >= 0 && last < InfoClasses.Num() )
		Class = InfoClasses(last);

	if ( !Class )
	{
		PLAYINFOERROR(AddSetting,*PropertyName,TEXT("ClassStack corrupted!"));
		debugf(NAME_Warning, TEXT("ClassStack[%i] referred to invalid InfoClasses index %i - InfoClasses.Length: %i"), ClassStack.Num() - 1, last, InfoClasses.Num());
		return;
	}

	// First, Find the required property
	for ( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Class); It; ++It )
		if ( (It->PropertyFlags & CPF_Config) && (It->GetFName() != NAME_None) && (PropertyName == It->GetName()) )
		{
			Prop = *It;
			break;
		}
	
	// Reject when property name is invalid
	if (!Prop)
	{
		PLAYINFOERROR(AddSetting,*PropertyName,TEXT("Property not found"));
		debugf(NAME_Warning, TEXT("Property '%s' not found in class '%s'"), *PropertyName, Class->GetFullName());
		return;
	}

	// Give a chance to the class to accept/reject adding the property in the list
	if (!Cast<AInfo>(Class->GetDefaultObject())->eventAcceptPlayInfoProperty(PropertyName))
	{
		PLAYINFOERROR(AddSetting,Prop->GetFullName(),TEXT("Class didn't accept property!"));
		return;
	}

	// Check for duplicates
	for (i=0; i<Settings.Num(); i++)
	{
		// If the property is already in the list
		if ( Settings(i).ThisProp == Prop )
		{
			// If the former property was added from this class, reject (class adding same variable twice - error, so log)
			if ( Settings(i).ClassFrom == Class )
			{
				PLAYINFOERROR(AddSetting,Prop->GetFullName(),TEXT("Class added duplicate property"));
				debugf(NAME_Warning, TEXT("Attempting to add duplicate property '%s' to PlayInfo from %s!"), Prop->GetFullName(), Class->GetName());
				return;
			}

			// If this property is globalconfig, it should only exist in the list once, or results will be unpredictable
			if ( Settings(i).bGlobal )
			{
				PLAYINFOERROR(AddSetting,Prop->GetFullName(), *FString::Printf(TEXT("Globalconfig property already added by %s."), Settings(i).ClassFrom->GetName()));
				return;
			}

			// If the property was already added from a parent class, we're in a linked list,
			// where the base class has added a property (not good practice), since each child will then be forced to
			// attempt to add this property as well....just reject silently
			if ( Class->IsChildOf(Settings(i).ClassFrom)  )
			{
				PLAYINFOERROR(AddSetting,Prop->GetFullName(), *FString::Printf(TEXT("Property already added by parent class %s"), Settings(i).ClassFrom->GetName()));
				return;
			}
		}
	}

	// Property ok to add to list
	FPlayInfoData PID;
	appMemzero( &PID, sizeof(FPlayInfoData) );

	// Get the mouse-over description for this property
	PID.Description = Cast<AInfo>(Class->GetDefaultObject())->eventGetDescriptionText(PropertyName);
	if ( PID.Description == TEXT("") )
		debugf(NAME_MenuText, TEXT("No description configured for property '%s'"), *PropertyName);

	TCHAR* TempStr = NULL;

	// Allocate a much larger buffer for arrays, strings, and structs
	if ( Prop->ArrayDim == 1 )
	{
		PID.ArrayDim = INDEX_NONE;
        if ( Prop->IsA(UStrProperty::StaticClass()) )
			TempStr = new TCHAR [1024];

		else if ( Prop->IsA(UArrayProperty::StaticClass()) )
		{
			PID.ArrayDim = 0;
			TempStr = new TCHAR [4096];
		}

		else if ( Prop->IsA(UStructProperty::StaticClass()) )
		{
			PID.bStruct = 1;
			TempStr = new TCHAR [4096];
		}

		else TempStr = new TCHAR [128];
	}
	else
	{
		PID.ArrayDim = Prop->ArrayDim;
		TempStr = new TCHAR [4096];
	}


	if ( !TempStr )
	{
		PLAYINFOERROR(AddSetting,Prop->GetFullName(),TEXT("Property value too large for internal buffer!"));
		debugf(NAME_Warning, TEXT("Memory allocation failure while adding property '%s' to PlayInfo!  Property couldn't be added!"), *PropertyName);
		return;
	}

	*TempStr = 0;
	if ( Prop->ArrayDim == 1 )
	{
		if ( Prop->ExportText( 0, TempStr, (BYTE*)Class->GetDefaultObject(), &Class->Defaults(0), PPF_Localized) )
			PID.Value = TempStr;
	}
	else
	{
		for ( INT i = 0; i < Prop->ArrayDim; i++ )
		{
			if ( PID.Value == TEXT("") )
				PID.Value = TEXT("(");
			else if (*TempStr)
				PID.Value += TEXT(",");

			if ( Prop->ExportText( i, TempStr, (BYTE*)Class->GetDefaultObject(), &Class->Defaults(0), PPF_Localized | PPF_Delimited) )
				PID.Value += TempStr;
		}

		PID.Value += TEXT(")");
	}

	delete TempStr;
	TempStr = NULL;

	PID.ThisProp = Prop;
	PID.ClassFrom = Class;
	PID.SettingName = FString::Printf(TEXT("%s.%s"), Class->GetName(), Prop->GetName());
	PID.DisplayName = DisplayName;
	PID.Grouping = Group;
	PID.SecLevel = SecLevel;
	PID.Weight = Weight;
	PID.Data = Extras;
	PID.ExtraPriv = ExtraPrivs;
	PID.bGlobal = Prop->PropertyFlags & CPF_GlobalConfig;
	PID.bMPOnly = bMPOnly;
	PID.bAdvanced = bAdvanced;

	if ( RenderType == TEXT("Check") )
		PID.RenderType = PIT_Check;
	else if ( RenderType == TEXT("Select") )
		PID.RenderType = PIT_Select;
	else if ( RenderType == TEXT("Text") )
		PID.RenderType = PIT_Text;
	else PID.RenderType = PIT_Custom;

	// Update the Groups array
	if (Groups.FindItemIndex(PID.Grouping) == INDEX_NONE)
	{
		// Find Sorted Insertion Point
		for (i = 0; i<Groups.Num(); i++)
			if (appStrcmp(*PID.Grouping, *Groups(i)) < 0)
				break;

		Groups.InsertZeroed(i);
		Groups(i) = *PID.Grouping;
	}

	// Find index to insert

	SortType = 0;
	for (i = 0; i<Settings.Num(); i++)
	{
		if ( Compare( PID, Settings(i) ) < 0 )
			break;
	}

	Settings.InsertZeroed(i);
	Settings(i) = PID;
	bResult = 1;

	unguardexecSlow;
}

// native(705) final function bool SaveSettings();	// Saves stored settings to ini file
void UPlayInfo::execSaveSettings( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UPlayInfo::execSaveSettings);
	P_FINISH;

	INT i;
	TArray<FString> values;
	TArray<UClass*> Classes;

	// For each item in the list
	for (i=0; i<Settings.Num(); i++)
	{
		// Copy the Value over
		FPlayInfoData& PID = Settings(i);
		UProperty *Prop    = PID.ThisProp;

		BYTE* Data = &PID.ClassFrom->Defaults(Prop->Offset);

		if ( Prop->ArrayDim == 1 || Prop->IsA(UArrayProperty::StaticClass()) )
			Prop->ImportText(*PID.Value, Data, PPF_Localized);
		else
		{
			SplitIntoArray( PID.Value, values );
			for ( INT i = 0; i < Prop->ArrayDim && i < values.Num(); i++ )
				Prop->ImportText( *values(i), Data + i * Prop->ElementSize, PPF_Localized );
		}

		Classes.AddUniqueItem(PID.ClassFrom);
	}

	for ( INT i = 0; i < Classes.Num(); i++ )
		Classes(i)->GetDefaultObject()->SaveConfig();

	*(UBOOL *)Result = Settings.Num()>0;
	unguardexecSlow;
}

// native(706) final function bool StoreSetting(int index, coerce string NewVal, optional string RangeData);	// Only validates and sets Settins[index].Value to passed value
void UPlayInfo::execStoreSetting( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UPlayInfo::execStoreSetting);
	P_GET_INT(Index);
	P_GET_STR(NewVal);
	P_GET_STR_OPTX(DataRange, TEXT(""));
	P_FINISH;

	UBOOL& bResult = *(UBOOL*)Result = 0;

	if ( Index >= 0 && Index < Settings.Num() )
	{
		FPlayInfoData& PID = Settings(Index);

		bResult = 1;
		UProperty *Prop = PID.ThisProp;
		check(Prop);

		if ( DataRange == TEXT("") )
			DataRange = PID.Data;

//	debugf(TEXT("PlayInfo.StoreSettings: PropName='%s', DispName='%s', OldValue='%s', NewValue='%s'"), *Settings(Index).SettingName, *Settings(Index).DisplayName, *Settings(Index).Value, *NewVal);
		// Checkboxes return "" when unchecked

		switch ( PID.RenderType )
		{
		case PIT_Check:
			if ( NewVal != TEXT("True") )
				NewVal = TEXT("False");

		case PIT_Custom:
		case PIT_Select:
			PID.Value = NewVal;
			break;

		case PIT_Text:
			if ( NeedsValidation( Prop ) && !ValidateRange( Prop, NewVal, DataRange ) )
			{
				PLAYINFOERROR(StoreSetting,Prop->GetFullName(),*FString::Printf(TEXT("Value out of range %s."), *NewVal));
				bResult = 0;
				return;
			}

			PID.Value = NewVal;
			break;
		}

		PID.Data = DataRange;
	}
	else
		PLAYINFOERROR(StoreSetting,TEXT(""),TEXT("Invalid Setting Index"));

	unguardexecSlow;
}

// native(707) final function bool GetSettings(string GroupName, out array<PlayInfoData> GroupSettings);	// rjp
void UPlayInfo::execGetSettings( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UPlayInfo::execGetSettings);

	P_GET_STR(GroupName);
	P_GET_TARRAY_REF(GroupSettings, FPlayInfoData);
	P_FINISH;

	ReturnFilteredSettings(*GroupSettings, GroupName);
	*(UBOOL*)Result = 1;

	unguardexecSlow;
}

// native(708) final function int  FindIndex(string SettingName);
void UPlayInfo::execFindIndex( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UPlayInfo::execFindIndex);

	P_GET_STR(SettingName);
	P_FINISH;

	INT& i = *(INT*)Result;
	INT pos = SettingName.InStr(TEXT("."));

	FString ShortName;
	for (i = 0; i<Settings.Num(); i++)
	{
		if ( pos != INDEX_NONE )
		{
			if ( SettingName == Settings(i).SettingName )
				return;
		}
		else
		{
			ShortName = Settings(i).SettingName.Mid( Settings(i).SettingName.InStr(TEXT(".")) + 1 );
			if ( SettingName == ShortName )
				return;
		}
	}

	i = INDEX_NONE;
	unguardexecSlow;
}

void UPlayInfo::ReturnFilteredSettings( TArray<FPlayInfoData>& PIData, const FString& GroupName )
{
	guard(UPlayInfo::ReturnFilteredSettings);

	PIData.Empty();
	INT i;
	for (i = 0; i < Settings.Num(); i++)
	{
		if ( GroupName == TEXT("") || Settings(i).Grouping == GroupName)
			new(PIData) FPlayInfoData( Settings(i) );
	}

	unguard;
}

UBOOL UPlayInfo::NeedsValidation( const UProperty* Prop ) const
{
	guard(UPlayInfo::NeedsValidation);

	if ( !Prop )
		return 0;

	if ( Prop->IsA(UArrayProperty::StaticClass()) )
		Prop = ConstCast<UArrayProperty>(Prop)->Inner;

	if ( Prop->IsA(UIntProperty::StaticClass()) )
		return 1;

	if ( Prop->IsA(UFloatProperty::StaticClass()) )
		return 1;

	if ( Prop->IsA(UStrProperty::StaticClass()) )
		return 1;

	return 0;
	unguard
}

UBOOL UPlayInfo::ValidateRange( const UProperty* Prop, FString& NewVal, FString& DataRange ) const
{
	guard(UPlayInfo::ValidateRange);

	FString Left, Temp, Right;

	UBOOL bArray = Prop->ArrayDim > 1 || Prop->IsA(UArrayProperty::StaticClass());
	TArray<FString> Array;
	if ( bArray )
	{
	SplitIntoArray(NewVal, Array);

		if ( Prop->IsA(UArrayProperty::StaticClass()) )
			Prop = ConstCast<UArrayProperty>(Prop)->Inner;
	}

	// HACK to easily support arraydim
	else new(Array) FString(NewVal);

	if (Prop->IsA(UIntProperty::StaticClass()))
	{
		INT RngMin = 0, RngMax = 0, IntVal = 0;
		for ( INT i = 0; i < Array.Num(); i++ )
		{
			// Do a numerical range validation
			if (DataRange.Split(TEXT(";"), &Left, &Temp))
			{
				if (Temp.Split(TEXT(":"), &Left, &Right))
				{

					IntVal = appAtoi(*Array(i));
					RngMin = appAtoi(*Left);
					RngMax = appAtoi(*Right);
					Array(i) = FString::Printf( TEXT("%i"), Clamp<INT>(IntVal,RngMin,RngMax) );
				}
			}
		}
	}

	// Check for Range Validation for Floats
	else if (Prop->IsA(UFloatProperty::StaticClass()))
	{
		FLOAT RngMin = 0.f, RngMax = 0.f, FltVal = 0.f;

		for ( INT i = 0; i < Array.Num(); i++ )
		{
			// Do a numerical range validation
			if (DataRange.Split(TEXT(";"), &Left, &Temp))
			{
				if (Temp.Split(TEXT(":"), &Left, &Right))
				{
					FltVal = appAtof(*Array(i));
					RngMin = appAtof(*Left);
					RngMax = appAtof(*Right);
					Array(i) = FString::Printf(TEXT("%.6f"), Clamp<FLOAT>(FltVal, RngMin, RngMax) );
				}
			}
		}
	}

	else if ( Prop->IsA(UStrProperty::StaticClass()) )
	{
		INT MaxWidth = appAtoi(*DataRange);

		// Crop to max width and re-wrap with quotes
		for ( INT i = 0; i < Array.Num(); i++ )
		{
			if ( Array(i).Len() > MaxWidth )
				Array(i) = bArray ? FString::Printf(TEXT("\"%s\""), *Array(i).Left(MaxWidth)) : Array(i).Left(MaxWidth);
			else if ( bArray )
				Array(i) = FString::Printf(TEXT("\"%s\""), *Array(i));
		}
	}

//	debugf(TEXT("ValidateRange Prop '%s'  bArray:%i  Array.Num():%i"), Prop->GetName(), bArray, Array.Num());
	if ( bArray )
	{
		NewVal = TEXT("(");
		for ( INT i = 0; i < Array.Num(); i++ )
		{
			if ( i > 0 )
				NewVal += TEXT(",");

			NewVal += Array(i);
		}
		NewVal += TEXT(")");
	}
	else
		NewVal = Array(0);
		
	return 1;
	unguard;
}
