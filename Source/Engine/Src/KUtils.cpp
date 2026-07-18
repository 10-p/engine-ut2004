/*============================================================================
	Karma Integration Support
    
    - MeMemory/MeMessage glue
    - Debug line drawing
============================================================================*/

#include "EnginePrivate.h"

#ifdef WITH_KARMA

#include "MeStream.h"
#include "MeAssetDBXMLIO.h"


void ENGINE_API KarmaTriListDataInit(KarmaTriListData* list)
{
	list->num_tri = 0;
	list->num_vert = 0;
	list->num_norm = 0;
}

void MEAPI KSetSecName(TCHAR* name)
{
    guard(KSetSecName);
    appStrncpy(KGData->SectionName, name, 256);
    unguard;
}

/* ******************** MEMORY HANDLERS **************************** */

void* MEAPI KMemCreate(size_t bytes)
{
    guardSlow(KMemCreate);
    return appMalloc(bytes, KGData->SectionName);
    unguardSlow;
}

void MEAPI KMemDestroy(void *const block)
{
    guardSlow(KMemDestroy);
    appFree(block);
    unguardSlow;
}

void* MEAPI KMemResize(void *const block, size_t bytes)
{
    guardSlow(KMemResize);
    return appRealloc(block, bytes, KGData->SectionName);
    unguardSlow;
}

void* MEAPI KMemCreateZeroed(size_t bytes)
{
    guardSlow(KMemCreateZeroed);
    size_t i;
    BYTE* mem = (BYTE*)appMalloc(bytes, KGData->SectionName);
    for(i=0; i<bytes; i++)
        mem[i] = BYTE(0);
    return (void*)mem;
    unguardSlow;
}

/* ******************** POOL HANDLERS **************************** */

void MEAPI KPoolMallocReset(MePool* u)
{
    guardSlow(KPoolMallocReset);

    MeWarning(1,"%s\n","MePoolMallocReset() not implemented");

	unguardSlow;
}

void MEAPI KPoolMallocInit(MePool* u, int poolSize, int structSize, int alignment)
{
    guardSlow(KPoolMallocInit);

    struct MePoolMalloc *const pool = &u->u.malloc;

    u->t = MePoolMALLOC;
    pool->usedStructs = 0;
    pool->poolSize = poolSize;
    pool->structSize = structSize;
    pool->alignment = alignment;

	unguardSlow;
}

void MEAPI KPoolMallocDestroy(MePool* u)
{
    guardSlow(KPoolMallocDestroy);

	struct MePoolMalloc *const pool = &u->u.malloc;
    MEASSERT(u->t == MePoolMALLOC);

    if (pool->usedStructs != 0)
        MeFatalError(0,"MePoolMallocDestroy(): %d structs still allocated", pool->usedStructs);

	unguardSlow;
}

void* MEAPI KPoolMallocGetStruct(MePool* u)
{
    guardSlow(KPoolMallocGetStruct);

	struct MePoolMalloc *const pool = &u->u.malloc;
    MEASSERT(u->t == MePoolMALLOC);

    void * p;
	if(pool->alignment == 0)
		p = MeMemoryAPI.create(pool->structSize);
	else
		p = MeMemoryAPI.createAligned(pool->structSize, pool->alignment);

	if (p == 0)
		return 0;

	pool->usedStructs++;

	return p;

	unguardSlow;
}

void MEAPI KPoolMallocPutStruct(MePool* u, void* s)
{
    guardSlow(KPoolMallocPutStruct);

    struct MePoolMalloc *const pool = &u->u.malloc;
    MEASSERT(u->t == MePoolMALLOC);

    if (pool->usedStructs == 0)
        MeWarning(0, "%s\n","MePoolMallocPutStruct: Putting structure back "
            "into pool 0x%08x with no allocated structs.",(INT)(PTRINT) pool);

    if (s != 0)
    {
		if(pool->alignment == 0)
			MeMemoryAPI.destroy(s);
		else
			MeMemoryAPI.destroyAligned(s);

        if (pool->usedStructs > 0)
          --pool->usedStructs;
    }

	unguardSlow;
}

int MEAPI KPoolMallocGetUsed(MePool* u)
{
    guardSlow(KPoolMallocGetUsed);

	const struct MePoolMalloc *const pool = &u->u.malloc;
    return pool->usedStructs;

	unguardSlow;
}

int MEAPI KPoolMallocGetUnused(MePool* u)
{
    guardSlow(KPoolMallocGetUnused);

    const struct MePoolMalloc *const pool = &u->u.malloc;
    return pool->poolSize - pool->usedStructs;

	unguardSlow;
}

/* ******************** MESSAGE HANDLERS **************************** */

void MEAPI KMessageShow(const int level,const char *const string)
{
    guard(KDebugShow);
#ifdef UNICODE
    TCHAR tmp[MeMAXMESSAGE];
    #ifdef WIN32  // !!! FIXME: wtf? --ryan.
      swprintf(tmp, TEXT("%hs"), string);
    #else
      wcscpy(tmp, (const UNICHAR *) string);
    #endif
    debugf(tmp);
#else
    debugf((TCHAR*)string);
#endif
    unguard;
}

void MEAPI KDebugHandler(const int level, const char *const format,va_list ap)
{
    guard(KMessageHandler);
    char message[MeMAXMESSAGE];
    char *tmp, *karmaString = "(Karma): ";

    if (level > MeInfoLevel)
        return;
        
    strncpy(message, karmaString, sizeof message);
    tmp = message + strlen(karmaString);
    (void) vsprintf(tmp,format,ap); /* KTODO: Should be vsnprintf! */
    tmp = message + strlen(message) - 1; /* Knock off final character (if its a newline) */
    if(*tmp == '\n')
        *tmp = '\0';

    (*MeInfoShow)(level,message);
    unguard;
}

void MEAPI KErrorHandler(const int level, const char *const format,va_list ap)
{
	guard(KMessageHandler);

    char message[MeMAXMESSAGE];
    char *tmp, *karmaString = "(Karma): ";

    if (level > MeInfoLevel)
        return;
        
    strncpy(message, karmaString, sizeof message);
    tmp = message + strlen(karmaString);
    (void) vsprintf(tmp,format,ap); /* KTODO: Should be vsnprintf! */
    tmp = message + strlen(message) - 1; /* Knock off final character (if its a newline) */
    if(*tmp == '\n')
        *tmp = '\0';

    (*MeInfoShow)(level,message);

	if(appIsDebuggerPresent()) 
		appDebugBreak();

	unguard;
}

/* ******************** MATHS / CONVERSION **************************** */

