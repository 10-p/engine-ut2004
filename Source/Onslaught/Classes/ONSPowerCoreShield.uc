//-----------------------------------------------------------
//
//-----------------------------------------------------------
class ONSPowerCoreShield extends Actor;

var Material TeamSkins[2];

//wake up call to fools shooting invalid target
var sound ShieldHitSound;
var float ImmediateWarningEndTime;
var int DamageCounter;
var PlayerController LastDamagedBy;

simulated function SetTeam(byte Team)
{
	ImmediateWarningEndTime = Level.TimeSeconds + 5;
	if (Team < 2)
		Skins[0] = TeamSkins[Team];
}

function TakeDamage(int Damage, Pawn EventInstigator, vector HitLocation, vector Momentum, class<DamageType> DamageType)
{
	local PlayerController PC;

	if (Damage <= 0 || EventInstigator == None || EventInstigator.Role < ROLE_Authority)
		return;

	Owner.TakeDamage(Damage, EventInstigator, HitLocation, Momentum, DamageType);
	PC = PlayerController(EventInstigator.Controller);
	if (PC != None)
	{
		if (Level.TimeSeconds < ImmediateWarningEndTime)
			PC.ClientPlaySound(ShieldHitSound);
		else
		{
			if (PC != LastDamagedBy)
			{
				DamageCounter = Damage;
				LastDamagedBy = PC;
			}
			else
				DamageCounter += Damage;
			if (DamageCounter > 200)
			{
				PC.ClientPlaySound(ShieldHitSound);
				DamageCounter -= 200;
			}
		}
	}
}

simulated function bool TeamLink(int TeamNum)
{
	if (Owner != None)
		return Owner.TeamLink(TeamNum);

	return false;
}

simulated function Touch(Actor Other)
{
	if (Projectile(Other) != None)
	{
		TakeDamage(1, Other.Instigator, Other.Location, vect(0,0,0), Projectile(Other).MyDamageType);
		Projectile(Other).Explode(Other.Location, Normal(Other.Velocity));
	}
}

function bool BlocksShotAt(Actor Other)
{
	if ( (Other == Owner) || (Other == ONSPowerCore(Owner).ShootTarget) )
		return false;
	return true;
}

DefaultProperties
{
    TeamSkins(1)=FinalBlend'AW-ShieldShaders.Shaders.BlueShieldFinal'

    DrawType=DT_StaticMesh
    bMovable=false
    bStasis=true
    RemoteRole=ROLE_None
    StaticMesh=StaticMesh'AW-ShieldShaders.ONS.EnergonShield'
    DrawScale3D=(X=2.0,Y=2.0)
    PrePivot=(Z=-250)
    bAcceptsProjectors=false
    bHidden=true
    bCollideActors=False
    bCollideWorld=False
    bBlockKarma=False
    bIgnoreEncroachers=True
    bBlockActors=False
    bProjTarget=True
    bUseCylinderCollision=true
    CollisionHeight=190.0
    CollisionRadius=215.0
    ShieldHitSound=Sound'ONSVehicleSounds-S.ShieldHit'
}
