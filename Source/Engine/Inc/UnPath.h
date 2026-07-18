/*=============================================================================
	UnPath.h: Path node creation and ReachSpec creations and management specific classes
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Steven Polge 3/97
=============================================================================*/

// NOTE - There are some assumptions about these values in UReachSpec::findBestReachable().
// It is assumed that the MINCOMMON, CROUCHEDHUMAN, and HUMAN sizes are all smaller than
// COMMON and MAXCOMMON sizes.  If this isn't the case, findBestReachable needs to be modified.
#define MAXVEHICLERADIUS 260 //max radius to consider in building paths
#define MAXCOMMONRADIUS 120 //max radius to consider in building paths
#define MAXCOMMONHEIGHT 120
#define MINCOMMONHEIGHT 80 //min typical height for non-human intelligent creatures
#define MINCOMMONRADIUS 48 //min typical radius for non-human intelligent creatures
#define COMMONRADIUS    72 //max typical radius of intelligent creatures
#define COMMONHEIGHT    100 //max typical radius of intelligent creatures
#define HUMANRADIUS     26 //normal player pawn radius
#define HUMANHEIGHT     44 //normal playerpawn height
#define CROUCHEDHUMANHEIGHT  29 //crouched playerpawn height
#define MAXPATHDIST		1200 // maximum distance for paths between two nodes
#define MAXPATHDISTSQ	MAXPATHDIST*MAXPATHDIST
#define TESTJUMPZ		340	// jumpz for defining paths (based on player jumpz for your game)
#define TESTGROUNDSPEED 440	// groundspeed for defining paths (based on player groundspeed for your game)
#define TESTMAXFALLSPEED 2500	// maximum landing speed without taking damage, for the pawntype that can survive the highest landing speed
#define TESTSTANDARDFALLSPEED 1200
#define	PATHPRUNING		1.2f // maximum relative length of indirect path to allow pruning of direct reachspec between two pathnodes
#define MAXJUMPHEIGHT	64.f // max height of ledge pawn can jump up onto
#define MINMOVETHRESHOLD 4.1f // minimum distance to consider an AI predicted move valid
#define SWIMCOSTMULTIPLIER 2.f // cost multiplier for paths which require swimming
#define CROUCHCOSTMULTIPLIER 1.1f // cost multiplier for paths which require crouching

//Reachability flags - using bits to save space

enum EReachSpecFlags
{
	R_WALK = 1,	//walking required
	R_FLY = 2,   //flying required 
	R_SWIM = 4,  //swimming required
	R_JUMP = 8,   // jumping required
	R_DOOR = 16,
	R_SPECIAL = 32,
	R_LADDER = 64,
	R_PROSCRIBED = 128,
	R_FORCED = 256,
	R_PLAYERONLY = 512
}; 

// path node placement parameters
#define MAXWAYPOINTDIST 2.f  // max distance to a usable waypoint to avoid placing a new one after left turn
							// (ratio to collisionradius)
 
#define MAXSORTED 32
class FSortedPathList
{
public:
	ANavigationPoint *Path[MAXSORTED];
	INT Dist[MAXSORTED];
	int numPoints;

	FSortedPathList() { numPoints = 0; }
	ANavigationPoint* findStartAnchor(APawn *Searcher); 
	ANavigationPoint* findEndAnchor(APawn *Searcher, AActor *GoalActor, FVector EndLocation, UBOOL bAnyVisible, UBOOL bOnlyCheckVisible); 
	void addPath(ANavigationPoint * node, INT dist);
};

class ENGINE_API FPathBuilder
{
public:
	
	int buildPaths (ULevel *ownerLevel);
	int removePaths (ULevel *ownerLevel);
	void definePaths (ULevel *ownerLevel);
	void defineChangedPaths (ULevel *ownerLevel);
	void undefinePaths (ULevel *ownerLevel);
	void ReviewPaths(ULevel *ownerLevel);

private:
	ULevel * Level;
	AScout * Scout;

	void SetPathCollision(INT bEnabled);
	int createPaths();
	ANavigationPoint*	newPath(FVector spot);
	void getScout();
	void Pass2From(FVector start, FVector moveDirection, FLOAT TurnDir);
	int TestWalk(FVector WalkDir, FCheckResult Hit, FLOAT Threshold);
	INT TestReach(FVector Start, FVector End);
	INT ValidNode(ANavigationPoint *node, AActor *node2);
	void testPathsFrom(FVector start);
	void testPathwithRadius(FVector start, FLOAT R);
	void FindBlockingNormal(FVector &BlockNormal);
};

