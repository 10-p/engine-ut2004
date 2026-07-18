/*=============================================================================
	UnEdExp.cpp: Editor exporters.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EditorPrivate.h"

/*------------------------------------------------------------------------------
	UTextBufferExporterTXT implementation.
------------------------------------------------------------------------------*/

void UTextBufferExporterTXT::StaticConstructor()
{
	guard(UTextBufferExporterTXT::StaticConstructor);

	SupportedClass = UTextBuffer::StaticClass();
	bText = 1;
	new(Formats)FString(TEXT("TXT"));

	unguard;
}
UBOOL UTextBufferExporterTXT::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	guard(UTextBufferExporterTXT::ExportText);
	UTextBuffer* TextBuffer = CastChecked<UTextBuffer>( Object );
	FString Str( TextBuffer->Text );

	TCHAR* Start = const_cast<TCHAR*>(*Str);
	TCHAR* End   = Start + Str.Len();
	while( Start<End && (Start[0]=='\r' || Start[0]=='\n' || Start[0]==' ') )
		Start++;
	while( End>Start && (End [-1]=='\r' || End [-1]=='\n' || End [-1]==' ') )
		End--;
	*End = 0;

	Ar.Log( Start );

	return 1;
	unguard;
}
IMPLEMENT_CLASS(UTextBufferExporterTXT);

/*------------------------------------------------------------------------------
	USoundExporterWAV implementation.
------------------------------------------------------------------------------*/

void USoundExporterWAV::StaticConstructor()
{
	guard(USoundExporterWAV::StaticConstructor);

	SupportedClass = USound::StaticClass();
	bText = 0;
	new(Formats)FString(TEXT("WAV"));

	unguard;
}
UBOOL USoundExporterWAV::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn )
{
	guard(USoundExporterWAV::ExportBinary);
	USound* Sound = CastChecked<USound>( Object );
	if( Sound->GetClass() == USound::StaticClass() ) // don't export sound groups/ procedural sounds
	{
		Sound->GetData().Load(); // gam
		Ar.Serialize( &Sound->GetData()(0), Sound->GetData().Num() ); // gam
		return 1;
	}
	else
		return 0;
	unguard;
}
IMPLEMENT_CLASS(USoundExporterWAV);

/*------------------------------------------------------------------------------
	UClassExporterH implementation.
------------------------------------------------------------------------------*/

static void RecursiveTagNames( UClass* Class )
{
	guard(RecursiveTagNames);
#if 0 /* Old version */
	if( (Class->GetFlags() & RF_TagExp) && (Class->GetFlags() & RF_Native) )
		for( TFieldIterator<UFunction> Function(Class); Function && Function.GetStruct()==Class; ++Function )
			if
			(	(Function->FunctionFlags & (FUNC_Event|FUNC_Delegate))
			&&	!Function->GetSuperFunction() )
				Function->GetFName().SetFlags( RF_TagExp );
	for( TObjectIterator<UClass> It; It; ++It )
		if( It->GetSuperClass()==Class )
			RecursiveTagNames( *It );
#else /* Faster version */
	for( TObjectIterator<UClass> It; It; ++It )
	{
		UClass* C=*It;
		if(C->IsChildOf(Class) && (C->GetFlags() & RF_TagExp) && (C->GetFlags() & RF_Native))
			for(TFieldFlagIterator<UFunction,CLASS_IsAUFunction> Function(C); Function && Function.GetStruct()==C; ++Function)
				if( (Function->FunctionFlags & (FUNC_Event|FUNC_Delegate)) && !Function->GetSuperFunction() )
					Function->GetFName().SetFlags(RF_TagExp);
	}
#endif
	unguard;
}

void UClassExporterH::StaticConstructor()
{
	guard(UClassExporterH::StaticConstructor);

	SupportedClass = UClass::StaticClass();
	bText = 1;
	new(Formats)FString(TEXT("H"));

	unguard;
}

static bool ShouldExportNatives(UClass* uclass, UObject* outer) // sjs
{
    if((uclass->GetFlags() & RF_Native) && (uclass->GetFlags() & RF_TagExp)) // catch native, no export classes
    {
        return true; // simple export scripts
    }
    else if((uclass->GetFlags() & RF_Native) && (uclass->ScriptText != NULL) && (uclass->GetOuter() == outer))
    {
        for( TFieldFlagIterator<UFunction,CLASS_IsAUFunction> Function(uclass); Function && Function.GetStruct()==uclass; ++Function )
        {
            if( Function->FunctionFlags & FUNC_Native )
            {
                return true; // no-export classes that have native functions
            }
        }
    }
    return false;
}

static INT Compare( FString& A, FString& B ) // gam
{
	return appStricmp( *A, *B );
}

