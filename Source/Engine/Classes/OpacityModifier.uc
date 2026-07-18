//
// OpacityModfifer - used to override a shader's opacity channel (eg for shaders on terrain).
//
class OpacityModifier extends Modifier
	noteditinlinenew
	native;

var Material Opacity;
var bool bOverrideTexModifier;

defaultproperties
{
	// MT_Modifier | MT_OpacityModifier
	MaterialType=33
}