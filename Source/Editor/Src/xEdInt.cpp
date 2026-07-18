/*=============================================================================
	xEdInt.cpp: Editor internationalization file import/export routines.
	Copyright 2001 Digital Extremes, Inc. All Rights Reserved.
	Confidential.
=============================================================================*/

#include "EditorPrivate.h"
#include "FConfigCacheIni.h"

static INT Compare (const UObject *p1, const UObject *p2)
{
	return appStrcmp(p1->GetPathName(), p2->GetPathName());
    int rc;

	UBOOL c1 = p1->IsA(UClass::StaticClass()), c2 = p2->IsA(UClass::StaticClass());

	if ( c1 )
	{
		if ( c2 )
			return appStrcmp(p1->GetName(), p2->GetName());

		if ( p2->GetOuter()->IsA(UClass::StaticClass()) )
		{
			rc = appStrcmp(p1->GetName(), p2->GetOuter()->GetName());
			if ( rc != 0 )
				return rc;
		}

		return -1;
	}

	if ( c2 )
	{
		if ( p1->GetOuter()->IsA(UClass::StaticClass()) )
		{
			rc = appStrcmp(p1->GetOuter()->GetName(), p2->GetName());
			if ( rc != 0 )
				return rc;
		}

		return 1;
	}

	c1 = p1->GetOuter()->IsA(UClass::StaticClass()), c2 = p2->GetOuter()->IsA(UClass::StaticClass());
	if ( c1 )
	{
		if ( c2 )
		{
			rc = appStrcmp(p1->GetOuter()->GetName(), p2->GetOuter()->GetName());
			if ( rc == 0 )
				return appStrcmp(p1->GetName(), p2->GetName());

			return rc;
		}

		return -1;
	}

	if ( c2 )
		return 1;

	rc = appStrcmp(p1->GetClass()->GetName(), p2->GetClass()->GetName());
	if ( rc == 0 )
		rc = appStrcmp(p1->GetName(), p2->GetName());

	return rc;
}

static INT GPropertyCount;

static void IntExportProp( UClass* Class, UClass* SuperClass, UClass* OuterClass, UProperty* Prop, const TCHAR *IntName, const TCHAR *SectionName, const TCHAR *KeyPrefix, BYTE* DataBase, INT DataOffset );
static void IntExportStruct( UClass* Class, UClass* SuperClass, UClass* OuterClass, UStruct* Struct, const TCHAR *IntName, const TCHAR *SectionName, const TCHAR *KeyPrefix, BYTE* DataBase, INT DataOffset, bool AtRoot = false );

