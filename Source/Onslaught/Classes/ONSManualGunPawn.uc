//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSManualGunPawn extends ONSStationaryWeaponPawn;

#exec OBJ LOAD FILE=..\Animations\ONSWeapons-A.ukx

simulated function ActivateOverlay(bool bActive)
{
	if (Controller==None)
		return;

    Super.ActivateOverlay(bActive);
    if (Gun != None)
    {
        if (bActive)
            Gun.SetBoneScale(4, 0.0, 'TurretCockpit');
        else
            Gun.SetBoneScale(4, 1.0, 'TurretCockpit');
    }
}

DefaultProperties
{
	bPathColliding=true
    Mesh=Mesh'ONSWeapons-A.NewManualGun'
    StaticMesh=StaticMesh'ONSDeadVehicles-SM.MANUALbaseGunDEAD'
    GunClass=class'Onslaught.ONSManualGun'
    bDrawDriverinTP=True
    DrivePos=(X=-50,Z=48)
	FPCamPos=(X=-27,Y=0,Z=26)
	CameraBone=TurretCockpit
	TPCamLookAt=(X=-200,Y=0,Z=220)
	TPCamDistance=450
    EntryPosition=(X=0,Y=0,Z=0)
    EntryRadius=175.0
    Health=450
    HealthMax=450
    DriverDamageMult=0.0
    Team=255
    HUDOverlayClass=class'Onslaught.ONSManualGunOverlay'
    HUDOverlayOffset=(X=60,Y=0,Z=-25)
    HUDOverlayFOV=45
}