void ENGINE_API KME2UCoords(FCoords* coords, const MeMatrix4 tm)
{
    guardSlow(KME2UCoords);
    FVector upos = FVector(
        K_ME2UScale * tm[3][0], 
        K_ME2UScale * tm[3][1], 
        K_ME2UScale * tm[3][2]);

    *coords = FCoords(
        upos,
        FVector(tm[0][0], tm[0][1], tm[0][2]), 
        FVector(tm[1][0], tm[1][1], tm[1][2]), 
        FVector(tm[2][0], tm[2][1], tm[2][2]));

    unguardSlow;
}

void ENGINE_API KU2METransform(MeMatrix4 tm, const FVector pos, const FRotator rot)
{
    guardSlow(KU2METransform);
    FCoords rotCoords = FCoords(FVector(0, 0, 0));
    rotCoords *= rot;
  
    tm[0][0] = rotCoords.XAxis.X;
    tm[0][1] = rotCoords.YAxis.X;
    tm[0][2] = rotCoords.ZAxis.X;
    tm[0][3] = 0;
    
    tm[1][0] = rotCoords.XAxis.Y;
    tm[1][1] = rotCoords.YAxis.Y;
    tm[1][2] = rotCoords.ZAxis.Y;
    tm[1][3] = 0;
    
    tm[2][0] = rotCoords.XAxis.Z;
    tm[2][1] = rotCoords.YAxis.Z;
    tm[2][2] = rotCoords.ZAxis.Z;
    tm[2][3] = 0;

    tm[3][0] = K_U2MEScale * pos.X;
    tm[3][1] = K_U2MEScale * pos.Y;
    tm[3][2] = K_U2MEScale * pos.Z;
    tm[3][3] = 1;
    unguardSlow;
}

void ENGINE_API KME2UTransform(FVector* pos, FRotator* rot, const MeMatrix4 tm)
{
    guardSlow(KME2UTransform);
    pos->X = K_ME2UScale * tm[3][0];
    pos->Y = K_ME2UScale * tm[3][1];
    pos->Z = K_ME2UScale * tm[3][2];

    FCoords c;
    KME2UCoords(&c, tm);
    *rot = c.OrthoRotation();
    unguardSlow;
}

void ENGINE_API KU2MEPosition(MeVector3 mv, const FVector fv)
{
    guardSlow(KU2MEPosition);
    mv[0] = K_U2MEScale * fv.X;
    mv[1] = K_U2MEScale * fv.Y;
    mv[2] = K_U2MEScale * fv.Z;
    unguardSlow;
}

void ENGINE_API KME2UPosition(FVector* fv, const MeVector3 mv)
{
    guardSlow(KME2UPosition);
    fv->X = K_ME2UScale * mv[0];
    fv->Y = K_ME2UScale * mv[1];
    fv->Z = K_ME2UScale * mv[2];
    unguardSlow;
}

void ENGINE_API KU2MEVecCopy(MeVector3 out, const FVector in)
{
    guardSlow(KU2MEVecCopy);
    out[0] = in.X;
    out[1] = in.Y;
    out[2] = in.Z;
    unguardSlow;
}

void ENGINE_API KME2UVecCopy(FVector* out, const MeVector3 in)
{
    guardSlow(KME2UVecCopy);
    out->X = in[0];
    out->Y = in[1];
    out->Z = in[2];
    unguardSlow;
}

void ENGINE_API KME2UMatrixCopy(FMatrix* out, MeMatrix4 in)
{
    guardSlow(KME2UVecCopy);
	
	out->M[0][0] = in[0][0];
	out->M[0][1] = in[0][1];
	out->M[0][2] = in[0][2];
	out->M[0][3] = in[0][3];

	out->M[1][0] = in[1][0];
	out->M[1][1] = in[1][1];
	out->M[1][2] = in[1][2];
	out->M[1][3] = in[1][3];

	out->M[2][0] = in[2][0];
	out->M[2][1] = in[2][1];
	out->M[2][2] = in[2][2];
	out->M[2][3] = in[2][3];

	out->M[3][0] = in[3][0];
	out->M[3][1] = in[3][1];
	out->M[3][2] = in[3][2];
	out->M[3][3] = in[3][3];

    unguardSlow;
}

void ENGINE_API KU2MEMatrixCopy(MeMatrix4 out, FMatrix* in)
{
    guardSlow(KME2UVecCopy);
	
	out[0][0] = in->M[0][0];
	out[0][1] = in->M[0][1];
	out[0][2] = in->M[0][2];
	out[0][3] = in->M[0][3];

	out[1][0] = in->M[1][0];
	out[1][1] = in->M[1][1];
	out[1][2] = in->M[1][2];
	out[1][3] = in->M[1][3];

	out[2][0] = in->M[2][0];
	out[2][1] = in->M[2][1];
	out[2][2] = in->M[2][2];
	out[2][3] = in->M[2][3];

	out[3][0] = in->M[3][0];
	out[3][1] = in->M[3][1];
	out[3][2] = in->M[3][2];
	out[3][3] = in->M[3][3];

    unguardSlow;
}

/********************* OTHER... *****************************/

// Starting at this actor - 'walk' to every other actor connected by a KConstraint,
// setting eaches JoinedTag to be the same.
// This function is recursive. If calling externally - always use newTag==1.

void MEAPI KUpdateJoined(AActor* actor, UBOOL newTag)
{
	guard(KUpdateJoined);

	// JTODO: Should this go somewhere better? Slightly unpleasant.
	static INT currentJoinedTag = 0;

	// Get a new tag number.
	if(newTag)
		currentJoinedTag++;

	// Traverse no further if we have already reached here.
	if(actor->JoinedTag == currentJoinedTag)
		return;

	// Set this Actor to be part of this joined group.
	actor->JoinedTag = currentJoinedTag;

	// Now iterate over all constraints, going to the joined Actor.
	McdModelID model = actor->getKModel();
	if(model)
	{
		MdtBodyID body = McdModelGetBody(model);
		if(body) // Can't have any constraints if it has no body.
		{
			MeDict *dict = &body->constraintDict;
			MeDictNode *nextNode, *node = MeDictFirst(dict);
			while(node != 0)
			{
				MdtConstraintID c = (MdtConstraintID)MeDictNodeGet(node);
				nextNode = MeDictNext(dict, node);

				// Dont want to traverse across contacts.
				if(MdtConstraintDCastContact(c) == 0 && MdtConstraintDCastContactGroup(c) == 0)
				{
					
				// Its safe to pass NULL into KBodyGetActor
				AActor* a1 = KBodyGetActor(MdtConstraintGetBody(c,0));
				AActor* a2 = KBodyGetActor(MdtConstraintGetBody(c,1));

				// One of these _must_ be the current actor.
				check(a1 == actor || a2 == actor);

				if(a1 && a1 != actor && !a1->bDeleteMe)
					KUpdateJoined(a1, 0);
				else if(a2 && a2 != actor && !a2->bDeleteMe)
					KUpdateJoined(a2, 0);
				}

				node = nextNode;
			}
		}
	}

	unguard;
}

