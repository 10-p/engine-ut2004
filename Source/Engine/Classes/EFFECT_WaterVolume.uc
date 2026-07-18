//=============================================================================
// EFFECT_WaterVolume
//=============================================================================

class EFFECT_WaterVolume extends I3DL2Listener
	editinlinenew;

defaultproperties
{
	EnvironmentSize=1.8;
	EnvironmentDiffusion=1.0;
	Room=-1000;
	RoomHF=-4000;
	DecayTime=1.49;
	DecayHFRatio=0.1;
	Reflections=-449;
	ReflectionsDelay=0.007;
	Reverb=1700;
	ReverbDelay=0.011;
	RoomRolloffFactor=0.0;
	AirAbsorptionHF=-5;
	bDecayTimeScale=true;
	bReflectionsScale=true;
	bReflectionsDelayScale=true;
	bReverbScale=true;
	bReverbDelayScale=true;
	bDecayHFLimit=true;
}