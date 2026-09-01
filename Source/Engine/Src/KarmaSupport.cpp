/*============================================================================
	Karma Integration Support
    
    - PerContact/PerPair Callbacks
    - Game/Level/Actor/Constraint Init/Term
============================================================================*/

#include "EnginePrivate.h"

#ifdef WITH_KARMA

/* Global data */
ENGINE_API KarmaGlobals* KGData = 0;

const int K_collisionGeometryTypesMaxCount = 3;
const int K_collisionGroupsMaxCount = 1;
const int K_materialsMaxCount = 2;
const int K_bridgeModelPairBufferSize = 1024;

#define ME_UNIT_LENGTH	((MeReal)0.95) // 0.95
#define ME_UNIT_MASS	((MeReal)0.95) 
#define ME_GAMMA_PERSEC ((MeReal)6) // 6
#define ME_EPSILON		((MeReal)0.001) // 0.001
#define ME_PENOFFSET	((MeReal)0.015) // 0.015
#define ME_PENSCALE		((MeReal)1.0) // 1
#define ME_CONTACTSOFT	((MeReal)0.008) // 0.005

// Try and make the effect of both slip and box friction the same.
#define ME_FRICTIONSCALE ((MeReal)0.5)

#define ME_MAX_TIMESTEP			((MeReal)0.04)
#define ME_SUBSTEPTARGET		((MeReal)0.025)
#define ME_MAX_SUBSTEPS			(4)

// If the timestep goes above this, will start ticking physics multiple times a frame

#define ME_MAX_PENETRATION ((MeReal)0.13)

/********************* BODY ENABLE/DISABLE CALLBACKS **********************/

// Currently these only deal with single-model karma actors (ie not ragdolls).


void MEAPI RemoveActiveActorFromBody(const MdtBodyID b)
{
	guard(RemoveActiveActorFromBody);

	AActor* actor = KBodyGetActor(b);

	if(!actor)
		return;

	if(actor->getKModel() && !actor->bDeleteMe)
		KActorContactGen(actor, 0);

	unguard;
}

void MEAPI AddActiveActorFromBody(const MdtBodyID b)
{
	guard(AddActiveActorFromBody);

	AActor* actor = KBodyGetActor(b);

	if(!actor)
		return;

	if(actor->getKModel() && !actor->bDeleteMe && actor->bBlockKarma && actor->bCollideActors)
		KActorContactGen(actor, 1);

	unguard;
}

void MEAPI DestroyContactGroupReferences(const MdtContactGroupID c)
{
	guard(DestroyContactGroupReferences);

    McdModelPairID m = (McdModelPairID)c->generator;
    if(m)
        m->responseData = 0;

	unguard;
}

// Straight add or remove from the 
void KActorContactGen(AActor* actor, UBOOL gen)
{
	guard(KActorContactGen);

	ULevel* level = actor->GetLevel();

	if(gen)
	{
		// Should set an actor to contact gen unless this is true!
		check( !actor->bDeleteMe );
		check( actor->Physics == PHYS_Karma || actor->Physics == PHYS_KarmaRagDoll );
		check( actor->bBlockKarma );
		check( actor->bCollideActors); 

		// Add if not already there.
		if(level->KContactGenActors.FindItemIndex(actor) == INDEX_NONE)
			level->KContactGenActors.AddItem(actor);
	}
	else
	{
		level->KContactGenActors.RemoveItem(actor);
	}

	unguard;
}

/********************* COPIED FROM TOOLKIT ***********************/

#define TM_STACK_SIZE 1000
#define MAX_ENTRY_COUNT 1000
#define CONTACT_BUFFER_SIZE 1000
#define MAX_PAIR_COUNT 1000
#define AGGREGATE_ARRAY_COUNT 16
#define AGGREGATE_ARRAY_SIZE 800 // 128
#define ATOMIC_ARRAY_SIZE 1024

static McdBatchContext* KBatchContextCreate(McdFramework *frame)
{
	guard(KBatchContextCreate);

    int i;

    McdBatchContext *context;
    
    context = (McdBatchContext *)MeMemoryAPI.create(sizeof(McdBatchContext));
    context->frame = frame;
    context->typeCount = context->frame->geometryRegisteredCountMax;

    context->tmArray = (MeMatrix4 *)
        MeMemoryAPI.createAligned(sizeof(MeMatrix4)*TM_STACK_SIZE,MeALIGNTO);
    context->tmTrack = (MeMatrix4Ptr **)
        MeMemoryAPI.create(sizeof(MeMatrix4Ptr *)*TM_STACK_SIZE);
    context->tmMaxCount = TM_STACK_SIZE;

    context->table = (McdBatchEntry **)MeMemoryAPI.create(sizeof(McdBatchEntry *) * 
        context->typeCount * context->typeCount); 

    context->entryArray = (McdBatchEntry *)MeMemoryAPI.create(sizeof(McdBatchEntry) * MAX_ENTRY_COUNT);
    context->maxEntryCount = MAX_ENTRY_COUNT;

    context->pairData = (McdBatchPairData *)MeMemoryAPI.create(sizeof(McdBatchPairData) * MAX_PAIR_COUNT);
    context->pairDataMaxCount = MAX_PAIR_COUNT;

    context->pools = (McdBatchContactPool *)MeMemoryAPI.create((AGGREGATE_ARRAY_COUNT+1)*sizeof(McdBatchContactPool));
    context->poolMaxCount = AGGREGATE_ARRAY_COUNT+1;
    context->pools[0].contacts = (McdContact *)MeMemoryAPI.create(ATOMIC_ARRAY_SIZE * sizeof(McdContact));
    context->pools[0].contactMaxCount = ATOMIC_ARRAY_SIZE;

    for(i=1;i<=AGGREGATE_ARRAY_COUNT;i++)
    {
        context->pools[i].contacts = (McdContact *)MeMemoryAPI.create(AGGREGATE_ARRAY_SIZE * sizeof(McdContact));
        context->pools[i].contactMaxCount = AGGREGATE_ARRAY_SIZE;
    }
    return context;

	unguard;
}

static MstBridgeID KBridgeCreate(McdFrameworkID frame, const unsigned int maxMaterials)
{
	guard(KBridgeCreate);

    unsigned int nElements, i;
    MstMaterialPair* p;
    MstBridgeID b = (MstBridgeID)MeMemoryAPI.create(sizeof(MstBridge));

    if(!b)
    {
        MeWarning(0, "MstBridgeCreate: Could not allocate memory for bridge.");
        return b;
    }

    /* Initialise material pool */
    b->maxMaterials = maxMaterials;
    nElements = MeUpperDiagonalSize(b->maxMaterials);
    b->materialPairArray = (MstMaterialPair *)
        MeMemoryAPI.create(nElements * sizeof (MstMaterialPair));

    /* Set all material pairs to the default contact params. */
    p = b->materialPairArray;
    for(i=0; i<nElements; i++)
    {
        MdtContactParamsReset(&(p->cp));

        /* Set contact callback to null. */
        p->contactCB = NULL;
        p->intersectCB = NULL;
        p->pairCB = NULL;
        p++;
    }

    /* Material '0' is used as default */
    b->usedMaterials = 1;

    /* Allocate ModelPair buffer. */
    b->pairs = McdModelPairContainerCreate(50);
    b->context = KBatchContextCreate(frame);

    /* Allocate McdContact buffer. */
    b->contactsMaxCount = 50;
    b->contacts = (McdContact*)
        MeMemoryAPI.create(b->contactsMaxCount * sizeof(McdContact));

    /* Set callbacks for body enable/disable. */
    return b;

	unguard;
}

/********************* CONTACTS ***********************/
// Do we always assume world geometry (blocking volumes etc.) have friction of 1.
#define WORLD_FRICTION_ONE (1)

