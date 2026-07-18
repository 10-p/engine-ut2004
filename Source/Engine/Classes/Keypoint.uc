//=============================================================================
// Keypoint, the base class of invisible actors which mark things.
//=============================================================================
class Keypoint extends Actor
	abstract
	placeable
	native;

// Sprite.
#exec Texture Import File=Textures\Keypoint.pcx Name=S_Keypoint Mips=Off MASKED=1

defaultproperties
{
	RemoteRole=ROLE_None
     bStatic=True
     bHidden=True
     SoundVolume=0
     CollisionRadius=+00010.000000
     CollisionHeight=+00010.000000
	 Texture=S_Keypoint
}
