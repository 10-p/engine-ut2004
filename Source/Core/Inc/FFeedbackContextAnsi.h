/*=============================================================================
	FFeedbackContextAnsi.h: Unreal Ansi user interface interaction.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

/*-----------------------------------------------------------------------------
	FFeedbackContextAnsi.
-----------------------------------------------------------------------------*/

//
// Feedback context.
//
class FFeedbackContextAnsi : public FFeedbackContext
{
public:
	// Variables.
	INT SlowTaskCount;
	INT WarningCount;
	FContextSupplier* Context;
	FOutputDevice* AuxOut;

	// Local functions.
	void LocalPrint( const TCHAR* Str )
	{
		printf( "%s", TCHAR_TO_ANSI(Str) );
	}

	// Constructor.
	FFeedbackContextAnsi()
	: SlowTaskCount( 0 )
	, WarningCount( 0 )
	, Context( NULL )
	, AuxOut( NULL )
	{}
	void Serialize( const TCHAR* V, EName Event )
	{
		guard(FFeedbackContextAnsi::Serialize);
		// The decorated line is built in an FString, so a message of any length is printed whole.
		FString Temp;
		if( Event==NAME_Title )
		{
			return;
		}
		else if( Event==NAME_Heading )
		{
            // gam --- Yes, I'm anal.
            INT StringLength, TotalDashes, PreceedingDashes, TrailingDashes;
            INT i;

            StringLength = appStrlen (V);
            TotalDashes = (StringLength < 70) ? 70 - StringLength : 0;
            PreceedingDashes = TotalDashes / 2;
            TrailingDashes = TotalDashes - PreceedingDashes;

            for (i = 0; i < PreceedingDashes; i++)
                Temp += TEXT("-");

            Temp += V;

            for (i = 0; i < TrailingDashes; i++)
                Temp += TEXT("-");

            // --- gam
			V = *Temp;
		}
		else if( Event==NAME_SubHeading )
		{
			Temp = FString(V) + TEXT("...");
			V = *Temp;
		}
		else if( Event==NAME_Error || Event==NAME_Warning || Event==NAME_ExecWarning || Event==NAME_ScriptWarning )
		{
			if( Context )
			{
				Temp = Context->GetContext() + TEXT(" : ") + *FName(Event) + TEXT(", ") + V;
				V = *Temp;
			}

            // gam ---
            if( Event==NAME_Error )
                ErrorCount++;
            else
			    WarningCount++;
            // --- gam
		}
		else if( Event==NAME_Progress )
		{
			LocalPrint( V );
			LocalPrint( TEXT("\r") );
			fflush( stdout );
			return;
		}
		LocalPrint( V );
		LocalPrint( TEXT("\n") );
		if( GLog != this )
			GLog->Serialize( V, Event );
		if( AuxOut )
			AuxOut->Serialize( V, Event );
		fflush( stdout );
		unguard;
	}
	UBOOL YesNof( const TCHAR* Fmt, ... )
	{
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt, Fmt );
		guard(FFeedbackContextAnsi::YesNof);
		if( (GIsClient || GIsEditor) && !ParseParam(appCmdLine(),TEXT("auto")) )//!!
		{
			LocalPrint( TempStr );
			LocalPrint( TEXT(" (Y/N): ") );
				char InputText[256];
				fgets( InputText, sizeof(InputText), stdin );
				return (InputText[0]=='Y' || InputText[0]=='y');
			}
		else return 1;
		unguard;
	}
	void BeginSlowTask( const TCHAR* Task, UBOOL StatusWindow )
	{
		guard(FFeedbackContextAnsi::BeginSlowTask);
		GIsSlowTask = ++SlowTaskCount>0;
		unguard;
	}
	void EndSlowTask()
	{
		guard(FFeedbackContextAnsi::EndSlowTask);
		check(SlowTaskCount>0);
		GIsSlowTask = --SlowTaskCount>0;
		unguard;
	}
	UBOOL VARARGS StatusUpdatef( INT Numerator, INT Denominator, const TCHAR* Fmt, ... )
	{
		guard(FFeedbackContextAnsi::StatusUpdatef);
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt, Fmt );
		if( GIsSlowTask )
		{
			//!!
		}
		return 1;
		unguard;
	}
	void SetContext( FContextSupplier* InSupplier )
	{
		guard(FFeedbackContextAnsi::SetContext);
		Context = InSupplier;
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