/* Called for all contacts. */
static MeBool MEAPI KPerContactCB(McdIntersectResult* result, McdContact* colC, MdtContactID dynC)
{
    guard(KPerContactCB);
    
    MeVector3 normal, position;

    /* Reject rogue contacts. */
    MeReal d2, penetration = MdtContactGetPenetration(dynC);
    
	/* Subtract an amount from penetration, so we get some resting penetration. */
	penetration -= KGData->SimParams.PenetrationOffset;
	if(penetration < 0)
        penetration = 0;

	MdtContactSetPenetration(dynC, penetration * KGData->SimParams.PenetrationScale);

    MdtContactGetPosition(dynC, position);
	MdtContactGetNormal(dynC, normal);
	d2 = MeVector3Magnitude(normal);
	if (d2 < 0.98f || d2 > 1.02f)
	{
		debugf(TEXT("(Karma:) Bad Normal Length: %f"), d2);
		return 0;
	}

    McdModelID m1, m2;
    AActor *a1, *a2;
    McdModelPairGetModels(result->pair, &m1, &m2);
	a1 = KModelGetActor(m1);
	a2 = KModelGetActor(m2);

    MeI16 g1type = McdGeometryGetTypeId(McdModelGetGeometry(m1));
    MeI16 g2type = McdGeometryGetTypeId(McdModelGetGeometry(m2));

	if(KGData->bDebugContacts)
	{
		if(a1)
		{
			if(a2)
				debugf( TEXT("CONTACT: %s %s"), a1->GetName(), a2->GetName() );
			else
				debugf( TEXT("CONTACT: %s [WORLD TRIS]"), a1->GetName() );
		}
		else
		{
			if(a2)
				debugf( TEXT("CONTACT: [WORLD TRIS] %s"), a2->GetName() );
			else
				debugf( TEXT("CONTACT: [WORLD TRIS] [WORLD TRIS]") );
		}
	}

	// NB: The ONLY model without an Actor must be the 'world', which must be the tri-list.
	if((a1 && !a1->KParams) || (a2 && !a2->KParams))
	{
		debugf(TEXT("(Karma:) Contact with Actor with no KParams."));
		return 1;
	}

    MdtBodyID b1 = MdtContactGetBody(dynC, 0);
    MdtBodyID b2 = MdtContactGetBody(dynC, 1);

    //  Calculate friction/restitution from actors/physics zone. 
	//	For the world (terrain/bsp) - we use per triangle data.
    MeReal totalFriction;
	MeReal totalRes;
	MeVector3 worldVel = {0, 0, 0};
	KarmaTriUserData* triData = 0;
	ULevel* level = NULL;
	short triFlags = 0;

	if(!a1)
	{
		if(!a2)
		{
			debugf(TEXT("(Karma): Contact with 2 Triangle Lists!"));
			return 1;
		}
		else
		{
			level = a2->GetLevel();

			triData = (KarmaTriUserData*)(colC->element1.ptr);
			triFlags = colC->dims & 0x00FF;
			check(triData);

			totalFriction = a2->KParams->KFriction * triData->localFriction;
			totalRes = a2->KParams->KRestitution * triData->localRestitution;

			a2->KParams->bContactingLevel = 1;
		}
	}
	else // Non-trilist
	{
		level = a1->GetLevel();

		if(!a2)
		{
			triData = (KarmaTriUserData*)(colC->element2.ptr);
			triFlags = colC->dims>>8;
			check(triData);

			totalFriction = a1->KParams->KFriction * triData->localFriction;
			totalRes = a1->KParams->KRestitution * triData->localRestitution;

			a1->KParams->bContactingLevel = 1;
		}
		else // Non-trilist
		{
			// If its a contact with itself (eg. ragdoll) then use zero friction.
			if(a1 == a2)
			{
				totalFriction = 0;
				totalRes = 0;
			}
			else
			{
				// HACK! If this is a contact with a world-geometry object,
				// we default the friction to 1.0
				if(!McdModelGetBody(m1))
				{
#if WORLD_FRICTION_ONE
					totalFriction = a2->KParams->KFriction;
					totalRes = a2->KParams->KRestitution;
#else
					totalFriction = a1->KParams->KFriction * a2->KParams->KFriction;
					totalRes = a1->KParams->KRestitution * a2->KParams->KRestitution;
#endif
					// Check for moving world geometry (ie. bStatic==False & no Body)
					// We calculate the world velocity, and enable body.
					if(a1 && !a1->bStatic)
					{
						KU2MEPosition(worldVel, a1->Velocity);
						MdtBodyEnable(McdModelGetBody(m2));
					}

					a2->KParams->bContactingLevel = 1;
				}
				else if(!McdModelGetBody(m2))
				{
#if WORLD_FRICTION_ONE
					totalFriction = a1->KParams->KFriction;
					totalRes = a1->KParams->KRestitution;
#else
					totalFriction = a1->KParams->KFriction * a2->KParams->KFriction;
					totalRes = a1->KParams->KRestitution * a2->KParams->KRestitution;
#endif
					if(a2 && !a2->bStatic)
					{
						KU2MEPosition(worldVel, a2->Velocity);
						MdtBodyEnable(McdModelGetBody(m1));
					}

					a1->KParams->bContactingLevel = 1;
				}
				else // Karma-Karma collision.
				{
					totalFriction = a1->KParams->KFriction * a2->KParams->KFriction;
					totalRes = a1->KParams->KRestitution * a2->KParams->KRestitution;
				}
			}
		}
	}

	check(level != NULL);

    MdtContactParamsID params = MdtContactGetParams(dynC);

	if(totalFriction < 0.01f)
	{
		MdtContactParamsSetType(params, MdtContactTypeFrictionZero);
		totalFriction = 0;
	}
	else
	{
		if(level->GetLevelInfo()->bKStaticFriction) // Box Friction
		{
			MdtContactParamsSetType(params, MdtContactTypeFriction2D);
			MdtContactParamsSetFrictionModel(params, MdtFrictionModelNormalForce);
			MdtContactParamsSetFrictionCoeffecient(params, ME_FRICTIONSCALE * totalFriction);
			MdtContactParamsSetSlip( params, 0 );
		}
		else // Slip Friction
		{
			MdtContactParamsSetType(params, MdtContactTypeFriction2D);
			MdtContactParamsSetFrictionModel(params, MdtFrictionModelBox);
			MdtContactParamsSetFrictionCoeffecient(params, MEINFINITY);
			MdtContactParamsSetFriction(params, MEINFINITY);
			MdtContactParamsSetSlip( params, MeMAX(KGData->SimParams.Epsilon, (1/totalFriction)-1) );
		}
	}
	MdtContactParamsSetRestitution(params, totalRes);
	MdtContactParamsSetSoftness(params, KGData->SimParams.ContactSoftness);

	MdtContactSetWorldVelocity(dynC, worldVel[0], worldVel[1], worldVel[2]);

#if 0
    // If its tri-list contact, do some extra filtering.
    if(g1type == kMcdGeometryTypeTriangleList || g2type == kMcdGeometryTypeTriangleList)
    {
#else
	// If dynamics-world contact, do extra filtering
	if( (b1 && !b2) || (!b1 && b2) )
	{
#endif
        // Don't do hack to sphere or sphyl contacts
        if(g1type != kMcdGeometryTypeSphere && g2type != kMcdGeometryTypeSphere //)
			&& g1type != kMcdGeometryTypeSphyl && g2type != kMcdGeometryTypeSphyl)
        {
            MdtBodyID body = (b1)?b1:b2;
            if(!body)
                appErrorf(TEXT("(Karma): Contact with no bodies!"));
            
            /* HACK! Cap contact penetration to the distance travelled last timestep. */
            MeVector3 bvel;
            MdtBodyGetLinearVelocity(body, bvel);
            MeVector3Scale(bvel, KGData->TimeStep);
            MeReal bvelMag = MeVector3Magnitude(bvel);
            
            if(penetration > bvelMag + KGData->SimParams.MaxPenetration)
            {
				//debugf(TEXT("Shrink Pen F: %f T: %f"), penetration, bvelMag + (MeReal)0.1);
                MdtContactSetPenetration(dynC, bvelMag + KGData->SimParams.MaxPenetration);
            }
        }
    }
	// If physics-physics - still cap maximum penetration
	else if(b1 && b2)
	{
		if(penetration > KGData->SimParams.MaxPenetration)
		{
			//debugf(TEXT("Shrink Pen F: %f T: %f"), penetration, bvelMag + (MeReal)0.1);
			MdtContactSetPenetration(dynC, KGData->SimParams.MaxPenetration);
		}
	}

    /* Find mag of relative normal and tangential velocity at contact. */
    MeVector3 relVel, bVel1, bVel2 = {0, 0, 0};
    MdtBodyGetVelocityAtPoint(b1, position, bVel1);
    if(b2)
        MdtBodyGetVelocityAtPoint(b2, position, bVel2);
    MeVector3Subtract(relVel, bVel2, bVel1);

    /*  If either of these actors are listening for the 'impact' event, 
        check rel vel magnitude and call event if threshold exceeded. */
    if( (a1 && a1->IsProbing(ENGINE_KImpact)) || (a2 && a2->IsProbing(ENGINE_KImpact)) )
    {
        MeReal relVelMag = MeVector3Magnitude(relVel);
        FVector upos, uvel, unorm = FVector(normal[0], normal[1], normal[2]);
        KME2UPosition(&upos, position);
        KME2UPosition(&uvel, relVel);
            
        if(a1 && (relVelMag * K_ME2UScale > a1->KParams->KImpactThreshold) && a1->IsProbing(ENGINE_KImpact))
            a1->eventKImpact(a2, upos, uvel, unorm);

        if(a2 && (relVelMag * K_ME2UScale > a2->KParams->KImpactThreshold) && a2->IsProbing(ENGINE_KImpact))
            a2->eventKImpact(a1, upos, uvel, unorm);
    }

    /*** TYRE MODEL ***/
    guard(TireModel);

	// Find which actor is the tyre, and which is the ground its on.
    AKTire* tire1 = Cast<AKTire>(a1);
    AKTire* tire2 = Cast<AKTire>(a2);

    AKTire* t;
	AActor* ground;
	if(tire1)
	{
		t = tire1;
		ground = a2;
	}
	else
	{
		t = tire2;
		ground = a1;
	}

    /* if this is a tyre/non-tyre collision, we do the tyre model stuff */
    if(t && !(tire1 && tire2))
    {
        AKCarWheelJoint* cw = t->WheelJoint;
        if(!cw)
           goto endContact;

		MdtConstraintID constraint = cw->getKConstraint();
		if(!constraint)
			goto endContact;

        MdtCarWheelID mdtCW = MdtConstraintDCastCarWheel(constraint);
        if(!mdtCW)
            goto endContact;

        MdtBodyID tBody = McdModelGetBody(t->getKModel());
        if(!tBody)
            goto endContact;

		// Do contact filtering for tri-list (ie when there is no ground actor).
		McdContact *cp1 = &(result->contacts[0]);
		if(!ground && colC != cp1) 
		{
			check(triData);

			// If its an edge or vertex contact, and its angle is too similar to our first face contact,
			// throw it away.
			if((triFlags != 2) && MeVector3Dot(normal, cp1->normal) > 0.5f) 
			{
				// Straightened normal is too similar to cp1's.  We can kill it.
				return 0;
			}
		}

#if 0
		// Set all normal direction to the same as the first face contact.
		if (colC == cp1 || ((colC->dims>>8) == 2)) 
		{
			colC->normal[0] = normal[0];
			colC->normal[1] = normal[1];
			colC->normal[2] = normal[2];

			MdtContactSetNormal(dynC, normal[0], normal[1], normal[2]);
			MdtContactSetPosition(dynC, position[0], position[1], position[2]);
			MdtContactSetPenetration(dynC, penetration);
		}
#endif

        /* calculate rolling direction of tyre at contact by cross product of wheel axis with contact normal */
        MeVector3 haxis, dir;
        MdtCarWheelGetHingeAxis(mdtCW, haxis);

        MeVector3Cross(dir, haxis, normal);
        if(MeVector3MagnitudeSqr(dir) < 0.01 * 0.01) /* If this is bad (ie. contact with side of wheel!) */
        {
            MeVector3 a, b;
            MeVector3PlaneSpace(normal, a, b); /* Pick any direction orth. to normal */
            MeVector3Copy(dir, a);
        }
        else
            MeVector3Normalize(dir);

        MdtContactSetDirection(dynC, dir[0], dir[1], dir[2]);

		// Calculate how far up the sphere the contact is.
		// Use frictionless contacts (and do no more tyre model stuff) if its less than 45 degrees (ie sides of wheel).
		MeReal contactAngle = MeVector3Dot(haxis, normal);
		if(fabs(contactAngle) > 0.7071f)
		{
			MdtContactParamsSetType(params, MdtContactTypeFrictionZero);
		}
		else // TYRE MODEL HERE!
		{
			MdtContactParamsSetType(params, MdtContactTypeFriction2D);
			MdtContactParamsSetFrictionModel(params, MdtFrictionModelNormalForce);

			if(triData)
			{
				MdtContactParamsSetPrimaryFrictionCoeffecient(params, t->RollFriction * triData->localFriction);
				MdtContactParamsSetSecondaryFrictionCoeffecient(params, t->LateralFriction * triData->localFriction);
			}
			else
			{
				MdtContactParamsSetPrimaryFrictionCoeffecient(params, t->RollFriction);
				MdtContactParamsSetSecondaryFrictionCoeffecient(params, t->LateralFriction);
			}

			// calculate slip (force proportional to velocity) terms
			MeVector3 angVel;
			MdtBodyGetAngularVelocity(tBody, angVel);
			MeReal meSpinSpeed = MeFabs(MeVector3Dot(haxis, angVel)); // rad/sec
			t->SpinSpeed = K_Rad2U * meSpinSpeed; // 65535 = 1 rev/sec

			MeReal priSlip = MeMIN(t->RollSlip, t->MinSlip + (meSpinSpeed * t->SlipRate));
			MeReal secSlip = MeMIN(t->LateralSlip, t->MinSlip + (meSpinSpeed * t->SlipRate));

			//debugf(TEXT("Slip: %f, %f"), priSlip, secSlip);

			// Fabs just here to be sure :)
			MdtContactParamsSetPrimarySlip(params, MeFabs(priSlip));
			MdtContactParamsSetSecondarySlip(params, MeFabs(secSlip));

			// normal direction
			MdtContactParamsSetRestitution(params, t->Restitution);
			MdtContactParamsSetMaxAdhesiveForce(params, t->Adhesion);
			MdtContactParamsSetSoftness(params, t->Softness);

			// This flag is reset at the start of KUpdateContacts.
			// Note - this flag not set for wheel-side contacts.
			t->bTireOnGround = 1;

			// Note with this stuff, if there are multiple contacts, it will just be the last contact,
			// but I can't think of a better way to choose. Normally there will be only 1.

			// Subtract component of velocity in normal direction and cal magnitude
			MeReal normVelMag = MeVector3Dot(relVel, normal);
			MeVector3MultiplyAdd(relVel, -normVelMag, normal);
			t->GroundSlipVel = MeVector3Magnitude(relVel); // used for squeels etc.
			KME2UPosition(&(t->GroundSlipVec), relVel);

			// Set the material that the tyre is on.
			if(triData)
			{
				check(!ground);
				t->GroundMaterial = triData->localMaterial;
				t->GroundSurfaceType = t->GroundMaterial->SurfaceType;
			}
			else
			{
				check(ground);
				t->GroundMaterial = NULL;
				t->GroundSurfaceType = ground->SurfaceType;
			}

#if 0
			if(!MdtCarWheelIsSteeringLocked(mdtCW))
			{
				TCHAR propName[512];
				appSprintf(propName, TEXT("%s-PriSlip"), t->GetName());
				//GStatGraph->AddDataPoint(propName, priSlip, 1);

				appSprintf(propName, TEXT("%s-SecSlip"), t->GetName());
				//GStatGraph->AddDataPoint(propName, secSlip, 1);

				appSprintf(propName, TEXT("%s-NormForce"), t->GetName());
				MeVector3 normForce;
				MdtContactGetForce(dynC, 0, normForce);
				GStatGraph->AddDataPoint(propName, MeSqrt(normForce[2] * normForce[2]), 1);

				appSprintf(propName, TEXT("%s-TanForce"), t->GetName());
				GStatGraph->AddDataPoint(propName, MeSqrt(normForce[0] * normForce[0] + normForce[1] * normForce[1]), 1);

				appSprintf(propName, TEXT("%s-SlipVel"), t->GetName());
				GStatGraph->AddDataPoint(propName, t->GroundSlipVel, 1);
				//debugf(TEXT("SS:%s-%f"), propName, t->GroundSlipVel);
			}
#endif
		}
    }
    
    unguard;

    

endContact:
	// Done inside KLevelDebugDrawConstraints now
    //KConstraintDraw(MdtContactQuaConstraint(dynC), KGData->DebugDrawOpt, KLineDraw);

    return 1;

    unguard;
}

/* Called for pairs of contacts. */
static MeBool MEAPI KPerPairCB(McdIntersectResult* result, MdtContactGroupID c)
{
    guard(KPerPairCB);
    return 1;
    unguard;
}

/********************** INIT/TERM *************************/


/*	This destroys the model, and the geometry if nothing else needs it. 
    Will also try to shutdown if bKGShutdownPending and no models/geometry left. */
void MEAPI KModelDestroy(McdModelID model)
{
    guard(KModelDestroy);

	MeMatrix4Ptr modelTM = McdModelGetTransformPtr(model);
    McdGeometryID geom = McdModelGetGeometry(model);

	KarmaModelUserData* data = (KarmaModelUserData*)McdModelGetUserData(model);
	check(data->OverlapModels.Num() == 0); // Check we have been properly removed from any pairs.
	delete data;

    McdModelDestroy(model);
    (KGData->ModelCount)--;
	
	MeMemoryAPI.destroyAligned(modelTM);
	
	/*  We dont want to destroy the Null geometry in the Geometry Manager. */
    if(McdGeometryGetTypeId(geom) != kMcdGeometryTypeNull)
    {
        KGMDestroyGeometry(KGData->GeomMan, geom);
    }

    KTermGameKarma(); // 'Try' and shut down.
    unguard;
}

/*  Add dynamics to an Actor.
    Should call KInitActorCollision first. */
void MEAPI KInitActorDynamics(AActor* actor)
{
    guard(KInitActorDynamics);

    if(!KGData || actor->bDeleteMe) 
        return;

    ULevel* level = actor->GetLevel();
    if(GIsEditor || !KGData->Framework || !level || actor->bDeleteMe)
        return;

	if(actor->bStatic)
		debugf(TEXT("(Karma): KInitActorDynamics: bStatic is true."));

	RTN_WITH_ERR_IF(!actor->KParams, "(Karma): KInitActorDynamics: No KParams.");

    // Check we have collision set up.
    McdModelID model = actor->getKModel();
	RTN_WITH_ERR_IF(!model, "(Karma): KInitActorDynamics: No Model.");

    MdtBodyID body = McdModelGetBody(model);
    if(body)
        return; /* We already have dynamics. */

    RTN_WITH_ERR_IF(actor->Physics != PHYS_Karma, "(Karma): KInitActorDynamics: Not in a Karma PHYS mode.");

	// We need to have a KarmaParams, a KarmaParamsCollision isn't enough.
	UKarmaParams* kparams = Cast<UKarmaParams>(actor->KParams);
    RTN_WITH_ERR_IF(!kparams, "(Karma): KInitActorDynamics: No KarmaParams.");

    MdtWorldID world = actor->GetLevel()->KWorld;
    RTN_WITH_ERR_IF(!world, "(Karma): KInitActorDynamics: No Karma World");

	// // GET SOME PHYSICS PROPERTIES // //
	UKMeshProps* mp = 0;

	// Try the static mesh
	if(actor->StaticMesh)
		mp = actor->StaticMesh->KPhysicsProps;

	// Now try a skeletal mesh
	if( !mp )
	{
		USkeletalMesh* smesh = 0;
		if(actor->Mesh)
			smesh = Cast<USkeletalMesh>(actor->Mesh);
		if(smesh)
			mp = smesh->KPhysicsProps;
	}

	// Last check is a KarmaParamsRBFull
	// We have to get inertia tensor/com-offset from _somewhere_!
    if( !mp && !kparams->IsA(UKarmaParamsRBFull::StaticClass()) )
	{
		debugf(TEXT("(Karma): KInitActorDynamics: (%s) No StaticMesh or KarmaParamsRBFull"), actor->GetName());
		return;
	}

    /* Get 'static' model transformation, to free later */
    MeMatrix4Ptr oldTM = McdModelGetTransformPtr(model);

	// Remove any model-pairs (will get recreated next frame)
	KGoodbyeAffectedPairs(model, level);

    /* Create a new body and set all its properties from the asset. */
    body = MdtBodyCreate(world);

    MeMatrix4 mMatrix;
    KU2METransform(mMatrix, actor->Location, actor->Rotation);
    MdtBodySetTransform(body, mMatrix);
    McdModelSetBody(model, body);

	// Use DrawScale as physics scaling
	kparams->KScale = actor->DrawScale;
	kparams->KScale3D = actor->DrawScale3D;

	// First set mass properties from data stored with StaticMesh.
	if(mp)
	{
		MeMatrix3 I;
		FVector totalScale = kparams->KScale3D * kparams->KScale;
		I[0][0] =			mp->InertiaTensor[0] * kparams->KMass * totalScale.Y * totalScale.Z;
		I[0][1] = I[1][0] = mp->InertiaTensor[1] * kparams->KMass * totalScale.X * totalScale.Y;
		I[0][2] = I[2][0] = mp->InertiaTensor[2] * kparams->KMass * totalScale.Z * totalScale.X;
		I[1][1] =			mp->InertiaTensor[3] * kparams->KMass * totalScale.X * totalScale.Z;
		I[1][2] = I[2][1] = mp->InertiaTensor[4] * kparams->KMass * totalScale.Y * totalScale.Z;
		I[2][2] =			mp->InertiaTensor[5] * kparams->KMass * totalScale.X * totalScale.Y;

		KBodySetInertiaTensor(body, I);
		KBodySetMass(body, kparams->KMass);
		
		MeVector3 o;
		KU2MEVecCopy(o, mp->COMOffset * totalScale);
		MeDictNode *node = MeDictFirst(&body->constraintDict);
		if(!node)
			MdtBodySetCenterOfMassRelativePosition(body, o);
	}

    // Set body properties from KParams parameters.
	// If KParams is a KarmaParamsRBFull it will use inertia-tensor/com-offset from there.
    actor->KParams->PostEditChange();

    //MeMemoryAPI.destroy(oldTM);
	MeMemoryAPI.destroyAligned(oldTM);


	if(kparams->KStartEnabled)
	{
		MdtBodyEnable(body); // This should call AddActiveActorFromBody
	}
	else
	{
		MdtBodyDisable(body); // This should call RemoveActiveActorFromBody
	}

	// Finally, set startup linear ang angular velocity 
	MeVector3 meLinVel, meAngVel;
	KU2MEPosition(meLinVel, kparams->KStartLinVel);
	KU2MEPosition(meAngVel, kparams->KStartAngVel);
	MdtBodySetLinearVelocity(body, meLinVel[0], meLinVel[1], meLinVel[2]);
	MdtBodySetAngularVelocity(body, meAngVel[0], meAngVel[1], meAngVel[2]);

	// Calculate the list of safe-time spheres to use when we move.
	KCalcModelSafeTimeSpheres(model);

    unguard;
}

/*  Remove dynamics from an Actor (leaving just collision). */
void MEAPI KTermActorDynamics(AActor* actor)
{
    guard(KTermActorDynamics);

    if(!KGData) 
        return;

    ULevel* level = actor->GetLevel();
    McdModelID model = actor->getKModel();
	if(!model || !level)
		return;
	
    MdtBodyID body = McdModelGetBody(model);
	if(!body)
		return;
    
	UKarmaParams* kparams = Cast<UKarmaParams>(actor->KParams);

	/* Remove any Angular3's on this Actor. */
    if(kparams && kparams->KAng3)
	{
		MdtAngular3ID ang3 = (MdtAngular3ID)kparams->KAng3;
		MdtAngular3Disable(ang3);
		MdtAngular3Destroy(ang3);
		kparams->KAng3 = 0;
		kparams->bKStayUpright = 0;
	}

	// Remove any KRepulsor contacts on this actor.
	if(kparams && kparams->Repulsors.Num() > 0)
	{
		for(INT i=0; i<kparams->Repulsors.Num(); i++)
		{
			AKRepulsor* rep = kparams->Repulsors(i);
			if(rep->KContact)
			{
				MdtContactGroupID cg = (MdtContactGroupID)rep->KContact;

				if( MdtContactGroupIsEnabled(cg) )
					MdtContactGroupDisable(cg);

				MdtContactGroupDestroy(cg);
				rep->KContact = 0;
			}
		}
	}

	// Remove any model-pairs (will get recreated next frame)
	KGoodbyeAffectedPairs(model, level);

	// Do any vehicle clean-up (removing wheel-contacts)
	ASVehicle* v = Cast<ASVehicle>(actor);
	if(v)
		KTermSVehicleDynamics(v);

	// Term any KConstraints to this body.
	KBodyTermKConstraints(body);
        
    // Allocate matrix, copy MdtBody transform into it and destroy MdtBody.
	KSetSecName(TEXT("KARMA: MODEL TM"));
    MeMatrix4Ptr modelTM = (MeMatrix4Ptr)MeMemoryAPI.createAligned(sizeof(MeMatrix4), 16);
	KSetSecName(TEXT("KARMA: POST MODEL TM"));

    MdtBodyGetTransform(body, modelTM);

    MdtBodyDisable(body); // _should_ freeze model, if in space
    
    McdModelSetBody(model, 0);
    McdModelSetTransformPtr(model, modelTM);

    MdtBodyDestroy(body);

	// We do a final update on its bounding box to make sure.
    McdGeometryID geom = McdModelGetGeometry(model);
    if(McdGeometryGetTypeId(geom) != kMcdGeometryTypeNull)
    {
        McdModelUpdate(model);
    }

	// Make sure actor is not in KContactGenActors array.
	KActorContactGen(actor, 0);

    unguard;
}

// Cleanup all McdGeometry currently in this StaticMesh. Must be called before the StaticMesh is destroyed.
void MEAPI KTermStaticMeshCollision(UStaticMesh* smesh)
{
	// Iterate over all McdGeometrys, decrementing reference count.
	int i;
	for(i=0; i<smesh->KCollisionGeom.Num(); i++)
	{
		McdGeometryID geom = smesh->KCollisionGeom(i);
		if(geom)
		{
			McdGeometryDecrementReferenceCount(geom);
			KGMDestroyGeometry(KGData->GeomMan, geom); // this will try to delete geometry, if nothing else needs it.
			smesh->KCollisionGeom(i) = 0;
		}
	}
	
	KTermGameKarma(); // In case we need to shut down.
}



static McdGeometryID KCreateActorGeometry(AActor* actor)
{
	guard(KCreateActorGeometry);

	McdGeometryID geom = 0;
	
	USkeletalMesh* smesh = 0;
	if(actor->Mesh)
		smesh = Cast<USkeletalMesh>(actor->Mesh);

	FVector scale3D = actor->DrawScale * actor->DrawScale3D;

	// For actors with a static mesh which has Karma physics props - use that.
	if(actor->StaticMesh && actor->StaticMesh->KPhysicsProps && actor->StaticMesh->UseSimpleKarmaCollision)
	{
		KSetSecName(TEXT("KARMA: INIT COLLISION: GEOM: STATICMESH"));

		if(actor->StaticMesh->KPhysicsProps->AggGeom.GetElementCount() == 0)
		{
			debugf(TEXT("(Karma): StaticMesh (%s) with empty Karma KAggregateGeometry."), actor->StaticMesh->GetName() );
			return NULL;
		}

		// First - see if this geometry has already been created at (about) the correct scale.
		for(INT i=0; i<actor->StaticMesh->KCollisionGeomScale3D.Num(); i++)
		{
			if((actor->StaticMesh->KCollisionGeomScale3D(i) - scale3D).IsNearlyZero())
				geom = actor->StaticMesh->KCollisionGeom(i); // yes it has!
		}

		if(!geom)
		{
			// If it hasn't, then create it here, and add to static meshes list of geoms.
			geom = KAggregateGeomInstance(&actor->StaticMesh->KPhysicsProps->AggGeom, 
				actor->DrawScale * actor->DrawScale3D,
				KGData->GeomMan, actor->GetName());

			if(geom)
			{
				actor->StaticMesh->KCollisionGeomScale3D.AddItem(scale3D);
				actor->StaticMesh->KCollisionGeom.AddItem(geom);
				McdGeometryIncrementReferenceCount(geom); // Count this StaticMeshes reference to this geometry.
			}
		}
	}
	// Skeletal meshes with karma collision - for use with vehicles
	else if(smesh && smesh->KPhysicsProps)
	{
		KSetSecName(TEXT("KARMA: INIT COLLISION: GEOM: SKELETALMESH"));

		if(!scale3D.IsUniform())
		{
			debugf(TEXT("(Karma): Actor (%s) with non uniform scale factor cannot use skeletal mesh karma collision."), actor->GetName() );
			return NULL;
		}

		if(smesh->KPhysicsProps->AggGeom.GetElementCount() == 0)
		{
			debugf(TEXT("(Karma): SkeletalMesh (%s) with empty Karma KAggregateGeometry."), smesh->GetName() );
			return NULL;
		}

		geom = KAggregateGeomInstance(&smesh->KPhysicsProps->AggGeom, 
				actor->DrawScale * actor->DrawScale3D,
				KGData->GeomMan, actor->GetName());
	}
	// Pawns without static meshes dont create collision.
	else if( actor->GetAPawn() )
	{
		return NULL;
	}
	// For blocking volumes, decompose into convex polytopes and make aggregate on load.
	else if( actor->Brush && actor->IsA(ABlockingVolume::StaticClass()) )
	{
		KSetSecName(TEXT("KARMA: INIT COLLISION: GEOM: BLOCKING VOLUME"));

		FKAggregateGeom aggGeom;

		// First, convert model into several convex hulls.
		KModelToHulls(&aggGeom, actor->Brush, FVector(0,0,0));

		if(aggGeom.ConvexElems.Num() == 0)
		{
			debugf( TEXT("(Karma): Could not create Karma collision from blocking volume: %s"), actor->GetName() );
			return NULL;
		}

		// Then instance that geometry using Karma.
		// Global count is incremented inside this function.
		geom = KAggregateGeomInstance(&aggGeom, FVector(1,1,1), KGData->GeomMan, actor->GetName());
	}

	return geom;

	unguard;
}

/*  Create the McdModel for an actor.
	If makeNull == true, create a 'null' geometry that wont collide with anything.
	For things with StaticMeshes with Karma collision props, use that.
	For Pawns without static meshes, use a cylinder.
	For anything else - just its bounding box. 

	This function assumes that the Actor has a KarmaParamsCollision (or child) */
void MEAPI KInitActorCollision(AActor* actor, UBOOL makeNull)
{
    guard(KInitActorCollision);

    if(!KGData || actor->bDeleteMe) 
        return;

    ULevel* level = actor->GetLevel();
    if(GIsEditor || !KGData->Framework || !level)
        return;

    McdModelID model = actor->getKModel();
	if(model)
        return; // already done!

	KSetSecName(TEXT("KARMA: INIT COLLISION: GEOM"));


	// Create required geometry.
	McdGeometryID geom;

	guard(CreateGeometry);
	check(KGData->GeomMan);

	if(makeNull)
		geom = KGData->GeomMan->nullGeom;
	else
		geom = KCreateActorGeometry(actor);

	if(!geom)
		return; // Couldn't create/find geometry for whatever reason

	unguard;

	guard(CreateModel);

	// Create actual model
	KSetSecName(TEXT("KARMA: INIT COLLISION: MODEL"));


	model = McdModelCreate(geom);
	check(model);
	(KGData->ModelCount)++;

    /* Convert actors position/orientation into tm matrix. */
	//MeMatrix4Ptr modelTM = (MeMatrix4Ptr)appMalloc(sizeof(MeMatrix4), TEXT("KARMA: MODEL TM"));
	KSetSecName(TEXT("KARMA: MODEL TM"));
	MeMatrix4Ptr modelTM = (MeMatrix4Ptr)MeMemoryAPI.createAligned(sizeof(MeMatrix4), 16);
	KSetSecName(TEXT("KARMA: POST MODEL TM"));

	FMatrix ActorTM = actor->LocalToWorld();
	FVector ActorOrigin( ActorTM.M[3][0], ActorTM.M[3][1], ActorTM.M[3][2] );

    KU2METransform(modelTM, ActorOrigin, actor->Rotation);
	McdModelSetTransformPtr(model, modelTM);

	if(McdGeometryGetTypeId(geom) != kMcdGeometryTypeNull)
		McdModelUpdate(model);
	
	unguard;

	guard(SetModelPointers);

	/* Model <-> Actor pointers. */
	KarmaModelUserData* data = new(KarmaModelUserData);
	check(data);
	data->actor = actor;
	McdModelSetUserData(model, (void*)data);

	check(actor->KParams);
    actor->KParams->KarmaData = (PTRINT)model;

	unguard;

    KSetSecName(TEXT("KARMA: POST INIT COLLISION"));

    unguard;
}

/* Remove _all_ existing dynamics & collision from this Actor. */
void MEAPI KTermActorCollision(AActor* actor)
{
    guard(KTermActorCollision);

    if(!KGData) 
        return;

    RTN_WITH_ERR_IF(!actor->getKModel(), "(Karma): KTermActorCollision: Actor has no collision.");
    
    McdModelID model = actor->getKModel();
    if(McdModelGetBody(model))
    {
        debugf(TEXT("(Karma): KTermActorCollision: Actor still has dynamics. Automatic KTermActorDynamics."));
        KTermActorDynamics(actor);
    }
    
    // Destroy any pairs that this actor's model was involved with.
    KGoodbyeAffectedPairs(model, actor->GetLevel());

    // This will destroy the geometry if its not needed by anything else any more.
    KModelDestroy(model);
	
    actor->KParams->KarmaData = 0;

    unguard;
}

/*  Initialise collision/constraint/skeleton for an Actor. 
    Dynamics is done inside AActor::physKarma */
void MEAPI KInitActorKarma(AActor* actor)
{
    guard(KInitActorKarma);

    if(!KGData || actor->bDeleteMe) 
        return;

    ULevel* level = actor->GetLevel();

    if(!level || GIsEditor || !KGData->Framework || actor->bDeleteMe)
        return;

	actor->bShouldStopKarma = KShouldStopKarma(actor); // Cache this for speed.

	/* *** SKELETAL *** */
	// Try and initialise rag-doll physics (must have a SkeletalMeshInstance)
	// Doesn't actually matter if we fail here - thi is tried again at the start of physKarmaRagDoll.
    if(actor->Physics == PHYS_KarmaRagDoll)
	{
		if( actor->Mesh == NULL || !actor->Mesh->IsA(USkeletalMesh::StaticClass()) )
			return;

		USkeletalMesh* skelMesh = Cast<USkeletalMesh>(actor->Mesh);
		USkeletalMeshInstance* inst = Cast<USkeletalMeshInstance>(skelMesh->MeshGetInstance(actor));

		KInitSkeletonKarma(inst);

		return;
	}
	
	/* *** CONSTRAINT *** */
	// See if its a Constraint - and init.
	AKConstraint* conActor = Cast<AKConstraint>(actor);
	if(conActor)
	{
		// If neither constrained actor has been set, do nothing (do not change physics mode)
		// This is useful when spawning a constraint in game, as you can set it all up and _then_ call SetPhysics(PHYS_Karma)
		if(!conActor->KConstraintActor1 && !conActor->KConstraintActor2)
		{
			conActor->Physics = PHYS_None;
			return;
		}
		else
			KInitConstraintKarma(conActor);

		return;
	}

	/* *** OTHER ACTOR *** */
	// If this actor is supposed to block Karma stuff - give it some kind of Karma collision geometry.
	if(actor->bBlockKarma)
	{
		// If this needs collision, but doesn't have a KarmaParamsCollision, create one here.
		// This will only allow you to turn Karma collision on (not dynamics),
		// but you would need to have given it a KarmaParams already anyway to do that.
		if(!actor->KParams)
		{
			actor->KParams = ConstructObject<UKarmaParamsCollision>( 
				UKarmaParamsCollision::StaticClass(), actor->GetOuter() );
		}
		
		KInitActorCollision(actor, 0);
	}

	if(actor->Physics == PHYS_Karma)
	{
		// If this is physics, but its not supposed to collide, we still need a model,
		// so create a 'null' one now.
		if(!actor->bBlockKarma)
		{
			if(!actor->KParams)
			{
				actor->KParams = ConstructObject<UKarmaParamsCollision>( 
					UKarmaParamsCollision::StaticClass(), actor->GetOuter() );
			}
			KInitActorCollision(actor, 1);
		}

		// Then initialise Karma dynamics.
		KInitActorDynamics(actor);
	}

    unguard;
}
    

void MEAPI KCheckActor(AActor* actor)
{
	guard(KCheckActor);

	// Check constraint
	AKConstraint* con = Cast<AKConstraint>(actor);
	if(con)
	{
		if(con->KConstraintActor2 && !con->KConstraintActor1)
			GWarn->MapCheck_Add( MCTYPE_ERROR, actor, 
				TEXT("Constraints to World must have second ConstraintActor as NULL."));

		// JTODO: Do more checking (ie. things connected have KarmaParams)
	}

	// no non-constraint actor should have Physics set to PHYS_Karma unless it has a KParams editinline struct
	if(actor->Physics == PHYS_Karma && !actor->KParams && !actor->IsA(AKConstraint::StaticClass()) ) 
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, actor, 
			TEXT("Actor using PHYS_Karma has no KParams."));
	}

	// if its set to KarmaRagDoll physics, but without KParams of type KarmaParamsSkel
	if(actor->Physics == PHYS_KarmaRagDoll && (!actor->KParams || !actor->KParams->IsA(UKarmaParamsSkel::StaticClass())))
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, actor, 
			TEXT("Actor using PHYS_KarmaRagDoll has no KParams of type KarmaParamsSkel."));
	}

	unguard;
}