static void IntExportProp( UClass* Class, UClass* SuperClass, UClass* OuterClass, UProperty* Prop, const TCHAR *IntName, const TCHAR *SectionName, const TCHAR *KeyPrefix, BYTE* DataBase, INT DataOffset )
{
	UStructProperty* StructProperty = Cast<UStructProperty>( Prop );
	if( StructProperty )
	{
        IntExportStruct( Class, SuperClass, OuterClass, StructProperty->Struct, IntName, SectionName, KeyPrefix, DataBase, DataOffset );
        return;
    }

	BYTE* DefaultData = NULL;
	if ( SuperClass && SuperClass->IsChildOf(OuterClass) )
		DefaultData = (BYTE*)&SuperClass->Defaults(0);

	if ( DefaultData && DefaultData != DataBase )
	{
		if ( Prop->Identical(DataBase + DataOffset, DefaultData + DataOffset) )
		{
			if ( !Class )
				return;

			if ( !(SuperClass->ClassFlags & CLASS_NoCacheExport) )
				return;

			if ( Class->ClassFlags & CLASS_NoCacheExport )
				return;

			if ( !(Prop->PropertyFlags & CPF_Cache) && Prop->GetOuter()->IsA(UClass::StaticClass()) )
				return;
		}
	}

	TCHAR RealValue[256 * 1024] = TEXT("");
	//	UArrayProperty* ArrayProp = Cast<UArrayProperty>(Prop);
	//	if ( ArrayProp )
	//		ArrayProp->Inner->ExportTextItem( RealValue, DataBase + DataOffset, NULL, PPF_Localized );
	//	else

	// rjp 2/25/04 -
	/* By passing in DefaultData for the Default param of ExportTextItem, we cause the exporter to only export properties that have been changed
       This works great for configuration values, but causes problems for localized dynamic arrays, e.g.
	   ClassA declares an array of localized strings; the defaultproperties for ClassA contain 5 members
	   ClassB adds an additional member to the end of the array.  If DefaultData is passed into ExportTextItem here,
	   the resulting output looks something like this:

	   (in ClassA)
	   PanelCaption=("Gametype","Select Map","Game Rules","Mutators","Bot Config")

	   (in ClassB)
	   PanelCaption=(,,,,,"Server Rules")


	   The problem:
	   ClassA calls InitClassDefaultObject, which does nothing for this array
	   ClassA calls SerializeTaggedProperties, which fills the valus of the array with the english version
	   ClassA calls LoadLocalized, which fills the value of the array with the localized values

	   ClassB calls InitClassDefaultObject, which copies the array values (which contain the correct localized text) into the default for ClassB
	   ClassB calls SerializeTaggedProperties, which overwrites the values of the array with the binary defaults, i.e. the english text
	   ClassB calls LoadLocalized, which will skip over any blank members of the array, thus only importing the localized version of the final element

	   Thus, even when values of individual members of the array in ClassB is identical to the values in ClassA, those values should be exported
	   in the ClassB section of the localization file as well
	   -- rjp */
	Prop->ExportTextItem( RealValue, DataBase + DataOffset, /*DefaultData ? DefaultData + DataOffset :*/ NULL, PPF_Delimited|PPF_LocalizedOnly );

	if ( appStrlen(RealValue) == 0 || !appStricmp(RealValue,TEXT("\"\"")) )
		return;

/*	INT RealLength = appStrlen( RealValue );
    check( RealLength < ARRAY_COUNT(RealValue) );

    if( ( RealLength == 0 ) || !appStrcmp( RealValue, TEXT("\"\"") ) )
        return;

    if( Class && SuperClass && OuterClass && (OuterClass != Class) )
    {
        // Only export if value has changed from base class:
		TCHAR DefaultValue[256 * 1024];
	    
		BYTE* DefaultDataBase = (BYTE*)&SuperClass->Defaults(0);
/ *		if (ArrayProp)
		{
			FArray* DiffArr = NULL;
			if (SuperClass && DefaultDataBase && SuperClass->IsChildOf(OuterClass))
				DiffArr = (FArray*)(DefaultDataBase + ArrayProp->Offset);

			// TODO What if DiffArr->Num() >= Arr->Num()?  Won't catch the rest of the elements, in that case
			INT i = DataOffset / ArrayProp->Inner->ElementSize;
			ArrayProp->Inner->ExportText( i, DefaultValue, DataBase, DiffArr && i < DiffArr->Num() ? (BYTE*)DiffArr->GetData() : NULL, PPF_Delimited );
		}
		else
		{
* /			Prop->ExportTextItem( DefaultValue, DefaultDataBase + DataOffset, NULL, PPF_Delimited );
			check( appStrlen( DefaultValue ) < ARRAY_COUNT(DefaultValue) );

			if( appStrcmp( DefaultValue, RealValue ) == 0 )
				return;
//		}
    }
*/    
	GConfig->SetString( SectionName, KeyPrefix, RealValue, IntName/*, !ArrayProp*/ );
    GPropertyCount++;
}

static void IntExportStruct( UClass* Class, UClass* SuperClass, UClass* OuterClass, UStruct* Struct, const TCHAR *IntName, const TCHAR *SectionName, const TCHAR *KeyPrefix, BYTE* DataBase, INT DataOffset, bool AtRoot )
{
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It( Struct ); It; ++It )
	{
	    UProperty* Prop = *It;
		if ( !Prop->IsLocalized() )
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

            INT NewOffset = DataOffset + (Prop->Offset) + (i * Prop->ElementSize );

/*			UArrayProperty* ArrayProp = Cast<UArrayProperty>(Prop);
			if (ArrayProp)
			{
				INT InnerSize = ArrayProp->Inner->ElementSize, Count = 0;
				FArray* Array = (FArray*)(DataBase + NewOffset);
				if (Array)
					Count = Array->Num();

				for (INT j = 0; j < Count; j++)
					IntExportProp( Class, SuperClass, AtRoot ? CastChecked<UClass>(Prop->GetOuter()) : OuterClass, ArrayProp, IntName, SectionName, *NewPrefix, (BYTE*)Array->GetData(), j * InnerSize);
			}
			else
 */           IntExportProp( Class, SuperClass, AtRoot ? Prop->GetOwnerClass() : OuterClass, Prop, IntName, SectionName, *NewPrefix, DataBase, NewOffset );
	    }
	}
}