/* Debug line-drawing - Takes lines in MathEngine scale!! Color components between 0 and 1. */
void MEAPI KLineDraw(MeVector3 start, MeVector3 end, MeReal r, MeReal g, MeReal b)
{
    guard(KLineDraw);
	
	if(!GTempLineBatcher)
        return;

	GTempLineBatcher->AddLine(
		K_ME2UScale * FVector(start[0], start[1], start[2]),
		K_ME2UScale * FVector(end[0], end[1], end[2]),
		FColor(255*r, 255*g, 255*b) );
    
    unguard;
}



/* For creating joints, find the nearest body in this actor and return it. */
void MEAPI KFindNearestActorBody(AActor* actor, MeVector3 pos, FName boneName, MdtBodyID* ab, McdModelID* am)
{
    guard(KFindNearestActorBody);
    /* Rag-doll actor - find nearest bone centre-of-mass */
    if(	actor->Physics == PHYS_KarmaRagDoll && 
		actor->Mesh && actor->Mesh->IsA(USkeletalMesh::StaticClass()))
    {
        USkeletalMesh* skelMesh = Cast<USkeletalMesh>(actor->Mesh);
        USkeletalMeshInstance* inst = 
            Cast<USkeletalMeshInstance>(skelMesh->MeshGetInstance(actor));

		// If there is a bone name supplied, find the bone with that name.
		if(boneName != NAME_None)
		{
			INT boneIx = inst->MatchRefBone(boneName);
			
			// Get model from this bone. If no model, return NULL model and body.
			*am = inst->KSkelModels(boneIx);
			if(!(*am))
			{
				*am = 0;
				*ab = 0;
				return;
			}

			// Get body and return it.
			*ab = McdModelGetBody(*am);
		}
		else
		{
			MeReal closeMag = MEINFINITY;
			MdtBodyID closeBody = 0;
			McdModelID closeModel = 0;
	        int b;

			for(b=0; b<inst->KSkelModels.Num(); b++)
			{
				if(inst->KSkelModels(b))
				{
					MdtBodyID body = McdModelGetBody(inst->KSkelModels(b));
					MeVector3 toCom;
					MeVector3Subtract(toCom, pos, body->comTM[3]);

					MeReal mag = MeVector3Magnitude(toCom);
					if(mag < closeMag)
					{
						closeMag = mag;
						closeBody = body;
						closeModel = inst->KSkelModels(b);
					}
				}
			} 

			*am = closeModel;
			*ab = closeBody;
		}

        return;
    }
    /* Single-body actor - easy! */
    else if(actor->getKModel())
    {
        *am = actor->getKModel();
        *ab = McdModelGetBody(*am);
        return;
    }
    else
    {
        *am = 0;
        *ab = 0;
        return;
    }
    unguard;
}

// Karma-related console command handling
UBOOL MEAPI KExecCommand(const TCHAR* Cmd, FOutputDevice* Ar)
{
	guard(KExecCommand);

    if( ParseCommand( &Cmd, TEXT("KDRAW")))
    {
		if(!KGData)
		{
			Ar->Log(TEXT("Cannot execute Karma debugging options (No KGData).") );
			return 0;
		}

        if(ParseCommand( &Cmd, TEXT("COLLISION"))) 
            KGData->DebugDrawOpt = KGData->DebugDrawOpt ^ KDRAW_Collision;
        else if(ParseCommand( &Cmd, TEXT("CONTACTS"))) 
            KGData->DebugDrawOpt = KGData->DebugDrawOpt ^ KDRAW_Contacts;
        else if(ParseCommand( &Cmd, TEXT("JOINTS"))) 
            KGData->DebugDrawOpt = KGData->DebugDrawOpt ^ (KDRAW_Axis | KDRAW_Limits);
        else if(ParseCommand( &Cmd, TEXT("TRIANGLES"))) 
            KGData->DebugDrawOpt = KGData->DebugDrawOpt ^ KDRAW_Triangles;
        else if(ParseCommand( &Cmd, TEXT("COM"))) 
            KGData->DebugDrawOpt = KGData->DebugDrawOpt ^ KDRAW_COM;
        else if(ParseCommand( &Cmd, TEXT("ORIGIN"))) 
            KGData->DebugDrawOpt = KGData->DebugDrawOpt ^ KDRAW_Origin;
        else if(ParseCommand( &Cmd, TEXT("WHEEL"))) 
            KGData->DebugDrawOpt = KGData->DebugDrawOpt ^ KDRAW_WheelPhys;
        else if(ParseCommand( &Cmd, TEXT("SAFETIME"))) 
            KGData->DebugDrawOpt = KGData->DebugDrawOpt ^ KDRAW_SafeTime;
		else if(ParseCommand( &Cmd, TEXT("REPULSORS"))) 
            KGData->DebugDrawOpt = KGData->DebugDrawOpt ^ KDRAW_Repulsors;
        else
            return 0;

        Ar->Log(TEXT("Karma debugging option recognized") );
        return 1;
    }
    else if( ParseCommand( &Cmd, TEXT("KSTEP")))
    {
		if(!KGData)
		{
			Ar->Log(TEXT("Cannot execute Karma debugging options (No KGData).") );
			return 0;
		}

        KGData->bDoTick = !KGData->bDoTick; 
        return 1;
    }
    else if( ParseCommand( &Cmd, TEXT("KSTOP")))
    {
		if(!KGData)
		{
			Ar->Log(TEXT("Cannot execute Karma debugging options (No KGData).") );
			return 0;
		}

        KGData->bAutoEvolve = !KGData->bAutoEvolve;
        return 1;
    }
	else if( ParseCommand( &Cmd, TEXT("KSAFETIME")))
	{
		if(!KGData)
		{
			Ar->Log(TEXT("Cannot execute Karma debugging options (No KGData).") );
			return 0;
		}

        if(ParseCommand( &Cmd, TEXT("0"))) 
            KGData->bUseSafeTime = 0;
		else if(ParseCommand( &Cmd, TEXT("1"))) 
            KGData->bUseSafeTime = 1;

		if(KGData->bUseSafeTime)
		{
			Ar->Log(TEXT("Karma Safe-Time Enabled."));
		}
		else
		{
			Ar->Log(TEXT("Karma Safe-Time Disabled."));
		}
		
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("KNOTRICOLL")))
	{
		if(!KGData)
			return 0;

		KGData->bDisableTriCollision = !KGData->bDisableTriCollision;

		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("KDEBUGCONTACTS")))
	{
		if(!KGData)
			return 0;

		KGData->bDebugContacts = !KGData->bDebugContacts;

		return 1;
	}

    return 0;

	unguard;
}