/* Terminate all dynamics and collision for an Actor. */
void MEAPI KTermActorKarma(AActor* actor)
{
    guard(KTermActorKarma);

    if(!KGData) 
        return;

	/* *** OTHER ACTOR *** */
    if(actor->getKModel())
    {
        /* If there are dynamics on this actor - terminate them. */
        if(McdModelGetBody(actor->getKModel()))
            KTermActorDynamics(actor);

        KTermActorCollision(actor);
		return;
    }

	/* *** SKELETAL *** */
    /* Terminate Skeleton dynamics/collision if present. */
    if(actor->MeshInstance &&
        actor->MeshInstance->IsA(USkeletalMeshInstance::StaticClass()))
    {
        USkeletalMeshInstance* inst = Cast<USkeletalMeshInstance>(actor->MeshInstance);
        KTermSkeletonKarma(inst);
		return;
    }

    /* *** CONSTRAINT *** */
    AKConstraint* conActor = Cast<AKConstraint>(actor);
    if(conActor && conActor->getKConstraint())
    {
        KTermConstraintKarma(conActor);
		return;
    }

    unguard;
}



/*  Initialise Karma for a level.
    theFramework needs to be valid for this to work. */
void MEAPI KInitLevelKarma(ULevel* level)
{
    guard(KInitLevelKarma);

    if(!KGData) 
        return;

    if(GIsEditor || level->KWorld || !KGData->Framework)
        return;
    
    debugf(NAME_DevKarma, TEXT("(Karma): Initialising Karma for Level."));
    
    /* ** COLLISION ** */
    guard(Collision);
    KSetSecName(TEXT("KARMA: MCDSPACE"));

    unguard;


    /* ** DYNAMICS ** */
    guard(Dynamics);
    KSetSecName(TEXT("KARMA: MDTWORLD"));

	// Using malloc pools, so max bodies/constraints unnecessary
    level->KWorld = MdtWorldCreate(1, 1, ME_UNIT_LENGTH, ME_UNIT_MASS);
    
	MdtWorldSetCheckSim(level->KWorld, 1);
    MdtWorldSetAutoDisable(level->KWorld, 1);
    MdtWorldSetEpsilon(level->KWorld, KGData->SimParams.Epsilon);
    
    /* Tweak auto-disable thresholds */
    MdtWorldSetAutoDisableVelocityThreshold(level->KWorld, (MeReal)0.02);
    MdtWorldSetAutoDisableAccelerationThreshold(level->KWorld, (MeReal)0.05);
    MdtWorldSetAutoDisableAngularVelocityThreshold(level->KWorld, (MeReal)0.001);
    MdtWorldSetAutoDisableAngularAccelerationThreshold(level->KWorld, (MeReal)0.05);

	// Set callback for when  
    level->KWorld->bodyDisableCallback = RemoveActiveActorFromBody;
    level->KWorld->bodyEnableCallback = AddActiveActorFromBody;
    level->KWorld->contactGroupDestroyCallback = DestroyContactGroupReferences;

    unguard;

    /* ** BRIDGE ** */
    
    guard(Bridge);
    KSetSecName(TEXT("KARMA: MSTBRIDGE"));

    level->KBridge = KBridgeCreate(KGData->Framework, K_materialsMaxCount);
    MstBridgeSetModelPairBufferSize(level->KBridge, K_bridgeModelPairBufferSize);

    unguard;

	/* ** ASSET FACTORY ** */
	guard(AssetFactory);
    KSetSecName(TEXT("KARMA: ASSETFACTORY"));

	// We dont use the normal MeAssetFactoryCreate function, because we want just one geometry manager
	// for the whole game, but we need a new asset factory for each level.
	//level->KAssetFactory = MeAssetFactoryCreate(level->KWorld, level->KSpace, KGData->Framework);
    level->KAssetFactory = (MeAssetFactory*)appMalloc(sizeof(MeAssetFactory), TEXT("KARMA: ASSET FACTORY"));
    level->KAssetFactory->gm = KGData->GeomMan;
    level->KAssetFactory->geometryPostCreateCB = 0;
    level->KAssetFactory->geometryPostCreateCBUserdata = 0;
    level->KAssetFactory->modelCreateFunc = KModelCreateFromMeFAssetPart;
    level->KAssetFactory->modelPostCreateCB = 0;
    level->KAssetFactory->modelPostCreateCBUserdata = 0;
    level->KAssetFactory->jointCreateFunc = MdtConstraintCreateFromMeFJoint;
    level->KAssetFactory->jointPostCreateCB = 0;
    level->KAssetFactory->jointPostCreateCBUserdata = 0;

	unguard;

    /* ** LEVEL ** */
    
    guard(LevelCollision);
	/*  'Level' triangle-list collision primitive.
        Terrain/StaticMesh/BSP triangles all go in here. 
        We can't use the normal AssetFactory for this tri-list geometry.
     */
    KSetSecName(TEXT("KARMA: TRILIST"));

    /* 'Level' collision model */
    //MeMatrix4Ptr identTM = (MeMatrix4Ptr)appMalloc(sizeof(MeMatrix4), TEXT("KARMA: TRILIST TM"));
    KSetSecName(TEXT("KARMA: MODEL TM"));
    MeMatrix4Ptr identTM = (MeMatrix4Ptr)MeMemoryAPI.createAligned(sizeof(MeMatrix4), 16);
    KSetSecName(TEXT("KARMA: POST MODEL TM"));
    MeMatrix4TMMakeIdentity(identTM);

#if 0
	McdPlaneID triList = McdPlaneCreate(KGData->Framework);
	identTM[3][2] = -7.68f;
#else
	MeVector3 worldMax = {WORLD_MAX,WORLD_MAX,WORLD_MAX};
	MeVector3 worldMin = {-WORLD_MAX,-WORLD_MAX,-WORLD_MAX};
	
	McdTriangleListID triList = McdTriangleListCreate(
        KGData->Framework, worldMin, worldMax, KTRILIST_SIZE, KTriListGenerator);
    (KGData->GeometryCount)++;

	// Need this to look up triangles in callback.
	McdTriangleListSetUserData(triList, (void*)level);
#endif
    
    level->KLevelModel = McdModelCreate(triList);
    (KGData->ModelCount)++;

	McdModelSetTransformPtr(level->KLevelModel, identTM);
	McdModelUpdate(level->KLevelModel);

	KarmaModelUserData* data = new(KarmaModelUserData);
	data->actor = 0;
	McdModelSetUserData(level->KLevelModel, (void*)data);

    unguard;

    McdRequest* defReq = McdFrameworkGetDefaultRequestPtr(KGData->Framework);
    defReq->faceNormalsFirst = 1;
    defReq->contactMaxCount = 10;
    
    /* Set some callbacks to set materials etc. */
    MstBridgeSetPerContactCB(level->KBridge, 
        MstBridgeGetDefaultMaterial(), MstBridgeGetDefaultMaterial(),
        KPerContactCB);
    
    MstBridgeSetPerPairCB(level->KBridge,  
        MstBridgeGetDefaultMaterial(), MstBridgeGetDefaultMaterial(),
        KPerPairCB);
    
    MdtContactParamsID params = MstBridgeGetContactParams(level->KBridge,
        MstBridgeGetDefaultMaterial(), MstBridgeGetDefaultMaterial());

    MdtContactParamsSetType(params, MdtContactTypeFriction2D);
    MdtContactParamsSetFrictionModel(params, MdtFrictionModelNormalForce);
    MdtContactParamsSetFrictionCoeffecient(params, (MeReal)0.5);
    MdtContactParamsSetSoftness(params, KGData->SimParams.ContactSoftness);
    MdtContactParamsSetFriction(params, 10);



    KSetSecName(TEXT("KARMA: POST INIT LEVEL"));

    unguard;
}