EDITOR_API UBOOL IntExport (UObject *Package, const TCHAR *IntName, UBOOL ExportFresh, UBOOL ExportInstances, UBOOL AutoCheckout)
{
    TArray<UObject *> Objects;
    INT objectNumber;
	
	TCHAR* TmpName = new TCHAR[appStrlen(IntName) + 6];
	appStrcpy(TmpName,IntName);
	appStrncat(TmpName,TEXT(".tmp"),appStrlen(IntName)+6);

	GFileManager->Delete( TmpName, 0, 1);

	FConfigCacheIni* Config = (FConfigCacheIni*)GConfig;

	// rjp --
	// Save any sections that aren't normally exported
	FString PersistantData = TEXT("");
	TArray<FString> StandardSections;
	new(StandardSections) FString(TEXT("DecoText"));
	new(StandardSections) FString(TEXT("Errors"));
	new(StandardSections) FString(TEXT("General"));
	new(StandardSections) FString(TEXT("KeyNames"));
	new(StandardSections) FString(TEXT("Language"));
	new(StandardSections) FString(TEXT("Progress"));
	new(StandardSections) FString(TEXT("Public"));
	new(StandardSections) FString(TEXT("Query"));
	new(StandardSections) FString(TEXT("TcpNetDriver"));
	new(StandardSections) FString(TEXT("UpgradeDrivers"));
	new(StandardSections) FString(TEXT("UdpBeacon"));

	for( FObjectIterator It; It; ++It )
	{
        UObject *Obj = *It;

		if( !Obj->IsIn(Package) )
            continue;

		UClass* Cls = Cast<UClass>(Obj);
		if (Cls && Cls->IsChildOf(UCommandlet::StaticClass()))
			new(StandardSections) FString( Obj->GetName() );

        if( Obj->GetFlags() & (RF_Transient | RF_NotForClient | RF_NotForServer | RF_Destroyed) )
            continue;

		if ( Cls && Cls->ClassFlags & CLASS_Localized )
			Objects.AddItem (Obj);
		else if ( Obj->GetClass()->ClassFlags & CLASS_Localized )
			Objects.AddItem(Obj);
	}


    if( Objects.Num() )
       Sort (&Objects(0), Objects.Num());

	const TCHAR* nl = TEXT("\r\n");
	for (INT i = 0; i < StandardSections.Num(); i++)
	{
		TMultiMap<FString,FString>* Buffer;
		Buffer = Config->GetSectionPrivate( *StandardSections(i), 0, 1, IntName );
		if (Buffer)
		{
			if (PersistantData != TEXT(""))
				PersistantData += nl;

			PersistantData += FString::Printf(TEXT("[%s]%s"),*StandardSections(i), nl);
			for (TMultiMap<FString,FString>::TIterator It(*Buffer); It; ++It)
			{
				UBOOL bNumeric = It.Value().IsNumeric();
				PersistantData += (FString::Printf(TEXT("%s=%s%s%s%s"),*It.Key(),bNumeric?TEXT(""):TEXT("\""),*It.Value(),bNumeric?TEXT(""):TEXT("\""), nl));
			}
		}
	}

	if ( Objects.Num() && PersistantData.Len() )
		appSaveStringToFile( PersistantData, TmpName, GFileManager );


    GPropertyCount = 0;
	UObject* obj = NULL;
	UClass* Class = NULL;
    for (objectNumber = 0; objectNumber < Objects.Num(); objectNumber++)
    {
		obj = Objects(objectNumber);
		if ( !obj )
			continue;

		Class = Cast<UClass>(obj);

		if( Class )
		    IntExportStruct( Class, Class->GetSuperClass(), Class, Class, TmpName, Class->GetName(), NULL, &Class->Defaults(0), 0, true );
		else
		{
			Class = obj->GetClass();
			UClass* SubObjectOuter = Cast<UClass>(obj->GetOuter());
			if( SubObjectOuter )	// if it's a subobject, export as [OuterClass] SubObjectName.Key=Value
			{
				// Find the Class property which references this subobject
				if( obj->GetFlags()&RF_PerObjectLocalized )
					IntExportStruct( NULL, Class, NULL, Class, TmpName, obj->GetOuter()->GetName(), obj->GetName(), (BYTE*)obj, 0, true );
			}
			else
			if( ExportInstances )
				IntExportStruct( Class, Class, NULL, Class, TmpName, obj->GetName(), NULL, (BYTE*)obj, 0, true );
		}
    }

	FConfigFile *NewFile = Config->Find(TmpName,0), *CurrentFile = Config->Find(IntName,NewFile != NULL);
	if ( NewFile )
	{
		if ( (*CurrentFile) != (*NewFile) )
		{
			*CurrentFile = *NewFile;
			if ( GFileManager->IsReadOnly(IntName) && AutoCheckout )
			{
				INT Code;
				void* Handle = appCreateProc( TEXT("p4"), *FString::Printf(TEXT("edit %s"), IntName) );
				while( !appGetProcReturnCode( Handle, &Code ) )
					appSleep(1);
			}
			GConfig->Flush( 0, IntName );
			GWarn->Logf( NAME_Log, TEXT("Exported %d properties."), GPropertyCount );
		}
		Config->UnloadFile(TmpName);

	}

	if ( GFileManager->FileSize(TmpName) >= 0 )
		GFileManager->Delete(TmpName);

	delete[] TmpName;

    return 1;
}

