class ONSHeadlightCorona extends Light
	native;

simulated function ChangeTeamTint(byte T)
{
	if(T == 0)
	{
		LightHue = 255;
		LightSaturation=240;
	}
	else
	{
		LightHue = 128;
		LightSaturation=175;
	}
}

defaultproperties
{
	bUnlit=True
	DrawType=DT_None
	bHidden=False
	DrawScale=0.4
	bHardAttach=True
	bCollideActors=False
	bCorona=True
	bBlockActors=False
	LightType=LT_None
	LightBrightness=0
	LightSaturation=175
	LightHue=255
	LightRadius=100
	LightPeriod=0
	LightCone=0
	bDynamicLight=False
	bMovable=True
	RemoteRole=ROLE_None
	bNetInitialRotation=true
	Physics=PHYS_None
	bNoDelete=false
	bStatic=false
	bDirectionalCorona=true
	CoronaRotation=10
    bDetailAttachment=true
}