static void MEAPI OutputType(MdtConstraintID con, void* userData)
{
	if(MdtConstraintDCastHinge(con))
		debugf(TEXT("(Karma:) Hinge"));
	else if(MdtConstraintDCastSkeletal(con))
		debugf(TEXT("(Karma:) Skeletal"));
	else if(MdtConstraintDCastContactGroup(con))
		debugf(TEXT("(Karma:) Contact Group"));
	else if(MdtConstraintDCastBSJoint(con))
		debugf(TEXT("(Karma:) Ball and Socket"));
	else
		debugf(TEXT("(Karma:) Other Constraint"));
}

/* Destroy all dynamics/collision from this level (including from actors). */
void MEAPI KTermLevelKarma(ULevel* level)
{
    guard(KTermLevelKarma);
    
    if(!KGData) 
        return;

    if(!(level->KWorld) || !(level->KBridge))
        return;

    debugf(NAME_DevKarma, TEXT("(Karma): Terminating Karma for Level."));

    guard(DePhysAllActors);
    /*  Cleanup Karma (if necessary) 
        First remove Karma stuff from all Actors. */
    for( INT iActor=0; iActor<level->Actors.Num(); iActor++ )
    {
        AActor* actor = level->Actors(iActor);
        if(actor)
        {
			KTermActorKarma(actor);
        }
    }
    unguard;

	check(level->KContactGenActors.Num() == 0);
	check(level->Ragdolls.Num() == 0);

    guard(RemoveLevelColl);
    /* Remove level model */
    if(level->KLevelModel)
    {
		// Make sure no pairs/contacts still reference this model.
		KGoodbyeAffectedPairs(level->KLevelModel, level);

        /* Do this manually - cant use Geometry Manager for tri-list. */
        McdGeometryID lgeom = McdModelGetGeometry(level->KLevelModel);
        MeMatrix4Ptr ltm = McdModelGetTransformPtr(level->KLevelModel);

		KarmaModelUserData* data = (KarmaModelUserData*)McdModelGetUserData(level->KLevelModel);
		check(data->OverlapModels.Num() == 0); // Check we have been properly removed from any pairs.
		check(data->actor == 0);
		delete data;

        McdModelDestroy(level->KLevelModel);
        (KGData->ModelCount)--;
        level->KLevelModel = 0;

        McdGeometryDestroy(lgeom);
        (KGData->GeometryCount)--;

		MeMemoryAPI.destroyAligned(ltm);
        //appFree(ltm);
    }
    unguard;
    
    guard(Bridge);
    /* Then remove bridge/space/world. */
    if(level->KBridge)   
    {
        MstBridgeDestroy(level->KBridge);
        level->KBridge = NULL;
    }
    unguard;

    guard(Collision);

	// Check there are no models still overlapping!
	check(level->OverlapPairs.Num() == 0);

	unguard;

    guard(Dynamics);
    if(level->KWorld)
    {
		INT bodyCount = MdtWorldGetTotalBodies(level->KWorld);
		if(bodyCount != 0)
		{
			debugf(TEXT("(Karma:) KTermLevelKarma: %f Bodies left in MdtWorld."), bodyCount);
		}
		check(bodyCount == 0);

		INT conCount = MdtWorldGetTotalConstraints(level->KWorld);
		if(conCount != 0)
		{
			debugf(TEXT("(Karma:) KTermLevelKarma: %d Constraints left in MdtWorld."), conCount);

			MdtWorldForAllConstraints(level->KWorld, OutputType, 0);
		}
		check(conCount == 0);

        MdtWorldDestroy(level->KWorld);
        level->KWorld = NULL;
    }
    unguard;
    
	guard(AssetFactory);
	if(level->KAssetFactory)
	{
		//MeAssetFactoryDestroy(level->KAssetFactory);
		appFree(level->KAssetFactory);
		level->KAssetFactory = NULL;
	}
	unguard;

    KTermGameKarma();

	// Free any tri-lists allocated for this level.
	while(level->TriListPool.Num() > 0)
	{
		KarmaTriListData* list = level->TriListPool(0);
		level->TriListPool.Remove(0);
		appFree(list);
	}

    debugf(TEXT("(Karma): Level Karma Terminated."));

    unguard;
}



