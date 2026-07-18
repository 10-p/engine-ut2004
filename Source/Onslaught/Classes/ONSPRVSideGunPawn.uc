//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSPRVSideGunPawn extends ONSWeaponPawn;

function bool StopWeaponFiring()
{
	if ( bWeaponIsAltFiring && (Bot(Controller) != None) )
	{
		Gun.CalcWeaponFire();
		Gun.AltFire(Controller);
	}
	return Super.StopWeaponFiring();
}

DefaultProperties
{
	VehiclePositionString="in a HellBender's side turret"
	VehicleNameString="HellBender Side Turret"
	EntryPosition=(X=40,Y=50,Z=-100)
	EntryRadius=170.0
	GunClass=class'Onslaught.ONSPRVSideGun'
	ExitPositions(0)=(X=0,Y=165,Z=100)
	ExitPositions(1)=(X=0,Y=-165,Z=100)
	ExitPositions(2)=(X=0,Y=165,Z=-100)
	ExitPositions(3)=(X=0,Y=-165,Z=-100)
	FPCamPos=(X=0,Y=0,Z=20)
	TPCamLookAt=(X=0,Y=0,Z=50)
	TPCamDistance=150.000000
	DrivePos=(X=0,Y=0,Z=0)
	DriverDamageMult=0.4
}