UBOOL UClassExporterH::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	guard(UClassExporterH::ExportText);
	UClass* Class = CastChecked<UClass>( Object );

	TCHAR API[256];
	appStrcpy( API, Class->GetOuter()->GetName() );
	appStrupr( API );

	// Export as C++ header.
	if( RecursionDepth==0 )
	{
		DidTop = 0;
		RecursiveTagNames( Class );
	}

	// Export this.
	if( Class->GetFlags() & RF_TagExp )
	{
		// Top of file.
		if( !DidTop )
		{
			DidTop = 1;
			Ar.Logf
			(
				TEXT("/*===========================================================================\r\n")
				TEXT("    C++ class definitions exported from UnrealScript.\r\n")
				TEXT("    This is automatically generated by the tools.\r\n")
				TEXT("    DO NOT modify this manually! Edit the corresponding .uc files instead!\r\n")
				TEXT("===========================================================================*/\r\n")
				TEXT("#if SUPPORTS_PRAGMA_PACK\r\n")
				TEXT("#pragma pack (push,%i)\r\n")
				TEXT("#endif\r\n")
				TEXT("\r\n")
				TEXT("#ifndef %s_API\r\n")
				TEXT("#define %s_API DLL_IMPORT\r\n")
				TEXT("#endif\r\n")
				TEXT("\r\n")
				TEXT("#ifndef NAMES_ONLY\r\n")
				TEXT("#define AUTOGENERATE_NAME(name) extern %s_API FName %s_##name;\r\n")
				TEXT("#define AUTOGENERATE_FUNCTION(cls,idx,name)\r\n")
				TEXT("#endif\r\n")
				TEXT("\r\n"),
				PROPERTY_ALIGNMENT,
				API,
				API,
				API,
				API
			);
			TArray<FString> AutoNames;

			for( INT i=0; i<FName::GetMaxNames(); i++ )
				if( FName::GetEntry(i) && (FName::GetEntry(i)->Flags & RF_TagExp) )
					new(AutoNames) FString(*FName((EName)(i)));

			Sort(&AutoNames(0), AutoNames.Num());

			for ( INT i=0; i<AutoNames.Num(); i++)
				Ar.Logf( TEXT("AUTOGENERATE_NAME(%s)\r\n"), *AutoNames(i) );

			for( INT i=0; i<FName::GetMaxNames(); i++ )
				if( FName::GetEntry(i) )
					FName::GetEntry(i)->Flags &= ~RF_TagExp;
			Ar.Logf( TEXT("\r\n#ifndef NAMES_ONLY\r\n\r\n") );
		}

		// Enum definitions.
		for( TFieldIterator<UEnum> ItE(Class); ItE && ItE.GetStruct()==Class; ++ItE )
		{
			// Export enum.
			if( ItE->GetOuter()==Class )
			{
				Ar.Logf( TEXT("%senum %s\r\n{\r\n"), appSpc(TextIndent), ItE->GetName() );
				INT i;
				for( i=0; i<ItE->Names.Num(); i++ )
					Ar.Logf( TEXT("%s    %-24s=%i,\r\n"), appSpc(TextIndent), *ItE->Names(i), i );
				if( appStrchr(*ItE->Names(0),'_') )
				{
					// Include tag_MAX enumeration.
					TCHAR Temp[256];
					appStrcpy( Temp, *ItE->Names(0) );
					appStrcpy( appStrchr(Temp,'_'),TEXT("_MAX"));
					Ar.Logf( TEXT("%s    %-24s=%i,\r\n"), appSpc(TextIndent), Temp, i );
				}
				Ar.Logf( TEXT("};\r\n") );
			}
			else Ar.Logf( TEXT("%senum %s;\r\n"), appSpc(TextIndent), ItE->GetName() );
		}

		// Struct definitions.
        TArray<UStruct*> ExportStructs; // sjs
		for( TFieldIterator<UStruct> ItS(Class); ItS && ItS.GetStruct()==Class; ++ItS )
		{
            if( ((ItS->GetFlags() & RF_Native) || ((Class->ClassFlags & CLASS_ExportStructs) || (ItS->StructFlags & STRUCT_Native)) && 
                !ItS->IsA(UState::StaticClass()) && !ItS->IsA(UFunction::StaticClass()) ) )
            // --- amb
			{
                ExportStructs.AddItem( *ItS );
            }
        }

        // sjs - walk backward through export structs so C++ dependencies are in correct order
        for ( int i=ExportStructs.Num()-1; i>=0; i-- )
        {
            UStruct* ItS = ExportStructs(i);
			// Export struct.
			Ar.Logf( TEXT("struct %s_API %s"), API, ItS->GetNameCPP() );
			if( ItS->SuperField )
			Ar.Logf(TEXT(" : public %s"), ItS->GetSuperStruct()->GetNameCPP() );
			Ar.Logf( TEXT("\r\n{\r\n") );
			UProperty* LastIt = NULL;
			for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It2(ItS); It2; ++It2 )
			{
				if( It2.GetStruct()==ItS && It2->ElementSize )
				{
					Ar.Logf( appSpc(TextIndent+4) );
					It2->ExportCpp( Ar, 0, 0, 0, (ItS->StructFlags&STRUCT_Export) || (ItS->StructFlags&STRUCT_Init) ? 1 : 0 );

					bool doPack = false;
					if (It2->IsA(UBoolProperty::StaticClass()))
					{
						if( !LastIt || !LastIt->IsA(UBoolProperty::StaticClass()) )
							doPack = true;
					}
					else
					{
						if ( LastIt == NULL )
							doPack = true;
						else if ( LastIt->IsA(UBoolProperty::StaticClass()) )
							doPack = true;
						else if ( LastIt->IsA(UByteProperty::StaticClass()) && !It2->IsA(UByteProperty::StaticClass()) )
							doPack = true;
					}

					if ( doPack )
						Ar.Logf( TEXT(" GCC_PACK(%i)"), PROPERTY_ALIGNMENT );

					LastIt = *It2;
					Ar.Logf(TEXT(";\r\n"));
				}
			}

			// Export serializer
			if( ItS->StructFlags&STRUCT_Export )
			{
				Ar.Logf( TEXT("%sfriend %s_API FArchive& operator<<(FArchive& Ar,%s& My%s)\r\n"), appSpc(TextIndent+4), API, ItS->GetNameCPP(), ItS->GetName() );
				Ar.Logf( TEXT("%s{\r\n"), appSpc(TextIndent+4) );
				Ar.Logf( TEXT("%sreturn Ar"), appSpc(TextIndent+8) );
				for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It2(ItS); It2; ++It2 )
				{
					if( It2.GetStruct()==ItS && It2->ElementSize )
					{
						if( It2->ArrayDim > 1 )
						{
							for( INT i=0;i<It2->ArrayDim;i++ )
                                Ar.Logf( TEXT(" << My%s.%s[%d]"), ItS->GetName(), It2->GetName(), i );
						}
						else
						{
							Ar.Logf( TEXT(" << My%s.%s"), ItS->GetName(), It2->GetName() );
						}
					}
				}
				Ar.Logf( TEXT(";\r\n%s}\r\n"), appSpc(TextIndent+4) );
			}

			if ( ItS->CppText )
				Ar.Logf( TEXT("%s\r\n"), *ItS->CppText->Text );

			Ar.Logf( TEXT("};\r\n\r\n") );
		}
        ExportStructs.Empty(); // sjs

		// Constants.
		for( TFieldIterator<UConst> ItC(Class); ItC && ItC.GetStruct()==Class; ++ItC )
		{
			FString V = ItC->Value;
			while( V.Left(1)==TEXT(" ") )
				V=V.Mid(1);
			if( V.Len()>1 && V.Left(1)==TEXT("'") && V.Right(1)==TEXT("'") )
				V = V.Mid(1,V.Len()-2);
			Ar.Logf( TEXT("#define UCONST_%s %s\r\n"), ItC->GetName(), *V );
		}
		if( TFieldIterator<UConst>(Class) )
			Ar.Logf( TEXT("\r\n") );

		// Parms struct definitions.
		{
			for( TFieldFlagIterator<UFunction,CLASS_IsAUFunction> Function(Class); Function && Function.GetStruct()==Class; ++Function )
			{
				if
				(	(Function->FunctionFlags & (FUNC_Event|FUNC_Delegate))
				&&	(!Function->GetSuperFunction()) )
				{
					Ar.Logf( TEXT("struct %s_event%s_Parms\r\n"), Class->GetNameCPP(), Function->GetName() );
					Ar.Log( TEXT("{\r\n") );
					for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(*Function); It && (It->PropertyFlags&CPF_Parm); ++It )
					{
						Ar.Log( TEXT("    ") );
						It->ExportCpp( Ar, 1, 0, 1, 0 );
						Ar.Log( TEXT(";\r\n") );
					}
					Ar.Log( TEXT("};\r\n") );
				}
			}
		}
		// Class definition.
		Ar.Logf( TEXT("class %s_API %s"), API, Class->GetNameCPP() );
		if( Class->GetSuperClass() )
			Ar.Logf( TEXT(" : public %s\r\n"), Class->GetSuperClass()->GetNameCPP() );
		Ar.Logf( TEXT("{\r\npublic:\r\n") );

		// All per-object properties defined in this class.
		UProperty* LastIt = NULL;
		for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Class); It; ++It )
		{
			if( It.GetStruct()==Class && It->ElementSize )
			{
				Ar.Logf( appSpc(TextIndent+4) );
				It->ExportCpp( Ar, 0, 0, 0, 0 );

				// !!! FIXME: This code is duplicated above for structs!  --ryan.
				bool doPack = false;

				if (It->IsA(UBoolProperty::StaticClass()))
				{
					if( !LastIt || !LastIt->IsA(UBoolProperty::StaticClass()) )
						doPack = true;
				}
				else
				{
					if ( LastIt == NULL )
						doPack = true;
					else if ( LastIt->IsA(UBoolProperty::StaticClass()) )
						doPack = true;
					else if ( LastIt->IsA(UByteProperty::StaticClass()) && !It->IsA(UByteProperty::StaticClass()) )
						doPack = true;
				}

				if ( doPack )
					Ar.Logf( TEXT(" GCC_PACK(%i)"), PROPERTY_ALIGNMENT );

				Ar.Logf( TEXT(";\r\n") );
			}
			LastIt = *It;
		}

		// C++ -> UnrealScript stubs.
		for( TFieldFlagIterator<UFunction,CLASS_IsAUFunction> Function(Class); Function && Function.GetStruct()==Class; ++Function )
			if( Function->FunctionFlags & FUNC_Native )
				Ar.Logf( TEXT("    DECLARE_FUNCTION(exec%s);\r\n"), Function->GetName() );

		// UnrealScript -> C++ proxies.
		for( TFieldFlagIterator<UFunction,CLASS_IsAUFunction> Function(Class); Function && Function.GetStruct()==Class; ++Function )
		{
			if
			(	(Function->FunctionFlags & (FUNC_Event|FUNC_Delegate))
			&&	(!Function->GetSuperFunction()) )
			{
				// Return type.
				UProperty* Return = Function->GetReturnProperty();
				Ar.Log( TEXT("    ") );
				if( !Return )
					Ar.Log( TEXT("void") );
				else
					Return->ExportCppItem( Ar,1 );	// rjp - return values need to be flagged as parms

				// Function name and parms.
				INT ParmCount=0;
				if( Function->FunctionFlags & FUNC_Delegate )
					Ar.Logf( TEXT(" delegate%s("), Function->GetName() );
				else
				Ar.Logf( TEXT(" event%s("), Function->GetName() );
				for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(*Function); It && (It->PropertyFlags&(CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
				{
					if( ParmCount++ )
						Ar.Log(TEXT(", "));
					It->ExportCpp( Ar, 0, 1, 1, 0 );
				}
				Ar.Log( TEXT(")\r\n") );

				// Function call.
				Ar.Log( TEXT("    {\r\n") );
				UBOOL ProbeOptimization = (Function->GetFName().GetIndex()>=NAME_PROBEMIN && Function->GetFName().GetIndex()<NAME_PROBEMAX);
				if( ParmCount || Return )
				{
					Ar.Logf( TEXT("        %s_event%s_Parms Parms;\r\n"), Class->GetNameCPP(), Function->GetName() );
					if( Return && !Cast<UStrProperty>(Return) )
						Ar.Logf( TEXT("        Parms.%s=0;\r\n"), Return->GetName() );
				}
				if( ProbeOptimization )
					Ar.Logf(TEXT("        if(IsProbing(NAME_%s)) {\r\n"),Function->GetName());
				if( ParmCount || Return )
				{
					// Parms struct initialization.
					for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(*Function); It && (It->PropertyFlags&(CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
					{
						if( It->ArrayDim>1 )
							Ar.Logf( TEXT("        appMemcpy(&Parms.%s,%s,sizeof(Parms.%s));\r\n"), It->GetName(), It->GetName(), It->GetName() );
						else
						{
							if ( It->IsA(UBoolProperty::StaticClass()) )   // need to handle bitfield byte order when passing into UnrealScript...  --ryan.
								Ar.Logf( TEXT("        Parms.%s=((%s) ? FIRST_BITFIELD : 0);\r\n"), It->GetName(), It->GetName() );
							else
								Ar.Logf( TEXT("        Parms.%s=%s;\r\n"), It->GetName(), It->GetName() );
						}
					}
					if( Function->FunctionFlags & FUNC_Delegate )
						Ar.Logf( TEXT("        ProcessDelegate(%s_%s,&__%s__Delegate,&Parms);\r\n"), API, Function->GetName(), Function->GetName() );
					else
					Ar.Logf( TEXT("        ProcessEvent(FindFunctionChecked(%s_%s),&Parms);\r\n"), API, Function->GetName() );
				}
				else
				{
					if( Function->FunctionFlags & FUNC_Delegate )
						Ar.Logf( TEXT("        ProcessDelegate(%s_%s,&__%s__Delegate,NULL);\r\n"), API, Function->GetName(), Function->GetName() );
					else
						Ar.Logf( TEXT("        ProcessEvent(FindFunctionChecked(%s_%s),NULL);\r\n"), API, Function->GetName() );
				}
				if( ProbeOptimization )
					Ar.Logf(TEXT("        }\r\n"));

				// Out parm copying.
				for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(*Function); It && (It->PropertyFlags&(CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
				{
					if( It->PropertyFlags & CPF_OutParm )
					{
						if( It->ArrayDim>1 )
							Ar.Logf( TEXT("        appMemcpy(&%s,&Parms.%s,sizeof(%s));\r\n"), It->GetName(), It->GetName(), It->GetName() );
						else
							Ar.Logf( TEXT("        %s=Parms.%s;\r\n"), It->GetName(), It->GetName() );
					}
				}

				// Return value.
				if( Return )
					Ar.Logf( TEXT("        return Parms.%s;\r\n"), Return->GetName() );
				Ar.Log( TEXT("    }\r\n") );
			}
		}

		// Code.
		Ar.Logf( TEXT("    DECLARE_CLASS(%s,"), Class->GetNameCPP() ); //warning: GetNameCPP uses static storage.
		Ar.Logf( TEXT("%s,0"), Class->GetSuperClass()->GetNameCPP() );
		if( Class->ClassFlags & CLASS_Transient      )
			Ar.Log( TEXT("|CLASS_Transient") );
		if( Class->ClassFlags & CLASS_Config )
			Ar.Log( TEXT("|CLASS_Config") );
		if( Class->ClassFlags & CLASS_NativeReplication )
			Ar.Log( TEXT("|CLASS_NativeReplication") );
		Ar.Logf( TEXT(",%s)\r\n"), Class->GetOuter()->GetName() );
		FString Filename = FString(TEXT("..")) * Class->GetOuter()->GetName() * TEXT("Inc") * Class->GetNameCPP() + TEXT(".h");
		if( Class->CppText )
			Ar.Log( *Class->CppText->Text );
		else
		if( GFileManager->FileSize(*Filename) > 0 )
			Ar.Logf( TEXT("    #include \"%s.h\"\r\n"), Class->GetNameCPP() );
		else
			Ar.Logf( TEXT("    NO_DEFAULT_CONSTRUCTOR(%s)\r\n"), Class->GetNameCPP() );

		// End of class.
		Ar.Logf( TEXT("};\r\n") );

		// End.
		Ar.Logf( TEXT("\r\n") );
	}

	// Export all child classes that are tagged for export.
	RecursionDepth++;
	for( TObjectIterator<UClass> It; It; ++It )
		if( It->GetSuperClass()==Class )
			UExporter::ExportToOutputDevice( *It, this, Ar, TEXT("H"), TextIndent );
	RecursionDepth--;

	// Finish C++ header.
	if( RecursionDepth==0 )
	{
		Ar.Logf( TEXT("#endif\r\n") );
		Ar.Logf( TEXT("\r\n") );

		for( TObjectIterator<UClass> It; It; ++It )
			if( It->GetFlags() & RF_TagExp )
				for( TFieldFlagIterator<UFunction,CLASS_IsAUFunction> Function(*It); Function && Function.GetStruct()==*It; ++Function )
					if( Function->FunctionFlags & FUNC_Native )
						Ar.Logf( TEXT("AUTOGENERATE_FUNCTION(%s,%i,exec%s);\r\n"), It->GetNameCPP(), Function->iNative ? Function->iNative : -1, Function->GetName() );

		Ar.Logf( TEXT("\r\n") );
		Ar.Logf( TEXT("#ifndef NAMES_ONLY\r\n") );
		Ar.Logf( TEXT("#undef AUTOGENERATE_NAME\r\n") );
		Ar.Logf( TEXT("#undef AUTOGENERATE_FUNCTION\r\n") );
		Ar.Logf( TEXT("#endif\r\n") );

		Ar.Logf( TEXT("\r\n") );
		Ar.Logf( TEXT("#if SUPPORTS_PRAGMA_PACK\r\n") );
		Ar.Logf( TEXT("#pragma pack (pop)\r\n") );
		Ar.Logf( TEXT("#endif\r\n") );

        // sjs ---
        //#ifndef UNENGINENATIVE_H
        //#define UNENGINENATIVE_H
        UObject* expOuter = NULL;
        for( TObjectIterator<UClass> It; It; ++It )
        {
            if( (It->GetFlags() & RF_TagExp) && (It->GetFlags() & RF_Native) )
            {
                appStrcpy(API, It->GetOuter()->GetName());
                expOuter = It->GetOuter();
                break;
            }
        }
        
		TCHAR upperAPI[256];
		appStrcpy(upperAPI, API);
		appStrupr(upperAPI);

		Ar.Logf( TEXT("\r\n") );
        Ar.Logf( TEXT("\r\n") );
        Ar.Logf( TEXT("#ifdef NATIVE_DEFS_ONLY\r\n"));

        int numNative = 0;
        for( TObjectIterator<UClass> It; It; ++It )
        {
            if( ShouldExportNatives(*It, expOuter) )
            {
                numNative++;
                //DECLARE_NATIVE_TYPE(Engine,AActor);
                Ar.Logf( TEXT("DECLARE_NATIVE_TYPE(%s,%s);\r\n"), API, It->GetNameCPP() );
            }
        }
        Ar.Logf( TEXT("\r\n") );

        // #define AUTO_INITIALIZE_REGISTRANTS_ENGINE \ //   
        Ar.Logf( TEXT("#define AUTO_INITIALIZE_REGISTRANTS_%s \\\r\n"), upperAPI );
        for( TObjectIterator<UClass> It; It; ++It )
        {
            //AGameStats::StaticClass(); \//
            if( (It->GetFlags() & RF_Native) && (It->GetOuter() == expOuter) )
            {
                Ar.Logf( TEXT("\t%s::StaticClass(); \\\r\n"), It->GetNameCPP() );
                if( ShouldExportNatives(*It, expOuter) )
                {
                    for( TFieldFlagIterator<UFunction,CLASS_IsAUFunction> Function(*It); Function && Function.GetStruct()==*It; ++Function )
                    {
                        if( Function->FunctionFlags & FUNC_Native )
                        {
                            Ar.Logf( TEXT("\tGNativeLookupFuncs[Lookup++] = &Find%s%sNative; \\\r\n"), API, It->GetNameCPP() );
                            break;
                        }
                    }
                }
            }
        }
        Ar.Logf( TEXT("\r\n") );
        Ar.Logf( TEXT("#endif // NATIVE_DEFS_ONLY\r\n"), API ); // #endif // NATIVE_DEFS_ONLY
        Ar.Logf( TEXT("\r\n") );

        // define function generation macros
        Ar.Logf( TEXT("#ifdef NATIVES_ONLY\r\n") ); // #if // __STATIC_LINK
        for( TObjectIterator<UClass> It; It; ++It )
        {
            if( ShouldExportNatives(*It, expOuter) )
            {
                int nativeFuncs = 0;
                for( TFieldFlagIterator<UFunction,CLASS_IsAUFunction> Function(*It); Function && Function.GetStruct()==*It; ++Function )
                {
                    if( Function->FunctionFlags & FUNC_Native )
                    {
                        nativeFuncs++;
                        break;
                    }
                }
                /*
                #if __STATIC_LINK
                #ifdef NAMES_ONLY
                NATIVE_INFO(UScriptedTexture) GEngineUScriptedTextureNatives[] =
                {
	                MAP_NATIVE(UScriptedTexture,execDrawPortal)
	                MAP_NATIVE(UScriptedTexture,execDrawTile)
	                MAP_NATIVE(UScriptedTexture,execTextSize)
	                MAP_NATIVE(UScriptedTexture,execDrawText)
	                MAP_NATIVE(UScriptedTexture,execSetSize)
	                {NULL,NULL}
                };
                IMPLEMENT_NATIVE_HANDLER(Engine,UScriptedTexture);
                #endif
                #endif
                */
                if( nativeFuncs )
                {
                    Ar.Logf( TEXT("NATIVE_INFO(%s) G%s%sNatives[] = \r\n"), It->GetNameCPP(), API, It->GetNameCPP() );
                    Ar.Logf( TEXT("{ \r\n"));
                    for( TFieldFlagIterator<UFunction,CLASS_IsAUFunction> Function(*It); Function && Function.GetStruct()==*It; ++Function )
                    {
                        if( Function->FunctionFlags & FUNC_Native )
                        {
                            Ar.Logf( TEXT("\tMAP_NATIVE(%s,exec%s)\r\n"), It->GetNameCPP(), Function->GetName() );
                        }
                    }
                    Ar.Logf( TEXT("\t{NULL,NULL}\r\n") );
                    Ar.Logf( TEXT("};\r\n") );
                    Ar.Logf( TEXT("IMPLEMENT_NATIVE_HANDLER(%s,%s);\r\n"), API, It->GetNameCPP() );
                    Ar.Logf( TEXT("\r\n") );
                }
            }
        }
        Ar.Logf( TEXT("#endif // NATIVES_ONLY\r\n"), API ); // #endif // NAMES_ONLY
        // --- sjs

		// Generate code to automatically verify class offsets and size.
		Ar.Logf( TEXT("\r\n#ifdef VERIFY_CLASS_SIZES\r\n") ); // #ifdef VERIFY_CLASS_SIZES
		for( TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt )
		{		
			UClass* Class = *ClassIt;
			if ( (Class->GetFlags() & RF_TagExp) ||
				((Class->GetFlags() & RF_Native) && (Class->GetOuter() == expOuter) && (Class->ClassFlags & CLASS_NoExport)) )
//			if( Class->GetFlags() & RF_TagExp )
			{	
#if 0
				// #ifdef'ed out due to compiler limitation
				for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> PropIt(Class); PropIt; ++PropIt )
				{
					if( PropIt.GetStruct()==Class && PropIt->ElementSize && !PropIt->IsA(UBoolProperty::StaticClass()) )
						Ar.Logf( TEXT("VERIFY_CLASS_OFFSET_NODIE(%s,%s,%s)\r\n"),*FString(Class->GetNameCPP()).Left(1),*FString(Class->GetNameCPP()).Mid(1),PropIt->GetName());
				}
#endif
				Ar.Logf( TEXT("VERIFY_CLASS_SIZE_NODIE(%s)\r\n"), Class->GetNameCPP() );
			}
		}
		Ar.Logf( TEXT("#endif // VERIFY_CLASS_SIZES\r\n") ); // #endif // VERIFY_CLASS_SIZES
	}

	return 1;
	unguard;
}
IMPLEMENT_CLASS(UClassExporterH);

/*------------------------------------------------------------------------------
	UClassExporterUC implementation.
------------------------------------------------------------------------------*/

void UClassExporterUC::StaticConstructor()
{
	guard(UClassExporterUC::StaticConstructor);

	SupportedClass = UClass::StaticClass();
	bText = 1;
	new(Formats)FString(TEXT("UC"));

	unguard;
}
UBOOL UClassExporterUC::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	guard(UClassExporterUC::ExportText);
	UClass* Class = CastChecked<UClass>( Object );

	// Export script text.
	check(Class->Defaults.Num());
	check(Class->ScriptText);
	UExporter::ExportToOutputDevice( Class->ScriptText, NULL, Ar, TEXT("txt"), TextIndent );

	// Export default properties that differ from parent's.
	Ar.Log( TEXT("\r\n\r\ndefaultproperties\r\n{\r\n") );
	ExportProperties
	(
		Ar,
		Class,
		&Class->Defaults(0),
		TextIndent+4,
		Class->GetSuperClass(),
		Class->GetSuperClass() ? &Class->GetSuperClass()->Defaults(0) : NULL
	);
	Ar.Log( TEXT("}\r\n") );

	return 1;
	unguard;
}
IMPLEMENT_CLASS(UClassExporterUC);

/*------------------------------------------------------------------------------
	USoundExporterWAV implementation.
------------------------------------------------------------------------------*/

void UPolysExporterT3D::StaticConstructor()
{
	guard(UPolysExporterT3D::StaticConstructor);

	SupportedClass = UPolys::StaticClass();
	bText = 1;
	new(Formats)FString(TEXT("T3D"));

	unguard;
}
UBOOL UPolysExporterT3D::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	guard(UPolysExporterT3D::ExportText);
	UPolys* Polys = CastChecked<UPolys>( Object );

	Ar.Logf( TEXT("%sBegin PolyList\r\n"), appSpc(TextIndent) );
	for( INT i=0; i<Polys->Element.Num(); i++ )
	{
		FPoly* Poly = &Polys->Element(i);
		TCHAR TempStr[256];

		// Start of polygon plus group/item name if applicable.
		Ar.Logf( TEXT("%s   Begin Polygon"), appSpc(TextIndent) );
		if( Poly->ItemName != NAME_None )
			Ar.Logf( TEXT(" Item=%s"), *Poly->ItemName );
		if( Poly->Material )
			Ar.Logf( TEXT(" Texture=%s"), Poly->Material->GetPathName() );
		if( Poly->PolyFlags != 0 )
			Ar.Logf( TEXT(" Flags=%i"), Poly->PolyFlags );
		if( Poly->iLink != INDEX_NONE )
			Ar.Logf( TEXT(" Link=%i"), Poly->iLink );
		Ar.Logf( TEXT("\r\n") );

		// All coordinates.
		Ar.Logf( TEXT("%s      Origin   %s\r\n"), appSpc(TextIndent), SetFVECTOR(TempStr,&Poly->Base) );
		Ar.Logf( TEXT("%s      Normal   %s\r\n"), appSpc(TextIndent), SetFVECTOR(TempStr,&Poly->Normal) );
		Ar.Logf( TEXT("%s      TextureU %s\r\n"), appSpc(TextIndent), SetFVECTOR(TempStr,&Poly->TextureU) );
		Ar.Logf( TEXT("%s      TextureV %s\r\n"), appSpc(TextIndent), SetFVECTOR(TempStr,&Poly->TextureV) );
		for( INT j=0; j<Poly->NumVertices; j++ )
			Ar.Logf( TEXT("%s      Vertex   %s\r\n"), appSpc(TextIndent), SetFVECTOR(TempStr,&Poly->Vertex[j]) );
		Ar.Logf( TEXT("%s   End Polygon\r\n"), appSpc(TextIndent) );
	}
	Ar.Logf( TEXT("%sEnd PolyList\r\n"), appSpc(TextIndent) );

	return 1;
	unguard;
}
IMPLEMENT_CLASS(UPolysExporterT3D);

/*------------------------------------------------------------------------------
	UModelExporterT3D implementation.
------------------------------------------------------------------------------*/

void UModelExporterT3D::StaticConstructor()
{
	guard(UModelExporterT3D::StaticConstructor);

	SupportedClass = UModel::StaticClass();
	bText = 1;
	new(Formats)FString(TEXT("T3D"));

	unguard;
}
UBOOL UModelExporterT3D::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	guard(UModelExporterT3D::ExportText);
	UModel* Model = CastChecked<UModel>( Object );

	Ar.Logf( TEXT("%sBegin Brush Name=%s\r\n"), appSpc(TextIndent), Model->GetName() );
	UExporter::ExportToOutputDevice( Model->Polys, NULL, Ar, Type, TextIndent+3 );
	Ar.Logf( TEXT("%sEnd Brush\r\n"), appSpc(TextIndent) );

	return 1;
	unguard;
}
IMPLEMENT_CLASS(UModelExporterT3D);