// Safe to pass in NULL
AActor* MEAPI KBodyGetActor(MdtBodyID body)
{
	guard(KBodyGetActor);

	if(!body)
		return NULL;

	McdModelID model = (McdModelID)(body->model);

	return KModelGetActor(model);

	unguard;
}

// Safe to pass in NULL
AActor* MEAPI KModelGetActor(McdModelID model)
{
	guard(KModelGetActor);

	if(!model)
		return NULL;

	KarmaModelUserData* data = (KarmaModelUserData*)McdModelGetUserData(model);
	return data->actor;

	unguard;
}

// Utility for drawing all constraints and bodies in the world.
// This is done outside the physics function, so that it can be viewed even if game is paused.
void MEAPI KLevelDebugDrawConstraints(const ULevel* Level)
{
	guard(KLevelDebugDrawConstraints);

	if(!Level->KWorld)
		return;

    MeDictNode *node;
    MeDict *dict = &Level->KWorld->constraintDict;

    for(node = MeDictFirst(dict); node !=0; node = MeDictNext(dict, node))
    {
		MdtConstraintID con = (MdtConstraintID)MeDictNodeGet(node);
		KConstraintDraw(con, KGData->DebugDrawOpt, KLineDraw);
    }

	unguard;
}

// Turn off (Term) any AKConstraints attached to this body.
void KBodyTermKConstraints(MdtBodyID body)
{
	guard(KBodyTermKConstraints);

	MeDict *dict = &body->constraintDict;
	MeDictNode *nextNode, *node = MeDictFirst(dict);
	while(node != 0)
	{
		MdtConstraintID c = (MdtConstraintID)MeDictNodeGet(node);
		nextNode = MeDictNext(dict, node);
		if(c->head.userData)
		{
			AKConstraint* con = Cast<AKConstraint>((UObject*)c->head.userData);

			if(con)
				KTermConstraintKarma(con); // This will destroy the Mdt constraint.
		}

		node = nextNode;
	}

	unguard;
}

/* ******************************************************************** */

static void MatrixScalePosition(MeMatrix4 tm, FVector scale3D)
{
	tm[3][0] *= scale3D.X;
	tm[3][1] *= scale3D.Y;
	tm[3][2] *= scale3D.Z;
}

// Create an McdGeometry (at the correct scale) from this static mesh.
// 'name' is really just for debugging
// The order of creation MUST be the same in order for KBoneNameFromElementIndex to work!!
McdGeometryID ENGINE_API KAggregateGeomInstance(FKAggregateGeom* uGeom, FVector scale3D, McdGeomMan* geomMan, const TCHAR* name)
{
	guard(KAggregateGeomInstance);

	int i, j, totalElems;
	
	// It's only convex elements that can be used if we have a non-uniform scale factor.
	if(scale3D.IsUniform())
		totalElems = uGeom->GetElementCount();
	else
		totalElems = uGeom->ConvexElems.Num();

	if(totalElems == 0)
	{
		// Do more useful warning if we were trying to 3D scale a geom with no convex.
		if(!scale3D.IsUniform() && uGeom->GetElementCount() > 0)
			debugf(TEXT("(Karma:) KAggregateGeomInstance: (%s) Cannot 3D-Scale Karma primitives (sphere, box, cylinder)."), name);
		else
			debugf(TEXT("(Karma:) KAggregateGeomInstance: (%S) No geometries in FKAggregateGeom."), name);

		return 0;
	}

	// Well - we didn't have it, so create a new one here.
	MeMatrix4 tmpTM;
	McdGeometryID tmpGeom;
	McdGeometryID aggGeom = McdAggregateCreate(geomMan->fwk, totalElems);
	if(KGData)
		(KGData->GeometryCount)++;

	// We can only use the sphere/box/cylinder parts if scale3D is uniform.
	if(scale3D.IsUniform())
	{
		for(i=0; i<uGeom->SphereElems.Num(); i++)
		{
			FKSphereElem* sphereElem = &uGeom->SphereElems(i);
			tmpGeom = McdSphereCreate(geomMan->fwk, sphereElem->Radius * scale3D.X);
			if(KGData)
				(KGData->GeometryCount)++;

			KU2MEMatrixCopy(tmpTM, &sphereElem->TM);
			MatrixScalePosition(tmpTM, scale3D);
			McdAggregateAddElement(aggGeom, tmpGeom, tmpTM);
		}

		for(i=0; i<uGeom->BoxElems.Num(); i++)
		{
			FKBoxElem* boxElem = &uGeom->BoxElems(i);
			tmpGeom = McdBoxCreate(geomMan->fwk, boxElem->X * scale3D.X, boxElem->Y * scale3D.X, boxElem->Z * scale3D.X);
			if(KGData)
				(KGData->GeometryCount)++;

			KU2MEMatrixCopy(tmpTM, &boxElem->TM);
			MatrixScalePosition(tmpTM, scale3D);
			McdAggregateAddElement(aggGeom, tmpGeom, tmpTM);
		}

		for(i=0; i<uGeom->CylinderElems.Num(); i++)
		{
			FKCylinderElem* cylElem = &uGeom->CylinderElems(i);
			tmpGeom = McdCylinderCreate(geomMan->fwk, cylElem->Radius * scale3D.X, cylElem->Height * scale3D.X);
			if(KGData)
				(KGData->GeometryCount)++;

			KU2MEMatrixCopy(tmpTM, &cylElem->TM);
			MatrixScalePosition(tmpTM, scale3D);
			McdAggregateAddElement(aggGeom, tmpGeom, tmpTM);
		}
	}

	for(i=0; i<uGeom->ConvexElems.Num(); i++)
	{
		FKConvexElem* convexElem = &uGeom->ConvexElems(i);
		MeVector3* verts = (MeVector3*)MeMemoryALLOCA(convexElem->VertexData.Num() * sizeof(MeVector3));
		
		for(j=0; j<convexElem->VertexData.Num(); j++)
		{
			FVector tmpV = convexElem->VertexData(j);
			verts[j][0] = tmpV.X * scale3D.X;
			verts[j][1] = tmpV.Y * scale3D.Y;
			verts[j][2] = tmpV.Z * scale3D.Z;
		}
		
		tmpGeom = McdConvexMeshCreateHull(geomMan->fwk, verts, convexElem->VertexData.Num(), 0);

		if(tmpGeom)
		{
			if(KGData)
				(KGData->GeometryCount)++;

			KU2MEMatrixCopy(tmpTM, &convexElem->TM);
			MatrixScalePosition(tmpTM, scale3D);
			McdAggregateAddElement(aggGeom, tmpGeom, tmpTM);
		}
		//else
		//	debugf(TEXT("(Karma:) Could not create Convex geometry for Actor: %s"), name);

		MeMemoryFREEA(verts);
	}

	// Only bother returning this aggregate if it actually has any elements.
	if(McdAggregateGetElementCount(aggGeom) > 0)
		return aggGeom;
	else
	{
		McdGeometryDestroy(aggGeom);
		if(KGData)
			(KGData->GeometryCount)--;

		return 0;
	}

	unguard;
}

