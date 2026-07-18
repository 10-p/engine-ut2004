// AVRiL - Player held anti-aircraft weapon

class ONSAVRiL extends Weapon
	config(User);

#exec OBJ LOAD FILE="..\Textures\VMWeaponsTX"

var Material BaseMaterial;
var Material ReticleOFFMaterial;
var Material ReticleONMaterial;
var bool bLockedOn;
var Vehicle HomingTarget;
var float LockCheckFreq, LockCheckTime;
var float MaxLockRange, LockAim;
var Color CrosshairColor;
var float CrosshairX, CrosshairY;
var Texture CrosshairTexture;

replication
{
	reliable if (bNetDirty && bNetOwner && Role == ROLE_Authority)
		bLockedOn, HomingTarget;
}

simulated function PostBeginPlay()
{
	Super.PostBeginPlay();

	Skins[0] = ReticleOFFMaterial;
	Skins[1] = BaseMaterial;
}

simulated function OutOfAmmo()
{
}

simulated function ActivateReticle(bool bActivate)
{
    if(bActivate)
        Skins[0] = ReticleONMaterial;
    else
        Skins[0] = ReticleOFFMaterial;
}

simulated function WeaponTick(float deltaTime)
{
	local vector StartTrace, LockTrace;
	local rotator Aim;
	local float BestAim, BestDist;
	local bool bLastLockedOn, bBotLock;
	local Vehicle LastHomingTarget;
	local Vehicle AIFocus;
	local Vehicle V;
	local Actor AlternateTarget;

	if (Role < ROLE_Authority)
	{
		ActivateReticle(bLockedOn);
		return;
	}

	if (Instigator == None || Instigator.Controller == None)
	{
		LoseLock();
		ActivateReticle(false);
		return;
	}

	if (Level.TimeSeconds < LockCheckTime)
		return;

	LockCheckTime = Level.TimeSeconds + LockCheckFreq;

	bLastLockedOn = bLockedOn;
	LastHomingTarget = HomingTarget;
	bBotLock = true;
	if (AIController(Instigator.Controller) != None)
	{
		AIFocus = Vehicle(AIController(Instigator.Controller).Focus);
		if ( CanLockOnTo(AIFocus) && ((AIFocus.Controller != None) || (AIFocus != Instigator.Controller.MoveTarget) || AIFocus.HasOccupiedTurret())
			&& FastTrace(AIFocus.Location, Instigator.Location + Instigator.EyeHeight * vect(0,0,1)) )
		{
			HomingTarget = AIFocus;
			bLockedOn = true;
		}
		else
		{
			bLockedOn = false;
			bBotLock = false;
		}
	}
	else if ( HomingTarget == None || Normal(HomingTarget.Location - Instigator.Location) Dot vector(Instigator.Controller.Rotation) < LockAim
		  || VSize(HomingTarget.Location - Instigator.Location) > MaxLockRange
		  || !FastTrace(HomingTarget.Location, Instigator.Location + Instigator.EyeHeight * vect(0,0,1)) )
	{
		StartTrace = Instigator.Location + Instigator.EyePosition();
		Aim = Instigator.GetViewRotation();
		BestAim = LockAim;

		HomingTarget = Vehicle(Instigator.Controller.PickTarget(BestAim, BestDist, Vector(Aim), StartTrace, MaxLockRange));
	}

	// If no homing target, check for alternate targets
	if (HomingTarget == None)
	{
		StartTrace = Instigator.Location + Instigator.EyePosition();
		Aim = Instigator.GetViewRotation();

		for (V = Level.Game.VehicleList; V != None; V = V.NextVehicle)
		{
            AlternateTarget = V.AlternateTarget();

            if (AlternateTarget != None)
            {
                LockTrace = AlternateTarget.Location - StartTrace;
                if ( (Normal(LockTrace) dot Vector(Aim)) > LockAim && VSize(LockTrace) < MaxLockRange && FastTrace(AlternateTarget.Location,StartTrace) )
                {
                    HomingTarget = V;
                    if ( AIController(Instigator.Controller) != none)
                    	AIController(Instigator.Controller).Focus = V;
                    break;
                }
            }
        }
    }

	bLockedOn = CanLockOnTo(HomingTarget);

	ActivateReticle(bLockedOn);
	if (!bLastLockedOn && bLockedOn)
	{
		if ( bBotLock && (HomingTarget != None) )
			HomingTarget.NotifyEnemyLockedOn();
		if ( PlayerController(Instigator.Controller) != None )
			PlayerController(Instigator.Controller).ClientPlaySound(Sound'WeaponSounds.LockOn');
	}
	else if (bLastLockedOn && !bLockedOn && LastHomingTarget != None)
		LastHomingTarget.NotifyEnemyLostLock();
}

function bool CanLockOnTo(Actor Other)
{
    local Vehicle V;
    V = Vehicle(Other);

    if (V == None || V == Instigator)
        return false;

    if (!Level.Game.bTeamGame)
        return true;

    return (V.Team != Instigator.PlayerReplicationInfo.Team.TeamIndex);
}