/*------------------------------------------------------------------------------
	ULevelExporterT3D implementation.
------------------------------------------------------------------------------*/

void ULevelExporterT3D::StaticConstructor()
{
	guard(ULevelExporterT3D::StaticConstructor);

	SupportedClass = ULevel::StaticClass();
	bText = 1;
	new(Formats)FString(TEXT("T3D"));
	new(Formats)FString(TEXT("COPY"));

	unguard;
}
UBOOL ULevelExporterT3D::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	guard(ULevelExporterT3D::ExportText);
	ULevel* Level = CastChecked<ULevel>( Object );

	for( FObjectIterator It; It; ++It )
		It->ClearFlags( RF_TagImp | RF_TagExp );

	Ar.Logf( TEXT("%sBegin Map\r\n"), appSpc(TextIndent) );
	UBOOL AllSelected = appStricmp(Type,TEXT("COPY"))!=0;
	for( INT iActor=0; iActor<Level->Actors.Num(); iActor++ )
	{
        Warn->StatusUpdatef( iActor, Level->Actors.Num(), TEXT("") ); // gam

		AActor* Actor = Level->Actors(iActor);
		if( Actor && !Cast<ACamera>(Actor) && (AllSelected ||Actor->bSelected) )
		{
			Ar.Logf( TEXT("%sBegin Actor Class=%s Name=%s\r\n"), appSpc(TextIndent), Actor->GetClass()->GetName(), Actor->GetName() );
			ExportProperties( Ar, Actor->GetClass(), (BYTE*)Actor, TextIndent+3, Actor->GetClass(), &Actor->GetClass()->Defaults(0) );
			Ar.Logf( TEXT("%sEnd Actor\r\n"), appSpc(TextIndent) );
		}
	}

	// Export information about the first selected surface in the map.  Used for copying/pasting
	// information from poly to poly.
	Ar.Logf( TEXT("%sBegin Surface\r\n"), appSpc(TextIndent) );
	TCHAR TempStr[256];
	for( INT i=0; i<Level->Model->Surfs.Num(); i++ )
	{
		FBspSurf *Poly = &Level->Model->Surfs(i);
		if( Poly->PolyFlags&PF_Selected )
		{
			Ar.Logf( TEXT("%sTEXTURE=%s\r\n"), appSpc(TextIndent+3), Poly->Material->GetPathName() );
			Ar.Logf( TEXT("%sBASE      %s\r\n"), appSpc(TextIndent+3), SetFVECTOR(TempStr,&(Level->Model->Points(Poly->pBase))) );
			Ar.Logf( TEXT("%sTEXTUREU  %s\r\n"), appSpc(TextIndent+3), SetFVECTOR(TempStr,&(Level->Model->Vectors(Poly->vTextureU))) );
			Ar.Logf( TEXT("%sTEXTUREV  %s\r\n"), appSpc(TextIndent+3), SetFVECTOR(TempStr,&(Level->Model->Vectors(Poly->vTextureV))) );
			Ar.Logf( TEXT("%sNORMAL    %s\r\n"), appSpc(TextIndent+3), SetFVECTOR(TempStr,&(Level->Model->Vectors(Poly->vNormal))) );
			Ar.Logf( TEXT("%sPOLYFLAGS=%d\r\n"), appSpc(TextIndent+3), Poly->PolyFlags );
			break;
		}
	}
	Ar.Logf( TEXT("%sEnd Surface\r\n"), appSpc(TextIndent) );

	Ar.Logf( TEXT("%sEnd Map\r\n"), appSpc(TextIndent) );


	return 1;
	unguard;
}
IMPLEMENT_CLASS(ULevelExporterT3D);