// Convert from the element index within the aggregate geometry, and the name of the bone its attached to.
// ONLY WORKS IF MESH IS UNIFORMLY SCALED!!!!
FName KBoneNameFromElementIndex(USkeletalMesh* smesh, INT ElementIndex)
{	
	guard(KBoneNameFromElementIndex);

	INT Ix = ElementIndex;

	if( Ix < smesh->BoneCollisionSpheres.Num() )
		return smesh->BoneCollisionSpheres(Ix).BoneName;

	Ix -= smesh->BoneCollisionSpheres.Num();

	if( Ix < smesh->BoneCollisionBoxes.Num() )
		return smesh->BoneCollisionBoxes(Ix).BoneName;

	// Should never get here - more elements than bone primitives.
	appErrorf( TEXT("Element Index (%d) larger than number of bone primitives! %s"), Ix, smesh->GetName() );

	return NAME_None;

	unguard;
}


/* *********************************************************************** */
/* *********************************************************************** */
/* *********************** MODELTOHULLS  ********************************* */
/* *********************************************************************** */
/* *********************************************************************** */

#define LOCAL_EPS (0.01f)

static void AddVertexIfNotPresent(TArray<FVector> &vertices, FVector &newVertex)
{
	guard(AddVertexIfNotPresent);

	UBOOL isPresent = 0;

	for(INT i=0; i<vertices.Num() && !isPresent; i++)
	{
		FLOAT diffSqr = (newVertex - vertices(i)).SizeSquared();
		if(diffSqr < LOCAL_EPS * LOCAL_EPS)
			isPresent = 1;
	}

	if(!isPresent)
		vertices.AddItem(newVertex);

	unguard;
}

static void AddConvexPrim(FKAggregateGeom* outGeom, TArray<FPlane> &planes, UModel* inModel, FVector &prePivot)
{
	guard(AddConvexPrim);

	// Add Hull.
	int ex = outGeom->ConvexElems.AddZeroed();
	FKConvexElem* c = &outGeom->ConvexElems(ex);

	c->PlaneData = planes;

	c->TM.SetIdentity();
	for(INT i=0; i<planes.Num(); i++)
	{
		FPoly	Polygon;
		FVector Base, AxisX, AxisY;

		Polygon.Normal = planes(i);
		Polygon.NumVertices = 4;
		Polygon.Normal.FindBestAxisVectors(AxisX,AxisY);

		Base = planes(i) * planes(i).W;

		Polygon.Vertex[0] = Base + AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX;
		Polygon.Vertex[1] = Base - AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX;
		Polygon.Vertex[2] = Base - AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX;
		Polygon.Vertex[3] = Base + AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX;

		for(INT j=0; j<planes.Num(); j++)
		{
			if(i != j)
			{
				if(!Polygon.Split(-FVector(planes(j)), planes(j) * planes(j).W))
				{
					Polygon.NumVertices = 0;
					break;
				}
			}
		}

		// Add vertices of polygon to convex primitive.
		for(INT j=0; j<Polygon.NumVertices; j++)
		{
			// Because of errors with the polygon-clipping, we dont use the vertices we just generated,
			// but the ones stored in the model. We find the closest.
			INT nearestVert = INDEX_NONE;
			FLOAT nearestDistSqr = FLT_MAX;

			for(INT k=0; k<inModel->Verts.Num(); k++)
			{
				// Find vertex vector. Bit of  hack - sometimes FVerts are uninitialised.
				INT pointIx = inModel->Verts(k).pVertex;
				if(pointIx < 0 || pointIx >= inModel->Points.Num())
					continue;

				FLOAT distSquared = (Polygon.Vertex[j] - inModel->Points(pointIx)).SizeSquared();

				if( distSquared < nearestDistSqr )
				{
					nearestVert = k;
					nearestDistSqr = distSquared;
				}
			}

			// If we have found a suitably close vertex, use that
			if( nearestVert != INDEX_NONE && nearestDistSqr < LOCAL_EPS )
			{
				FVector localVert = ((inModel->Points(inModel->Verts(nearestVert).pVertex)) - prePivot) * K_U2MEScale;
				AddVertexIfNotPresent(c->VertexData, localVert);
			}
			else
			{
				FVector localVert = (Polygon.Vertex[j] - prePivot) * K_U2MEScale;
				AddVertexIfNotPresent(c->VertexData, localVert);
			}
		}
	}

#if 1
	// We need at least 4 vertices to make a convex hull with non-zero volume.
	// We shouldn't have the same vertex multiple times (using AddVertexIfNotPresent above)
	if(c->VertexData.Num() < 4)
	{
		outGeom->ConvexElems.Remove(ex);
		return;
	}

	// Check that not all vertices lie on a line (ie. find plane)
	// Again, this should be a non-zero vector because we shouldn't have duplicate verts.
	UBOOL found;
	FVector dir2, dir1;
	
	dir1 = c->VertexData(1) - c->VertexData(0);
	dir1.Normalize();

	found = 0;
	for(INT i=2; i<c->VertexData.Num() && !found; i++)
	{
		dir2 = c->VertexData(i) - c->VertexData(0);
		dir2.Normalize();

		// If line are non-parallel, this vertex forms our plane
		if((dir1 | dir2) < (1 - LOCAL_EPS))
			found = 1;
	}

	if(!found)
	{
		outGeom->ConvexElems.Remove(ex);
		return;
	}

	// Now we check that not all vertices lie on a plane, by checking at least one lies of the plane we have formed.
	FVector normal = dir1 ^ dir2;
	normal.Normalize();

	FPlane plane(c->VertexData(0), normal);

	found = 0;
	for(INT i=2; i<c->VertexData.Num() && !found; i++)
	{
		if(plane.PlaneDot(c->VertexData(i)) > LOCAL_EPS)
			found = 1;
	}

	if(!found)
	{
		outGeom->ConvexElems.Remove(ex);
		return;
	}
#endif

	unguard;
}