/*============================================================================
	KARMA STATE TRACE — one CSV row per simulated body per frame.
	ufront 2026-08-28, karma/decompile behavioural work.

	WHY IT EXISTS. karma-decomp's gate set compares CALLS: shadow-harness
	divergences, per-pair difftests, substituted-scene trajectories. Every one
	runs on i386 and every one is blind to the class of defect the owner is
	reporting — a physics behaviour that is wrong in the TRAJECTORY the calls
	add up to, and wrong on wasm32 while native looks fine. Nothing in this
	project could answer "does it reproduce off wasm", so this exists to be the
	one instrument that can: identical engine code on both targets, emitting
	the same rows, diffable line by line.

	★ IT REFUSES TO RUN WITHOUT -FIXEDFPS, AND THAT IS THE WHOLE POINT.
	KTickLevelKarma derives FullTimestep, SubTimestep AND NumSubSteps from
	DeltaSeconds. A browser at 60 fps and a headless native run at whatever
	xvfb manages are therefore integrating DIFFERENT EQUATIONS, and two traces
	taken that way would diverge for a reason that has nothing to do with the
	recovery. Emitting them anyway would have produced a confident wrong answer
	on the first run, so the refusal is a hard error rather than a warning.

	THE KEY IS THE ENGINE'S ACTOR ARRAY, not MdtWorld's bodyDict. The dict is
	keyed on pointers; its iteration order is an allocation artefact and need
	not agree between two builds, let alone two targets. level->Actors is the
	map's own order and is identical everywhere, so (actor index, name, bone)
	is a key that means the same thing on both sides of a diff.

	  -FIXEDFPS=<n>        REQUIRED. 30 is what the trace runs use.
	  -KTRACE=<path>       enable; CSV written here (relative = engine cwd)
	  -KTRACEEVERY=<n>     log every n-th frame (default 1)
	  -KTRACEFRAMES=<n>    stop after n logged frames (default 0 = unlimited)
	  -KTRACEBONES         also log ragdoll bone bodies (off by default: they
	                       respawn, so their actor indices churn and swamp the
	                       diff with rows that have no counterpart)
	  -KTRACECONTACTS      also log the CONTACTS holding each body up — see below

	★ WHY -KTRACECONTACTS EXISTS. The B rows record STATE, and state is a
	continuous quantity: two builds whose arithmetic differs in the last bit
	produce B rows that differ in the last digit, and the difference grows. That
	instrument cannot tell those apart from a DISCRETE difference — a contact
	that one build generates and the other does not — and the two need opposite
	responses. Measured 2026-09-01: with the FP model held fixed on both sides
	(`-mfpmath=sse` at 32-bit), 14 of 15 bodies on ONS-Torlan are BIT-identical
	over all 89 frames and `ONSHoverBike3` is bit-identical for eight frames and
	then steps by 1.06 at frame 9. Exact agreement followed by a step change of
	order one is not arithmetic amplifying; it is a decision flipping. A hover
	bike is held up by repulsor line checks, so the decision that can flip is a
	contact — and until these rows existed the trace could not see one.

	The C row is the count and the K rows are the contacts themselves. Contact
	ORDER is not meaningful: a body's constraints hang off `MeDict`, whose order
	is an allocation artefact and need not agree between two builds. The rows are
	emitted in dict order and `ktrace_contacts.py` sorts them before comparing —
	the same reason the B rows are keyed on the engine's actor array rather than
	on `MdtWorld`'s `bodyDict`.
============================================================================*/

