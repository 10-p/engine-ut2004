class ONSBrakelightCorona extends ONSHeadlightCorona;

simulated function UpdateBrakelightState(float Brake, int Gear)
{
	if(Brake > 0.01f)
	{
		bCorona = true;
		LightHue = 255;
		LightSaturation = 48;
	}
	else if(Gear == 0)
	{
		bCorona = true;
		LightHue = 255;
		LightSaturation = 255;
	}
	else
	{
		bCorona = false;
	}
}

defaultproperties
{
	DrawScale=0.3
	LightRadius=64
	MaxCoronaSize=50
}