// Worker function for traversing collision mode/blocking volumes BSP.
// At each node, we record, the plane at this node, and carry on traversing.
// We are interested in 'inside' ie solid leafs.
static void ModelToHullsWorker(FKAggregateGeom* outGeom,
							   UModel* inModel, 
							   INT nodeIx, 
							   UBOOL bOutside, 
							   TArray<FPlane> &planes, 
							   FVector prePivot)
{
	guard(ModelToHullsWorker);

	FBspNode* node = &inModel->Nodes(nodeIx);

	// FRONT
	if(node->iChild[0] != INDEX_NONE) // If there is a child, recurse into it.
	{
		planes.AddItem(node->Plane);
		ModelToHullsWorker(outGeom, inModel, node->iChild[0], node->ChildOutside(0, bOutside), planes, prePivot);
		planes.Remove(planes.Num()-1);
	}
	else if(!node->ChildOutside(0, bOutside)) // If its a leaf, and solid (inside)
	{
		planes.AddItem(node->Plane);
		AddConvexPrim(outGeom, planes, inModel, prePivot);
		planes.Remove(planes.Num()-1);
	}

	// BACK
	if(node->iChild[1] != INDEX_NONE)
	{
		planes.AddItem(node->Plane.Flip());
		ModelToHullsWorker(outGeom, inModel, node->iChild[1], node->ChildOutside(1, bOutside), planes, prePivot);
		planes.Remove(planes.Num()-1);
	}
	else if(!node->ChildOutside(1, bOutside))
	{
		planes.AddItem(node->Plane.Flip());
		AddConvexPrim(outGeom, planes, inModel, prePivot);
		planes.Remove(planes.Num()-1);
	}

	unguard;
}

// Function to create a set of convex geometries from a UModel.
// Replaces any convex elements already in the FKAggregateGeom.
// Create it around the model origin, and applies the UNreal->Karma scaling.
void ENGINE_API KModelToHulls(FKAggregateGeom* outGeom, UModel* inModel, FVector prePivot)
{
	guard(KModelToHulls);

	outGeom->ConvexElems.Empty();
	
	TArray<FPlane>	planes;
	ModelToHullsWorker(outGeom, inModel, 0, inModel->RootOutside, planes, prePivot);

	unguard;
}

// Utility to update the mass properties.
void ENGINE_API KUpdateMassProps(UKMeshProps* mp)
{
	guard(KUpdateMassProps);

	// Total geoms is 1 (for the aggregate) + number of children.
	int totalGeoms = 1 + mp->AggGeom.GetElementCount();

	// If this geometry is empty- do nothing.
	if(totalGeoms == 1)
		return;

	//  This is a bit of a hack - we need a framework to instance the geom to calc its mass props.
	McdFrameworkID frame = McdInit(0, 1, totalGeoms, 1);
	McdGeomMan* gm = McdGMCreate(frame);

	McdPrimitivesRegisterTypes(frame);
	McdPrimitivesRegisterInteractions(frame);
	McdAggregateRegisterType(frame);
	McdAggregateRegisterInteractions(frame);
	McdConvexMeshRegisterType(frame);
	McdConvexMeshPrimitivesRegisterInteractions(frame);
	McdConvexMeshConvexMeshRegisterInteraction(frame);
	McdNullRegisterType(frame);

	McdGeometryID tmpg = KAggregateGeomInstance(&mp->AggGeom, FVector(1,1,1), gm, TEXT("UpdateMeshProps"));

	MeMatrix4 comTM;
	MeMatrix3 I;
	MeReal vol;
	McdGeometryGetMassProperties(tmpg, comTM, I, &vol);

	McdGMDestroyGeometry(gm, tmpg);
	McdGMDestroy(gm);
	McdTerm(frame);

	// Now copy over the mass properties
	mp->InertiaTensor[0] = I[0][0];
	mp->InertiaTensor[1] = I[0][1];		
	mp->InertiaTensor[2] = I[0][2];
	mp->InertiaTensor[3] = I[1][1];
	mp->InertiaTensor[4] = I[1][2];
	mp->InertiaTensor[5] = I[2][2];

	mp->COMOffset[0] = comTM[3][0];
	mp->COMOffset[1] = comTM[3][1];
	mp->COMOffset[2] = comTM[3][2];

	unguard;
}

// Copy mass properties to the clipboard. Useful form when manually doing stuff using KarmaParamsRBFull.
void ENGINE_API KCopyMassPropsToClipboard(UKMeshProps* mp)
{
	FString props;

	props += FString(FString::Printf(TEXT("\tKInertiaTensor(0)=%f\n"), mp->InertiaTensor[0]));
	props += FString(FString::Printf(TEXT("\tKInertiaTensor(1)=%f\n"), mp->InertiaTensor[1]));
	props += FString(FString::Printf(TEXT("\tKInertiaTensor(2)=%f\n"), mp->InertiaTensor[2]));
	props += FString(FString::Printf(TEXT("\tKInertiaTensor(3)=%f\n"), mp->InertiaTensor[3]));
	props += FString(FString::Printf(TEXT("\tKInertiaTensor(4)=%f\n"), mp->InertiaTensor[4]));
	props += FString(FString::Printf(TEXT("\tKInertiaTensor(5)=%f\n"), mp->InertiaTensor[5]));
	props += FString(FString::Printf(TEXT("\tKCOMOffset=(X=%f,Y=%f,Z=%f)\n"), mp->COMOffset.X, mp->COMOffset.Y, mp->COMOffset.Z));

	appClipboardCopy(*props);
}

/* ************************* SAFE TIME SPHERES  *********************************** */

#define MAX_SAFETIME_SPHERES (3)

static void FitSpheres(const FVector& LongAxis, FLOAT LongLength, FLOAT ShortLength, TArray<FSphere>& outSpheres)
{
	guard(FitSpheres);

	INT NumSpheres = appFloor(LongLength/ShortLength);
	NumSpheres = Min(NumSpheres, MAX_SAFETIME_SPHERES);

	if(NumSpheres == 1) // If we can only fit one sphere into the object - put it in the middle.
	{
		outSpheres.AddItem( FSphere( FVector(0,0,0), 0.5f * ShortLength ) );
	}
	else // If we can fit more - spread them out as much as possible along the long axis.
	{
		FLOAT StartDist = -0.5f * (LongLength - ShortLength);
		FLOAT DeltaDist = (LongLength - ShortLength)/(NumSpheres - 1);

		for(INT i=0; i<NumSpheres; i++)
		{
			outSpheres.AddItem( FSphere( (StartDist + (DeltaDist * i)) * LongAxis, 0.5f * ShortLength ) );
		}
	}

	unguard;
}