static FILE*	GKTraceFile		= NULL;
/* ⚠ MEMFS IS NOT A RELIABLE CHANNEL OFF A THREADED WASM BUILD, and two runs were lost to
   finding that out: the engine runs on a pthread and the MAIN thread's `FS` is not the same
   filesystem the worker wrote to, so a trace that works natively reads as an empty or
   truncated file in the browser with no error anywhere. -KTRACECONSOLE routes every row
   through stdout instead, which Emscripten proxies to the main thread and Playwright
   captures verbatim. Same format, same parser, one channel that exists on every target. */
static UBOOL	GKTraceToConsole = 0;
#define KTOUT(...)	do { if(GKTraceToConsole) printf(__VA_ARGS__); \
						 else if(GKTraceFile) fprintf(GKTraceFile, __VA_ARGS__); } while(0)
static INT		GKTraceEvery	= 1;
static INT		GKTraceLimit	= 0;
static INT		GKTraceFrame	= 0;
static INT		GKTraceWritten	= 0;
static UBOOL	GKTraceBones	= 0;
static UBOOL	GKTraceContacts	= 0;
static UBOOL	GKTraceTried	= 0;

/* NAMING THE OTHER SIDE OF A CONTACT. A `MdtBodyID` carries no user data in this engine —
   `KarmaModelUserData` hangs off the McdModel, not the body — so there is no way back from a
   contact's second body to an actor without a map. It is rebuilt each traced frame from
   `level->Actors`, which is the same order on every build and every target and is already what
   the B rows are keyed on. Fifteen bodies on the vehicle map, so a linear scan is the whole cost. */
struct KTraceLabel
{
	MdtBodyID		body;
	const TCHAR*	name;
	INT				actor;
	INT				bone;
};
static TArray<KTraceLabel> GKTraceLabels;

static const KTraceLabel* KTraceFindLabel(MdtBodyID body)
{
	for(INT i=0; i<GKTraceLabels.Num(); i++)
		if(GKTraceLabels(i).body == body)
			return &GKTraceLabels(i);
	return NULL;
}

static void KTraceAddLabel(INT ActorIndex, const TCHAR* Name, INT BoneIndex, McdModelID model)
{
	if(!model)
		return;
	MdtBodyID body = McdModelGetBody(model);
	if(!body)
		return;

	KTraceLabel L;
	L.body	= body;
	L.name	= Name;
	L.actor	= ActorIndex;
	L.bone	= BoneIndex;
	GKTraceLabels.AddItem(L);
}

/* One body. %.9g round-trips a float exactly, so a diff that shows nothing is
   evidence of bit-identity rather than of a narrow print format. */
static void KTraceRow(INT ActorIndex, const TCHAR* Name, INT BoneIndex, McdModelID model)
{
	if(!model)
		return;

	MdtBodyID body = McdModelGetBody(model);
	if(!body)
		return;	/* static world geometry — a model with no body has no state to trace */

	MeVector3 p, v, w;
	MeVector4 q;
	MdtBodyGetPosition(body, p);
	MdtBodyGetQuaternion(body, q);
	MdtBodyGetLinearVelocity(body, v);
	MdtBodyGetAngularVelocity(body, w);

	/* `enabled` is not decoration. Karma auto-disables a body that has come to
	   rest, so "sits still" and "sinks slowly" differ in this column before
	   they differ visibly in position. */
	KTOUT(
		"B,%d,%d,%s,%d,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",
		GKTraceFrame, ActorIndex, appToAnsi(Name), BoneIndex,
		(INT)MdtBodyIsEnabled(body),
		(double)p[0], (double)p[1], (double)p[2],
		(double)q[0], (double)q[1], (double)q[2], (double)q[3],
		(double)v[0], (double)v[1], (double)v[2],
		(double)w[0], (double)w[1], (double)w[2]);
}

/* One body's contacts. Walks the body's constraint list, keeps the ones that are contact
   GROUPS, and emits the group count, the total contact count, and one row per contact.

   ⚠ THE PENETRATION AND THE NORMAL ARE THE POINT, not just the count. Two builds can agree on
   "one contact with the world" and disagree on how deep it is, and a repulsor's force is a
   function of the depth — so a count-only row would report a match on exactly the case this
   was written to catch. */
static void KTraceContactRows(INT ActorIndex, const TCHAR* Name, INT BoneIndex, McdModelID model)
{
	if(!model)
		return;

	MdtBodyID body = McdModelGetBody(model);
	if(!body)
		return;

	/* ⚠ USE appToAnsi's RETURN VALUE, never the buffer. On an ANSI build (TCHAR == char) it
	   returns the input untouched and never writes the buffer at all, so reading the buffer
	   yields a stack full of nothing on exactly the targets where the conversion is a no-op. */
	ANSICHAR SelfBuf[256];
	const ANSICHAR* SelfA = appToAnsi(Name, SelfBuf, ARRAY_COUNT(SelfBuf));

	INT NumGroups = 0, NumContacts = 0;

	/* ⚠ WALK `constraintDict` DIRECTLY, NOT `MdtBodyGetFirstConstraint`. That accessor is
	   declared in `MdtBody.h` and is NOT one of the 145 objects the recovery covers, so a build
	   with `USE_KARMA_DECOMP=ON` fails to LINK against it. The dict walk is what `KPhysic.cpp`
	   and `KSkeletal.cpp` already use for the same job, and it is inside the SDK's own header
	   either way. */
	MeDict* dict = &body->constraintDict;
	for(MeDictNode* node = MeDictFirst(dict); node != NULL; node = MeDictNext(dict, node))
	{
		MdtConstraintID con = (MdtConstraintID)MeDictNodeGet(node);
		MdtContactGroupID cg = MdtConstraintDCastContactGroup(con);
		if(!cg)
			continue;	/* hinges, ball-and-sockets, car wheels — real constraints, not contacts */

		NumGroups++;

		/* WHERE THE GROUP CAME FROM, which is the first thing you want to know and was a
		   throwaway probe before it was a column. THREE code paths create contact groups on a
		   body and they behave differently:

		     nearfield   `CreateContactGroup` in KFarfield.cpp, from a collision pair — and it
		                 is the ONLY one that calls `MdtContactGroupSetGenerator`, so a
		                 non-NULL generator identifies it exactly. The generator IS the
		                 `McdModelPair`, so it also names the other MODEL, which is more than
		                 the second body can do: a contact against level geometry and one
		                 against a static mesh actor both have a NULL second body and would
		                 otherwise both read `world`.
		     repulsor    KScript.cpp, `AKRepulsor` — a line check, one contact, no second body
		     wheel       UnSVehicle.cpp, `SVehicleWheel` — same shape

		   ⚠ Measured 2026-09-01: an extra group appeared on `ONSHoverBike3` at LP64 and the
		   nearfield and engine paths BOTH logged identical sequences, so it read as coming from
		   nowhere. It was a nearfield pair whose other model has no actor — level geometry. */
		MdtBodyID b0 = MdtContactGroupGetBody(cg, 0);
		MdtBodyID b1 = MdtContactGroupGetBody(cg, 1);
		MdtBodyID other = (b0 == body) ? b1 : b0;

		ANSICHAR OtherBuf[256];
		const ANSICHAR* OtherA;
		const KTraceLabel* lbl = NULL;
		McdModelPair* gen = (McdModelPair*)MdtContactGroupGetGenerator(cg);
		if(gen)
		{
			/* The pair's own models, not the bodies: `model2` is the one with no body when the
			   contact is against the world, and it still has an identity. */
			McdModelID mine = McdModelGetBody(gen->model1) == body ? gen->model1 : gen->model2;
			McdModelID them = (mine == gen->model1) ? gen->model2 : gen->model1;
			AActor* a = KModelGetActor(them);
			OtherA = a ? appToAnsi(a->GetName(), OtherBuf, ARRAY_COUNT(OtherBuf)) : "level";
		}
		else if(other)
		{
			lbl = KTraceFindLabel(other);
			OtherA = lbl ? appToAnsi(lbl->name, OtherBuf, ARRAY_COUNT(OtherBuf)) : "unmapped";
		}
		else
		{
			/* No generator and no second body: the engine attached this one itself. */
			OtherA = "attached";
		}

		for(MdtContactID c = MdtContactGroupGetFirstContact(cg);
		    c != NULL;
		    c = MdtContactGroupGetNextContact(cg, c))
		{
			MeVector3 cp, cn;
			MdtContactGetPosition(c, cp);
			MdtContactGetNormal(c, cn);

			KTOUT("K,%d,%d,%s,%d,%s,%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n",
				GKTraceFrame, ActorIndex, SelfA, BoneIndex,
				OtherA, lbl ? lbl->bone : -1,
				(double)cp[0], (double)cp[1], (double)cp[2],
				(double)cn[0], (double)cn[1], (double)cn[2],
				(double)MdtContactGetPenetration(c));
			NumContacts++;
		}
	}

	KTOUT("C,%d,%d,%s,%d,%d,%d\n",
		GKTraceFrame, ActorIndex, SelfA, BoneIndex, NumGroups, NumContacts);
}