EDITOR_API UBOOL IntExport (const TCHAR *PackageName, const TCHAR *IntName, UBOOL ExportFresh, UBOOL ExportInstances, UBOOL AutoCheckout)
{
    UObject* Package;
    UBOOL rc;
    bool bNeedToUnload = false;

    Package = FindObject<UPackage>( NULL, PackageName );

    if( !Package )
    {
        Package = UObject::LoadPackage (NULL, PackageName, 0 );
        bNeedToUnload = true;
    }

	if( !Package )
    {
		GWarn->Logf( NAME_Error, TEXT("Could not load package %s"), PackageName );
		return 0;
    }

    rc = IntExport (Package, IntName, ExportFresh, ExportInstances, AutoCheckout);

    if( bNeedToUnload )
        UObject::ResetLoaders (Package, 0, 1);

    return (rc);
}

EDITOR_API UBOOL IntMatchesPackage (UObject *Package, const TCHAR *IntName)
{
    FString TempIntName;
    FString TextBufferA, TextBufferB;

    TempIntName = IntName;
    TempIntName += TEXT ("-temp.int");

    bool isCaseSensitive;
    int i;

    if (GFileManager->FileSize (*TempIntName) >= 0)
        GFileManager->Delete (*TempIntName);

    if (GFileManager->FileSize (*TempIntName) >= 0)
    {
        GWarn->Logf(NAME_Warning, TEXT ("Could not remove \"%s\"."), *TempIntName);
        return (1);
    }
    
    if (!IntExport (Package, *TempIntName, true, true))
    {
        GWarn->Logf(NAME_Error, TEXT ("Could not export \"%s\"."), *TempIntName);
        return (1);
    }

    if ((GFileManager->FileSize (*TempIntName) <= 0) && (GFileManager->FileSize (IntName) <= 0))
        return (1);

    if (GFileManager->FileSize (*TempIntName) != GFileManager->FileSize (IntName))
    {
        GFileManager->Delete (*TempIntName);
        return (0);
    }

	if( !appLoadFileToString( TextBufferA, IntName ) )
    {
        GWarn->Logf( NAME_Error, TEXT("Could not open %s"), IntName );
        return (1);
    }

	if( !appLoadFileToString( TextBufferB, *TempIntName ) )
    {
        GWarn->Logf( NAME_Error, TEXT("Could not open %s"), *TempIntName );
        return (1);
    }

    if (GFileManager->FileSize (*TempIntName) >= 0)
        GFileManager->Delete (*TempIntName);

    if (GFileManager->FileSize (*TempIntName) >= 0)
        GWarn->Logf(NAME_Warning, TEXT ("Could not remove \"%s\"."), *TempIntName);

    isCaseSensitive = false;

    check (TextBufferA.Len() == TextBufferB.Len());

    for (i = 0; i < TextBufferA.Len(); i++)
    {
        const TCHAR *a, *b;

        a = *TextBufferA;
        b = *TextBufferB;

        if (isCaseSensitive)
        {
            if (a[i] != b[i])
                return (0);
        }
        else
        {
            if (appToLower (a[i]) != appToLower (b[i]))
                return (0);
        }

        if (a[i] == '=')
            isCaseSensitive = true;
        else if (a[i] == '\n')
            isCaseSensitive = false;
    }

    return (1);
}

EDITOR_API UBOOL IntMatchesPackage (const TCHAR *PackageName, const TCHAR *IntName)
{
    UObject* Package;
    UBOOL rc;
    bool bNeedToUnload = false;

    Package = FindObject<UPackage>( NULL, PackageName );

    if( !Package )
    {
        Package = UObject::LoadPackage (NULL, PackageName, 0 );
        bNeedToUnload = true;
    }

	if( !Package )
    {
		GWarn->Logf( NAME_Error, TEXT("Could not load package %s"), PackageName );
		return 0;
    }

    rc = IntMatchesPackage (Package, IntName);

    if( bNeedToUnload )
        UObject::ResetLoaders (Package, 0, 1);

    return (rc);
}

EDITOR_API void IntGetNameFromPackageName (const FString &PackageName, FString &IntName)
{
    INT i;

    IntName = PackageName;

    i = IntName.InStr (TEXT ("."), 1);

    if (i >= 0)
        IntName = IntName.Left (i);

    IntName += TEXT (".int");

    i = IntName.InStr (TEXT ("/"), 1);
    
    if (i >= 0)
        IntName = IntName.Right (IntName.Len () - i - 1);

    i = IntName.InStr (TEXT ("\\"), 1);
    
    if (i >= 0)
        IntName = IntName.Right (IntName.Len () - i - 1);

    IntName = FString (appBaseDir()) + IntName;
}

EDITOR_API UBOOL IntMatchesPackage (const TCHAR *PackageName)
{
    FString IntName;
    IntGetNameFromPackageName (PackageName, IntName);
    return( IntMatchesPackage( PackageName, *IntName ));
}