// For a given geometry, fit a set of spheres inside to do safetime checks with. Local space, Karma scale.
static void FindGeometrySafeSpheres(McdGeometryID geom, TArray<FSphere>& outSpheres)
{
	guard(FindGeometrySafeRadius);

	McdGeometryType type = McdGeometryGetTypeId(geom);

	if(type == kMcdGeometryTypeSphere)
	{
		// Trivial case - just add a sphere
		outSpheres.AddItem( FSphere( FVector(0,0,0), McdSphereGetRadius(geom) ) );
	}
	else if(type == kMcdGeometryTypeBox)
	{
		FLOAT LX, LY, LZ, MinLength;
		FVector MaxAxis;
		McdBoxGetDimensions(geom, &LX, &LY, &LZ); // This gives lengths.

		// Find longest axis and min and max lengths.
		MinLength = MeMIN3(LX, LY, LZ);

		if(LX > LY)
		{
			if(LZ > LX)
				FitSpheres( FVector(0,0,1), LZ, MinLength, outSpheres ); // Z is longest axis
			else
				FitSpheres( FVector(1,0,0), LX, MinLength, outSpheres ); // X is longest axis
		}
		else
		{
			if(LZ > LY)
				FitSpheres( FVector(0,0,1), LZ, MinLength, outSpheres ); // Z is longest axis
			else
				FitSpheres( FVector(0,1,0), LY, MinLength, outSpheres ); // Y is longest axis
		}
	}
	else if(type == kMcdGeometryTypeCylinder)
	{
		MeReal radius = McdCylinderGetRadius(geom);
		MeReal halfH = McdCylinderGetHalfHeight(geom);

		FitSpheres( FVector(0,0,1), 2.0f * halfH, 2.0f * radius, outSpheres ); // Could have a wide, flat cylinder.. ah well..
	}
	else if(type == kMcdGeometryTypeSphyl)
	{
		MeReal radius = McdSphylGetRadius(geom);
		MeReal height = McdSphylGetHeight(geom);

		FitSpheres( FVector(0,0,1), height + 2.0f * radius, 2.0f * radius, outSpheres );
	}

	unguard;
}

void KCalcModelSafeTimeSpheres(McdModelID model)
{
	guard(KCalcModelSafeTimeSpheres);

	McdAggregateID aggGeom = McdModelGetGeometry(model);
	if(McdGeometryGetTypeId(aggGeom) != kMcdGeometryTypeAggregate)
		return;

	KarmaModelUserData* ModelData = (KarmaModelUserData*)McdModelGetUserData(model);
	if(!ModelData)
		return;

	ModelData->SafeTimeSpheres.Empty();

	for(INT i=0; i<McdAggregateGetElementCount(aggGeom); i++)
	{
		McdGeometryID elemGeom = McdAggregateGetElementGeometry(aggGeom, i);

		// Currently, dont do safetime for convex meshes...
		if( McdGeometryGetTypeId(elemGeom) == kMcdGeometryTypeConvexMesh )
			continue;

		MeMatrix4Ptr relTM = McdAggregateGetElementTransformPtr(aggGeom, i); // Get element -> model transform

		TArray<FSphere> SafeTimeSpheres;
		FindGeometrySafeSpheres(elemGeom, SafeTimeSpheres);

		for(INT j=0; j<SafeTimeSpheres.Num(); j++)
		{
			MeVector3 relPos, sphereCenter;
			KU2MEVecCopy( sphereCenter, SafeTimeSpheres(j) );

			// Work out sphere center in model space.
			MeMatrix4TMTransform(relPos, relTM, sphereCenter); 

			// Add to array of spheres.
			FSphere NewSphere;
			NewSphere.X = relPos[0];
			NewSphere.Y = relPos[1];
			NewSphere.Z = relPos[2];
			NewSphere.W = SafeTimeSpheres(j).W;
			ModelData->SafeTimeSpheres.AddItem( NewSphere );
		}

	}

	unguard;
}


/* ************************* ASSET DB  *********************************** */
/* Create a MeFile and fill it from directory */

void MEAPI KCreateAssetDB(MeAssetDB** db, McdGeomMan** gm)
{
    guard(KCreateAssetDB);

    MeStream stream;
	MeAssetDBXMLInput *input;

	debugf(NAME_DevKarma,TEXT("(Karma): Creating MeAssetDB."));

	KSetSecName(TEXT("KARMA: ASSETDB"));
    *db = MeAssetDBCreate();
	KSetSecName(TEXT("KARMA: ASSETDBXMLINPUT"));
	input = MeAssetDBXMLInputCreate(*db, 0);

	// Hack for Steve to force a .KA path if one doesn't exist.  Mod authors take note
	
	TArray<FString> StevePaths;
	new(StevePaths)FString( TEXT("../KarmaData/*.ka") );
	if ( appStrlen(GModName)>0 )
	{
		int k = StevePaths.AddZeroed();
		(StevePaths)(k) = FString::Printf(TEXT("../%s/KarmaData/*.ka"),GModName );
	}

    if(*db)
    {
        /* Create geometry manager */
		KSetSecName(TEXT("KARMA: GEOMMAN"));
        *gm = McdGMCreate(KGData->Framework);

		for (INT Pth=0; Pth < StevePaths.Num(); Pth++)
		{
			if ( StevePaths(Pth).Right(3)==TEXT(".ka") )
			{
				FString Path = StevePaths(Pth).Left(StevePaths(Pth).Len()-5);
				FString Mask = FString::Printf(TEXT("%s"),*Path)*=TEXT("*.ka");
				// Iterate over *.me files in Karma directory
				TArray<FString> Found = GFileManager->FindFiles( *Mask , 1, 0 );
				for( INT i=0; i<Found.Num(); i++ )
				{
					FString Temp = FString::Printf(TEXT("%s"),*Path) *= Found(i);
					debugf(NAME_DevKarma,TEXT("(Karma): Loading: %s"), (TCHAR*)Temp.GetCharArray().GetData());

					/* Now we convert the name in Temp to an ASCII char array with ..\Karma\ prefixed
					There has to be a better way. Perhaps the toolkit should use Unicode :)
					This is a nasty little hack.
					*/
					TArray<TCHAR> chararray = Temp.GetCharArray();
					char *data = (char*)chararray.GetData();
					char *out = (char*)MeMemoryAPI.create(512);

					#if WIN32 // !!! FIXME: this conflicts on Linux. --ryan.
						wcstombs(out, (unsigned short*)data, 512);
					#else
						strncpy(out, appToAnsi((TCHAR *) data), 512);
					#endif

					char *filename = (char*)MeMemoryAPI.create(strlen(out)+32);
					filename[0] = 0;

/*
					#if WIN32   // !!! FIXME: more cross-platform fun!  --ryan.
						filename = strcat(filename, "..\\KarmaData\\");
					#else
						filename = strcat(filename, "../KarmaData/");
					#endif
*/
					filename = strcat(filename,out);

					stream = MeStreamOpen(filename, kMeOpenModeRDONLY);
		            
					if( stream )
					{
						KSetSecName(TEXT("KARMA: INPUTREAD"));
						MeBool loaded = MeAssetDBXMLInputRead(input, stream);
						KSetSecName(TEXT("KARMA: POST INPUTREAD"));

						if( !loaded )
							debugf(TEXT("(Karma): Non-fatal error in MeFileLoad."));

						MeStreamClose(stream);
					}

					MeMemoryAPI.destroy(filename);
					MeMemoryAPI.destroy(out);
				} 
			}
		}

        /* Load all geometrys/models/joints from XML into Asset DB */
        //MeAssetDBLoadAllAssets(*db);
    }

	MeAssetDBXMLInputDestroy(input);

	if(*db)
		debugf(NAME_DevKarma,TEXT("(Karma): Finished Creating MeAssetDB (%d Assets)."), MeAssetDBGetAssetCount(*db));

    unguard;
}