/* THE ONE DEFINITION OF "WHICH BODIES ARE IN THE TRACE", walked once per emitter. The bone
   branch and the -KTRACEBONES rule live here and nowhere else, so the label table, the B rows
   and the contact rows cannot drift into disagreeing about the body set — which would show up
   as contacts naming `unmapped` and read like a Karma fault. */
typedef void (*KTraceBodyFn)(INT ActorIndex, const TCHAR* Name, INT BoneIndex, McdModelID model);

static void KTraceForEachBody(ULevel* level, KTraceBodyFn fn)
{

	for(INT i=0; i<level->Actors.Num(); i++)
	{
		AActor* Actor = level->Actors(i);
		if(!Actor)
			continue;

		if(GKTraceBones &&
		   Actor->Physics == PHYS_KarmaRagDoll &&
		   Actor->Mesh && Actor->Mesh->IsA(USkeletalMesh::StaticClass()))
		{
			USkeletalMesh* skelMesh = Cast<USkeletalMesh>(Actor->Mesh);
			USkeletalMeshInstance* inst = Cast<USkeletalMeshInstance>(skelMesh->MeshGetInstance(Actor));
			if(inst)
				for(INT b=0; b<inst->KSkelModels.Num(); b++)
					fn(i, Actor->GetName(), b, inst->KSkelModels(b));
			continue;
		}

		fn(i, Actor->GetName(), -1, Actor->getKModel());
	}
}

static void KTraceLevel(ULevel* level, FLOAT FullTimestep, FLOAT SubTimestep, INT NumSubSteps)
{
	if(!GKTraceTried)
	{
		GKTraceTried = 1;

		TCHAR Path[1024];
		if(!Parse(appCmdLine(), TEXT("KTRACE="), Path, ARRAY_COUNT(Path)))
			return;

		if(!GUseFixedTimeStep)
		{
			/* Loud, and on the fatal path, because a trace taken at a variable
			   timestep is not merely noisy — it is a different simulation, and
			   it would be diffed against another one in good faith. */
			appErrorf(TEXT("(Karma): -KTRACE requires -FIXEDFPS=<n>. KTickLevelKarma derives its ")
			          TEXT("timestep AND its substep count from DeltaSeconds, so two traces taken ")
			          TEXT("at different frame rates integrate different equations and cannot be ")
			          TEXT("compared."));
			return;
		}

		Parse(appCmdLine(), TEXT("KTRACEEVERY="), GKTraceEvery);
		Parse(appCmdLine(), TEXT("KTRACEFRAMES="), GKTraceLimit);
		GKTraceBones = ParseParam(appCmdLine(), TEXT("KTRACEBONES"));
		GKTraceContacts = ParseParam(appCmdLine(), TEXT("KTRACECONTACTS"));
		if(GKTraceEvery < 1)
			GKTraceEvery = 1;

		GKTraceToConsole = ParseParam(appCmdLine(), TEXT("KTRACECONSOLE"));
		if(!GKTraceToConsole)
			GKTraceFile = fopen(appToAnsi(Path), "w");
		if(!GKTraceToConsole && !GKTraceFile)
		{
			debugf(NAME_DevKarma, TEXT("(Karma): -KTRACE could not open '%s'."), Path);
			return;
		}
		KTOUT("# ktrace v1  fixedstep=%.9g every=%d limit=%d bones=%d contacts=%d\n",
			(double)GFixedTimeStep, GKTraceEvery, GKTraceLimit, (INT)GKTraceBones, (INT)GKTraceContacts);
		KTOUT("# F,frame,fullstep,substep,numsubsteps\n");
		KTOUT("# B,frame,actor,name,bone,enabled,px,py,pz,qx,qy,qz,qw,vx,vy,vz,wx,wy,wz\n");
	KTOUT("# A,actor,name,class,physics,hasmodel,hasbody,bstatic\n");
		KTOUT("# C,frame,actor,name,bone,groups,contacts\n");
		KTOUT("# K,frame,actor,name,bone,other,otherbone,px,py,pz,nx,ny,nz,penetration\n");
		debugf(NAME_DevKarma, TEXT("(Karma): KTRACE -> %s (every %d, limit %d, bones %d, contacts %d)"),
			Path, GKTraceEvery, GKTraceLimit, (INT)GKTraceBones, (INT)GKTraceContacts);
	}

	if(!GKTraceFile && !GKTraceToConsole)
		return;

	GKTraceFrame++;
	if((GKTraceFrame % GKTraceEvery) != 0)
		return;
	if(GKTraceLimit > 0 && GKTraceWritten >= GKTraceLimit)
	{
		/* Ask to quit as soon as the trace is complete. A bisect runs this dozens of times and
		   the alternative is every run costing its full `timeout` regardless of how few frames
		   it needed — which is most of the wall clock of the whole search. */
		if(GKTraceFile || GKTraceToConsole)
		{
			if(GKTraceFile) fclose(GKTraceFile);
			GKTraceFile = NULL;
			GKTraceToConsole = 0;
			debugf(NAME_DevKarma, TEXT("(Karma): KTRACE complete (%d frames) — requesting exit."), GKTraceWritten);
			appRequestExit(0);
		}
		return;
	}
	GKTraceWritten++;

	/* The step the frame actually used, so a trace carries the evidence that
	   the two runs stepped identically instead of asking the reader to trust it. */
	KTOUT("F,%d,%.9g,%.9g,%d\n",
		GKTraceFrame, (double)FullTimestep, (double)SubTimestep, NumSubSteps);

	/* ONE-SHOT INVENTORY, first traced frame only. "The body is not in the trace" has two very
	   different causes — Karma never gave the actor a model, or the actor is not in the level at
	   all — and a trace that simply omits the row cannot tell them apart. The first run of this
	   hook showed 8 bodies where the map defines 18 physics actors, and without these rows the
	   obvious reading (the recovery is dropping bodies) would have been wrong: they are map and
	   gametype facts. Emitted for every actor carrying KParams, model or not. */
	if(GKTraceWritten == 1)
	{
		for(INT i=0; i<level->Actors.Num(); i++)
		{
			AActor* Actor = level->Actors(i);
			if(!Actor || !Actor->KParams)
				continue;
			McdModelID m = Actor->getKModel();
			KTOUT("A,%d,%s,%s,%d,%d,%d,%d\n",
				i, appToAnsi(Actor->GetName()), appToAnsi(Actor->GetClass()->GetName()),
				(INT)Actor->Physics, m ? 1 : 0,
				(m && McdModelGetBody(m)) ? 1 : 0,
				(INT)Actor->bStatic);
		}
	}

	/* ★ THE LABEL TABLE IS BUILT BEFORE ANY CONTACT ROW IS WRITTEN, and that is why it is its
	   own pass rather than filled in as the B rows go out: a contact names the body on its
	   OTHER side, and that body's row may not have been reached yet. Half a table would name
	   half the contacts `unmapped` and read like Karma losing bodies. */
	if(GKTraceContacts)
	{
		GKTraceLabels.Empty();
		KTraceForEachBody(level, KTraceAddLabel);
	}

	KTraceForEachBody(level, KTraceRow);

	if(GKTraceContacts)
		KTraceForEachBody(level, KTraceContactRows);

	if(GKTraceFile) fflush(GKTraceFile);	/* a run that is killed by `timeout` must still leave a usable file */
}

void MEAPI KTickLevelKarma(ULevel* level, FLOAT DeltaSeconds)
{
    guard(KTickLevelKarma);
    
    if(!KGData || !level->KWorld)
        return;
    
	UBOOL bNormalEvolve = (KGData->bAutoEvolve && !level->GetLevelInfo()->bFreezeKarma);
	FLOAT DesiredTimestep = DeltaSeconds * level->GetLevelInfo()->KarmaTimeScale; // Overall Karma timestep scaling.

    if(bNormalEvolve)
        KGData->bDoTick = 1;

	FLOAT FullTimestep, SubTimestep;
	INT NumSubSteps;
	if(bNormalEvolve)
	{
		FullTimestep = Min(DesiredTimestep, KGData->SimParams.MaxTimestep);

		NumSubSteps = appCeil(DeltaSeconds / ME_SUBSTEPTARGET);
		NumSubSteps = Clamp<INT>(NumSubSteps, 1, ME_MAX_SUBSTEPS);

		SubTimestep = DesiredTimestep/NumSubSteps;
	}
	else
	{
		FullTimestep = 0.03f;

		NumSubSteps = 1;

		SubTimestep = 0.03f;
	}

    MdtWorldSetEpsilon(level->KWorld, KGData->SimParams.Epsilon);

	///////////// SINGLE STEP /////////////////
    MeReal meGamma = MeCLAMP(KGData->SimParams.GammaPerSec * FullTimestep, 0, (MeReal)0.5);
    MdtWorldSetGamma(level->KWorld, meGamma);
    KGData->TimeStep = FullTimestep;
    
	// Update triangle-list for each rag-doll.
	for(INT i=0; i<level->Ragdolls.Num(); i++)
		KUpdateRagdollTrilist(level->Ragdolls(i), 0);

	KSetSecName(TEXT("KARMA: UPDATE CONTACTS"));
	clock(GStats.DWORDStats(GEngineStats.STATS_Karma_KUpdateContacts));

    KUpdateContacts(level->KContactGenActors, level, 0); 

	unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_KUpdateContacts));
    KSetSecName(TEXT("KARMA: POST UPDATE CONTACTS"));
    
	if(KGData->bDoTick)
	{
		KSetSecName(TEXT("KARMA: WORLDSTEP"));
		clock(GStats.DWORDStats(GEngineStats.STATS_Karma_KWorldStepSafeTime));

		KWorldStepSafeTime(level->KWorld, FullTimestep, level, 0, 1);

		unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_KWorldStepSafeTime));
		KSetSecName(TEXT("KARMA: POST WORLDSTEP"));
	}

	///////////// SUB STEPS /////////////////

	meGamma = MeCLAMP(ME_GAMMA_PERSEC*SubTimestep, 0, (MeReal)0.5);
    MdtWorldSetGamma(level->KWorld, meGamma);
	KGData->TimeStep = SubTimestep;

	for(INT t=0; t<NumSubSteps; t++)
	{
		// Update triangle-list for each rag-doll.
		for(INT i=0; i<level->Ragdolls.Num(); i++)
			KUpdateRagdollTrilist(level->Ragdolls(i), 1);

		KSetSecName(TEXT("KARMA: UPDATE CONTACTS"));
		clock(GStats.DWORDStats(GEngineStats.STATS_Karma_KUpdateContacts));

		KUpdateContacts(level->KContactGenActors, level, 1); 

		unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_KUpdateContacts));
		KSetSecName(TEXT("KARMA: POST UPDATE CONTACTS"));

		if(KGData->bDoTick)
		{
			KSetSecName(TEXT("KARMA: WORLDSTEP"));
			clock(GStats.DWORDStats(GEngineStats.STATS_Karma_KWorldStepSafeTime));

			KWorldStepSafeTime(level->KWorld, SubTimestep, level, 1, t == NumSubSteps-1);

			unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_KWorldStepSafeTime));
			KSetSecName(TEXT("KARMA: POST WORLDSTEP"));
		}
	}

	KTraceLevel(level, FullTimestep, SubTimestep, NumSubSteps);

    unguard;
}

