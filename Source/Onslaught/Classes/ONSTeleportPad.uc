//for node teleporting
class ONSTeleportPad extends Actor
	placeable;

var array<Material> RedSkins, BlueSkins;

function UsedBy(Pawn user)
{
	if (Owner != None)
		Owner.UsedBy(user);
}

simulated function SetTeam(byte Team)
{
	if (Team == 0)
		Skins = RedSkins;
	else if (Team == 1)
		Skins = BlueSkins;
	else
		Skins.length = 0;
}

defaultproperties
{
	bStatic=true
	bStasis=true
	RemoteRole=ROLE_None
	DrawType=DT_StaticMesh
	DrawScale=2.0
	StaticMesh=StaticMesh'VMStructures.CoreGroue.BaseNodeSM'
	bCollideActors=true
	bBlockActors=true
	bProjTarget=true
	bAcceptsProjectors=False

	RedSkins(0)=Texture'ONSstructureTextures.CoreGroup.PowerNodeTEX'
	RedSkins(1)=FinalBlend'ONSstructureTextures.CoreGroup.powerNodeUpperFLAREredFinal'
	RedSkins(2)=FinalBlend'AW-2004Particles.Energy.BeamHitFinal'
	BlueSkins(0)=Texture'ONSstructureTextures.CoreGroup.PowerNodeTEX'
	BlueSkins(1)=FinalBlend'ONSstructureTextures.CoreGroup.powerNodeUpperFLAREblueFinal'
	BlueSkins(2)=FinalBlend'AW-2004Particles.Energy.BeamHitFinal'
}
