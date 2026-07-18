#exec Texture Import File=Textures\S_FluidSurfOsc.pcx Name=S_FluidSurfOsc Mips=Off MASKED=1

//=============================================================================
// FluidSurfaceOscillator.
//=============================================================================
class FluidSurfaceOscillator extends Actor
	native
	placeable;

cpptext
{
	void UpdateOscillation( FLOAT DeltaTime );
	virtual void PostEditChange();
	virtual void Destroy();
}

// FluidSurface to oscillate
var() edfindable FluidSurfaceInfo	FluidInfo;
var() float							Frequency;
var() byte							Phase;
var() float							Strength;
var() float							Radius;

var transient const float			OscTime;

defaultproperties
{
	Texture=S_FluidSurfOsc
	bHidden=true
	Frequency=1
	Phase=0
	Strength=10
	Radius=0
}