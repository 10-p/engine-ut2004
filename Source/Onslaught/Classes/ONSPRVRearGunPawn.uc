//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSPRVRearGunPawn extends ONSWeaponPawn;

function AltFire(optional float F)
{
	local PlayerController PC;

	PC = PlayerController(Controller);
	if (PC == None)
		return;

	bWeaponIsAltFiring = true;
	PC.ToggleZoom();
}

function ClientVehicleCeaseFire(bool bWasAltFire)
{
	local PlayerController PC;

	if (!bWasAltFire)
	{
		Super.ClientVehicleCeaseFire(bWasAltFire);
		return;
	}

	PC = PlayerController(Controller);
	if (PC == None)
		return;

	bWeaponIsAltFiring = false;
	PC.StopZoom();
}

simulated function ClientKDriverLeave(PlayerController PC)
{
	Super.ClientKDriverLeave(PC);

	bWeaponIsAltFiring = false;
	PC.EndZoom();
}

DefaultProperties
{
	VehiclePositionString="in a HellBender's rear turret"
	VehicleNameString="HellBender Rear Turret"
	EntryPosition=(X=-50,Y=0,Z=0)
	EntryRadius=160.0
	ExitPositions(0)=(X=-235)
	ExitPositions(1)=(Y=165)
	ExitPositions(2)=(Y=-165)
	ExitPositions(3)=(Z=100)
    GunClass=class'Onslaught.ONSPRVRearGun'
	FPCamPos=(X=0,Y=0,Z=0)
	FPCamViewOffset=(X=0,Y=0,Z=40)
	bFPNoZFromCameraPitch=False
	TPCamLookAt=(X=0,Y=0,Z=100)
	TPCamWorldOffset=(X=0,Y=0,Z=50)
	TPCamDistance=350
	CameraBone=REARgunTURRET
	DrivePos=(X=-20,Z=90)
	DriverDamageMult=0.6
	bHasAltFire=false
}