/* ******************* CUSTOM MASS PROPS ******************* */

static void 
computeInverseInertia(const MeMatrix3 a, MeVector4 inv0, MeVector4 inv1, MeVector4 inv2)
{
    MeReal    r, a00, a10, a20, a11, a21, a22;

    //First, get r which is the triple product of the columns of matrix A.
    r = a[0][0] * (a[1][1] * a[2][2] - a[2][1] * a[2][1]) +
        a[1][0] * (a[2][1] * a[2][0] - a[1][0] * a[2][2]) +
        a[2][0] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);

    r = MeRecip(r);

    // Now, we some components of 3 cross products and I can't seem to find a way to do that in place.
    a00 = (a[1][1] * a[2][2] - a[2][1] * a[1][2]) * r;
    a10 = (a[2][1] * a[2][0] - a[1][0] * a[2][2]) * r;
    a20 = (a[1][0] * a[2][1] - a[1][1] * a[2][0]) * r;

    //Note that b2[0] is not needed.
    a11 = (a[2][2] * a[0][0] - a[2][0] * a[2][0]) * r;
    a21 = (a[2][0] * a[1][0] - a[2][1] * a[0][0]) * r;

    // Note that b3[0] and b3[2] are not needed.
    a22 = (a[0][0] * a[1][1] - a[1][0] * a[1][0]) * r;

    inv0[0] = a00;
    inv0[1] = a10;
    inv0[2] = a20;

    inv1[1] = a11;
    inv1[2] = a21;

    inv2[2] = a22;

    // Symmetrize if needed.
    inv1[0] = inv0[1];
    inv2[0] = inv0[2];
    inv2[1] = inv1[2];
}

void KBodySetInertiaTensor(const MdtBodyID b, const MeMatrix3 i)
{
    int j;
    MeMatrix3 clamped;
    MeReal iMag; 

    iMag = b->world->params.massScale * b->world->params.lengthScale * b->world->params.lengthScale;

    MeMatrix3Copy(clamped,i);

#if 1
	clamped[0][0] = MeCLAMP(clamped[0][0], iMag / MDTBODY_INERTIA_RANGE_LIMIT, iMag * MDTBODY_INERTIA_RANGE_LIMIT);
	clamped[1][1] = MeCLAMP(clamped[1][1], iMag / MDTBODY_INERTIA_RANGE_LIMIT, iMag * MDTBODY_INERTIA_RANGE_LIMIT);
	clamped[2][2] = MeCLAMP(clamped[2][2], iMag / MDTBODY_INERTIA_RANGE_LIMIT, iMag * MDTBODY_INERTIA_RANGE_LIMIT);
#endif

    for (j = 0; j < 3; j++)
    {
        b->keaBody.I0[j] = clamped[0][j];
        b->keaBody.I1[j] = clamped[1][j];
        b->keaBody.I2[j] = clamped[2][j];
    }

    /* invert the matrix and store in a better aligned data structure */
    computeInverseInertia(clamped, b->keaBody.invI0, b->keaBody.invI1,
        b->keaBody.invI2);
}

void KBodySetMass(const MdtBodyID b, const MeReal mass)
{
    b->mass = mass;

#if 1
	b->mass = MeCLAMP(b->mass, 
		b->world->params.massScale / MDTBODY_MASS_RANGE_LIMIT, 
		b->world->params.massScale * MDTBODY_MASS_RANGE_LIMIT);
#endif

    b->keaBody.invmass = MeRecip(b->mass);
}


void KGMDestroyGeometry(McdGeomMan* gm, McdGeometryID geom)
{
    if(McdGeometryGetReferenceCount(geom) > 0)
        return;

	if(McdGeometryGetTypeId(geom) == kMcdGeometryTypeAggregate)   
	{   
		int g, gCount = McdAggregateGetElementCount(geom);   
		
		for(g=0; g<gCount; g++)   
		{   
			McdGeometryID partGeom = McdAggregateGetElementGeometry(geom, g);   
			
			if(partGeom)
			{   
                /*  We need to remove this element from the aggregate to 
                    decrement its ref count. */
                McdAggregateRemoveElement(geom, g);
                KGMDestroyGeometry(gm, partGeom);   
			}   
		}    
	} 

	if(McdGeometryGetTypeId(geom) != kMcdGeometryTypeNull)
   	{
		 /*  Use geom2name has table to find name, and remove from both hash
			tables before destroying. */
		char *name = (char*)MeHashLookup(geom, gm->geom2name);
    
		// We are now going to destroy a geometry.
		// Destroying a geometry either decreases the GeometryManager count (ie hash size)
		// or decreases our global geometry count.
		if(name)
		{
			/* Remove from both hash tables. */
			MeHashDelete(name, gm->name2geom); 
			MeHashDelete(geom, gm->geom2name);
			McdGeometryDestroy(geom);
		}
		else
		{
			// DESTROY GEOMETRY EVEN IF NOT IN HASH
			McdGeometryDestroy(geom);
			(KGData->GeometryCount)--;
		}
	}
}

#endif // WITH_KARMA