function LoseLock()
{
	if (bLockedOn && HomingTarget != None)
		HomingTarget.NotifyEnemyLostLock();
	bLockedOn = false;
}

simulated function Destroyed()
{
	LoseLock();
	super.Destroyed();
}

simulated function DetachFromPawn(Pawn P)
{
	LoseLock();
	Super.DetachFromPawn(P);
}

simulated event RenderOverlays(Canvas Canvas)
{
	if (!FireMode[1].bIsFiring || ONSAVRiLAltFire(FireMode[1]) == None)
	{
		if (bLockedOn)
		{
			Canvas.DrawColor = CrosshairColor;
			Canvas.DrawColor.A = 255;
			Canvas.Style = ERenderStyle.STY_Alpha;
			Canvas.SetPos(Canvas.SizeX*0.5-CrosshairX, Canvas.SizeY*0.5-CrosshairY);
			Canvas.DrawTile(CrosshairTexture, CrosshairX*2.0, CrosshairY*2.0, 0.0, 0.0, CrosshairTexture.USize, CrosshairTexture.VSize);
		}

		Super.RenderOverlays(Canvas);
	}
}

// AI Interface
function float SuggestAttackStyle()
{
    return -0.4;
}

function float SuggestDefenseStyle()
{
    return 0.5;
}

function byte BestMode()
{
	return 0;
}

function float GetAIRating()
{
	local Bot B;
	local float ZDiff, dist, Result;

	B = Bot(Instigator.Controller);
	if ( (B.Target != None) && B.Target.IsA('ONSMortarCamera') )
		return 2;

	if ( (B == None) || (B.Enemy == None) )
		return AIRating;

	if (Vehicle(B.Enemy) == None)
		return 0;

	result = AIRating;
	ZDiff = Instigator.Location.Z - B.Enemy.Location.Z;
	if ( ZDiff < -200 )
		result += 0.1;
	dist = VSize(B.Enemy.Location - Instigator.Location);
	if ( dist > 2000 )
		return ( FMin(2.0,result + (dist - 2000) * 0.0002) );

	return result;
}

function bool RecommendRangedAttack()
{
	local Bot B;

	B = Bot(Instigator.Controller);
	if ( (B == None) || (B.Enemy == None) )
		return true;

	return ( VSize(B.Enemy.Location - Instigator.Location) > 2000 * (1 + FRand()) );
}
// end AI Interface

defaultproperties
{
    ItemName="AVRiL"
    Description="The AVRiL, or Anti-Vehicle Rocket Launcher, shoots homing missiles that pack quite a punch."
    IconMaterial=Material'HudContent.Generic.HUD'
    IconCoords=(X1=429,Y1=212,X2=508,Y2=251)

    FireModeClass(0)=ONSAVRiLFire
    FireModeClass(1)=ONSAVRiLAltFire
    InventoryGroup=8
	GroupOffset=1

    Mesh=Mesh'ONSWeapons-A.AVRiL_1st'

    BaseMaterial=Texture'VMWeaponsTX.PlayerWeaponsGroup.AVRiLtex'
    ReticleOFFMaterial=Shader'VMWeaponsTX.PlayerWeaponsGroup.AVRiLreticleTEX'
    ReticleONMaterial=Shader'VMWeaponsTX.PlayerWeaponsGroup.AVRiLreticleTEXRed'

    BobDamping=2.2
    PickupClass=class'ONSAVRiLPickup'
    EffectOffset=(X=100.0,Y=32.0,Z=-20.0)
    AttachmentClass=class'ONSAVRiLAttachment'
    PutDownAnim=PutDown

    DisplayFOV=45
    DrawScale=1.0
	AmbientGlow=64.0
    PlayerViewOffset=(X=100,Y=35.5,Z=-32.5)
    SmallViewOffset=(X=116,Y=43.5,Z=-40.5)
    PlayerViewPivot=(Pitch=0,Roll=0,Yaw=0)
    CenteredRoll=5500
    SelectSound=Sound'WeaponSounds.FlakCannon.SwitchToFlakCannon'
    SelectForce="SwitchToFlakCannon"

    AIRating=+0.55
    CurrentRating=+0.55
    HudColor=(r=0,g=0,b=255,a=255)
    Priority=8
    CustomCrosshair=16
	CustomCrosshairTextureName="ONSInterface-TX.avrilRETICLEtrack"
    CustomCrosshairColor=(r=0,g=255,b=0,a=255)

    MaxLockRange=15000.0
    LockAim=0.996 // 5 deg
    LockCheckFreq=0.20

    CrosshairColor=(R=0,G=255,B=0,A=255)
    CrosshairX=32
    CrosshairY=32
    CrosshairTexture=Texture'ONSInterface-TX.avrilRETICLE'

    BringUpTime=0.45
    SelectAnimRate=2.0
    PutDownAnimRate=1.75
    MinReloadPct=0.0
}
