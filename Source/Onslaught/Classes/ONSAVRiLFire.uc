class ONSAVRiLFire extends ProjectileFire;

var vector  KickMomentum;
var float ReloadAnimDelay;

function PlayFiring()
{
    Super.PlayFiring();
    if (Weapon.HasAnim(ReloadAnim))
	   SetTimer(ReloadAnimDelay, false);
}

function Projectile SpawnProjectile(vector Start, rotator Dir)
{
	local Projectile P;

	P = Super.SpawnProjectile(Start, Dir);
	if (P != None)
		P.SetOwner(Weapon);

	return P;
}

function Timer()
{
	if (Weapon.ClientState == WS_ReadyToFire)
	{
		Weapon.PlayAnim(ReloadAnim, ReloadAnimRate, TweenTime);
		Weapon.PlaySound(ReloadSound,SLOT_None,,,512.0,,false);
		ClientPlayForceFeedback(ReloadForce);
	}
}

function ShakeView()
{
    Super.ShakeView();

    if (Instigator != None)
        Instigator.AddVelocity(KickMomentum >> Instigator.Rotation);
}

function float MaxRange()
{
	return 15000;
}

function StartBerserk()
{
	if (Level.GRI != None && Level.GRI.WeaponBerserk > 1.0)
		return;

	Super.StartBerserk();

	ReloadAnimDelay = default.ReloadAnimDelay * 0.75;
}

function StopBerserk()
{
	if (Level.GRI != None && Level.GRI.WeaponBerserk > 1.0)
		return;

	Super.StopBerserk();

	ReloadAnimDelay = default.ReloadAnimDelay;
}

function StartSuperBerserk()
{
	Super.StartSuperBerserk();

	ReloadAnimDelay = default.ReloadAnimDelay / Level.GRI.WeaponBerserk;
}

defaultproperties
{
    AmmoClass=class'ONSAVRiLAmmo'
    AmmoPerFire=1

    FireAnim=Fire
    FireAnimRate=1.1
    ReloadAnimRate=1.35
    ReloadAnimDelay=1.0

    ProjectileClass=class'Onslaught.ONSAVRiLRocket'

    ProjSpawnOffset=(X=25,Y=6,Z=-6)

    KickMomentum=(X=-350,Y=0,Z=175)

    FireSound=Sound'ONSVehicleSounds-S.AVRiL.AVRiLFire01'
    ReloadSound=Sound'ONSVehicleSounds-S.AVRiL.AVRiLReload01'
    FireForce="AVRiLFire"
    ReloadForce="AVRiLReload"

    FireRate=4.0
    TweenTime=0.0

    bSplashDamage=true
    bRecommendSplashDamage=false
    bSplashJump=false
    BotRefireRate=0.5
    WarnTargetPct=+1.0
    bModeExclusive=false

    ShakeOffsetMag=(X=-20.0,Y=0.00,Z=0.00)
    ShakeOffsetRate=(X=-1000.0,Y=0.0,Z=0.0)
    ShakeOffsetTime=2
    ShakeRotMag=(X=0.0,Y=0.0,Z=0.0)
    ShakeRotRate=(X=0.0,Y=0.0,Z=0.0)
    ShakeRotTime=2
}
