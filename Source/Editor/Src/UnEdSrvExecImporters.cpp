//=============================================================================
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#include "EditorPrivate.h"

extern TCHAR TempStr[256], TempFname[256], TempName[256];
//TCHAR TempStr[256], TempFname[256], TempName[256], Temp[256];

#define NODECA 1

#if !NODECA
#include "Deca.h" // sjs
// sjs --- import handlers for deca
UBOOL UEditorEngine::Exec_DecaAnim( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("IMPORT")) )
	{
		// ANIM animating hierarchy object import.
		if
		(	Parse( Str, TEXT("ANIM="), TempName, ARRAY_COUNT(TempName) )
		&&	Parse( Str, TEXT("ANIMFILE="), TempStr, ARRAY_COUNT(TempStr) )
		)
		{
			DecaAnim* pAnim = NULL;
            pAnim = new( ParentContext, TempName, RF_Public|RF_Standalone )DecaAnim;
			FLOAT CompDefault = 1.0f;
			Parse( Str, TEXT("COMPRESS="), CompDefault );
            pAnim->animCompress = CompDefault;
            ImportDecaAnimation( TempStr, pAnim, Ar );
            debugf(TEXT("Imported DECA ANIM"));
		}
		else
            Ar.Log(NAME_ExecWarning,TEXT("Bad DECA ANIM IMPORT"));
		return 1;
	}
    else if( ParseCommand(&Str,TEXT("SEQUENCE")) )
	{
		// Set up skeletal animation sequences. 
		DecaAnim *pAnim = NULL;
		
		FLOAT AnimRate;
		FLOAT TrackTime;
        SequenceDataT NewSeq;

		if
		(	ParseObject<DecaAnim>( Str, TEXT("ANIM="), pAnim, ANY_PACKAGE )
		&&	Parse( Str, TEXT("SEQ="), NewSeq.name )
		&&	Parse( Str, TEXT("STARTFRAME="), NewSeq.startKey )
		&&	Parse( Str, TEXT("NUMFRAMES="), NewSeq.numKeys )
        )
		{
			if ( pAnim->Keys.Num() == 0 )
			{
				Ar.Logf(NAME_ExecWarning,TEXT("Illegal DECA sequence, the animation data is empty %s! (file missing?)"), pAnim->GetName() );
				return 1;
			}

            int totalKeys = pAnim->Keys.Num() / pAnim->AnimBones.Num();
            if ( NewSeq.startKey <= totalKeys-NewSeq.numKeys )
            {
                NewSeq.numRawFrames = NewSeq.numKeys;

				// Optional parameters

				if (!Parse( Str, TEXT("GROUP="), NewSeq.group ))
					NewSeq.group = NAME_None; // gam

				if( !Parse( Str, TEXT("RATE="), AnimRate ))
					AnimRate = 1.0f;
            
                NewSeq.rate = AnimRate;
				
				if( !Parse( Str, TEXT("TRACKTIME="), TrackTime ))
					TrackTime = 1.0f;
				
				// Detect which anim sequence to change, or make a new one.
                INT i;
				for( i=0; i<pAnim->Sequences.Num(); i++ )
                {
                    SequenceDataT* pSeq = &pAnim->Sequences(i);
					if( pSeq->name == NewSeq.name )
                    {
						break;
                    }
                }
                
				if( i<pAnim->Sequences.Num() )
				{
					pAnim->Sequences(i) = NewSeq;
				}
				else
				{
                    i = pAnim->Sequences.Num();
					pAnim->Sequences.AddZeroed(1);
                    pAnim->Sequences(i) = NewSeq;
				}
                debugf( TEXT("Imported DecaSeq: %s Group: %s"), *NewSeq.name, *NewSeq.group );
            }
            else
                Ar.Logf(NAME_ExecWarning,TEXT("Bad DECA ANIM SEQUENCE %s: frames out of bounds!"), *NewSeq.name );

		}
        else
            Ar.Log(NAME_ExecWarning,TEXT("Bad DECA ANIM SEQUENCE"));
        return 1;
    }
    else if( ParseCommand(&Str,TEXT("NOTIFY")) )
	{
		// Animation notifications.
		DecaAnim *Anim;
		FName SeqName;
		FMeshAnimNotify Notify;
		FLOAT Frame = -1.0f;
		if
		(	ParseObject<DecaAnim>( Str, TEXT("ANIM="), Anim, ANY_PACKAGE )
		&&	Parse( Str, TEXT("SEQ="), SeqName )
		&&	( Parse( Str, TEXT("TIME="), Notify.Time ) || Parse( Str, TEXT("FRAME="), Frame) )
		&&	Parse( Str, TEXT("FUNCTION="), Notify.Function ) )
		{
            // Detect which anim sequence to change, or make a new one.
            SequenceDataT* Seq = NULL;
			for( INT i=0; i<Anim->Sequences.Num(); i++ )
            {
				if( Anim->Sequences(i).name == SeqName )
                {
                    Seq = &Anim->Sequences(i);
					break;
                }
            }

            // calc notify time if frame was specified
            if( Seq && Frame >= 0.0f)
			{		
				// Valid Frame indicators are in range [0..(NumFrames-1)]
				if( (Seq->numRawFrames > 1) && (Frame < ((FLOAT)Seq->numRawFrames-1)) )
					Notify.Time = ( Frame / ((FLOAT)Seq->numRawFrames-1 ) );							
				else
                {
					Ar.Log( NAME_ExecWarning, TEXT(" Out-of-range notification frame in ANIM NOTIFY"));    
                    return 1;
                }
			}

            if ( Seq )
                new( Seq->Notifys )FMeshAnimNotify( Notify );
            else
                Ar.Log( NAME_ExecWarning, TEXT("Unknown skeletal animation sequence in ANIM NOTIFY") );
		}
		else Ar.Log( NAME_ExecWarning, TEXT("Bad ANIM NOTIFY") );
		return 1;
	}
    else if( ParseCommand(&Str,TEXT("DIGEST")) )
	{
		// Animation final digest - along the lines of our Sequences and Notifys.
		DecaAnim *pAnim;
		if(	ParseObject<DecaAnim>( Str, TEXT("ANIM="), pAnim, ANY_PACKAGE ) )
		{
			// Write debugging info to log if verbose mode requested.
			UBOOL bVerbose;
			FString TempStr;
			bVerbose = Parse( Str, TEXT("VERBOSE"), TempStr);
            pAnim->Compress( 0.0f, 0.0f );
            for( int i=0; i<pAnim->Sequences.Num(); i++ )
            {
                debugf(TEXT("Sequence: %s Group: %s RawFrames: %d"), *pAnim->Sequences(i).name,
                    *pAnim->Sequences(i).group, pAnim->Sequences(i).numRawFrames );
            }
		}
		else
            Ar.Log( NAME_ExecWarning, TEXT("Bad ANIM AnimCompress") );
		return 1;
	}
    else
    {
        Ar.Log( NAME_ExecWarning, TEXT("Bad DECA exec") );
    }
	return 0;
}