/* Called once at startup - stuff that persists during entire game. */
void MEAPI KInitGameKarma()
{
    guard(KInitGameKarma);

    if(GIsEditor)
    {
        debugf(NAME_DevKarma, TEXT("(Karma): Not Initialising Karma In Editor."));
        return;
    }

    if(KGData == 0)
    {
        debugf(NAME_DevKarma, TEXT("(Karma): Beginning Karma for game."));

        KGData = (KarmaGlobals*)appMalloc(sizeof(KarmaGlobals), TEXT("KarmaGlobals"));
        appMemset(KGData, 0, sizeof(KarmaGlobals));
        KGData->bAutoEvolve = 1;
        KGData->bDoTick = 1;
		KGData->bUseSafeTime = 1;			
		KGData->bDebugContacts = 0;

		// Set SimParams (also changeable from script)
		KGData->SimParams.ContactSoftness = ME_CONTACTSOFT;
		KGData->SimParams.Epsilon = ME_EPSILON;
		KGData->SimParams.GammaPerSec = ME_GAMMA_PERSEC;
		KGData->SimParams.MaxPenetration = ME_MAX_PENETRATION;
		KGData->SimParams.MaxTimestep = ME_MAX_TIMESTEP;
		KGData->SimParams.PenetrationOffset = ME_PENOFFSET;
		KGData->SimParams.PenetrationScale = ME_PENSCALE;

        guard(SetHandlers);
        KSetSecName(TEXT("KARMA: SET HANDLERS"));

        /* Route memory allocation through Unreal memory allocator */
        MeMemoryAPI.create =		&KMemCreate;
        MeMemoryAPI.destroy =		&KMemDestroy;
        MeMemoryAPI.resize =		&KMemResize;
        MeMemoryAPI.createZeroed =	&KMemCreateZeroed;

        /* Don't use fixed size pools - use malloc instead. */
		MePoolAPI.init =			&KPoolMallocInit;
		MePoolAPI.destroy =			&KPoolMallocDestroy;
		MePoolAPI.deset =			&KPoolMallocReset;
		MePoolAPI.getStruct =		&KPoolMallocGetStruct;
		MePoolAPI.putStruct =		&KPoolMallocPutStruct;
		MePoolAPI.getUsed =			&KPoolMallocGetUsed;
		MePoolAPI.getUnused =		&KPoolMallocGetUnused;

        /* Route messages through Unreal logging */
        MeSetInfoShow(KMessageShow);
        MeSetWarningShow(KMessageShow);
        MeSetFatalErrorShow(KMessageShow);
        MeSetDebugShow(KMessageShow);

        MeSetInfoHandler(KDebugHandler);
        MeSetWarningHandler(KDebugHandler);
        MeSetFatalErrorHandler(KErrorHandler);
        MeSetDebugHandler(KDebugHandler);
        
        unguard;

        guard(Collision);
        KSetSecName(TEXT("KARMA: MCDINIT"));

        // Most stuff lives with a ULevel - but we need this to keep McdGeometry across levels.
		// Using malloc pools, so geom instance max count unnecessary
        KGData->Framework = McdInit(
            K_collisionGeometryTypesMaxCount, 
            K_collisionModelsMaxCount,
            1, ME_UNIT_LENGTH);

		KSetSecName(TEXT("KARMA: MCDINIT: REGISTER"));

        McdPrimitivesRegisterTypes(KGData->Framework);
        McdConvexMeshRegisterType(KGData->Framework);
        McdAggregateRegisterType(KGData->Framework);
        McdNullRegisterType(KGData->Framework);

        McdPrimitivesRegisterInteractions(KGData->Framework);
        McdConvexMeshPrimitivesRegisterInteractions(KGData->Framework);
        McdAggregateRegisterInteractions(KGData->Framework);

        //McdConvexMeshConvexMeshRegisterInteraction(KGData->Framework);
        //McdTriangleListConvexMeshRegisterInteractions(KGData->Framework);

		KSetSecName(TEXT("KARMA: MCDINIT: MODELPAIRCONTAINER"));

		KGData->filterPairs = McdModelPairContainerCreate(K_bridgeModelPairBufferSize);

		KGData->ModelCount = 0;
		KGData->GeometryCount = 0;
        unguard;

        guard(AssetDB);
        KSetSecName(TEXT("KARMA: MEFILE"));

        /* Load all geometry into KGeometryMeFile */
        KCreateAssetDB(&KGData->AssetDB, &KGData->GeomMan);
        unguard;
    }
    else
        debugf(NAME_DevKarma,TEXT("(Karma): KInitGameKarma: Already Initialised!"));

    KSetSecName(TEXT("KARMA: POST INIT GAME"));

    unguard;
}

/* 
    Cleanup on exit. 
    This function is 'safe' because it will do nothing if the game is not shutting down,
    or there are still any geometries/models around.
*/
void ENGINE_API KTermGameKarma()
{
    guard(KTermGameKarma);

    if(!KGData) 
        return;

    if(!KGData->bShutdownPending || !KGData->GeomMan || 
        McdGMGetGeomCount(KGData->GeomMan) > 0 ||
		KGData->GeometryCount > 0 ||
		KGData->ModelCount > 0 )
        return;

	if(KGData->bShutdownPending && McdGMGetGeomCount(KGData->GeomMan) < 0)
		debugf(TEXT("(Karma): Negative GeometryManager Geometry Count on Shutdown."));
	
	if(KGData->bShutdownPending && KGData->GeometryCount < 0)
		debugf(TEXT("(Karma): Negative Geometry Count on Shutdown."));
		
	if(KGData->bShutdownPending && KGData->ModelCount < 0)
		debugf(TEXT("(Karma): Negative Model Count on Shutdown."));	

    debugf(NAME_DevKarma,TEXT("(Karma): Ending Karma for game."));

    guard(ModelPairContainer);
	if(KGData->filterPairs)
	{
		McdModelPairContainerDestroy(KGData->filterPairs);
		KGData->filterPairs = NULL;
	}
	unguard;

    guard(GeomMan);
    if(KGData->GeomMan)
    {
        debugf(NAME_DevKarma,TEXT("(Karma): Destroying Geometry Manager."));
        McdGMDestroy(KGData->GeomMan);
        KGData->GeomMan = NULL;
    }
    unguard;

    guard(AssetDB);
    if( KGData->AssetDB )
    {
        debugf(NAME_DevKarma,TEXT("(Karma): Destroying Asset Database."));
        MeAssetDBDestroy(KGData->AssetDB);
        KGData->AssetDB = NULL;
    }
    unguard;

    guard(Collision);
    if(KGData->Framework)
    {
        debugf(NAME_DevKarma,TEXT("(Karma): Destroying Framework."));
        McdTerm(KGData->Framework);
        KGData->Framework = NULL;
    }
    unguard;

	KGData->StaticMeshTris.Empty();

    appFree(KGData);
    KGData=NULL;

    unguard;
}

/* ********************* */

// 'newBlock' indicates if this Actor should have Karma collision now.
// Doesn't look at bBlockKarma.
void MEAPI KSetActorCollision(AActor* actor, UBOOL newBlock)
{
	guard(KSetActorCollision);

	// Do nothing for constraints!
	if(actor->IsA(AKConstraint::StaticClass()) || actor->bDeleteMe)
		return;

	ULevel* level = actor->GetLevel();

	/////////// UPDATE KCONTACTGENACTORS ////////////

	// Turned off bBlockKarma - make sure actor is not in KContactGenActors.
	if(!newBlock)
		KActorContactGen(actor, 0);
	// Turning on bBlockKarma - add to KContactGenActors if in a Karma physics mode.
	else if( actor->bCollideActors && (actor->Physics == PHYS_KarmaRagDoll || actor->Physics == PHYS_Karma) )
		KActorContactGen(actor, 1);

	//////// SKELETAL CASE /////////
	// For ragdoll karma, we dont destroy the geometry, we just clean up any existing pairs.
	// (And that only needs doing if we are turning off collision)
	// If we are turning collision on, we will just start generating pairs!
	if(actor->Physics == PHYS_KarmaRagDoll)
	{
		if(!newBlock)
			KGoodbyeActorAffectedPairs(actor);

		return;
	}

	//////// KARMA ACTOR CASE /////////
	McdModelID model = actor->getKModel();

	if(!model)
	{
		// If there is no model, and our new state is to have no Karma collision, we're done.
		if(!newBlock)
			return;
		else
		{
			// Create a model and geometry.
			KInitActorCollision(actor, 0);
			return;
		}
	}

	// We already have a model

	// Just in case its a model with no body, and we are turning off collision,
	// we might as well just remove it. A model with null geometry and no body
	// is just silly.
	if(!McdModelGetBody(model) && !newBlock)
	{
		KTermActorCollision(actor);
		return;
	}


	// Otherwise, we need to change its current geometry.
	McdGeometryID geom = McdModelGetGeometry(model);
	if(McdGeometryGetTypeId(geom) == kMcdGeometryTypeNull)
	{
		// If geometry is type 'null', and our new state is to have no Karma collision, we're done.
		if(!newBlock)
			return;
		else
		{
			// Otherwise, we need to create the right geometry here, and change the McdModel to it.
			McdGeometryID newGeom = KCreateActorGeometry(actor);
			McdModelSetGeometry(model, newGeom);
			// Don't destroy old geometry because it was the 'null' one (part of McdGeomMan).
		}
	}
	else
	{
		// If geometry is non-null, and our new state is having Karma collision, we're done.
		if(newBlock)
			return;
		else
		{
			// Otherwise, assign this model the 'null' geometry here.
			
			// Clean up any pairs with other models
			KGoodbyeAffectedPairs(model, level);

			McdModelSetGeometry(model, KGData->GeomMan->nullGeom);

			// TEMPORARY BUG WORKAROUND (FIXED IN KARMA 1.2)
			model->mInstance.child = 0;

			// Destroy old geometry, if no longer being used.
			KGMDestroyGeometry(KGData->GeomMan, geom);
		}
	}

	unguard;
}

#endif // WITH_KARMA

