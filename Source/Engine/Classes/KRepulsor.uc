class KRepulsor extends Actor
	native;

cpptext
{
#ifdef WITH_KARMA
	void Destroy();
#endif
}

var()	bool	bEnableRepulsion;
var()   bool    bRepulseWater; // Repulsor should repulse against water volumes
var		bool	bRepulsorInContact; // Repulsor is currently contacting something.
var     bool    bRepulsorOnWater; //Repulsor is contacting water (bRepulseWater must be set)
var()	vector	CheckDir; // In owner ref frame
var()	float	CheckDist;
var()	float	Softness;
var()	float	PenScale;
var()	float	PenOffset;

// Used internally for Karma stuff - DO NOT CHANGE!
var		transient const pointer		KContact;



defaultproperties
{
    bEnableRepulsion=true
	CheckDir=(X=0.0,Y=0.0,Z=-1.0)
	CheckDist=50
	Softness=0.1
	PenScale=1.0
	PenOffset=0.0

	bCollideActors=false
	bCollideWorld=false
	bBlockActors=false
	bBlockNonZeroExtentTraces=false
	bBlockZeroExtentTraces=false
	bProjTarget=false
	bHardAttach=true
    RemoteRole=ROLE_None
	bNoDelete=false
}