UBOOL UEditorEngine::Exec_DecaMesh( const TCHAR* Str, FOutputDevice& Ar )
{
	if( ParseCommand(&Str,TEXT("MODELIMPORT")) )
	{
        if( Parse( Str, TEXT("MESH="), TempName, ARRAY_COUNT(TempName) )
			&& Parse( Str, TEXT("MODELFILE="), TempStr, ARRAY_COUNT(TempStr)) )
        {
            UDecaMesh* pDecaMesh = NULL;
            pDecaMesh = FindObject<UDecaMesh>( ParentContext, TempName, 1 );
            if ( !pDecaMesh ) // create the mesh and import the skin
            {
                pDecaMesh = new( ParentContext, TempName, RF_Public|RF_Standalone )UDecaMesh;
                pDecaMesh->BoundingBox.Init();
            }
            //TempStr, use for skin name
            DecaSkin* pSkin = NULL;
            pSkin = FindObject<DecaSkin>( ParentContext, TempStr, 1 );
            if ( !pSkin )
            {
                pSkin = new( ParentContext, TempStr, RF_Public|RF_Standalone )DecaSkin;
            }
            ImportDecaModel( TempStr, pSkin, Ar ); // gam
            pDecaMesh->skins.AddItem( pSkin );

            for ( int i=0; i<pSkin->BasePoints.Num(); i++ )
            {
                pDecaMesh->BoundingBox += *(FVector*)&pSkin->BasePoints(i).v;
            }
            debugf(TEXT("Imported DECA Skin!"));
            UBOOL Rigid = 0;
            Parse( Str, TEXT("RIGID="), Rigid );
            if ( Rigid )
            {
                pSkin->alwaysRigid = 1;
                debugf(TEXT("DECA Skin set Rigid-only!"));
            }
        }
        else
        {
            Ar.Log(NAME_ExecWarning,TEXT("Bad MODEL IMPORT"));
        }
		return 1;
    }
    else if( ParseCommand(&Str,TEXT("POSTPROCESS")) )
	{
        if ( Parse( Str, TEXT("MESH="), TempName, ARRAY_COUNT(TempName)) )
        {
            UDecaMesh* pDecaMesh = NULL;
            pDecaMesh = FindObject<UDecaMesh>( ParentContext, TempName, 1 );
            if ( !pDecaMesh ) // create the mesh and import the skin
            {
                Ar.Logf(NAME_ExecWarning,TEXT("Bad POSTPROCESS Mesh %s not found!"), TempName );
                return 1;
            }
            pDecaMesh->PostProcess();
            Ar.Logf(NAME_ExecWarning,TEXT("%s POSTPROCESS done."), TempName);
        }
        else
        {
            Ar.Log(NAME_ExecWarning,TEXT("Bad POSTPROCESS"));
        }
        return 1;
    }
    else if( ParseCommand(&Str,TEXT("ORIGIN")) )
	{
		// Mesh origin. 
		UDecaMesh *Mesh;
		if( ParseObject<UDecaMesh>(Str,TEXT("MESH="),Mesh,ANY_PACKAGE) )
		{
			if( Mesh->IsA(UDecaMesh::StaticClass()))
			{
				UDecaMesh *pDecaMesh = (UDecaMesh*)Mesh;
				GetFVECTOR ( Str, pDecaMesh->origin );
				GetFROTATOR( Str, pDecaMesh->rotation, 256 );
			}
			else
                Ar.Log( NAME_ExecWarning, TEXT("Unsupported Deca mesh class for ORIGIN"));
		}
		else
            Ar.Log( NAME_ExecWarning, TEXT("Bad MESH ORIGIN") );
		return 1;
	}
    else if( ParseCommand( &Str, TEXT("SCALE") ) )
	{
		// Mesh scaling.
		UDecaMesh* Mesh;
		if( ParseObject<UDecaMesh>( Str, TEXT("MESH="), Mesh, ANY_PACKAGE ) )
		{
			FVector Scale(0.f,0.f,0.f);
			GetFVECTOR( Str, Mesh->scale );
		}
		else
            Ar.Log( NAME_ExecWarning, TEXT("Scaling Missing DecaMesh") );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("SETTEXTURE") ) )
	{
		// Mesh texture mapping.
		UDecaMesh* Mesh = NULL;
		UTexture* Texture = NULL;
		INT Num = 0;

		if(	!ParseObject<UDecaMesh>( Str, TEXT("MESH="), Mesh, ANY_PACKAGE ) )
            Ar.Logf( NAME_ExecWarning, TEXT("Bad or missing DecaMesh specification (%s)"), Str );
		else if ( !ParseObject<UTexture>( Str, TEXT("TEXTURE="), Texture, ANY_PACKAGE ) )
            Ar.Logf( NAME_ExecWarning, TEXT("Bad or missing texture specification (%s)"), Str );
		else if ( !Parse( Str, TEXT("NUM="), Num ) )
            Ar.Logf( NAME_ExecWarning, TEXT("Bad or missing num specification (%s)"), Str );
        else if ( !Mesh->IsA(UDecaMesh::StaticClass() ) )
            Ar.Logf( NAME_ExecWarning, TEXT("%s is not an DecaMesh (%s)"), Mesh->GetName(), Str );
   		else
		{
            if ( Num+1 > Mesh->textures.Num() )
            {
                Mesh->textures.AddZeroed( (Num+1) - Mesh->textures.Num() );
            }
			Mesh->textures( Num ) = Texture;
			debugf( TEXT("Added texture number: %i total %i for mesh %s"), Num, Mesh->textures.Num(), Mesh->GetName() );
		}
		return 1;
	}
    else if( ParseCommand(&Str, TEXT("ATTACHNAME")) )
	{
		UDecaMesh* Mesh;
		FName BoneName;
		FName TagName;
        AttachmentT att;

		if
		(	ParseObject<UDecaMesh>( Str, TEXT("MESH="), Mesh, ANY_PACKAGE )
		&&	Parse( Str, TEXT("BONE="), att.bone )
		&&	Parse( Str, TEXT("TAG="), att.alias ))	
		{						
			// Parse Optional adjustment.
			GetFVECTOR( Str, att.origin );				
			GetFROTATOR( Str, att.rotation, 256 );
			FCoords TagCoords = GMath.UnitCoords / att.origin / att.rotation;
            att.localMat = TagCoords.Matrix();
            if ( Mesh->skins.Num() == 0 || Mesh->skins(0)->Skeleton.Num() == 0 )
            {
                Ar.Log( NAME_ExecWarning, TEXT("Deca MESH missing skin or skeleton.") );
            }
            else
            {
                INT i;
                for ( i=0; i<Mesh->skins(0)->Skeleton.Num(); i++ )
                {
                    if ( att.bone == Mesh->skins(0)->Skeleton(i).name )
                    {
                        att.boneIndex = i;
                        Mesh->attachments.AddItem( att );
                        debugf(TEXT("Deca bone attachment successful.") );
                        break;
                    }
                }
                if( i==Mesh->skins(0)->Skeleton.Num() )
                {
                    Ar.Logf( NAME_ExecWarning, TEXT("Failed Attachment, bone not found: %s."), *att.bone );
                }
            }
		}
		else
            Ar.Log( NAME_ExecWarning, TEXT("Bad MESH ATTACHNAME parameters.") );
		return 1;
    }
    else if( ParseCommand(&Str, TEXT("DEFAULTANIM")) )
	{
		// Link up a UMeshAnimation object to be the default animation repertoire for a skeletal mesh
		// Meant for backwards compatibility where calling a 'linkanim' would be difficult.
		UDecaMesh* Mesh;
		DecaAnim* Anim;
		if
		(	ParseObject<UDecaMesh>( Str, TEXT("MESH="), Mesh, ANY_PACKAGE )
		&&	ParseObject<DecaAnim>( Str, TEXT("ANIM="), Anim, ANY_PACKAGE ) )
		{
			if( Mesh->IsA(UDecaMesh::StaticClass()))
            {
				Mesh->gameAnims = Anim;
                Mesh->menuAnims = Anim;
            }
            debugf(TEXT("Assigned DECA DEFAULTANIM") );
            Mesh->PostProcess();
		}
		else
            Ar.Log( NAME_ExecWarning, TEXT("Bad MESH DECA DEFAULTANIM") );
		return 1;

	}
    else if( ParseCommand(&Str, TEXT("LODPARAMS")) )
    {
        // Stub.
        return 1;
    }
    else
    {
        Ar.Log( NAME_ExecWarning, TEXT("Bad DECA exec") );
    }
	return 0;
}
// --- sjs
#endif

