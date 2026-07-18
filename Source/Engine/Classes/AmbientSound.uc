//=============================================================================
// Ambient sound -- Extended to support random interval sound emitters (gam).
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================
class AmbientSound extends Keypoint
	native
	exportstructs
	hidecategories(Movement,Collision,Lighting,LightColor,Karma,Force,Wind);

cpptext
{
    virtual void PostBeginPlay();
	virtual UBOOL Tick( FLOAT DeltaTime, enum ELevelTick TickType );
}

#exec Texture Import File=Textures\Ambient.pcx Name=S_Ambient Mips=Off MASKED=1

// Sound will trigger every EmitInterval +/- Rand(EmitVariance) seconds.

struct SoundEmitter
{
    var() float EmitInterval;
    var() float EmitVariance;
    
    var transient float EmitTime;

    var() Sound EmitSound; // Manually re-order because Dan turned off property sorting and broke binary compatibility.
};

var(Sound) Array<SoundEmitter> SoundEmitters;
var globalconfig float AmbientVolume;		// ambient volume multiplier (scaling)

defaultproperties
{
    Texture=S_Ambient

	AmbientVolume=+0.3
    SoundRadius=100
    SoundVolume=100
    SoundPitch=64

    RemoteRole=ROLE_None
	bStatic=false
	bNoDelete=true
	bNotOnDedServer=true
	bFullVolume=false
}