/*------------------------------------------------------------------------------
	ULevelExporterSTL implementation.
------------------------------------------------------------------------------*/

void ULevelExporterSTL::StaticConstructor()
{
	guard(ULevelExporterSTL::StaticConstructor);

	SupportedClass = ULevel::StaticClass();
	bText = 1;
	new(Formats)FString(TEXT("STL"));

	unguard;
}
UBOOL ULevelExporterSTL::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	guard(ULevelExporterSTL::ExportText);
	ULevel* Level = CastChecked<ULevel>( Object );

	for( FObjectIterator It; It; ++It )
		It->ClearFlags( RF_TagImp | RF_TagExp );

	//
	// GATHER TRIANGLES
	//

	TArray<FVector> Triangles;

	// Specific actors can be exported
	for( INT iActor=0; iActor<Level->Actors.Num(); iActor++ )
	{
		ATerrainInfo* TI = Cast<ATerrainInfo>(Level->Actors(iActor));
		if( TI && TI->bSelected )
		{
			for( int y=0;y<TI->HeightmapY-1;y++ )
				for( int x=0;x<TI->HeightmapX-1;x++ )
				{
					FVector P1	= TI->Vertices(TI->GetGlobalVertex(x,y));
					FVector P2	= TI->Vertices(TI->GetGlobalVertex(x,y+1));
					FVector P3	= TI->Vertices(TI->GetGlobalVertex(x+1,y+1));
					FVector P4	= TI->Vertices(TI->GetGlobalVertex(x+1,y));

					if( TI->GetQuadVisibilityBitmap( x, y ) )
						if( TI->GetEdgeTurnBitmap( x, y ) )
						{
							Triangles.AddItem( P1 );
							Triangles.AddItem( P4 );
							Triangles.AddItem( P2 );

							Triangles.AddItem( P4 );
							Triangles.AddItem( P3 );
							Triangles.AddItem( P2 );
						}
						else
						{
							Triangles.AddItem( P1 );
							Triangles.AddItem( P4 );
							Triangles.AddItem( P3 );

							Triangles.AddItem( P1 );
							Triangles.AddItem( P3 );
							Triangles.AddItem( P2 );
						}
				}
		}

		AActor* Actor = Cast<AActor>(Level->Actors(iActor));
		if( Actor && Actor->bSelected && Actor->StaticMesh )
		{
			if(!Actor->StaticMesh->RawTriangles.Num())
				Actor->StaticMesh->RawTriangles.Load();

			for( INT tri = 0 ; tri < Actor->StaticMesh->RawTriangles.Num() ; tri++ )
			{
				FStaticMeshTriangle* smt = &Actor->StaticMesh->RawTriangles(tri);

				for( INT v = 2 ; v > -1 ; v-- )
				{
					FVector vtx = Actor->LocalToWorld().TransformFVector( smt->Vertices[v] );
					Triangles.AddItem( vtx );
				}
			}
		}
	}

	// Selected BSP surfaces
	for( INT i=0;i<Level->Model->Nodes.Num();i++ )
	{
		FBspNode* Node = &Level->Model->Nodes(i);
		if( Level->Model->Surfs(Node->iSurf).PolyFlags&PF_Selected )
		{
			if( Node->NumVertices > 2 )
			{
				FVector vtx1(Level->Model->Points(Level->Model->Verts(Node->iVertPool+0).pVertex)),
					vtx2(Level->Model->Points(Level->Model->Verts(Node->iVertPool+1).pVertex)),
					vtx3;

				for( INT v = 2 ; v < Node->NumVertices ; v++ )
				{
					vtx3 = Level->Model->Points(Level->Model->Verts(Node->iVertPool+v).pVertex);

					Triangles.AddItem( vtx1 );
					Triangles.AddItem( vtx2 );
					Triangles.AddItem( vtx3 );

					vtx2 = vtx3;
				}
			}
		}
	}

	//
	// WRITE THE FILE
	//

	Ar.Logf( TEXT("%ssolid LevelBSP\r\n"), appSpc(TextIndent) );

	for( INT tri = 0 ; tri < Triangles.Num() ; tri += 3 )
	{
		FVector vtx[3];
		vtx[0] = Triangles(tri);
		vtx[1] = Triangles(tri+1);
		vtx[2] = Triangles(tri+2);

		FPlane Normal( vtx[0], vtx[1], vtx[2] );

		Ar.Logf( TEXT("%sfacet normal %1.6f %1.6f %1.6f\r\n"), appSpc(TextIndent+2), Normal.X, Normal.Y, Normal.Z );
		Ar.Logf( TEXT("%souter loop\r\n"), appSpc(TextIndent+4) );

		Ar.Logf( TEXT("%svertex %1.6f %1.6f %1.6f\r\n"), appSpc(TextIndent+6), vtx[0].X, vtx[0].Y, vtx[0].Z );
		Ar.Logf( TEXT("%svertex %1.6f %1.6f %1.6f\r\n"), appSpc(TextIndent+6), vtx[1].X, vtx[1].Y, vtx[1].Z );
		Ar.Logf( TEXT("%svertex %1.6f %1.6f %1.6f\r\n"), appSpc(TextIndent+6), vtx[2].X, vtx[2].Y, vtx[2].Z );

		Ar.Logf( TEXT("%sendloop\r\n"), appSpc(TextIndent+4) );
		Ar.Logf( TEXT("%sendfacet\r\n"), appSpc(TextIndent+2) );
	}

	Ar.Logf( TEXT("%sendsolid LevelBSP\r\n"), appSpc(TextIndent) );

	Triangles.Empty();

	return 1;
	unguard;
}
IMPLEMENT_CLASS(ULevelExporterSTL);

/*------------------------------------------------------------------------------
	UPrefabExporterT3D implementation.
------------------------------------------------------------------------------*/

void UPrefabExporterT3D::StaticConstructor()
{
	guard(UPrefabExporterT3D::StaticConstructor);

	SupportedClass = UPrefab::StaticClass();
	bText = 1;
	new(Formats)FString(TEXT("T3D"));

	unguard;
}
UBOOL UPrefabExporterT3D::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	guard(UPrefabExporterT3D::ExportText);
	UPrefab* Prefab = CastChecked<UPrefab>( Object );

	Ar.Logf( TEXT("%s"), *Prefab->T3DText );

	return 1;
	unguard;
}
IMPLEMENT_CLASS(UPrefabExporterT3D);

/*------------------------------------------------------------------------------
	The end.
------------------------------------------------------------------------------*/