// amb, jij ---
UBOOL UEditorEngine::Exec_StaticMeshImport( const TCHAR* Str, FOutputDevice& Ar )
{
    FString File, Name;
    FName PkgName, GroupName;
    UStaticMesh* Mesh;

    Parse(Str,TEXT("FILE="),File);

	if( !Parse( Str, TEXT("Name="), Name ) )
	{
        TCHAR TempName[NAME_SIZE];

		// Deduce package name from filename.
		const TCHAR* End = *File + File.Len();
		while( End>*File && End[-1]!=PATH_SEPARATOR[0] && End[-1]!='/' )
			End--;
		appStrncpy( TempName, End, NAME_SIZE );

		if( appStrchr(TempName,'.') )
			*appStrchr(TempName,'.') = 0;

        Name = TempName;
	}

    if ( !Parse( Str, TEXT("PACKAGE="), PkgName ) )
    {
        PkgName = ParentContext ? ParentContext->GetFName() : NAME_None;
    }
    UPackage* Pkg = CreatePackage( NULL,*PkgName );
    if( Parse( Str, TEXT("GROUP="), GroupName ) && GroupName!=NAME_None )
    {
        Pkg = CreatePackage(Pkg,*GroupName);
    }
    Pkg->bDirty = 1;

	UBOOL	Collision = 1;
	Parse(Str,TEXT("COLLISION="),Collision);

	GBuildStaticMeshCollision = Collision;

    // use extension to determine if we have a Lightwave file
    if ( appStrstr(*File,TEXT(".lwo")) )
    {
        // todo: could allow user to specify a layer, and then import from that layer 
        // of the Lightwave file only...                
        Mesh = CreateStaticMeshFromLWO( Pkg, *Name, *File, NULL, NULL );
    }
    else
    {
        Ar.Logf( NAME_ExecWarning, TEXT("Could not import non .lwo format: %s"), *File );
        return 0;
    }

	GBuildStaticMeshCollision = 1;

    if( !Mesh )
    {
        Ar.Logf( NAME_ExecWarning, TEXT("Could not import %s as %s"), *File, *Name );
    }

    return 1;
}

UBOOL UEditorEngine::Exec_StaticMeshImportToEditor( const TCHAR* Str, FOutputDevice& Ar )
{
	if( Parse(Str,TEXT("FILE="),TempFname,ARRAY_COUNT(TempFname)) )
	{
        GWarn->BeginSlowTask( TEXT("Importing Static Mesh"), 1);
        Trans->Begin( TEXT("BRUSH IMPORT_STATIC") );

        FString FileName = TempFname;

        FString ActorClassName;
        Parse(Str,TEXT("CLASS="),ActorClassName);

		UClass* Class;

		if( !ParseObject<UClass>( Str, TEXT("CLASS="), Class, ANY_PACKAGE ) )
            Class = AStaticMeshActor::StaticClass();

		Level->Modify();
		FinishAllSnaps(Level);

        if ( appStrstr(*FileName,TEXT(".lwo")) )
        {
            CreateStaticMeshFromLWO( Level->GetOuter(), NAME_None, FileName, Level, Class );
        }

        Trans->End();
        GWarn->EndSlowTask();
	}
	else 
    {
        Ar.Log( NAME_ExecWarning, TEXT("Missing filename") );
    }

	return 1;
}
// --- jij, amb

