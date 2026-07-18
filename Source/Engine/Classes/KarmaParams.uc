//=============================================================================
// The Karma physics parameters class.
// This provides 'extra' parameters needed by Karma physics to the Actor class.
// Need one of these (or a subclass) to set Physics to PHYS_Karma.
// (see Actor.uc)
// NB: All parameters are in KARMA scale!
//=============================================================================

class KarmaParams extends KarmaParamsCollision
	editinlinenew
	native;

cpptext
{
#ifdef WITH_KARMA
    void PostEditChange();
	void Destroy();
#endif
}

// Used internally for Karma stuff - DO NOT CHANGE!
var transient const pointer		KAng3;
var transient const pointer		KTriList;
var transient const float   KLastVel;

var()    float   KMass;						// Mass used for Karma physics
var()    float   KLinearDamping;			// Linear velocity damping (drag)
var()    float   KAngularDamping;			// Angular velocity damping (drag)

var()	 float   KBuoyancy;					// Applies in water volumes. 0 = no buoyancy. 1 = neutrally buoyant

var()    bool    KStartEnabled;				// Start simulating body as soon as PHYS_Karma starts
var()    vector  KStartLinVel;				// Initial linear velocity for actor
var()    vector  KStartAngVel;              // Initial angular velocity for actor

var()	 bool	 bKNonSphericalInertia;		// Simulate body without using sphericalised inertia tensor

var()	 float   KActorGravScale;		    // Scale how gravity affects this actor.

var()	 float   KVelDropBelowThreshold;    // Threshold that when actor drops below, KVelDropBelow event is triggered.

var()    float   KMaxSpeed;                 // Maximum allowed speed (unreal units)
var()	 float	 KMaxAngularSpeed;			// Maximum allowed angular velocity (radians per sec).

// NB - the below settings only apply to PHYS_Karma (not PHYS_KarmaRagDoll)
var()	 bool    bHighDetailOnly;			// Only turn on karma physics for this actor if the level PhysicsDetailLevel is PDL_High
var      bool    bClientOnly;				// Only turn on karma physics for this actor on the client (not server).
var() const bool bKDoubleTickRate;			// Allows higher karma sim rate (double normal) for some objects.

var()	 bool	 bKStayUpright;				// Stop this object from being able to rotate (using Angular3 constraint)
var()	 bool	 bKAllowRotate;				// Allow this object to rotate about a vertical axis. Ignored unless KStayUpright == true.
var		 bool	 bDestroyOnSimError;		// If there is a problem with the physics, destroy, or leave around to be fixed (eg. by network).
var()	 bool	 bDestroyOnWorldPenetrate;  // If the center of this object passes through the world, destroy it.
var()	 bool	 bDoSafetime;				// If true, do extra checks to avoid object passing through world.

var()	 float   StayUprightStiffness;
var()	 float   StayUprightDamping;

var()	array<KRepulsor>	Repulsors;

// default is sphere with mass 1 and radius 1
defaultproperties
{
    KMass=1
    KAngularDamping=0.2
    KLinearDamping=0.2
	KStartEnabled=false
	bKStayUpright=false
	bKAllowRotate=false
	bKNonSphericalInertia=false
	bKDoubleTickRate=false
	KBuoyancy=0
	KMaxSpeed=2500.0
	KMaxAngularSpeed=10.0
	bClientOnly=1
	bHighDetailOnly=1
	KActorGravScale=1
	KVelDropBelowThreshold=1000000
	bDestroyOnSimError=True
	bDestroyOnWorldPenetrate=False
	StayUprightStiffness=50
	StayUprightDamping=0
}
