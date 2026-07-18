//=============================================================================
// BlockingVolume:  a bounding volume
// used to block certain classes of actors
// primary use is to provide collision for non-zero extent traces around static meshes 

//=============================================================================

class BlockingVolume extends Volume
	native;

cpptext // sjs
{
    virtual UBOOL ShouldTrace(AActor *SourceActor, DWORD TraceFlags);
}

var() bool bClampFluid;
var() bool bClassBlocker; // sjs
var() Array< class<Actor> > BlockedClasses; // sjs

defaultproperties
{
	 bBlockZeroExtentTraces=false
	 bWorldGeometry=true
     bCollideActors=True
     bBlockActors=True
     bBlockKarma=True
     bClampFluid=True
     bClassBlocker=false
}